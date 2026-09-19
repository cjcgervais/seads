// The S8-drone target drone (docs/TEACHING.md Addendum A.2): a second plant
// instance flown by a dedicated bank-hold autopilot (S8-drone follow-up). Pins
// the PURE pieces (drone/drone.h) with mutation-verified cases — the drone
// touches the sphere invariants + frame-correctness, so each test names the
// defect it would catch and the value that defect produces.

#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>

#include "app/instructor_tick.h"  // app::spawn_state — the player-spawn oracle
#include "config/load_aircraft.h"
#include "config/load_controller.h"
#include "control/extract.h"
#include "drone/drone.h"
#include "render/rig.h"  // rig-B: apply_deflection (the render-only firewall leg)
#include "sim/world.h"

#ifdef NDEBUG
#error "SEADS gate requires an assert-live build (SPEC 6.1)"
#endif

namespace {

const sim::AircraftParams kAp =
    cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");
const control::ControllerParams kCp =
    cfg::load_controller_toml(SEADS_CONFIG_DIR "/controller.toml", kAp);

bool finite3(const glm::dvec3& v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

}  // namespace

// The flight PROGRAM schedule (a pure function of the tick age): straight legs
// read 0, turn legs read +/-turn_bank with the sign ALTERNATING each turn, and
// turn_time == 0 disables it. MUTATION: drop the alternation (always
// +turn_bank)
// -> the second turn's sign check fails; return turn_bank in the straight leg
// -> the straight check fails.
TEST_CASE("drone::program_bank: straight legs, then alternating turns") {
    drone::DroneParams dp;
    dp.straight_time = 1.0;  // 120 ticks straight
    dp.turn_time = 1.0;      // 120 ticks turn; cycle = 240
    dp.turn_bank = 0.3;
    const double dt = kAp.sim_dt;  // 1/120

    REQUIRE(drone::program_bank(0, dp, dt) == Catch::Approx(0.0));   // straight
    REQUIRE(drone::program_bank(60, dp, dt) == Catch::Approx(0.0));  // straight
    REQUIRE(drone::program_bank(180, dp, dt) ==
            Catch::Approx(0.3));  // turn 0 (+)
    REQUIRE(
        drone::program_bank(420, dp, dt) ==
        Catch::Approx(-0.3));  // turn 1 (-): cycle 240, tick 420 in 2nd turn
    REQUIRE(drone::program_bank(660, dp, dt) ==
            Catch::Approx(0.3));  // turn 2 (+): back to positive

    // Disabled -> pure straight.
    drone::DroneParams none = dp;
    none.turn_time = 0.0;
    REQUIRE(drone::program_bank(180, none, dt) == Catch::Approx(0.0));
}

auto extract_drone = [](const drone::DroneState& s,
                        const sim::AircraftParams& ap) {
    return control::extract(s.curr, s.curr.last_vhat, ap.v_dir_eps);
};

// The bank-hold AUTOPILOT holds the commanded bank + level flight +
// COORDINATION, steadily (no oscillation), and stays airborne. This is the
// whole point of the gentle-turn rework. MUTATIONS: flip the roll sign (kBankP)
// -> bank runs away from target -> the phi check fails; flip the pitch sign
// (kLevelP) -> climbs/ dives -> the |sin gamma| check fails; flip the yaw sign
// (kCoordP) -> sideslip diverges (the plant has NO aero yaw stability,
// sim/step.cpp) -> the beta check fails (red-team P1).
TEST_CASE("drone::autopilot: holds a steady gentle bank, level, coordinated") {
    drone::DroneParams dp;
    dp.straight_time = 0.0;  // always the turn leg -> hold +turn_bank
    dp.turn_time = 100.0;
    dp.turn_bank = 0.40;  // ~23 deg gentle bank
    drone::DroneState d = drone::spawn_drone(kAp, dp, 0, 1);

    for (int i = 0; i < 1200; ++i) {  // 10 s to settle
        drone::tick(d, kAp, dp, nullptr);
        REQUIRE(sim::altitude(d.curr.position, kAp) > 0.0);  // never crashes
        REQUIRE(std::abs(glm::length(d.curr.orientation) - 1.0) < 1e-9);
    }
    const control::Extracted e = extract_drone(d, kAp);
    // Held the commanded +bank (right wing down) within a few degrees.
    REQUIRE(e.phi == Catch::Approx(0.40).margin(0.10));
    // Roughly level: |sin(flight-path angle)| small (holds altitude in the
    // turn).
    REQUIRE(std::abs(glm::dot(e.vhat, e.local_up)) < 0.15);
    // Coordinated: sideslip stays small (a flipped yaw sign diverges it).
    REQUIRE(std::abs(e.beta) < 0.06);  // < ~3.4 deg
    // No limit cycle: bank barely moves over the next second.
    double lo = 1e9, hi = -1e9;
    for (int i = 0; i < 120; ++i) {
        drone::tick(d, kAp, dp, nullptr);
        const double phi = extract_drone(d, kAp).phi;
        lo = std::min(lo, phi);
        hi = std::max(hi, phi);
    }
    REQUIRE(hi - lo < 0.05);  // steady, no oscillation
}

// Coordination directly: seed a sideslip (yaw the airframe off its velocity)
// and confirm the autopilot NULLS it. MUTATION: flip the yaw sign (kCoordP) ->
// the sideslip GROWS instead of shrinking (positive feedback, no aero
// restoring).
TEST_CASE("drone::autopilot: coordination nulls a seeded sideslip") {
    drone::DroneParams dp;
    dp.turn_time = 0.0;  // straight (target bank 0) so only coordination acts
    drone::DroneState d = drone::spawn_drone(kAp, dp, 0, 1);
    // Yaw the body ~8 deg about local_up (wings stay level at spawn: body_up ==
    // local_up), so the nose points off the velocity -> a real sideslip.
    const glm::dvec3 up = sim::local_up(d.curr.position);
    d.curr.orientation =
        glm::normalize(glm::angleAxis(0.14, up) * d.curr.orientation);
    const double beta0 = std::abs(extract_drone(d, kAp).beta);
    REQUIRE(beta0 > 0.10);  // the seed took

    for (int i = 0; i < 480; ++i) drone::tick(d, kAp, dp, nullptr);  // 4 s
    const double beta1 = std::abs(extract_drone(d, kAp).beta);
    REQUIRE(beta1 < 0.4 * beta0);  // coordination pulled it toward zero
}

// Stability at the CONFIG CEILING (45 deg) and at LOW speed (the S6 "overdamped
// at one q" trap, red-team P2): the autopilot must still hold the bank without
// oscillation and stay airborne where the plant authority is weakest (q -> the
// floor). Justifies the loader's 45 deg cap.
TEST_CASE("drone::autopilot: stable at the 45 deg ceiling and low speed") {
    drone::DroneParams dp;
    dp.straight_time = 0.0;
    dp.turn_time = 100.0;
    dp.turn_bank = 45.0 * 3.14159265358979323846 / 180.0;  // the loader ceiling
    dp.speed = 90.0;                                       // low q
    drone::DroneState d = drone::spawn_drone(kAp, dp, 0, 1);
    const double alt0 = sim::altitude(d.curr.position, kAp);

    for (int i = 0; i < 1800; ++i) {  // 15 s
        drone::tick(d, kAp, dp, nullptr);
        REQUIRE(sim::altitude(d.curr.position, kAp) > 0.0);
        REQUIRE(std::abs(glm::length(d.curr.orientation) - 1.0) < 1e-9);
        // Sustainable at the ceiling: holds altitude (Fable P2-1), not just
        // alive.
        REQUIRE(std::abs(sim::altitude(d.curr.position, kAp) - alt0) < 250.0);
    }
    const control::Extracted e = extract_drone(d, kAp);
    REQUIRE(e.phi ==
            Catch::Approx(dp.turn_bank).margin(0.12));  // holds the bank
    double lo = 1e9, hi = -1e9;
    for (int i = 0; i < 120; ++i) {
        drone::tick(d, kAp, dp, nullptr);
        const double phi = extract_drone(d, kAp).phi;
        lo = std::min(lo, phi);
        hi = std::max(hi, phi);
    }
    REQUIRE(hi - lo < 0.05);  // no oscillation at the ceiling / low q
}

// The level-hold is FEEDBACK on gamma (no curvature feedforward), so a long
// patrol could in principle drift into the ground (red-team P2). Pin that it
// does NOT: over a full-minute default patrol the altitude stays in a band (a
// slow bounded climb to the drag-limited speed at throttle 1.0, never a sink).
TEST_CASE("drone::autopilot: a long patrol holds an altitude band") {
    drone::DroneParams dp;  // default 8 s straight / 4 s turn / 25 deg
    drone::DroneState d = drone::spawn_drone(kAp, dp, 0, 1);
    const double alt0 = sim::altitude(d.curr.position, kAp);
    for (int i = 0; i < 7200; ++i) {  // 60 s
        drone::tick(d, kAp, dp, nullptr);
        const double alt = sim::altitude(d.curr.position, kAp);
        REQUIRE(alt > alt0 - 300.0);  // never drifts down into the ground
        REQUIRE(alt < alt0 + 800.0);  // and the climb is bounded
    }
}

// ===========================================================================
// spawn_state — sphere-correct airborne birth: on the shell at R + spawn_alt,
// wings level (body-up == local_up), nose along the velocity, unit quaternion.
// And at zero offset it REPRODUCES the player spawn (app::spawn_state) — the
// oracle keeping the two in sync by construction (no upward include into app/).
// ===========================================================================
TEST_CASE("drone::spawn_state: on the shell, level, nose along velocity") {
    drone::DroneParams dp;  // default offsets (ahead + beside)
    const sim::SimState s = drone::spawn_state(kAp, dp, 0, 1);  // drone 0

    REQUIRE(glm::length(s.position) ==
            Catch::Approx(kAp.R + dp.spawn_alt).margin(1e-6));
    REQUIRE(std::abs(glm::length(s.orientation) - 1.0) ==
            Catch::Approx(0.0).margin(1e-12));

    const glm::dvec3 up = sim::local_up(s.position);
    const glm::dvec3 body_up = s.orientation * glm::dvec3{0.0, 1.0, 0.0};
    const glm::dvec3 nose = s.orientation * glm::dvec3{0.0, 0.0, -1.0};
    const glm::dvec3 vdir = glm::normalize(s.velocity);
    REQUIRE(glm::dot(body_up, up) == Catch::Approx(1.0).margin(1e-9));  // level
    REQUIRE(glm::dot(nose, vdir) ==
            Catch::Approx(1.0).margin(1e-9));  // nose||v
    REQUIRE(s.throttle == Catch::Approx(dp.throttle));
    // Placed OFF the player spawn (so Chad sees a separate bandit).
    REQUIRE(glm::length(s.position - app::spawn_state(kAp).position) > 100.0);
}

TEST_CASE(
    "drone::spawn_state: drone 0 at zero offset reproduces player spawn") {
    drone::DroneParams dp;
    dp.spawn_ahead = 0.0;
    dp.spawn_side = 0.0;
    dp.spawn_alt = 2000.0;
    dp.speed = 140.0;
    dp.throttle = 1.0;
    const sim::SimState d = drone::spawn_state(kAp, dp, 0, 1);
    const sim::SimState p = app::spawn_state(kAp);
    REQUIRE(glm::length(d.position - p.position) ==
            Catch::Approx(0.0).margin(1e-6));
    REQUIRE(glm::length(d.velocity - p.velocity) ==
            Catch::Approx(0.0).margin(1e-6));
    // Orientation: same rotation (dot of quats ~ +/-1).
    REQUIRE(std::abs(glm::dot(d.orientation, p.orientation)) ==
            Catch::Approx(1.0).margin(1e-9));
}

// The FLEET scatters to distinct, valid, level, on-shell states clustered
// WITHIN spread_m of the spawn sub-point (+X) so it is a visible squadron.
// MUTATION: collapse the scatter (return the index-0 placement for every slot)
// -> distinct-position fails; a wrong spread (whole-sphere) -> the
// within-spread bound fails.
TEST_CASE("drone::spawn_state: the fleet clusters within spread_m of spawn") {
    drone::DroneParams dp;
    const int n = 10;
    const glm::dvec3 spawn_dir{1.0, 0.0, 0.0};  // +X sub-point
    std::vector<glm::dvec3> ps;
    for (int i = 0; i < n; ++i) {
        const sim::SimState s = drone::spawn_state(kAp, dp, i, n);
        // On the shell, wings level.
        REQUIRE(glm::length(s.position) ==
                Catch::Approx(kAp.R + dp.spawn_alt).margin(1e-6));
        const glm::dvec3 up = sim::local_up(s.position);
        const glm::dvec3 body_up = s.orientation * glm::dvec3{0.0, 1.0, 0.0};
        REQUIRE(glm::dot(body_up, up) == Catch::Approx(1.0).margin(1e-9));
        REQUIRE(std::abs(glm::length(s.orientation) - 1.0) < 1e-9);
        // Within the cluster: angular distance from +X <= spread_angle (+slop
        // for drone 0's ahead/side offset).
        const double ang = std::acos(glm::clamp(
            glm::dot(glm::normalize(s.position), spawn_dir), -1.0, 1.0));
        REQUIRE(ang <= dp.spread_m / kAp.R + 0.05);
        ps.push_back(s.position);
    }
    // Distinct (a real scatter, not all stacked on one point).
    for (int i = 0; i < n; ++i)
        for (int j = i + 1; j < n; ++j)
            REQUIRE(glm::length(ps[i] - ps[j]) > 50.0);
    // The cluster actually spreads (not a degenerate point): the widest pair
    // spans a good fraction of the spread.
    double maxsep = 0.0;
    for (int i = 0; i < n; ++i)
        for (int j = i + 1; j < n; ++j)
            maxsep = std::max(maxsep, glm::length(ps[i] - ps[j]));
    REQUIRE(maxsep > 1000.0);
}

// ===========================================================================
// drone::tick — the drone flies (invariants hold on the SECOND plant instance,
// same as the player): finite state, unit quaternion, and a level cruise does
// NOT dive through the planet. A straight leg (turn_time=0) holds heading; a
// turn leg banks and turns.
// ===========================================================================
TEST_CASE("drone::tick: a level cruise stays airborne with valid state") {
    drone::DroneParams dp;
    dp.turn_time = 0.0;  // never turn -> straight great circle
    drone::DroneState d = drone::spawn_drone(kAp, dp, 0, 1);

    const glm::dvec3 heading0 = [&] {
        const glm::dvec3 up = sim::local_up(d.curr.position);
        const glm::dvec3 v = d.curr.velocity;
        return glm::normalize(v - glm::dot(v, up) * up);
    }();

    for (int i = 0; i < 900; ++i) {  // 7.5 s at 120 Hz
        drone::tick(d, kAp, dp, nullptr);
        REQUIRE(finite3(d.curr.position));
        REQUIRE(finite3(d.curr.velocity));
        REQUIRE(std::abs(glm::length(d.curr.orientation) - 1.0) < 1e-9);
        REQUIRE(sim::altitude(d.curr.position, kAp) > 0.0);  // never crashes
    }
    // Bounded energy (didn't rocket away or stall to a stop).
    const double sp = glm::length(d.curr.velocity);
    REQUIRE(sp > 80.0);
    REQUIRE(sp < 220.0);
    // Straight: heading barely changed over the run (just the V/R curvature).
    const glm::dvec3 up = sim::local_up(d.curr.position);
    const glm::dvec3 heading1 =
        glm::normalize(d.curr.velocity - glm::dot(d.curr.velocity, up) * up);
    REQUIRE(std::acos(glm::clamp(glm::dot(heading0, heading1), -1.0, 1.0)) <
            0.20);  // < ~11 deg drift over 7.5 s
}

TEST_CASE("drone::tick: a turn-bank bandit actually turns (gently)") {
    drone::DroneParams dp;
    dp.straight_time =
        0.0;  // turn immediately, hold the bank for the whole run
    dp.turn_time = 100.0;
    dp.turn_bank = 0.50;  // ~29 deg gentle bank
    drone::DroneState d = drone::spawn_drone(kAp, dp, 0, 1);

    const glm::dvec3 heading0 = [&] {
        const glm::dvec3 up = sim::local_up(d.curr.position);
        return glm::normalize(d.curr.velocity -
                              glm::dot(d.curr.velocity, up) * up);
    }();

    for (int i = 0; i < 1200; ++i) drone::tick(d, kAp, dp, nullptr);  // 10 s

    REQUIRE(sim::altitude(d.curr.position, kAp) > 0.0);
    const glm::dvec3 up = sim::local_up(d.curr.position);
    const glm::dvec3 heading1 =
        glm::normalize(d.curr.velocity - glm::dot(d.curr.velocity, up) * up);
    // A real (gentle) banked turn: heading swung well past the straight
    // drift. S-wvane 2026-07-11 (investigated NOT blessed): the drone's
    // P-only bank-hold turns UNcoordinated (the nose lags the turning path),
    // and the new fuselage side-force honestly pulls the velocity back
    // toward the lagging nose — the fleet turns ~14% gentler (0.172 rad
    // measured, was just above 0.20). Attributed to Cy_beta alone by knob
    // A/B. If the fleet should carve like before, the fix is the drone's own
    // coordination gain (drone.h kCoordP), not this bound — deferred.
    // DIFFERENTIAL pin (diff red-team P2-3): a fixed absolute floor is
    // asymmetrically tight on the false-FAIL side under the next gentling
    // retune; the honest oracle is turn >> the SAME drone flown straight.
    const double turn_swing =
        std::acos(glm::clamp(glm::dot(heading0, heading1), -1.0, 1.0));
    drone::DroneParams dps = dp;
    dps.turn_bank = 0.0;  // same drone, wings level — the null arm
    drone::DroneState ds = drone::spawn_drone(kAp, dps, 0, 1);
    const glm::dvec3 s_heading0 = [&] {
        const glm::dvec3 u = sim::local_up(ds.curr.position);
        return glm::normalize(ds.curr.velocity -
                              glm::dot(ds.curr.velocity, u) * u);
    }();
    for (int i = 0; i < 1200; ++i) drone::tick(ds, kAp, dps, nullptr);
    const glm::dvec3 s_up = sim::local_up(ds.curr.position);
    const glm::dvec3 s_heading1 = glm::normalize(
        ds.curr.velocity - glm::dot(ds.curr.velocity, s_up) * s_up);
    const double straight_swing =
        std::acos(glm::clamp(glm::dot(s_heading0, s_heading1), -1.0, 1.0));
    REQUIRE(turn_swing > straight_swing + 0.05);
}

// Determinism: the tick is a pure function of (state, params) — two copies
// stepped identically stay bit-identical (no hidden clock / global).
TEST_CASE("drone::tick: deterministic (two copies step identically)") {
    drone::DroneParams dp;
    dp.turn_bank = 0.4;
    drone::DroneState a = drone::spawn_drone(kAp, dp, 0, 1);
    drone::DroneState b = a;

    for (int i = 0; i < 300; ++i) {
        drone::tick(a, kAp, dp, nullptr);
        drone::tick(b, kAp, dp, nullptr);
    }
    REQUIRE(a.curr.position == b.curr.position);
    REQUIRE(a.curr.velocity == b.curr.velocity);
    REQUIRE(a.age_ticks == b.age_ticks);
}

// ===========================================================================
// The time-on-target meter is FRAME-RATE INDEPENDENT (AT-9 discipline): it
// advances once per SIM tick inside step_frame, so the same sim-time at 30 vs
// 240 fps produces the IDENTICAL count. MUTATION (verified by hand): move
// meter_tick to step_frame's per-FRAME level -> the two rates diverge.
// ===========================================================================
TEST_CASE("meter rides the sim tick: identical count at 30 vs 240 fps") {
    auto run = [](double frame_dt, int n_frames) {
        app::LoopState lp;
        lp.curr = app::spawn_state(kAp);
        lp.prev = lp.curr;
        lp.prev_up = sim::local_up(lp.curr.position);
        lp.aim.reseed(lp.curr.orientation, lp.prev_up);
        lp.grounded = true;

        app::DroneWorld dw;
        // A bandit straight ahead, CLOSE (in gun range) flying the same great
        // circle as the player, so the nose sits on the lead -> hit_ticks > 0
        // (exercises the HIT count's frame-independence, not just total —
        // red-team P2).
        dw.dparams.count = 1;
        dw.dparams.turn_time = 0.0;  // straight ahead
        dw.dparams.spawn_ahead = 300.0;
        dw.dparams.spawn_side = 0.0;
        dw.drones.push_back(drone::spawn_drone(kAp, dw.dparams, 0, 1));

        app::Accumulator accum(kAp.sim_dt);
        app::FrameInput fin;  // instructor mode, level
        fin.throttle = 1.0;
        double pdx = 0.0, pdy = 0.0;
        for (int f = 0; f < n_frames; ++f)
            app::step_frame(lp, accum, frame_dt, fin, pdx, pdy, kAp, kCp,
                            nullptr, &dw);
        return dw.meter;
    };

    // Both cover exactly 400 sim ticks (100 * 4 == 800 * 0.5).
    const render::OnTargetMeter coarse = run(4.0 * kAp.sim_dt, 100);
    const render::OnTargetMeter fine = run(0.5 * kAp.sim_dt, 800);

    REQUIRE(coarse.total_ticks > 0);  // engaged + in gun range
    REQUIRE(coarse.hit_ticks > 0);    // nose on the lead (HIT path exercised)
    REQUIRE(coarse.total_ticks == fine.total_ticks);
    REQUIRE(coarse.hit_ticks == fine.hit_ticks);
}

// The engaged-target selection LATCHES (Fable P1-2, "every gate hysteretic"):
// driven OPEN-LOOP on a fixed rival oscillating across the engaged target's
// range (the S6 dwell discipline), it must NOT strobe. MUTATION: drop the 0.85
// sticky rule (nearest-always) -> it flips every tick as the rival crosses.
TEST_CASE("app::select_engaged_target: sticky latch does not strobe") {
    render::GunsightParams gp;  // defaults: track_range 2000, cone_cos 0.707
    const glm::dvec3 shooter{0, 0, 0}, nose{0, 0, -1};
    std::vector<drone::DroneState> fleet(2);
    fleet[0].curr.position = {0, 0, -1000};  // A: fixed, dead ahead at 1000 m
    int prev = -1, switches = 0;
    for (int i = 0; i < 40; ++i) {
        const double rb =
            (i % 2 == 0) ? 1010.0 : 990.0;  // B crosses A each tick
        fleet[1].curr.position = {0, 0, -rb};
        const int eng =
            app::select_engaged_target(prev, fleet, shooter, nose, gp);
        if (i > 0 && eng != prev) ++switches;
        prev = eng;
    }
    REQUIRE(switches <= 1);  // only the initial acquisition; then latched
}

// ===========================================================================
// THE FIREWALL, pinned WITH drones present (red-team P1): the existing mirror /
// golden tests only run the no-dw path, so a mutation INSIDE the drone/meter
// block that touched the player would pass the whole gate. Step two identical
// players from one seed — one WITH a live fleet (&dw), one WITHOUT (nullptr) —
// and REQUIRE the player trajectory bit-identical. MUTATION: any write to
// st.curr in the drone block (e.g. st.curr.velocity += ...) -> the two diverge.
// ===========================================================================
TEST_CASE(
    "firewall: a live fleet does not perturb the player (bit-identical)") {
    auto seed = [] {
        app::LoopState lp;
        lp.curr = app::spawn_state(kAp);
        lp.prev = lp.curr;
        lp.prev_up = sim::local_up(lp.curr.position);
        lp.aim.reseed(lp.curr.orientation, lp.prev_up);
        lp.grounded = true;
        return lp;
    };
    app::LoopState with = seed();
    app::LoopState without = seed();

    app::DroneWorld dw;  // a full default fleet, engaged (drone 0 ahead)
    for (int i = 0; i < dw.dparams.count; ++i)
        dw.drones.push_back(
            drone::spawn_drone(kAp, dw.dparams, i, dw.dparams.count));

    app::TickInput in;
    in.raw_mode = false;
    in.throttle = 1.0;
    for (int i = 0; i < 600; ++i) {
        app::tick(with, in, kAp, kCp, nullptr, &dw);
        app::tick(without, in, kAp, kCp, nullptr, nullptr);
        REQUIRE(with.curr.position == without.curr.position);
        REQUIRE(with.curr.velocity == without.curr.velocity);
        REQUIRE(with.curr.orientation == without.curr.orientation);
    }
    // Sanity: the fleet actually ran AND the meter path executed (drone 0 sits
    // within track_range ahead, so it engages — else the test proves nothing).
    REQUIRE(dw.meter.has_target);
    REQUIRE(dw.drones[0].age_ticks > 0);
}

// ===========================================================================
// The drone crash -> in-place respawn (red-team P1): the flying tests require
// NO crash, so this branch was dead. The shipped default (committed turn) hits
// it every session. Force a crash and pin the respawn: back at its OWN scatter
// slot, prev == curr (no render streak), grounded, age_ticks reset, respawned.
// MUTATION: respawn at the wrong slot / drop prev=curr / forget grounded ->
// fail.
// ===========================================================================
TEST_CASE("drone::tick: a crash respawns the bandit in place") {
    drone::DroneParams dp;
    const int idx = 3, n = 10;
    drone::DroneState d = drone::spawn_drone(kAp, dp, idx, n);
    // Put it low and diving straight in so altitude crosses 0 within a few
    // ticks.
    const glm::dvec3 up = sim::local_up(d.curr.position);
    d.curr.position = glm::normalize(d.curr.position) * (kAp.R + 3.0);
    d.curr.velocity = -up * 200.0;  // radially inward
    d.curr.last_vhat = glm::normalize(d.curr.velocity);
    d.grounded = false;

    bool respawned = false;
    for (int i = 0; i < 30 && !respawned; ++i)
        respawned = drone::tick(d, kAp, dp, nullptr).respawned;
    REQUIRE(respawned);

    const sim::SimState slot = drone::spawn_state(kAp, dp, idx, n);
    REQUIRE(glm::length(d.curr.position - slot.position) < 1e-6);  // OWN slot
    REQUIRE(d.curr.position == d.prev.position);  // no interpolation streak
    REQUIRE(d.grounded);
    REQUIRE(d.age_ticks == 0);
    REQUIRE(sim::altitude(d.curr.position, kAp) > 0.0);  // reborn airborne
}

// rig-B: the bandit records the autopilot Inputs it COMMANDED this tick (the
// Fleet Rig poses its surfaces from them — the same causally-correct source as
// the player). A REPORT field the goldens/physics checks are blind to (the
// AT-9 consumed_dx class), so pin it explicitly: after a tick last_inputs
// equals the autopilot output for that state, and it resets to {} on respawn.
TEST_CASE("rig-B drone: last_inputs mirrors the commanded autopilot output") {
    drone::DroneParams dp;
    dp.turn_time = 0.0;  // straight leg: a clean, reproducible command
    drone::DroneState d = drone::spawn_drone(kAp, dp, 0, 1);
    // The expected command is the autopilot on the pre-tick state + the
    // autothrottle holding dp.speed (mirrors drone::tick's own composition).
    const double bank = drone::program_bank(d.age_ticks, dp, kAp.sim_dt);
    sim::Inputs expect = drone::autopilot(d.curr, kAp, bank);
    expect.throttle = static_cast<float>(
        std::clamp(drone::kCruiseTrim +
                       drone::kSpeedP * (dp.speed - glm::length(d.curr.velocity)),
                   0.0, dp.throttle));
    drone::tick(d, kAp, dp, nullptr);
    REQUIRE(d.last_inputs.pitch == expect.pitch);
    REQUIRE(d.last_inputs.roll == expect.roll);
    REQUIRE(d.last_inputs.yaw == expect.yaw);
    REQUIRE(d.last_inputs.throttle == expect.throttle);

    // Respawn zeroes it (a reborn bandit's surfaces start neutral). Drive it
    // into the planet: aim the state down and step until it crashes.
    d.curr.velocity = -glm::normalize(d.curr.position) * 200.0;
    drone::DroneTickResult r{};
    for (int i = 0; i < 4000 && !r.respawned; ++i)
        r = drone::tick(d, kAp, dp, nullptr);
    REQUIRE(r.respawned);
    REQUIRE(d.last_inputs.pitch == 0.0f);
    REQUIRE(d.last_inputs.roll == 0.0f);
    REQUIRE(d.last_inputs.yaw == 0.0f);
    REQUIRE(d.last_inputs.throttle == 0.0f);
}

// rig-B RA9 firewall (Fable before-consult P1): the render consumer
// apply_deflection reads the drone's commanded Inputs + gear and mutates ONLY a
// render Rig — it can NEVER perturb the flight. Toggle it: two bandits from the
// same spawn, one arm calls apply_deflection every tick, the other never — and
// REQUIRE their sim state stays BIT-IDENTICAL (not a tolerance band). Const
// inputs + separate Rig memory guarantee it; this makes the firewall
// executable, so a future coupling that routed the rig back into the sim trips
// here.
TEST_CASE(
    "rig-B drone: apply_deflection never perturbs the flight (firewall)") {
    drone::DroneParams dp;  // default patrol (turns + straight legs)
    drone::DroneState ref = drone::spawn_drone(kAp, dp, 2, 5);
    drone::DroneState feat = drone::spawn_drone(kAp, dp, 2, 5);
    render::Rig scratch = render::build_aircraft_rig();
    const render::DeflectGains g{0.3f, 0.35f, 0.4f, 1.5f};
    for (int i = 0; i < 1200; ++i) {  // 10 s, through several turns
        drone::tick(ref, kAp, dp, nullptr);
        drone::tick(feat, kAp, dp, nullptr);
        // The render side: pose the rig from the bandit's commanded surfaces +
        // actual gear. Writes ONLY `scratch`.
        render::apply_deflection(scratch, feat.last_inputs,
                                 static_cast<float>(feat.curr.gear), g);
        REQUIRE(feat.curr.position == ref.curr.position);
        REQUIRE(feat.curr.velocity == ref.curr.velocity);
        REQUIRE(feat.curr.orientation == ref.curr.orientation);
        REQUIRE(feat.curr.angular_vel == ref.curr.angular_vel);
    }
}

// ===========================================================================
// Bandit combat AI (docs/bandit_combat_plan.md). The pursuit steering + fire
// decision are PURE geometry — pin them in the bandit's OWN body frame so the
// checks are robust to the world-sphere placement. drone::pursue reads the
// player, writes nothing (the firewall).
// ===========================================================================

// P1-6 acceptance gate: the generalized autopilot with target_gamma==0 is
// BIT-IDENTICAL to the level-hold path (std::sin(0.0)==+0.0, x-0.0==x). The whole
// firewall rests on this. MUTATION: reassociate the level term -> a ULP drift
// that fails the bit-exact compare.
TEST_CASE("drone::autopilot: target_gamma=0 is bit-identical to level hold") {
    drone::DroneParams dp;
    drone::DroneState d = drone::spawn_drone(kAp, dp, 2, 5);
    // Give it a non-trivial attitude/rate so the term isn't degenerate.
    d.curr.velocity = glm::dvec3{5.0, 3.0, -120.0} + d.curr.velocity;
    d.curr.angular_vel = glm::dvec3{0.02, -0.01, 0.03};
    const sim::Inputs level = drone::autopilot(d.curr, kAp, 0.30);
    const sim::Inputs gamma0 = drone::autopilot(d.curr, kAp, 0.30, 0.0);
    REQUIRE(level.pitch == gamma0.pitch);  // bit-exact, not Approx
    REQUIRE(level.roll == gamma0.roll);
    REQUIRE(level.yaw == gamma0.yaw);
}

// Bank sign: a player off the bandit's RIGHT commands +bank (right wing down =
// bank-to-turn right); off the LEFT commands -bank. MUTATION: flip the atan2
// argument order / sign -> the bandit banks AWAY from the player.
TEST_CASE("drone::pursue: banks toward the player (right -> +bank, left -> -bank)") {
    drone::DroneParams dp;
    drone::DroneState b = drone::spawn_drone(kAp, dp, 0, 1);
    const glm::dvec3 nose = b.curr.orientation * glm::dvec3{0, 0, -1};
    const glm::dvec3 right = b.curr.orientation * glm::dvec3{1, 0, 0};

    sim::SimState pr;  // player ahead + to the bandit's right
    pr.position = b.curr.position + nose * 1000.0 + right * 500.0;
    const drone::PursueCmd cr = drone::pursue(b.curr, pr, dp);
    REQUIRE(cr.target_bank > 0.05);  // bank right toward the player

    sim::SimState pl;  // player ahead + to the bandit's left
    pl.position = b.curr.position + nose * 1000.0 - right * 500.0;
    const drone::PursueCmd cl = drone::pursue(b.curr, pl, dp);
    REQUIRE(cl.target_bank < -0.05);  // bank left
}

// Gamma sign: a player ABOVE the bandit's horizon commands a climb (+gamma),
// below a dive (-gamma). MUTATION: flip the elevation sign -> the bandit dives
// away from a target above it.
TEST_CASE("drone::pursue: climbs toward a player above, dives toward one below") {
    drone::DroneParams dp;
    drone::DroneState b = drone::spawn_drone(kAp, dp, 0, 1);
    const glm::dvec3 nose = b.curr.orientation * glm::dvec3{0, 0, -1};
    const glm::dvec3 bup = b.curr.orientation * glm::dvec3{0, 1, 0};  // == local_up (level)

    sim::SimState above;
    above.position = b.curr.position + nose * 1000.0 + bup * 500.0;
    REQUIRE(drone::pursue(b.curr, above, dp).target_gamma > 0.02);

    sim::SimState below;
    below.position = b.curr.position + nose * 1000.0 - bup * 500.0;
    REQUIRE(drone::pursue(b.curr, below, dp).target_gamma < -0.02);
}

// Fire gate: nose-on inside the range band fires; out of cone, too far, and too
// near all withhold. MUTATION: drop the cone or range gate -> a bandit sprays
// with the player off-boresight / out of range.
TEST_CASE("drone::pursue: fires only nose-on within the range band") {
    drone::DroneParams dp;  // fire_cone ~3deg, range [60, 600]
    drone::DroneState b = drone::spawn_drone(kAp, dp, 0, 1);
    const glm::dvec3 nose = b.curr.orientation * glm::dvec3{0, 0, -1};
    const glm::dvec3 right = b.curr.orientation * glm::dvec3{1, 0, 0};

    sim::SimState on;  // dead ahead, 300 m
    on.position = b.curr.position + nose * 300.0;
    REQUIRE(drone::pursue(b.curr, on, dp).fire);

    sim::SimState off;  // 300 m ahead but 300 m to the side (way off cone)
    off.position = b.curr.position + nose * 300.0 + right * 300.0;
    REQUIRE_FALSE(drone::pursue(b.curr, off, dp).fire);

    sim::SimState far;  // nose-on but past fire_range_max
    far.position = b.curr.position + nose * 5000.0;
    REQUIRE_FALSE(drone::pursue(b.curr, far, dp).fire);

    sim::SimState near;  // nose-on but inside fire_range_min
    near.position = b.curr.position + nose * 30.0;
    REQUIRE_FALSE(drone::pursue(b.curr, near, dp).fire);
}

// P1-3 degeneracy guards: a player coincident with the bandit produces no
// command and no fire (no NaN into sim::step). MUTATION: drop the eps guard ->
// normalize(0) = NaN propagates.
TEST_CASE("drone::pursue: coincident player is a NaN-safe no-op") {
    drone::DroneParams dp;
    drone::DroneState b = drone::spawn_drone(kAp, dp, 0, 1);
    sim::SimState coincident;
    coincident.position = b.curr.position;  // exactly on the bandit
    const drone::PursueCmd c = drone::pursue(b.curr, coincident, dp);
    REQUIRE(std::isfinite(c.target_bank));
    REQUIRE(std::isfinite(c.target_gamma));
    REQUIRE(c.target_bank == 0.0);
    REQUIRE(c.target_gamma == 0.0);
    REQUIRE_FALSE(c.fire);
}

// An engaged bandit actually swings its nose toward a player off to the side —
// the whole point of "seek me out in merges". Off-arm: the SAME geometry with
// engaged=false (patrol) does NOT chase. MUTATION: ignore the engaged flag ->
// the patrol arm also turns to the player.
TEST_CASE("drone::tick: an engaged bandit turns to face the player") {
    drone::DroneParams dp;
    dp.turn_time = 0.0;         // patrol = straight, so any swing is the pursuit
    dp.pursue_lead_speed = 0.0;  // boresight (no lead) so the nose points AT the
                                 //   stationary player — isolates the TURN from
                                 //   the deflection offset (lead is pinned separately)
    drone::DroneState eng = drone::spawn_drone(kAp, dp, 0, 1);
    const glm::dvec3 right = eng.curr.orientation * glm::dvec3{1, 0, 0};
    const glm::dvec3 nose0 = eng.curr.orientation * glm::dvec3{0, 0, -1};
    // v5 RECONCILIATION (2026-07-24): abeam range 600 -> 2500 m. Under the
    // rung-D arcade energy model the bandit HOLDS its speed through the turn
    // (measured V 113-117 across the whole window vs the trainer's bleed),
    // so its turn radius (~V^2/(g tan(bank)) ~ 800 m at V=115) stays LARGER
    // than a 600 m range forever — pointing at a stationary target inside
    // your own turn circle is geometrically impossible (measured: nose_dot
    // peaked 0.414 at 1 s then orbited negative for 9 s). The trainer-era
    // fixture encoded speed-bleed physics. 2500 m sits outside the turn
    // diameter, so the engaged pursuit can genuinely point; the patrol
    // discriminator is unchanged (a straight line still never turns).
    sim::SimState player;
    player.position = eng.curr.position + right * 2500.0 + nose0 * 200.0;
    player.velocity = glm::dvec3{0.0};

    drone::DroneState pat = eng;  // identical twin flown as a patroller
    eng.engaged = true;
    auto nose_dot = [&](const drone::DroneState& d) {
        const glm::dvec3 nz = d.curr.orientation * glm::dvec3{0, 0, -1};
        return glm::dot(glm::normalize(player.position - d.curr.position), nz);
    };
    // Track the BEST nose-on each achieves over the window — the aggressive
    // pursuit swings hard (and climbs into the turn), so compare the peak reached.
    // Window 15 s: a ~90-deg turn at the held-V radius takes ~11 s
    // (pi*R/2/V at R ~ 800, V ~ 115) — the energy-retaining v5 bandit
    // turns WIDE, not slow.
    double best_eng = nose_dot(eng), best_pat = nose_dot(pat);
    for (int i = 0; i < 1800; ++i) {  // 15 s
        drone::tick(eng, kAp, dp, nullptr, &player);   // pursuing
        drone::tick(pat, kAp, dp, nullptr, &player);   // engaged=false -> patrol (ignores player)
        best_eng = std::max(best_eng, nose_dot(eng));
        best_pat = std::max(best_pat, nose_dot(pat));
        if (i % 120 == 0)
            std::printf("DBG i=%d nd=%.3f alt=%.0f V=%.1f\n", i, nose_dot(eng),
                        glm::length(eng.curr.position) - kAp.R,
                        glm::length(eng.curr.velocity));
    }
    // The pursuer swings its nose toward the player; the straight patroller flies
    // its great-circle line and never turns to it. (Range-closing / gun solutions
    // against a MOVING player are pinned by the "fleet engages and closes"
    // integration test — this isolates the TURN from lead/geometry.)
    REQUIRE(best_eng > best_pat + 0.1);       // turns toward the player, patrol doesn't
    REQUIRE(pat.age_ticks == eng.age_ticks);  // both actually ran
}

// assign_engagements flags the nearest max_engaged, the rest patrol. MUTATION:
// pick farthest / all -> the wrong set is engaged.
TEST_CASE("drone::assign_engagements: nearest N are the attackers") {
    drone::DroneParams dp;  // engage 2500, disengage 3500
    sim::SimState player;
    player.position = glm::dvec3{kAp.R + 2000.0, 0.0, 0.0};
    std::vector<drone::DroneState> fleet(4);
    const double d[4] = {1000.0, 2000.0, 3000.0, 4000.0};
    for (int i = 0; i < 4; ++i)
        fleet[i].curr.position = player.position + glm::dvec3{0, 0, -d[i]};

    drone::assign_engagements(fleet, player, 2, dp);
    REQUIRE(fleet[0].engaged);       // 1000 m
    REQUIRE(fleet[1].engaged);       // 2000 m
    REQUIRE_FALSE(fleet[2].engaged); // 3000 m — qualifies but not in the nearest 2
    REQUIRE_FALSE(fleet[3].engaged); // 4000 m > engage_range 3200
}

// Hysteresis: a currently-engaged bandit inside DISENGAGE range but beyond
// ENGAGE range KEEPS its slot over a closer FRESH bandit (holder priority). And
// once it exceeds disengage it drops. MUTATION: drop the holder priority ->
// nearest-always flips the slot to the closer fresh bandit (chatter).
TEST_CASE("drone::assign_engagements: hysteresis keeps a holder over a closer fresh bandit") {
    drone::DroneParams dp;  // engage 3200, disengage 4200
    sim::SimState player;
    player.position = glm::dvec3{kAp.R + 2000.0, 0.0, 0.0};
    std::vector<drone::DroneState> fleet(2);
    fleet[0].curr.position = player.position + glm::dvec3{0, 0, -3000.0};  // fresh, within engage
    fleet[1].curr.position = player.position + glm::dvec3{0, 0, -3800.0};  // holder zone
    fleet[1].engaged = true;  // already engaged (a holder)

    drone::assign_engagements(fleet, player, 1, dp);
    REQUIRE(fleet[1].engaged);        // the holder keeps the single slot...
    REQUIRE_FALSE(fleet[0].engaged);  // ...even though 0 is closer (no chatter)

    // Push the holder past disengage -> it drops, the fresh one takes over.
    fleet[1].curr.position = player.position + glm::dvec3{0, 0, -4400.0};
    drone::assign_engagements(fleet, player, 1, dp);
    REQUIRE(fleet[0].engaged);        // fresh bandit now engaged
    REQUIRE_FALSE(fleet[1].engaged);  // holder released (beyond disengage)
}

// The soft AoA limiter (drone.h autopilot aoa_protect): an ENGAGED bandit forced
// into the hardest maneuver — a ~180 deg reversal onto a player dead BEHIND it —
// must ride the stall envelope, never departing into the post-stall porpoise that
// augered a bandit in (Chad 2026-07-14 "one fell on its own"). Run at the
// AGGRESSIVE header-default combat gains (pitch 2.2 / pull 20 deg) so the test
// pins the LIMITER, not the gentler scenario.toml tune. MUTATION: force
// aoa_protect false (or envelope -> 1) and AoA blows past 3x stall (the pre-fix
// probe hit ~72 deg == 3.5x) -> the bound fails; and the bandit can auger in ->
// the altitude REQUIRE fails.
TEST_CASE("drone::autopilot: pursuit AoA stays inside the stall envelope") {
    drone::DroneParams dp;  // header combat defaults (pitch 2.2, pull 0.35 rad)
    dp.speed = 85.0;
    drone::DroneState d = drone::spawn_drone(kAp, dp, 0, 10);
    d.hp = dp.hp; d.engaged = true;
    sim::SimState player = app::spawn_state(kAp);  // dead behind -> 180 reversal
    const double stall_a = kAp.Cl_max / kAp.Cl_alpha;
    double max_aoa = 0.0;
    for (int t = 0; t < 25 * 120; ++t) {  // 25 s engagement
        drone::tick(d, kAp, dp, nullptr, &player);
        REQUIRE(sim::altitude(d.curr.position, kAp) > 0.0);  // never augers in
        const glm::dvec3 vb = sim::body_dir_of(d.curr.orientation,
            sim::current_vhat(d.curr, kAp));
        max_aoa = std::max(max_aoa, std::abs(sim::alpha_of(vb)));
    }
    // Measured ~1.21x stall (24.9 deg) at these gains — the small overshoot past
    // the 0.95x cap is the ZOH/pitch-momentum lag. 1.4x leaves margin yet still
    // catches a disabled limiter (the pre-fix PIO reached 3.5x == ~72 deg).
    REQUIRE(max_aoa < 1.4 * stall_a);
}

// ===========================================================================
// CONQUEST AIR-SENSE (bubble leash) + PUMP RAIDS (ADEPT-AI + COMPETITIVE rung,
// 2026-07-25 fly-2). The leash keeps a non-tunnel maverick inside its faction
// ellipse; the raid steers a designated raider at the enemy surface pump. Both
// default OFF (bit-identical). drone/ stays pure — these pin the pure pieces.
// ===========================================================================

namespace {
// Place a drone at arc = `frac` * r_eff along the VALLEY ellipse's MAJOR axis
// (where r_eff == the semi-major `a`), heading outward along that axis so a
// home-steer is a real turn. alt above the shell.
drone::DroneState valley_drone_at_frac(double frac, double alt = 1500.0) {
    const glm::dvec3 c = glm::normalize(world::kValleyCenterDir);
    const glm::dvec3 m = glm::normalize(world::kValleyMajorAxis);
    const double a = world::kValleyMajorRadiusM;
    const double theta = frac * a / kAp.R;  // arc angle along the major axis
    const glm::dvec3 dir = std::cos(theta) * c + std::sin(theta) * m;
    const glm::dvec3 pos = glm::normalize(dir) * (kAp.R + alt);
    drone::DroneParams dp;
    drone::DroneState d;
    d.curr = drone::level_state_at(dp, pos, m);  // heading outward along +major
    d.prev = d.curr;
    d.grounded = false;
    return d;
}

drone::BubbleLeash valley_leash() {
    drone::BubbleLeash lz;
    lz.enabled = true;
    lz.center_dir = world::kValleyCenterDir;
    lz.major_axis = world::kValleyMajorAxis;
    lz.a_m = world::kValleyMajorRadiusM;
    lz.b_m = world::kValleyMinorRadiusM;
    return lz;  // steer 0.55, hard 0.75, release 0.42 (the struct defaults)
}
}  // namespace

// The leash ENGAGES beyond steer_frac and steers the commanded bank toward home,
// is INERT (bit-identical) well inside, and FULLY OVERRIDES a pursuit bank beyond
// hard_frac. MUTATION: drop the engage gate (always steer) -> the deep-inside
// leg fails (target_bank moved); drop the blend saturation -> the beyond-hard leg
// (blend==1, output==home) fails.
TEST_CASE("drone::apply_bubble_leash: engages beyond the fraction, overrides") {
    const drone::BubbleLeash lz = valley_leash();

    // Deep inside (frac 0.5): inert, target_bank returned unchanged, engaged off.
    {
        drone::DroneState d = valley_drone_at_frac(0.5);
        bool eng = false;
        const drone::LeashResult r =
            drone::apply_bubble_leash(lz, d.curr, 0.20, kAp.R, eng);
        REQUIRE_FALSE(eng);
        REQUIRE(r.target_bank == 0.20);  // bit-identical no-op
        REQUIRE(r.blend == 0.0);
    }
    // Beyond steer_frac (frac 0.90): engages and moves the bank homeward.
    {
        drone::DroneState d = valley_drone_at_frac(0.90);
        bool eng = false;
        const drone::LeashResult r =
            drone::apply_bubble_leash(lz, d.curr, 0.0, kAp.R, eng);
        REQUIRE(eng);
        REQUIRE(r.blend > 0.0);
        REQUIRE(std::abs(r.target_bank) > 0.05);  // a real home turn commanded
    }
    // Beyond hard_frac (frac 1.02): blend saturates at 1 -> a pursuit bank is
    // FULLY replaced by the home bank (they break off the chase).
    {
        drone::DroneState d = valley_drone_at_frac(1.02);
        bool eng = false;
        const double pursuit_bank = 0.6;  // a hard pursuit roll
        const drone::LeashResult r =
            drone::apply_bubble_leash(lz, d.curr, pursuit_bank, kAp.R, eng);
        REQUIRE(eng);
        REQUIRE(r.blend == Catch::Approx(1.0).epsilon(1e-9));
        // Fully homed: the output is the home bank, NOT the pursuit bank.
        REQUIRE(r.target_bank != Catch::Approx(pursuit_bank));
    }
}

// HYSTERESIS pinned OPEN-LOOP on a DWELLING boundary frac (the S6 lesson: a
// closed-loop chase nulls to monotone and pins nothing). Once engaged (a high
// frac), the leash HOLDS engaged + a nonzero home blend while frac dwells in the
// (release_frac, steer_frac) band; a fresh disengaged drone at the SAME dwell
// frac stays off. MUTATION: make engage non-hysteretic (engaged = frac >
// steer_frac each tick, ignoring prev) -> the held case drops to engaged=false /
// blend=0 at the dwell and the REQUIRE fails.
TEST_CASE("drone::apply_bubble_leash: hysteretic latch does not chatter") {
    const drone::BubbleLeash lz = valley_leash();
    const double dwell = 0.48;  // between release 0.42 and steer 0.55
    REQUIRE(dwell > lz.release_frac);
    REQUIRE(dwell < lz.steer_frac);

    // Arm the latch with one tick above steer_frac, then dwell in the band.
    drone::DroneState hi = valley_drone_at_frac(0.90);
    bool held = false;
    drone::apply_bubble_leash(lz, hi.curr, 0.0, kAp.R, held);
    REQUIRE(held);  // engaged

    drone::DroneState mid = valley_drone_at_frac(dwell);
    const drone::LeashResult r_held =
        drone::apply_bubble_leash(lz, mid.curr, 0.0, kAp.R, held);
    REQUIRE(held);                // STAYS engaged through the band (hysteresis)
    REQUIRE(r_held.blend > 0.0);  // and keeps steering home

    // A drone that was NEVER above steer_frac at the same dwell frac stays off.
    bool fresh = false;
    const drone::LeashResult r_fresh =
        drone::apply_bubble_leash(lz, mid.curr, 0.0, kAp.R, fresh);
    REQUIRE_FALSE(fresh);
    REQUIRE(r_fresh.blend == 0.0);

    // Release only below release_frac.
    drone::DroneState lo = valley_drone_at_frac(0.35);
    drone::apply_bubble_leash(lz, lo.curr, 0.0, kAp.R, held);
    REQUIRE_FALSE(held);
}

// TUNNEL-MODE EXEMPTION: a maverick committed to a bore run (RUN mode) is NOT
// leashed even beyond the edge (the run must cross the vacuum on the deck); the
// SAME drone in PATROL engages the leash. MUTATION: drop the is_tunnel_mode gate
// -> the RUN twin also engages and the REQUIRE_FALSE fails.
TEST_CASE("drone::tick: the bubble leash exempts committed tunnel modes") {
    drone::DroneParams dp;
    dp.maverick.enabled = true;  // tunnel modes exist; env->tunnels null so the
                                 // maverick brain is skipped and mode is held.
    const drone::BubbleLeash lz = valley_leash();

    drone::DroneState run = valley_drone_at_frac(0.90);
    run.leash = lz;
    run.mav.mode = maverick::MaverickState::Mode::RUN;
    drone::tick(run, kAp, dp, nullptr);
    REQUIRE_FALSE(run.leash_engaged);  // exempt — the run leaves the dome

    drone::DroneState pat = valley_drone_at_frac(0.90);
    pat.leash = lz;
    pat.mav.mode = maverick::MaverickState::Mode::PATROL;
    drone::tick(pat, kAp, dp, nullptr);
    REQUIRE(pat.leash_engaged);  // PATROL is leashed
}

// OFF-ARM bit-identity: enabling the leash while the drone is DEEP INSIDE the
// bubble (frac 0.3, never engaging) flies the EXACT same trajectory as leash-off
// over many ticks. MUTATION: any leash effect inside the steer fraction (e.g.
// steering from frac 0) diverges the positions.
TEST_CASE("drone::tick: leash inside the bubble is bit-identical to leash-off") {
    drone::DroneParams dp;  // maverick disabled -> pure patrol
    drone::DroneState on = valley_drone_at_frac(0.30);
    drone::DroneState off = on;  // identical twin
    on.leash = valley_leash();   // enabled but never engages this deep
    // off.leash stays default (disabled)
    for (int i = 0; i < 120; ++i) {
        drone::tick(on, kAp, dp, nullptr);
        drone::tick(off, kAp, dp, nullptr);
    }
    REQUIRE_FALSE(on.leash_engaged);
    REQUIRE(on.curr.position.x == off.curr.position.x);  // bit-exact
    REQUIRE(on.curr.position.y == off.curr.position.y);
    REQUIRE(on.curr.position.z == off.curr.position.z);
}

// RAID STEERING: a raider with an active raid order swings its nose toward the
// target pump; the SAME drone with raid inactive patrols straight and does not.
// MUTATION: ignore raid.active in drone::tick -> the inactive twin also turns
// toward the pump.
TEST_CASE("drone::tick: an active raid order steers toward the pump") {
    drone::DroneParams dp;
    dp.turn_time = 0.0;  // patrol = straight, so any swing is the raid steer
    drone::DroneState raider = valley_drone_at_frac(0.40);
    const glm::dvec3 nose0 = raider.curr.orientation * glm::dvec3{0, 0, -1};
    const glm::dvec3 right = raider.curr.orientation * glm::dvec3{1, 0, 0};
    // A pump well off to the side (outside the turn diameter so the nose can
    // genuinely point at it) at the same altitude.
    const glm::dvec3 pump_pos =
        raider.curr.position + right * 3000.0 + nose0 * 200.0;

    drone::DroneState patrol = raider;  // identical twin, no raid
    raider.raid.active = true;
    raider.raid.target_pos = pump_pos;

    auto nose_dot = [&](const drone::DroneState& d) {
        const glm::dvec3 nz = d.curr.orientation * glm::dvec3{0, 0, -1};
        return glm::dot(glm::normalize(pump_pos - d.curr.position), nz);
    };
    double best_raid = nose_dot(raider), best_pat = nose_dot(patrol);
    for (int i = 0; i < 1200; ++i) {  // 10 s
        drone::tick(raider, kAp, dp, nullptr);
        drone::tick(patrol, kAp, dp, nullptr);
        best_raid = std::max(best_raid, nose_dot(raider));
        best_pat = std::max(best_pat, nose_dot(patrol));
    }
    REQUIRE(best_raid > best_pat + 0.1);  // the raider turns to the pump
    REQUIRE(sim::altitude(raider.curr.position, kAp) > 0.0);
}

// ---------------------------------------------------------------------------
// ★★★ RUNG S1-DECK — THE TWO PURE LAWS, pinned where they can fail.
//
// A HEIGHT FIELD WITH A REAL RIDGE IN FRONT OF THE AEROPLANE, painted through
// the INVERSE of world::equirect_uv so the texel the law samples is the texel
// this test wrote. Both laws read the same field, which is the point: the
// shipped sink-rate look-ahead (avoid_lookahead_s) is structurally blind to a
// ridge FACE ahead of a LEVEL aeroplane — sink is zero, so eff_agl == agl and
// nothing arms — and both of these can see it.
namespace deck_fx {

constexpr double kPiD = 3.14159265358979323846;

world::HeightField flat_field() {
    world::HeightField hf;
    hf.w = 512;
    hf.h = 256;
    hf.R = 15000.0;
    hf.relief_scale = 800.0;
    hf.u_offset = 0.0;
    hf.px.assign(static_cast<std::size_t>(hf.w) * hf.h, 0);
    return hf;
}

// Paint every texel whose direction is within `arc_m` (great-circle) of `dir`
// to full height. The direction per texel is equirect_uv inverted exactly:
// u = 0.5 + atan2(z,x)/2pi, v = 0.5 - asin(y)/pi (world/heightfield.cpp:15-21).
void paint_cap(world::HeightField& hf, const glm::dvec3& dir, double arc_m) {
    const glm::dvec3 c = glm::normalize(dir);
    for (int y = 0; y < hf.h; ++y) {
        for (int x = 0; x < hf.w; ++x) {
            const double u = (static_cast<double>(x) + 0.5) / hf.w;
            const double v = (static_cast<double>(y) + 0.5) / hf.h;
            const double lat = (0.5 - v) * kPiD;
            const double lon = (u - 0.5) * 2.0 * kPiD;
            const glm::dvec3 p{std::cos(lat) * std::cos(lon), std::sin(lat),
                               std::cos(lat) * std::sin(lon)};
            const double d =
                hf.R * std::acos(glm::clamp(glm::dot(p, c), -1.0, 1.0));
            if (d < arc_m)
                hf.px[static_cast<std::size_t>(y) * hf.w + x] = 65535;
        }
    }
}

// A cap centred `ahead_m` along `fwd` from `up`.
glm::dvec3 ahead_dir(const glm::dvec3& up, const glm::dvec3& fwd, double R,
                     double ahead_m) {
    const double arc = ahead_m / R;
    return glm::normalize(std::cos(arc) * up + std::sin(arc) * fwd);
}

}  // namespace deck_fx

// THE TRACK LAW. Level at the hold altitude over flat ground it commands ~0;
// high over flat ground it commands the dive cap; with a ridge inside the
// forward window it commands a CLIMB. MUTATION APPLIED AND CONFIRMED RED:
// replace `std::max(g_ff, g_res)` with `g_res` alone (drop the terrain
// feed-forward) -> the ridge case reads the dive cap and fails.
TEST_CASE("S1-DECK deck_track_gamma: flat is level, a ridge ahead is a climb") {
    world::HeightField hf = deck_fx::flat_field();
    drone::DroneParams dp;
    dp.deck_track_agl_m = 100.0;
    dp.deck_track_gain = 0.0045;
    dp.deck_track_dive_cap = 12.0 * deck_fx::kPiD / 180.0;
    dp.avoid_gamma = 26.0 * deck_fx::kPiD / 180.0;
    dp.deck_lookahead_s = 12.0;

    const glm::dvec3 up{0.0, 0.0, 1.0};
    const glm::dvec3 fwd{1.0, 0.0, 0.0};  // tangent: dot(up, fwd) == 0
    sim::SimState s;
    s.position = up * (hf.radius_at(up) + dp.deck_track_agl_m);
    s.velocity = fwd * 85.0;

    // 1. FLAT GROUND at the hold altitude: neither climb nor dive.
    {
        const drone::DeckLook lk = drone::deck_look(s, hf, dp.deck_lookahead_s,
                             /*env=*/nullptr);
        REQUIRE(lk.ok);
        CHECK(std::abs(drone::deck_track_gamma(lk, dp)) < 0.01);
    }
    // 2. HIGH over flat ground: descend, bounded by the dive cap.
    {
        sim::SimState hi = s;
        hi.position = up * (hf.radius_at(up) + 600.0);
        const drone::DeckLook lk =
            drone::deck_look(hi, hf, dp.deck_lookahead_s,
                             /*env=*/nullptr);
        REQUIRE(lk.ok);
        CHECK(drone::deck_track_gamma(lk, dp) ==
              Catch::Approx(-dp.deck_track_dive_cap));
    }
    // 3. A RIDGE inside the 12 s / 1020 m window, the ONLY thing that changes.
    {
        const glm::dvec3 ahead = deck_fx::ahead_dir(up, fwd, hf.R, 1500.0);
        deck_fx::paint_cap(hf, ahead, 800.0);
        REQUIRE(hf.radius_at(up) == Catch::Approx(hf.R));  // still flat HERE
        const glm::dvec3 crest_dir = deck_fx::ahead_dir(up, fwd, hf.R, 1000.0);
        REQUIRE(hf.radius_at(crest_dir) > hf.R + 300.0);  // a real wall
        const drone::DeckLook lk = drone::deck_look(s, hf, dp.deck_lookahead_s,
                             /*env=*/nullptr);
        REQUIRE(lk.ok);
        const double g = drone::deck_track_gamma(lk, dp);
        CHECK(g > 0.20);                     // a genuine climb, not the flat ~0
        CHECK(g <= dp.avoid_gamma + 1e-12);  // never past the pull-up angle
    }
}

// THE FORWARD EYES, and the red team's defect pinned: over DEAD-FLAT deck the
// predicate must be FALSE at every speed. Charging the rotation time with no
// obstacle above the flight path makes it TRUE on every tick of level flight —
// guns muted for the whole crossing, i.e. the limit cycle this rung deletes,
// rebuilt in a new costume. MUTATION APPLIED AND CONFIRMED RED: delete the
// `if (climb <= 0.0) continue;` obstacle gate -> both flat cases fail.
TEST_CASE("S1-DECK deck_forward_violated: flat never arms, a ridge does") {
    world::HeightField hf = deck_fx::flat_field();
    drone::DroneParams dp;
    dp.avoid_gamma = 26.0 * deck_fx::kPiD / 180.0;
    dp.deck_lookahead_s = 12.0;
    dp.avoid_pull_net_g = 3.0;  // the shipped, MEASURED value
    const double enter_m = 60.0;

    const glm::dvec3 up{0.0, 0.0, 1.0};
    const glm::dvec3 fwd{1.0, 0.0, 0.0};
    sim::SimState s;
    s.position = up * (hf.radius_at(up) + 100.0);

    // FLAT, at the shipped cruise AND at the measured pursuit speed.
    for (double v : {85.0, 156.0}) {
        s.velocity = fwd * v;
        const drone::DeckLook lk = drone::deck_look(s, hf, dp.deck_lookahead_s,
                             /*env=*/nullptr);
        REQUIRE(lk.ok);
        INFO("flat deck at " << v << " m/s");
        CHECK_FALSE(drone::deck_forward_violated(lk, dp, enter_m));
        // And the shipped sink-rate look-ahead has nothing to see either:
        // level flight, so the whole margin it subtracts is zero.
        CHECK(std::max(0.0, -glm::dot(s.velocity, up)) == Catch::Approx(0.0));
    }
    // avoid_pull_net_g = 0 is the arming's own off switch.
    {
        drone::DroneParams off = dp;
        off.avoid_pull_net_g = 0.0;
        s.velocity = fwd * 85.0;
        const drone::DeckLook lk = drone::deck_look(s, hf, dp.deck_lookahead_s,
                             /*env=*/nullptr);
        CHECK_FALSE(drone::deck_forward_violated(lk, off, enter_m));
    }
    // A RIDGE inside the window: it must arm.
    const glm::dvec3 ahead = deck_fx::ahead_dir(up, fwd, hf.R, 1500.0);
    deck_fx::paint_cap(hf, ahead, 800.0);
    REQUIRE(hf.radius_at(deck_fx::ahead_dir(up, fwd, hf.R, 1000.0)) >
            hf.R + 300.0);
    s.velocity = fwd * 85.0;
    const drone::DeckLook lk = drone::deck_look(s, hf, dp.deck_lookahead_s,
                             /*env=*/nullptr);
    REQUIRE(lk.ok);
    CHECK(drone::deck_forward_violated(lk, dp, enter_m));
}

// ===========================================================================
// ★★★ RUNG D2 — THE REPOSITION'S GEOMETRY.
//
// Chad, 2026-08-27: "if they are near the outer one third near the boundary
// and their own pump is getting attacked maybe they go down and move closer to
// the inside of their bubble". Two shared primitives carry that: ellipse_frac
// (the ONE normalized-radius law, factored out of apply_bubble_leash so the
// trigger cannot fork the containment geometry) and regroup_dir (the bearing
// of the point to fly to). These legs pin BOTH, and they pin the degeneracy
// guards, because RegroupOrder's default target is the ORIGIN and a
// normalize() of that is a NaN into the plant (the guard-every-normalize law).

// ⚠ NO COMMA IN THIS NAME (house rule): a Catch2 test name containing a comma
// cannot be selected by a filter -- it silently runs NOTHING and reads exactly
// like green. This one was authored as "... frac, not a second geometry".
TEST_CASE("D2 ellipse_frac IS the leash's own frac and not a second geometry") {
    // A circular dome (b == a) so the expected arc/r_eff is analytic.
    const glm::dvec3 cdir = glm::normalize(glm::dvec3{0.0, 1.0, 0.0});
    const glm::dvec3 maj = glm::dvec3{1.0, 0.0, 0.0};
    const double a = 10000.0;
    const double b = 10000.0;
    // Walk a bearing out from the centre and check frac == arc / a exactly.
    for (double arc : {0.0, 2000.0, 6670.0, 10000.0, 13500.0}) {
        const double th = arc / kAp.R;
        const glm::dvec3 dir =
            cdir * std::cos(th) + glm::dvec3{0.0, 0.0, 1.0} * std::sin(th);
        const glm::dvec3 pos = dir * (kAp.R + 2000.0);
        const double f = drone::ellipse_frac(pos, cdir, maj, a, b, kAp.R);
        INFO("arc " << arc);
        CHECK(f == Catch::Approx(arc / a).margin(1e-6));
    }
    // ★ AND IT IS THE SAME NUMBER THE LEASH LATCHES ON. Put the drone exactly
    // at the leash's steer_frac and confirm the latch flips there — if these
    // two ever forked, this leg goes red.
    drone::BubbleLeash lz;
    lz.enabled = true;
    lz.center_dir = cdir;
    lz.major_axis = maj;
    lz.a_m = a;
    lz.b_m = b;
    const auto at_frac = [&](double f) {
        const double th = f * a / kAp.R;
        const glm::dvec3 dir =
            cdir * std::cos(th) + glm::dvec3{0.0, 0.0, 1.0} * std::sin(th);
        sim::SimState s;
        s.position = dir * (kAp.R + 2000.0);
        s.velocity = glm::dvec3{0.0};
        return s;
    };
    // ⚠ THIS CLAUSE WAS WEAK AND A MUTATION PROVED IT. It used to poke the
    // latch at steer_frac +/- 0.05, which only catches a fork bigger than 10%
    // -- and MUT-2 (the leash multiplying the shared frac by 1.10) sailed
    // through GREEN, because 1.10 * (0.55 - 0.05) is EXACTLY 0.55 and the
    // latch tests `>` strictly. So do not poke the threshold: MEASURE it. Scan
    // the latch's own engage point and require it to BE steer_frac. Any fork
    // between the leash and ellipse_frac moves that point, at any magnitude.
    double engage_at = -1.0;
    {
        bool eng = false;
        for (int i = 0; i <= 4000; ++i) {
            const double f = 0.0 + 0.0005 * i;  // 0.00 .. 2.00
            const bool before = eng;
            drone::apply_bubble_leash(lz, at_frac(f), 0.0, kAp.R, eng);
            if (!before && eng) {
                engage_at = f;
                break;
            }
        }
    }
    INFO("latch engaged at frac " << engage_at << " steer_frac "
                                  << lz.steer_frac);
    REQUIRE(engage_at > 0.0);  // it engages at all
    CHECK(engage_at == Catch::Approx(lz.steer_frac).margin(1e-3));
    // ...and the release point is the leash's OWN release_frac, on the same
    // shared law (the hysteresis is measured, not assumed).
    double release_at = -1.0;
    {
        bool eng = false;
        drone::apply_bubble_leash(lz, at_frac(1.5), 0.0, kAp.R, eng);
        REQUIRE(eng);
        for (int i = 4000; i >= 0; --i) {
            const double f = 0.0005 * i;
            const bool before = eng;
            drone::apply_bubble_leash(lz, at_frac(f), 0.0, kAp.R, eng);
            if (before && !eng) {
                release_at = f;
                break;
            }
        }
    }
    INFO("latch released at frac " << release_at << " release_frac "
                                   << lz.release_frac);
    REQUIRE(release_at > 0.0);
    CHECK(release_at == Catch::Approx(lz.release_frac).margin(1e-3));
    // Degenerate edge => 0, the leash's own contract ("no edge, no frac").
    CHECK(drone::ellipse_frac(at_frac(1.0).position, cdir, maj, 0.0, 0.0,
                              kAp.R) == Catch::Approx(0.0));
}

TEST_CASE("D2 regroup_dir lands INSIDE the release band and guards degeneracy") {
    const glm::dvec3 cdir = glm::normalize(glm::dvec3{0.0, 1.0, 0.0});
    const glm::dvec3 maj = glm::dvec3{1.0, 0.0, 0.0};
    const double a = 10000.0;
    const double b = 7000.0;
    const double pull = 0.40;
    const double release = 0.45;
    // From ANYWHERE in and beyond the trigger band, on several bearings, the
    // point must come back at pull_frac -- i.e. STRICTLY INSIDE the release,
    // or an episode could never complete and every one would run the timeout.
    for (double az : {0.0, 0.7, 1.9, 3.4, 5.1}) {
        const glm::dvec3 tang =
            glm::normalize(maj * std::cos(az) +
                           glm::normalize(glm::cross(cdir, maj)) *
                               std::sin(az));
        for (double f : {0.70, 0.90, 1.10, 1.35}) {
            // Place the drone at normalized radius f on this bearing.
            const double r_eff_here = world::ellipse_r_eff(
                cdir, maj, a, b,
                glm::normalize(cdir * std::cos(0.001) + tang * std::sin(0.001)));
            const double th = f * r_eff_here / kAp.R;
            const glm::dvec3 dir = cdir * std::cos(th) + tang * std::sin(th);
            const glm::dvec3 pos = dir * (kAp.R + 2000.0);
            glm::dvec3 out{0.0};
            REQUIRE(drone::regroup_dir(pos, cdir, maj, a, b, kAp.R, pull, out));
            CHECK(glm::length(out) == Catch::Approx(1.0).margin(1e-9));
            const double f_out = drone::ellipse_frac(out * kAp.R, cdir, maj, a,
                                                     b, kAp.R);
            INFO("az " << az << " from frac " << f);
            CHECK(f_out < release);
            CHECK(f_out == Catch::Approx(pull).margin(2e-3));
        }
    }
    // ★ THE DEGENERACY GUARDS. Every one of these would be a normalize() of a
    // zero vector -- a NaN straight into the plant.
    glm::dvec3 out{1.0, 2.0, 3.0};
    const glm::dvec3 keep = out;
    CHECK_FALSE(drone::regroup_dir(glm::dvec3{0.0}, cdir, maj, a, b, kAp.R,
                                   pull, out));
    CHECK(out == keep);  // untouched on refusal
    CHECK_FALSE(drone::regroup_dir(cdir * kAp.R, glm::dvec3{0.0}, maj, a, b,
                                   kAp.R, pull, out));
    CHECK_FALSE(drone::regroup_dir(cdir * kAp.R, cdir, maj, 0.0, 0.0, kAp.R,
                                   pull, out));
    CHECK_FALSE(
        drone::regroup_dir(cdir * kAp.R, cdir, maj, a, b, 0.0, pull, out));
    CHECK(out == keep);
}

// ===========================================================================
// ★★★ RUNG D2 — THE REPOSITION ACTUALLY FLIES THE AEROPLANE.
//
// ⚠ THIS LEG EXISTS BECAUSE A MUTATION EMBARRASSED THE MATCH PROBE. [.regroup]
// graded stage-0 liveness on "arm D's enemy hash differs from arm O's", and
// MUT-3 -- which kills the ENTIRE regroup steering branch in drone::tick while
// leaving the app's order-arming intact -- SAILED THROUGH THAT CHECK GREEN
// (0x2465caf9d8807b31 != the OFF hash). The order still perturbs the match via
// the tunnel router's `!d.regroup.active` term, so the hash moves whether or
// not a single aeroplane ever turns. "Different hash" is not "it flew", and 6
// of 8 episodes still read "completed" with the steering dead.
//
// So liveness is pinned HERE instead, closed-loop and cheap: with the order
// armed the drone must CLOSE ON THE POINT, and with it unarmed (identical
// state otherwise) it must not. Nothing but the steering branch can do that.
TEST_CASE("D2 an armed regroup order actually closes on its point") {
    drone::DroneParams dp;        // header defaults -- this leg pins the
    dp.maverick.enabled = false;  // BRANCH, not the shipped dial values

    const glm::dvec3 centre = glm::normalize(glm::dvec3{0.0, 0.0, 1.0});
    const glm::dvec3 east =
        glm::normalize(glm::cross(glm::dvec3{0.0, 1.0, 0.0}, centre));
    const glm::dvec3 pos = centre * (kAp.R + 3000.0);
    // The point to fly to: 6 km along the surface, deliberately BEHIND the
    // drone's initial heading so closing requires a real turn, not coasting.
    const double th = 6000.0 / kAp.R;
    const glm::dvec3 tdir = centre * std::cos(th) - east * std::sin(th);
    const glm::dvec3 target = tdir * (kAp.R + 500.0);

    const auto range_after = [&](bool armed) {
        drone::DroneState d;
        d.curr = drone::level_state_at(dp, pos, east);  // flying AWAY from it
        d.prev = d.curr;
        d.hp = dp.hp;
        d.engaged = false;
        d.foe = drone::kFoeNone;
        if (armed) {
            d.regroup.active = true;
            d.regroup.reason = 1;
            d.regroup.target_pos = target;
        }
        for (int i = 0; i < 7200; ++i) {  // 60 s -- long enough for the
                                          // 180 deg reversal to CLOSE, not
                                          // just to have started turning
            drone::tick(d, kAp, dp, nullptr, nullptr);
            REQUIRE(finite3(d.curr.position));
        }
        return glm::length(target - d.curr.position);
    };

    const double r0 = glm::length(target - pos);
    const double armed_r = range_after(true);
    const double idle_r = range_after(false);
    INFO("start " << r0 << " armed " << armed_r << " idle " << idle_r);
    // The armed drone must have CLOSED, and must be meaningfully nearer than
    // the same drone with no order. MUT-3 collapses armed_r onto idle_r.
    CHECK(armed_r < r0);
    CHECK(armed_r < idle_r - 100.0);
}

// ★★★ RUNG D3 — THE PARABOLIC DIVE, FLOWN. Chad: "climb up then perform a
// parabolic dive toward the other bubble."
//
// ⚠ THIS IS A CLOSED-LOOP LEG ON PURPOSE, and it exists because of the MUT-3
// lesson one rung down: a MOVED HASH proves the ORDER exists, not that an
// aeroplane ever turned. [.bdeck]'s hash arms would still separate if every
// line of the phase-keyed steering were deleted, because the phase latch also
// perturbs the tunnel router. So the actual manoeuvre is graded HERE, in
// METRES of altitude and DEGREES of flight path, against the same aeroplane
// flying the same raid order with no phase.
TEST_CASE("D3 the ballistic phases actually fly the climb and the dive") {
    drone::DroneParams dp;        // header defaults -- this leg pins the
    dp.maverick.enabled = false;  // BRANCH, not the shipped dial values
    dp.raid_ballistic_climb_agl_m = 2000.0;  // the dead switch, OFF by default

    const glm::dvec3 up = glm::normalize(glm::dvec3{0.0, 0.0, 1.0});
    const glm::dvec3 east =
        glm::normalize(glm::cross(glm::dvec3{0.0, 1.0, 0.0}, up));
    const glm::dvec3 pos = up * (kAp.R + 3000.0);
    // A pump 30 km ahead on the surface -- far enough that aim_at's own
    // elevation channel is a shallow descent, so any climb measured below is
    // the PHASE's doing and not the geometry's.
    const double th = 30000.0 / kAp.R;
    const glm::dvec3 tdir = up * std::cos(th) + east * std::sin(th);
    const glm::dvec3 target = tdir * kAp.R;

    struct Flown {
        double d_alt;   // [m] altitude change over the run
        double gamma;   // [deg] final flight-path angle
        double speed;   // [m/s] final airspeed
    };
    const auto fly = [&](drone::BallisticPhase phase) {
        drone::DroneState d;
        d.curr = drone::level_state_at(dp, pos, east);
        d.prev = d.curr;
        d.hp = dp.hp;
        d.engaged = false;
        d.foe = drone::kFoeNone;
        d.raid.active = true;
        d.raid.target_pos = target;
        d.ballistic = phase;
        const double a0 = glm::length(d.curr.position);
        for (int i = 0; i < 1800; ++i) {  // 15 s: past the pitch transient
            drone::tick(d, kAp, dp, nullptr, nullptr);
            REQUIRE(finite3(d.curr.position));
        }
        const glm::dvec3 u = glm::normalize(d.curr.position);
        const double v = glm::length(d.curr.velocity);
        Flown f;
        f.speed = v;
        f.d_alt = glm::length(d.curr.position) - a0;
        f.gamma = v > 1e-6
                      ? std::asin(glm::clamp(
                            glm::dot(d.curr.velocity, u) / v, -1.0, 1.0)) *
                            180.0 / 3.14159265358979323846
                      : 0.0;
        return f;
    };

    const Flown none = fly(drone::BallisticPhase::NONE);
    const Flown climb = fly(drone::BallisticPhase::CLIMB);
    const Flown dive = fly(drone::BallisticPhase::DIVE);
    INFO("NONE d_alt " << none.d_alt << " gamma " << none.gamma
                       << " | CLIMB d_alt " << climb.d_alt << " gamma "
                       << climb.gamma << " | DIVE d_alt " << dive.d_alt
                       << " gamma " << dive.gamma);

    // (1) THE CLIMB CLIMBS, and it climbs because of the PHASE: the same raid
    // order with no phase is flying the pump's own shallow elevation channel.
    // Killing the CLIMB branch collapses climb.d_alt onto none.d_alt.
    CHECK(climb.d_alt > 0.0);
    CHECK(climb.d_alt > none.d_alt + 500.0);
    // ⚠ The COMMAND is avoid_gamma (26 deg); the FLOWN angle settles at ~14
    // because the climb is THRUST-limited at the 85 m/s header cruise, not
    // because the command did not arrive. The bar grades the manoeuvre, not
    // the dial -- fitting it to 26 would grade an aeroplane that does not
    // exist.
    CHECK(climb.gamma > 10.0);

    // (2) THE DIVE DIVES, and it dives STEEPER THAN THE SAME RAID ORDER
    // FLOWN WITHOUT A PHASE. ⚠ THE COMPARISON AGAINST `none` IS THE POINT,
    // NOT AN ABSOLUTE THRESHOLD: an aeroplane pointed at a pump 30 km away
    // and 3 km below is ALREADY descending ~13 deg on aim_at's own elevation
    // channel, so a clause like "dive.gamma < -12" passes with every line of
    // the DIVE steering deleted. MUT-1 (delete the dive's target_gamma) was
    // applied and sailed through exactly that clause; this one collapses --
    // dive.gamma lands on none.gamma to the last digit.
    CHECK(dive.d_alt < 0.0);
    CHECK(dive.gamma < none.gamma - 3.0);
    // ... and it holds the COMMANDED angle rather than saturating past it.
    CHECK(dive.gamma > -30.0);
    // ... and it is a SPRINT, not just a nose-down: the dive's speed target
    // is raid_ballistic_dive_speed, well above the errand speed `none` flies.
    CHECK(dive.speed > none.speed + 20.0);

    // (3) NON-VACUOUS: the two phases must be different manoeuvres, not one
    // shared command wearing two names.
    CHECK(climb.d_alt - dive.d_alt > 1000.0);
    CHECK(climb.gamma > none.gamma + 15.0);
}

// ★★★ RUNG D3 — THE ONE DELIBERATE SCOPE CHANGE, GRADED IN THE AIR IT
// CHANGES. The leg above flies with env == nullptr, so the S1-DECK track law
// never runs there and a mis-keyed exclusion would sail through it green.
// THIS leg gives the drone real ground and a bare deck-only atmosphere, which
// is exactly the state the track law owns: deck scope TRUE, errand tick, and
// therefore -12 deg (deck_track_dive_cap) on the elevation channel unless the
// DIVE is excluded. It is the mutation-catcher for that one predicate.
TEST_CASE("D3 an armed DIVE outdives the deck track law it is excluded from") {
    world::HeightField hf = deck_fx::flat_field();
    sim::AtmosphereField af;  // NO bubbles: the bare global deck everywhere,
                              // so the air at the 400 m release probe is thin
                              // and deck scope latches TRUE -- the exact
                              // condition under which the track law bites.
    sim::Environment env;
    env.ground = &hf;
    env.atm = &af;

    drone::DroneParams dp;
    dp.maverick.enabled = false;
    dp.deck_avoid_agl_enter_m = 60.0;
    dp.deck_avoid_agl_release_m = 110.0;
    dp.deck_track_agl_m = 100.0;
    dp.deck_track_gain = 0.0045;
    dp.deck_track_dive_cap = 12.0 * deck_fx::kPiD / 180.0;
    dp.deck_lookahead_s = 12.0;
    dp.raid_ballistic_climb_agl_m = 2000.0;  // the dead switch, OFF by default
    dp.raid_ballistic_dive_gamma = 18.0 * deck_fx::kPiD / 180.0;

    const glm::dvec3 up{0.0, 0.0, 1.0};
    const glm::dvec3 fwd{1.0, 0.0, 0.0};
    const double th = 30000.0 / hf.R;
    const glm::dvec3 tdir = glm::normalize(up * std::cos(th) + fwd * std::sin(th));

    const auto flown_gamma = [&](drone::BallisticPhase phase) {
        drone::DroneState d;
        // 1,500 m AGL: high above the 100 m hold, so the track law's altitude
        // residual is saturated at its dive cap and the two commands are
        // maximally separated.
        d.curr = drone::level_state_at(
            dp, up * (hf.radius_at(up) + 1500.0), fwd);
        d.prev = d.curr;
        d.hp = dp.hp;
        d.engaged = false;
        d.foe = drone::kFoeNone;
        d.raid.active = true;
        d.raid.target_pos = tdir * hf.radius_at(tdir);
        d.ballistic = phase;
        // ★ GRADED ON THE B2 COMMAND WITNESS (d.cmd_gamma), not on the flown
        // flight path. cmd_gamma is written at the ONE final seam AFTER the
        // terrain-avoid override, the arena clamp and the bank slew, so it IS
        // what this tick asked the aeroplane for -- and it is free of the
        // pitch transient, which overshoots either command and would make the
        // two arms look alike for a reason that is not the predicate.
        double cmd = 0.0;
        for (int i = 0; i < 600; ++i) {  // 5 s -- ends well above the band
            drone::tick(d, kAp, dp, &env, nullptr);
            REQUIRE(finite3(d.curr.position));
            REQUIRE(d.deck_scope);  // non-vacuous: the track law is IN SCOPE
            cmd = d.cmd_gamma * 180.0 / deck_fx::kPiD;
        }
        return cmd;
    };

    const double none_g = flown_gamma(drone::BallisticPhase::NONE);
    const double dive_g = flown_gamma(drone::BallisticPhase::DIVE);
    INFO("track-law errand command " << none_g << " deg ; DIVE " << dive_g
                                     << " deg ; cap -12 ; dive dial -18");
    // (1) THE TRACK LAW REALLY DOES PIN THE ERRAND AT ITS CAP. Without this
    // the clause below could pass on an aeroplane nothing was constraining --
    // it is the non-vacuous half, and it is the reason the dive needed an
    // exclusion at all.
    CHECK(none_g == Catch::Approx(-12.0).margin(0.5));
    // (2) AND THE DIVE GETS PAST IT, at least as steep as its own dial.
    // Mis-key the exclusion in drone::tick (aim it at RUN, or delete it) and
    // this collapses onto none_g at the cap.
    // ⚠ MEASURED -20.05, not -18, and the extra 2 deg is NOT slop: it is the
    // E11 AIR-SEEK (avoid_air_dive_gamma, 0.35 rad = 20.053 deg) taking a
    // min() with the command in thin air. That law can only ever make a
    // descent STEEPER, never shallower, so the clause is a bound and not an
    // equality -- an equality here would be a constant describing another
    // table, which is the trap this ladder has paid for ten times.
    CHECK(dive_g <= -18.0 + 1e-9);
    CHECK(dive_g < none_g - 4.0);
}
