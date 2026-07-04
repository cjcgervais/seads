// SEADS authoritative AUTHENTICATED + ASYNC + CATCH-UP server (netcode layer 23). See authcatchupserver.h.
//
// This is broadcast_auth_async (authasyncserver.cpp) with broadcast_live's history-retention + windowed
// catch-up replay (broadcast.cpp, mirrored from boundcatchupserver.cpp) folded in — reimplemented here as a
// SIBLING so the sealed broadcast.cpp AND broadcast_bound / broadcast_bidi / broadcast_bound_async /
// broadcast_bound_catchup / broadcast_auth / broadcast_auth_async are byte-for-byte untouched. The
// downstream helpers (flush_client / over_cap / enqueue_bytes / drop_client / reap_dead) mirror
// authasyncserver.cpp's static helpers verbatim; CredentialTable / seat_authorizes / HELLO-001 / BIND-001
// are reused verbatim from layers 21/18. Two things differ from layer 22:
//   * a `history` vector retains the produced payloads (the last catchup_window of them, or all when
//     window==0), evicting the oldest per new frame (Stats.trimmed) — broadcast.cpp's retention;
//   * accept_all, after enqueueing the client's BIND-001 record, ENQUEUES the retained catch-up prefix
//     (each frame through enqueue_bytes; the byte-cap applies per replayed frame, a joiner it trips is shed
//     as `capped` before it ever becomes live, its seat returned) — broadcast_bound_catchup's replay, with
//     the identity accept (HELLO -> CredentialTable) of authasyncserver.
// Transport only: no kernel/det_math, no seal.
#include "authcatchupserver.h"

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
// Identical to authasyncserver.cpp's AuthAsyncClient — catch-up adds no per-client state (the retained
// history lives on the server, not the client).
struct AuthCatchupClient {
    netsock::socket_t s;
    framing::StreamReassembler rx;    // upstream: reassemble whole INPUT-001 records from TCP chunks
    std::vector<std::uint8_t> buf;    // downstream: pending send bytes (BIND-001, then catch-up prefix, then frames)
    std::size_t off = 0;
    std::size_t sent_total = 0;       // cumulative bytes the kernel has accepted (receive-progress signal)
    std::size_t idle_frames = 0;      // consecutive produced frames with no progress
    std::size_t last_sent = 0;        // sent_total snapshot at the last liveness check
    std::int64_t seat = bind001::SPECTATOR;
    bool pending() const { return off < buf.size(); }
    std::size_t pending_bytes() const { return buf.size() - off; }
};

