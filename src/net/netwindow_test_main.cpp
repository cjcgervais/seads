// SEADS bounded/windowed CATCH-UP determinism BRIDGE for the live broadcast server
// (netcode LAYER 14).
//
// Layer 13's catch-up retains EVERY produced payload — O(stream) server memory, which is exactly
// what a genuinely open-ended live source cannot afford (the honest boundary its ADR named).
// Layer 14 bounds it: broadcast_live's `catchup_window` retains only the LAST W produced payloads,
// so a mid-stream joiner accepted at frame fi is replayed exactly frames[max(0,fi-W):fi] and then
// fed live — its delivered stream is PRECISELY the contiguous suffix frames[max(0,fi-W):], while
// catch-up memory stays O(W) on a stream of any length. This bridge proves, over real 127.0.0.1
// sockets with NO sleeps / NO timing guesses, one claim per window regime (all catchup=true, all
// frames pulled live from session::FrameProducer — the kernel stepped inside the loop):
//
//   LEG 0 (W = 1, the minimal window): the joiner rendezvoused at frame J receives exactly
//   frames[J-1:] — one replayed frame, then live; trimmed == N-1 (every retention but the last
//   evicted). The window's floor works.
//
//   LEG 1 (1 < W < J): the joiner receives exactly frames[J-W:] byte-for-byte — frame-aligned,
//   no gap, no duplicate — and the EARLY client (present from frame 0) is byte-identical to the
//   batch reference AND reconstructs the sealed SESSION-SK-001 digest (the window touches ONLY
//   the joiner's replay depth, never which bytes flow to anyone else); trimmed == N-W.
//
//   LEG 2 (W >= stream length): the window never evicts (trimmed == 0) and the joiner receives
//   the WHOLE frames[0:], reconstructing the SAME sealed digest — the layer-13 retain-all
//   degenerate case, recovered exactly.
//
// Every assertion is on delivered BYTES; all rendezvous are cv+notify_all (no std::future — MinGW
// call_once caveat; no sleeps — on_frame pins exact frames). A finite watchdog fails (not wedges)
// on any socket hang. Exit 0 PASS, 1 FAIL.
//
// TRANSPORT-ONLY: no kernel/det_math/rails/wire/golden touched; rides seal v1.21r0.
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

// One live-socket leg: broadcast_live (catchup=true, the given catchup_window) pulls a FRESH
// FrameProducer to an EARLY client and a mid-stream joiner rendezvoused at frame kJoin.
struct LegResult {
    std::vector<std::vector<std::uint8_t>> early, late;
    std::size_t early_pending = 1, late_pending = 1;
    netbcast::Stats stats;
    bool setup_ok = false;
};

static LegResult run_window_leg(const Rails& rails, std::size_t kJoin, std::size_t window,
                                const char* leg_name) {
    LegResult res;

    std::mutex mtx;
    std::condition_variable cv;
    std::uint16_t port_val = 0;
    bool port_ready = false;
    bool server_paused = false;   // server is holding frame kJoin for the joiner
    bool late_connected = false;  // the joiner's connect() has returned

    std::atomic<int> done{0};

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

        // THE LAYER-14 POINT: catch-up runs against a live-stepped stream while the retained
        // history is bounded to the last `window` payloads.
        session::FrameProducer producer(rails, sess_vec::SCENARIO);
        auto source = [&](std::vector<std::uint8_t>& payload) {
            std::int64_t emit_tick = 0;
            return producer.next(emit_tick, payload);
        };
        auto on_frame = [&](std::size_t fi) {
            if (fi != kJoin) return;
            std::unique_lock<std::mutex> lk(mtx);
            server_paused = true;  // tell the joiner it's time to connect
            cv.notify_all();
            cv.wait(lk, [&] { return late_connected; });  // hold frame kJoin until it's in
        };
        res.stats = netbcast::broadcast_live(listener, source, /*min_initial=*/1,
                                             /*accept_deadline_ms=*/10000, on_frame,
                                             /*catchup=*/true, /*cap_bytes=*/0, window);
        netsock::close_socket(listener);
    });

    auto wait_port = [&]() -> std::uint16_t {
        std::unique_lock<std::mutex> lk(mtx);
        cv.wait(lk, [&] { return port_ready; });
        return port_val;
    };

    std::thread early([&] {
        std::uint16_t port = wait_port();
        if (port != 0) {
            netsock::socket_t s = netsock::connect_loopback(port);
            if (netsock::is_valid(s)) {
                collect_to_eof(s, res.early, res.early_pending);
                netsock::close_socket(s);
            }
        }
        ++done;
    });

    std::thread late([&] {
        std::uint16_t port = wait_port();
        if (port == 0) { ++done; return; }
        {
            std::unique_lock<std::mutex> lk(mtx);
            cv.wait(lk, [&] { return server_paused; });  // server is holding frame kJoin
        }
        netsock::socket_t s = netsock::connect_loopback(port);
        {
            std::lock_guard<std::mutex> lk(mtx);
            late_connected = true;  // release the server: accept us (replay the retained window)
        }
        cv.notify_all();
        if (netsock::is_valid(s)) {
            collect_to_eof(s, res.late, res.late_pending);
            netsock::close_socket(s);
        }
        ++done;
    });

    // watchdog: fail, never wedge, on any socket hang
    std::thread watchdog([&] {
        for (int i = 0; i < 400 && done.load() < 2; ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (done.load() < 2) {
            std::printf("FAIL layer-14 window bridge (%s) TIMED OUT (socket hang)\n", leg_name);
            std::fflush(stdout);
            std::_Exit(1);
        }
    });
    watchdog.detach();

    server.join();
    early.join();
    late.join();
    res.setup_ok = true;
    return res;
}

