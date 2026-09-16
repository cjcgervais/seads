// THE MAVERICK SQUADRON (v5 world; drone/maverick.h): the tunnel-run adversary
// AI. Pins the pure pieces with mutation-verified, non-vacuous cases — the
// strict-superset firewall (enabled=false is bit-identical), the closed-loop
// WORTHINESS run (dive in, thread the -2000 m bore, climb out), the hysteretic
// wall guard, determinism, the airframe-relative envelope, the nearest_s
// window, and the run-COMMIT (an engaged maverick in the bore ignores the
// player).
//
// Discipline (CLAUDE.md ## Learned): every leg proves its baseline premise
// before the assertion (the fixture-no-op class); the differential firewall
// pins bit-identity, not a hide-band; the hysteresis latch is driven OPEN-LOOP.

#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <cstdlib>
#include <glm/glm.hpp>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "combat/conquest.h"
#include "combat/kill.h"
#include "combat/raid.h"
#include "config/load_aircraft.h"
#include "config/load_game.h"
#include "config/load_scenario.h"
#include "drone/drone.h"
#include "drone/maverick.h"
#include "sim/aero.h"  // sim::rho_at (the envelope guard's v_stall)
#include "sim/environment.h"
#include "sim/state.h"
#include "sim/world.h"
#include "world/faction_bubbles.h"
#include "world/heightfield.h"
#include "world/tunnel_geo.h"
#include "world/tunnel_net.h"

#ifdef NDEBUG
#error "SEADS gate requires an assert-live build (SPEC 6.1)"
#endif

namespace {

constexpr double kPi = 3.14159265358979323846;

const sim::AircraftParams kAp =
    cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");

// The committed maverick tuning (the real dials the game ships) — the closed-
// loop run is graded against THESE, so a retune that breaks the run trips here.
const cfg::ScenarioParams kScen =
    cfg::load_scenario_toml(SEADS_CONFIG_DIR "/scenario.toml", kAp);

// Uniform terrain at `elev_m` (test_tunnel.cpp fixture) so the mouths sit at a
// known surface radius and a plane above the cavern crashes on terrain.
world::HeightField uniform_field(double elev_m, double relief = 4000.0) {
    world::HeightField hf;
    hf.w = 8;
    hf.h = 4;
    hf.R = kAp.R;
    hf.relief_scale = relief;
    hf.u_offset = 0.0;
    const double f = std::min(std::max(elev_m / relief, 0.0), 1.0);
    hf.px.assign(static_cast<std::size_t>(hf.w) * hf.h,
                 static_cast<std::uint16_t>(f * 65535.0 + 0.5));
    return hf;
}

// The T1 canon tunnel dials (test_tunnel.cpp's own params).
world::TunnelParams test_tp() {
    world::TunnelParams tp;
    tp.sphere_R = kAp.R;
    tp.tube_width_m = 110.0;
    tp.tube_height_m = 90.0;
    tp.depth_m = 1600.0;
    tp.soft_m = 40.0;
    tp.ramp_frac = 0.3;
    tp.spacing_m = 150.0;
    tp.floor_height_m = 0.0;
    tp.arena_a_m = 7350.0;
    tp.arena_c_m = 2600.0;
    tp.arena_depth_m = 1500.0;
    tp.cavern_core_m = 2500.0;
    tp.breach_margin_m = 300.0;
    tp.chamber_long_m = 200.0;
    tp.chamber_lat_m = 140.0;
    tp.chamber_vert_m = 120.0;
    tp.chamber_breach_offset_m = 800.0;
    tp.connector_radius_m = 60.0;
    tp.chambers_on = true;
    tp.bowl_radius_m = 450.0;
    tp.bowl_depth_m = 300.0;
    tp.mouth_sink_m = 130.0;
    tp.min_cover_m = 60.0;
    tp.trench_len_m = 450.0;
    tp.trench_rim_m = 150.0;
    return tp;
}

sim::GroundParams tunnel_ground_params() {
    sim::GroundParams gp;
    gp.slope_limit_cos = std::cos(15.0 * kPi / 180.0);
    gp.friction = 0.08;
    gp.max_sink_ms = 5.0;
    gp.normal_probe_m = 60.0;
    gp.contact_height_m = 0.0;
    gp.deep_penetration_m = 50.0;
    return gp;
}

// A maverick forced into DIVE_IN at the entry mouth for `run_dir` (+1 =
// Errington->Murray, -1 the reverse) — the "dropped in at the mouth approach"
// fixture the closed-loop worthiness + tripwire pins share. Sets the approach
// pose TRANSIT flies to (set back along the bore tangent + up), the entry
// speed, and seeds the mode machine into DIVE_IN.
inline drone::DroneState dive_in_drone(const drone::DroneParams& dp,
                                       const world::TunnelNet& net, int run_dir,
                                       int idx) {
    const maverick::MaverickParams& mp = dp.maverick;
    const maverick::TunnelRoute route(net);
    const glm::dvec3 entry =
        (run_dir > 0) ? net.spine.front().pos : net.spine.back().pos;
    const glm::dvec3 edir = glm::normalize(entry);
    const glm::dvec3 into_tan =
        (run_dir > 0) ? net.spine.front().tan : -net.spine.back().tan;
    const glm::dvec3 approach =
        entry - into_tan * mp.approach_back_m + edir * mp.approach_alt_m;
    const glm::dvec3 fwd = glm::normalize(entry - approach);

    drone::DroneState d = drone::spawn_drone(kAp, dp, idx, 10);
    d.curr = drone::level_state_at(dp, approach, fwd);
    const double v0 =
        std::min(mp.tunnel_speed_max,
                 mp.base_speed * maverick::traits_for(idx).speed_scale);
    d.curr.velocity = fwd * v0;
    d.curr.last_vhat = fwd;
    d.prev = d.curr;
    d.grounded = false;
    d.mav.inited = true;
    d.mav.mode = maverick::MaverickState::Mode::DIVE_IN;
    d.mav.run_dir = run_dir;
    d.mav.s_est = (run_dir > 0) ? 0.0 : route.L;
    return d;
}

}  // namespace

// ===========================================================================
// (a) STRICT SUPERSET — enabled=false is BIT-IDENTICAL to the pre-maverick
// fleet. The maverick block in drone::tick is gated on (enabled && tunnels), so
// a DISABLED drone flown through a full ground+tunnel env must reproduce
// EXACTLY the same trajectory as the pre-maverick patrol/pursue path
// (env=nullptr), for a drone far from the tunnel (so the tunnel's own
// crash-yield never fires). Patrol AND engaged. MUTATION: drop the `enabled`
// gate (always run the brain)
// -> the maverick weave replaces the patrol program -> the compare diverges.
// ===========================================================================
TEST_CASE(
    "maverick: enabled=false is bit-identical to the pre-maverick fleet") {
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(test_tp(), &hf);
    sim::Environment env_full;
    env_full.ground = &hf;
    env_full.tunnels = &net;
    env_full.ground_params = tunnel_ground_params();

    drone::DroneParams dp = kScen.drone;
    dp.maverick.enabled = false;  // the strict-superset premise

    auto run_pair = [&](bool engaged, const sim::SimState* player) {
        // A: disabled + FULL env (ground+tunnels). B: disabled + no env.
        drone::DroneState a = drone::spawn_drone(kAp, dp, 4, 10);
        drone::DroneState b = a;
        a.engaged = engaged;
        b.engaged = engaged;
        for (int i = 0; i < 600; ++i) {
            drone::tick(a, kAp, dp, &env_full, player);
            drone::tick(b, kAp, dp, nullptr, player);
            REQUIRE(a.curr.position == b.curr.position);
            REQUIRE(a.curr.velocity == b.curr.velocity);
            REQUIRE(a.curr.orientation == b.curr.orientation);
        }
        // Premise: the drone actually flew (a live run, not a stalled fixture).
        REQUIRE(a.age_ticks == 600);
        // Premise: it never went near the tunnel (so env_full's tunnel crash-
        // yield is genuinely inert here — the compare isolates the maverick
        // gate).
        REQUIRE(sim::altitude(a.curr.position, kAp) > 0.0);
    };

    SECTION("patrol") { run_pair(false, nullptr); }
    SECTION("engaged") {
        const sim::SimState player = drone::spawn_state(kAp, dp, 0, 10);
        run_pair(true, &player);
    }
}

// A tighter gate leg: enabled=TRUE but tunnels NULL must ALSO skip the brain
// (the second half of the gate) — bit-identical to enabled=false. MUTATION:
// gate on `enabled` alone (ignore the null tunnel) -> maverick_step deref's a
// null net -> crash / divergence.
TEST_CASE("maverick: enabled but no tunnel is bit-identical to disabled") {
    const world::HeightField hf = uniform_field(300.0);
    sim::Environment env_ground;  // ground live, tunnels NULL
    env_ground.ground = &hf;
    env_ground.ground_params = tunnel_ground_params();

    drone::DroneParams on = kScen.drone;
    on.maverick.enabled = true;
    drone::DroneParams off = kScen.drone;
    off.maverick.enabled = false;

    drone::DroneState a = drone::spawn_drone(kAp, on, 4, 10);
    drone::DroneState b = drone::spawn_drone(kAp, off, 4, 10);
    for (int i = 0; i < 400; ++i) {
        drone::tick(a, kAp, on, &env_ground, nullptr);
        drone::tick(b, kAp, off, &env_ground, nullptr);
        REQUIRE(a.curr.position == b.curr.position);
        REQUIRE(a.curr.orientation == b.curr.orientation);
    }
    REQUIRE(a.age_ticks == 400);
}

