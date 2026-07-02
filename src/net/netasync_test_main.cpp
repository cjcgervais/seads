// SEADS ASYNC-OUTPUT determinism BRIDGE for the single-thread select-broadcast server
// (netcode LAYER 11).
//
// Layers 9/10 stream with BLOCKING send_all: once a slow client's kernel socket buffers fill, the
// whole broadcast — every other client's frames, and the layer-10 catch-up replay burst — stalls
// behind it. Layer 11 (netbcast::broadcast_async) removes that back-pressure: every client is
// non-blocking and owns a userspace send buffer; frames (and a joiner's catch-up prefix) are
// ENQUEUED, the kernel takes what it can (send_some), and the remainder flushes when the same
// select() that services JOIN/LEAVE reports the client writable (select_rw). This bridge proves,
// over real 127.0.0.1 sockets and with NO sleeps / NO timing guesses, two claims:
//
//   LEG 1 (no back-pressure, volume): one SLOW client that reads NOTHING while the server streams
//   a synthetic ~8 MiB payload list through a deliberately TINY kernel send buffer (SO_SNDBUF
//   pinned on the listener — inherited by the accepted socket, disabling send-side autotuning; and
//   a never-reading receiver's window holds near its initial size, since receive autotuning tracks
//   the application's read rate — so combined kernel capacity is a few hundred KiB, ~40x under the
//   stream). The receiver's SO_RCVBUF is deliberately NOT touched: shrinking it after the window
//   scale is negotiated is a known TCP pathology on Linux (drops + retransmission backoff — it
//   broke this leg's drain on the CI GCC/Clang x64 legs). A blocking layer-9/10 server provably
//   WEDGES at that capacity (the watchdog would fail this test). The async server's frame loop
//   must instead complete all N frames — the on_frame hook observes it reach the LAST frame while
//   SLOW has consumed zero bytes — and after SLOW then drains to EOF its received stream must be
//   byte-identical to the encoded frame list.
//
//   LEG 2 (sealed-session fidelity, async + catch-up): the layer-10 shape run through the async
//   path. EARLY (connected before frame 0, reads live) and CATCHUP (rendezvoused to join at frame
//   J via on_frame; its missed prefix frames[0:J] is ENQUEUED — not blocking-burst — by the async
//   accept) both receive the WHOLE stream byte-identically and reconstruct the SAME sealed
//   SESSION-SK-001 digest as the in-process run_session reference. Buffered output adds zero
//   information and zero nondeterminism.
//
// Every assertion is on delivered BYTES; all rendezvous are cv+notify_all (no std::future — MinGW
// call_once caveat; no sleeps — on_frame pins exact frames). A finite watchdog fails (not wedges)
// on any socket hang. Exit 0 PASS, 1 FAIL.
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
// LEG 1: no back-pressure. Synthetic volume stream >> the pinned kernel buffers; SLOW reads
// nothing until the server's frame loop reaches the last frame. Returns the number of failures.
// ---------------------------------------------------------------------------------------------
static int run_volume_leg() {
    // 512 frames x 16 KiB ~= 8 MiB — orders of magnitude beyond the kernel buffering (SO_SNDBUF
    // pinned to 16 KiB server-side; the never-read receive window holds near its initial ~64-128
    // KiB), so a BLOCKING server provably wedges on the never-reading client where the async one
    // must not.
    const std::size_t kFrames = 512, kFrameBytes = 16 * 1024;
    const int kBufBytes = 16 * 1024;
    std::vector<std::vector<std::uint8_t>> payloads(kFrames);
    for (std::size_t i = 0; i < kFrames; ++i) {
        payloads[i].resize(kFrameBytes);
        for (std::size_t b = 0; b < kFrameBytes; ++b)  // deterministic, frame-distinct fill
            payloads[i][b] = static_cast<std::uint8_t>((i * 131 + b * 7) & 0xFF);
    }

    std::mutex mtx;
    std::condition_variable cv;
    std::uint16_t port_val = 0;
    bool port_ready = false;
    bool last_frame_reached = false;  // server's frame loop got to frame N-1 (SLOW still unread)
    std::atomic<long long> slow_bytes_read{0};

    netbcast::Stats stats;
    std::atomic<int> done{0};

    std::thread server([&] {
        std::uint16_t port = 0;
        netsock::socket_t listener = netsock::listen_loopback(0, port, /*backlog=*/2);
        if (netsock::is_valid(listener)) {
            // Pin a tiny kernel send buffer BEFORE accept: accepted sockets inherit it, and an
            // explicit SO_SNDBUF disables send-side autotuning — the wedge trap is armed.
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

        auto on_frame = [&](std::size_t fi) {
            if (fi != kFrames - 1) return;
            // The frame loop reached the LAST frame. A blocking server could not get here with a
            // never-reading client behind these buffers — it wedges around the capacity mark.
            std::lock_guard<std::mutex> lk(mtx);
            last_frame_reached = true;
            cv.notify_all();
        };
        stats = netbcast::broadcast_async(listener, payloads, /*min_initial=*/1,
                                          /*accept_deadline_ms=*/10000, on_frame,
                                          /*catchup=*/false);
        netsock::close_socket(listener);
    });

    std::vector<std::vector<std::uint8_t>> slow_payloads;
    std::size_t slow_pending = 0;
    long long read_before_release = -1;
    std::thread slow([&] {
        std::uint16_t port;
        {
            std::unique_lock<std::mutex> lk(mtx);
            cv.wait(lk, [&] { return port_ready; });
            port = port_val;
        }
        if (port == 0) { ++done; return; }
        netsock::socket_t s = netsock::connect_loopback(port);
        if (netsock::is_valid(s)) {
            // The receive buffer is deliberately left alone (see the file header): a never-reading
            // receiver's window holds near its initial size anyway, and shrinking SO_RCVBUF after
            // the window scale is negotiated wedges the later drain on Linux (drops + RTO backoff).
            {
                // Read NOTHING until the server's frame loop has reached the last frame.
                std::unique_lock<std::mutex> lk(mtx);
                cv.wait(lk, [&] { return last_frame_reached; });
            }
            read_before_release = slow_bytes_read.load();  // provably 0: we never recv()'d
            framing::StreamReassembler r;
            std::uint8_t buf[4096];
            bool ok = true;
            while (true) {
                std::ptrdiff_t n = netsock::recv_some(s, buf, sizeof(buf));
                if (n > 0) {
                    slow_bytes_read += n;
                    if (!r.feed(buf, static_cast<std::size_t>(n), slow_payloads)) { ok = false; break; }
                } else {
                    break;
                }
            }
            slow_pending = ok ? r.pending() : static_cast<std::size_t>(-1);
            netsock::close_socket(s);
        }
        ++done;
    });

    std::thread watchdog([&] {
        for (int i = 0; i < 400 && done.load() < 1; ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));  // up to ~40 s
        if (done.load() < 1) {
            std::printf("FAIL layer-11 volume leg TIMED OUT — the broadcast back-pressured on the "
                        "slow client (async output must never wedge)\n");
            std::fflush(stdout);  // _Exit skips stdio flush; don't lose the diagnostic
            std::_Exit(1);
        }
    });
    watchdog.detach();

    server.join();
    slow.join();

    int fails = 0;
    if (read_before_release != 0) {
        ++fails;
        std::printf("FAIL volume leg: SLOW had read %lld bytes before the server reached the last "
                    "frame (rendezvous broken — expected 0)\n", read_before_release);
    }
    if (!stats.ok || stats.frames_sent != kFrames) {
        ++fails;
        std::printf("FAIL volume leg: server did not enqueue all frames (ok=%d sent=%zu/%zu)\n",
                    stats.ok ? 1 : 0, stats.frames_sent, kFrames);
    }
    if (stats.leaves != 0) {
        ++fails;
        std::printf("FAIL volume leg: server dropped %zu client(s) (drain must deliver, not drop)\n",
                    stats.leaves);
    }
    bool bytes_ok = (slow_pending == 0 && slow_payloads.size() == kFrames);
    for (std::size_t i = 0; bytes_ok && i < kFrames; ++i)
        if (slow_payloads[i] != payloads[i]) bytes_ok = false;
    if (!bytes_ok) {
        ++fails;
        std::printf("FAIL volume leg: SLOW's drained stream != the enqueued frames (got %zu of %zu, "
                    "pending %zu)\n", slow_payloads.size(), kFrames, slow_pending);
    }
    if (fails == 0)
        std::printf("  leg 1 PASS: async server enqueued all %zu frames (~%zu KiB) through %d-KiB "
                    "kernel buffers while SLOW had read 0 bytes; drained stream byte-identical\n",
                    kFrames, kFrames * kFrameBytes / 1024, kBufBytes / 1024);
    return fails;
}

