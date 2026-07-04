// SEADS BIDIRECTIONAL-server determinism BRIDGE (netcode LAYER 16): the layer-15b upstream input path
// merged with the layer-11/12/15a downstream output hygiene (async send buffers / byte-cap / liveness
// reap). Layer 15b (seads_netinput_test) proved the authoritative kernel's frames are a pure function
// of the tick-stamped command SET, but over a BLOCKING send_all downstream — one slow client stalls the
// whole broadcast, and a silently-dead peer is never shed. Layer 16 brings the server->client-only
// hygiene of layers 11/12/15a to the bidirectional server (broadcast_bidi). This bridge proves, over
// real 127.0.0.1 sockets with NO sleeps / NO timing guesses, that the merge preserves the determinism
// claim AND bounds a misbehaving client:
//
//   LEG A (the async path preserves upstream order/chunk invariance — the merge anchor): a single
//   cooperative client sends a whole scenario's commands UP in a SCRAMBLED order + adversarial byte
//   CHUNKING while the server runs the FULL broadcast_bidi async path (non-blocking per-client send
//   buffers, cap_bytes=0, liveness=0), streaming the snapshots back DOWN. Grid-exact (dyadic) command
//   values round-trip the lossy wire bit-for-bit, so the frames are BYTE-IDENTICAL to
//   session::build_server_frames — exactly the layer-15b claim, now through the async send path (which
//   changes WHEN bytes move, never WHICH). Two sub-legs with different scrambles/chunkings.
//
//   LEG B (liveness reaps a dead client at cap_bytes==0 — orthogonal to the byte-cap): a LONG scenario
//   (a ~1 MB downstream stream) to TWO clients through a pinned tiny kernel send buffer, cap_bytes=0 so
//   ONLY liveness can drop anyone. FAST sends the driving commands UP once and is drained every frame by
//   the on_frame hook (the netheartbeat pattern) => never reaped, its downstream BYTE-IDENTICAL to
//   build_server_frames, and its commands drove the sim (cmds_ok == n_cmds). DEAD connects, sends
//   nothing, reads nothing => its backlog fills, sent_total freezes, and after liveness_frames silent
//   frames it is reaped (reaped==1, leaves==1, capped==0). Reaping DEAD changes nothing about the frames
//   or FAST's bytes — the determinism claim is untouched by a downstream drop.
//
//   LEG C (byte-cap sheds a slow client — drop-slowest): the same long scenario, liveness=0 and a finite
//   cap_bytes, so ONLY the cap can drop. FAST (hook-drained, commands-driving) survives byte-identical;
//   DEAD's pending backlog exceeds the cap and it is shed (capped==1, leaves==1, reaped==0). The cap
//   decides only WHO is dropped, never WHICH bytes flow.
//
// Every assertion is on delivered BYTES; all rendezvous are cv+notify_all (no sleeps — pacing coupled to
// on_frame / commands_sent). A finite watchdog fails (not wedges) on any socket hang. Exit 0 PASS, 1 FAIL.
//
// TRANSPORT-ONLY: no kernel/det_math/rails/wire/golden touched; broadcast_bidi is a SIBLING of
// broadcast_input + broadcast_live (sealed broadcast.cpp byte-for-byte untouched). Rides seal v1.26r0.
#include "bidiserver.h"
#include "inputserver.h"
#include "input001.h"
#include "cmdqueue.h"
#include "session.h"
#include "session_vectors.h"
#include "framing.h"
#include "socket.h"
#include "golden_params.h"
#include "kernel.h"

#include <algorithm>
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

// ---------------------------------------------------------------------------------------------
// A GRID-EXACT scenario (the INPUT-SK-001 shape): the three SESSION-SK-001 envelopes + start states,
// DYADIC command values so quantize->dequantize at 1e6 is the identity — the input path can then be
// byte-identical to the phase path. `ticks` is a parameter: a short run for LEG A (fast anchor) and a
// long run for LEG B/C (a ~1 MB downstream stream to overflow a never-reading client's buffers). The
// phases fire at ticks 0/50/80/120; beyond the last phase hold-last carries the sim for the whole run.
// ---------------------------------------------------------------------------------------------
static const session::Phase BI_P0[] = {
    {0u,   0.0,  1.0, 0.75, true},
    {50u,  0.5,  1.5, 1.0,  false},
    {120u, 0.0,  1.0, 0.75, false},
};
static const session::Phase BI_P1[] = {
    {0u,  -0.5,  1.0, 0.5,  false},
    {80u,  0.25, 2.0, 1.0,  true},
};
static const session::Phase BI_P2[] = {
    {0u,   0.5,  1.5, 1.0,  false},
};