// ===========================================================================
// (b) THE WORTHINESS PIN — the closed-loop tunnel run, NOW A CLEAN SURFACE
// BREAK-OUT (the curvature-ff bore autopilot rung, v5 2026-07-24). A maverick
// forced into DIVE_IN at the entry mouth must, for BOTH directions (their exit
// slopes differ — 21 deg Murray vs 22.4 deg Errington — and the ff reads the
// LOCAL tangent so one law serves both): ENTER the net, NEVER crash threading
// the bore/climb-out, keep the signed distance well below -wall_margin_floor
// deep in the bore, progress monotonically along the travel direction, go DEEP
// (the -1600 m dip), thread (nearly) the WHOLE bore under the slope
// feedforward, break CLEAN out to the surface, hand back to PATROL, and then
// fly free above the surface without crashing. The old "honest ceiling" (a
// final steep-ascent clip at exit_frac ~0.72) is GONE — the slope-feedforward +
// throttle-feedforward
// + the virtual centreline extension out of the mouth thread the steep exit and
// climb out of the open bowl. MUTATION: break the slope ff (slope_ff_gain=0,
// pinned in (b2)) -> the ascent PIOs / the descent stalls and it augers.
//
// Non-vacuous premises: REQUIRE it entered AND that the STEEP segment was
// actually flown (a path slope > 15 deg observed inside the bore) before
// grading the feedforward — else the fixture could pass without exercising the
// ff.
// ===========================================================================
TEST_CASE(
    "maverick: a worthy tunnel run threads the bore and breaks CLEAN out") {
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(test_tp(), &hf);
    sim::Environment env;
    env.ground = &hf;
    env.tunnels = &net;
    env.ground_params = tunnel_ground_params();

    drone::DroneParams dp = kScen.drone;  // the committed tuning
    REQUIRE(dp.maverick.enabled);  // premise: the ship config runs mavericks
    const maverick::MaverickParams& mp = dp.maverick;
    const maverick::TunnelRoute route(net);
    REQUIRE(route.L > 8000.0);  // premise: a real ~12 km bore

    auto grade = [&](int run_dir) {
        INFO("run_dir = " << run_dir);
        drone::DroneState d = dive_in_drone(dp, net, run_dir, 0);

        bool entered = false, reached_patrol = false, crashed = false;
        double min_alt = 1e9, thread_min_margin = 1e9, max_gpath = 0.0,
               max_regress = 0.0, max_prog = 0.0, prev_prog = 0.0;
        bool have_prev = false;
        int i = 0;
        for (; i < 300 * 120; ++i) {  // generous cap (~300 s)
            const drone::DroneTickResult r =
                drone::tick(d, kAp, dp, &env, nullptr);
            if (r.respawned) {
                crashed = true;
                break;
            }
            const glm::dvec3 pos = d.curr.position;
            if (net.contains(pos)) {
                entered = true;
                const double sd = net.signed_distance(pos);
                const double alt = glm::length(pos) - kAp.R;
                min_alt = std::min(min_alt, alt);
                // Wall clearance deep in the bore (alt < -300 m excludes the
                // legitimate sd->0 crossing as it LEAVES the net at the exit).
                if (alt < -300.0)
                    thread_min_margin = std::min(thread_min_margin, -sd);
                // The path's own climb angle here (the ff premise: the steep
                // ascent must actually be flown).
                const glm::dvec3 lup = glm::normalize(pos);
                const double gp = std::asin(std::clamp(
                    static_cast<double>(run_dir) *
                        glm::dot(route.point_at(d.mav.s_est).tan, lup),
                    -1.0, 1.0));
                max_gpath = std::max(max_gpath, gp);
                // Progress along the travel direction (monotone both ways).
                const double prog =
                    (run_dir > 0) ? d.mav.s_est : (route.L - d.mav.s_est);
                max_prog = std::max(max_prog, prog);
                if (have_prev)
                    max_regress = std::max(max_regress, prev_prog - prog);
                prev_prog = prog;
                have_prev = true;
            }
            if (d.mav.mode == maverick::MaverickState::Mode::PATROL &&
                i > 200) {
                reached_patrol = true;
                break;
            }
        }

        REQUIRE(entered);        // dove into the pit / bore
        REQUIRE_FALSE(crashed);  // never crashed (respawn == death)
        REQUIRE(max_gpath >
                15.0 * kPi / 180.0);         // premise: the STEEP ascent flew
        REQUIRE(min_alt < -1400.0);          // went DEEP (the -1600 m dip)
        REQUIRE(max_prog > 0.95 * route.L);  // threaded (nearly) the FULL bore
        REQUIRE(thread_min_margin >
                mp.wall_margin_floor_m);  // wall-clear deep in
        REQUIRE(max_regress < 150.0);     // monotone progress
        REQUIRE(reached_patrol);          // broke CLEAN out, handed to PATROL

        // ...and flies FREE above the surface for a few seconds without
        // crashing (a genuine surface break-out, not a momentary pop that
        // re-clips).
        double free_min_alt = 1e9;
        for (int j = 0; j < 3 * 120 && i < 300 * 120; ++j, ++i) {
            const drone::DroneTickResult r =
                drone::tick(d, kAp, dp, &env, nullptr);
            REQUIRE_FALSE(r.respawned);
            free_min_alt =
                std::min(free_min_alt, glm::length(d.curr.position) - kAp.R);
        }
        REQUIRE(free_min_alt >
                0.0);  // stayed above the surface (broke out clean)
    };

    SECTION("Errington -> Murray (+1)") { grade(+1); }
    SECTION("Murray -> Errington (-1)") { grade(-1); }
}

// ===========================================================================
// (b2) THE FEEDFORWARD TRIPWIRE (the knob-off arm — proves the slope
// feedforward is LOAD-BEARING, not decorative). The SAME closed-loop run with
// slope_ff_gain = 0 (the path slope is NOT fed forward; gamma is error-only)
// must FAIL — the plane can no longer command the bore's steep descent/ascent
// from a near-zero tracking error, so it augers and respawns. Contrast with
// (b), where the shipped slope_ff_gain = 1 threads clean out. BOTH directions.
// (Mutation-equivalent: this is the ff term deleted at the config seam.)
// ===========================================================================
TEST_CASE(
    "maverick: the path-slope feedforward is load-bearing (knob-off arm)") {
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(test_tp(), &hf);
    sim::Environment env;
    env.ground = &hf;
    env.tunnels = &net;
    env.ground_params = tunnel_ground_params();

    drone::DroneParams dp = kScen.drone;
    dp.maverick.slope_ff_gain = 0.0;  // KILL the feedforward (error-only gamma)

    auto crashes = [&](int run_dir) {
        drone::DroneState d = dive_in_drone(dp, net, run_dir, 0);
        bool entered = false;
        for (int i = 0; i < 60 * 120; ++i) {  // 60 s is ample to fail
            const drone::DroneTickResult r =
                drone::tick(d, kAp, dp, &env, nullptr);
            if (net.contains(d.curr.position)) entered = true;
            if (r.respawned) return std::pair<bool, bool>{entered, true};
            if (d.mav.mode == maverick::MaverickState::Mode::PATROL && i > 200)
                return std::pair<bool, bool>{
                    entered, false};  // (unexpectedly) broke out
        }
        return std::pair<bool, bool>{entered, false};
    };

    SECTION("+1 without the ff augers") {
        const auto [entered, respawned] = crashes(+1);
        REQUIRE(entered);    // premise: it did dive into the bore
        REQUIRE(respawned);  // ...and then crashed WITHOUT the feedforward
    }
    SECTION("-1 without the ff augers") {
        const auto [entered, respawned] = crashes(-1);
        REQUIRE(entered);
        REQUIRE(respawned);
    }
}

// ===========================================================================
// (c) THE WALL GUARD HYSTERESIS — pinned OPEN-LOOP on a fixed rippling signal
// straddling the engage threshold (the S6 discipline: a closed-loop chase nulls
// to monotone and pins nothing). Hysteretic: engage once, then HOLD (the ripple
// never reaches the release depth) -> <= 1 switch. A single-threshold reference
// strobes every ripple. MUTATION: collapse release to margin (non-hysteretic)
// -> the switch count matches the strobing reference.
// ===========================================================================
TEST_CASE("maverick::update_wall_guard: hysteretic latch does not strobe") {
    const double margin = 34.0, release = 60.0;  // release > margin (the gap)
    bool hyst = false, single = false;
    int hyst_sw = 0, single_sw = 0;
    // sd ripples across -margin (between -30 and -38), never as deep as
    // -release.
    for (int i = 0; i < 40; ++i) {
        const double sd = (i % 2 == 0) ? -30.0 : -38.0;
        const bool h = maverick::update_wall_guard(hyst, sd, margin, release);
        const bool s =
            (sd > -margin);  // the non-hysteretic single-threshold ref
        if (i > 0 && h != hyst) ++hyst_sw;
        if (i > 0 && s != single) ++single_sw;
        hyst = h;
        single = s;
    }
    REQUIRE(hyst_sw <= 1);     // engages once, then latched (no chatter)
    REQUIRE(single_sw >= 10);  // premise: the signal really does straddle
}

