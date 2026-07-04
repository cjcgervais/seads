// SEADS AUTHENTICATED + ASYNC + CATCH-UP determinism BRIDGE (netcode LAYER 23): the layer-21 IDENTITY
// binding + the layer-16/19 async hygiene + broadcast_live's windowed late-join CATCH-UP, all composed —
// the LAST rung of the authenticated arc (21 -> 22 -> 23, the 19->20 step re-run on the authenticated
// server). broadcast_auth_catchup is a SIBLING of both broadcast_auth_async (layer 22) and
// broadcast_bound_catchup (layer 20). This bridge proves, over real 127.0.0.1 sockets with NO sleeps / NO
// timing guesses, that the composition preserves ALL THREE claims:
//
//   LEG 1 (identity binding + catch-up window regimes — the headline): THREE identities present DISTINCT
//   tokens from the initial gather (token order != seat order: 100->2, 200->0, 300->1), learn their seat
//   from the BIND handshake, and upstream ONLY that seat's commands (SCRAMBLED + chunked) — so the produced
//   stream is BYTE-IDENTICAL to session::build_server_frames and each draws EXACTLY its designated seat. A
//   FOURTH client joins mid-stream at frame kJoin with an UNKNOWN token (999 -> SPECTATOR, seats full
//   anyway) and — across three window regimes W ∈ {1, kJoin/2, retain-all} — receives EXACTLY
//   [BIND(spectator) | frames[max(0,kJoin-W):]], frame-aligned, with trimmed == max(0, N-W). The three
//   seated identities are byte-identical to the whole stream under EVERY window.
//
//   LEG 2 (authentication + authorization compose with catch-up): the mid-stream SPECTATOR (unknown token)
//   upstreams the WHOLE scenario's commands; a spectator authorizes NOTHING, so every one is dropped
//   (cmds_unauth == the whole set) and the produced stream is unchanged — yet the spectator STILL catches
//   up the whole stream byte-for-byte. Catch-up did not weaken authentication; authentication did not
//   weaken catch-up.
//
//   LEG 3 (hygiene composes with identity + catch-up): on a LONG stream through a pinned tiny kernel send
//   buffer, a seated FAST identity (token 200 -> seat 0, hook-drained) is byte-identical to its seat's
//   reference, while a NON-READING authenticated DEAD identity (token 300 -> seat 1) joining mid-stream has
//   its catch-up REPLAY backlog shed by the byte-cap (Stats.capped) — its SEAT (1) freed on the drop, so a
//   reconnecting identity 300 would reclaim it. The cap decided WHO is dropped, never WHICH bytes flow:
//   FAST is untouched and DEAD's delivery is [BIND(seat 1) | strict byte-prefix]. (Whether the cap trips
//   during the replay itself or just after the joiner becomes live is an OS-buffering detail — the same
//   policy outcome; the leg asserts the platform-invariant.)
//
// Every socket assertion is on delivered BYTES; all rendezvous are cv+notify_all (no sleeps). A finite
// watchdog fails (not wedges) on any socket hang. Exit 0 PASS, 1 FAIL.
//
// TRANSPORT-ONLY: no kernel/det_math/rails/wire/golden touched; broadcast_auth_catchup is a SIBLING of
// broadcast_auth_async + broadcast_bound_catchup (sealed broadcast.cpp + all prior servers untouched);
// HELLO-001/BIND-001 are transport metadata (layer-7 framing-envelope category, NOT a sealed rails.wire
// block) => no seal. Rides v1.26r0.
#include "authcatchupserver.h"
#include "authserver.h"
#include "boundserver.h"
#include "inputserver.h"
#include "input001.h"
#include "hello001.h"
#include "bind001.h"
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
#include <functional>
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
// INPUT-SK-001: the SAME grid-exact (dyadic) 3-aircraft scenario the layer-15b..22 bridges use, so a
// grid-exact command survives the lossy wire UNCHANGED and the input path is byte-identical to the
// phase-schedule server. `ticks` is a parameter: short for LEG 1/2 (fast anchors), long for LEG 3
// (a big downstream, so a non-reading joiner's catch-up backlog overflows a tiny buffer).
// ---------------------------------------------------------------------------------------------
static const session::Phase AC_P0[] = {
    {0u,   0.0,  1.0, 0.75, true},
    {50u,  0.5,  1.5, 1.0,  false},
    {120u, 0.0,  1.0, 0.75, false},
};
static const session::Phase AC_P1[] = {
    {0u,  -0.5,  1.0, 0.5,  false},
    {80u,  0.25, 2.0, 1.0,  true},
};
static const session::Phase AC_P2[] = {
    {0u,   0.5,  1.5, 1.0,  false},
};

