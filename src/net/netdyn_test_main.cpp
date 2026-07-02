// SEADS dynamic-membership determinism BRIDGE for the single-thread select-broadcast server
// (netcode LAYER 9).
//
// Layer 8 proved N clients that ALL connect before streaming each reconstruct SESSION-SK-001 to the
// sealed in-process digest. Layer 9 proves the fan-out is a genuine single-threaded select() event
// loop with DYNAMIC membership: over one real broadcast, three clients with different lifetimes each
// receive EXACTLY the contiguous frame suffix defined by when they were connected —
//   * FULL   — connected before frame 0, stays to EOF: receives ALL frames => reconstructs the
//              sealed session digest (24f71845...c332), same proof as layers 7/8 but now the server
//              is driven by netbcast::broadcast_select (one select loop, no thread-per-client);
//   * LATE   — joins mid-stream (rendezvoused to an exact frame via the broadcast_select on_frame
//              hook): receives EXACTLY frames[K:] where K is the frame index it decodes from its
//              FIRST payload's server_tick — dynamic join is lossless and correctly offset (a
//              late joiner legitimately cannot reconstruct the whole fight, it missed ticks, but
//              the TRANSPORT delivered precisely the right bytes);
//   * LEAVER — connected at frame 0, reads a prefix then closes: receives a contiguous frames[0:m]
//              PREFIX, and the server drops it from the broadcast set (LEAVE detected via select +
//              recv==0) WITHOUT disturbing FULL/LATE — proving one client's departure corrupts
//              neither the stream nor the others.
//
// The join point is pinned with NO sleeps and NO timing guesses: the server pauses at frame J (the
// on_frame hook) until the late client has connected, so its suffix is deterministic; every
// assertion is checked against the delivered BYTES, not wall-clock. A finite watchdog fails (not
// wedges) on any socket hang. Exit 0 PASS, 1 FAIL.
//
// TRANSPORT-ONLY: no kernel/det_math/rails/wire/golden touched; rides seal v1.17r0.
#include "session.h"
#include "session_vectors.h"
#include "framing.h"
#include "broadcast.h"
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

// Rebuild the ServerFrames list run_client expects, keyed on each frame's DECODED server_tick.
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

// Read the whole stream to EOF, reassembling frames. Returns false on a malformed stream.
static bool collect_to_eof(netsock::socket_t s, std::vector<std::vector<std::uint8_t>>& out,
                           std::size_t& pending) {
    framing::StreamReassembler r;
    std::uint8_t buf[4096];
    bool ok = true;
    while (true) {
        std::ptrdiff_t n = netsock::recv_some(s, buf, sizeof(buf));
        if (n > 0) {
            if (!r.feed(buf, static_cast<std::size_t>(n), out)) { ok = false; break; }
        } else {
            break;  // clean EOF / error
        }
    }
    pending = r.pending();
    return ok;
}

// Read until at least `want` frames have been reassembled, then close mid-stream (a LEAVE).
static void collect_n_then_close(netsock::socket_t s, std::size_t want,
                                 std::vector<std::vector<std::uint8_t>>& out) {
    framing::StreamReassembler r;
    std::uint8_t buf[4096];
    while (out.size() < want) {
        std::ptrdiff_t n = netsock::recv_some(s, buf, sizeof(buf));
        if (n > 0) {
            if (!r.feed(buf, static_cast<std::size_t>(n), out)) break;
        } else {
            break;
        }
    }
    netsock::close_socket(s);  // close BEFORE draining the rest => the server sees EOF (a LEAVE)
}