// ===========================================================================
// (d) DETERMINISM — the brain is a pure function; two identical mavericks flown
// in the same tunnel step BIT-IDENTICALLY for 1000 ticks (no hidden clock/rng).
// ===========================================================================
TEST_CASE("maverick: deterministic (two identical pilots step identically)") {
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(test_tp(), &hf);
    sim::Environment env;
    env.ground = &hf;
    env.tunnels = &net;
    env.ground_params = tunnel_ground_params();

    drone::DroneParams dp = kScen.drone;
    drone::DroneState a = drone::spawn_drone(kAp, dp, 3, 10);
    drone::DroneState b = a;
    for (int i = 0; i < 1000; ++i) {
        drone::tick(a, kAp, dp, &env, nullptr);
        drone::tick(b, kAp, dp, &env, nullptr);
    }
    REQUIRE(a.curr.position == b.curr.position);
    REQUIRE(a.curr.velocity == b.curr.velocity);
    REQUIRE(a.curr.orientation == b.curr.orientation);
    REQUIRE(a.mav.mode == b.mav.mode);
    REQUIRE(a.mav.s_est == b.mav.s_est);
    REQUIRE(a.mav.patrol_countdown == b.mav.patrol_countdown);
}

// ===========================================================================
// (e) THE ENVELOPE — every one of the 10 pilots' derived speeds sits in the
// airframe's controllable band, and the caps stay within the autopilot's
// authority. Config-RELATIVE (recomputed from aircraft.toml), never welded
// constants (the AT-15 trap). The committed table loads without throwing (the
// loader's envelope guards ran).
// ===========================================================================
TEST_CASE("maverick: every pilot's derived envelope is airframe-legal") {
    const maverick::MaverickParams& mp = kScen.drone.maverick;
    // Stall at the drones' spawn altitude (the loader's own v_stall).
    const double v_stall = std::sqrt(
        2.0 * kAp.mass * kAp.g /
        (sim::rho_at(kScen.drone.spawn_alt, kAp) * kAp.S * kAp.Cl_max));
    REQUIRE(v_stall > 0.0);

    for (int i = 0; i < 10; ++i) {
        const maverick::MaverickTraits& tr = maverick::kMavericks[i];
        INFO("pilot " << tr.callsign);
        // Trait sanity (the flavour table stays in its documented bands).
        REQUIRE(tr.speed_scale >= 1.1);
        REQUIRE(tr.speed_scale <= 1.55);
        REQUIRE(tr.aggression >= 0.0);
        REQUIRE(tr.aggression <= 1.0);
        REQUIRE(tr.daredevil >= 0.0);
        REQUIRE(tr.daredevil <= 1.0);
        REQUIRE(std::abs(tr.run_dir) == 1);
        REQUIRE(tr.period_s > 0.0);

        // The patrol/transit cruise and the in-bore hot speed both stay above
        // stall and below redline (the controllable band).
        const double cruise = mp.base_speed * tr.speed_scale;
        const double hot =
            std::min(mp.tunnel_speed_max, cruise * (1.0 + 0.30 * tr.daredevil));
        REQUIRE(cruise >= 1.3 * v_stall);
        REQUIRE(cruise <= kAp.v_redline);
        REQUIRE(hot >= 1.3 * v_stall);
        REQUIRE(hot <= kAp.v_redline);

        // The per-pilot wall margin never drops below the hard floor.
        const double margin = std::max(
            mp.wall_margin_floor_m,
            mp.wall_margin_m -
                tr.daredevil * (mp.wall_margin_m - mp.wall_margin_floor_m));
        REQUIRE(margin >= mp.wall_margin_floor_m);
        REQUIRE(margin < 90.0);  // below the bore vertical semi-axis
    }

    // Caps within the autopilot's authority (mirrors the loader ceilings).
    REQUIRE(mp.dive_gamma_cap <= 65.0 * kPi / 180.0);
    REQUIRE(mp.run_gamma_cap <= 45.0 * kPi / 180.0);
    REQUIRE(mp.bank_cap <= 70.0 * kPi / 180.0);
    REQUIRE(mp.exit_frac > 0.5);
    REQUIRE(mp.exit_frac <= 1.0);
}

// (The loader's airframe-relative rejection of an over-redline tunnel_speed_max
// is pinned in test_load_scenario.cpp.)

// ===========================================================================
// (f) THE nearest_s WINDOW — progress tracking stays LOCAL to the hint (the
// guard against the vertical doubling-back at the dip). Walking along the spine
// with the previous result as the hint gives monotone progress; and a FAR hint
// keeps the search in its own window (does NOT teleport to a globally-nearer
// arc station). MUTATION: widen the window to a global argmin -> the far-hint
// query snaps to the global nearest and the "stays near the hint" check fails.
// ===========================================================================
TEST_CASE("maverick::TunnelRoute::nearest_s: windowed, monotone, no teleport") {
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(test_tp(), &hf);
    const maverick::TunnelRoute route(net);
    REQUIRE(route.L > 8000.0);

    // Monotone tracking: sample forward along the centreline, hint = prev
    // result.
    double hint = 0.0, prev = -1.0;
    for (double s = 0.0; s <= route.L; s += 200.0) {
        const glm::dvec3 p = route.point_at(s).pos;
        const double r = route.nearest_s(p, hint);
        REQUIRE(r >= prev - 1e-6);  // monotone (non-decreasing)
        REQUIRE(std::abs(r - s) <
                200.0);  // tracks the true arc within a window
        prev = r;
        hint = r;
    }

    // No teleport: a point AT the deep dip queried with a FAR hint stays near
    // the far hint's window — it does NOT jump to the (globally nearer) dip
    // station. Find the deepest spine station.
    double s_deep = 0.0, deepest = 1e9, acc = 0.0;
    for (std::size_t i = 1; i < net.spine.size(); ++i) {
        acc += glm::length(net.spine[i].pos - net.spine[i - 1].pos);
        const double r = glm::length(net.spine[i].pos);
        if (r < deepest) {
            deepest = r;
            s_deep = acc;
        }
    }
    const glm::dvec3 p_deep = route.point_at(s_deep).pos;
    // Local hint recovers the dip arc.
    const double r_local = route.nearest_s(p_deep, s_deep);
    REQUIRE(std::abs(r_local - s_deep) < 200.0);
    // Far hint (2 km past the dip) stays local to ITSELF, not the dip.
    const double far_hint = std::min(route.L, s_deep + 2000.0);
    const double r_far = route.nearest_s(p_deep, far_hint);
    REQUIRE(std::abs(r_far - far_hint) < 900.0);  // in the far hint's window
    REQUIRE(std::abs(r_far - s_deep) > 900.0);    // did NOT teleport to the dip
}

// ===========================================================================
// (g) COMMIT TO THE RUN — a maverick in the BORE (RUN mode, OUTSIDE the arena)
// IGNORES the player, even when engaged: its steering comes from the route, so
// its trajectory is UNAFFECTED by whether a player is present. Contrast: a
// maverick in PATROL (aggressive) DOES pursue, so the player DOES move it — the
// differential proves the test isn't vacuous. MUTATION: let RUN honour
// pursue_player -> the RUN arm diverges when the player is present.
//
// RUNG E2.4 RE-SCOPE (spec P1-6 fixture rule). This pin used to park the drone
// at 0.4*L, which for the synthetic net is INSIDE the arena ellipsoid — where
// the new chamber FIGHT INTERRUPT deliberately fires. Merely loosening the
// assertion would have deleted the pin; instead the fixture MOVES OUT of the
// arena (asserted, not assumed, so a future net retune cannot silently slide it
// back in) and the outside-arena bit-identity claim stays exactly as strong as
// it was. The in-arena case gets its OWN leg below, which proves the interrupt
// engages — the behaviour that replaced the old blanket claim, pinned rather
// than merely permitted.
// ===========================================================================
TEST_CASE("maverick: a committed run in the bore ignores the player") {
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(test_tp(), &hf);
    sim::Environment env;
    env.ground = &hf;
    env.tunnels = &net;
    env.ground_params = tunnel_ground_params();

    drone::DroneParams dp = kScen.drone;
    const maverick::TunnelRoute route(net);

    // Place a maverick in the BORE (outside the arena pocket), flying along the
    // tangent, forced into RUN.
    const double s_mid = 0.12 * route.L;
    const maverick::TunnelRoute::Sample mid = route.point_at(s_mid);
    REQUIRE_FALSE(drone::in_arena(net, mid.pos));  // the re-scope, asserted
    auto make_run_drone = [&]() {
        drone::DroneState d = drone::spawn_drone(kAp, dp, 0, 10);
        d.curr = drone::level_state_at(dp, mid.pos, mid.tan);
        d.curr.velocity = mid.tan * dp.maverick.base_speed;
        d.curr.last_vhat = mid.tan;
        d.prev = d.curr;
        d.grounded = false;
        d.engaged = true;  // ENGAGED — a lesser AI would break off to pursue
        d.mav.inited = true;
        d.mav.mode = maverick::MaverickState::Mode::RUN;
        d.mav.run_dir = 1;
        d.mav.s_est = s_mid;
        return d;
    };

    // A player parked right beside the maverick (a strong pursuit lure).
    sim::SimState player;
    player.position = mid.pos + mid.tan * 300.0;
    player.velocity = glm::dvec3{0.0};
    player.orientation = glm::dquat{1.0, 0.0, 0.0, 0.0};
    player.last_vhat = mid.tan;

    drone::DroneState with = make_run_drone();
    drone::DroneState without = make_run_drone();
    for (int i = 0; i < 200; ++i) {
        drone::tick(with, kAp, dp, &env, &player);
        drone::tick(without, kAp, dp, &env, nullptr);
        REQUIRE(with.curr.position == without.curr.position);  // player ignored
        REQUIRE(with.curr.orientation == without.curr.orientation);
    }
    REQUIRE(with.mav.mode ==
            maverick::MaverickState::Mode::RUN);  // still running

    // Non-vacuous contrast: the SAME engaged maverick in PATROL (aggressive)
    // DOES pursue — the player present vs absent moves it. (GULCH aggression
    // 0.75 >= engage_aggression_min, so PATROL sets pursue_player.)
    auto make_patrol_drone = [&]() {
        drone::DroneState d = drone::spawn_drone(kAp, dp, 0, 10);
        d.engaged = true;
        d.mav.inited = true;
        d.mav.mode = maverick::MaverickState::Mode::PATROL;
        d.mav.patrol_countdown = 100000;  // stay in PATROL for the window
        return d;
    };
    sim::SimState pplayer =
        drone::spawn_state(kAp, dp, 1, 10);  // off to the side
    drone::DroneState pwith = make_patrol_drone();
    drone::DroneState pwithout = make_patrol_drone();
    bool diverged = false;
    for (int i = 0; i < 200; ++i) {
        // env live (tunnels) so the maverick brain runs: PATROL + aggressive
        // sets pursue_player, which falls through to pursue() when a player is
        // present.
        drone::tick(pwith, kAp, dp, &env, &pplayer);
        drone::tick(pwithout, kAp, dp, &env, nullptr);
        if (pwith.curr.position != pwithout.curr.position) diverged = true;
    }
    REQUIRE(
        diverged);  // PATROL pursues -> the player matters (test not vacuous)
}