static session::AircraftSpec AC_AC[3];
static const session::Scenario ac_scenario(unsigned ticks) {
    static bool built = [] {
        for (int i = 0; i < 3; ++i) AC_AC[i] = sess_vec::AIRCRAFT[i];  // envelope + start state
        AC_AC[0].sched = AC_P0; AC_AC[0].n_phase = 3;
        AC_AC[1].sched = AC_P1; AC_AC[1].n_phase = 2;
        AC_AC[2].sched = AC_P2; AC_AC[2].n_phase = 1;
        return true;
    }();
    (void)built;
    session::Scenario sc = {AC_AC, 3u, ticks, /*snap_every=*/5u, /*lag=*/0u, /*render=*/0u,
                            /*drops=*/nullptr, /*n_drops=*/0u};
    return sc;
}

// The pre-shared roster the server enrolls: token -> designated seat, token order != seat order (so a
// join-order policy would NOT reproduce it). The SAME mapping is asserted client-side.
struct Credential { std::int64_t token; std::int64_t seat; };
static const Credential ROSTER[] = {{100, 2}, {200, 0}, {300, 1}};

static void enroll_roster(netinput::CredentialTable& creds) {
    for (const auto& c : ROSTER) creds.enroll(c.token, c.seat);
}

static std::vector<std::vector<std::uint8_t>> payloads_of(const session::ServerFrames& f) {
    std::vector<std::vector<std::uint8_t>> out;
    for (const auto& p : f) out.push_back(p.second);
    return out;
}

static std::vector<input001::InputCommand> filter_by_aircraft(
    const std::vector<input001::InputCommand>& all, std::int64_t aircraft) {
    std::vector<input001::InputCommand> out;
    for (const auto& c : all) if (c.aircraft == aircraft) out.push_back(c);
    return out;
}

// Run the input-driven producer in-process to exhaustion, submitting `cmds` (any order) up front.
static std::vector<std::vector<std::uint8_t>> run_input_frames(
    const Rails& rails, const session::Scenario& sc,
    const std::vector<input001::InputCommand>& cmds) {
    netinput::CommandQueue q(sc.n_aircraft);
    for (const auto& c : cmds) q.submit(c);
    netinput::InputProducer producer(rails, sc, q);
    std::vector<std::vector<std::uint8_t>> frames;
    std::int64_t emit_tick = 0;
    std::vector<std::uint8_t> payload;
    while (producer.next(emit_tick, payload)) frames.push_back(payload);
    return frames;
}

static bool frames_equal(const std::vector<std::vector<std::uint8_t>>& a,
                         const std::vector<std::vector<std::uint8_t>>& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (a[i] != b[i]) return false;
    return true;
}

static std::vector<std::uint8_t> encode_upstream(const std::vector<input001::InputCommand>& cmds) {
    std::vector<std::uint8_t> upstream;
    for (const auto& c : cmds) {
        std::vector<std::uint8_t> rec, framed;
        input001::encode_command(c, rec);
        framing::encode_frame(rec, framed);
        upstream.insert(upstream.end(), framed.begin(), framed.end());
    }
    return upstream;
}

static std::vector<std::uint8_t> hello_frame(std::int64_t token) {
    hello001::HelloInfo hi{token};
    std::vector<std::uint8_t> rec, framed;
    hello001::encode_hello(hi, rec);
    framing::encode_frame(rec, framed);
    return framed;
}

// =============================================================================================
// LEG 1 / LEG 2 harness: THREE seated identities (initial gather, cooperative; token[ci] designates
// seat) each fly their own seat; a FOURTH client joins mid-stream at frame kJoin with an UNKNOWN token
// (spectator). The server runs broadcast_auth_catchup(min_initial=3, cap=0, liveness=0, `window`) over an
// enrolled roster. If `spectator_sends_all` the joiner ALSO upstreams the whole command set (all
// unauthorized) before reading its catch-up. Returns Stats + per-early-client BIND/frames + the
// spectator's BIND/frames.
// =============================================================================================
struct CatchupResult {
    // three seated early identities (ci -> token ROSTER[ci].token -> seat ROSTER[ci].seat)
    bind001::BindInfo early_bind[3];
    bool early_bind_ok[3] = {false, false, false};
    std::vector<std::vector<std::uint8_t>> early_frames[3];
    std::size_t early_pending[3] = {1, 1, 1};
    // the mid-stream spectator joiner
    bind001::BindInfo late_bind;
    bool late_bind_ok = false;
    std::vector<std::vector<std::uint8_t>> late_frames;
    std::size_t late_pending = 1;
};

