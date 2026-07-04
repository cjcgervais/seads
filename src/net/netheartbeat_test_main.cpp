// SEADS HEARTBEAT / LIVENESS-TIMEOUT LEAVE determinism BRIDGE for the live broadcast server
// (netcode LAYER 15a).
//
// Every leave through layer 14 is EXPLICIT — a clean TCP EOF (recv==0), a fatal send, or a layer-12
// byte-cap shed by backlog SIZE. A SILENTLY-dead client (process killed with the socket half-open,
// or a network partition) sends no EOF for minutes: it is neither readable nor writable, and at
// cap_bytes==0 its userspace backlog grows without bound on an open-ended live stream. Layer 15a adds
// broadcast_live's `liveness_frames`: a client that makes NO receive progress (drains zero bytes AND
// stays pending) for more than liveness_frames consecutive PRODUCED frames is REAPED (Stats.reaped,
// and — being live — also a leave). This bridge proves, over real 127.0.0.1 sockets with NO sleeps /
// NO timing guesses, two claims:
//
//   LEG 1 (a dead client is reaped, even at cap_bytes==0): a ~7.5 MiB synthetic stream pulled from a
//   live FrameSource through a pinned tiny kernel send buffer, to TWO clients, with cap_bytes==0 (so
//   ONLY liveness can drop anyone — orthogonal to layer 12). FAST keeps up BY CONSTRUCTION — the
//   on_frame hook drains FAST's receive pipe at the top of every frame iteration (the netcap
//   pattern; the hook runs on the server thread, reading the CLIENT endpoint the test owns), so
//   FAST's backlog stays ~one frame and its sent_total advances every frame ⇒ it is NEVER reaped and
//   receives the whole stream byte-identically. DEAD reads NOTHING: once its kernel/userspace buffers
//   fill, its sent_total freezes and after liveness_frames silent frames it is reaped (reaped==1,
//   leaves==1). WHICH frame DEAD is reaped at is OS-timing (how many frames its buffers absorb before
//   send stalls) — deliberately unasserted, exactly like netcap's shed frame; the deterministic
//   claims are that DEAD IS reaped (bounded backlog), the survivor's bytes are untouched, and DEAD's
//   delivered bytes are a strict byte-PREFIX of the encoded stream. A capless layer-11/13 server
//   would instead buffer the whole ~7.5 MiB for DEAD forever — this is exactly the bound layer 15a
//   adds.
//
//   LEG 2 (liveness never bites the healthy path): the sealed SESSION-SK-001 stream, pulled live
//   from session::FrameProducer, to an EARLY client that reads continuously, run with an AGGRESSIVE
//   liveness_frames==1. A keeping-up client is fully drained every frame (or advances sent_total), so
//   it is NEVER reaped: it receives the whole stream byte-identically, reconstructs the sealed
//   digest, and reaped==0. Enabling the policy on a client that keeps up changes nothing (the reap
//   decides only WHO is dropped, never WHICH bytes flow) — the exact mirror of netcap's leg 2.
//
// Every assertion is on delivered BYTES; all rendezvous are cv+notify_all (no std::future — MinGW
// call_once caveat; no sleeps — pacing is coupled to on_frame). A finite watchdog fails (not wedges)
// on any socket hang. Exit 0 PASS, 1 FAIL.
//
// TRANSPORT-ONLY: no kernel/det_math/rails/wire/golden touched; rides seal v1.25r0.
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
// LEG 1: liveness reaps a DEAD client at cap_bytes==0. FAST (drained by the hook) survives whole;
// DEAD (reads nothing) is reaped, its backlog bounded, its bytes a strict prefix. Returns fail count.
// ---------------------------------------------------------------------------------------------
static int run_reap_leg() {
    // 512 frames x 15 KiB ~= 7.5 MiB: orders of magnitude beyond DEAD's kernel-absorbable bytes, so
    // DEAD provably goes silent (sent_total frozen) for far more than liveness_frames.
    const std::size_t kFrames = 512, kFrameBytes = 15 * 1024;
    const int kBufBytes = 16 * 1024;
    const std::size_t kLiveness = 16;  // reap after 16 no-progress frames — small vs the stream
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
    netsock::socket_t fast_sock = netsock::invalid_socket();  // FAST's client endpoint (hook drains)
    std::vector<std::uint8_t> fast_bytes;

    netbcast::Stats stats;
    std::atomic<int> done{0};

    std::thread server([&] {
        std::uint16_t port = 0;
        netsock::socket_t listener = netsock::listen_loopback(0, port, /*backlog=*/2);
        if (netsock::is_valid(listener)) {
            netsock::set_sndbuf(listener, kBufBytes);  // tiny send buffer: DEAD stalls quickly
            netsock::set_nonblocking(listener);
        }
        {
            std::lock_guard<std::mutex> lk(mtx);
            port_val = netsock::is_valid(listener) ? port : 0;
            port_ready = true;
        }
        cv.notify_all();
        if (!netsock::is_valid(listener)) return;

        // FAST keeps up BY CONSTRUCTION: the hook drains FAST's receive pipe at the top of every
        // frame iteration (a poll, no sleeps; the netcap Linux lesson — never park the producer on
        // a consumer it is the only flusher for).
        auto on_frame = [&](std::size_t) {
            netsock::socket_t s;
            {
                std::lock_guard<std::mutex> lk(mtx);
                s = fast_sock;
            }
            if (!netsock::is_valid(s)) return;  // frame 0 can race FAST's connect-return; harmless
            std::uint8_t buf[4096];
            while (netsock::wait_readable(s, 0)) {
                std::ptrdiff_t n = netsock::recv_some(s, buf, sizeof(buf));
                if (n <= 0) break;
                fast_bytes.insert(fast_bytes.end(), buf, buf + n);
            }
        };

        std::size_t next = 0;
        auto source = [&](std::vector<std::uint8_t>& payload) {
            if (next >= kFrames) return false;
            payload = payloads[next++];  // a whole synthetic "snapshot" payload
            return true;
        };
        // cap_bytes==0: ONLY liveness can drop anyone — this is the orthogonality proof.
        stats = netbcast::broadcast_live(listener, source, /*min_initial=*/2,
                                         /*accept_deadline_ms=*/10000, on_frame, /*catchup=*/false,
                                         /*cap_bytes=*/0, /*catchup_window=*/0, kLiveness);
        netsock::close_socket(listener);
        {
            std::lock_guard<std::mutex> lk(mtx);
            server_done = true;
        }
        cv.notify_all();
        ++done;
    });

    auto wait_port = [&]() -> std::uint16_t {
        std::unique_lock<std::mutex> lk(mtx);
        cv.wait(lk, [&] { return port_ready; });
        return port_val;
    };

    // DEAD: connects, reads NOTHING until the broadcast returns (reaped long before), then drains
    // its kernel-buffered prefix to EOF.
    std::vector<std::uint8_t> dead_bytes;
    std::thread dead([&] {
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
                dead_bytes.insert(dead_bytes.end(), buf, buf + n);
            }
            netsock::close_socket(s);
        }
        ++done;
    });

    std::thread watchdog([&] {
        for (int i = 0; i < 600 && done.load() < 2; ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));  // up to ~60 s
        if (done.load() < 2) {
            std::printf("FAIL layer-15a reap leg TIMED OUT (wedge: liveness reap or the FAST drain "
                        "broke the frame loop)\n");
            std::fflush(stdout);  // _Exit skips stdio flush; don't lose the diagnostic (CI lesson)
            std::_Exit(1);
        }
    });
    watchdog.detach();

    // FAST connects on the main thread; the server's on_frame hook does its reading.
    {
        std::uint16_t port = wait_port();
        if (port != 0) {
            netsock::socket_t s = netsock::connect_loopback(port);
            std::lock_guard<std::mutex> lk(mtx);
            fast_sock = s;
        }
    }

    server.join();
    dead.join();

    // Final drain of FAST: the hook stopped at the last frame; the server has closed FAST's endpoint
    // so a blocking read runs to EOF.
    std::vector<std::vector<std::uint8_t>> fast_payloads;
    std::size_t fast_pending = 0;
    bool fast_ok = false;
    if (netsock::is_valid(fast_sock)) {
        std::uint8_t buf[4096];
        while (true) {
            std::ptrdiff_t n = netsock::recv_some(fast_sock, buf, sizeof(buf));
            if (n <= 0) break;
            fast_bytes.insert(fast_bytes.end(), buf, buf + n);
        }
        netsock::close_socket(fast_sock);
        framing::StreamReassembler r;
        fast_ok = fast_bytes.empty() || r.feed(fast_bytes.data(), fast_bytes.size(), fast_payloads);
        fast_pending = r.pending();
    }

    int fails = 0;
    if (!stats.ok || stats.frames_sent != kFrames) {
        ++fails;
        std::printf("FAIL reap leg: server did not finish the stream (ok=%d sent=%zu/%zu)\n",
                    stats.ok ? 1 : 0, stats.frames_sent, kFrames);
    }
    // cap_bytes==0, so capped MUST be 0 — the drop can only be liveness. Exactly DEAD is reaped.
    if (stats.joins != 2 || stats.reaped != 1 || stats.leaves != 1 || stats.capped != 0) {
        ++fails;
        std::printf("FAIL reap leg: expected exactly DEAD reaped by liveness (joins=%zu leaves=%zu "
                    "reaped=%zu capped=%zu; want 2/1/1/0)\n",
                    stats.joins, stats.leaves, stats.reaped, stats.capped);
    }
    bool fast_identical = fast_ok && fast_pending == 0 && fast_payloads.size() == kFrames;
    for (std::size_t i = 0; fast_identical && i < kFrames; ++i)
        if (fast_payloads[i] != payloads[i]) fast_identical = false;
    if (!fast_identical) {
        ++fails;
        std::printf("FAIL reap leg: FAST's stream not byte-identical (got %zu of %zu frames, "
                    "pending %zu)\n", fast_payloads.size(), kFrames, fast_pending);
    }
    const bool dead_strict = !dead_bytes.empty() && dead_bytes.size() < expected_stream.size();
    bool dead_prefix = dead_strict;
    for (std::size_t i = 0; dead_prefix && i < dead_bytes.size(); ++i)
        if (dead_bytes[i] != expected_stream[i]) dead_prefix = false;
    if (!dead_prefix) {
        ++fails;
        std::printf("FAIL reap leg: DEAD's %zu delivered bytes are not a strict byte-prefix of the "
                    "%zu-byte encoded stream\n", dead_bytes.size(), expected_stream.size());
    }
    if (fails == 0)
        std::printf("  leg 1 PASS: cap=0, liveness=%zu reaped exactly DEAD (joins=2 reaped=1) — "
                    "its backlog bounded, FAST byte-identical, DEAD a strict byte-prefix, stream "
                    "completed un-wedged\n", kLiveness);
    return fails;
}

