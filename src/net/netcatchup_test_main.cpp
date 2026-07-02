// SEADS late-join CATCH-UP determinism BRIDGE for the single-thread select-broadcast server
// (netcode LAYER 10).
//
// Layer 9 proved a late joiner receives exactly the contiguous frame SUFFIX frames[K:] from its
// join point — but it deliberately left an honest-scope gap: that joiner CANNOT reconstruct the
// whole fight, because it missed the ticks before K. Layer 10 closes the gap with catch-up: the
// broadcast server, on accepting a mid-stream joiner at frame K, first REPLAYS the missed prefix
// frames[0:K] (each length-prefixed, atomically) and only then feeds it the live suffix frames[K:].
// So the catch-up joiner receives the WHOLE stream frames[0:] — byte-identical to a client present
// from frame 0 — and reconstructs the SAME sealed SESSION-SK-001 digest (24f71845...c332).
//
// This bridge runs one real 127.0.0.1 broadcast with catchup=true and two clients:
//   * EARLY   — connected before frame 0, stays to EOF: receives ALL frames => reconstructs the
//               sealed session digest (the layer-8/9 result, sanity that catch-up mode doesn't
//               perturb an already-present client);
//   * CATCHUP — joins mid-stream at frame J (rendezvoused to an exact frame via the broadcast_select
//               on_frame hook): receives the prefix replay frames[0:J] followed by the live suffix
//               frames[J:] == the whole frames[0:], byte-for-byte equal to EARLY's stream, and
//               reconstructs the SAME digest — the late joiner now sees the whole fight.
//
// The join point is pinned with NO sleeps / NO timing guesses: the server pauses at frame J (the
// on_frame hook) until the catch-up client's connect() has returned (so it is in the accept queue),
// then accepts + replays + resumes. Every assertion is on the delivered BYTES, not wall-clock. A
// finite watchdog fails (not wedges) on any socket hang. Exit 0 PASS, 1 FAIL.
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
    const std::size_t kJoinFrame = N / 2;  // CATCHUP joins mid-stream here

    // --- OS-port handshake (cv, not std::future) ---
    std::mutex mtx;
    std::condition_variable cv;
    std::uint16_t port_val = 0;
    bool port_ready = false;
    // --- catch-up rendezvous: server pauses at frame J until the catch-up client has connected ---
    bool server_paused = false;    // server has reached frame J and is waiting
    bool late_connected = false;   // catch-up client's connect() has returned

    netbcast::Stats stats;
    std::atomic<int> done{0};  // client threads finished (early, catchup)

    // --- server: single-thread select-broadcast with catch-up + the rendezvous hook ---
    std::thread server([&] {
        std::uint16_t port = 0;
        netsock::socket_t listener = netsock::listen_loopback(0, port, /*backlog=*/8);
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
            server_paused = true;                 // tell the catch-up client it's time to join
            cv.notify_all();
            cv.wait(lk, [&] { return late_connected; });  // don't send frame J until it's in
        };
        // min_initial=1: EARLY must be present before frame 0. CATCHUP is rendezvoused in at frame J
        // by the hook. catchup=true => it is replayed frames[0:J]. 10 s bounded initial-accept.
        stats = netbcast::broadcast_select(listener, ref_payloads, /*min_initial=*/1,
                                           /*accept_deadline_ms=*/10000, on_frame, /*catchup=*/true);
        netsock::close_socket(listener);
    });

    auto wait_port = [&]() -> std::uint16_t {
        std::unique_lock<std::mutex> lk(mtx);
        cv.wait(lk, [&] { return port_ready; });
        return port_val;
    };

    // --- EARLY client: all frames -> reconstruct the sealed digest ---
    std::vector<std::vector<std::uint8_t>> early_payloads;
    std::size_t early_pending = 0;
    std::string early_digest;
    std::thread early([&] {
        std::uint16_t port = wait_port();
        if (port == 0) { ++done; return; }
        netsock::socket_t s = netsock::connect_loopback(port);
        if (netsock::is_valid(s)) {
            collect_to_eof(s, early_payloads, early_pending);
            netsock::close_socket(s);
            if (early_pending == 0 && !early_payloads.empty())
                early_digest = session::run_client(rails, sess_vec::SCENARIO,
                                                   delivered_from_payloads(early_payloads),
                                                   /*reconcile=*/true).digest;
        }
        ++done;
    });

    // --- CATCHUP client: wait for the server's pause, join mid-stream, read prefix+suffix to EOF ---
    std::vector<std::vector<std::uint8_t>> catchup_payloads;
    std::size_t catchup_pending = 0;
    std::string catchup_digest;
    std::thread catchup([&] {
        std::uint16_t port = wait_port();
        if (port == 0) { ++done; return; }
        {
            std::unique_lock<std::mutex> lk(mtx);
            cv.wait(lk, [&] { return server_paused; });  // server is holding frame J for us
        }
        netsock::socket_t s = netsock::connect_loopback(port);
        {
            std::lock_guard<std::mutex> lk(mtx);
            late_connected = true;  // release the server: accept us, replay frames[0:J], resume
        }
        cv.notify_all();
        if (netsock::is_valid(s)) {
            collect_to_eof(s, catchup_payloads, catchup_pending);
            netsock::close_socket(s);
            if (catchup_pending == 0 && !catchup_payloads.empty())
                catchup_digest = session::run_client(rails, sess_vec::SCENARIO,
                                                     delivered_from_payloads(catchup_payloads),
                                                     /*reconcile=*/true).digest;
        }
        ++done;
    });

    // --- watchdog: fail, never wedge, on any socket hang ---
    std::thread watchdog([&] {
        for (int i = 0; i < 400 && done.load() < 2; ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));  // up to ~40 s
        if (done.load() < 2) {
            std::printf("FAIL layer-10 catch-up bridge TIMED OUT (socket hang)\n");
            std::_Exit(1);
        }
    });
    watchdog.detach();

    server.join();
    early.join();
    catchup.join();

    // ---- verify ----
    int fails = 0;
    auto equals_ref = [&](const std::vector<std::vector<std::uint8_t>>& got) {
        if (got.size() != N) return false;
        for (std::size_t i = 0; i < N; ++i)
            if (got[i] != ref_payloads[i]) return false;
        return true;
    };

    // EARLY: every frame; digest matches the sealed in-process session (catch-up mode didn't perturb).
    if (!equals_ref(early_payloads)) {
        ++fails;
        std::printf("FAIL early client did not receive all %zu frames (got %zu)\n", N,
                    early_payloads.size());
    }
    if (early_digest != expected) {
        ++fails;
        std::printf("FAIL early-client digest != in-process session digest:\n  got %s\n  exp %s\n",
                    early_digest.c_str(), expected.c_str());
    }

    // CATCHUP: the WHOLE stream frames[0:] (prefix replay + live suffix), byte-identical to EARLY,
    // and the SAME reconstructed digest — the late joiner sees the whole fight.
    if (catchup_pending != 0 || !equals_ref(catchup_payloads)) {
        ++fails;
        std::printf("FAIL catch-up client did not receive the whole frames[0:] "
                    "(got %zu of %zu, pending %zu)\n",
                    catchup_payloads.size(), N, catchup_pending);
    }
    if (catchup_digest != expected) {
        ++fails;
        std::printf("FAIL catch-up-client digest != in-process session digest:\n  got %s\n  exp %s\n",
                    catchup_digest.c_str(), expected.c_str());
    }
    if (catchup_payloads != early_payloads) {
        ++fails;
        std::printf("FAIL catch-up stream != early stream (fan-out + replay must be byte-identical)\n");
    }

    // SERVER: sent every frame; saw exactly 2 joins (early + catch-up).
    if (!stats.ok || stats.frames_sent != N) {
        ++fails;
        std::printf("FAIL server did not broadcast all frames (ok=%d sent=%zu/%zu)\n",
                    stats.ok ? 1 : 0, stats.frames_sent, N);
    }
    if (stats.joins != 2) {
        ++fails;
        std::printf("FAIL server join count != 2 (got %zu)\n", stats.joins);
    }

    if (fails == 0) {
        std::printf("PASS: layer-10 late-join catch-up — single-thread select() fan-out with prefix "
                    "replay over real 127.0.0.1 sockets: EARLY got all %zu frames; CATCHUP joined at "
                    "frame %zu, was replayed frames[0:%zu] then streamed the live suffix, received the "
                    "whole frames[0:] byte-identically, and BOTH reconstructed SESSION-SK-001 (%s) "
                    "(joins=%zu)\n",
                    N, kJoinFrame, kJoinFrame, expected.c_str(), stats.joins);
        return 0;
    }
    std::printf("RESULT: layer-10 catch-up bridge FAIL (%d mismatches)\n", fails);
    return 1;
}