static void run_catchup_leg(const Rails& rails, const session::Scenario& sc, std::size_t kJoin,
                            std::size_t window, bool spectator_sends_all,
                            const std::vector<input001::InputCommand>& canon, netinput::Stats& stats_out,
                            CatchupResult& res, const char* label) {
    std::mutex mtx;
    std::condition_variable cv;
    std::uint16_t port_val = 0;
    bool port_ready = false;
    std::size_t early_cmds_sent = 0;   // early clients that have upstreamed their commands
    bool late_ready = false;           // server has reached frame kJoin (spectator may connect)
    bool late_connected = false;       // the spectator's connect() has returned

    std::atomic<int> done{0};
    const int expect_done = 5;  // 3 early + 1 late + server

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
        if (!netsock::is_valid(listener)) { ++done; return; }

        netinput::CommandQueue q(sc.n_aircraft);
        netinput::InputProducer producer(rails, sc, q);
        netinput::CredentialTable creds(producer.n_aircraft());
        enroll_roster(creds);
        auto on_frame = [&](std::size_t fi) {
            if (fi == 0) {  // hold frame 0 until the three seated identities' commands are all in
                std::unique_lock<std::mutex> lk(mtx);
                cv.wait(lk, [&] { return early_cmds_sent == 3; });
            }
            if (fi == kJoin) {  // hold frame kJoin until the mid-stream spectator has connected
                std::unique_lock<std::mutex> lk(mtx);
                late_ready = true;
                cv.notify_all();
                cv.wait(lk, [&] { return late_connected; });
            }
        };
        stats_out = netinput::broadcast_auth_catchup(listener, producer, q, creds, /*min_initial=*/3,
                                                     /*accept_deadline_ms=*/10000, on_frame,
                                                     /*cap_bytes=*/0, /*liveness_frames=*/0, window);
        netsock::close_socket(listener);
        ++done;
    });

    auto wait_port = [&]() -> std::uint16_t {
        std::unique_lock<std::mutex> lk(mtx);
        cv.wait(lk, [&] { return port_ready; });
        return port_val;
    };

    // three cooperative seated identities (present from the initial gather)
    std::vector<std::thread> early;
    for (std::size_t ci = 0; ci < 3; ++ci) {
        early.emplace_back([&, ci] {
            std::uint16_t port = wait_port();
            if (port == 0) { ++done; return; }
            netsock::socket_t s = netsock::connect_loopback(port);
            if (!netsock::is_valid(s)) { ++done; return; }
            // present the HELLO-001 identity claim FIRST (as one framing frame).
            {
                const std::vector<std::uint8_t> h = hello_frame(ROSTER[ci].token);
                netsock::send_all(s, h);
            }
            framing::StreamReassembler r;
            std::vector<std::vector<std::uint8_t>> all;
            std::uint8_t buf[4096];
            while (all.empty()) {  // read until the BIND handshake is complete
                std::ptrdiff_t n = netsock::recv_some(s, buf, sizeof(buf));
                if (n <= 0) break;
                if (!r.feed(buf, static_cast<std::size_t>(n), all)) break;
            }
            if (!all.empty()) {
                std::size_t pos = 0;
                res.early_bind_ok[ci] =
                    bind001::decode_bind(all[0].data(), all[0].size(), pos, res.early_bind[ci]) &&
                    pos == all[0].size();
            }
            // build this client's OWN-seat commands (scrambled/chunked), upstream them
            std::vector<input001::InputCommand> cmds =
                res.early_bind_ok[ci] ? filter_by_aircraft(canon, res.early_bind[ci].seat)
                                      : std::vector<input001::InputCommand>{};
            const bool reverse = (ci % 2 == 0);
            if (reverse) std::reverse(cmds.begin(), cmds.end());
            std::vector<std::uint8_t> up = encode_upstream(cmds);
            const std::size_t chunk = ci == 0 ? 1 : (ci == 1 ? 7 : 3);
            for (std::size_t off = 0; off < up.size(); off += chunk) {
                std::size_t take = (off + chunk <= up.size()) ? chunk : up.size() - off;
                netsock::send_all(s, up.data() + off, take);
            }
            {
                std::lock_guard<std::mutex> lk(mtx);
                ++early_cmds_sent;
            }
            cv.notify_all();
            // read the downstream snapshots to EOF
            while (true) {
                std::ptrdiff_t n = netsock::recv_some(s, buf, sizeof(buf));
                if (n <= 0) break;
                if (!r.feed(buf, static_cast<std::size_t>(n), all)) break;
            }
            res.early_pending[ci] = r.pending();
            for (std::size_t i = 1; i < all.size(); ++i) res.early_frames[ci].push_back(all[i]);
            netsock::close_socket(s);
            ++done;
        });
    }

    // the mid-stream spectator: connect only once the server is holding frame kJoin
    std::thread late([&] {
        std::uint16_t port = wait_port();
        if (port == 0) { ++done; return; }
        {
            std::unique_lock<std::mutex> lk(mtx);
            cv.wait(lk, [&] { return late_ready; });
        }
        netsock::socket_t s = netsock::connect_loopback(port);
        {
            std::lock_guard<std::mutex> lk(mtx);
            late_connected = true;  // release the server: accept us (replay the retained window)
        }
        cv.notify_all();
        if (!netsock::is_valid(s)) { ++done; return; }
        // present an UNKNOWN token first (999 -> spectator; the seats are full anyway).
        {
            const std::vector<std::uint8_t> h = hello_frame(999);
            netsock::send_all(s, h);
        }
        framing::StreamReassembler r;
        std::vector<std::vector<std::uint8_t>> all;
        std::uint8_t buf[4096];
        while (all.empty()) {  // read until the BIND record is complete
            std::ptrdiff_t n = netsock::recv_some(s, buf, sizeof(buf));
            if (n <= 0) break;
            if (!r.feed(buf, static_cast<std::size_t>(n), all)) break;
        }
        if (!all.empty()) {
            std::size_t pos = 0;
            res.late_bind_ok =
                bind001::decode_bind(all[0].data(), all[0].size(), pos, res.late_bind) &&
                pos == all[0].size();
        }
        if (spectator_sends_all) {  // a spectator authorizes nothing: every command is dropped
            std::vector<std::uint8_t> up = encode_upstream(canon);
            for (std::size_t off = 0; off < up.size(); off += 5) {
                std::size_t take = (off + 5 <= up.size()) ? 5 : up.size() - off;
                netsock::send_all(s, up.data() + off, take);
            }
        }
        while (true) {  // read the catch-up + live stream to EOF
            std::ptrdiff_t n = netsock::recv_some(s, buf, sizeof(buf));
            if (n <= 0) break;
            if (!r.feed(buf, static_cast<std::size_t>(n), all)) break;
        }
        res.late_pending = r.pending();
        for (std::size_t i = 1; i < all.size(); ++i) res.late_frames.push_back(all[i]);
        netsock::close_socket(s);
        ++done;
    });

    std::thread watchdog([&] {
        for (int i = 0; i < 400 && done.load() < expect_done; ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));  // up to ~40 s
        if (done.load() < expect_done) {
            std::printf("FAIL layer-23 auth-catchup leg [%s] TIMED OUT (socket hang)\n", label);
            std::fflush(stdout);
            std::_Exit(1);
        }
    });
    watchdog.detach();

    server.join();
    for (auto& t : early) t.join();
    late.join();
}

