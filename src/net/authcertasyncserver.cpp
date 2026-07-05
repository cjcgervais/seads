// SEADS authoritative CERTIFICATE-PKI + ASYNC server (netcode layer 31). See authcertasyncserver.h.
//
// This is broadcast_authsig_async's async loop (authsigasyncserver.cpp) with the ENROLLED-public-key verify
// replaced by the layer-30 CA-certificate verify: on accept send a fresh CHALLENGE-001 nonce (blocking),
// read the client's HELLO-004 [certificate, challenge_signature] (bounded-blocking), and CaTable::authenticate
// verifies the certificate under the trusted CA public key (+ revocation + epoch floor) and the possession
// proof under the CERTIFIED client key before binding the certificate's designated seat; then go non-blocking,
// ENQUEUE the BIND-001, and run the SAME async send buffers / byte-cap / liveness reap + per-seat
// authorization as layer 28. The sealed broadcast.cpp AND broadcast_bound / broadcast_bidi /
// broadcast_bound_async / broadcast_auth / broadcast_auth_async / broadcast_authmac / broadcast_authsig /
// broadcast_authsig_async / broadcast_authcert are byte-for-byte UNTOUCHED — a sibling that owns its own
// client struct + downstream helpers, reusing CaTable / seat_authorizes / CHALLENGE-001 / HELLO-004 /
// BIND-001 verbatim. Transport only: no kernel/det_math, no seal, no Stats change.
#include "authcertasyncserver.h"

#include <cstddef>
#include <utility>
#include <vector>

#include "authmac001.h"  // CHALLENGE-001 codec + derive_nonce (freshness, reused from layer 26/27/30)
#include "bind001.h"
#include "cert001.h"     // HELLO-004 / CERT-001 codec (the CA certificate + possession proof)
#include "framing.h"
#include "input001.h"

