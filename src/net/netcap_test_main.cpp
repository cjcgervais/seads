// SEADS BYTE-CAP / DROP-SLOWEST determinism BRIDGE for the async single-thread broadcast server
// (netcode LAYER 12).
//
// Layer 11 (broadcast_async) removed all back-pressure: a slow client just accumulates a userspace
// send buffer — unbounded by design there, safe only because the stream is precomputed and finite.
// Pointed at an open-ended live stream, a permanently-slow client grows server memory without
// bound. Layer 12 adds the hygiene policy: an opt-in per-client byte-cap (`cap_bytes`) — a client
// whose pending backlog an enqueue leaves above the cap is DROPPED (drop-slowest), counted in
// Stats.capped, while every surviving client's delivered bytes are untouched. This bridge proves,
// over real 127.0.0.1 sockets and with NO sleeps / NO timing guesses, two claims:
//
//   LEG 1 (drop-slowest under volume): a ~7.5 MiB synthetic stream through a pinned tiny kernel
//   send buffer, to TWO clients: FAST reads continuously (rate-coupled to the frame loop via the
//   on_frame hook so its backlog is provably bounded WELL under the cap — never at risk), SLOW
//   reads NOTHING. With cap_bytes = 1 MiB the server must shed exactly SLOW (capped == 1), finish
//   every frame un-wedged, and deliver FAST the whole stream byte-identically. SLOW's delivered
//   bytes must be a strict byte-PREFIX of the encoded stream (the kernel-accepted prefix — the cap
//   discards the pending tail whole, it never reorders or corrupts). WHICH frame index SLOW is shed
//   at is OS-timing (how many bytes its kernel buffers absorbed) — deliberately unasserted; the
//   deterministic claims are who survives, what the survivor gets, and the prefix shape of what the
//   shed client got. A capless (layer-11) server would instead buffer ~7 MiB for SLOW and deliver
//   it all — seads_netasync_test still gates exactly that; the two bridges together pin both sides
//   of the policy boundary.
//
//   LEG 2 (cap does not perturb the healthy path): the layer-11 sealed-session shape — EARLY (from
//   frame 0) + CATCHUP (joins at frame J, prefix ENQUEUED) — run with a GENEROUS cap. Both receive
//   the whole stream byte-identically, both reconstruct the sealed SESSION-SK-001 digest, and
//   capped == 0: enabling the policy on clients that keep up changes nothing (the cap decides only
//   WHO is dropped, never WHICH bytes flow).
//
// Every assertion is on delivered BYTES; all rendezvous are cv+notify_all (no std::future — MinGW
// call_once caveat; no sleeps — pacing is coupled to on_frame). A finite watchdog fails (not
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