// ===========================================================================
// (g2) RUNG E2.4 — THE CHAMBER FIGHT INTERRUPT. The other half of the re-scope
// above: the SAME committed run, at a station INSIDE the arena, with the same
// engaged player alongside, must now FIGHT. Three claims, none of them a
// loosened assertion:
//   * the trajectory DIVERGES with the player present (the run no longer
//     ignores him in the chamber),
//   * the interrupt's own budget actually ran (arena_fight_ticks > 0) — so the
//     divergence is the interrupt and not some incidental coupling,
//   * arena_fight_range_m = 0 restores the OLD behaviour bit-for-bit at the
//     SAME station (the knob-off arm, which is what proves the pin above is
//     about the dial and not about the station).
// MUTATION: delete the `if (arena_fight) maverick_committed = false;` line ->
// the divergence and the budget both vanish.
// ===========================================================================
TEST_CASE("maverick: a committed run in the arena fights the player") {
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(test_tp(), &hf);
    sim::Environment env;
    env.ground = &hf;
    env.tunnels = &net;
    env.ground_params = tunnel_ground_params();

    const drone::DroneParams shipped = kScen.drone;
    REQUIRE(shipped.arena_fight_range_m > 0.0);  // non-vacuous: the dial ships
    const maverick::TunnelRoute route(net);

    // The spine station deepest inside the arena pocket (scanned, never a
    // welded fraction — the arena's extent follows the [tunnel] table).
    double s_arena = -1.0, best = 1e18;
    for (double s = 0.0; s <= route.L; s += 100.0) {
        const glm::dvec3 p = route.point_at(s).pos;
        if (!drone::in_arena(net, p)) continue;
        const double n2 = drone::arena_norm2(net, p);
        if (n2 < best) {
            best = n2;
            s_arena = s;
        }
    }
    REQUIRE(s_arena >= 0.0);
    const maverick::TunnelRoute::Sample mid = route.point_at(s_arena);

    const auto make_run_drone = [&](const drone::DroneParams& dp) {
        drone::DroneState d = drone::spawn_drone(kAp, dp, 0, 10);
        d.curr = drone::level_state_at(dp, mid.pos, mid.tan);
        d.curr.velocity = mid.tan * dp.maverick.base_speed;
        d.curr.last_vhat = mid.tan;
        d.prev = d.curr;
        d.grounded = false;
        d.engaged = true;
        d.foe = drone::kFoePlayer;
        d.mav.inited = true;
        d.mav.mode = maverick::MaverickState::Mode::RUN;
        d.mav.run_dir = 1;
        d.mav.s_est = s_arena;
        return d;
    };

    // A player 300 m off the nose — the same lure the bore leg refuses.
    sim::SimState player;
    player.position = mid.pos + mid.tan * 300.0;
    player.velocity = glm::dvec3{0.0};
    player.orientation = glm::dquat{1.0, 0.0, 0.0, 0.0};
    player.last_vhat = mid.tan;

    drone::DroneState with = make_run_drone(shipped);
    drone::DroneState without = make_run_drone(shipped);
    bool diverged = false;
    for (int i = 0; i < 200; ++i) {
        drone::tick(with, kAp, shipped, &env, &player);
        drone::tick(without, kAp, shipped, &env, nullptr);
        if (with.curr.position != without.curr.position) diverged = true;
    }
    REQUIRE(diverged);                    // the chamber fight engaged
    REQUIRE(with.arena_fight_ticks > 0);  // and it was the INTERRUPT that did
    REQUIRE(without.arena_fight_ticks == 0);  // (no foe -> no interrupt)

    // Knob-off arm at the SAME station: the pre-E2.4 behaviour returns
    // bit-for-bit.
    drone::DroneParams off = shipped;
    off.arena_fight_range_m = 0.0;
    drone::DroneState owith = make_run_drone(off);
    drone::DroneState owithout = make_run_drone(off);
    for (int i = 0; i < 200; ++i) {
        drone::tick(owith, kAp, off, &env, &player);
        drone::tick(owithout, kAp, off, &env, nullptr);
        REQUIRE(owith.curr.position == owithout.curr.position);
        REQUIRE(owith.curr.orientation == owithout.curr.orientation);
    }
}

// ===========================================================================
// (h) THE FEEDFORWARD LAW, PINNED OPEN-LOOP (the curvature-ff rung). Place a
// maverick EXACTLY on the centreline at a steep-ascent station, facing along
// the tangent (so the altitude-error residual is ~0), and read the ONE command
// tick:
//   * the commanded gamma == the PATH slope fed forward (slope_ff_gain * the
//     tangent climb angle sampled gamma_lookahead ahead) — NOT zero, NOT the
//     error-only pursuit;
//   * the throttle feedforward == climb_throttle_gain * sin(gamma_cmd) > 0.
// Then slope_ff_gain = 0 collapses the gamma command to ~level (error-only) —
// the knob-off arm proving the slope ff is the load-bearing term. This pins
// BOTH ff terms at unit level (the closed-loop (b) run tolerates throttle_ff =
// 0, so without this leg a throttle_ff deletion ships green — the "additive
// feature is only half-pinned" trap). MUTATION: slope_ff_gain in the gamma law;
// the climb_throttle_gain factor in throttle_ff.
// ===========================================================================
TEST_CASE("maverick: the slope + throttle feedforward command the path climb") {
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(test_tp(), &hf);
    const maverick::TunnelRoute route(net);
    const double kDeg = kPi / 180.0;

    drone::DroneParams dp = kScen.drone;
    const maverick::MaverickParams& mp = dp.maverick;

    // Find the steepest ASCENT station in the first 85% of the bore (an
    // Errington->Murray, run_dir +1, climb) — kept clear of the exit so the RUN
    // mode does not hand off this tick.
    double s_steep = 0.0, best = -1.0;
    for (double s = 0.0; s <= 0.85 * route.L; s += 100.0) {
        const maverick::TunnelRoute::Sample sm = route.point_at(s);
        const double g = glm::dot(sm.tan, glm::normalize(sm.pos));  // +1 climb
        if (g > best) {
            best = g;
            s_steep = s;
        }
    }
    REQUIRE(std::asin(std::clamp(best, -1.0, 1.0)) >
            15.0 * kDeg);  // real steep

    const maverick::TunnelRoute::Sample sm = route.point_at(s_steep);
    drone::DroneState d = drone::spawn_drone(kAp, dp, 0, 10);
    d.curr =
        drone::level_state_at(dp, sm.pos, sm.tan);  // ON centreline, on tangent
    d.curr.velocity = sm.tan * mp.tunnel_speed_max;
    d.curr.last_vhat = sm.tan;
    d.prev = d.curr;
    d.grounded = false;
    d.mav.inited = true;
    d.mav.mode = maverick::MaverickState::Mode::RUN;
    d.mav.run_dir = 1;
    d.mav.s_est = s_steep;

    // The expected feedforward gamma: the tangent climb angle a lookahead ahead
    // (alt-error residual is ~0 on the centreline), scaled by slope_ff_gain.
    const glm::dvec3 lup = glm::normalize(sm.pos);
    const glm::dvec3 tan_ahead =
        route.point_ext(s_steep + mp.gamma_lookahead_m).tan;
    const double gpath_ff =
        std::asin(std::clamp(glm::dot(tan_ahead, lup), -1.0, 1.0));

    maverick::MaverickState m = d.mav;
    const maverick::MaverickCmd cmd =
        maverick::maverick_step(m, d.curr, kAp, mp, route, net, 0, kAp.sim_dt);

    CHECK(cmd.target_gamma > 15.0 * kDeg);  // the climb IS commanded
    CHECK(cmd.target_gamma ==
          Catch::Approx(mp.slope_ff_gain * gpath_ff).margin(2.0 * kDeg));
    // Throttle feedforward = gain * sin(gamma), positive in a climb.
    CHECK(cmd.throttle_ff ==
          Catch::Approx(mp.climb_throttle_gain * std::sin(cmd.target_gamma))
              .margin(1e-9));
    CHECK(cmd.throttle_ff > 0.0);

    // Knob-off arm: kill the slope feedforward -> the gamma command collapses
    // to ~level (only the ~0 alt-error residual remains), a > 10 deg
    // difference.
    maverick::MaverickParams mp0 = mp;
    mp0.slope_ff_gain = 0.0;
    maverick::MaverickState m0 = d.mav;
    const maverick::MaverickCmd cmd0 = maverick::maverick_step(
        m0, d.curr, kAp, mp0, route, net, 0, kAp.sim_dt);
    CHECK(std::abs(cmd0.target_gamma) < 5.0 * kDeg);  // no ff -> ~level
    CHECK(cmd.target_gamma - cmd0.target_gamma >
          10.0 * kDeg);  // ff IS the climb
}

