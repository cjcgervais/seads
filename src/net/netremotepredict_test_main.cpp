// SEADS REMOTE-AIRCRAFT-PREDICTION determinism BRIDGE (netcode LAYER 24) — predict OTHERS, not just
// interpolate them.
//
// Layer 4b/17 predict the OWN aircraft (replay the LOCAL commands the client upstreams). Layer 4a
// renders REMOTE aircraft by INTERPOLATION — ~100 ms in the PAST, smooth but structurally LATE.
// Layer 24 closes the gap: predict the REMOTE aircraft to "now" by DEAD-RECKONING COAST. A client
// has no remote's input commands (authorization: only its own seat's), only the remote's KINEMATIC
// state on the wire — so it seeds a one-aircraft kernel from the freshest authoritative snapshot and
// advances it with the SEALED no-arg Kernel::step() (the pure kinematic tail), reseeding when a
// fresher snapshot arrives. This bridge proves three claims, mirroring tools/remotepredict_ref.py
// bit-for-bit (canonical + wire digests pinned below):
//
//   LEG 1 (COAST TRACKS "NOW" and BEATS INTERPOLATION, in-process): over the steady cruise window
//   the coast's position error vs the true "now" is a tiny fraction of the layer-4a interpolation
//   baseline's structural render-lag error. The coasted remote's per-tick hash sequence == the
//   reference canonical digest (reproducible).
//
//   LEG 2 (RECONCILE IS LOAD-BEARING, in-process): with reconcile the coast's now-error stays
//   BOUNDED across the maneuver (the hard banked break at tick 90); a no-reconcile control
//   (spawn-seeded coast, never corrected) drifts without bound. The reconcile, not luck, keeps the
//   remote tracked — the remote analogue of layer 17's heal.
//
//   LEG 3 (BOUNDED over a real 127.0.0.1 socket): the remote's commands are upstreamed SCRAMBLED +
//   finely CHUNKED through broadcast_input; the authoritative frames stream back DOWN byte-identical
//   to session::build_server_frames (the layer-15b invariant), and the observing coast reseeds
//   against the DECODED, lossy protocol-7 frames — staying within a small bound of the true "now",
//   its hash sequence == the reference wire digest. (Upstreaming here just DRIVES the authoritative
//   server on a real socket; the remote-prediction logic under test is strictly downstream.)
//
// Every assertion is on det_math kernel snapshots / delivered bytes; the socket rendezvous is
// cv+notify_all (no sleeps), and a finite watchdog fails (never wedges) on a socket hang. Exit 0 PASS.
//
// TRANSPORT / CLIENT ONLY: no kernel/det_math/rails/wire/golden touched — composes the EXISTING
// protocol-7 wire + the sealed no-arg kinematic tail. Rides seal v1.26r0 (a no-seal integration rung).
#include "remotepredict.h"
#include "inputclient.h"     // netpredict::authoritative_own_states (the remote's authoritative trajectory)
#include "inputserver.h"
#include "input001.h"
#include "cmdqueue.h"
#include "session.h"
#include "predict.h"
#include "interp.h"
#include "framing.h"
#include "socket.h"
#include "snapshot.h"
#include "envelope_tables.h"
#include "golden_params.h"
#include "kernel.h"

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

// Reference digests pinned by tools/remotepredict_ref.py (REMOTE-SK-001, 200 ticks, snap/5, lag 10).
static const char* PIN_CANONICAL =
    "d28979c30e3c694fae0792d697cc7d3be79d9b965e342eb9e52030fefb6ab5e2";
static const char* PIN_WIRE =
    "7468a4edacacddbfd13990646fb734271ca60c3a1c5a7ebf259b2b4d84fb2879";
// Layer-25 SMOOTHED display digests over SMOOTH-SK-001 (200 ticks, snap/20, lag 20, smooth 0.25).
static const char* PIN_SMOOTH_CANON =
    "8fa8148481bf91e090c34f827c5c12d12a564c582771cf48c4cf22c8a1075e54";
static const char* PIN_SMOOTH_WIRE =
    "67db5c51a378e956de706a169b501d174ce07d990da3c60e9fd9bc774f0c1af3";
static const double SMOOTH_FACTOR = 0.25;   // == remotepredict_ref.SMOOTH_FACTOR

static const unsigned CLIENT_LAG = 10;      // ~100 ms; a multiple of snap_every -> reseed lands on emits
static const unsigned RENDER_DELAY = 15;    // layer-4a interp render delay (lag + one frame)
static const unsigned STEADY_LO = 40, STEADY_HI = 85;  // steady cruise window (buffer full, pre-maneuver)

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
// REMOTE-SK-001 — ONE non-firing Ki-61 we WATCH: wings-level cruise (coast tracks), a hard banked
// break at tick 90 (the maneuver), roll-out at tick 150. Schedule bank angles are pre-converted to
// radians via the SAME deg2rad the reference uses (deg * PI/180 with the shared hex-float PI), so
// the authoritative trajectory — and thus the canonical digest — is bit-identical.
// ---------------------------------------------------------------------------------------------
static constexpr double D2R = netsnap::DEG2RAD;  // == ref_kernel.deg2rad's PI/180 (same hex-float PI)