// ---------------------------------------------------------------------------------------------
// LEG 1: identity binding + catch-up window regimes. Three identities each fly their designated seat; a
// mid-stream spectator receives exactly frames[max(0,kJoin-W):].
// ---------------------------------------------------------------------------------------------
static int run_leg1() {
    const Rails rails = sealed_rails();
    const session::Scenario sc = ac_scenario(/*ticks=*/150u);
    const auto ref = payloads_of(session::build_server_frames(rails, sc));
    const auto canon = netinput::commands_from_scenario(sc);
    const std::size_t N = ref.size();
    const std::size_t kJoin = N / 2;

    struct Regime { const char* name; std::size_t window; };
    const Regime regimes[] = {
        {"W=1 (minimal)", 1},
        {"W=kJoin/2 (partial)", kJoin / 2},
        {"W=0 (retain-all)", 0},
    };

    int fails = 0;
    for (const Regime& reg : regimes) {
        const std::size_t W = reg.window;
        const std::size_t start = (W > 0) ? (kJoin > W ? kJoin - W : 0) : 0;   // retain-all => 0
        const std::size_t want_trimmed = (W > 0 && N > W) ? N - W : 0;

        netinput::Stats st;
        CatchupResult res;
        run_catchup_leg(rails, sc, kJoin, W, /*spectator_sends_all=*/false, canon, st, res, reg.name);

        int lf = 0;
        if (!st.ok || st.frames_sent != N || st.joins != 4 || st.leaves != 0 || st.capped != 0 ||
            st.reaped != 0 || st.trimmed != want_trimmed) {
            ++lf;
            std::printf("FAIL leg1 [%s]: server stats (ok=%d sent=%zu/%zu joins=%zu leaves=%zu capped=%zu "
                        "reaped=%zu trimmed=%zu want_trimmed=%zu)\n", reg.name, st.ok ? 1 : 0,
                        st.frames_sent, N, st.joins, st.leaves, st.capped, st.reaped, st.trimmed,
                        want_trimmed);
        }
        if (st.cmds_ok != canon.size() || st.cmds_unauth != 0 || st.cmds_stale != 0 || st.cmds_oob != 0) {
            ++lf;
            std::printf("FAIL leg1 [%s]: commands (ok=%zu unauth=%zu stale=%zu oob=%zu; want %zu/0/0/0)\n",
                        reg.name, st.cmds_ok, st.cmds_unauth, st.cmds_stale, st.cmds_oob, canon.size());
        }
        // the three seated identities each drew EXACTLY their designated seat and got the WHOLE stream
        // under EVERY window.
        for (std::size_t ci = 0; ci < 3; ++ci) {
            const auto& b = res.early_bind[ci];
            const std::int64_t want_seat = ROSTER[ci].seat;
            if (!res.early_bind_ok[ci] || b.n_aircraft != 3 || b.seat != want_seat) {
                ++lf;
                std::printf("FAIL leg1 [%s]: identity %zu (token %lld) bound to seat %lld, want %lld\n",
                            reg.name, ci, static_cast<long long>(ROSTER[ci].token),
                            static_cast<long long>(b.seat), static_cast<long long>(want_seat));
            }
            if (res.early_pending[ci] != 0 || !frames_equal(res.early_frames[ci], ref)) {
                ++lf;
                std::printf("FAIL leg1 [%s]: seated identity %zu not byte-identical to build_server_frames "
                            "(got %zu of %zu, pending %zu)\n", reg.name, ci, res.early_frames[ci].size(),
                            N, res.early_pending[ci]);
            }
        }
        // the spectator: a mid-stream SPECTATOR bind, and exactly frames[start:].
        if (!res.late_bind_ok || res.late_bind.seat != bind001::SPECTATOR || res.late_bind.n_aircraft != 3) {
            ++lf;
            std::printf("FAIL leg1 [%s]: spectator BIND (ok=%d seat=%lld n=%lld; want seat -1 / n 3)\n",
                        reg.name, res.late_bind_ok ? 1 : 0, static_cast<long long>(res.late_bind.seat),
                        static_cast<long long>(res.late_bind.n_aircraft));
        }
        bool suffix_ok = res.late_pending == 0 && res.late_frames.size() == N - start;
        for (std::size_t i = 0; suffix_ok && i < res.late_frames.size(); ++i)
            suffix_ok = res.late_frames[i] == ref[start + i];
        if (!suffix_ok) {
            ++lf;
            std::printf("FAIL leg1 [%s]: spectator did not receive exactly frames[%zu:] (got %zu of %zu, "
                        "pending %zu)\n", reg.name, start, res.late_frames.size(), N - start,
                        res.late_pending);
        }
        if (lf == 0)
            std::printf("  LEG1 [%s] PASS: 3 identities each flew the seat their TOKEN designates "
                        "(100->2, 200->0, 300->1) byte-identical to build_server_frames; spectator caught "
                        "up EXACTLY frames[%zu:] (%zu frames), trimmed=%zu\n", reg.name, start, N - start,
                        want_trimmed);
        fails += lf;
    }
    return fails;
}