// ===========================================================================
// game-AI-R2 — GREAT-CIRCLE ELEVATION (the transit fix). The old vertical
// channel measured elevation against the straight chord to the target; on the
// 15 km sphere a far target at the SAME altitude sits tens of degrees "below
// the horizon" by pure geometry, so TRANSIT flew a saturated nose-down dive
// from every spawn range (R1 probe: 178 attempts, 0 entries). gc_elevation
// reads radial-altitude-over-surface-arc instead. MUTATION: revert aim_at's
// elev to asin(dot(to_hat, lup)) -> the far-target legs saturate nose-down.
// ===========================================================================
TEST_CASE("maverick::gc_elevation: curvature-safe vertical channel") {
    const double R = kAp.R;
    const glm::dvec3 u0 = glm::normalize(glm::dvec3{1.0, 1.0, 1.0});  // generic
    // An orthonormal tangent at u0 (generic point — no world-axis coincidence).
    const glm::dvec3 t0 =
        glm::normalize(glm::cross(u0, glm::dvec3{0.0, 1.0, 0.0}));

    SECTION(
        "far target at the SAME altitude reads ~level, chord reads a dive") {
        const double alt = 500.0;
        const glm::dvec3 pos = u0 * (R + alt);
        const double arc = 10000.0;  // 10 km away over the surface
        const double ang = arc / (R + alt);
        const glm::dvec3 u1 =
            glm::normalize(u0 * std::cos(ang) + t0 * std::sin(ang));
        const glm::dvec3 target = u1 * (R + alt);
        // Premise (the mutation contrast): the CHORD elevation here is a hard
        // dive — this is what saturated TRANSIT. If this stops holding, the
        // fixture no longer discriminates and must move further out.
        const glm::dvec3 to_hat = glm::normalize(target - pos);
        const double chord_elev =
            std::asin(std::clamp(glm::dot(to_hat, u0), -1.0, 1.0));
        REQUIRE(chord_elev < -15.0 * kPi / 180.0);
        // The fix: same altitude -> essentially level.
        REQUIRE(std::abs(maverick::gc_elevation(pos, target)) <
                0.5 * kPi / 180.0);
    }

    SECTION("converges to the chord form close-in (DIVE_IN unchanged)") {
        const glm::dvec3 pos = u0 * (R + 400.0);
        // 500 m ahead, 150 m down — a pit-entry-scale geometry.
        const glm::dvec3 target =
            glm::normalize(u0 * (R + 400.0) + t0 * 500.0) * (R + 250.0);
        const glm::dvec3 to_hat = glm::normalize(target - pos);
        const double chord =
            std::asin(std::clamp(glm::dot(to_hat, u0), -1.0, 1.0));
        const double gc = maverick::gc_elevation(pos, target);
        REQUIRE(std::abs(gc - chord) < 1.5 * kPi / 180.0);
        REQUIRE(gc < -10.0 * kPi / 180.0);  // premise: a real descent command
    }

    SECTION("straight below reads -90, straight above +90") {
        const glm::dvec3 pos = u0 * (R + 500.0);
        REQUIRE(maverick::gc_elevation(pos, u0 * (R - 500.0)) ==
                Catch::Approx(-kPi / 2).margin(1e-9));
        REQUIRE(maverick::gc_elevation(pos, u0 * (R + 1500.0)) ==
                Catch::Approx(kPi / 2).margin(1e-9));
    }

    SECTION("aim_at at transit range no longer commands the saturated dive") {
        drone::DroneParams dp = kScen.drone;
        const maverick::MaverickParams& mp = dp.maverick;
        const double alt = 600.0;
        const glm::dvec3 pos = u0 * (R + alt);
        const double ang = 12000.0 / R;  // 12 km transit
        const glm::dvec3 u1 =
            glm::normalize(u0 * std::cos(ang) + t0 * std::sin(ang));
        const glm::dvec3 target = u1 * (R + alt);
        const sim::SimState s = drone::level_state_at(
            dp, pos, glm::normalize(t0 - glm::dot(t0, u0) * u0));
        const maverick::Steer st = maverick::aim_at(
            s, target, mp.k_az, mp.k_el, mp.bank_cap, mp.transit_gamma_cap);
        // Chord elevation at 12 km is ~-23 deg -> k_el saturates the -30 deg
        // cap; great-circle reads ~0 -> a near-level command.
        REQUIRE(std::abs(st.target_gamma) < 5.0 * kPi / 180.0);
    }
}

// ===========================================================================
// game-AI-R2 — TRANSIT KEEPS THE TERRAIN NET. The old exemption keyed the
// terrain-avoid skip on is_tunnel_mode, which includes TRANSIT — disarming the
// pull-up exactly while TRANSIT flew its saturated dive. Now only the
// underground-committed modes (DIVE_IN/RUN/CLIMB_OUT) are exempt. MUTATION:
// key the terrain skip back on is_tunnel_mode -> the latch never engages and
// the sink flies into the terrain.
// ===========================================================================
TEST_CASE("maverick: a TRANSIT leg keeps the terrain-avoid safety net") {
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(test_tp(), &hf);
    sim::Environment env;
    env.ground = &hf;
    env.tunnels = &net;
    env.ground_params = tunnel_ground_params();

    drone::DroneParams dp = kScen.drone;
    REQUIRE(dp.maverick.enabled);

    // Predicate sanity: TRANSIT is a tunnel mode but NOT underground.
    REQUIRE(maverick::is_tunnel_mode(maverick::MaverickState::Mode::TRANSIT));
    REQUIRE_FALSE(
        maverick::is_underground_mode(maverick::MaverickState::Mode::TRANSIT));
    REQUIRE(maverick::is_underground_mode(maverick::MaverickState::Mode::RUN));

    // A maverick in TRANSIT far from the mouth, LOW over the terrain (AGL ~80,
    // below avoid_agl_enter_m) and level: the net must engage and hold it off
    // the ground while the run continues.
    const glm::dvec3 entry_dir = glm::normalize(net.spine.front().pos);
    const glm::dvec3 axis =
        glm::normalize(glm::cross(entry_dir, glm::dvec3{0.0, 1.0, 0.0}));
    const double ang = 7000.0 / kAp.R;  // 7 km from the mouth
    const glm::dvec3 u =
        glm::normalize(entry_dir * std::cos(ang) +
                       glm::cross(axis, entry_dir) * std::sin(ang));
    const double surf_r = hf.radius_at(u);
    drone::DroneState d = drone::spawn_drone(kAp, dp, 0, 10);
    const glm::dvec3 to_mouth0 = net.spine.front().pos - u * surf_r;
    const glm::dvec3 fwd0 =
        glm::normalize(to_mouth0 - glm::dot(to_mouth0, u) * u);
    d.curr = drone::level_state_at(dp, u * (surf_r + 80.0), fwd0);
    d.curr.velocity = fwd0 * dp.maverick.base_speed;
    d.curr.last_vhat = fwd0;
    d.prev = d.curr;
    d.grounded = false;
    d.mav.inited = true;
    d.mav.mode = maverick::MaverickState::Mode::TRANSIT;
    d.mav.run_dir = +1;

    bool engaged = false;
    for (int i = 0; i < 15 * 120; ++i) {
        const drone::DroneTickResult r = drone::tick(d, kAp, dp, &env, nullptr);
        REQUIRE_FALSE(r.respawned);  // the net holds — no auger
        engaged = engaged || d.terrain_avoid_engaged;
    }
    REQUIRE(engaged);  // the latch actually fired (non-vacuous)
    // ...and it climbed away from the deck rather than mushing along it.
    REQUIRE(glm::length(d.curr.position) -
                hf.radius_at(glm::normalize(d.curr.position)) >
            150.0);
}

