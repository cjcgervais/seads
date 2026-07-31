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
        drone::tick(d, kAp, dp);
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
        drone::tick(d, kAp, dp);
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

    for (int i = 0; i < 480; ++i) drone::tick(d, kAp, dp);  // 4 s
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
        drone::tick(d, kAp, dp);
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
        drone::tick(d, kAp, dp);
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
        drone::tick(d, kAp, dp);
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
        drone::tick(d, kAp, dp);
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

    for (int i = 0; i < 1200; ++i) drone::tick(d, kAp, dp);  // 10 s

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
    for (int i = 0; i < 1200; ++i) drone::tick(ds, kAp, dps);
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
        drone::tick(a, kAp, dp);
        drone::tick(b, kAp, dp);
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
            app::step_frame(lp, accum, frame_dt, fin, pdx, pdy, kAp, kCp, &dw);
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
        app::tick(with, in, kAp, kCp, &dw);
        app::tick(without, in, kAp, kCp, nullptr);
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
        respawned = drone::tick(d, kAp, dp).respawned;
    REQUIRE(respawned);

    const sim::SimState slot = drone::spawn_state(kAp, dp, idx, n);
    REQUIRE(glm::length(d.curr.position - slot.position) < 1e-6);  // OWN slot
    REQUIRE(d.curr.position == d.prev.position);  // no interpolation streak
    REQUIRE(d.grounded);
    REQUIRE(d.age_ticks == 0);
    REQUIRE(sim::altitude(d.curr.position, kAp) > 0.0);  // reborn airborne
}