// Push as much of the pending tail as the kernel will take now (authasyncserver.cpp::flush_client
// verbatim). Returns false only on a fatal send error; a full kernel buffer (send_some==0) is not one.
bool flush_client(AuthCatchupClient& c) {
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
bool over_cap(const AuthCatchupClient& c, std::size_t cap_bytes) {
    return cap_bytes > 0 && c.pending_bytes() > cap_bytes;
}

// Append bytes to the downstream queue and opportunistically flush. False on a fatal send error.
bool enqueue_bytes(AuthCatchupClient& c, const std::vector<std::uint8_t>& bytes) {
    c.buf.insert(c.buf.end(), bytes.begin(), bytes.end());
    return flush_client(c);
}

// Drop client i: return its seat to the CredentialTable (so a reconnecting identity reclaims it — the one
// binding-specific step over bidiserver's drop_client, matching authasyncserver but on the roster), close
// the socket, erase, and count the leave. Called for EVERY live-member drop path (EOF, malformed framing,
// fatal flush, byte-cap shed, liveness reap) so a seat is never stranded.
void drop_client(std::vector<AuthCatchupClient>& clients, std::size_t i, Stats& st, CredentialTable& creds) {
    creds.release(clients[i].seat);
    netsock::close_socket(clients[i].s);
    clients.erase(clients.begin() + static_cast<std::ptrdiff_t>(i));
    ++st.leaves;
}

// Layer-15a liveness reap (once per produced frame): a client that made NO receive progress for more than
// liveness_frames consecutive frames is presumed dead and dropped (reaped + leave; seat freed).
// authasyncserver.cpp::reap_dead verbatim.
void reap_dead(std::vector<AuthCatchupClient>& clients, Stats& st, std::size_t liveness_frames,
               CredentialTable& creds) {
    if (liveness_frames == 0) return;
    for (std::size_t i = clients.size(); i-- > 0;) {
        AuthCatchupClient& c = clients[i];
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

Stats broadcast_auth_catchup(netsock::socket_t listener, InputProducer& producer, CommandQueue& queue,
                             CredentialTable& creds, std::size_t min_initial, int accept_deadline_ms,
                             const std::function<void(std::size_t)>& on_frame, std::size_t cap_bytes,
                             std::size_t liveness_frames, std::size_t catchup_window) {
    Stats st;
    std::vector<AuthCatchupClient> clients;
    // Retained catch-up history: the produced payloads still available to replay to a mid-stream joiner.
    // Bounded to the last catchup_window (window==0 = retain all). broadcast.cpp's `history`.
    std::vector<std::vector<std::uint8_t>> history;

    // Decode one client's burst of reassembled upstream records into the CommandQueue, but ONLY for
    // commands naming this client's own seat (authorization). A foreign-aircraft command — or any command
    // from a spectator — is dropped as unauthorized (byte-identical to never arriving). authasyncserver's
    // submit_payloads verbatim.
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
    // blocking — the one non-async touch, matching layers 21/22), authenticate the token to a DESIGNATED
    // seat, go non-blocking, and ENQUEUE the client's one-time BIND-001 record as the first bytes of its
    // send buffer. Then — the layer-20/23 addition — the retained catch-up prefix (the current `history`,
    // itself already windowed) is enqueued right after the BIND, before the joiner enters the live stream.
    // A missing/malformed HELLO drops the joiner before it takes a seat; a fatal flush of the handshake OR
    // any replayed prefix frame, or a replay frame that trips the byte-cap, drops the joiner BEFORE it
    // becomes a member (its seat returned): a cap shed is counted `capped`, but it is NOT a join and NOT a
    // leave. Any command frames pipelined AFTER the HELLO in the same read are submitted immediately
    // (authorized to the resolved seat) once the joiner is confirmed a member.
    auto accept_all = [&]() {
        while (true) {
            netsock::socket_t c = netsock::accept_one(listener);
            if (!netsock::is_valid(c)) break;
            AuthCatchupClient ac;
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
            bool alive = enqueue_bytes(ac, framed);  // BIND-001 first (fatal-only, like layer 22)

            // 3) catch-up: replay the retained prefix right after the BIND (broadcast_bound_catchup's
            // replay, with the cap per replayed frame). history is already bounded to the last window.
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

            // 4) submit any command frames pipelined after the HELLO in the same read (authorized).
            if (upframes.size() > 1) {
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
        for (AuthCatchupClient& c : clients) netsock::close_socket(c.s);
        return st;  // ok stays false
    }

    // --- bidirectional frame loop: read upstream commands, step the producer, enqueue the frame down,
    // retain it for catch-up. Nothing here blocks on any single client. --------------------------------
    std::vector<netsock::socket_t> rfds, wfds, readable, writable;
    std::vector<std::vector<std::uint8_t>> pv;
    std::vector<std::uint8_t> payload, frame;
    std::uint8_t rbuf[4096];
    for (std::size_t fi = 0;; ++fi) {
        if (on_frame) on_frame(fi);  // rendezvous hook: block until a client has sent its commands

        // one select_rw over {listener} u {all clients readable} u {pending clients writable}
        rfds.clear();
        wfds.clear();
        for (const AuthCatchupClient& c : clients) {
            rfds.push_back(c.s);
            if (c.pending()) wfds.push_back(c.s);
        }
        rfds.push_back(listener);
        if (netsock::select_rw(rfds, wfds, 0, readable, writable)) {
            if (in_set(readable, listener)) accept_all();  // a mid-stream joiner gets BIND + catch-up prefix

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

        // enqueue frame fi downstream to every live client; a fatal send drops it, and a client the enqueue
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

        // retain this frame for late-join catch-up, evicting the oldest past the window (Stats.trimmed).
        // broadcast.cpp's retention: after the frame is enqueued to the current members.
        history.push_back(std::move(payload));
        if (catchup_window > 0 && history.size() > catchup_window) {
            history.erase(history.begin());
            ++st.trimmed;
        }
    }

    // --- bounded DRAIN: flush the stragglers' pending downstream buffers (authasyncserver's tail).
    int idle = 0;
    while (idle < 600) {
        rfds.clear();
        wfds.clear();
        for (const AuthCatchupClient& c : clients) {
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

    for (AuthCatchupClient& c : clients) netsock::close_socket(c.s);
    st.ok = true;  // initial gather succeeded and the producer ran to its end
    return st;
}

}  // namespace netinput
}  // namespace seads