int main() {
    netsock::WsaGuard wsa;
    const Rails rails = sealed_rails();

    // reference: sealed in-process session digest + the exact lossless frame list (payloads)
    const std::string expected =
        session::run_session(rails, sess_vec::SCENARIO, /*reconcile=*/true).digest;
    const session::ServerFrames ref_frames =
        session::build_server_frames(rails, sess_vec::SCENARIO);
    std::vector<std::vector<std::uint8_t>> ref_payloads;
    for (const auto& f : ref_frames) ref_payloads.push_back(f.second);
    const std::size_t N = ref_payloads.size();
    const std::size_t kJoinFrame = N / 2;    // LATE joins mid-stream here
    const std::size_t kLeaveAfter = N / 4;   // LEAVER reads at least this many, then closes

    // --- OS-port handshake (cv, not std::future) ---
    std::mutex mtx;
    std::condition_variable cv;
    std::uint16_t port_val = 0;
    bool port_ready = false;
    // --- late-join rendezvous: server pauses at frame J until the late client has connected ---
    bool server_paused = false;   // server has reached frame J and is waiting
    bool late_connected = false;  // late client's connect() has returned

    netbcast::Stats stats;
    std::atomic<int> done{0};  // client threads finished (full, leaver, late)

    // --- server: single-thread select-broadcast with the rendezvous hook ---
    std::thread server([&] {
        std::uint16_t port = 0;
        netsock::socket_t listener =
            netsock::listen_loopback(0, port, /*backlog=*/8);
        if (!netsock::is_valid(listener)) {
            std::lock_guard<std::mutex> lk(mtx);
            port_ready = true;
            cv.notify_all();
            return;
        }
        netsock::set_nonblocking(listener);
        {
            std::lock_guard<std::mutex> lk(mtx);
            port_val = port;
            port_ready = true;
        }
        cv.notify_all();

        auto on_frame = [&](std::size_t fi) {
            if (fi != kJoinFrame) return;
            std::unique_lock<std::mutex> lk(mtx);
            server_paused = true;                 // tell the late client it's time to join
            cv.notify_all();
            cv.wait(lk, [&] { return late_connected; });  // don't send frame J until it's in
        };
        // min_initial=2: FULL + LEAVER must both be present before frame 0. The LATE client is
        // rendezvoused in at frame J by the hook. 10 s bounded initial-accept => fail-not-wedge.
        stats = netbcast::broadcast_select(listener, ref_payloads, /*min_initial=*/2,
                                           /*accept_deadline_ms=*/10000, on_frame);
        netsock::close_socket(listener);
    });

    auto wait_port = [&]() -> std::uint16_t {
        std::unique_lock<std::mutex> lk(mtx);
        cv.wait(lk, [&] { return port_ready; });
        return port_val;
    };

    // --- FULL client: all frames -> reconstruct the sealed digest ---
    std::vector<std::vector<std::uint8_t>> full_payloads;
    std::size_t full_pending = 0;
    std::string full_digest;
    std::thread full([&] {
        std::uint16_t port = wait_port();
        if (port == 0) { ++done; return; }
        netsock::socket_t s = netsock::connect_loopback(port);
        if (netsock::is_valid(s)) {
            collect_to_eof(s, full_payloads, full_pending);
            netsock::close_socket(s);
            if (full_pending == 0 && !full_payloads.empty())
                full_digest = session::run_client(rails, sess_vec::SCENARIO,
                                                  delivered_from_payloads(full_payloads),
                                                  /*reconcile=*/true).digest;
        }
        ++done;
    });

    // --- LEAVER client: connect at frame 0, read a prefix, close mid-stream ---
    std::vector<std::vector<std::uint8_t>> leaver_payloads;
    std::thread leaver([&] {
        std::uint16_t port = wait_port();
        if (port == 0) { ++done; return; }
        netsock::socket_t s = netsock::connect_loopback(port);
        if (netsock::is_valid(s)) collect_n_then_close(s, kLeaveAfter, leaver_payloads);
        ++done;
    });

    // --- LATE client: wait for the server's pause, join mid-stream, read its suffix to EOF ---
    std::vector<std::vector<std::uint8_t>> late_payloads;
    std::size_t late_pending = 0;
    std::thread late([&] {
        std::uint16_t port = wait_port();
        if (port == 0) { ++done; return; }
        {
            std::unique_lock<std::mutex> lk(mtx);
            cv.wait(lk, [&] { return server_paused; });  // server is holding frame J for us
        }
        netsock::socket_t s = netsock::connect_loopback(port);
        {
            std::lock_guard<std::mutex> lk(mtx);
            late_connected = true;  // release the server: it accepts us, then sends frame J on
        }
        cv.notify_all();
        if (netsock::is_valid(s)) {
            collect_to_eof(s, late_payloads, late_pending);
            netsock::close_socket(s);
        }
        ++done;
    });

    // --- watchdog: fail, never wedge, on any socket hang ---
    std::thread watchdog([&] {
        for (int i = 0; i < 400 && done.load() < 3; ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));  // up to ~40 s
        if (done.load() < 3) {
            std::printf("FAIL layer-9 dynamic bridge TIMED OUT (socket hang)\n");
            std::_Exit(1);
        }
    });
    watchdog.detach();

    server.join();
    full.join();
    leaver.join();
    late.join();

    // ---- verify ----
    int fails = 0;
    auto is_prefix_of_ref = [&](const std::vector<std::vector<std::uint8_t>>& got) {
        for (std::size_t i = 0; i < got.size(); ++i)
            if (i >= N || got[i] != ref_payloads[i]) return false;
        return true;
    };

    // FULL: every frame, digest matches the sealed in-process session.
    if (full_payloads.size() != N || !is_prefix_of_ref(full_payloads)) {
        ++fails;
        std::printf("FAIL full client did not receive all %zu frames (got %zu)\n", N,
                    full_payloads.size());
    }
    if (full_digest != expected) {
        ++fails;
        std::printf("FAIL full-client digest != in-process session digest:\n  got %s\n  exp %s\n",
                    full_digest.c_str(), expected.c_str());
    }

    // LATE: a clean contiguous SUFFIX frames[K:], joined strictly mid-stream (0<K<N).
    std::size_t K = N;  // late's join index = position of its first delivered frame
    if (!late_payloads.empty()) {
        for (std::size_t i = 0; i < N; ++i)
            if (ref_payloads[i] == late_payloads[0]) { K = i; break; }
    }
    bool late_ok = !late_payloads.empty() && late_pending == 0 && K > 0 && K < N &&
                   late_payloads.size() == N - K;
    if (late_ok)
        for (std::size_t i = 0; i < late_payloads.size(); ++i)
            if (late_payloads[i] != ref_payloads[K + i]) { late_ok = false; break; }
    if (!late_ok) {
        ++fails;
        std::printf("FAIL late client did not receive a clean mid-stream suffix "
                    "(K=%zu, got %zu frames, pending %zu)\n",
                    K, late_payloads.size(), late_pending);
    }

    // LEAVER: a contiguous PREFIX frames[0:m], 0<m<N (it read some, then left).
    bool leaver_ok = !leaver_payloads.empty() && leaver_payloads.size() < N &&
                     leaver_payloads.size() >= kLeaveAfter && is_prefix_of_ref(leaver_payloads);
    if (!leaver_ok) {
        ++fails;
        std::printf("FAIL leaver did not receive a clean prefix (got %zu of %zu, want >=%zu)\n",
                    leaver_payloads.size(), N, kLeaveAfter);
    }

    // SERVER: sent every frame; saw exactly 3 joins (full+leaver+late) and >=1 leave (the leaver).
    if (!stats.ok || stats.frames_sent != N) {
        ++fails;
        std::printf("FAIL server did not broadcast all frames (ok=%d sent=%zu/%zu)\n",
                    stats.ok ? 1 : 0, stats.frames_sent, N);
    }
    if (stats.joins != 3) {
        ++fails;
        std::printf("FAIL server join count != 3 (got %zu)\n", stats.joins);
    }
    if (stats.leaves < 1) {
        ++fails;
        std::printf("FAIL server never observed the leaver's departure (leaves=%zu)\n", stats.leaves);
    }

    if (fails == 0) {
        std::printf("PASS: layer-9 dynamic broadcast — single-thread select() fan-out over real "
                    "127.0.0.1 sockets: FULL client got all %zu frames and reconstructed "
                    "SESSION-SK-001 (%s); LATE client joined at frame %zu and got exactly the "
                    "frames[%zu:] suffix; LEAVER got a clean %zu-frame prefix then left cleanly "
                    "(joins=%zu leaves=%zu)\n",
                    N, expected.c_str(), K, K, leaver_payloads.size(), stats.joins, stats.leaves);
        return 0;
    }
    std::printf("RESULT: layer-9 dynamic bridge FAIL (%d mismatches)\n", fails);
    return 1;
}