static const session::Phase RS_P[] = {
    {0u,   0.0 * D2R,  1.0, 0.72, false},   // cruise (steady)
    {90u,  55.0 * D2R, 1.8, 1.0,  false},   // hard break (maneuver)
    {150u, 0.0 * D2R,  1.0, 0.72, false},   // roll out
};

static session::AircraftSpec RS_AC[1];
static const session::Scenario& remote_scenario() {
    static bool built = [] {
        RS_AC[0].env = &envtab::KI61;
        RS_AC[0].lat = 0.0;
        RS_AC[0].lon = 0.0;
        RS_AC[0].psi = 90.0 * D2R;
        RS_AC[0].phi = 0.0;
        RS_AC[0].alt = 4000.0;
        RS_AC[0].tas = 200.0;
        RS_AC[0].sched = RS_P;
        RS_AC[0].n_phase = 3;
        return true;
    }();
    (void)built;
    static const session::Scenario sc = {
        RS_AC, 1u, /*ticks=*/200u, /*snap_every=*/5u, /*lag=*/0u, /*render=*/0u,
        /*drops=*/nullptr, /*n_drops=*/0u};
    return sc;
}

// ---------------------------------------------------------------------------------------------
// SMOOTH-SK-001 (layer 25 demo) — the SAME Ki-61 under a harsher bad-network regime: SPARSE 5 Hz
// snapshots (snap/20), a 200 ms lag (lag 20), and a VIOLENT break (bank 75deg, g 3.0). Here the coast
// genuinely drifts across the long gaps and each reseed POPS the display — where the hard snap jerks
// and smoothing earns its keep. Radians via the shared hex-float D2R (bit-matches the Python ref).
// ---------------------------------------------------------------------------------------------
static const unsigned SMOOTH_LAG = 20, SMOOTH_SNAP = 20;

static const session::Phase SM_P[] = {
    {0u,   0.0 * D2R,  1.0, 0.72, false},   // cruise
    {60u,  75.0 * D2R, 3.0, 1.0,  false},   // VIOLENT break
    {150u, 0.0 * D2R,  1.0, 0.72, false},   // roll out
};

static session::AircraftSpec SM_AC[1];
static const session::Scenario& smooth_scenario() {
    static bool built = [] {
        SM_AC[0].env = &envtab::KI61;
        SM_AC[0].lat = 0.0;
        SM_AC[0].lon = 0.0;
        SM_AC[0].psi = 90.0 * D2R;
        SM_AC[0].phi = 0.0;
        SM_AC[0].alt = 4000.0;
        SM_AC[0].tas = 220.0;
        SM_AC[0].sched = SM_P;
        SM_AC[0].n_phase = 3;
        return true;
    }();
    (void)built;
    static const session::Scenario sc = {
        SM_AC, 1u, /*ticks=*/200u, /*snap_every=*/SMOOTH_SNAP, /*lag=*/0u, /*render=*/0u,
        /*drops=*/nullptr, /*n_drops=*/0u};
    return sc;
}

// ---------------------------------------------------------------------------------------------
// LEG 1: coast tracks "now" and beats interpolation over the steady window; canonical digest == pin.
// ---------------------------------------------------------------------------------------------
static int leg_beats_interp(const Rails& rails, const session::Scenario& sc) {
    const auto states = netpredict::authoritative_own_states(rails, sc);
    const auto frames = session::build_server_frames(rails, sc);

    double coast_err = netremote::coast_now_error(rails, states, frames, CLIENT_LAG,
                                                  STEADY_LO, STEADY_HI, netremote::Source::CANONICAL);
    double interp_err = netremote::interp_now_error(states, frames, CLIENT_LAG, RENDER_DELAY,
                                                    STEADY_LO, STEADY_HI, /*drops=*/{});

    netremote::RemoteResult r = netremote::run_remote_client(
        rails, states, frames, CLIENT_LAG, /*drops=*/{}, /*reconcile=*/true,
        netremote::Source::CANONICAL);
    netremote::RemoteResult r2 = netremote::run_remote_client(
        rails, states, frames, CLIENT_LAG, /*drops=*/{}, /*reconcile=*/true,
        netremote::Source::CANONICAL);

    int fails = 0;
    if (!(coast_err > 0.0) || !(interp_err > 0.0) || !(coast_err < interp_err)) {
        ++fails;
        std::printf("FAIL LEG1: coast now-err %.3e not < interp now-err %.3e\n", coast_err, interp_err);
    }
    if (r.digest != PIN_CANONICAL) {
        ++fails;
        std::printf("FAIL LEG1: canonical digest %s != pin %s\n", r.digest.c_str(), PIN_CANONICAL);
    }
    if (r.digest != r2.digest) {
        ++fails;
        std::printf("FAIL LEG1: canonical digest not reproducible\n");
    }
    if (fails == 0)
        std::printf("  LEG1 PASS: coast tracks NOW %.1fx tighter than interpolation "
                    "(coast %.2e vs interp %.2e rad over ticks %u..%u) -> canonical digest %s\n",
                    interp_err / coast_err, coast_err, interp_err, STEADY_LO, STEADY_HI,
                    r.digest.c_str());
    return fails;
}

