// SEADS multi-client fan-out determinism BRIDGE for the socket transport (netcode LAYER 8).
//
// Layer 7 proved a SINGLE client over a real 127.0.0.1 socket reconstructs SESSION-SK-001 to the
// exact in-process digest. Layer 8 proves the fan-out case: ONE server, N INDEPENDENT client
// connections (each its own TCP socket + StreamReassembler), all reconstruct the byte-identical
// digest — i.e. broadcasting the frame stream to many clients adds ZERO information and ZERO
// nondeterminism per client, regardless of connect order or how each client's stream is chunked.
//
// The server binds :0, then accepts N connections with a NON-BLOCKING accept loop (the layer-8
// primitives set_nonblocking + wait_readable, so an absent client never wedges the server — a
// finite deadline fails the leg instead), and then BROADCASTS the identical lossless frame stream
// (build_server_frames — every frame incl. tick 0) to each client. Each client applies the sealed
// integer lag + drop set purely from each frame's decoded server_tick (never wall-clock), so
// run_client over the socket-delivered list is byte-identical to the in-process reference for EVERY
// client. A finite watchdog fails (not wedges) on any socket hang. Exit 0 PASS, 1 FAIL.
//
// TRANSPORT-ONLY: no kernel/det_math/rails/wire/golden touched; rides seal v1.17r0.
#include "session.h"
#include "session_vectors.h"
#include "framing.h"
#include "socket.h"
#include "snapshot.h"
#include "golden_params.h"
#include "kernel.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace seads;

static const int kNumClients = 3;  // small fan-out; enough to prove per-client determinism

static Rails sealed_rails() {
    Rails r;
    r.R = golden::R_M;
    r.dt = golden::DT_S;
    r.g0 = golden::G0;
    r.atm_top = golden::ATM_TOP_M;
    r.soft = golden::SOFT_M;
    return r;
}

// Reassemble a whole delivered stream into the ServerFrames list run_client expects (keyed on each
// frame's DECODED server_tick — the same determinism seam layer 7 uses).
static session::ServerFrames delivered_from_payloads(
    const std::vector<std::vector<std::uint8_t>>& payloads) {
    session::ServerFrames out;
    for (const auto& p : payloads) {
        netsnap::Snapshot dec;
        std::size_t pos = 0;
        if (netsnap::decode_snapshot(p.data(), p.size(), pos, dec))
            out.emplace_back(dec.server_tick, p);
    }
    return out;
}

