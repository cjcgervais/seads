// SEADS PREDICTIVE-INPUT-CLIENT determinism BRIDGE (netcode LAYER 17) — closing the round-trip loop.
//
// Layers 5-15a are server->client. Layer 15b/16 opened the UPSTREAM path (a client's tick-stamped
// INPUT-001 commands drive the authoritative sealed kernel). Layer 17 is the missing half: the client
// predicts its OWN aircraft LOCALLY from the very commands it upstreams, and reconciles against the
// authoritative frames when they come back — so control is instant and the correction is invisible when
// the loop is lossless and the input arrived in time. This is the layer-4b Predictor (predict.h) driven
// against the layer-15b/16 authoritative input server (src/net/inputclient). The bridge proves three
// claims, mirroring tools/inputpredict_ref.py bit-for-bit (canonical + wire digests pinned below):
//
//   LEG 1 (SEAMLESS round-trip, in-process): a client that predicts its own P-47D from the SAME
//   INPUT-SK-001 command timeline it upstreams, reconciling against the CANONICAL authoritative own
//   state each frame, is in sync EVERY tick — the reconcile is a zero-correction no-op (its local sim
//   IS the server's, offset only by latency). Its per-tick own-ship hash sequence == the reference
//   canonical digest.
//
//   LEG 2 (BOUNDED round-trip, over a real 127.0.0.1 socket): the whole scenario's commands are sent UP
//   in a SCRAMBLED order + adversarial CHUNKING through broadcast_input; the resulting authoritative
//   frames stream back DOWN (byte-identical to session::build_server_frames — the layer-15b invariant),
//   and the client reconciles its own ship against the DECODED, lossy protocol-7 frames. The
//   reconstruction stays within a few wire quanta of the authoritative trajectory (bounded, the real
//   remote/late-join path) and its own-ship hash sequence == the reference wire digest.
//
//   LEG 3 (HEAL under a stale-dropped command, in-process): the client applies a command LOCALLY that
//   the authoritative server DROPPED as STALE (arrived after its apply_tick was stepped). It mispredicts
//   from that tick, and is HEALED exactly at the first authoritative frame whose snapshot clears the bad
//   input; a no-reconcile control stays broken forever (the reconcile, not luck, heals).
//
// Every assertion is on det_math kernel snapshots / delivered bytes; the socket rendezvous is
// cv+notify_all (no sleeps), and a finite watchdog fails (never wedges) on a socket hang. Exit 0 PASS.
//
// TRANSPORT / CLIENT ONLY: no kernel/det_math/rails/wire/golden touched — composes the EXISTING
// protocol-7 wire + the sealed layer-4b predictor. Rides seal v1.26r0 (a no-seal integration rung).
#include "inputclient.h"
#include "inputserver.h"
#include "input001.h"
#include "cmdqueue.h"
#include "session.h"
#include "session_vectors.h"
#include "predict.h"
#include "framing.h"
#include "socket.h"
#include "snapshot.h"
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
#include <string>
#include <thread>
#include <vector>

using namespace seads;

// Reference digests pinned by tools/inputpredict_ref.py (INPUT-SK-001, 150 ticks, snap/5, lag 10).
static const char* PIN_CANONICAL =
    "abecf117812d1a9037c774c3746c80f92ccfd542ca2d934651672a092cec1b72";
static const char* PIN_WIRE =
    "007c4b9d6cbde8be87a25c6e12d950e6221b13cc3b0c7bc8b4d76bc729645f19";
