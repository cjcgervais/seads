// FIX-F4 (docs/ai_phase2_fix_spec.md, Opus probe attribution
// D:\seads_sandboxes\ai-probes\f4\ATTRIBUTION.md): TUNNEL-RUN LETHALITY.
//
// PROBE FACT: 69% of net entries died, 24/25 on the Errington bore ramp.
// Mechanism: DIVE_IN crosses the mouth ~30 deg nose-low at 173-183 m/s; the
// pull-out is flown BANKED, so the track rotates 3.7 deg -> 10 deg off the
// bore axis while cross-track is still ~0 (an entry TRACK-ANGLE error, not a
// lateral position error). The roll command saturates at bank_cap for 2.5 s
// because lookahead_m=220 (~334 m effective at speed) is only 0.21x the
// achievable turn radius (1574 m at 182 m/s / 65 deg bank); cross-track runs
// out to exactly tube_width_m and the plane exits contains() 580-720 m in.
//
// THE BLIND SPOT (probe section 6b): test_maverick.cpp's own dive_in_drone
// fixture enters EXACTLY on the bore tangent with ZERO cross-track -- a
// measure-zero perfect entry that is 0/10 fatal. Offsetting the fixture's
// APPROACH POINT laterally by as little as 40 m (which converges the
// approach heading onto the true mouth at an angle -- exactly the track-
// angle error the trace shows, not a parallel lateral shift) is 20/20 fatal
// under BOTH the real and certificate tunnel dials, while TRANSIT's own arm
// gate (transit_line_tol_m = 300 m) legally admits exactly this much
// cross-track before ever handing off to DIVE_IN. That is the certificate
// gap this TU closes.
//
// THE FIX (config-only, docs/ai_phase2_fix_spec.md): config/scenario.toml
// [maverick] lookahead_m 220->500, centerline_gain 2.0->1.0.
//
// This TU is standalone from test_maverick.cpp (off-limits per the spec) and
// mirrors its dive_in_drone fixture, its test_tp() tunnel dials, and its
// OPEN-LOOP/closed-loop-run discipline. TEST_CASE names are STRICTLY ASCII
// (the repo's own non-ASCII-name trap: such a name silently never runs under
// ctest).

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <cstdint>
#include <glm/glm.hpp>

#include "config/load_aircraft.h"
#include "config/load_scenario.h"
#include "drone/drone.h"
#include "drone/maverick.h"
#include "sim/environment.h"
#include "sim/state.h"
#include "sim/world.h"
#include "world/heightfield.h"
#include "world/tunnel_net.h"

#ifdef NDEBUG
#error "SEADS gate requires an assert-live build (SPEC 6.1)"
#endif