static session::AircraftSpec BI_AC[3];
static const session::Scenario bidi_scenario(unsigned ticks) {
    static bool built = [] {
        for (int i = 0; i < 3; ++i) BI_AC[i] = sess_vec::AIRCRAFT[i];  // envelope + start state
        BI_AC[0].sched = BI_P0; BI_AC[0].n_phase = 3;
        BI_AC[1].sched = BI_P1; BI_AC[1].n_phase = 2;
        BI_AC[2].sched = BI_P2; BI_AC[2].n_phase = 1;
        return true;
    }();
    (void)built;
    session::Scenario sc = {BI_AC, 3u, ticks, /*snap_every=*/5u, /*lag=*/0u, /*render=*/0u,
                            /*drops=*/nullptr, /*n_drops=*/0u};
    return sc;
}

static std::vector<std::vector<std::uint8_t>> payloads_of(const session::ServerFrames& f) {
    std::vector<std::vector<std::uint8_t>> out;
    for (const auto& p : f) out.push_back(p.second);
    return out;
}

static bool frames_equal(const std::vector<std::vector<std::uint8_t>>& a,
                         const std::vector<std::vector<std::uint8_t>>& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (a[i] != b[i]) return false;
    return true;
}

// Encode a command list into one framed upstream byte stream (framing(encode_command) per record).
static std::vector<std::uint8_t> encode_upstream(
    const std::vector<input001::InputCommand>& cmds) {
    std::vector<std::uint8_t> upstream;
    for (const auto& c : cmds) {
        std::vector<std::uint8_t> rec, framed;
        input001::encode_command(c, rec);
        framing::encode_frame(rec, framed);
        upstream.insert(upstream.end(), framed.begin(), framed.end());
    }
    return upstream;
}

// Read the whole downstream to EOF, reassembling frames.
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
            break;
        }
    }
    pending = r.pending();
    return ok;
}

