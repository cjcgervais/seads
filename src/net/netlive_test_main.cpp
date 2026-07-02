// SEADS open-ended LIVE-frame-SOURCE determinism BRIDGE for the async broadcast server
// (netcode LAYER 13).
//
// Layers 7-12 all broadcast a PRECOMPUTED finite payload list: the sealed session ran to
// completion before the first byte moved, and the loops indexed frames[fi] of a known size. A
// real live server does the opposite — it STEPS THE SIMULATION BETWEEN SENDS and does not know
// how long the stream is. Layer 13 (netbcast::broadcast_live) feeds broadcast_async's machinery
// from a pull SOURCE (session::FrameProducer — the authoritative kernel stepped on demand, one
// 20 Hz frame per pull). This bridge proves, over real 127.0.0.1 sockets with NO sleeps / NO
// timing guesses, three claims:
//
//   LEG 0 (incremental == batch, pure): session::FrameProducer::next(), pulled to exhaustion,
//   yields exactly build_server_frames' (emit_tick, bytes) list — same count, same ticks, every
//   payload byte-identical. (build_server_frames is implemented ON the producer, and the sealed
//   session digest gates it — this leg pins the equivalence explicitly.)
//
//   LEG 1 (live socket, catchup=false): a server whose kernel is stepped INSIDE the broadcast
//   loop (a fresh FrameProducer pulled by broadcast_live) streams to an EARLY client (connected
//   before frame 0) and a LATE joiner (rendezvoused to an exact frame J via on_frame). EARLY's
//   delivered payloads are byte-identical to the batch reference AND reconstruct the sealed
//   SESSION-SK-001 digest; LATE receives exactly the suffix frames[J:] — the layer-9 membership
//   law, from a stream that never existed as a whole.
//
//   LEG 2 (live socket, catchup=true): same shape, but the mid-stream joiner is replayed the
//   missed prefix out of the ON-THE-FLY retained history — it receives the WHOLE stream
//   byte-identically to EARLY and reconstructs the SAME sealed digest. Catch-up works without a
//   precomputed frame list.
//
// Every assertion is on delivered BYTES; all rendezvous are cv+notify_all (no std::future — MinGW
// call_once caveat; no sleeps — on_frame pins exact frames). A finite watchdog fails (not wedges)
// on any socket hang. Exit 0 PASS, 1 FAIL.
//
// TRANSPORT-ONLY: no kernel/det_math/rails/wire/golden touched; rides seal v1.20r0.
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

// One live-socket leg: broadcast_live pulls a FRESH FrameProducer (the kernel steps inside the
// loop) to an EARLY client and a mid-stream joiner rendezvoused at frame kJoin. Returns the
// delivered payload lists + digests + server stats through the out-params; false on setup failure.
struct LegResult {
    std::vector<std::vector<std::uint8_t>> early, late;
    std::size_t early_pending = 1, late_pending = 1;
    std::string early_digest, late_digest;
    netbcast::Stats stats;
    bool setup_ok = false;
};