static const unsigned CLIENT_LAG = 10;  // ~100 ms; a multiple of snap_every so reconcile lands on emits

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
// INPUT-SK-001 — the SAME grid-exact scenario as the layer-15b bridge (dyadic radian command values so
// the INPUT-001 wire round-trips bit-for-bit and the frames match build_server_frames). Own = AC0 (the
// P-47D). Reuses the three SESSION-SK-001 envelopes + start states.
// ---------------------------------------------------------------------------------------------
static const session::Phase IN_P0[] = {
    {0u,   0.0,  1.0, 0.75, true},
    {50u,  0.5,  1.5, 1.0,  false},
    {120u, 0.0,  1.0, 0.75, false},
};
static const session::Phase IN_P1[] = {
    {0u,  -0.5,  1.0, 0.5,  false},
    {80u,  0.25, 2.0, 1.0,  true},
};
static const session::Phase IN_P2[] = {
    {0u,   0.5,  1.5, 1.0,  false},
};

static session::AircraftSpec IN_AC[3];
static const session::Scenario& input_scenario() {
    static bool built = [] {
        for (int i = 0; i < 3; ++i) IN_AC[i] = sess_vec::AIRCRAFT[i];
        IN_AC[0].sched = IN_P0; IN_AC[0].n_phase = 3;
        IN_AC[1].sched = IN_P1; IN_AC[1].n_phase = 2;
        IN_AC[2].sched = IN_P2; IN_AC[2].n_phase = 1;
        return true;
    }();
    (void)built;
    static const session::Scenario sc = {
        IN_AC, 3u, /*ticks=*/150u, /*snap_every=*/5u, /*lag=*/0u, /*render=*/0u,
        /*drops=*/nullptr, /*n_drops=*/0u};
    return sc;
}

// Integer phase select (mirrors session::phase_at).
static const session::Phase& phase_at(const session::AircraftSpec& a, unsigned t) {
    unsigned idx = 0;
    for (unsigned j = 0; j < a.n_phase; ++j) {
        if (a.sched[j].start_tick <= t) idx = j; else break;
    }
    return a.sched[idx];
}

// The own ship's per-tick KINEMATIC command timeline (local_cmds[t-1] applied stepping t-1 -> t).
static std::vector<Command> own_timeline(const session::AircraftSpec& own, unsigned ticks) {
    std::vector<Command> cmds;
    cmds.reserve(ticks);
    for (unsigned t = 0; t < ticks; ++t) {
        const session::Phase& p = phase_at(own, t);
        cmds.push_back(Command{p.target_phi, p.target_g, p.throttle, false});
    }
    return cmds;
}

static predict::OwnState own_start(const session::AircraftSpec& a) {
    return {a.lat, a.lon, a.psi, a.phi, a.alt, a.tas, 0.0};
}

// ---------------------------------------------------------------------------------------------
// LEG 1: SEAMLESS canonical round-trip (in-process). Predict own(0) from its own timeline, reconcile
// against the CANONICAL authoritative own states each frame -> in sync every tick, digest == pin.
// ---------------------------------------------------------------------------------------------
static int leg_seamless(const Rails& rails, const session::Scenario& sc) {
    const session::AircraftSpec& own = sc.aircraft[session::OWN_ID];
    const auto frames = session::build_server_frames(rails, sc);
    const auto canonical = netpredict::authoritative_own_states(rails, sc);
    const auto local = own_timeline(own, sc.ticks);

    netpredict::ClientResult r = netpredict::run_predictive_client(
        rails, own.env, own_start(own), local, frames, CLIENT_LAG, /*drops=*/{}, /*reconcile=*/true,
        netpredict::Source::CANONICAL, canonical);

    int fails = 0;
    if (!r.in_sync || r.first_divergent != -1 || r.max_pos_err != 0.0) {
        ++fails;
        std::printf("FAIL LEG1: canonical reconcile not seamless (in_sync=%d first_div=%ld err=%.3e)\n",
                    r.in_sync ? 1 : 0, r.first_divergent, r.max_pos_err);
    }
    if (r.digest != PIN_CANONICAL) {
        ++fails;
        std::printf("FAIL LEG1: canonical digest %s != pin %s\n", r.digest.c_str(), PIN_CANONICAL);
    }
    if (fails == 0)
        std::printf("  LEG1 PASS: predict-from-upstreamed-commands reconciles SEAMLESSLY vs canonical "
                    "(%u frames, in sync all %u ticks, zero correction) -> digest %s\n",
                    r.reconciles, sc.ticks, r.digest.c_str());
    return fails;
}

