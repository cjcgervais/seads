// SEADS layer-7/8/9 demo SERVER (cross-process socket transport). Drives the sealed kernel over
// SESSION-SK-001, then serves every 20 Hz protocol-6 snapshot frame — each wrapped in the layer-7
// length prefix — to connecting clients over TCP. Layer 9: a single-thread select()-based
// broadcast (netbcast::broadcast_select) with DYNAMIC join/leave — it waits for num_clients to
// connect, then streams, ALSO accepting any late joiner (who receives every frame from its join
// point onward) and dropping any client that closes. A companion to seads_netclient; the loopback
// determinism BRIDGEs (seads_netloop_test / seads_multiclient_test / seads_netdyn_test) are what CI
// gates, this pair is the human demo.
//
// Usage:  seads_netserver [port] [num_clients] [catchup] [async]
//   port 0 or omitted => OS-assigned (the chosen port is printed); num_clients defaults to 1.
//   catchup 0/1 (default 0): with 1 (layer 10) a client that joins mid-stream is first replayed the
//   missed prefix frames[0:join], so it too reconstructs the WHOLE dogfight; with 0 (layer 9) a late
//   joiner gets only the remaining frames.
//   async 0/1 (default 0): with 1 (layer 11) output is non-blocking through per-client userspace
//   send buffers (broadcast_async) — a slow client cannot stall the other clients' stream; with 0
//   sends are blocking (broadcast_select, layers 9/10).
//   The server waits for num_clients connection(s), then broadcasts the identical frame stream to
//   each — every client present from the start reconstructs the same dogfight.
#include "session.h"
#include "session_vectors.h"
#include "broadcast.h"
#include "socket.h"
#include "golden_params.h"
#include "kernel.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

using namespace seads;

static Rails sealed_rails() {
    Rails r;
    r.R = golden::R_M;
    r.dt = golden::DT_S;
    r.g0 = golden::G0;
    r.atm_top = golden::ATM_TOP_M;
    r.soft = golden::SOFT_M;
    return r;
}

int main(int argc, char** argv) {
    netsock::WsaGuard wsa;
    std::uint16_t req_port = 0;
    if (argc > 1) req_port = static_cast<std::uint16_t>(std::atoi(argv[1]));
    int num_clients = 1;
    if (argc > 2) num_clients = std::atoi(argv[2]);
    if (num_clients < 1) num_clients = 1;
    bool catchup = (argc > 3) && std::atoi(argv[3]) != 0;
    bool use_async = (argc > 4) && std::atoi(argv[4]) != 0;

    const Rails rails = sealed_rails();
    const session::ServerFrames frames = session::build_server_frames(rails, sess_vec::SCENARIO);

    std::uint16_t port = 0;
    netsock::socket_t listener = netsock::listen_loopback(req_port, port, /*backlog=*/num_clients);
    if (!netsock::is_valid(listener)) {
        std::printf("ERROR: could not bind/listen on 127.0.0.1:%u\n", req_port);
        return 1;
    }
    std::printf("seads_netserver: listening on 127.0.0.1:%u (%zu frames ready, expecting %d client(s), "
                "catchup=%d, async=%d)\n",
                port, frames.size(), num_clients, catchup ? 1 : 0, use_async ? 1 : 0);
    std::fflush(stdout);

    // the raw per-frame payloads (broadcast_select length-prefixes each so joiners stay frame-aligned)
    std::vector<std::vector<std::uint8_t>> payloads;
    for (const auto& f : frames) payloads.push_back(f.second);

    // layer-9/10/11 single-thread select() broadcast: wait for num_clients, then stream, accepting
    // any late joiner (catchup? whole stream via prefix replay : just the suffix) and dropping any
    // client that closes. async => layer-11 non-blocking output (per-client userspace send buffers;
    // a slow client can't stall the rest). 60 s bounded initial-wait. Same shared loops the CI
    // bridges gate — no untested divergence.
    netsock::set_nonblocking(listener);
    netbcast::Stats st =
        use_async ? netbcast::broadcast_async(listener, payloads,
                                              static_cast<std::size_t>(num_clients),
                                              /*accept_deadline_ms=*/60000, /*on_frame=*/{}, catchup)
                  : netbcast::broadcast_select(listener, payloads,
                                               static_cast<std::size_t>(num_clients),
                                               /*accept_deadline_ms=*/60000, /*on_frame=*/{}, catchup);
    netsock::close_socket(listener);

    if (!st.ok) {
        std::printf("ERROR: broadcast failed (only %zu of %d client(s) connected in time)\n",
                    st.joins, num_clients);
        return 1;
    }
    std::printf("seads_netserver: broadcast %zu frames (joins=%zu, leaves=%zu); done\n",
                st.frames_sent, st.joins, st.leaves);
    return 0;
}
