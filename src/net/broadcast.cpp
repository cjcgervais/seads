// SEADS single-thread select-based fan-out broadcast server (netcode layer 9). See broadcast.h.
#include "broadcast.h"

#include "framing.h"

#include <cstdint>
#include <utility>
#include <vector>

namespace seads {
namespace netbcast {

using netsock::socket_t;

// Replay the missed prefix frames[0:upto] (each length-prefixed, atomically) to a just-accepted
// late joiner so it too receives the WHOLE stream (layer-10 catch-up). Returns false if the joiner
// dies mid-replay (it then never enters the live broadcast set). A synchronous burst — bounded by
// the prefix length.
static bool catch_up_client(socket_t c, const std::vector<std::vector<std::uint8_t>>& payloads,
                            std::size_t upto) {
    std::vector<std::uint8_t> frame;
    for (std::size_t i = 0; i < upto; ++i) {
        frame.clear();
        framing::encode_frame(payloads[i], frame);
        if (!netsock::send_all(c, frame)) return false;
    }
    return true;
}

// Accept every pending connection on a non-blocking listener (drain the accept queue). Each new
// socket is appended to `clients` and counted as a join. Safe to call every frame: on an empty
// queue accept_one returns invalid and we stop. When `catchup` and `upto>0`, a new joiner is first
// replayed the missed prefix frames[0:upto] (layer 10); a joiner that dies during that replay is
// closed and never enters the broadcast set (no join counted). During the initial gather (upto==0)
// nothing is replayed — those clients are present from frame 0 and get everything live.
static void accept_pending(socket_t listener, std::vector<socket_t>& clients, Stats& st,
                           const std::vector<std::vector<std::uint8_t>>& payloads,
                           std::size_t upto, bool catchup) {
    while (true) {
        socket_t c = netsock::accept_one(listener);
        if (!netsock::is_valid(c)) break;  // no more pending (EWOULDBLOCK on a non-blocking listener)
        if (catchup && upto > 0 && !catch_up_client(c, payloads, upto)) {
            netsock::close_socket(c);  // joiner died during prefix replay; never becomes live
            continue;
        }
        clients.push_back(c);
        ++st.joins;
    }
}

// Drop any client that has closed: a readable client with recv()==0 is at EOF (LEAVE). `ready` is
// the select() subset; we only probe clients in it (an idle live client is never readable here,
// since the broadcast is server->client only, so it is never mistaken for a leaver).
static void reap_leavers(std::vector<socket_t>& clients, const std::vector<socket_t>& ready,
                         Stats& st) {
    std::uint8_t sink[512];
    for (std::size_t i = clients.size(); i-- > 0;) {
        socket_t c = clients[i];
        bool is_ready = false;
        for (socket_t r : ready)
            if (r == c) { is_ready = true; break; }
        if (!is_ready) continue;
        std::ptrdiff_t n = netsock::recv_some(c, sink, sizeof(sink));
        if (n <= 0) {  // 0 = clean EOF, <0 = error: the peer is gone
            netsock::close_socket(c);
            clients.erase(clients.begin() + static_cast<std::ptrdiff_t>(i));
            ++st.leaves;
        }
        // n>0 (unexpected client->server bytes) is ignored: this is a one-way broadcast.
    }
}

Stats broadcast_select(socket_t listener,
                       const std::vector<std::vector<std::uint8_t>>& payloads,
                       std::size_t min_initial, int accept_deadline_ms,
                       const std::function<void(std::size_t)>& on_frame, bool catchup) {
    Stats st;
    std::vector<socket_t> clients;

    // --- gather the initial clients (bounded wait) before frame 0 -----------------------------
    // 100 ms readability slices up to accept_deadline_ms total, so an absent client fails (returns
    // with ok=false) rather than wedging the server. upto==0 => no catch-up replay (these clients
    // are present from frame 0).
    int waited = 0;
    while (clients.size() < min_initial && waited < accept_deadline_ms) {
        if (netsock::wait_readable(listener, 100))
            accept_pending(listener, clients, st, payloads, /*upto=*/0, catchup);
        waited += 100;
    }
    if (clients.size() < min_initial) {
        for (socket_t c : clients) netsock::close_socket(c);
        return st;  // ok stays false
    }

    // --- stream every frame to whoever is connected, servicing joins/leaves each iteration ------
    std::vector<socket_t> ready;
    std::vector<std::uint8_t> frame;
    for (std::size_t fi = 0; fi < payloads.size(); ++fi) {
        if (on_frame) on_frame(fi);  // test hook: rendezvous a deterministic mid-stream join

        // one select() over {listener} u {clients}: listener-ready => JOIN, client-ready => LEAVE.
        std::vector<socket_t> fds = clients;
        fds.push_back(listener);
        if (netsock::select_readable(fds, 0, ready)) {
            bool listener_ready = false;
            for (socket_t r : ready)
                if (r == listener) { listener_ready = true; break; }
            // a joiner accepted here is replayed frames[0:fi] first (catch-up) so it too gets the
            // whole stream; otherwise it enters live at fi and receives only frames[fi:].
            if (listener_ready) accept_pending(listener, clients, st, payloads, /*upto=*/fi, catchup);
            reap_leavers(clients, ready, st);  // ignores the listener entry
        }

        // broadcast frame fi atomically (length-prefixed) to every current client; drop send fails.
        frame.clear();
        framing::encode_frame(payloads[fi], frame);
        for (std::size_t i = clients.size(); i-- > 0;) {
            if (!netsock::send_all(clients[i], frame)) {
                netsock::close_socket(clients[i]);
                clients.erase(clients.begin() + static_cast<std::ptrdiff_t>(i));
                ++st.leaves;
            }
        }
        ++st.frames_sent;
    }

    // --- drain any trailing leavers so a post-stream close is observed, then close everyone ------
    for (int pass = 0; pass < 3 && !clients.empty(); ++pass) {
        std::vector<socket_t> fds = clients;
        if (!netsock::select_readable(fds, 50, ready)) break;
        reap_leavers(clients, ready, st);
    }
    for (socket_t c : clients) netsock::close_socket(c);
    st.ok = (st.frames_sent == payloads.size());
    return st;
}

// ===================== layer 11: async single-thread OUTPUT =====================================
// Same delivered bytes as broadcast_select; no client can back-pressure the frame loop. Each
// client is non-blocking and owns a userspace send buffer: buf[off:] is the pending tail the
// kernel has not yet accepted. Compaction happens only when fully drained, so memory is bounded by
// the bytes still owed to that client (<= the total encoded stream).
struct BufClient {
    netsock::socket_t s;
    std::vector<std::uint8_t> buf;
    std::size_t off = 0;
    bool pending() const { return off < buf.size(); }
    std::size_t pending_bytes() const { return buf.size() - off; }
};

// Push as much of the pending tail as the kernel will take right now. Returns false only on a
// fatal send error (peer gone); a full kernel buffer (send_some == 0) is not an error — the tail
// stays queued for the next writability. The consumed prefix is compacted away so per-client
// memory tracks the pending BACKLOG (the quantity the layer-12 cap bounds), not the total bytes
// ever flushed.
static bool flush_client(BufClient& c) {
    while (c.pending()) {
        std::ptrdiff_t r = netsock::send_some(c.s, c.buf.data() + c.off, c.buf.size() - c.off);
        if (r < 0) return false;
        if (r == 0) break;  // kernel full; select_rw will report writability later
        c.off += static_cast<std::size_t>(r);
    }
    if (c.off > 0) {
        c.buf.erase(c.buf.begin(), c.buf.begin() + static_cast<std::ptrdiff_t>(c.off));
        c.off = 0;
    }
    return true;
}

// Layer-12 byte-cap policy: with cap_bytes>0, a client whose pending backlog an enqueue has left
// above the cap is beyond the drop threshold (drop-slowest). cap_bytes==0 disables the policy.
static bool over_cap(const BufClient& c, std::size_t cap_bytes) {
    return cap_bytes > 0 && c.pending_bytes() > cap_bytes;
}

// Append bytes to the client's queue and opportunistically flush (fast path: an unclogged client
// never accumulates a buffer at all). Returns false on a fatal send error.
static bool enqueue_bytes(BufClient& c, const std::vector<std::uint8_t>& bytes) {
    c.buf.insert(c.buf.end(), bytes.begin(), bytes.end());
    return flush_client(c);
}

static void drop_client(std::vector<BufClient>& clients, std::size_t i, Stats& st) {
    netsock::close_socket(clients[i].s);
    clients.erase(clients.begin() + static_cast<std::ptrdiff_t>(i));
    ++st.leaves;
}

// Async accept: drain the accept queue; each joiner goes non-blocking and — when catchup && upto>0
// — is ENQUEUED the missed prefix frames[0:upto] (not burst: unlike layer 10's catch_up_client, a
// slow joiner's replay cannot stall the loop; it drains on writability like any other pending
// bytes). A joiner whose very first flush hits a fatal error is dropped before counting as a join.
// The layer-12 cap applies to each replayed prefix frame too: a joiner it trips is closed before
// ever becoming live (counted in `capped` only — it was never a member, so it is not a leave).
static void accept_pending_async(netsock::socket_t listener, std::vector<BufClient>& clients,
                                 Stats& st,
                                 const std::vector<std::vector<std::uint8_t>>& payloads,
                                 std::size_t upto, bool catchup, std::size_t cap_bytes) {
    std::vector<std::uint8_t> frame;
    while (true) {
        netsock::socket_t c = netsock::accept_one(listener);
        if (!netsock::is_valid(c)) break;
        netsock::set_nonblocking(c);
        BufClient bc;
        bc.s = c;
        bool alive = true;
        if (catchup && upto > 0) {
            for (std::size_t i = 0; i < upto && alive; ++i) {
                frame.clear();
                framing::encode_frame(payloads[i], frame);
                alive = enqueue_bytes(bc, frame);
                if (alive && over_cap(bc, cap_bytes)) {
                    ++st.capped;
                    alive = false;
                }
            }
        }
        if (!alive) {
            netsock::close_socket(bc.s);  // died/capped on the replay; never live
            continue;
        }
        clients.push_back(std::move(bc));
        ++st.joins;
    }
}

// Async reap: identical policy to reap_leavers — a select()-readable client at recv EOF/error has
// left. (A merely SLOW client is never readable on this one-way broadcast, so it is never reaped;
// its bytes wait in the userspace buffer.)
static void reap_leavers_async(std::vector<BufClient>& clients,
                               const std::vector<netsock::socket_t>& ready, Stats& st) {
    std::uint8_t sink[512];
    for (std::size_t i = clients.size(); i-- > 0;) {
        netsock::socket_t c = clients[i].s;
        bool is_ready = false;
        for (netsock::socket_t r : ready)
            if (r == c) { is_ready = true; break; }
        if (!is_ready) continue;
        std::ptrdiff_t n = netsock::recv_some(c, sink, sizeof(sink));
        if (n <= 0) drop_client(clients, i, st);
        // n>0 (unexpected client->server bytes) is ignored: this is a one-way broadcast.
    }
}

// Flush every select()-writable client; a fatal flush drops it.
static void flush_writable(std::vector<BufClient>& clients,
                           const std::vector<netsock::socket_t>& writable, Stats& st) {
    for (std::size_t i = clients.size(); i-- > 0;) {
        bool is_writable = false;
        for (netsock::socket_t w : writable)
            if (w == clients[i].s) { is_writable = true; break; }
        if (is_writable && !flush_client(clients[i])) drop_client(clients, i, st);
    }
}

Stats broadcast_async(netsock::socket_t listener,
                      const std::vector<std::vector<std::uint8_t>>& payloads,
                      std::size_t min_initial, int accept_deadline_ms,
                      const std::function<void(std::size_t)>& on_frame, bool catchup,
                      std::size_t cap_bytes) {
    Stats st;
    std::vector<BufClient> clients;

    // --- gather the initial clients (bounded wait) before frame 0, exactly as broadcast_select ---
    int waited = 0;
    while (clients.size() < min_initial && waited < accept_deadline_ms) {
        if (netsock::wait_readable(listener, 100))
            accept_pending_async(listener, clients, st, payloads, /*upto=*/0, catchup, cap_bytes);
        waited += 100;
    }
    if (clients.size() < min_initial) {
        for (BufClient& c : clients) netsock::close_socket(c.s);
        return st;  // ok stays false
    }

    // --- frame loop: one select_rw() per frame services JOIN + LEAVE + writability flush; then
    // frame fi is ENQUEUED to every live client. Nothing here blocks on any single client. --------
    std::vector<netsock::socket_t> rfds, wfds, readable, writable;
    std::vector<std::uint8_t> frame;
    for (std::size_t fi = 0; fi < payloads.size(); ++fi) {
        if (on_frame) on_frame(fi);  // test hook: rendezvous a deterministic mid-stream join

        rfds.clear();
        wfds.clear();
        for (const BufClient& c : clients) {
            rfds.push_back(c.s);
            if (c.pending()) wfds.push_back(c.s);
        }
        rfds.push_back(listener);
        if (netsock::select_rw(rfds, wfds, 0, readable, writable)) {
            bool listener_ready = false;
            for (netsock::socket_t r : readable)
                if (r == listener) { listener_ready = true; break; }
            if (listener_ready)
                accept_pending_async(listener, clients, st, payloads, /*upto=*/fi, catchup,
                                     cap_bytes);
            reap_leavers_async(clients, readable, st);  // ignores the listener entry
            flush_writable(clients, writable, st);
        }

        // enqueue frame fi to every live client; a fatal send drops as before, and a client the
        // enqueue leaves above the byte-cap is shed (layer 12 drop-slowest: counted capped + leave).
        frame.clear();
        framing::encode_frame(payloads[fi], frame);
        for (std::size_t i = clients.size(); i-- > 0;) {
            if (!enqueue_bytes(clients[i], frame)) {
                drop_client(clients, i, st);
            } else if (over_cap(clients[i], cap_bytes)) {
                ++st.capped;
                drop_client(clients, i, st);
            }
        }
        ++st.frames_sent;
    }

    // --- bounded DRAIN: flush the stragglers' pending buffers. Progress-bound (finite bytes owed)
    // plus an idle cap: ~30 s of CONSECUTIVE timeouts with no readiness at all => the still-pending
    // clients are dropped as leaves (fail-not-wedge), mirroring a send failure. Any readiness
    // resets the cap — a slowly-reading client that is making progress is never dropped (the finite
    // stream bounds the total work). -------------------------------------------------------------
    int idle = 0;
    while (idle < 600) {
        rfds.clear();
        wfds.clear();
        for (const BufClient& c : clients) {
            rfds.push_back(c.s);
            if (c.pending()) wfds.push_back(c.s);
        }
        if (wfds.empty()) break;  // every buffer drained
        if (netsock::select_rw(rfds, wfds, 50, readable, writable)) {
            reap_leavers_async(clients, readable, st);
            flush_writable(clients, writable, st);
            idle = 0;  // progress observed: only an unbroken silent stretch counts against the cap
        } else {
            ++idle;
        }
    }
    for (std::size_t i = clients.size(); i-- > 0;)
        if (clients[i].pending()) drop_client(clients, i, st);  // drain deadline: still owed bytes

    // --- drain any trailing leavers so a post-stream close is observed, then close everyone ------
    std::vector<netsock::socket_t> ready;
    for (int pass = 0; pass < 3 && !clients.empty(); ++pass) {
        rfds.clear();
        for (const BufClient& c : clients) rfds.push_back(c.s);
        if (!netsock::select_readable(rfds, 50, ready)) break;
        reap_leavers_async(clients, ready, st);
    }
    for (BufClient& c : clients) netsock::close_socket(c.s);
    st.ok = (st.frames_sent == payloads.size());
    return st;
}

}  // namespace netbcast
}  // namespace seads