// ---------------------------------------------------------------------------------------------
// LEG 2: BOUNDED round-trip over a real socket. Scrambled upstream through broadcast_input -> the frames
// stream back (byte-identical to build_server_frames) -> the client reconciles own(0) against the
// decoded lossy wire -> bounded error + digest == pin.
// ---------------------------------------------------------------------------------------------
static int leg_socket(const Rails& rails, const session::Scenario& sc) {
    const session::AircraftSpec& own = sc.aircraft[session::OWN_ID];
    const auto ref = session::build_server_frames(rails, sc);
    // scrambled + finely-chunked upstream: whole command set, reversed, 1 byte per send
    auto canon = netinput::commands_from_scenario(sc);
    std::vector<input001::InputCommand> cmds(canon.rbegin(), canon.rend());
    std::vector<std::uint8_t> upstream;
    for (const auto& c : cmds) {
        std::vector<std::uint8_t> rec, framed;
        input001::encode_command(c, rec);
        framing::encode_frame(rec, framed);
        upstream.insert(upstream.end(), framed.begin(), framed.end());
    }

    std::mutex mtx;
    std::condition_variable cv;
    std::uint16_t port_val = 0;
    bool port_ready = false, commands_sent = false;
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
        stats = netinput::broadcast_input(listener, producer, q, /*min_initial=*/1,
                                          /*accept_deadline_ms=*/10000, on_frame);
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
            for (std::size_t off = 0; off < upstream.size(); off += 1)
                netsock::send_all(s, upstream.data() + off, 1);
            {
                std::lock_guard<std::mutex> lk(mtx);
                commands_sent = true;
            }
            cv.notify_all();
            framing::StreamReassembler r;
            std::uint8_t buf[4096];
            while (true) {
                std::ptrdiff_t n = netsock::recv_some(s, buf, sizeof(buf));
                if (n > 0) { if (!r.feed(buf, (std::size_t)n, got)) break; } else break;
            }
            pending = r.pending();
            netsock::close_socket(s);
        }
        ++done;
    });

    std::thread watchdog([&] {
        for (int i = 0; i < 400 && done.load() < 2; ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (done.load() < 2) {
            std::printf("FAIL LEG2 socket TIMED OUT (socket hang)\n");
            std::fflush(stdout);
            std::_Exit(1);
        }
    });
    watchdog.detach();
    server.join();
    client.join();

    int fails = 0;
    if (!stats.ok || pending != 0 || got.size() != ref.size()) {
        ++fails;
        std::printf("FAIL LEG2: transport (ok=%d got=%zu/%zu pending=%zu)\n", stats.ok ? 1 : 0,
                    got.size(), ref.size(), pending);
        return fails;
    }
    // frames byte-identical to build_server_frames (the layer-15b invariant), rebuilt keyed on server_tick
    session::ServerFrames delivered;
    for (std::size_t i = 0; i < got.size(); ++i) {
        if (got[i] != ref[i].second) { ++fails; }
        netsnap::Snapshot dec;
        std::size_t pos = 0;
        if (!netsnap::decode_snapshot(got[i].data(), got[i].size(), pos, dec)) { ++fails; break; }
        delivered.emplace_back(dec.server_tick, got[i]);
    }
    if (fails) {
        std::printf("FAIL LEG2: downstream frames NOT byte-identical to build_server_frames\n");
        return fails;
    }

    const auto canonical = netpredict::authoritative_own_states(rails, sc);
    const auto local = own_timeline(own, sc.ticks);
    netpredict::ClientResult r = netpredict::run_predictive_client(
        rails, own.env, own_start(own), local, delivered, CLIENT_LAG, /*drops=*/{}, /*reconcile=*/true,
        netpredict::Source::WIRE, canonical);

    if (r.delivered == 0 || r.max_pos_err > 1e-3) {
        ++fails;
        std::printf("FAIL LEG2: wire reconcile unbounded (delivered=%u max_pos_err=%.3e)\n",
                    r.delivered, r.max_pos_err);
    }
    if (r.digest != PIN_WIRE) {
        ++fails;
        std::printf("FAIL LEG2: wire digest %s != pin %s\n", r.digest.c_str(), PIN_WIRE);
    }
    if (fails == 0)
        std::printf("  LEG2 PASS: %zu commands UP scrambled (1 B/send) -> %zu frames byte-identical to "
                    "build_server_frames; own ship reconciled vs lossy wire within %.2e rad -> digest %s\n",
                    cmds.size(), ref.size(), r.max_pos_err, r.digest.c_str());
    return fails;
}