// ---------------------------------------------------------------------------------------------
// LEG 2: reconcile is load-bearing. With reconcile the now-error stays bounded across the maneuver;
// a no-reconcile control (spawn-seeded, never corrected) drifts without bound.
// ---------------------------------------------------------------------------------------------
static int leg_reconcile_load_bearing(const Rails& rails, const session::Scenario& sc) {
    const auto states = netpredict::authoritative_own_states(rails, sc);
    const auto frames = session::build_server_frames(rails, sc);

    double bounded = netremote::run_remote_client(rails, states, frames, CLIENT_LAG, {}, true,
                                                  netremote::Source::CANONICAL).max_pos_err;
    double drifting = netremote::run_remote_client(rails, states, frames, CLIENT_LAG, {}, false,
                                                   netremote::Source::CANONICAL).max_pos_err;
    int fails = 0;
    if (!(drifting > 10.0 * bounded)) {
        ++fails;
        std::printf("FAIL LEG2: no-reconcile did not drift (bounded=%.3e drift=%.3e)\n",
                    bounded, drifting);
    }
    if (fails == 0)
        std::printf("  LEG2 PASS: reconcile keeps the coast BOUNDED (%.2e rad) across the tick-90 "
                    "break; no-reconcile control drifts %.1fx (%.2e rad)\n",
                    bounded, drifting / bounded, drifting);
    return fails;
}

// ---------------------------------------------------------------------------------------------
// LEG 3: BOUNDED over a real socket. The remote's commands are upstreamed scrambled/chunked through
// broadcast_input -> frames stream back byte-identical to build_server_frames -> the observing coast
// reseeds against the decoded lossy wire -> bounded error + wire digest == pin.
// ---------------------------------------------------------------------------------------------
static int leg_socket(const Rails& rails, const session::Scenario& sc) {
    const auto states = netpredict::authoritative_own_states(rails, sc);
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
            std::printf("FAIL LEG3 socket TIMED OUT (socket hang)\n");
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
        std::printf("FAIL LEG3: transport (ok=%d got=%zu/%zu pending=%zu)\n", stats.ok ? 1 : 0,
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
        std::printf("FAIL LEG3: downstream frames NOT byte-identical to build_server_frames\n");
        return fails;
    }

    netremote::RemoteResult r = netremote::run_remote_client(
        rails, states, delivered, CLIENT_LAG, /*drops=*/{}, /*reconcile=*/true,
        netremote::Source::WIRE);

    if (r.delivered == 0 || r.max_pos_err > 1e-2) {
        ++fails;
        std::printf("FAIL LEG3: wire coast unbounded (delivered=%u max_pos_err=%.3e)\n",
                    r.delivered, r.max_pos_err);
    }
    if (r.digest != PIN_WIRE) {
        ++fails;
        std::printf("FAIL LEG3: wire digest %s != pin %s\n", r.digest.c_str(), PIN_WIRE);
    }
    if (fails == 0)
        std::printf("  LEG3 PASS: %zu commands UP scrambled (1 B/send) -> %zu frames byte-identical to "
                    "build_server_frames; remote coasted vs lossy wire within %.2e rad -> wire digest %s\n",
                    cmds.size(), ref.size(), r.max_pos_err, r.digest.c_str());
    return fails;
}