namespace {

constexpr double kPi = 3.14159265358979323846;

const sim::AircraftParams kAp =
    cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");

// The committed maverick tuning (picks up the FIX-F4 [maverick] dials from
// the shipped scenario.toml).
const cfg::ScenarioParams kScen =
    cfg::load_scenario_toml(SEADS_CONFIG_DIR "/scenario.toml", kAp);

// Uniform terrain (the test_maverick.cpp / test_tunnel.cpp fixture,
// duplicated here since that TU is off-limits).
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

// The T1 canon tunnel dials (test_maverick.cpp's own test_tp(), duplicated
// here -- the probe (ATTRIBUTION.md section 6a) proved the real game.toml
// [tunnel] dials and this certificate set die at the IDENTICAL rate, tick
// for tick, on this exact mechanism: "the historic certificates never ran
// the real dials gap is real but is not the bug. Running them on the real
// dials changes nothing." So test_tp() is a faithful stand-in.
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

// test_maverick.cpp's dive_in_drone, PARAMETERIZED over the entry
// cross-track offset (the certificate blind spot, ATTRIBUTION.md 6b): the
// APPROACH point is displaced `lat_offset_m` along the lateral direction at
// the mouth (perpendicular to both the bore tangent and the local radial),
// with the TRUE mouth position (`entry`) held fixed. The approach heading
// (`fwd`, computed from entry - approach) therefore converges onto the
// mouth at an angle for any nonzero offset -- exactly the entry TRACK-ANGLE
// error the probe's trace shows (a track error with near-zero cross-track
// AT the mouth, growing as DIVE_IN is flown), reproducing the probe's own
// sweep methodology verbatim (section 6b: "re-flies that fixture verbatim
// while sweeping the approach point laterally"). A pure translation of the
// approach point with the heading held parallel to the tangent (no
// convergence) was tried first and rejected: it does not reproduce a real
// TRANSIT->DIVE_IN handoff geometry at all -- offset by more than a few
// tube-widths it flies parallel to the bore, at approach altitude, well
// wide of the narrow entry pit, and crashes into ordinary terrain outside
// any tunnel structure before ever registering net.contains() (entered
// stayed false at every nonzero offset) -- the wrong fixture entirely,
// not evidence about the mechanism.
drone::DroneState dive_in_drone_offset(const drone::DroneParams& dp,
                                       const world::TunnelNet& net,
                                       int run_dir, int idx,
                                       double lat_offset_m) {
    const maverick::MaverickParams& mp = dp.maverick;
    const glm::dvec3 entry =
        (run_dir > 0) ? net.spine.front().pos : net.spine.back().pos;
    const glm::dvec3 edir = glm::normalize(entry);
    const glm::dvec3 into_tan =
        (run_dir > 0) ? net.spine.front().tan : -net.spine.back().tan;
    glm::dvec3 lateral = glm::cross(edir, into_tan);
    const double lateral_len = glm::length(lateral);
    lateral = lateral_len > 1e-9 ? lateral / lateral_len
                                 : glm::dvec3{0.0, 0.0, 1.0};  // degenerate guard
    const glm::dvec3 approach = entry - into_tan * mp.approach_back_m +
                                edir * mp.approach_alt_m +
                                lateral * lat_offset_m;
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
    d.mav.s_est = (run_dir > 0) ? 0.0 : maverick::TunnelRoute(net).L;
    return d;
}

struct RunOutcome {
    bool entered = false;
    bool crashed = false;
    bool reached_patrol = false;
};

// Flies the offset-entry DIVE_IN run to a respawn (crash), a clean PATROL
// hand-off, or the generous time cap -- whichever comes first. Mirrors
// test_maverick.cpp's (b)/(b2) closed-loop run structure.
RunOutcome fly_offset_run(const drone::DroneParams& dp,
                          const world::TunnelNet& net,
                          const sim::Environment& env, int run_dir,
                          double lat_offset_m, int idx) {
    drone::DroneState d = dive_in_drone_offset(dp, net, run_dir, idx, lat_offset_m);
    RunOutcome out;
    for (int i = 0; i < 300 * 120; ++i) {  // generous cap (~300 s)
        const drone::DroneTickResult r = drone::tick(d, kAp, dp, &env, nullptr);
        if (net.contains(d.curr.position)) out.entered = true;
        if (r.respawned) {
            out.crashed = true;
            return out;
        }
        if (d.mav.mode == maverick::MaverickState::Mode::PATROL && i > 200) {
            out.reached_patrol = true;
            return out;
        }
    }
    return out;
}

}  // namespace

