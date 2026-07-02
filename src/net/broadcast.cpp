// SEADS single-thread select-based fan-out broadcast server (netcode layer 9). See broadcast.h.
#include "broadcast.h"

#include "framing.h"

#include <cstdint>
#include <vector>

namespace seads {
namespace netbcast {

using netsock::socket_t;

// Accept every pending connection on a non-blocking listener (drain the accept queue). Each new
// socket is appended to `clients` and counted as a join. Safe to call every frame: on an empty
// queue accept_one returns invalid and we stop.
static void accept_pending(socket_t listener, std::vector<socket_t>& clients, Stats& st) {
    while (true) {
        socket_t c = netsock::accept_one(listener);
        if (!netsock::is_valid(c)) break;  // no more pending (EWOULDBLOCK on a non-blocking listener)
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
                       const std::function<void(std::size_t)>& on_frame) {
    Stats st;
    std::vector<socket_t> clients;

    // --- gather the initial clients (bounded wait) before frame 0 -----------------------------
    // 100 ms readability slices up to accept_deadline_ms total, so an absent client fails (returns
    // with ok=false) rather than wedging the server.
    int waited = 0;
    while (clients.size() < min_initial && waited < accept_deadline_ms) {
        if (netsock::wait_readable(listener, 100)) accept_pending(listener, clients, st);
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
            if (listener_ready) accept_pending(listener, clients, st);
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

}  // namespace netbcast
}  // namespace seads