// ---------------------------------------------------------------------------------------------
// LEG 4 (LAYER 25 — SMOOTHING HIDES THE POP, in-process on SMOOTH-SK-001): the smoothed display's
// worst single-tick state jump is a fraction of the hard snap's; smooth=1 reproduces the coast
// EXACTLY (the degenerate identity — only the render blend differs); the smoothed canonical + wire
// digests == the pinned reference values; and the smoothed display is bounded but LAGGIER than the
// snap (the honest trade-off). The coast target is byte-identical to layer 24 throughout.
// ---------------------------------------------------------------------------------------------
static int leg_smoothing(const Rails& rails, const session::Scenario& sc) {
    const auto states = netpredict::authoritative_own_states(rails, sc);
    const auto frames = session::build_server_frames(rails, sc);

    // the layer-24 coast on SMOOTH-SK (the reference the hard snap must reproduce)
    netremote::RemoteResult coast = netremote::run_remote_client(
        rails, states, frames, SMOOTH_LAG, /*drops=*/{}, /*reconcile=*/true,
        netremote::Source::CANONICAL);
    // smooth=1 hard snap, and the actual smoothed display (both canonical + wire)
    netremote::SmoothResult snap = netremote::run_remote_client_smoothed(
        rails, states, frames, SMOOTH_LAG, {}, true, netremote::Source::CANONICAL, /*smooth=*/1.0);
    netremote::SmoothResult sc_canon = netremote::run_remote_client_smoothed(
        rails, states, frames, SMOOTH_LAG, {}, true, netremote::Source::CANONICAL, SMOOTH_FACTOR);
    netremote::SmoothResult sc_wire = netremote::run_remote_client_smoothed(
        rails, states, frames, SMOOTH_LAG, {}, true, netremote::Source::WIRE, SMOOTH_FACTOR);
    netremote::SmoothResult sc_canon2 = netremote::run_remote_client_smoothed(
        rails, states, frames, SMOOTH_LAG, {}, true, netremote::Source::CANONICAL, SMOOTH_FACTOR);

    int fails = 0;
    // (a) smoothing shrinks the worst single-tick pop
    if (!(snap.max_jump > 0.0) || !(sc_canon.max_jump < snap.max_jump)) {
        ++fails;
        std::printf("FAIL LEG4: smoothing did not shrink the pop (snap=%.3e smooth=%.3e)\n",
                    snap.max_jump, sc_canon.max_jump);
    }
    // (b) degenerate identity: smooth=1 hard snap == the layer-24 coast, bit-for-bit
    if (snap.digest != coast.digest) {
        ++fails;
        std::printf("FAIL LEG4: smooth=1 not identical to the coast (%s != %s)\n",
                    snap.digest.c_str(), coast.digest.c_str());
    }
    // (c) smoothed digests reproduce the pinned reference values (canonical + wire), reproducibly
    if (sc_canon.digest != PIN_SMOOTH_CANON) {
        ++fails;
        std::printf("FAIL LEG4: smoothed canonical digest %s != pin %s\n",
                    sc_canon.digest.c_str(), PIN_SMOOTH_CANON);
    }
    if (sc_wire.digest != PIN_SMOOTH_WIRE) {
        ++fails;
        std::printf("FAIL LEG4: smoothed wire digest %s != pin %s\n",
                    sc_wire.digest.c_str(), PIN_SMOOTH_WIRE);
    }
    if (sc_canon.digest != sc_canon2.digest) {
        ++fails;
        std::printf("FAIL LEG4: smoothed digest not reproducible\n");
    }
    // (d) honest trade-off: bounded, but laggier than the hard snap during the transient
    if (!(sc_canon.max_pos_err >= coast.max_pos_err) || sc_canon.max_pos_err > 1e-1) {
        ++fails;
        std::printf("FAIL LEG4: trade-off broken (snap now-err %.3e smooth now-err %.3e)\n",
                    coast.max_pos_err, sc_canon.max_pos_err);
    }
    if (fails == 0)
        std::printf("  LEG4 PASS: smoothing shrinks the worst pop %.1fx (%.2e -> %.2e over the "
                    "SMOOTH-SK break) with smooth=%.2f; smooth=1 == the coast digest; smoothed "
                    "canonical %s + wire %s pinned; laggier now-err %.2e (bounded)\n",
                    snap.max_jump / sc_canon.max_jump, snap.max_jump, sc_canon.max_jump, SMOOTH_FACTOR,
                    sc_canon.digest.c_str(), sc_wire.digest.c_str(), sc_canon.max_pos_err);
    return fails;
}

int main() {
    netsock::WsaGuard wsa;
    const Rails rails = sealed_rails();
    const session::Scenario& sc = remote_scenario();
    int fails = 0;
    fails += leg_beats_interp(rails, sc);
    fails += leg_reconcile_load_bearing(rails, sc);
    fails += leg_socket(rails, sc);
    fails += leg_smoothing(rails, smooth_scenario());
    if (fails == 0) {
        std::printf("PASS: layer-24 remote-aircraft prediction — dead-reckoning coast tracks a remote "
                    "to NOW (far tighter than interpolation), stays BOUNDED across a maneuver via the "
                    "reconcile, reconstructs the pinned digest over a real lossy socket; and layer-25 "
                    "reconcile SMOOTHING hides the maneuver pop (bounded, reproducible)\n");
        return 0;
    }
    std::printf("RESULT: layer-24/25 remote-predict bridge FAIL (%d mismatches)\n", fails);
    return 1;
}