// =============================================================================================
// LEG A: one anchor sub-leg. A single cooperative client sends `cmds` UP in the given order, chunked
// to `chunk` bytes/send; the server runs broadcast_bidi through the FULL async path (cap=0, liveness=0)
// and the client reads the frames back. Asserts the downstream frames are byte-identical to `ref`
// (== build_server_frames) — the async send path preserves the layer-15b claim. Returns fail count.
// =============================================================================================
static int run_anchor_subleg(const Rails& rails, const session::Scenario& sc,
                             const std::vector<std::vector<std::uint8_t>>& ref,
                             const std::vector<input001::InputCommand>& cmds, std::size_t chunk,
                             const char* label) {
    const std::vector<std::uint8_t> upstream = encode_upstream(cmds);

    std::mutex mtx;
    std::condition_variable cv;
    std::uint16_t port_val = 0;
    bool port_ready = false;
    bool commands_sent = false;

    netinput::Stats stats;
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
        if (!netsock::is_valid(listener)) { ++done; return; }

        netinput::CommandQueue q(sc.n_aircraft);
        netinput::InputProducer producer(rails, sc, q);
        auto on_frame = [&](std::size_t fi) {
            if (fi != 0) return;
            std::unique_lock<std::mutex> lk(mtx);
            cv.wait(lk, [&] { return commands_sent; });
        };
        // FULL async path: cap_bytes=0, liveness_frames=0 (no downstream drop can occur — the merge
        // must reproduce build_server_frames exactly through the userspace send buffers).
        stats = netinput::broadcast_bidi(listener, producer, q, /*min_initial=*/1,
                                         /*accept_deadline_ms=*/10000, on_frame,
                                         /*cap_bytes=*/0, /*liveness_frames=*/0);
        netsock::close_socket(listener);
        ++done;
    });

    auto wait_port = [&]() -> std::uint16_t {
        std::unique_lock<std::mutex> lk(mtx);
        cv.wait(lk, [&] { return port_ready; });
        return port_val;
    };

    std::vector<std::vector<std::uint8_t>> got;
    std::size_t pending = 0;
    std::thread client([&] {
        std::uint16_t port = wait_port();
        if (port == 0) { ++done; return; }
        netsock::socket_t s = netsock::connect_loopback(port);
        if (netsock::is_valid(s)) {
            for (std::size_t off = 0; off < upstream.size(); off += chunk) {
                std::size_t take = (off + chunk <= upstream.size()) ? chunk : upstream.size() - off;
                netsock::send_all(s, upstream.data() + off, take);
            }
            {
                std::lock_guard<std::mutex> lk(mtx);
                commands_sent = true;
            }
            cv.notify_all();
            collect_to_eof(s, got, pending);
            netsock::close_socket(s);
        }
        ++done;
    });

    std::thread watchdog([&] {
        for (int i = 0; i < 400 && done.load() < 2; ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));  // up to ~40 s
        if (done.load() < 2) {
            std::printf("FAIL layer-16 anchor sub-leg [%s] TIMED OUT (socket hang)\n", label);
            std::fflush(stdout);
            std::_Exit(1);
        }
    });
    watchdog.detach();

    server.join();
    client.join();

    int fails = 0;
    const std::size_t n_cmds = cmds.size();
    if (!stats.ok || stats.frames_sent != ref.size() || stats.joins != 1) {
        ++fails;
        std::printf("FAIL [%s]: server stats (ok=%d sent=%zu/%zu joins=%zu)\n", label,
                    stats.ok ? 1 : 0, stats.frames_sent, ref.size(), stats.joins);
    }
    if (stats.cmds_ok != n_cmds || stats.cmds_stale != 0 || stats.cmds_oob != 0 ||
        stats.capped != 0 || stats.reaped != 0) {
        ++fails;
        std::printf("FAIL [%s]: command/hygiene accounting (ok=%zu stale=%zu oob=%zu capped=%zu "
                    "reaped=%zu; want %zu/0/0/0/0)\n", label, stats.cmds_ok, stats.cmds_stale,
                    stats.cmds_oob, stats.capped, stats.reaped, n_cmds);
    }
    if (pending != 0 || !frames_equal(got, ref)) {
        ++fails;
        std::printf("FAIL [%s]: downstream frames NOT byte-identical to build_server_frames "
                    "(got %zu of %zu, pending %zu)\n", label, got.size(), ref.size(), pending);
    }
    if (fails == 0)
        std::printf("  anchor [%s] PASS: %zu commands sent UP scrambled (chunk=%zu B) through the "
                    "async path -> %zu frames byte-identical to build_server_frames\n",
                    label, n_cmds, chunk, ref.size());
    return fails;
}

static int run_anchor_leg() {
    const Rails rails = sealed_rails();
    const session::Scenario sc = bidi_scenario(/*ticks=*/150u);
    const auto ref = payloads_of(session::build_server_frames(rails, sc));
    const auto canon = netinput::commands_from_scenario(sc);

    int fails = 0;
    // Sub-leg A: fully REVERSED command order, 1 byte per send (maximal reassembler fragmentation).
    std::vector<input001::InputCommand> rev(canon.rbegin(), canon.rend());
    fails += run_anchor_subleg(rails, sc, ref, rev, /*chunk=*/1, "reversed/1B");
    // Sub-leg B: ROTATE-by-3 order, 7 bytes per send (a different scramble + chunk boundary).
    std::vector<input001::InputCommand> rot = canon;
    if (rot.size() > 3) std::rotate(rot.begin(), rot.begin() + 3, rot.end());
    fails += run_anchor_subleg(rails, sc, ref, rot, /*chunk=*/7, "rotate3/7B");
    return fails;
}