namespace seads {
namespace netinput {
namespace {

// One connected client: its socket, an upstream reassembler (client->server INPUT-001 command bytes), a
// downstream userspace send buffer buf[off:], the three liveness fields (inert when liveness_frames==0),
// and its resolved SEAT. authsigasyncserver.cpp's AuthSigAsyncClient exactly — the certificate credential
// changes only HOW the seat is resolved (a verified CA certificate vs a verified enrolled signature).
struct AuthCertAsyncClient {
    netsock::socket_t s;
    framing::StreamReassembler rx;    // upstream: reassemble whole INPUT-001 records from TCP chunks
    std::vector<std::uint8_t> buf;    // downstream: pending send bytes (BIND-001 first, then frames)
    std::size_t off = 0;
    std::size_t sent_total = 0;       // cumulative bytes the kernel has accepted (receive-progress signal)
    std::size_t idle_frames = 0;      // consecutive produced frames with no progress
    std::size_t last_sent = 0;        // sent_total snapshot at the last liveness check
    std::int64_t seat = bind001::SPECTATOR;
    bool pending() const { return off < buf.size(); }
    std::size_t pending_bytes() const { return buf.size() - off; }
};

bool flush_client(AuthCertAsyncClient& c) {
    const std::size_t start_off = c.off;
    while (c.pending()) {
        std::ptrdiff_t r = netsock::send_some(c.s, c.buf.data() + c.off, c.buf.size() - c.off);
        if (r < 0) return false;
        if (r == 0) break;  // kernel full; select_rw will report writability later
        c.off += static_cast<std::size_t>(r);
    }
    c.sent_total += c.off - start_off;  // receive-progress signal (bytes the kernel took this flush)
    if (c.off > 0) {
        c.buf.erase(c.buf.begin(), c.buf.begin() + static_cast<std::ptrdiff_t>(c.off));
        c.off = 0;
    }
    return true;
}

bool over_cap(const AuthCertAsyncClient& c, std::size_t cap_bytes) {
    return cap_bytes > 0 && c.pending_bytes() > cap_bytes;
}

bool enqueue_bytes(AuthCertAsyncClient& c, const std::vector<std::uint8_t>& bytes) {
    c.buf.insert(c.buf.end(), bytes.begin(), bytes.end());
    return flush_client(c);
}

// Drop client i: return its seat to the CaTable (so a reconnecting identity reclaims it), close, erase,
// count the leave. authsigasyncserver.cpp::drop_client verbatim, on the CA-trust roster.
void drop_client(std::vector<AuthCertAsyncClient>& clients, std::size_t i, Stats& st, CaTable& creds) {
    creds.release(clients[i].seat);
    netsock::close_socket(clients[i].s);
    clients.erase(clients.begin() + static_cast<std::ptrdiff_t>(i));
    ++st.leaves;
}

void reap_dead(std::vector<AuthCertAsyncClient>& clients, Stats& st, std::size_t liveness_frames,
               CaTable& creds) {
    if (liveness_frames == 0) return;
    for (std::size_t i = clients.size(); i-- > 0;) {
        AuthCertAsyncClient& c = clients[i];
        if (!c.pending() || c.sent_total > c.last_sent) {
            c.idle_frames = 0;
            c.last_sent = c.sent_total;  // progress observed: the deadline restarts
        } else if (++c.idle_frames > liveness_frames) {
            drop_client(clients, i, st, creds);
            ++st.reaped;
        }
    }
}

bool in_set(const std::vector<netsock::socket_t>& set, netsock::socket_t s) {
    for (netsock::socket_t r : set)
        if (r == s) return true;
    return false;
}

}  // namespace

Stats broadcast_authcert_async(netsock::socket_t listener, InputProducer& producer, CommandQueue& queue,
                               CaTable& creds, std::uint64_t session_k0, std::uint64_t session_k1,
                               std::size_t min_initial, int accept_deadline_ms,
                               const std::function<void(std::size_t)>& on_frame, std::size_t cap_bytes,
                               std::size_t liveness_frames) {
    Stats st;
    std::vector<AuthCertAsyncClient> clients;
    std::uint64_t accept_counter = 0;  // increments per accepted socket -> a distinct challenge nonce

    auto submit_payloads = [&](std::int64_t seat,
                               const std::vector<std::vector<std::uint8_t>>& payloads) {
        for (const auto& p : payloads) {
            input001::InputCommand ic;
            std::size_t pos = 0;
            if (!input001::decode_command(p.data(), p.size(), pos, ic)) continue;  // skip malformed
            if (!seat_authorizes(seat, ic.aircraft)) { ++st.cmds_unauth; continue; }
            switch (queue.submit(ic)) {
                case CommandQueue::Result::ACCEPTED: ++st.cmds_ok; break;
                case CommandQueue::Result::STALE: ++st.cmds_stale; break;
                case CommandQueue::Result::OUT_OF_RANGE: ++st.cmds_oob; break;
            }
        }
    };

    // Accept every pending connection: (a) send a fresh CHALLENGE-001 nonce DOWN (blocking — the socket is
    // still blocking; a cooperative client reads it first); (b) read its HELLO-004 (the FIRST upstream
    // framing frame, bounded-blocking); (c) VERIFY the CA certificate + possession proof to a DESIGNATED
    // seat (or spectator); (d) go non-blocking and ENQUEUE the BIND-001 as the first bytes of its send buffer
    // (after the CHALLENGE). Any command frames pipelined AFTER the HELLO in the same read are submitted
    // immediately (authorized). A failed challenge send, missing/malformed HELLO, or fatal BIND flush drops
    // the joiner (its seat, if verified, is returned).
    auto accept_all = [&]() {
        while (true) {
            netsock::socket_t c = netsock::accept_one(listener);
            if (!netsock::is_valid(c)) break;
            AuthCertAsyncClient ac;
            ac.s = c;

            // (a) derive + send the CHALLENGE-001 nonce (blocking; before set_nonblocking).
            std::uint64_t nonce = authmac001::derive_nonce(session_k0, session_k1, accept_counter++);
            {
                authmac001::ChallengeInfo ci{nonce};
                std::vector<std::uint8_t> rec, framed;
                authmac001::encode_challenge(ci, rec);
                framing::encode_frame(rec, framed);
                if (!netsock::send_all(c, framed)) { netsock::close_socket(c); continue; }
            }

            // (b) read upstream until the HELLO-004 record (the first framing frame) completes (bounded).
            std::vector<std::vector<std::uint8_t>> upframes;
            std::uint8_t hbuf[4096];
            bool dead = false;
            while (upframes.empty()) {
                if (!netsock::wait_readable(c, accept_deadline_ms)) { dead = true; break; }
                std::ptrdiff_t n = netsock::recv_some(c, hbuf, sizeof(hbuf));
                if (n <= 0) { dead = true; break; }
                if (!ac.rx.feed(hbuf, static_cast<std::size_t>(n), upframes)) { dead = true; break; }
            }
            cert001::Hello4Info hi;
            bool handshake_ok = false;
            if (!dead && !upframes.empty()) {
                std::size_t pos = 0;
                handshake_ok =
                    cert001::decode_hello4(upframes[0].data(), upframes[0].size(), pos, hi) &&
                    pos == upframes[0].size();
            }
            if (!handshake_ok) { netsock::close_socket(c); continue; }  // no valid HELLO: never a member

            // (c) VERIFY the certificate + possession proof to a seat (or spectator); (d) go non-blocking;
            // enqueue the BIND.
            ac.seat = creds.authenticate(hi.cert.data(), hi.cert.size(), nonce, hi.sig.data(),
                                         hi.sig.size());
            netsock::set_nonblocking(c);
            bind001::BindInfo info{ac.seat, creds.size()};
            std::vector<std::uint8_t> rec, framed;
            bind001::encode_bind(info, rec);
            framing::encode_frame(rec, framed);
            if (!enqueue_bytes(ac, framed)) {  // fatal handshake send: never a member, return the seat
                creds.release(ac.seat);
                netsock::close_socket(c);
                continue;
            }

            if (upframes.size() > 1) {  // command frames pipelined after the HELLO (authorized)
                std::vector<std::vector<std::uint8_t>> tail(upframes.begin() + 1, upframes.end());
                submit_payloads(ac.seat, tail);
            }
            clients.push_back(std::move(ac));
            ++st.joins;
        }
    };

    // --- gather the initial clients (bounded wait) before frame 0 -------------------------------
    int waited = 0;
    while (clients.size() < min_initial && waited < accept_deadline_ms) {
        if (netsock::wait_readable(listener, 100)) accept_all();
        waited += 100;
    }
    if (clients.size() < min_initial) {
        for (AuthCertAsyncClient& c : clients) netsock::close_socket(c.s);
        return st;  // ok stays false
    }

    // --- bidirectional frame loop (authsigasyncserver.cpp's loop verbatim) -----------------------
    std::vector<netsock::socket_t> rfds, wfds, readable, writable;
    std::vector<std::vector<std::uint8_t>> pv;
    std::vector<std::uint8_t> payload, frame;
    std::uint8_t rbuf[4096];
    for (std::size_t fi = 0;; ++fi) {
        if (on_frame) on_frame(fi);  // rendezvous hook

        rfds.clear();
        wfds.clear();
        for (const AuthCertAsyncClient& c : clients) {
            rfds.push_back(c.s);
            if (c.pending()) wfds.push_back(c.s);
        }
        rfds.push_back(listener);
        if (netsock::select_rw(rfds, wfds, 0, readable, writable)) {
            if (in_set(readable, listener)) accept_all();

            for (std::size_t i = clients.size(); i-- > 0;)
                if (in_set(writable, clients[i].s) && !flush_client(clients[i]))
                    drop_client(clients, i, st, creds);

            for (std::size_t i = clients.size(); i-- > 0;) {
                if (!in_set(readable, clients[i].s)) continue;
                bool leave = false;
                do {
                    std::ptrdiff_t n = netsock::recv_some(clients[i].s, rbuf, sizeof(rbuf));
                    if (n <= 0) { leave = true; break; }  // clean EOF / error: LEAVE
                    pv.clear();
                    if (!clients[i].rx.feed(rbuf, static_cast<std::size_t>(n), pv)) {
                        leave = true;  // malformed framing: drop the client
                        break;
                    }
                    submit_payloads(clients[i].seat, pv);
                } while (netsock::wait_readable(clients[i].s, 0));
                if (leave) drop_client(clients, i, st, creds);
            }
        }

        std::int64_t emit_tick = 0;
        if (!producer.next(emit_tick, payload)) break;

        frame.clear();
        framing::encode_frame(payload, frame);
        for (std::size_t i = clients.size(); i-- > 0;) {
            if (!enqueue_bytes(clients[i], frame)) {
                drop_client(clients, i, st, creds);
            } else if (over_cap(clients[i], cap_bytes)) {
                ++st.capped;
                drop_client(clients, i, st, creds);
            }
        }
        reap_dead(clients, st, liveness_frames, creds);
        ++st.frames_sent;
    }

    // --- bounded DRAIN (authsigasyncserver.cpp's tail verbatim) ----------------------------------
    int idle = 0;
    while (idle < 600) {
        rfds.clear();
        wfds.clear();
        for (const AuthCertAsyncClient& c : clients) {
            rfds.push_back(c.s);
            if (c.pending()) wfds.push_back(c.s);
        }
        if (wfds.empty()) break;  // every buffer drained
        if (netsock::select_rw(rfds, wfds, 50, readable, writable)) {
            for (std::size_t i = clients.size(); i-- > 0;)
                if (in_set(writable, clients[i].s) && !flush_client(clients[i]))
                    drop_client(clients, i, st, creds);
            idle = 0;  // progress observed
        } else {
            ++idle;
        }
    }
    for (std::size_t i = clients.size(); i-- > 0;)
        if (clients[i].pending()) drop_client(clients, i, st, creds);  // drain deadline: still owed bytes

    for (AuthCertAsyncClient& c : clients) netsock::close_socket(c.s);
    st.ok = true;  // initial gather succeeded and the producer ran to its end
    return st;
}

}  // namespace netinput
}  // namespace seads
