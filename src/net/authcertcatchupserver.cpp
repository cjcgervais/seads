// SEADS authoritative CERTIFICATE-PKI + ASYNC + CATCH-UP server (netcode layer 32). See
// authcertcatchupserver.h.
//
// This is broadcast_authcert_async's async loop (authcertasyncserver.cpp) with broadcast_authsig_catchup's
// history-retention + windowed catch-up replay folded in — a SIBLING so the sealed broadcast.cpp AND every
// prior server are byte-for-byte untouched. The downstream helpers mirror authcertasyncserver.cpp's static
// helpers verbatim; CaTable / seat_authorizes / CHALLENGE-001 / HELLO-004 / BIND-001 are reused verbatim
// from layers 30/18. Two things differ from layer 31:
//   * a `history` vector retains the produced payloads (the last catchup_window, or all when window==0),
//     evicting the oldest per new frame (Stats.trimmed);
//   * accept_all, after enqueueing the BIND-001, ENQUEUES the retained catch-up prefix (each frame through
//     enqueue_bytes; the byte-cap applies per replayed frame, a joiner it trips is shed as `capped` before
//     it ever becomes live, its seat returned).
// Transport only: no kernel/det_math, no seal.
#include "authcertcatchupserver.h"

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

// One connected client. Identical to authcertasyncserver.cpp's AuthCertAsyncClient — catch-up adds no
// per-client state (the retained history lives on the server, not the client).
struct AuthCertCatchupClient {
    netsock::socket_t s;
    framing::StreamReassembler rx;    // upstream: reassemble whole INPUT-001 records from TCP chunks
    std::vector<std::uint8_t> buf;    // downstream: BIND-001, then catch-up prefix, then live frames
    std::size_t off = 0;
    std::size_t sent_total = 0;       // cumulative bytes the kernel has accepted (receive-progress signal)
    std::size_t idle_frames = 0;      // consecutive produced frames with no progress
    std::size_t last_sent = 0;        // sent_total snapshot at the last liveness check
    std::int64_t seat = bind001::SPECTATOR;
    bool pending() const { return off < buf.size(); }
    std::size_t pending_bytes() const { return buf.size() - off; }
};

bool flush_client(AuthCertCatchupClient& c) {
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

bool over_cap(const AuthCertCatchupClient& c, std::size_t cap_bytes) {
    return cap_bytes > 0 && c.pending_bytes() > cap_bytes;
}

bool enqueue_bytes(AuthCertCatchupClient& c, const std::vector<std::uint8_t>& bytes) {
    c.buf.insert(c.buf.end(), bytes.begin(), bytes.end());
    return flush_client(c);
}

void drop_client(std::vector<AuthCertCatchupClient>& clients, std::size_t i, Stats& st, CaTable& creds) {
    creds.release(clients[i].seat);
    netsock::close_socket(clients[i].s);
    clients.erase(clients.begin() + static_cast<std::ptrdiff_t>(i));
    ++st.leaves;
}

void reap_dead(std::vector<AuthCertCatchupClient>& clients, Stats& st, std::size_t liveness_frames,
               CaTable& creds) {
    if (liveness_frames == 0) return;
    for (std::size_t i = clients.size(); i-- > 0;) {
        AuthCertCatchupClient& c = clients[i];
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

Stats broadcast_authcert_catchup(netsock::socket_t listener, InputProducer& producer, CommandQueue& queue,
                                 CaTable& creds, std::uint64_t session_k0, std::uint64_t session_k1,
                                 std::size_t min_initial, int accept_deadline_ms,
                                 const std::function<void(std::size_t)>& on_frame, std::size_t cap_bytes,
                                 std::size_t liveness_frames, std::size_t catchup_window) {
    Stats st;
    std::vector<AuthCertCatchupClient> clients;
    std::uint64_t accept_counter = 0;  // increments per accepted socket -> a distinct challenge nonce
    // Retained catch-up history: the produced payloads still available to replay to a mid-stream joiner.
    std::vector<std::vector<std::uint8_t>> history;

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

    // Accept every pending connection: (a) send a fresh CHALLENGE-001 nonce DOWN (blocking); (b) read its
    // HELLO-004 (bounded-blocking); (c) VERIFY the CA certificate + possession proof to a seat (or
    // spectator); (d) go non-blocking and ENQUEUE the BIND-001; (e) — the layer-20/29/32 addition — ENQUEUE
    // the retained catch-up prefix right after the BIND. A joiner shed during replay by the byte-cap is
    // `capped`, its seat returned, never a member. Command frames pipelined after the HELLO are submitted
    // once the joiner is confirmed a member.
    auto accept_all = [&]() {
        while (true) {
            netsock::socket_t c = netsock::accept_one(listener);
            if (!netsock::is_valid(c)) break;
            AuthCertCatchupClient ac;
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
            bool alive = enqueue_bytes(ac, framed);  // BIND-001 first (fatal-only)

            // (e) catch-up: replay the retained prefix right after the BIND (cap per replayed frame).
            for (std::size_t k = 0; alive && k < history.size(); ++k) {
                framed.clear();
                framing::encode_frame(history[k], framed);
                alive = enqueue_bytes(ac, framed);
                if (alive && over_cap(ac, cap_bytes)) {  // replay backlog past the cap: shed on the spot
                    ++st.capped;
                    alive = false;
                }
            }
            if (!alive) {  // fatal handshake/replay send or cap shed: never a member; return the seat
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

    // --- gather the initial clients (bounded wait) before frame 0 (history empty => no replay) -------
    int waited = 0;
    while (clients.size() < min_initial && waited < accept_deadline_ms) {
        if (netsock::wait_readable(listener, 100)) accept_all();
        waited += 100;
    }
    if (clients.size() < min_initial) {
        for (AuthCertCatchupClient& c : clients) netsock::close_socket(c.s);
        return st;  // ok stays false
    }

    // --- bidirectional frame loop (authsigcatchupserver.cpp's loop verbatim) ----------------------
    std::vector<netsock::socket_t> rfds, wfds, readable, writable;
    std::vector<std::vector<std::uint8_t>> pv;
    std::vector<std::uint8_t> payload, frame;
    std::uint8_t rbuf[4096];
    for (std::size_t fi = 0;; ++fi) {
        if (on_frame) on_frame(fi);  // rendezvous hook

        rfds.clear();
        wfds.clear();
        for (const AuthCertCatchupClient& c : clients) {
            rfds.push_back(c.s);
            if (c.pending()) wfds.push_back(c.s);
        }
        rfds.push_back(listener);
        if (netsock::select_rw(rfds, wfds, 0, readable, writable)) {
            if (in_set(readable, listener)) accept_all();  // a mid-stream joiner gets BIND + catch-up prefix

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

        // retain this frame for late-join catch-up, evicting the oldest past the window (Stats.trimmed).
        history.push_back(std::move(payload));
        if (catchup_window > 0 && history.size() > catchup_window) {
            history.erase(history.begin());
            ++st.trimmed;
        }
    }

    // --- bounded DRAIN (authsigcatchupserver.cpp's tail verbatim) --------------------------------
    int idle = 0;
    while (idle < 600) {
        rfds.clear();
        wfds.clear();
        for (const AuthCertCatchupClient& c : clients) {
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

    for (AuthCertCatchupClient& c : clients) netsock::close_socket(c.s);
    st.ok = true;  // initial gather succeeded and the producer ran to its end
    return st;
}

}  // namespace netinput
}  // namespace seads