int main() {
    netsock::WsaGuard wsa;
    const Rails rails = sealed_rails();
    int fails = 0;

    // reference: sealed in-process session digest + the exact batch frame list
    const std::string expected =
        session::run_session(rails, sess_vec::SCENARIO, /*reconcile=*/true).digest;
    const session::ServerFrames ref_frames =
        session::build_server_frames(rails, sess_vec::SCENARIO);
    std::vector<std::vector<std::uint8_t>> ref_payloads;
    for (const auto& f : ref_frames) ref_payloads.push_back(f.second);
    const std::size_t N = ref_payloads.size();
    const std::size_t kJoin = N / 2;

    auto equals_ref = [&](const std::vector<std::vector<std::uint8_t>>& got) {
        if (got.size() != N) return false;
        for (std::size_t i = 0; i < N; ++i)
            if (got[i] != ref_payloads[i]) return false;
        return true;
    };
    auto digest_of = [&](const std::vector<std::vector<std::uint8_t>>& payloads) {
        return session::run_client(rails, sess_vec::SCENARIO, delivered_from_payloads(payloads),
                                   /*reconcile=*/true).digest;
    };

    // Each leg: the joiner at kJoin must receive exactly frames[start:] where
    // start = kJoin > W ? kJoin - W : 0, and the window must have evicted exactly
    // max(0, N - W) retentions by stream end. The early client is invariant across regimes.
    struct Leg {
        const char* name;
        std::size_t window;
    };
    const Leg legs[] = {
        {"leg0 (W=1, minimal)", 1},
        {"leg1 (1<W<J, partial)", kJoin / 2},
        {"leg2 (W>=N, degenerate=layer13)", N},
    };

    for (const Leg& leg : legs) {
        const std::size_t W = leg.window;
        const std::size_t start = kJoin > W ? kJoin - W : 0;
        const std::size_t want_trimmed = N > W ? N - W : 0;
        LegResult r = run_window_leg(rails, kJoin, W, leg.name);
        int leg_fails = 0;

        // the early client is untouched by ANY window: whole stream + the sealed digest.
        if (r.early_pending != 0 || !equals_ref(r.early)) {
            ++leg_fails;
            std::printf("FAIL %s: early client stream != batch reference (got %zu of %zu, "
                        "pending %zu)\n", leg.name, r.early.size(), N, r.early_pending);
        } else if (digest_of(r.early) != expected) {
            ++leg_fails;
            std::printf("FAIL %s: early-client digest != sealed session digest\n", leg.name);
        }

        // the joiner receives EXACTLY the contiguous suffix frames[start:] — windowed replay
        // (frames[start:kJoin]) then live (frames[kJoin:]), frame-aligned, no gap, no duplicate.
        bool suffix_ok = r.late_pending == 0 && r.late.size() == N - start;
        for (std::size_t i = 0; suffix_ok && i < r.late.size(); ++i)
            suffix_ok = r.late[i] == ref_payloads[start + i];
        if (!suffix_ok) {
            ++leg_fails;
            std::printf("FAIL %s: joiner did not receive exactly frames[%zu:] "
                        "(got %zu of %zu, pending %zu)\n", leg.name, start, r.late.size(),
                        N - start, r.late_pending);
        } else if (start == 0 && digest_of(r.late) != expected) {
            // a window covering the whole stream so far must recover full catch-up: the joiner
            // reconstructs the SAME sealed digest as a from-frame-0 client.
            ++leg_fails;
            std::printf("FAIL %s: whole-stream joiner digest != sealed session digest\n",
                        leg.name);
        }

        if (!r.stats.ok || r.stats.frames_sent != N || r.stats.joins != 2 ||
            r.stats.capped != 0 || r.stats.trimmed != want_trimmed) {
            ++leg_fails;
            std::printf("FAIL %s: server stats (ok=%d sent=%zu/%zu joins=%zu capped=%zu "
                        "trimmed=%zu want_trimmed=%zu)\n", leg.name, r.stats.ok ? 1 : 0,
                        r.stats.frames_sent, N, r.stats.joins, r.stats.capped, r.stats.trimmed,
                        want_trimmed);
        }

        if (leg_fails == 0)
            std::printf("%s PASS: joiner = exact suffix frames[%zu:], trimmed=%zu\n", leg.name,
                        start, want_trimmed);
        fails += leg_fails;
    }

    if (fails == 0) {
        std::printf("PASS layer-14 bounded/windowed catch-up bridge (minimal, partial, "
                    "degenerate windows; %zu frames, join at %zu)\n", N, kJoin);
        return 0;
    }
    std::printf("FAIL layer-14 window bridge: %d failure(s)\n", fails);
    return 1;
}