// ---------------------------------------------------------------------------
// The offset-entry legs, under the SHIPPED (FIX-F4) [maverick] dials. All
// three fly the Errington -> Murray direction (run_dir = +1) -- the probe's
// own fatal direction (24/25 deaths; the Murray -> Errington leg has a
// 450 m open bowl and is comparatively forgiving).
// ---------------------------------------------------------------------------
// Red-team P2-1 (VERDICT.md): renamed from "...survive under the FIX-F4
// dials" -- that name overclaimed for the 120 m SECTION, whose own body and
// comment already disclose the offset matrix's finding (idx 0/2 still crash
// at 120 m; only idx 1/SHAFT survives). The name now matches what a bare
// `ctest` line actually promises.
TEST_CASE(
    "maverick tunnel run: off-axis Errington entries survive at 0/40 m; "
    "80 m survives for EVERY trait (FIX-F4b)") {
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(test_tp(), &hf);
    sim::Environment env;
    env.ground = &hf;
    env.tunnels = &net;
    env.ground_params = tunnel_ground_params();

    const drone::DroneParams dp = kScen.drone;  // the shipped tuning
    REQUIRE(dp.maverick.enabled);  // premise: the ship config runs mavericks
    // Non-vacuous: the shipped table actually carries the FIX-F4 dials (not
    // a stale cached load of the pre-fix values).
    REQUIRE(dp.maverick.lookahead_m == Catch::Approx(500.0));
    REQUIRE(dp.maverick.centerline_gain == Catch::Approx(1.0));
    REQUIRE(dp.maverick.dive_lookahead_m == Catch::Approx(250.0));  // FIX-F4b

    SECTION("0 m offset (the legacy on-axis certificate arm) still survives") {
        const RunOutcome out = fly_offset_run(dp, net, env, +1, 0.0, 0);
        REQUIRE(out.entered);
        REQUIRE_FALSE(out.crashed);
    }
    SECTION("40 m offset survives (100% fatal pre-fix, per the probe)") {
        const RunOutcome out = fly_offset_run(dp, net, env, +1, 40.0, 1);
        REQUIRE(out.entered);
        REQUIRE_FALSE(out.crashed);
    }
    SECTION(
        "80 m offset survives for EVERY trait (the FIX-F4b envelope "
        "witness)") {
        // Premise: TRANSIT's arm gate legally admits this much cross-track
        // before ever handing off to DIVE_IN -- 80 m is not an exotic
        // stress value, it is inside what the mode machine itself produces.
        REQUIRE(dp.maverick.transit_line_tol_m >= 80.0);
        // FIX-F4b RE-DERIVATION (dive_lookahead_m 150 -> 250; Opus
        // adjudication, docs/ai_phase2_records/f4b_offset_witness_
        // adjudication.md). The old witness (120 m, idx=1/SHAFT only) was
        // the ONLY trait-dependent cell in either dial's survival matrix --
        // a fixture calibration to the old dial, not a mechanism pin. Under
        // 250 the matrix is trait-INdependent: all three traits survive
        // 0-80 m and crash 100-140 m (boundary pinned to 1 m: 117 m old ->
        // 82 m new in THIS fixture). That 30% envelope shrink is a measured
        // trade, ruled on fleet primary data over the real DEM (150 min:
        // total in-net deaths 32% -> 8%, the pit-FLOOR class 19 -> 0 --
        // docs/ai_phase2_records/f4b_pitcrown_attribution.md). Note the
        // fixture-reference nuance the adjudication measured: this
        // fixture's entry nose is welded to the MOUTH NODE, the one legal
        // reference where a longer lead banks MORE at t0 (+51%); off the
        // bore TANGENT (what a real TRANSIT hand-off produces,
        // transit_align_min = 0.85) the lead banks LESS (-9%), which is
        // the desaturation the fleet measured. The residual off-axis fatal
        // envelope inside transit_line_tol_m = 300 remains a disclosed,
        // real finding (it already existed under FIX-F4: idx 0/2 died at
        // 120 m), owned by the fleet-level probes, not this per-drone leg.
        for (int idx = 0; idx < 3; ++idx) {
            const RunOutcome out = fly_offset_run(dp, net, env, +1, 80.0, idx);
            REQUIRE(out.entered);
            REQUIRE_FALSE(out.crashed);
        }
    }
}

// ---------------------------------------------------------------------------
// ANTI-VACUITY MUTATION PIN: a LOCAL MaverickParams copy with the OLD dials
// (never touching the shipped config) must still kill the 40 m-offset entry
// -- proving the legs above actually exercise the mechanism FIX-F4 killed,
// not some unrelated tunnel-net leniency. MUTATION: if this leg ever stops
// crashing, the fixture has drifted away from the probe's mechanism and the
// three "survives" legs above are no longer meaningful evidence of the fix.
// ---------------------------------------------------------------------------
TEST_CASE(
    "maverick tunnel run: the OLD lookahead/centerline dials still kill a "
    "40 m off-axis entry (anti-vacuity)") {
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(test_tp(), &hf);
    sim::Environment env;
    env.ground = &hf;
    env.tunnels = &net;
    env.ground_params = tunnel_ground_params();

    drone::DroneParams dp = kScen.drone;      // start from the shipped tuning
    dp.maverick.lookahead_m = 220.0;          // ...then roll back JUST these
    dp.maverick.centerline_gain = 2.0;        // two dials, locally, in-test
    REQUIRE(dp.maverick.enabled);

    const RunOutcome out = fly_offset_run(dp, net, env, +1, 40.0, 0);
    REQUIRE(out.entered);   // premise: it did dive into the bore
    REQUIRE(out.crashed);   // ...and the OLD dials still kill it
}
