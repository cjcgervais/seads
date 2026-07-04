// SEADS authoritative AUTHENTICATED + ASYNC server (netcode layer 22). See authasyncserver.h.
//
// This is broadcast_bound_async's async loop (boundasyncserver.cpp) with broadcast_auth's identity
// handshake replacing the join-order seat assignment (authserver.cpp): on accept the server reads the
// client's HELLO-001 credential (bounded-blocking), looks it up in a CredentialTable to a DESIGNATED seat
// (invariant to join order; unknown token -> spectator), goes non-blocking, and ENQUEUES the SAME BIND-001
// record + applies the SAME per-seat authorization as layers 18/21 — but through the layer-16/19 async
// send buffers (byte-cap drop-slowest + liveness reap). The sealed broadcast.cpp AND broadcast_bound /
// broadcast_bidi / broadcast_bound_async / broadcast_auth are byte-for-byte UNTOUCHED — this is a sibling
// that owns its own client struct + downstream helpers (mirroring boundasyncserver.cpp), reusing
// CredentialTable / seat_authorizes / HELLO-001 / BIND-001 verbatim. Transport only: no kernel/det_math,
// no seal, no Stats change.
#include "authasyncserver.h"

#include <cstddef>
#include <utility>
#include <vector>

#include "bind001.h"
#include "framing.h"
#include "hello001.h"
#include "input001.h"