// ===========================================================================
// game-AI-R2 — THE MONEY LEG: a full TRANSIT from patrol range COMPLETES (the
// R1 probe's headline failure: 178 transit attempts, 0 tunnel entries, 100%
// crashed — the chord dive augered kilometres short of the mouth every time).
// A maverick set into TRANSIT 8 km out at patrol altitude must reach the
// approach point, arm DIVE_IN, and ENTER the net (mode RUN), never crashing.
// HONEST MUTATION LEDGER (verified 2026-08-05): this composite leg does NOT
// discriminate gc_elevation alone — with the chord reverted it still
// completes, because the two-waypoint pattern + the restored TRANSIT
// terrain net absorb the chord dive (defense in depth; the R1 auger needed
// BOTH the chord AND the disarmed net). gc_elevation's own kill lives in
// the aim_at transit-range section above; the terrain net's kill in the
// TRANSIT-net leg. This leg pins the COMPOSITE outcome: a transit from
// patrol range completes into the bore.
// ===========================================================================
TEST_CASE("maverick: a transit from 8 km completes into the bore") {
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(test_tp(), &hf);
    sim::Environment env;
    env.ground = &hf;
    env.tunnels = &net;
    env.ground_params = tunnel_ground_params();

    drone::DroneParams dp = kScen.drone;
    REQUIRE(dp.maverick.enabled);

    const glm::dvec3 entry_dir = glm::normalize(net.spine.front().pos);
    const glm::dvec3 axis =
        glm::normalize(glm::cross(entry_dir, glm::dvec3{0.0, 1.0, 0.0}));
    const double ang = 8000.0 / kAp.R;
    const glm::dvec3 u =
        glm::normalize(entry_dir * std::cos(ang) +
                       glm::cross(axis, entry_dir) * std::sin(ang));
    const double surf_r = hf.radius_at(u);
    drone::DroneState d = drone::spawn_drone(kAp, dp, 0, 10);
    const glm::dvec3 to_mouth = net.spine.front().pos - u * (surf_r + 500.0);
    const glm::dvec3 fwd0 =
        glm::normalize(to_mouth - glm::dot(to_mouth, u) * u);
    d.curr = drone::level_state_at(dp, u * (surf_r + 500.0), fwd0);
    d.curr.velocity = fwd0 * dp.maverick.base_speed;
    d.curr.last_vhat = fwd0;
    d.prev = d.curr;
    d.grounded = false;
    d.mav.inited = true;
    d.mav.mode = maverick::MaverickState::Mode::TRANSIT;
    d.mav.run_dir = +1;

    bool reached_dive = false, entered = false;
    for (int i = 0; i < 360 * 120; ++i) {  // generous 360 s cap
        const int mode_before = static_cast<int>(d.mav.mode);
        const glm::dvec3 p_before = d.curr.position;
        const drone::DroneTickResult r = drone::tick(d, kAp, dp, &env, nullptr);
        INFO("tick " << i << " mode_before " << mode_before << " alt "
                     << (glm::length(p_before) - kAp.R) << " agl "
                     << (glm::length(p_before) -
                         hf.radius_at(glm::normalize(p_before)))
                     << " sd " << net.signed_distance(p_before) << " avoid "
                     << d.terrain_avoid_engaged);
        REQUIRE_FALSE(r.respawned);  // the R1 failure mode: crashed short
        if (d.mav.mode == maverick::MaverickState::Mode::DIVE_IN)
            reached_dive = true;
        if (d.mav.mode == maverick::MaverickState::Mode::RUN) {
            entered = true;  // DIVE_IN -> RUN fires exactly on net.contains
            break;
        }
    }
    REQUIRE(reached_dive);  // the approach point was actually reached
    REQUIRE(entered);       // ...and the dive threaded into the bore
}

// ===========================================================================
// game-AI-R2 AIR AUDIT (the vacuum-gap question, measured): real game.toml
// bubbles + real [tunnel] dials on the bare sphere — the WHOLE R2 transit
// pattern (fix -> final leg -> approach -> mouth, both directions, plus a
// patrol-band point behind the fix) must sit in breathable air at full
// faction strength. Measured 2026-08-05: atm_frac = 1.0 at every sample —
// the inter-bubble vacuum gap is NOT on the transit path (canon: each mouth
// sits on its bubble's proximal edge; the pattern extends INTO the bubble).
// This leg is the config-drift tripwire: a bubble/tunnel retune that starves
// the pattern (and would silently re-strand the tunnel program) fails here.
// ===========================================================================
TEST_CASE("game config: the R2 transit pattern lies in full air") {
    const cfg::GameParams game =
        cfg::load_game_toml(SEADS_CONFIG_DIR "/game.toml", kAp);
    world::TunnelParams tp;
    tp.sphere_R = kAp.R;
    tp.tube_width_m = game.tunnel.tube_width_m;
    tp.tube_height_m = game.tunnel.tube_height_m;
    tp.depth_m = game.tunnel.depth_m;
    tp.soft_m = game.tunnel.soft_m;
    tp.ramp_frac = game.tunnel.ramp_frac;
    tp.spacing_m = game.tunnel.spacing_m;
    tp.floor_height_m = game.tunnel.floor_height_m;
    tp.arena_a_m = game.tunnel.arena_a_m;
    tp.arena_c_m = game.tunnel.arena_c_m;
    tp.arena_depth_m = game.tunnel.arena_depth_m;
    tp.cavern_core_m = game.tunnel.cavern_core_m;
    tp.bowl_radius_m = game.tunnel.bowl_radius_m;
    tp.bowl_depth_m = game.tunnel.bowl_depth_m;
    tp.mouth_sink_m = game.tunnel.mouth_sink_m;
    tp.min_cover_m = game.tunnel.min_cover_m;
    tp.trench_len_m = game.tunnel.trench_len_m;
    tp.trench_rim_m = game.tunnel.trench_rim_m;
    const world::TunnelNet net = world::build_tunnel_net(tp, nullptr);

    sim::AtmosphereField af;
    af.deck_agl_m = game.atmosphere.deck_agl_m;
    af.deck_soft_m = game.atmosphere.deck_soft_m;
    const world::FactionGrowth grow[2] = {{1.0, 1.0}, {1.0, 1.0}};
    world::build_faction_bubbles(game.atmosphere, grow, af.bubbles);
    sim::Environment env;
    env.atm = &af;

    const cfg::ScenarioParams scen =
        cfg::load_scenario_toml(SEADS_CONFIG_DIR "/scenario.toml", kAp);
    const maverick::MaverickParams& mp = scen.drone.maverick;

    for (int run_dir : {+1, -1}) {
        const glm::dvec3 entry_pos =
            (run_dir > 0) ? net.spine.front().pos : net.spine.back().pos;
        const glm::dvec3 entry_dir = glm::normalize(entry_pos);
        const glm::dvec3 into_tan =
            (run_dir > 0) ? net.spine.front().tan : -net.spine.back().tan;
        const glm::dvec3 approach = entry_pos - into_tan * mp.approach_back_m +
                                    entry_dir * mp.approach_alt_m;
        const glm::dvec3 tan_h = glm::normalize(
            into_tan - glm::dot(into_tan, entry_dir) * entry_dir);
        const glm::dvec3 app_dir = glm::normalize(approach);
        const glm::dvec3 th_app =
            glm::normalize(tan_h - glm::dot(tan_h, app_dir) * app_dir);
        const glm::dvec3 fixpt = approach - th_app * mp.transit_fix_back_m;
        INFO("run_dir " << run_dir);
        REQUIRE(sim::atm_frac_at(entry_pos, &env, kAp) > 0.9);
        REQUIRE(sim::atm_frac_at(approach, &env, kAp) > 0.9);
        REQUIRE(sim::atm_frac_at(fixpt, &env, kAp) > 0.9);
        for (double f : {0.0, 0.25, 0.5, 0.75, 1.0}) {
            INFO("leg fraction " << f);
            const glm::dvec3 p = fixpt + f * (approach - fixpt);
            REQUIRE(sim::atm_frac_at(p, &env, kAp) > 0.9);
        }
        // Patrol-band sample: 2 km further back from the fix at patrol alt.
        const glm::dvec3 patrol =
            glm::normalize(fixpt - th_app * 2000.0) * (kAp.R + 800.0);
        REQUIRE(sim::atm_frac_at(patrol, &env, kAp) > 0.9);
    }
}