static LegResult run_live_leg(const Rails& rails, std::size_t kJoin, bool catchup,
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

        // THE LAYER-13 POINT: the frames do not exist yet — the sealed kernel is stepped between
        // sends, one 20 Hz frame per pull, inside the broadcast loop.
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
                                             /*accept_deadline_ms=*/10000, on_frame, catchup,
                                             /*cap_bytes=*/0);
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
            late_connected = true;  // release the server: accept us (replay history iff catchup)
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
            std::printf("FAIL layer-13 live bridge (%s) TIMED OUT (socket hang)\n", leg_name);
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

    // ---- LEG 0: incremental producer == batch builder, byte-for-byte (pure, no sockets) --------
    {
        session::FrameProducer producer(rails, sess_vec::SCENARIO);
        session::ServerFrames inc;
        std::int64_t emit_tick = 0;
        std::vector<std::uint8_t> payload;
        while (producer.next(emit_tick, payload)) inc.emplace_back(emit_tick, payload);
        bool same = inc.size() == ref_frames.size();
        for (std::size_t i = 0; same && i < inc.size(); ++i)
            same = inc[i].first == ref_frames[i].first && inc[i].second == ref_frames[i].second;
        if (!same) {
            ++fails;
            std::printf("FAIL leg0: FrameProducer stream != build_server_frames "
                        "(%zu vs %zu frames)\n", inc.size(), ref_frames.size());
        } else {
            std::printf("leg0 PASS: incremental producer == batch builder (%zu frames)\n",
                        inc.size());
        }
        // the producer must also report a clean end exactly once more (idempotent exhaustion)
        if (producer.next(emit_tick, payload)) {
            ++fails;
            std::printf("FAIL leg0: producer yielded a frame past its end\n");
        }
    }

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

    // ---- LEG 1: live socket, catchup=false — EARLY whole+digest, LATE the exact suffix ---------
    {
        LegResult r = run_live_leg(rails, kJoin, /*catchup=*/false, "leg1");
        if (r.early_pending != 0 || !equals_ref(r.early)) {
            ++fails;
            std::printf("FAIL leg1: early client stream != batch reference (got %zu of %zu, "
                        "pending %zu)\n", r.early.size(), N, r.early_pending);
        } else if (digest_of(r.early) != expected) {
            ++fails;
            std::printf("FAIL leg1: early-client digest != sealed session digest\n");
        }
        bool suffix_ok = r.late_pending == 0 && r.late.size() == N - kJoin;
        for (std::size_t i = 0; suffix_ok && i < r.late.size(); ++i)
            suffix_ok = r.late[i] == ref_payloads[kJoin + i];
        if (!suffix_ok) {
            ++fails;
            std::printf("FAIL leg1: late joiner did not receive exactly frames[%zu:] "
                        "(got %zu of %zu, pending %zu)\n", kJoin, r.late.size(), N - kJoin,
                        r.late_pending);
        }
        if (!r.stats.ok || r.stats.frames_sent != N || r.stats.joins != 2 || r.stats.capped != 0) {
            ++fails;
            std::printf("FAIL leg1: server stats (ok=%d sent=%zu/%zu joins=%zu capped=%zu)\n",
                        r.stats.ok ? 1 : 0, r.stats.frames_sent, N, r.stats.joins, r.stats.capped);
        }
        if (fails == 0)
            std::printf("leg1 PASS: live-stepped stream — early byte-identical + sealed digest, "
                        "late = exact suffix frames[%zu:]\n", kJoin);
    }

    // ---- LEG 2: live socket, catchup=true — the joiner gets the WHOLE stream from the on-the-fly
    // retained history and reconstructs the sealed digest -----------------------------------------
    {
        LegResult r = run_live_leg(rails, kJoin, /*catchup=*/true, "leg2");
        if (r.early_pending != 0 || !equals_ref(r.early) || digest_of(r.early) != expected) {
            ++fails;
            std::printf("FAIL leg2: early client stream/digest wrong under catch-up mode\n");
        }
        if (r.late_pending != 0 || !equals_ref(r.late)) {
            ++fails;
            std::printf("FAIL leg2: catch-up joiner did not receive the whole frames[0:] "
                        "(got %zu of %zu, pending %zu)\n", r.late.size(), N, r.late_pending);
        } else if (digest_of(r.late) != expected) {
            ++fails;
            std::printf("FAIL leg2: catch-up-joiner digest != sealed session digest\n");
        }
        if (!r.stats.ok || r.stats.frames_sent != N || r.stats.joins != 2) {
            ++fails;
            std::printf("FAIL leg2: server stats (ok=%d sent=%zu/%zu joins=%zu)\n",
                        r.stats.ok ? 1 : 0, r.stats.frames_sent, N, r.stats.joins);
        }
        if (fails == 0)
            std::printf("leg2 PASS: catch-up from on-the-fly history — joiner reconstructs the "
                        "sealed digest\n");
    }

    if (fails == 0) {
        std::printf("PASS layer-13 live-frame-source bridge (incremental==batch, live suffix, "
                    "live catch-up; %zu frames)\n", N);
        return 0;
    }
    std::printf("FAIL layer-13 live bridge: %d failure(s)\n", fails);
    return 1;
}
