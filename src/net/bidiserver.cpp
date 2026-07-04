// SEADS bidirectional server (netcode layer 16). See bidiserver.h.
//
// This is broadcast_input's loop (inputserver.cpp) with broadcast_live's downstream machinery
// (broadcast.cpp, layers 11/12/15a) folded in — reimplemented here as a SIBLING so the sealed
// broadcast.cpp is byte-for-byte untouched. The helpers below (flush_client / over_cap /
// enqueue_bytes / drop_client / reap_dead) mirror broadcast.cpp's static helpers exactly; the only new
// wrinkle is BidiClient carrying an upstream StreamReassembler beside its downstream send buffer, and
// the readable-client handling draining COMMANDS into the CommandQueue (not just probing for EOF).
#include "bidiserver.h"

#include <cstddef>
#include <utility>
#include <vector>

#include "framing.h"
#include "input001.h"

namespace seads {
namespace netinput {
namespace {

// One connected client: its socket, an upstream reassembler (client->server INPUT-001 command bytes),
// and a downstream userspace send buffer buf[off:] (the pending tail the kernel has not yet accepted).
// The three liveness fields mirror broadcast.cpp::BufClient and are inert when liveness_frames==0.
struct BidiClient {
    netsock::socket_t s;
    framing::StreamReassembler rx;    // upstream: reassemble whole INPUT-001 records from TCP chunks
    std::vector<std::uint8_t> buf;    // downstream: pending send bytes
    std::size_t off = 0;
    std::size_t sent_total = 0;       // cumulative bytes the kernel has accepted (receive-progress signal)
    std::size_t idle_frames = 0;      // consecutive produced frames with no progress
    std::size_t last_sent = 0;        // sent_total snapshot at the last liveness check
    bool pending() const { return off < buf.size(); }
    std::size_t pending_bytes() const { return buf.size() - off; }
};

// Push as much of the pending tail as the kernel will take now (broadcast.cpp::flush_client verbatim).
// Returns false only on a fatal send error; a full kernel buffer (send_some==0) is not an error.
bool flush_client(BidiClient& c) {
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
bool over_cap(const BidiClient& c, std::size_t cap_bytes) {
    return cap_bytes > 0 && c.pending_bytes() > cap_bytes;
}

// Append bytes to the downstream queue and opportunistically flush. False on a fatal send error.
bool enqueue_bytes(BidiClient& c, const std::vector<std::uint8_t>& bytes) {
    c.buf.insert(c.buf.end(), bytes.begin(), bytes.end());
    return flush_client(c);
}

void drop_client(std::vector<BidiClient>& clients, std::size_t i, Stats& st) {
    netsock::close_socket(clients[i].s);
    clients.erase(clients.begin() + static_cast<std::ptrdiff_t>(i));
    ++st.leaves;
}

// Layer-15a liveness reap (once per produced frame, after the frame is enqueued/flushed): a client
// that made NO receive progress — buffer non-empty AND no bytes left the kernel since the last check —
// for more than liveness_frames consecutive frames is presumed dead and dropped (reaped + leave). A
// client fully drained (!pending) or that advanced sent_total this frame resets its idle counter, so a
// slow-but-alive client is never reaped. liveness_frames==0 disables the policy bit-for-bit.
void reap_dead(std::vector<BidiClient>& clients, Stats& st, std::size_t liveness_frames) {
    if (liveness_frames == 0) return;
    for (std::size_t i = clients.size(); i-- > 0;) {
        BidiClient& c = clients[i];
        if (!c.pending() || c.sent_total > c.last_sent) {
            c.idle_frames = 0;
            c.last_sent = c.sent_total;  // progress observed: the deadline restarts
        } else if (++c.idle_frames > liveness_frames) {
            drop_client(clients, i, st);
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

Stats broadcast_bidi(netsock::socket_t listener, InputProducer& producer, CommandQueue& queue,
                     std::size_t min_initial, int accept_deadline_ms,
                     const std::function<void(std::size_t)>& on_frame, std::size_t cap_bytes,
                     std::size_t liveness_frames) {
    Stats st;
    std::vector<BidiClient> clients;

    // Decode a burst of reassembled upstream records into the CommandQueue (broadcast_input verbatim).
    auto submit_payloads = [&](const std::vector<std::vector<std::uint8_t>>& payloads) {
        for (const auto& p : payloads) {
            input001::InputCommand ic;
            std::size_t pos = 0;
            if (!input001::decode_command(p.data(), p.size(), pos, ic)) continue;  // skip malformed
            switch (queue.submit(ic)) {
                case CommandQueue::Result::ACCEPTED: ++st.cmds_ok; break;
                case CommandQueue::Result::STALE: ++st.cmds_stale; break;
                case CommandQueue::Result::OUT_OF_RANGE: ++st.cmds_oob; break;
            }
        }
    };

    // Accept every pending connection (non-blocking listener). Each joiner goes non-blocking so its
    // downstream send never wedges the loop. No catch-up prefix (see the honest-scope note in the header).
    auto accept_all = [&]() {
        while (true) {
            netsock::socket_t c = netsock::accept_one(listener);
            if (!netsock::is_valid(c)) break;
            netsock::set_nonblocking(c);
            BidiClient bc;
            bc.s = c;
            clients.push_back(std::move(bc));
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
        for (BidiClient& c : clients) netsock::close_socket(c.s);
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
        for (const BidiClient& c : clients) {
            rfds.push_back(c.s);
            if (c.pending()) wfds.push_back(c.s);
        }
        rfds.push_back(listener);
        if (netsock::select_rw(rfds, wfds, 0, readable, writable)) {
            if (in_set(readable, listener)) accept_all();

            // flush writable clients (push pending downstream; a fatal flush drops the client)
            for (std::size_t i = clients.size(); i-- > 0;)
                if (in_set(writable, clients[i].s) && !flush_client(clients[i]))
                    drop_client(clients, i, st);

            // drain readable clients: a readable client is USUALLY sending upstream command bytes
            // (n>0, fed to the queue); only recv<=0 is a LEAVE. Drain all currently-available bytes so
            // a whole command burst is ingested before the ticks it governs are stepped this iteration.
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
                    submit_payloads(pv);
                } while (netsock::wait_readable(clients[i].s, 0));
                if (leave) drop_client(clients, i, st);
            }
        }

        // produce the next authoritative frame (steps the sim, consuming this batch's commands)
        std::int64_t emit_tick = 0;
        if (!producer.next(emit_tick, payload)) break;

        // enqueue frame fi downstream to every client; a fatal send drops it, and a client the enqueue
        // leaves above the byte-cap is shed (layer-12 drop-slowest: counted capped + leave).
        frame.clear();
        framing::encode_frame(payload, frame);
        for (std::size_t i = clients.size(); i-- > 0;) {
            if (!enqueue_bytes(clients[i], frame)) {
                drop_client(clients, i, st);
            } else if (over_cap(clients[i], cap_bytes)) {
                ++st.capped;
                drop_client(clients, i, st);
            }
        }
        // layer-15a: reap any client gone silent (no receive progress) past the liveness deadline.
        reap_dead(clients, st, liveness_frames);
        ++st.frames_sent;
    }

    // --- bounded DRAIN: flush the stragglers' pending downstream buffers (broadcast_async's tail).
    // Progress-bound (finite bytes owed) plus an idle cap: ~30 s of consecutive readiness-free
    // timeouts => the still-pending clients are dropped as leaves (fail-not-wedge). --------------
    int idle = 0;
    while (idle < 600) {
        rfds.clear();
        wfds.clear();
        for (const BidiClient& c : clients) {
            rfds.push_back(c.s);
            if (c.pending()) wfds.push_back(c.s);
        }
        if (wfds.empty()) break;  // every buffer drained
        if (netsock::select_rw(rfds, wfds, 50, readable, writable)) {
            for (std::size_t i = clients.size(); i-- > 0;)
                if (in_set(writable, clients[i].s) && !flush_client(clients[i]))
                    drop_client(clients, i, st);
            idle = 0;  // progress observed: only an unbroken silent stretch counts against the cap
        } else {
            ++idle;
        }
    }
    for (std::size_t i = clients.size(); i-- > 0;)
        if (clients[i].pending()) drop_client(clients, i, st);  // drain deadline: still owed bytes

    for (BidiClient& c : clients) netsock::close_socket(c.s);
    st.ok = true;  // initial gather succeeded and the producer ran to its end
    return st;
}

}  // namespace netinput
}  // namespace seads