// ---------------------------------------------------------------------------------------------
// LEG 1: drop-slowest. FAST (paced reader) + SLOW (reads nothing); cap sheds exactly SLOW while
// FAST's stream is byte-identical and SLOW's is a strict byte-prefix. Returns failure count.
// ---------------------------------------------------------------------------------------------
static int run_dropslowest_leg() {
    // 512 frames x 15 KiB ~= 7.5 MiB. 15 KiB (not 16) keeps one ENCODED frame (2-byte LEB128
    // prefix + 15360 = 15362 bytes) strictly under the pinned 16-KiB kernel send buffer — the
    // slack makes the FAST-pacing rendezvous below provably deadlock-free (see kPaceWindow).
    const std::size_t kFrames = 512, kFrameBytes = 15 * 1024;
    const std::size_t kEncFrame = 2 + kFrameBytes;  // LEB128(15360) is 2 bytes
    const int kBufBytes = 16 * 1024;
    const std::size_t kCap = 1u << 20;  // 1 MiB: >> FAST's bounded backlog, << the stream size
    // FAST is released to read frame fi's bytes only once the loop is at frame fi+kPaceWindow, so
    // FAST's userspace backlog is bounded by ~(kPaceWindow+1)*kEncFrame ~= 507 KiB < kCap/2 —
    // FAST can never be the one capped, deterministically. Deadlock-free: when the loop parks in
    // on_frame(fi), the bytes already FLUSHED to the kernel exceed the pacing requirement (the
    // last kernel-full flush left >= kBufBytes buffered kernel-side, and kBufBytes > kEncFrame).
    const std::size_t kPaceWindow = 32;
    std::vector<std::vector<std::uint8_t>> payloads(kFrames);
    for (std::size_t i = 0; i < kFrames; ++i) {
        payloads[i].resize(kFrameBytes);
        for (std::size_t b = 0; b < kFrameBytes; ++b)  // deterministic, frame-distinct fill
            payloads[i][b] = static_cast<std::uint8_t>((i * 131 + b * 7) & 0xFF);
    }
    std::vector<std::uint8_t> expected_stream, tmp;
    for (const auto& p : payloads) {
        tmp.clear();
        framing::encode_frame(p, tmp);
        expected_stream.insert(expected_stream.end(), tmp.begin(), tmp.end());
    }

    std::mutex mtx;
    std::condition_variable cv;
    std::uint16_t port_val = 0;
    bool port_ready = false;
    bool server_done = false;
    long long fast_bytes = 0;  // raw bytes FAST has read so far (guards the pacing rendezvous)

    netbcast::Stats stats;
    std::atomic<int> done{0};

    std::thread server([&] {
        std::uint16_t port = 0;
        netsock::socket_t listener = netsock::listen_loopback(0, port, /*backlog=*/2);
        if (netsock::is_valid(listener)) {
            // Pin a tiny kernel send buffer BEFORE accept (inherited; disables send autotuning):
            // SLOW's kernel-absorbable bytes stay tiny next to the stream, so its userspace
            // backlog provably crosses the cap. (No SO_RCVBUF twin — Linux pathology; see socket.h.)
            netsock::set_sndbuf(listener, kBufBytes);
            netsock::set_nonblocking(listener);
        }
        {
            std::lock_guard<std::mutex> lk(mtx);
            port_val = netsock::is_valid(listener) ? port : 0;
            port_ready = true;
        }
        cv.notify_all();
        if (!netsock::is_valid(listener)) return;

        // Pace the loop to FAST's reads: don't enqueue frame fi until FAST has consumed everything
        // up to frame fi-kPaceWindow. Keeps FAST's backlog far under the cap with no sleeps.
        auto on_frame = [&](std::size_t fi) {
            if (fi <= kPaceWindow) return;
            const long long need =
                static_cast<long long>(fi - kPaceWindow) * static_cast<long long>(kEncFrame);
            std::unique_lock<std::mutex> lk(mtx);
            cv.wait(lk, [&] { return fast_bytes >= need; });
        };
        stats = netbcast::broadcast_async(listener, payloads, /*min_initial=*/2,
                                          /*accept_deadline_ms=*/10000, on_frame,
                                          /*catchup=*/false, kCap);
        netsock::close_socket(listener);
        {
            std::lock_guard<std::mutex> lk(mtx);
            server_done = true;
        }
        cv.notify_all();
    });

    auto wait_port = [&]() -> std::uint16_t {
        std::unique_lock<std::mutex> lk(mtx);
        cv.wait(lk, [&] { return port_ready; });
        return port_val;
    };

    // FAST: reads continuously, publishing its byte count for the pacing rendezvous.
    std::vector<std::vector<std::uint8_t>> fast_payloads;
    std::size_t fast_pending = 0;
    bool fast_ok = false;
    std::thread fast([&] {
        std::uint16_t port = wait_port();
        if (port == 0) { ++done; return; }
        netsock::socket_t s = netsock::connect_loopback(port);
        if (netsock::is_valid(s)) {
            framing::StreamReassembler r;
            std::uint8_t buf[4096];
            bool ok = true;
            while (true) {
                std::ptrdiff_t n = netsock::recv_some(s, buf, sizeof(buf));
                if (n > 0) {
                    {
                        std::lock_guard<std::mutex> lk(mtx);
                        fast_bytes += n;
                    }
                    cv.notify_all();
                    if (!r.feed(buf, static_cast<std::size_t>(n), fast_payloads)) { ok = false; break; }
                } else {
                    break;
                }
            }
            fast_pending = r.pending();
            fast_ok = ok;
            netsock::close_socket(s);
        }
        ++done;
    });

    // SLOW: connects, reads NOTHING until the whole broadcast has returned (it will have been
    // capped long before then), then drains its kernel-buffered prefix to EOF.
    std::vector<std::uint8_t> slow_bytes;
    std::thread slow([&] {
        std::uint16_t port = wait_port();
        if (port == 0) { ++done; return; }
        netsock::socket_t s = netsock::connect_loopback(port);
        if (netsock::is_valid(s)) {
            {
                std::unique_lock<std::mutex> lk(mtx);
                cv.wait(lk, [&] { return server_done; });
            }
            std::uint8_t buf[4096];
            while (true) {
                std::ptrdiff_t n = netsock::recv_some(s, buf, sizeof(buf));
                if (n <= 0) break;
                slow_bytes.insert(slow_bytes.end(), buf, buf + n);
            }
            netsock::close_socket(s);
        }
        ++done;
    });

    std::thread watchdog([&] {
        for (int i = 0; i < 600 && done.load() < 2; ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));  // up to ~60 s
        if (done.load() < 2) {
            std::printf("FAIL layer-12 drop-slowest leg TIMED OUT (wedge: the cap policy or the "
                        "pacing rendezvous broke the frame loop)\n");
            std::_Exit(1);
        }
    });
    watchdog.detach();

    server.join();
    fast.join();
    slow.join();

    int fails = 0;
    if (!stats.ok || stats.frames_sent != kFrames) {
        ++fails;
        std::printf("FAIL drop-slowest leg: server did not finish the stream (ok=%d sent=%zu/%zu)\n",
                    stats.ok ? 1 : 0, stats.frames_sent, kFrames);
    }
    if (stats.joins != 2 || stats.capped != 1 || stats.leaves != 1) {
        ++fails;
        std::printf("FAIL drop-slowest leg: expected exactly SLOW shed by the cap "
                    "(joins=%zu leaves=%zu capped=%zu; want 2/1/1)\n",
                    stats.joins, stats.leaves, stats.capped);
    }
    bool fast_identical = fast_ok && fast_pending == 0 && fast_payloads.size() == kFrames;
    for (std::size_t i = 0; fast_identical && i < kFrames; ++i)
        if (fast_payloads[i] != payloads[i]) fast_identical = false;
    if (!fast_identical) {
        ++fails;
        std::printf("FAIL drop-slowest leg: FAST's stream not byte-identical (got %zu of %zu "
                    "frames, pending %zu)\n", fast_payloads.size(), kFrames, fast_pending);
    }
    const bool slow_strict = !slow_bytes.empty() && slow_bytes.size() < expected_stream.size();
    bool slow_prefix = slow_strict;
    for (std::size_t i = 0; slow_prefix && i < slow_bytes.size(); ++i)
        if (slow_bytes[i] != expected_stream[i]) slow_prefix = false;
    if (!slow_prefix) {
        ++fails;
        std::printf("FAIL drop-slowest leg: SLOW's %zu delivered bytes are not a strict byte-prefix "
                    "of the %zu-byte encoded stream\n", slow_bytes.size(), expected_stream.size());
    }
    if (fails == 0)
        std::printf("  leg 1 PASS: cap=%zu shed exactly SLOW (joins=2 capped=1) at ~%zu of %zu "
                    "frames' bytes; FAST byte-identical, SLOW a strict byte-prefix, stream "
                    "completed un-wedged\n",
                    kCap, slow_bytes.size() / kEncFrame, kFrames);
    return fails;
}