// ---------------------------------------------------------------------------------------------
// LEG 2: authentication + authorization compose with catch-up. The mid-stream spectator upstreams the
// WHOLE command set; all are rejected (cmds_unauth), the produced stream is unchanged, and it still
// catches up the whole stream.
// ---------------------------------------------------------------------------------------------
static int run_leg2() {
    const Rails rails = sealed_rails();
    const session::Scenario sc = ac_scenario(/*ticks=*/150u);
    const auto ref = payloads_of(session::build_server_frames(rails, sc));
    const auto canon = netinput::commands_from_scenario(sc);
    const std::size_t N = ref.size();
    const std::size_t kJoin = N / 2;

    netinput::Stats st;
    CatchupResult res;
    run_catchup_leg(rails, sc, kJoin, /*window=*/0, /*spectator_sends_all=*/true, canon, st, res,
                    "leg2/spectator-foreign-reject");

    int fails = 0;
    if (!st.ok || st.frames_sent != N || st.joins != 4 || st.leaves != 0 || st.capped != 0 ||
        st.reaped != 0 || st.trimmed != 0) {
        ++fails;
        std::printf("FAIL leg2: server stats (ok=%d sent=%zu/%zu joins=%zu leaves=%zu capped=%zu "
                    "reaped=%zu trimmed=%zu)\n", st.ok ? 1 : 0, st.frames_sent, N, st.joins, st.leaves,
                    st.capped, st.reaped, st.trimmed);
    }
    // the 3 seated identities authorized the whole set; the spectator's whole set was rejected.
    if (st.cmds_ok != canon.size() || st.cmds_unauth != canon.size() || st.cmds_stale != 0 ||
        st.cmds_oob != 0) {
        ++fails;
        std::printf("FAIL leg2: commands (ok=%zu unauth=%zu; want %zu authorized / %zu spectator-rejected)\n",
                    st.cmds_ok, st.cmds_unauth, canon.size(), canon.size());
    }
    // the produced stream is unchanged: seated identities + spectator all see the whole build_server_frames.
    for (std::size_t ci = 0; ci < 3; ++ci) {
        if (res.early_pending[ci] != 0 || !frames_equal(res.early_frames[ci], ref)) {
            ++fails;
            std::printf("FAIL leg2: seated identity %zu stream changed by spectator's rejected commands "
                        "(got %zu of %zu)\n", ci, res.early_frames[ci].size(), N);
        }
    }
    if (!res.late_bind_ok || res.late_bind.seat != bind001::SPECTATOR) {
        ++fails;
        std::printf("FAIL leg2: spectator BIND (ok=%d seat=%lld; want -1)\n", res.late_bind_ok ? 1 : 0,
                    static_cast<long long>(res.late_bind.seat));
    }
    if (res.late_pending != 0 || !frames_equal(res.late_frames, ref)) {
        ++fails;
        std::printf("FAIL leg2: spectator did not catch up the whole stream (got %zu of %zu, pending %zu)\n",
                    res.late_frames.size(), N, res.late_pending);
    }
    if (fails == 0)
        std::printf("  LEG2 PASS: the unknown-token spectator's %zu commands were ALL rejected "
                    "(cmds_unauth=%zu); the produced stream was unchanged and the spectator still caught up "
                    "the whole %zu-frame stream byte-for-byte\n", canon.size(), st.cmds_unauth, N);
    return fails;
}

