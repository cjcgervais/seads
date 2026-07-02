// SEADS layer-7 demo CLIENT (cross-process socket transport). Connects to a seads_netserver, reads
// the length-prefixed protocol-6 frame stream off the TCP socket, reassembles it with the layer-7
// codec, then reconstructs the SESSION-SK-001 dogfight (own predicted + remotes interpolated + wire-
// sourced HP/kills/rounds) and prints the whole-session digest. Over a lossless loopback that digest
// equals the sealed in-process session digest — the same invariant seads_netloop_test gates in CI.
//
// Usage:  seads_netclient <port>
#include "session.h"
#include "session_vectors.h"
#include "framing.h"
#include "socket.h"
#include "snapshot.h"
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
    if (argc < 2) {
        std::printf("usage: seads_netclient <port>\n");
        return 2;
    }
    netsock::WsaGuard wsa;
    std::uint16_t port = static_cast<std::uint16_t>(std::atoi(argv[1]));

    netsock::socket_t s = netsock::connect_loopback(port);
    if (!netsock::is_valid(s)) {
        std::printf("ERROR: could not connect to 127.0.0.1:%u\n", port);
        return 1;
    }

    framing::StreamReassembler r;
    std::vector<std::vector<std::uint8_t>> payloads;
    std::uint8_t buf[4096];
    while (true) {
        std::ptrdiff_t n = netsock::recv_some(s, buf, sizeof(buf));
        if (n > 0) {
            if (!r.feed(buf, static_cast<std::size_t>(n), payloads)) {
                std::printf("ERROR: malformed frame stream\n");
                netsock::close_socket(s);
                return 1;
            }
        } else {
            break;  // EOF / error
        }
    }
    netsock::close_socket(s);

    session::ServerFrames delivered;
    for (const auto& p : payloads) {
        netsnap::Snapshot dec;
        std::size_t pos = 0;
        if (netsnap::decode_snapshot(p.data(), p.size(), pos, dec))
            delivered.emplace_back(dec.server_tick, p);
    }

    const Rails rails = sealed_rails();
    const session::SessionResult res =
        session::run_client(rails, sess_vec::SCENARIO, delivered, /*reconcile=*/true);

    std::printf("seads_netclient: received %zu frames; reconstructed %zu ticks\n",
                delivered.size(), res.per_tick.size());
    std::printf("seads_netclient: session digest %s\n", res.digest.c_str());
    return 0;
}
