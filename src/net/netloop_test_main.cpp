// SEADS loopback determinism BRIDGE for the cross-process socket transport (netcode layer 7).
//
// Proves that shipping the SESSION-SK-001 frames over a REAL OS socket (two threads, an OS-assigned
// 127.0.0.1 port) and reassembling them with the layer-7 length-prefix codec reconstructs the
// dogfight to the IDENTICAL digest as the sealed in-process session — i.e. sockets + framing add
// ZERO information and ZERO nondeterminism. The reference is NOT re-derived: it is
// session::run_session(rails, SESSION-SK-001, reconcile=true).digest computed in-process here.
//
// The bridge reproduces the EXACT sealed lossy scenario (OPTION 2a): the server sends EVERY emitted
// frame LOSSLESSLY over the socket (including the tick-0 pre-step frame); the CLIENT applies the
// sealed integer lag + drop set purely from each frame's server_tick tag (server_tick rides inside
// the protocol-6 payload). Timing/jitter/coalescing cannot change a bit: the client keys the whole
// reconstruction on t / server_tick, never on wall-clock arrival — so run_client over the
// socket-delivered frame list is byte-identical to run_client over the in-process list.
//
// A finite watchdog fails the leg rather than wedging if a socket op ever hangs.
// Exit 0 PASS, 1 FAIL.
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

static Rails sealed_rails() {
    Rails r;
    r.R = golden::R_M;
    r.dt = golden::DT_S;
    r.g0 = golden::G0;
    r.atm_top = golden::ATM_TOP_M;
    r.soft = golden::SOFT_M;
    return r;
}

int main() {
    netsock::WsaGuard wsa;  // Winsock init on Windows; no-op on POSIX
    const Rails rails = sealed_rails();

    // --- reference: the sealed IN-PROCESS session digest (do NOT re-derive) ---
    const std::string expected = session::run_session(rails, sess_vec::SCENARIO, /*reconcile=*/true).digest;

    // --- the exact server frame stream the socket path will carry losslessly ---
    const session::ServerFrames frames = session::build_server_frames(rails, sess_vec::SCENARIO);

    // OS-assigned-port handshake: server binds :0, publishes the chosen port under a mutex +
    // condition_variable so the client connects only once it is known (a plain cv handshake — no
    // std::future, which pulls call_once and trips some MinGW libstdc++ links).
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
        port_cv.notify_one();
    };

    std::atomic<bool> server_ok{false};
    std::atomic<bool> client_done{false};
    std::string got;

    // --- server thread: listen 127.0.0.1:0, hand the OS port to the client, send every frame ---
    std::thread server([&] {
        std::uint16_t port = 0;
        netsock::socket_t listener = netsock::listen_loopback(0, port);
        if (!netsock::is_valid(listener)) { publish_port(0); return; }
        publish_port(port);  // ready-signal handshake: client connects only after this
        netsock::socket_t conn = netsock::accept_one(listener);
        if (!netsock::is_valid(conn)) { netsock::close_socket(listener); return; }
        bool ok = true;
        for (const auto& f : frames) {
            std::vector<std::uint8_t> framed;
            framing::encode_frame(f.second, framed);  // LEB128(len) || payload
            ok = netsock::send_all(conn, framed) && ok;
        }
        netsock::close_socket(conn);      // clean EOF -> client's recv loop ends
        netsock::close_socket(listener);
        server_ok = ok;
    });

    // --- client thread: connect, reassemble the frame stream, reconstruct the dogfight ---
    std::thread client([&] {
        std::uint16_t port;
        {
            std::unique_lock<std::mutex> lk(port_mtx);
            port_cv.wait(lk, [&] { return port_ready; });
            port = port_val;
        }
        if (port == 0) { client_done = true; return; }
        netsock::socket_t s = netsock::connect_loopback(port);
        if (!netsock::is_valid(s)) { client_done = true; return; }

        framing::StreamReassembler r;
        std::vector<std::vector<std::uint8_t>> payloads;
        std::uint8_t buf[4096];
        bool stream_ok = true;
        while (true) {
            std::ptrdiff_t n = netsock::recv_some(s, buf, sizeof(buf));
            if (n > 0) {
                if (!r.feed(buf, static_cast<std::size_t>(n), payloads)) { stream_ok = false; break; }
            } else {
                break;  // n==0 clean EOF (all frames sent) / n<0 error
            }
        }
        netsock::close_socket(s);

        if (stream_ok && r.pending() == 0) {
            // rebuild the ServerFrames list keyed on each frame's DECODED server_tick — this is the
            // determinism bridge: run_client keys on emit_tick, so a socket-delivered list is
            // reconstructed identically to the in-process one.
            session::ServerFrames delivered;
            for (const auto& p : payloads) {
                netsnap::Snapshot dec;
                std::size_t pos = 0;
                if (netsnap::decode_snapshot(p.data(), p.size(), pos, dec)) {
                    delivered.emplace_back(dec.server_tick, p);
                }
            }
            got = session::run_client(rails, sess_vec::SCENARIO, delivered, /*reconcile=*/true).digest;
        }
        client_done = true;
    });

    // --- watchdog: fail (don't wedge) if a socket op ever hangs ---
    std::thread watchdog([&] {
        for (int i = 0; i < 300 && !client_done.load(); ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));  // up to ~30 s
        if (!client_done.load()) {
            std::printf("FAIL netloop bridge TIMED OUT (socket hang)\n");
            std::_Exit(1);
        }
    });
    watchdog.detach();

    server.join();
    client.join();

    int fails = 0;
    if (!server_ok.load()) { ++fails; std::printf("FAIL server failed to send all frames\n"); }
    if (got.empty()) { ++fails; std::printf("FAIL client produced no digest (socket path failed)\n"); }
    if (got != expected) {
        ++fails;
        std::printf("FAIL socket-path digest != in-process session digest:\n  got %s\n  exp %s\n",
                    got.c_str(), expected.c_str());
    }

    if (fails == 0) {
        std::printf("PASS: layer-7 socket transport reconstructs SESSION-SK-001 bit-for-bit "
                    "(%zu frames over a real 127.0.0.1 socket); digest matches the in-process "
                    "session\n  digest %s\n", frames.size(), expected.c_str());
        return 0;
    }
    std::printf("RESULT: netloop bridge FAIL (%d mismatches)\n", fails);
    return 1;
}