// ---------------------------------------------------------------------------------------------
// LEG 2: an aggressive liveness deadline never bites a client that keeps up — the sealed-session
// stream to an EARLY continuous reader with liveness_frames==1; whole stream + sealed digest,
// reaped==0. The mirror of netcap's healthy-path leg.
// ---------------------------------------------------------------------------------------------
static int run_healthy_leg() {
    const Rails rails = sealed_rails();
    const std::string expected =
        session::run_session(rails, sess_vec::SCENARIO, /*reconcile=*/true).digest;
    const session::ServerFrames ref_frames =
        session::build_server_frames(rails, sess_vec::SCENARIO);
    std::vector<std::vector<std::uint8_t>> ref_payloads;
    for (const auto& f : ref_frames) ref_payloads.push_back(f.second);
    const std::size_t N = ref_payloads.size();
    const std::size_t kLiveness = 1;  // aggressive: a keeping-up client must still never be reaped

    std::mutex mtx;
    std::condition_variable cv;
    std::uint16_t port_val = 0;
    bool port_ready = false;

    netbcast::Stats stats;
    std::atomic<int> done{0};

    std::thread server([&] {
        std::uint16_t port = 0;
        netsock::socket_t listener = netsock::listen_loopback(0, port, /*backlog=*/2);
        if (netsock::is_valid(listener)) netsock::set_nonblocking(listener);
        {
            std::lock_guard<std::mutex> lk(mtx);
            port_val = netsock::is_valid(listener) ? port : 0;
            port_ready = true;
        }
        cv.notify_all();
        if (!netsock::is_valid(listener)) return;

        session::FrameProducer producer(rails, sess_vec::SCENARIO);
        auto source = [&](std::vector<std::uint8_t>& payload) {
            std::int64_t emit_tick = 0;
            return producer.next(emit_tick, payload);
        };
        stats = netbcast::broadcast_live(listener, source, /*min_initial=*/1,
                                         /*accept_deadline_ms=*/10000, /*on_frame=*/{},
                                         /*catchup=*/false, /*cap_bytes=*/0, /*catchup_window=*/0,
                                         kLiveness);
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
            collect_to_eof(s, early_payloads, early_pending);  // reads continuously => keeps up
            netsock::close_socket(s);
            if (early_pending == 0 && !early_payloads.empty())
                early_digest = session::run_client(rails, sess_vec::SCENARIO,
                                                   delivered_from_payloads(early_payloads),
                                                   /*reconcile=*/true).digest;
        }
        ++done;
    });

    std::thread watchdog([&] {
        for (int i = 0; i < 400 && done.load() < 1; ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));  // up to ~40 s
        if (done.load() < 1) {
            std::printf("FAIL layer-15a healthy leg TIMED OUT (socket hang)\n");
            std::fflush(stdout);
            std::_Exit(1);
        }
    });
    watchdog.detach();

    server.join();
    early.join();

    int fails = 0;
    auto equals_ref = [&](const std::vector<std::vector<std::uint8_t>>& got) {
        if (got.size() != N) return false;
        for (std::size_t i = 0; i < N; ++i)
            if (got[i] != ref_payloads[i]) return false;
        return true;
    };
    if (early_pending != 0 || !equals_ref(early_payloads) || early_digest != expected) {
        ++fails;
        std::printf("FAIL healthy leg: EARLY stream/digest mismatch under an aggressive liveness "
                    "deadline (got %zu of %zu frames, pending %zu; digest %s, expected %s)\n",
                    early_payloads.size(), N, early_pending, early_digest.c_str(),
                    expected.c_str());
    }
    if (!stats.ok || stats.frames_sent != N || stats.joins != 1 || stats.reaped != 0 ||
        stats.leaves != 0) {
        ++fails;
        std::printf("FAIL healthy leg: server stats wrong (ok=%d sent=%zu/%zu joins=%zu reaped=%zu "
                    "leaves=%zu; a keeping-up client must never be reaped)\n",
                    stats.ok ? 1 : 0, stats.frames_sent, N, stats.joins, stats.reaped,
                    stats.leaves);
    }
    if (fails == 0)
        std::printf("  leg 2 PASS: liveness=1 (aggressive), reaped=0 — EARLY (continuous reader) "
                    "received all %zu frames and reconstructed SESSION-SK-001 (%s)\n",
                    N, expected.c_str());
    return fails;
}

int main() {
    netsock::WsaGuard wsa;
    int fails = 0;
    fails += run_reap_leg();
    fails += run_healthy_leg();
    if (fails == 0) {
        std::printf("PASS: layer-15a heartbeat/liveness-timeout — a silently-dead client is reaped "
                    "(bounded backlog even at cap_bytes=0), a keeping-up client never is\n");
        return 0;
    }
    std::printf("RESULT: layer-15a heartbeat bridge FAIL (%d mismatches)\n", fails);
    return 1;
}
