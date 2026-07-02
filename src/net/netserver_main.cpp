// SEADS layer-7 demo SERVER (cross-process socket transport). Drives the sealed kernel over
// SESSION-SK-001, then serves every 20 Hz protocol-6 snapshot frame — each wrapped in the layer-7
// length prefix — to one connecting client over TCP. A companion to seads_netclient; the loopback
// determinism BRIDGE (seads_netloop_test) is what CI gates, this pair is the human two-process demo.
//
// Usage:  seads_netserver [port]        (port 0 or omitted => OS-assigned; the chosen port is printed)
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

    const Rails rails = sealed_rails();
    const session::ServerFrames frames = session::build_server_frames(rails, sess_vec::SCENARIO);

    std::uint16_t port = 0;
    netsock::socket_t listener = netsock::listen_loopback(req_port, port);
    if (!netsock::is_valid(listener)) {
        std::printf("ERROR: could not bind/listen on 127.0.0.1:%u\n", req_port);
        return 1;
    }
    std::printf("seads_netserver: listening on 127.0.0.1:%u (%zu frames ready)\n",
                port, frames.size());
    std::fflush(stdout);

    netsock::socket_t conn = netsock::accept_one(listener);
    if (!netsock::is_valid(conn)) {
        std::printf("ERROR: accept failed\n");
        netsock::close_socket(listener);
        return 1;
    }
    std::printf("seads_netserver: client connected; streaming...\n");

    // one contiguous stream: concat of LEB128(len)||payload frames
    std::vector<std::uint8_t> stream;
    for (const auto& f : frames) framing::encode_frame(f.second, stream);
    bool ok = netsock::send_all(conn, stream);

    netsock::close_socket(conn);
    netsock::close_socket(listener);
    if (!ok) { std::printf("ERROR: send failed\n"); return 1; }
    std::printf("seads_netserver: sent %zu frames (%zu bytes); done\n", frames.size(), stream.size());
    return 0;
}
