// SEADS layer-7/8 demo SERVER (cross-process socket transport). Drives the sealed kernel over
// SESSION-SK-001, then serves every 20 Hz protocol-6 snapshot frame — each wrapped in the layer-7
// length prefix — to one OR MORE connecting clients over TCP (layer-8 fan-out). A companion to
// seads_netclient; the loopback determinism BRIDGE (seads_netloop_test / seads_multiclient_test) is
// what CI gates, this pair is the human demo.
//
// Usage:  seads_netserver [port] [num_clients]
//   port 0 or omitted => OS-assigned (the chosen port is printed); num_clients defaults to 1.
//   With num_clients>1 the server waits for that many connections (non-blocking accept), then
//   broadcasts the identical frame stream to each — every client reconstructs the same dogfight.
#include "session.h"
#include "session_vectors.h"
#include "framing.h"
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

    const Rails rails = sealed_rails();
    const session::ServerFrames frames = session::build_server_frames(rails, sess_vec::SCENARIO);

    std::uint16_t port = 0;
    netsock::socket_t listener = netsock::listen_loopback(req_port, port, /*backlog=*/num_clients);
    if (!netsock::is_valid(listener)) {
        std::printf("ERROR: could not bind/listen on 127.0.0.1:%u\n", req_port);
        return 1;
    }
    std::printf("seads_netserver: listening on 127.0.0.1:%u (%zu frames ready, expecting %d client(s))\n",
                port, frames.size(), num_clients);
    std::fflush(stdout);

    // one contiguous stream: concat of LEB128(len)||payload frames — broadcast identically to each
    std::vector<std::uint8_t> stream;
    for (const auto& f : frames) framing::encode_frame(f.second, stream);

    // gather num_clients connections (blocking accept; the layer-8 fan-out demo waits for them all)
    std::vector<netsock::socket_t> conns;
    for (int i = 0; i < num_clients; ++i) {
        netsock::socket_t conn = netsock::accept_one(listener);
        if (!netsock::is_valid(conn)) {
            std::printf("ERROR: accept failed for client %d\n", i);
            for (netsock::socket_t c : conns) netsock::close_socket(c);
            netsock::close_socket(listener);
            return 1;
        }
        conns.push_back(conn);
        std::printf("seads_netserver: client %d connected\n", i);
    }
    std::printf("seads_netserver: %d client(s) connected; broadcasting...\n", num_clients);

    bool ok = true;
    for (netsock::socket_t c : conns) ok = netsock::send_all(c, stream) && ok;

    for (netsock::socket_t c : conns) netsock::close_socket(c);
    netsock::close_socket(listener);
    if (!ok) { std::printf("ERROR: send failed\n"); return 1; }
    std::printf("seads_netserver: sent %zu frames (%zu bytes) to %d client(s); done\n",
                frames.size(), stream.size(), num_clients);
    return 0;
}