// ---------------------------------------------------------------------------------------------
// LEG 2: a generous cap does not perturb the healthy path — the layer-11 sealed-session shape
// (EARLY + CATCHUP) under cap_bytes, both digests sealed, capped == 0.
// ---------------------------------------------------------------------------------------------
static int run_session_leg() {
    const Rails rails = sealed_rails();
    const std::string expected =
        session::run_session(rails, sess_vec::SCENARIO, /*reconcile=*/true).digest;
    const session::ServerFrames ref_frames =
        session::build_server_frames(rails, sess_vec::SCENARIO);
    std::vector<std::vector<std::uint8_t>> ref_payloads;
    for (const auto& f : ref_frames) ref_payloads.push_back(f.second);
    const std::size_t N = ref_payloads.size();
    const std::size_t kJoinFrame = N / 2;
    const std::size_t kCap = 8u << 20;  // generous: the whole session stream is a few KiB

    std::mutex mtx;
    std::condition_variable cv;
    std::uint16_t port_val = 0;
    bool port_ready = false;
    bool server_paused = false;
    bool late_connected = false;

    netbcast::Stats stats;
    std::atomic<int> done{0};

    std::thread server([&] {
        std::uint16_t port = 0;
        netsock::socket_t listener = netsock::listen_loopback(0, port, /*backlog=*/8);
        if (netsock::is_valid(listener)) netsock::set_nonblocking(listener);
        {
            std::lock_guard<std::mutex> lk(mtx);
            port_val = netsock::is_valid(listener) ? port : 0;
            port_ready = true;
        }
        cv.notify_all();
        if (!netsock::is_valid(listener)) return;

        auto on_frame = [&](std::size_t fi) {
            if (fi != kJoinFrame) return;
            std::unique_lock<std::mutex> lk(mtx);
            server_paused = true;
            cv.notify_all();
            cv.wait(lk, [&] { return late_connected; });
        };
        stats = netbcast::broadcast_async(listener, ref_payloads, /*min_initial=*/1,
                                          /*accept_deadline_ms=*/10000, on_frame, /*catchup=*/true,
                                          kCap);
        netsock::close_socket(listener);
    });

    auto wait_port = [&]() -> std::uint16_t {
        std::unique_lock<std::mutex> lk(mtx);
        cv.wait(lk, [&] { return port_ready; });
        return port_val;
    };

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

    std::vector<std::vector<std::uint8_t>> catchup_payloads;
    std::size_t catchup_pending = 0;
    std::string catchup_digest;
    std::thread catchup([&] {
        std::uint16_t port = wait_port();
        if (port == 0) { ++done; return; }
        {
            std::unique_lock<std::mutex> lk(mtx);
            cv.wait(lk, [&] { return server_paused; });
        }
        netsock::socket_t s = netsock::connect_loopback(port);
        {
            std::lock_guard<std::mutex> lk(mtx);
            late_connected = true;  // release the server: accept us, ENQUEUE frames[0:J], resume
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

    std::thread watchdog([&] {
        for (int i = 0; i < 400 && done.load() < 2; ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));  // up to ~40 s
        if (done.load() < 2) {
            std::printf("FAIL layer-12 session leg TIMED OUT (socket hang)\n");
            std::_Exit(1);
        }
    });
    watchdog.detach();

    server.join();
    early.join();
    catchup.join();

    int fails = 0;
    auto equals_ref = [&](const std::vector<std::vector<std::uint8_t>>& got) {
        if (got.size() != N) return false;
        for (std::size_t i = 0; i < N; ++i)
            if (got[i] != ref_payloads[i]) return false;
        return true;
    };
    if (!equals_ref(early_payloads) || early_digest != expected) {
        ++fails;
        std::printf("FAIL session leg: EARLY stream/digest mismatch under the cap (got %zu of %zu "
                    "frames; digest %s, expected %s)\n", early_payloads.size(), N,
                    early_digest.c_str(), expected.c_str());
    }
    if (catchup_pending != 0 || !equals_ref(catchup_payloads) || catchup_digest != expected) {
        ++fails;
        std::printf("FAIL session leg: CATCHUP stream/digest mismatch under the cap (got %zu of "
                    "%zu frames, pending %zu; digest %s, expected %s)\n", catchup_payloads.size(),
                    N, catchup_pending, catchup_digest.c_str(), expected.c_str());
    }
    if (!stats.ok || stats.frames_sent != N || stats.joins != 2 || stats.capped != 0) {
        ++fails;
        std::printf("FAIL session leg: server stats wrong (ok=%d sent=%zu/%zu joins=%zu "
                    "capped=%zu; a healthy client must never be capped)\n",
                    stats.ok ? 1 : 0, stats.frames_sent, N, stats.joins, stats.capped);
    }
    if (fails == 0)
        std::printf("  leg 2 PASS: generous cap, capped=0 — EARLY and CATCHUP (joined at frame "
                    "%zu) both received all %zu frames and reconstructed SESSION-SK-001 (%s)\n",
                    kJoinFrame, N, expected.c_str());
    return fails;
}

int main() {
    netsock::WsaGuard wsa;
    int fails = 0;
    fails += run_dropslowest_leg();
    fails += run_session_leg();
    if (fails == 0) {
        std::printf("PASS: layer-12 send-buffer byte-cap — the slowest client is shed at the cap, "
                    "every survivor's bytes are untouched, and the healthy path is unperturbed\n");
        return 0;
    }
    std::printf("RESULT: layer-12 byte-cap bridge FAIL (%d mismatches)\n", fails);
    return 1;
}