// ===========================================================================
// THE GAME-LOOP CERTIFICATE (game-AI-R3 + R4; Chad's rulings 2026-08-05:
// "make the ai participate in a playable game... they attack pumps... act as
// a team... they do everything... Be sure its all good before I fly it", and
// the fly report: friendlies must HELP, pumps must be DEFENDED, fights must
// RESOLVE). A full headless conquest-shaped world — terrain + tunnel net +
// all four pumps + both 5-pilot factions — driven by the REAL pure order
// functions (assign_foes / assign_defense / faction_raider — no forked
// logic) and the app's damage arbitration glue.
//
// Two DETERMINISTIC sections (one free-for-all made coverage stochastic —
// a lethal air war starves the tunnel pipeline inside a single window):
//   A. THE OBJECTIVE LOOP (player parked far): both surface pumps AND both
//      deep pumps take AI damage, tunnel runs happen, the fleet survives.
//   B. THE AIR WAR (player parked ON the enemy surface pump): defenders
//      scramble, enemies take the player as a foe, friendlies NEVER do,
//      AI-vs-AI gunnery deals real damage and KILLS.
// MUTATION: delete the strike divert -> A's deep damage vanishes; revert
// faction_raider to enemy-only -> one surface pump untouched in A; drop the
// fight-yield or the leash fight-exemption -> B's ai_damage collapses.
// ===========================================================================
TEST_CASE("game loop certificate: objectives, defense, and the air war") {
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(test_tp(), &hf);
    sim::Environment env;
    env.ground = &hf;
    env.tunnels = &net;
    env.ground_params = tunnel_ground_params();

    drone::DroneParams dp = kScen.drone;
    REQUIRE(dp.maverick.enabled);
    REQUIRE(net.arena_on);

    // Faction geography: VALLEY = the Errington (spine.front) side, SUDBURY =
    // Murray (spine.back). Surface pumps 4 km behind each mouth; deep pumps
    // exactly as main.cpp places them.
    const glm::dvec3 mouthE = net.spine.front().pos;
    const glm::dvec3 mouthM = net.spine.back().pos;
    auto home_dir = [&](const glm::dvec3& mouth, const glm::dvec3& into) {
        const glm::dvec3 up = glm::normalize(mouth);
        const glm::dvec3 th = glm::normalize(into - glm::dot(into, up) * up);
        return glm::normalize(mouth - th * 4000.0);
    };
    const glm::dvec3 homeE = home_dir(mouthE, net.spine.front().tan);
    const glm::dvec3 homeM = home_dir(mouthM, -net.spine.back().tan);
    const double lift =
        net.arena.a_pos *
        std::sqrt(1.0 - combat::kDeepPumpFrac * combat::kDeepPumpFrac);
    combat::Pump pumps[4];
    combat::make_pumps(
        homeE * (hf.radius_at(homeE) + 10.0),
        homeM * (hf.radius_at(homeM) + 10.0),
        combat::place_deep_pump(net.arena.center, net.arena.u_long,
                                glm::normalize(mouthE), net.arena.a_pos,
                                net.arena.b, combat::kDeepPumpFrac, lift),
        combat::place_deep_pump(net.arena.center, net.arena.u_long,
                                glm::normalize(mouthM), net.arena.a_pos,
                                net.arena.b, combat::kDeepPumpFrac, lift),
        60.0, pumps);
    REQUIRE(net.contains(pumps[2].pos));
    REQUIRE(net.contains(pumps[3].pos));

    // Ten pilots, each faction scattered over its own side at 600 m AGL.
    std::vector<drone::DroneState> fleet;
    for (int i = 0; i < 10; ++i) {
        const int own = combat::maverick_faction(i);
        const glm::dvec3 c = (own == combat::CQ_VALLEY) ? homeE : homeM;
        const glm::dvec3 up = glm::normalize(c);
        glm::dvec3 seed{0.0, 1.0, 0.0};
        if (std::abs(glm::dot(seed, up)) > 0.9) seed = glm::dvec3{1, 0, 0};
        const glm::dvec3 e1 = glm::normalize(glm::cross(up, seed));
        const glm::dvec3 e2 = glm::normalize(glm::cross(up, e1));
        const double brg = 2.399963 * i;
        const double r_arc = 1500.0 * ((i % 5) + 1) / 5.0;
        const glm::dvec3 u =
            glm::normalize(up * std::cos(r_arc / kAp.R) +
                           (e1 * std::cos(brg) + e2 * std::sin(brg)) *
                               std::sin(r_arc / kAp.R));
        drone::DroneState d = drone::spawn_drone(kAp, dp, i, 10);
        const glm::dvec3 fwd = glm::normalize(e1 - glm::dot(e1, u) * u);
        d.curr = drone::level_state_at(dp, u * (hf.radius_at(u) + 600.0), fwd);
        d.curr.velocity = fwd * dp.maverick.base_speed;
        d.curr.last_vhat = fwd;
        d.prev = d.curr;
        d.grounded = false;
        d.hp = dp.hp;
        fleet.push_back(d);
    }

    const int player_faction = combat::CQ_VALLEY;
    // The in-game AI-vs-AI DPS at the SHIPPED difficulty, read off the LIVE
    // table (app/instructor_tick.h's kAiVsAiDpsFrac site) -- the cert measures
    // the REAL attrition pace.
    //
    // ★ THIS WAS A WELDED COPY (8.0 x 11.0 x 0.8) and RUNG E12.3 moved the
    // table underneath it, which is the exact failure mode the ladder has now
    // hit three times (the ai_guns_on band, E8's decision table, this): a
    // constant that DESCRIBES the shipped table stops describing it the moment
    // the table moves, and nothing goes red. Single-sourced now.
    const combat::DifficultyParams kDiff =
        combat::difficulty_params(kScen.combat.difficulty);
    const double ai_dps = kDiff.bandit_rof_hz * kDiff.bandit_damage * 0.8;
    const double per_tick = 60.0 / (40.0 * 120.0);
    const combat::RaidParams rp_surface;

    struct Metrics {
        double dealt[4] = {0.0, 0.0, 0.0, 0.0};
        int respawns = 0, runs_entered = 0, ai_kills = 0;
        double ai_damage = 0.0;
        long long defend_orders = 0, friendly_on_player = 0,
                  enemy_on_player = 0;
    };
    // `air_war=false` isolates the OBJECTIVE loop (no foes assigned) — the
    // sections deliberately certify one mechanism class each; the app runs
    // both together, and the mid-map raider dogfights make combined 10-min
    // coverage stochastic (measured: whichever side's raiders tangle, that
    // side's deep-strike traffic starves for the window).
    // `strikes` isolates the sections the same way `air_war` does (the
    // header's warning made real: with the E2 on-order pipeline actually
    // WORKING, strike sorties starve the air war inside one window — pre-E2
    // the strikes were ineffective, so B was isolated only de facto).
    // A certifies the objective loop (strikes on, air war off); B certifies
    // defense + gunnery (air war on, strikes off).
    auto run_world = [&](const sim::SimState& player, int minutes,
                         bool air_war, bool strikes) {
        Metrics m;
        for (int t = 0; t < minutes * 60 * 120; ++t) {
            if (air_war) {
                combat::assign_foes(fleet, player, player_faction, 2, dp);
                combat::assign_defense(fleet, pumps, player, player_faction,
                                       2500.0, 2);
            }
            for (const drone::DroneState& d : fleet) {
                if (d.defend.active) ++m.defend_orders;
                if (d.foe == drone::kFoePlayer) {
                    if (d.friendly_side)
                        ++m.friendly_on_player;
                    else
                        ++m.enemy_on_player;
                }
            }
            for (drone::DroneState& d : fleet) {
                const int ef = 1 - combat::maverick_faction(d.spawn_index);
                drone::RaidOrder ro;
                if (combat::faction_raider(d.spawn_index) && pumps[ef].alive) {
                    // The friendly gate + the hysteretic player-range latch
                    // (mirrors the app orders block).
                    const double pr =
                        glm::length(player.position - d.curr.position);
                    d.raid_range_ok = d.raid_range_ok
                                          ? (pr > dp.engage_range)
                                          : (pr > dp.disengage_range);
                    if (d.friendly_side || d.raid_range_ok) {
                        ro.active = true;
                        ro.target_pos = pumps[ef].pos;
                        ro.pump_idx = ef;
                    }
                }
                d.raid = ro;
                // Mirrors the app's E2 stamping (instructor_tick.h): the
                // order carries the CONFIG strike envelope + the derived
                // own-mouth entry direction, never the struct defaults — a
                // default order here re-welds the pre-E2 35 deg/90 s/600 m
                // envelope into the certificate.
                drone::StrikeOrder so;
                so.engage_m = dp.strike_engage_m;
                so.k_az = dp.strike_k_az;
                so.k_el = dp.strike_k_el;
                so.bank_cap = dp.strike_bank_cap;
                so.gamma_cap = dp.strike_gamma_cap;
                so.bail_s = dp.strike_bail_s;
                so.station_range_m = dp.strike_station_range_m;
                so.station_cos = dp.strike_station_cos;
                if (strikes && pumps[2 + ef].alive) {
                    so.active = true;
                    so.target_pos = pumps[2 + ef].pos;
                    so.pump_idx = 2 + ef;
                    const int own = combat::maverick_faction(d.spawn_index);
                    const glm::dvec3& own_deep = pumps[2 + own].pos;
                    const double d_front =
                        glm::length(net.spine.front().pos - own_deep);
                    const double d_back =
                        glm::length(net.spine.back().pos - own_deep);
                    so.entry_dir = d_front <= d_back ? +1 : -1;
                }
                d.strike = so;

                const bool was_run =
                    d.mav.mode == maverick::MaverickState::Mode::RUN;
                const sim::SimState* target = nullptr;
                if (d.foe == drone::kFoePlayer)
                    target = &player;
                else if (d.foe >= 0 && d.foe < static_cast<int>(fleet.size()))
                    target = &fleet[d.foe].curr;
                const drone::DroneTickResult r =
                    drone::tick(d, kAp, dp, &env, target);
                if (r.respawned) ++m.respawns;
                if (!was_run &&
                    d.mav.mode == maverick::MaverickState::Mode::RUN)
                    ++m.runs_entered;

                // Damage arbitration glue (mirrors app block 4 + 6c).
                combat::RaidParams rp_deep;
                int pi = -1;
                const combat::RaidParams* rp = nullptr;
                if (d.raid.active && !maverick::is_tunnel_mode(d.mav.mode)) {
                    pi = d.raid.pump_idx;
                    rp = &rp_surface;
                } else if (d.strike.active && !d.strike_bailed &&
                           d.mav.mode == maverick::MaverickState::Mode::RUN) {
                    pi = d.strike.pump_idx;
                    // Per-drone, from the order it actually flew (mirrors
                    // instructor_tick.h -- a default reproduces
                    // combat::strike_params()).
                    rp_deep = combat::strike_params(d.strike.station_range_m,
                                                    d.strike.station_cos);
                    rp = &rp_deep;
                }
                if (pi >= 0 && pumps[pi].alive &&
                    combat::raider_on_station(d.curr, pumps[pi].pos, *rp)) {
                    pumps[pi].hp -= per_tick;
                    m.dealt[pi] += per_tick;
                    if (pumps[pi].hp <= 0.0) pumps[pi].alive = false;
                }
                if (d.engaged && d.foe >= 0 &&
                    d.foe < static_cast<int>(fleet.size()) &&
                    combat::ai_guns_on(d.curr, fleet[d.foe].curr,
                                       dp.fire_range_min,
                                       dp.fire_range_max)) {
                    drone::DroneState& victim = fleet[d.foe];
                    victim.hp -= ai_dps / 120.0;
                    m.ai_damage += ai_dps / 120.0;
                    if (victim.hp <= 0.0) {
                        ++m.ai_kills;
                        drone::respawn_in_place(victim, kAp, dp);
                        victim.hp = dp.hp;
                    }
                }
            }
        }
        return m;
    };

    SECTION("A: the objective loop (player far)") {
        const glm::dvec3 far_up = glm::normalize(-glm::normalize(mouthE));
        const sim::SimState player = drone::level_state_at(
            dp, far_up * (kAp.R + 1000.0),
            glm::normalize(glm::cross(far_up, glm::dvec3{0.0, 1.0, 0.0})));
        const Metrics m = run_world(player, 10, /*air_war=*/false, /*strikes=*/true);
        INFO("dealt V/S surface " << m.dealt[0] << "/" << m.dealt[1] << " deep "
                                  << m.dealt[2] << "/" << m.dealt[3]
                                  << " respawns " << m.respawns << " runs "
                                  << m.runs_entered);
        // BOTH factions raided the other's surface pump — sustained pressure.
        REQUIRE(m.dealt[0] > 2.0);
        REQUIRE(m.dealt[1] > 2.0);
        // BOTH deep pumps were strafed in the stope (strike works both ways).
        REQUIRE(m.dealt[2] > 1.0);
        REQUIRE(m.dealt[3] > 1.0);
        // Tunnel runs really happen alongside the objectives (the stronger
        // both-factions cross-check is the deep pair above).
        REQUIRE(m.runs_entered >= 2);
        // The fleet does not attrit itself into irrelevance.
        REQUIRE(m.respawns < 40);
        // A friendly NEVER takes the player as a foe, in any section.
        REQUIRE(m.friendly_on_player == 0);
    }

    SECTION("B: the air war (player attacking the enemy surface pump)") {
        const glm::dvec3 player_up = glm::normalize(pumps[1].pos);
        glm::dvec3 pseed{0.0, 1.0, 0.0};
        if (std::abs(glm::dot(pseed, player_up)) > 0.9)
            pseed = glm::dvec3{1, 0, 0};
        const glm::dvec3 poff = glm::normalize(glm::cross(player_up, pseed));
        const glm::dvec3 pu =
            glm::normalize(player_up * std::cos(1500.0 / kAp.R) +
                           poff * std::sin(1500.0 / kAp.R));
        const sim::SimState player = drone::level_state_at(
            dp, pu * (hf.radius_at(pu) + 700.0),
            glm::normalize(poff - glm::dot(poff, pu) * pu));
        const Metrics m = run_world(player, 6, /*air_war=*/true, /*strikes=*/false);
        INFO("ai_damage " << m.ai_damage << " ai_kills " << m.ai_kills
                          << " defend " << m.defend_orders << " enemy_on_p "
                          << m.enemy_on_player << " friendly_on_p "
                          << m.friendly_on_player);
        // Enemies actually take the player as a foe; friendlies NEVER do.
        REQUIRE(m.enemy_on_player > 0);
        REQUIRE(m.friendly_on_player == 0);
        // The threatened faction scrambles pump defenders.
        REQUIRE(m.defend_orders > 0);
        // Real AI-vs-AI fighting with real attrition. The KILL floor is
        // deliberately absent until the open kill-chain rung lands (section
        // C's ledger): the same weak pursuit-execution layer that cannot
        // gun a parked player keeps AI-vs-AI windows too brief to close a
        // 100 hp kill reliably in six minutes. Restore REQUIRE(ai_kills >=
        // 1) with that rung.
        REQUIRE(m.ai_damage > 50.0);
    }

    // C — THE KILL CHAIN (Chad's order 2026-08-05: "I ordered for ai that
    // can kill me"). RESOLVED and GATED: shipped-difficulty attackers vs a
    // physically-parked player through the REAL fire pipeline must deal
    // heavy damage inside three minutes (measured: they kill it). The chain
    // was rebuilt link by link, every claim measured (the full evidence
    // trail is docs/killchain_handoff.md): fire cone 5 -> 14 deg; merges no
    // longer stolen by the tunnel scheduler / the leash / raid orders;
    // corner-speed turnarounds; and the R5 core — the round leaves on a
    // TRUE ballistic solve (gravity + the round's own drag decaying the
    // INHERITED velocity component too — the measured 15 m crossing-shot
    // error in bfm::lead_point) slewed up to 10 deg off boresight, with a
    // 0.4 deg deterministic spray for tracer texture. The final 28 m
    // mystery was the FIXTURE: level_state_at stamps cruise velocity into a
    // never-stepped state and the (correct) solver led the phantom motion —
    // a parked target must carry zero velocity.
    SECTION("C kill chain") {
        // The REAL fire pipeline (enemy_fire_tick -> combat_player_tick,
        // the shipped [combat] setup incl. the R4 fire_cone retune) against
        // a PARKED player near the enemy home: if the AI cannot kill a
        // stationary plane, it cannot kill anyone. Passing = substantial hp
        // damage lands inside three minutes.
        combat::CombatWorld cw;
        cw.setup = kScen.combat;
        cw.player_invuln_ticks = 0;
        // Parked at the fleet's own cruise altitude (600 AGL): the pursuit
        // gamma cap (30 deg) makes a target far ABOVE structurally
        // un-gunnable near the merge, and the terrain-avoid guard forbids
        // pointed dives at a DECK target — both real, noted envelope
        // limits. This leg measures the co-altitude gun chain, the case
        // Chad's dogfights actually live in.
        const glm::dvec3 up = glm::normalize(homeM);
        sim::SimState player = drone::level_state_at(
            dp, up * (hf.radius_at(up) + 600.0),
            glm::normalize(glm::cross(up, glm::dvec3{0.0, 1.0, 0.0})));
        // PHYSICALLY parked: level_state_at stamps cruise velocity into
        // the state, but this fixture never steps it — a mover that
        // never moves. The ballistic solver (correctly) led that phantom
        // velocity 60-90 m ahead of the eternal position: the ENTIRE
        // measured 28-80 m miss saga was this fixture artifact (the
        // inverse of the fixture-no-op class — a fixture-manufactured
        // defect). A real in-game player always moves as its velocity
        // says.
        player.velocity = glm::dvec3{0.0};
        double dmg_taken = 0.0;
        double true_min_miss = 1e18;
        for (int t = 0; t < 3 * 60 * 120; ++t) {
            combat::assign_foes(fleet, player, player_faction, 3, dp);
            for (drone::DroneState& d : fleet) {
                const sim::SimState* target = nullptr;
                if (d.foe == drone::kFoePlayer)
                    target = &player;
                else if (d.foe >= 0 && d.foe < static_cast<int>(fleet.size()))
                    target = &fleet[d.foe].curr;
                drone::tick(d, kAp, dp, &env, target);
            }
            combat::enemy_fire_tick(cw, fleet, kAp.sim_dt, kAp.g, kAp.R, &net);
            for (const weapon::Projectile& pr : cw.enemy_pool) {
                if (!pr.active) continue;
                const glm::dvec3 seg = pr.pos - pr.prev_pos;
                const double dd = glm::dot(seg, seg);
                const double tt =
                    dd > 1e-12
                        ? std::clamp(
                              glm::dot(player.position - pr.prev_pos, seg) / dd,
                              0.0, 1.0)
                        : 0.0;
                const double cdist =
                    glm::length(player.position - (pr.prev_pos + seg * tt));
                if (cdist < true_min_miss) true_min_miss = cdist;
            }
            const double hp0 = combat::summary_hp(cw.damage);
            combat::combat_player_tick(cw, player, player);
            dmg_taken += hp0 - combat::summary_hp(cw.damage);
            cw.player_invuln_ticks = 0;  // no respawn loop in the fixture
            if (combat::is_dead(cw.damage)) break;  // shredded: proven
        }
        INFO("player damage taken " << dmg_taken << " hits " << cw.hits
                                    << " true_min_miss " << true_min_miss);
        // The kill chain is real: a parked plane takes heavy fire. (A dead
        // player is the expected outcome; the floor tolerates KE variance.)
        REQUIRE(dmg_taken > 30.0);
    }
}