int main() {
    netsock::WsaGuard wsa;  // Winsock init on Windows; no-op on POSIX
    const Rails rails = sealed_rails();

    // reference: the sealed IN-PROCESS session digest (do NOT re-derive)
    const std::string expected = session::run_session(rails, sess_vec::SCENARIO, /*reconcile=*/true).digest;

    // the exact lossless frame stream every client will receive
    const session::ServerFrames frames = session::build_server_frames(rails, sess_vec::SCENARIO);
    std::vector<std::uint8_t> stream;
    for (const auto& f : frames) framing::encode_frame(f.second, stream);

    // OS-assigned-port handshake (cv, not std::future — MinGW libstdc++ call_once link quirk).
    std::mutex port_mtx;
    std::condition_variable port_cv;
    std::uint16_t port_val = 0;
    bool port_ready = false;
    auto publish_port = [&](std::uint16_t p) {
        {
            std::lock_guard<std::mutex> lk(port_mtx);
            port_val = p;
            port_ready = true;
        }
        port_cv.notify_all();  // all N client threads wait on this
    };

    std::atomic<bool> server_ok{false};
    std::atomic<int> clients_done{0};
    std::vector<std::string> got(kNumClients);

    // --- server thread: listen :0, NON-BLOCKING accept N clients, broadcast the stream to each ---
    std::thread server([&] {
        std::uint16_t port = 0;
        netsock::socket_t listener = netsock::listen_loopback(0, port, /*backlog=*/kNumClients);
        if (!netsock::is_valid(listener)) { publish_port(0); return; }
        netsock::set_nonblocking(listener);  // layer-8: accept never blocks; wait_readable gates it
        publish_port(port);

        std::vector<netsock::socket_t> conns;
        // finite deadline: up to ~15 s of 100 ms readability waits to gather all N clients
        for (int tries = 0; tries < 150 && static_cast<int>(conns.size()) < kNumClients; ++tries) {
            if (!netsock::wait_readable(listener, 100)) continue;  // timeout -> retry (non-blocking)
            netsock::socket_t c = netsock::accept_one(listener);   // ready => a pending connection
            if (netsock::is_valid(c)) conns.push_back(c);          // else spurious wakeup -> retry
        }
        if (static_cast<int>(conns.size()) == kNumClients) {
            bool ok = true;
            for (netsock::socket_t c : conns) ok = netsock::send_all(c, stream) && ok;  // fan-out
            server_ok = ok;
        }
        for (netsock::socket_t c : conns) netsock::close_socket(c);  // clean EOF -> clients' recv end
        netsock::close_socket(listener);
    });

    // --- N client threads: each connects, reassembles its OWN stream, reconstructs the dogfight ---
    std::vector<std::thread> clients;
    for (int i = 0; i < kNumClients; ++i) {
        clients.emplace_back([&, i] {
            std::uint16_t port;
            {
                std::unique_lock<std::mutex> lk(port_mtx);
                port_cv.wait(lk, [&] { return port_ready; });
                port = port_val;
            }
            if (port == 0) { ++clients_done; return; }
            netsock::socket_t s = netsock::connect_loopback(port);
            if (!netsock::is_valid(s)) { ++clients_done; return; }

            framing::StreamReassembler r;
            std::vector<std::vector<std::uint8_t>> payloads;
            std::uint8_t buf[4096];
            bool stream_ok = true;
            while (true) {
                std::ptrdiff_t n = netsock::recv_some(s, buf, sizeof(buf));
                if (n > 0) {
                    if (!r.feed(buf, static_cast<std::size_t>(n), payloads)) { stream_ok = false; break; }
                } else {
                    break;  // clean EOF / error
                }
            }
            netsock::close_socket(s);

            if (stream_ok && r.pending() == 0) {
                got[i] = session::run_client(rails, sess_vec::SCENARIO,
                                             delivered_from_payloads(payloads), /*reconcile=*/true)
                             .digest;
            }
            ++clients_done;
        });
    }

    // --- watchdog: fail (don't wedge) if a socket op ever hangs ---
    std::thread watchdog([&] {
        for (int i = 0; i < 400 && clients_done.load() < kNumClients; ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));  // up to ~40 s
        if (clients_done.load() < kNumClients) {
            std::printf("FAIL multi-client bridge TIMED OUT (socket hang)\n");
            std::_Exit(1);
        }
    });
    watchdog.detach();

    server.join();
    for (auto& t : clients) t.join();

    int fails = 0;
    if (!server_ok.load()) { ++fails; std::printf("FAIL server failed to accept/broadcast to %d clients\n", kNumClients); }
    for (int i = 0; i < kNumClients; ++i) {
        if (got[i].empty()) {
            ++fails;
            std::printf("FAIL client %d produced no digest (socket path failed)\n", i);
        } else if (got[i] != expected) {
            ++fails;
            std::printf("FAIL client %d digest != in-process session digest:\n  got %s\n  exp %s\n",
                        i, got[i].c_str(), expected.c_str());
        }
    }

    if (fails == 0) {
        std::printf("PASS: layer-8 fan-out — %d independent clients each reconstruct SESSION-SK-001 "
                    "bit-for-bit over real 127.0.0.1 sockets (%zu frames broadcast); all digests "
                    "match the in-process session\n  digest %s\n",
                    kNumClients, frames.size(), expected.c_str());
        return 0;
    }
    std::printf("RESULT: multi-client bridge FAIL (%d mismatches)\n", fails);
    return 1;
}