// =============================================================================================
// LEG 3: hygiene composes with identity + catch-up. On a LONG stream through a pinned tiny kernel send
// buffer, a seated FAST identity (token 200 -> seat 0, hook-drained) is byte-identical to its seat's
// reference; a NON-READING authenticated DEAD identity (token 300 -> seat 1) joining mid-stream has its
// catch-up REPLAY backlog shed by the byte-cap DURING replay (capped) + its seat freed. DEAD's bytes are
// [BIND(seat 1) | strict prefix]; FAST untouched. Because seats are bound by IDENTITY, DEAD is
// deterministically seat 1 (unlike layer 20's dynamic real seat).
// =============================================================================================
static int run_leg3() {
    const Rails rails = sealed_rails();
    const session::Scenario sc = ac_scenario(/*ticks=*/20000u);  // ~4001 frames (a big retained prefix)
    const auto canon = netinput::commands_from_scenario(sc);
    const auto own0 = filter_by_aircraft(canon, 0);
    const auto ref = run_input_frames(rails, sc, own0);          // FAST is seat 0: aircraft-0-only world
    const std::size_t N = ref.size();
    const std::size_t kJoin = N / 2;
    const std::size_t n_own = own0.size();

    const int kBufBytes = 16 * 1024;         // tiny kernel send buffer: a non-reading peer stalls quickly
    const std::size_t kCap = 128 * 1024;     // shed a catch-up backlog past 128 KiB (< the retained prefix)

    std::mutex mtx;
    std::condition_variable cv;
    std::uint16_t port_val = 0;
    bool port_ready = false;
    bool server_done = false;
    bool fast_cmds_sent = false;
    bool dead_ready = false;       // server reached kJoin (DEAD may connect)
    bool dead_connected = false;
    netsock::socket_t fast_sock = netsock::invalid_socket();
    std::vector<std::uint8_t> fast_bytes;

    netinput::Stats stats;
    std::atomic<int> done{0};

    std::thread server([&] {
        std::uint16_t port = 0;
        netsock::socket_t listener = netsock::listen_loopback(0, port, /*backlog=*/3);
        if (netsock::is_valid(listener)) {
            netsock::set_sndbuf(listener, kBufBytes);
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
        netinput::CredentialTable creds(producer.n_aircraft());
        enroll_roster(creds);
        auto on_frame = [&](std::size_t fi) {
            if (fi == 0) {
                std::unique_lock<std::mutex> lk(mtx);
                cv.wait(lk, [&] { return fast_cmds_sent; });
            }
            if (fi == kJoin) {
                std::unique_lock<std::mutex> lk(mtx);
                dead_ready = true;
                cv.notify_all();
                cv.wait(lk, [&] { return dead_connected; });
            }
            // drain FAST every frame so it never backs up (it is the cooperative client).
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

        // min_initial=1: only FAST is present at the initial gather; DEAD joins mid-stream at kJoin.
        stats = netinput::broadcast_auth_catchup(listener, producer, q, creds, /*min_initial=*/1,
                                                 /*accept_deadline_ms=*/10000, on_frame, kCap,
                                                 /*liveness_frames=*/0, /*catchup_window=*/0);
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

    // DEAD: present token 300 (seat 1), connect at kJoin, read NOTHING until the broadcast returns (shed
    // long before), then drain its kernel-buffered prefix (BIND + a frame prefix) to EOF.
    std::vector<std::uint8_t> dead_bytes;
    std::thread dead([&] {
        std::uint16_t port = wait_port();
        if (port == 0) { ++done; return; }
        {
            std::unique_lock<std::mutex> lk(mtx);
            cv.wait(lk, [&] { return dead_ready; });
        }
        netsock::socket_t s = netsock::connect_loopback(port);
        {
            std::lock_guard<std::mutex> lk(mtx);
            dead_connected = true;
        }
        cv.notify_all();
        if (netsock::is_valid(s)) {
            const std::vector<std::uint8_t> h = hello_frame(300);  // valid identity -> seat 1
            netsock::send_all(s, h);
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
        for (int i = 0; i < 600 && done.load() < 3; ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));  // up to ~60 s
        if (done.load() < 3) {
            std::printf("FAIL layer-23 hygiene leg TIMED OUT (wedge)\n");
            std::fflush(stdout);
            std::_Exit(1);
        }
    });
    watchdog.detach();

    // FAST connects on the main thread, presents token 200 (seat 0), reads its BIND, upstreams ONLY its
    // own seat's commands.
    bind001::BindInfo fast_bind;
    bool fast_bind_ok = false;
    framing::StreamReassembler fast_rx;
    std::vector<std::vector<std::uint8_t>> fast_all;
    {
        std::uint16_t port = wait_port();
        if (port != 0) {
            netsock::socket_t s = netsock::connect_loopback(port);
            if (netsock::is_valid(s)) {
                const std::vector<std::uint8_t> h = hello_frame(200);  // valid identity -> seat 0
                netsock::send_all(s, h);
                std::uint8_t buf[4096];
                while (fast_all.empty()) {
                    std::ptrdiff_t n = netsock::recv_some(s, buf, sizeof(buf));
                    if (n <= 0) break;
                    if (!fast_rx.feed(buf, static_cast<std::size_t>(n), fast_all)) break;
                }
                if (!fast_all.empty()) {
                    std::size_t pos = 0;
                    fast_bind_ok = bind001::decode_bind(fast_all[0].data(), fast_all[0].size(), pos,
                                                        fast_bind) && pos == fast_all[0].size();
                }
                // send scrambled (reversed) — order-invariance still holds under hygiene
                std::vector<input001::InputCommand> rev(own0.rbegin(), own0.rend());
                const std::vector<std::uint8_t> up = encode_upstream(rev);
                for (std::size_t off = 0; off < up.size(); off += 32) {
                    std::size_t take = (off + 32 <= up.size()) ? 32 : up.size() - off;
                    netsock::send_all(s, up.data() + off, take);
                }
                {
                    std::lock_guard<std::mutex> lk(mtx);
                    fast_sock = s;
                    fast_cmds_sent = true;
                }
                cv.notify_all();
            }
        }
    }

    server.join();
    dead.join();

    std::size_t fast_pending = 0;
    if (netsock::is_valid(fast_sock)) {
        std::uint8_t buf[4096];
        while (true) {
            std::ptrdiff_t n = netsock::recv_some(fast_sock, buf, sizeof(buf));
            if (n <= 0) break;
            fast_bytes.insert(fast_bytes.end(), buf, buf + n);
        }
        netsock::close_socket(fast_sock);
        framing::StreamReassembler r;
        r.feed(fast_bytes.data(), fast_bytes.size(), fast_all);
        fast_pending = r.pending();
    }
    std::vector<std::vector<std::uint8_t>> fast_frames;
    for (std::size_t i = 1; i < fast_all.size(); ++i) fast_frames.push_back(fast_all[i]);

    int fails = 0;
    if (!stats.ok || stats.frames_sent != N) {
        ++fails;
        std::printf("FAIL leg3: server did not finish the stream (ok=%d sent=%zu/%zu)\n", stats.ok ? 1 : 0,
                    stats.frames_sent, N);
    }
    if (!fast_bind_ok || fast_bind.seat != 0 || fast_bind.n_aircraft != 3) {
        ++fails;
        std::printf("FAIL leg3: FAST BIND (ok=%d seat=%lld n=%lld; want seat 0 by identity)\n",
                    fast_bind_ok ? 1 : 0, static_cast<long long>(fast_bind.seat),
                    static_cast<long long>(fast_bind.n_aircraft));
    }
    if (stats.cmds_ok != n_own || stats.cmds_stale != 0 || stats.cmds_oob != 0 ||
        stats.cmds_unauth != 0) {
        ++fails;
        std::printf("FAIL leg3: FAST's commands (ok=%zu stale=%zu oob=%zu unauth=%zu; want %zu/0/0/0)\n",
                    stats.cmds_ok, stats.cmds_stale, stats.cmds_oob, stats.cmds_unauth, n_own);
    }
    // DEAD is bounded by the byte-cap (capped==1, reaped==0). Whether its catch-up backlog trips the cap
    // DURING accept_all's replay (never a live member: joins stays 1, no leave) or just AFTER it joins
    // (joins 2, then a capped leave) depends on the OS's loopback socket buffering — both are the same
    // policy outcome. Platform-invariant: exactly one cap shed, no reap, leaves == joins - 1, joins in
    // {1,2}. FAST (seat 0, the sole survivor) never leaves.
    if (stats.capped != 1 || stats.reaped != 0 || stats.leaves != stats.joins - 1 ||
        stats.joins < 1 || stats.joins > 2) {
        ++fails;
        std::printf("FAIL leg3: expected DEAD shed by the byte-cap (joins=%zu leaves=%zu capped=%zu "
                    "reaped=%zu; want capped 1 / reaped 0 / leaves==joins-1 / joins in {1,2})\n",
                    stats.joins, stats.leaves, stats.capped, stats.reaped);
    }
    if (fast_pending != 0 || !frames_equal(fast_frames, ref)) {
        ++fails;
        std::printf("FAIL leg3: FAST's stream (after BIND) not byte-identical to the aircraft-0-only "
                    "reference (got %zu of %zu, pending %zu)\n", fast_frames.size(), ref.size(),
                    fast_pending);
    }
    // DEAD's delivery: [BIND(seat 1) | strict prefix of the ACTUAL produced stream]. Only FAST's seat (0)
    // is driven, so the produced stream — and hence DEAD's catch-up prefix — is FAST's seat world (`ref`).
    // DEAD draws seat 1 by identity (token 300); it is shed, so its frames are strictly short.
    {
        framing::StreamReassembler r;
        std::vector<std::vector<std::uint8_t>> dead_recs;
        bool ok = r.feed(dead_bytes.data(), dead_bytes.size(), dead_recs);
        bool dead_bind_ok = false;
        if (ok && !dead_recs.empty()) {
            std::size_t pos = 0;
            bind001::BindInfo bd;
            dead_bind_ok = bind001::decode_bind(dead_recs[0].data(), dead_recs[0].size(), pos, bd) &&
                           pos == dead_recs[0].size() && bd.n_aircraft == 3 && bd.seat == 1;
        }
        const std::size_t got_frames = dead_recs.empty() ? 0 : dead_recs.size() - 1;
        bool prefix = dead_bind_ok && got_frames < ref.size();  // strictly short => shed
        for (std::size_t i = 0; prefix && i < got_frames; ++i)
            if (dead_recs[i + 1] != ref[i]) prefix = false;
        if (!prefix) {
            ++fails;
            std::printf("FAIL leg3: DEAD's delivery is not [BIND(seat 1) | strict prefix] "
                        "(bind_ok=%d frames=%zu/%zu)\n", dead_bind_ok ? 1 : 0, got_frames, ref.size());
        }
    }
    if (fails == 0)
        std::printf("  LEG3 PASS: DEAD (token 300 -> seat 1) shed by the byte-cap (capped=1) mid catch-up + "
                    "its seat freed; FAST (token 200 -> seat 0) byte-identical to the aircraft-0-only "
                    "reference + its %zu commands drove the sim; DEAD got [BIND(seat 1) | strict prefix]\n",
                    n_own);
    return fails;
}

int main() {
    netsock::WsaGuard wsa;
    int fails = 0;
    fails += run_leg1();
    fails += run_leg2();
    fails += run_leg3();
    if (fails == 0) {
        std::printf("PASS: layer-23 authenticated + async + catch-up server — the layer-21 IDENTITY binding "
                    "(HELLO-001 + CredentialTable + BIND-001 + authorization) + the layer-16/19 "
                    "async/byte-cap/liveness hygiene + layer-20 windowed late-join CATCH-UP all compose: "
                    "authenticated clients each fly their designated aircraft to build_server_frames "
                    "byte-for-byte, a mid-stream joiner catches up an exact window suffix, an unknown-token "
                    "spectator's commands are all rejected yet it still catches up, and a dead authenticated "
                    "client's replay backlog is shed by the byte-cap + its seat freed\n");
        return 0;
    }
    std::printf("RESULT: layer-23 auth-catchup bridge FAIL (%d mismatches)\n", fails);
    return 1;
}