// =============================================================================================
// LEG B/C: a shared hygiene harness over a LONG scenario. FAST sends the driving commands UP once, is
// drained every frame by the on_frame hook (so it keeps up by construction) => byte-identical
// downstream + its commands drive the sim. DEAD connects, sends/reads nothing => it is dropped by the
// policy under test (liveness at cap=0, or the byte-cap at liveness=0). `use_liveness` selects the
// policy; asserts exactly DEAD is dropped by it (the OTHER policy stat stays 0) and FAST is untouched.
// =============================================================================================
static int run_hygiene_leg(bool use_liveness, const char* label) {
    const Rails rails = sealed_rails();
    const session::Scenario sc = bidi_scenario(/*ticks=*/20000u);  // ~4000 frames, ~1 MB downstream
    const auto ref = payloads_of(session::build_server_frames(rails, sc));
    const auto canon = netinput::commands_from_scenario(sc);
    const std::size_t n_cmds = canon.size();
    // FAST sends the commands scrambled (reversed) — order-invariance still holds under hygiene.
    const std::vector<input001::InputCommand> fast_cmds(canon.rbegin(), canon.rend());
    const std::vector<std::uint8_t> upstream = encode_upstream(fast_cmds);

    const int kBufBytes = 16 * 1024;      // tiny kernel send buffer: DEAD stalls quickly
    const std::size_t kLiveness = use_liveness ? 16 : 0;
    const std::size_t kCap = use_liveness ? 0 : 128 * 1024;  // shed DEAD's backlog past 128 KiB

    std::mutex mtx;
    std::condition_variable cv;
    std::uint16_t port_val = 0;
    bool port_ready = false;
    bool server_done = false;
    bool commands_sent = false;
    netsock::socket_t fast_sock = netsock::invalid_socket();  // FAST's client endpoint (hook drains down)
    std::vector<std::uint8_t> fast_bytes;

    netinput::Stats stats;
    std::atomic<int> done{0};

    std::thread server([&] {
        std::uint16_t port = 0;
        netsock::socket_t listener = netsock::listen_loopback(0, port, /*backlog=*/3);
        if (netsock::is_valid(listener)) {
            netsock::set_sndbuf(listener, kBufBytes);  // accepted sockets inherit the tiny send buffer
            netsock::set_nonblocking(listener);
        }
        {
            std::lock_guard<std::mutex> lk(mtx);
            port_val = netsock::is_valid(listener) ? port : 0;
            port_ready = true;
        }
        cv.notify_all();
        if (!netsock::is_valid(listener)) { ++done; return; }

        netinput::CommandQueue q(sc.n_aircraft);
        netinput::InputProducer producer(rails, sc, q);
        // on_frame: at fi==0 block until FAST has sent its commands (ingested before tick 0 is stepped);
        // every frame, drain FAST's downstream so it keeps up by construction (netheartbeat pattern).
        auto on_frame = [&](std::size_t fi) {
            if (fi == 0) {
                std::unique_lock<std::mutex> lk(mtx);
                cv.wait(lk, [&] { return commands_sent; });
            }
            netsock::socket_t s;
            {
                std::lock_guard<std::mutex> lk(mtx);
                s = fast_sock;
            }
            if (!netsock::is_valid(s)) return;
            std::uint8_t buf[4096];
            while (netsock::wait_readable(s, 0)) {
                std::ptrdiff_t n = netsock::recv_some(s, buf, sizeof(buf));
                if (n <= 0) break;
                fast_bytes.insert(fast_bytes.end(), buf, buf + n);
            }
        };

        stats = netinput::broadcast_bidi(listener, producer, q, /*min_initial=*/2,
                                         /*accept_deadline_ms=*/10000, on_frame, kCap, kLiveness);
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

    // DEAD: connects, sends nothing, reads nothing until the broadcast returns (dropped long before),
    // then drains its kernel-buffered prefix to EOF.
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
            std::printf("FAIL layer-16 hygiene leg [%s] TIMED OUT (wedge)\n", label);
            std::fflush(stdout);
            std::_Exit(1);
        }
    });
    watchdog.detach();

    // FAST connects on the main thread and sends its commands UP; the server's hook reads its downstream.
    {
        std::uint16_t port = wait_port();
        if (port != 0) {
            netsock::socket_t s = netsock::connect_loopback(port);
            if (netsock::is_valid(s)) {
                for (std::size_t off = 0; off < upstream.size(); off += 32) {
                    std::size_t take = (off + 32 <= upstream.size()) ? 32 : upstream.size() - off;
                    netsock::send_all(s, upstream.data() + off, take);
                }
                {
                    std::lock_guard<std::mutex> lk(mtx);
                    fast_sock = s;
                    commands_sent = true;
                }
                cv.notify_all();
            }
        }
    }

    server.join();
    dead.join();

    // Final drain of FAST: the server has closed FAST's endpoint so a blocking read runs to EOF.
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
    if (!stats.ok || stats.frames_sent != ref.size()) {
        ++fails;
        std::printf("FAIL [%s]: server did not finish the stream (ok=%d sent=%zu/%zu)\n", label,
                    stats.ok ? 1 : 0, stats.frames_sent, ref.size());
    }
    // FAST's commands must have driven the sim (all accepted, none stale/oob).
    if (stats.cmds_ok != n_cmds || stats.cmds_stale != 0 || stats.cmds_oob != 0) {
        ++fails;
        std::printf("FAIL [%s]: FAST's commands did not all land (ok=%zu stale=%zu oob=%zu; want "
                    "%zu/0/0)\n", label, stats.cmds_ok, stats.cmds_stale, stats.cmds_oob, n_cmds);
    }
    // Exactly DEAD dropped, by the policy under test; the OTHER policy stat stays 0.
    const std::size_t want_reaped = use_liveness ? 1u : 0u;
    const std::size_t want_capped = use_liveness ? 0u : 1u;
    if (stats.joins != 2 || stats.leaves != 1 || stats.reaped != want_reaped ||
        stats.capped != want_capped) {
        ++fails;
        std::printf("FAIL [%s]: expected exactly DEAD dropped by %s (joins=%zu leaves=%zu reaped=%zu "
                    "capped=%zu; want 2/1/%zu/%zu)\n", label, use_liveness ? "liveness" : "byte-cap",
                    stats.joins, stats.leaves, stats.reaped, stats.capped, want_reaped, want_capped);
    }
    bool fast_identical = fast_ok && fast_pending == 0 && fast_payloads.size() == ref.size();
    for (std::size_t i = 0; fast_identical && i < ref.size(); ++i)
        if (fast_payloads[i] != ref[i]) fast_identical = false;
    if (!fast_identical) {
        ++fails;
        std::printf("FAIL [%s]: FAST's stream not byte-identical to build_server_frames (got %zu of "
                    "%zu frames, pending %zu)\n", label, fast_payloads.size(), ref.size(),
                    fast_pending);
    }
    // DEAD's delivered bytes must be a clean, strictly-short byte-prefix of FAST's whole stream.
    std::vector<std::uint8_t> whole;
    for (const auto& p : ref) {
        std::vector<std::uint8_t> tmp;
        framing::encode_frame(p, tmp);
        whole.insert(whole.end(), tmp.begin(), tmp.end());
    }
    bool dead_prefix = !dead_bytes.empty() && dead_bytes.size() < whole.size();
    for (std::size_t i = 0; dead_prefix && i < dead_bytes.size(); ++i)
        if (dead_bytes[i] != whole[i]) dead_prefix = false;
    if (!dead_prefix) {
        ++fails;
        std::printf("FAIL [%s]: DEAD's %zu delivered bytes are not a strict byte-prefix of the "
                    "%zu-byte encoded stream\n", label, dead_bytes.size(), whole.size());
    }
    if (fails == 0)
        std::printf("  %s PASS: DEAD dropped by %s (reaped=%zu capped=%zu), FAST byte-identical to "
                    "build_server_frames + its %zu commands drove the sim, DEAD a strict byte-prefix\n",
                    label, use_liveness ? "liveness" : "byte-cap", stats.reaped, stats.capped, n_cmds);
    return fails;
}

int main() {
    netsock::WsaGuard wsa;
    int fails = 0;
    fails += run_anchor_leg();
    fails += run_hygiene_leg(/*use_liveness=*/true, "leg B (liveness reap, cap=0)");
    fails += run_hygiene_leg(/*use_liveness=*/false, "leg C (byte-cap shed, liveness=0)");
    if (fails == 0) {
        std::printf("PASS: layer-16 bidirectional server — the async/byte-cap/liveness downstream "
                    "hygiene composes with the upstream input path; scrambled commands still produce "
                    "build_server_frames byte-for-byte, and a dead client is bounded (reaped/capped) "
                    "without disturbing a cooperative client\n");
        return 0;
    }
    std::printf("RESULT: layer-16 bidi bridge FAIL (%d mismatches)\n", fails);
    return 1;
}