// ---------------------------------------------------------------------------------------------
// LEG 2: sealed-session fidelity through the async path, catch-up included (the layer-10 shape).
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
                                          /*accept_deadline_ms=*/10000, on_frame, /*catchup=*/true);
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
            std::printf("FAIL layer-11 session leg TIMED OUT (socket hang)\n");
            std::fflush(stdout);  // _Exit skips stdio flush; don't lose the diagnostic
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
        std::printf("FAIL session leg: EARLY stream/digest mismatch (got %zu of %zu frames; digest "
                    "%s, expected %s)\n", early_payloads.size(), N, early_digest.c_str(),
                    expected.c_str());
    }
    if (catchup_pending != 0 || !equals_ref(catchup_payloads) || catchup_digest != expected) {
        ++fails;
        std::printf("FAIL session leg: CATCHUP stream/digest mismatch (got %zu of %zu frames, "
                    "pending %zu; digest %s, expected %s)\n", catchup_payloads.size(), N,
                    catchup_pending, catchup_digest.c_str(), expected.c_str());
    }
    if (catchup_payloads != early_payloads) {
        ++fails;
        std::printf("FAIL session leg: catch-up stream != early stream (buffered fan-out + replay "
                    "must be byte-identical)\n");
    }
    if (!stats.ok || stats.frames_sent != N || stats.joins != 2) {
        ++fails;
        std::printf("FAIL session leg: server stats wrong (ok=%d sent=%zu/%zu joins=%zu)\n",
                    stats.ok ? 1 : 0, stats.frames_sent, N, stats.joins);
    }
    if (fails == 0)
        std::printf("  leg 2 PASS: async + catch-up — EARLY and CATCHUP (joined at frame %zu, "
                    "prefix ENQUEUED) both received the whole %zu frames byte-identically and "
                    "reconstructed SESSION-SK-001 (%s)\n", kJoinFrame, N, expected.c_str());
    return fails;
}

int main() {
    netsock::WsaGuard wsa;
    int fails = 0;
    fails += run_volume_leg();
    fails += run_session_leg();
    if (fails == 0) {
        std::printf("PASS: layer-11 async single-thread output — no client back-pressures the "
                    "broadcast, and the buffered path reconstructs the sealed session digest\n");
        return 0;
    }
    std::printf("RESULT: layer-11 async-output bridge FAIL (%d mismatches)\n", fails);
    return 1;
}