namespace seads {
namespace netinput {
namespace {

// One connected client: its socket, an upstream reassembler (client->server INPUT-001 command bytes), a
// downstream userspace send buffer buf[off:] (the pending tail the kernel has not yet accepted), the three
// liveness fields (inert when liveness_frames==0), and its resolved SEAT (aircraft index, or SPECTATOR).
// boundasyncserver.cpp's BoundAsyncClient exactly — authentication changes only HOW the seat is chosen
// (a CredentialTable lookup on the HELLO token, not a join-order SeatPolicy).
struct AuthAsyncClient {
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

// Push as much of the pending tail as the kernel will take now (boundasyncserver.cpp::flush_client
// verbatim). Returns false only on a fatal send error; a full kernel buffer (send_some==0) is not an error.
bool flush_client(AuthAsyncClient& c) {
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

// Layer-12 byte-cap: cap_bytes>0 and a pending backlog above it => beyond the drop threshold. 0 = off.
bool over_cap(const AuthAsyncClient& c, std::size_t cap_bytes) {
    return cap_bytes > 0 && c.pending_bytes() > cap_bytes;
}

// Append bytes to the downstream queue and opportunistically flush. False on a fatal send error.
bool enqueue_bytes(AuthAsyncClient& c, const std::vector<std::uint8_t>& bytes) {
    c.buf.insert(c.buf.end(), bytes.begin(), bytes.end());
    return flush_client(c);
}

// Drop client i: return its seat to the CredentialTable (so a reconnecting identity reclaims it — the one
// binding-specific step over bidiserver's drop_client, matching boundasyncserver but on the roster), close
// the socket, erase, and count the leave.
void drop_client(std::vector<AuthAsyncClient>& clients, std::size_t i, Stats& st, CredentialTable& creds) {
    creds.release(clients[i].seat);
    netsock::close_socket(clients[i].s);
    clients.erase(clients.begin() + static_cast<std::ptrdiff_t>(i));
    ++st.leaves;
}

// Layer-15a liveness reap (once per produced frame, after the frame is enqueued/flushed): a client that
// made NO receive progress — buffer non-empty AND no bytes left the kernel since the last check — for more
// than liveness_frames consecutive frames is presumed dead and dropped (reaped + leave; seat freed). A
// client fully drained (!pending) or that advanced sent_total this frame resets its idle counter, so a
// slow-but-alive client is never reaped. liveness_frames==0 disables the policy bit-for-bit.
void reap_dead(std::vector<AuthAsyncClient>& clients, Stats& st, std::size_t liveness_frames,
               CredentialTable& creds) {
    if (liveness_frames == 0) return;
    for (std::size_t i = clients.size(); i-- > 0;) {
        AuthAsyncClient& c = clients[i];
        if (!c.pending() || c.sent_total > c.last_sent) {
            c.idle_frames = 0;
            c.last_sent = c.sent_total;  // progress observed: the deadline restarts
        } else if (++c.idle_frames > liveness_frames) {
            drop_client(clients, i, st, creds);
            ++st.reaped;
        }
    }
}

// Is socket `s` in the ready subset select_rw filled?
bool in_set(const std::vector<netsock::socket_t>& set, netsock::socket_t s) {
    for (netsock::socket_t r : set)
        if (r == s) return true;
    return false;
}

}  // namespace

Stats broadcast_auth_async(netsock::socket_t listener, InputProducer& producer, CommandQueue& queue,
                           CredentialTable& creds, std::size_t min_initial, int accept_deadline_ms,
                           const std::function<void(std::size_t)>& on_frame, std::size_t cap_bytes,
                           std::size_t liveness_frames) {
    Stats st;
    std::vector<AuthAsyncClient> clients;

    // Decode one client's burst of reassembled upstream records into the CommandQueue, but ONLY for
    // commands naming this client's own seat (authorization). A foreign-aircraft command — or any command
    // from a spectator — is dropped as unauthorized (byte-identical to never arriving). boundserver's /
    // authserver's submit_payloads verbatim.
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

    // Accept every pending connection: read its HELLO-001 (the FIRST upstream framing frame, bounded-
    // blocking — the one non-async touch, matching layer 21), authenticate the token to a DESIGNATED seat,
    // go non-blocking, and ENQUEUE the client's one-time BIND-001 record as the first bytes of its send
    // buffer (before any snapshot). Any command frames pipelined AFTER the HELLO in the same read are
    // submitted immediately (authorized to the resolved seat). A missing/malformed HELLO drops the joiner
    // before it takes a seat; a fatal handshake flush drops it and returns its seat. Unlike layer 21's
    // blocking send_all, the BIND rides the same async send buffer as every frame — enqueued first, so the
    // flush delivers it first.
    auto accept_all = [&]() {
        while (true) {
            netsock::socket_t c = netsock::accept_one(listener);
            if (!netsock::is_valid(c)) break;
            AuthAsyncClient ac;
            ac.s = c;

            // 1) read upstream until the HELLO-001 record (the first framing frame) completes (bounded).
            std::vector<std::vector<std::uint8_t>> upframes;
            std::uint8_t hbuf[4096];
            bool dead = false;
            while (upframes.empty()) {
                if (!netsock::wait_readable(c, accept_deadline_ms)) { dead = true; break; }
                std::ptrdiff_t n = netsock::recv_some(c, hbuf, sizeof(hbuf));
                if (n <= 0) { dead = true; break; }
                if (!ac.rx.feed(hbuf, static_cast<std::size_t>(n), upframes)) { dead = true; break; }
            }
            hello001::HelloInfo hi;
            bool handshake_ok = false;
            if (!dead && !upframes.empty()) {
                std::size_t pos = 0;
                handshake_ok = hello001::decode_hello(upframes[0].data(), upframes[0].size(), pos, hi) &&
                               pos == upframes[0].size();
            }
            if (!handshake_ok) { netsock::close_socket(c); continue; }  // no valid HELLO: never a member

            // 2) authenticate the credential to a seat (or spectator); go non-blocking; enqueue the BIND.
            ac.seat = creds.authenticate(hi.token);
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

            // 3) submit any command frames pipelined after the HELLO in the same read (authorized).
            if (upframes.size() > 1) {
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
        for (AuthAsyncClient& c : clients) netsock::close_socket(c.s);
        return st;  // ok stays false
    }

    // --- bidirectional frame loop: read upstream commands, step the producer, enqueue the frame down.
    // Nothing here blocks on any single client (non-blocking sends + userspace buffers). -----------
    std::vector<netsock::socket_t> rfds, wfds, readable, writable;
    std::vector<std::vector<std::uint8_t>> pv;
    std::vector<std::uint8_t> payload, frame;
    std::uint8_t rbuf[4096];
    for (std::size_t fi = 0;; ++fi) {
        if (on_frame) on_frame(fi);  // rendezvous hook: block until a client has sent its commands

        // one select_rw over {listener} u {all clients readable} u {pending clients writable}
        rfds.clear();
        wfds.clear();
        for (const AuthAsyncClient& c : clients) {
            rfds.push_back(c.s);
            if (c.pending()) wfds.push_back(c.s);
        }
        rfds.push_back(listener);
        if (netsock::select_rw(rfds, wfds, 0, readable, writable)) {
            if (in_set(readable, listener)) accept_all();

            // flush writable clients (push pending downstream; a fatal flush drops the client + seat)
            for (std::size_t i = clients.size(); i-- > 0;)
                if (in_set(writable, clients[i].s) && !flush_client(clients[i]))
                    drop_client(clients, i, st, creds);

            // drain readable clients: a readable client is USUALLY sending upstream command bytes (n>0,
            // authorized into the queue for its OWN seat); only recv<=0 is a LEAVE. Drain all currently-
            // available bytes so a whole command burst is ingested before the ticks it governs are stepped.
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

        // produce the next authoritative frame (steps the sim, consuming this batch's commands)
        std::int64_t emit_tick = 0;
        if (!producer.next(emit_tick, payload)) break;

        // enqueue frame fi downstream to every client; a fatal send drops it, and a client the enqueue
        // leaves above the byte-cap is shed (layer-12 drop-slowest: counted capped + leave, seat freed).
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
        // layer-15a: reap any client gone silent (no receive progress) past the liveness deadline.
        reap_dead(clients, st, liveness_frames, creds);
        ++st.frames_sent;
    }

    // --- bounded DRAIN: flush the stragglers' pending downstream buffers (broadcast_async's tail).
    // Progress-bound (finite bytes owed) plus an idle cap: ~30 s of consecutive readiness-free
    // timeouts => the still-pending clients are dropped as leaves (fail-not-wedge). --------------
    int idle = 0;
    while (idle < 600) {
        rfds.clear();
        wfds.clear();
        for (const AuthAsyncClient& c : clients) {
            rfds.push_back(c.s);
            if (c.pending()) wfds.push_back(c.s);
        }
        if (wfds.empty()) break;  // every buffer drained
        if (netsock::select_rw(rfds, wfds, 50, readable, writable)) {
            for (std::size_t i = clients.size(); i-- > 0;)
                if (in_set(writable, clients[i].s) && !flush_client(clients[i]))
                    drop_client(clients, i, st, creds);
            idle = 0;  // progress observed: only an unbroken silent stretch counts against the cap
        } else {
            ++idle;
        }
    }
    for (std::size_t i = clients.size(); i-- > 0;)
        if (clients[i].pending()) drop_client(clients, i, st, creds);  // drain deadline: still owed bytes

    for (AuthAsyncClient& c : clients) netsock::close_socket(c.s);
    st.ok = true;  // initial gather succeeded and the producer ran to its end
    return st;
}

}  // namespace netinput
}  // namespace seads