// ---------------------------------------------------------------------------------------------
// LEG 3: HEAL under a stale-dropped command (in-process). The client banks at tick 30 locally, but that
// command was dropped STALE by the server (hold-last), so the client mispredicts a 20-tick window and
// is healed only once the authoritative snapshot advances past it. A no-reconcile control never heals.
// ---------------------------------------------------------------------------------------------
static int leg_heal(const Rails& rails, const session::Scenario& sc) {
    const session::AircraftSpec& own = sc.aircraft[session::OWN_ID];
    const auto frames = session::build_server_frames(rails, sc);
    const auto canonical = netpredict::authoritative_own_states(rails, sc);

    // client's LOCAL timeline == the truth timeline but banking 0.5 rad from tick 30 (the stale command)
    std::vector<Command> local = own_timeline(own, sc.ticks);
    for (unsigned t = 30; t < 50; ++t) local[t] = Command{0.5, 1.5, 1.0, false};  // held to truth's tick-50 phase

    netpredict::ClientResult healed = netpredict::run_predictive_client(
        rails, own.env, own_start(own), local, frames, CLIENT_LAG, /*drops=*/{}, /*reconcile=*/true,
        netpredict::Source::CANONICAL, canonical);
    netpredict::ClientResult broken = netpredict::run_predictive_client(
        rails, own.env, own_start(own), local, frames, CLIENT_LAG, /*drops=*/{}, /*reconcile=*/false,
        netpredict::Source::CANONICAL, canonical);

    int fails = 0;
    if (healed.first_divergent != 31 || healed.heal_tick != 60 || healed.in_sync) {
        ++fails;
        std::printf("FAIL LEG3: stale-drop heal (first_div=%ld heal=%ld in_sync=%d; want 31/60/0)\n",
                    healed.first_divergent, healed.heal_tick, healed.in_sync ? 1 : 0);
    }
    if (broken.in_sync || broken.heal_tick != -1 || broken.first_divergent != 31) {
        ++fails;
        std::printf("FAIL LEG3: no-reconcile control healed (first_div=%ld heal=%ld in_sync=%d)\n",
                    broken.first_divergent, broken.heal_tick, broken.in_sync ? 1 : 0);
    }
    if (fails == 0)
        std::printf("  LEG3 PASS: stale-dropped command mispredicts ticks 31..59, HEALED at tick 60 by "
                    "the authoritative frame; no-reconcile control stays broken\n");
    return fails;
}

int main() {
    netsock::WsaGuard wsa;
    const Rails rails = sealed_rails();
    const session::Scenario& sc = input_scenario();
    int fails = 0;
    fails += leg_seamless(rails, sc);
    fails += leg_socket(rails, sc);
    fails += leg_heal(rails, sc);
    if (fails == 0) {
        std::printf("PASS: layer-17 predictive input client — the round-trip loop is SEAMLESS when the "
                    "client predicts its upstreamed commands, BOUNDED over the lossy wire, and HEALS a "
                    "stale-dropped misprediction\n");
        return 0;
    }
    std::printf("RESULT: layer-17 predict bridge FAIL (%d mismatches)\n", fails);
    return 1;
}
