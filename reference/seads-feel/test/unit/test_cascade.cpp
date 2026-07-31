// Section 4b — the cascade core, closed-loop through the REAL spherical
// sim::step() (HARNESS §3: never a flat stub). These are the 4b-relevant
// acceptance tests — AT-2 (no oscillation), AT-14a (feedforward kills the
// deadzone limit cycle), AT-17 (BALLISTIC apex, NaN-free + recoverable) —
// plus the structural guarantees: signs propagate from the AT-0 primitives
// through the loop to the emitted Inputs, the deadzone holds trim, and
// control::step never mutates its `internal` argument (SPEC §9.7).
//
// Gains are the untuned STARTING table (overdamped by construction); 4b
// proves oscillation dies structurally, Section 7 tunes for feel.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <cstdio>
#include <utility>

#include "config/load_aircraft.h"
#include "config/load_controller.h"
#include "control/controller.h"
#include "sim/world.h"
#include "test/harness/injector.h"
#include "test/harness/instructor.h"

#ifdef NDEBUG
#error \
    "SEADS gate requires an assert-live build (SPEC 6.1); configure with CMAKE_BUILD_TYPE=Debug"
#endif

namespace {

const sim::AircraftParams kAp =
    cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");
const control::ControllerParams kCp =
    cfg::load_controller_toml(SEADS_CONFIG_DIR "/controller.toml", kAp);

constexpr double kPi = 3.14159265358979323846;
double rad(double d) { return d * kPi / 180.0; }
double deg(double r) { return r * 180.0 / kPi; }

bool finite_state(const sim::SimState& s) {
    auto ok3 = [](const glm::dvec3& v) {
        return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
    };
    return ok3(s.position) && ok3(s.velocity) && ok3(s.angular_vel) &&
           std::isfinite(s.orientation.w) && std::isfinite(s.orientation.x) &&
           std::isfinite(s.orientation.y) && std::isfinite(s.orientation.z);
}

}  // namespace

// ---------------------------------------------------------------------------
// Signs propagate: an off-nose target, one tick from rest (omega = 0), emits
// the Input sign the SPEC §7 table demands. omega = 0 => tau_cmd =
// K_w*omega_des so the emitted Input sign is exactly sign(omega_des) — the
// cascade must not invert what the AT-0 primitives fixed. FINE-regime offsets
// (3 deg) exercise the elevator+rudder pointing; a MANEUVER offset (30 deg)
// exercises the bank-to-turn roll (at 15 deg the airframe banks, not yaws —
// regime matters).
// ---------------------------------------------------------------------------
TEST_CASE("cascade: pointing-demand signs reach the emitted Inputs") {
    const glm::dvec3 up{1.0, 0.0, 0.0};
    const glm::dvec3 heading{0.0, 0.0, -1.0};
    const sim::SimState s0 =
        harness::level_state(kAp, 140.0, 2000.0, up, heading);
    const glm::dvec3 nose = s0.orientation * glm::dvec3{0.0, 0.0, -1.0};
    const glm::dvec3 right = s0.orientation * glm::dvec3{1.0, 0.0, 0.0};
    const glm::dvec3 up_b = s0.orientation * glm::dvec3{0.0, 1.0, 0.0};

    auto one_tick = [&](const glm::dvec3& target) {
        control::Input in;
        in.target_dir_world = target;
        in.throttle = 0.7;
        return control::step(s0, in, control::reset(), kAp, kCp, kAp.sim_dt);
    };

    // FINE (3 deg): elevator + rudder point directly.
    // 3 deg RIGHT -> yaw-right = -omega_y -> Input.yaw < 0.
    auto o = one_tick(glm::angleAxis(-rad(3.0), up_b) * nose);
    CHECK(o.inputs.yaw < 0.0f);
    // 3 deg LEFT -> Input.yaw > 0.
    o = one_tick(glm::angleAxis(rad(3.0), up_b) * nose);
    CHECK(o.inputs.yaw > 0.0f);
    // 3 deg UP -> pitch-up = +omega_x -> Input.pitch > 0.
    o = one_tick(glm::angleAxis(rad(3.0), right) * nose);
    CHECK(o.inputs.pitch > 0.0f);
    // 3 deg DOWN -> Input.pitch < 0.
    o = one_tick(glm::angleAxis(-rad(3.0), right) * nose);
    CHECK(o.inputs.pitch < 0.0f);

    // MANEUVER (30 deg): bank-to-turn. Target RIGHT -> roll-right = -omega_z
    // -> Input.roll < 0.
    o = one_tick(glm::angleAxis(-rad(30.0), up_b) * nose);
    CHECK(o.inputs.roll < 0.0f);
    // Target LEFT -> roll-left -> Input.roll > 0.
    o = one_tick(glm::angleAxis(rad(30.0), up_b) * nose);
    CHECK(o.inputs.roll > 0.0f);
}

// ---------------------------------------------------------------------------
// MB-rud: the bank-aligned rudder gate + the shared yaw ceiling (Mission B,
// docs/mission_b_instructor_plan.md D1/D2). One level state, one tick from
// reset, two 30-deg MANEUVER aims that differ ONLY in their lateral direction
// theta (measured from body-up; bank_error == theta by construction):
//   theta = 45 deg  (up-right)   -> cos(bankErr) = +0.707 -> yaw_gate = 1
//   theta = 105 deg (down-right) -> cos(bankErr) = -0.259 < -yaw_align_band
//                                   -> yaw_gate = yaw_min_frac
// 105 deg sits INSIDE the (90, push_gate_bank=120] window: past knife-edge
// (gate at its floor) yet below the push-entry bank guard, so the leg pins the
// bank-to-turn branch, not the push branch. Both aims saturate sqrt_law at
// yaw_max (K_theta*yaw_scale*|e_y| > yaw_max for each), so:
//   * the ALIGNED pointed yaw == yaw_max EXACTLY — pins D1's ceiling
//     re-ordering (mutation: revert to the old post-scale `sqrt_law(...,
//     K_theta, ...) * yaw_scale` -> reads yaw_scale*yaw_max = 1.36, FAILS);
//   * the ratio anti-aligned/aligned == yaw_min_frac — pins the gate KEY
//     (mutation: re-key to the old blend fade -> ratio 0.8; gate deleted ->
//     ratio 1.0; both FAIL). Config-relative: both pins track any
//     yaw_max/yaw_scale/yaw_min_frac retune.
// beta == 0 (velocity has no lateral component) so coordination contributes
// nothing; the curvature feedforward is reproduced exactly and subtracted, so
// the pointed term is isolated (its yaw component is ~0 here anyway — level
// flight's ff is pitch-down).
// ---------------------------------------------------------------------------
TEST_CASE("MB-rud: bank-aligned rudder gate + shared yaw ceiling") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    const sim::SimState s0 =
        harness::level_state(kAp, 140.0, 2000.0, up, heading);
    const glm::dvec3 nose = s0.orientation * glm::dvec3{0.0, 0.0, -1.0};
    const glm::dvec3 right = s0.orientation * glm::dvec3{1.0, 0.0, 0.0};
    const glm::dvec3 up_b = s0.orientation * glm::dvec3{0.0, 1.0, 0.0};

    // The controller's own feedforward, reproduced (SPEC §9.3) to isolate the
    // pointed yaw from omega_des_total.
    const glm::dvec3 local_up = glm::normalize(s0.position);
    const double ff_y =
        sim::body_dir_of(s0.orientation,
                         glm::cross(local_up, s0.velocity) / kAp.R)
            .y;

    auto pointed_yaw = [&](double theta_deg, double off_deg) {
        const glm::dvec3 lat =
            std::sin(rad(theta_deg)) * right + std::cos(rad(theta_deg)) * up_b;
        const glm::dvec3 target = glm::normalize(std::cos(rad(off_deg)) * nose +
                                                 std::sin(rad(off_deg)) * lat);
        control::Input in;
        in.target_dir_world = target;
        in.throttle = 0.7;
        const control::Output o =
            control::step(s0, in, control::reset(), kAp, kCp, kAp.sim_dt);
        return o.telem.omega_des.y - ff_y;
    };

    const double yA = pointed_yaw(45.0, 30.0);   // aligned side: gate = 1
    const double yB = pointed_yaw(105.0, 30.0);  // past knife-edge: floor
    std::printf(
        "[MB-rud gate] pointed yaw aligned=%.4f anti=%.4f ratio=%.3f "
        "(yaw_max=%.4f min_frac=%.2f)\n",
        yA, yB, yB / yA, kCp.yaw_max, kCp.yaw_min_frac);
    // Aligned: saturated at the SHARED ceiling exactly (aim right -> -omega_y).
    CHECK(yA == Catch::Approx(-kCp.yaw_max).margin(1e-9));
    // Past knife-edge: the same saturated law scaled by the gate floor.
    CHECK(yB / yA == Catch::Approx(kCp.yaw_min_frac).margin(1e-6));

    // BAND-PLACEMENT pins (diff red-team P1-1: a smoothstep(0,+band) or
    // symmetric (-band,+band) mutant passed BOTH samples above bit-identically
    // — 45 deg and 105 deg sit outside every candidate band placement, and
    // the only failing artifact was the golden, which MB-4's retune re-records
    // — the "re-record blesses the bug" trap). Oracle = the SPEC'd gate
    // formula recomputed from CONFIG (an independent path — a code-side band
    // mutant diverges from it):
    //   theta = 88 deg: cos = +0.035, INSIDE a wrongly-(0,+band) mutant's
    //     fade — the shipped (-band, 0] gate must read EXACTLY 1 there
    //     (Chad's Q1b: full crab all the way TO knife-edge);
    //   theta = 95 deg: cos = -0.087, mid-fade — strictly between the floor
    //     and 1, equal to the formula.
    auto expected_gate = [&](double theta_deg) {
        const double c = std::cos(rad(theta_deg));
        const double t = std::clamp(
            (c - (-kCp.yaw_align_band)) / (0.0 - (-kCp.yaw_align_band)), 0.0,
            1.0);
        return kCp.yaw_min_frac +
               (1.0 - kCp.yaw_min_frac) * (t * t * (3.0 - 2.0 * t));
    };
    const double y88 = pointed_yaw(88.0, 30.0);
    const double y95 = pointed_yaw(95.0, 30.0);
    std::printf(
        "[MB-rud band] ratio(88)=%.4f (expect 1) ratio(95)=%.4f "
        "(expect %.4f)\n",
        y88 / yA, y95 / yA, expected_gate(95.0));
    CHECK(y88 / yA == Catch::Approx(1.0).margin(1e-6));
    CHECK(expected_gate(88.0) == 1.0);  // premise: 88 deg is out of the band
    CHECK(y95 / yA == Catch::Approx(expected_gate(95.0)).margin(1e-6));
    CHECK(expected_gate(95.0) > kCp.yaw_min_frac + 0.05);  // genuinely mid-
    CHECK(expected_gate(95.0) < 0.95);                     // fade, both ways
}

// MB-rud: coordination stays OUTSIDE the gate (diff red-team P1-2: a mutant
// wrapping the whole yaw sum in the gate factor — yaw = factor*(coord +
// pointed) — survived the ENTIRE 218-case gate, golden included: aligned
// flight has factor == 1 so the scripted flight is bit-identical, and the
// split-S legs never read beta. The SPEC contract "coordination always on,
// OUTSIDE the pointing gate" was pinned for deadzone/override/ballistic but
// not for the new MANEUVER-branch scaling — the 4c "grep every branch that
// does the gated thing" class). Two one-tick runs at the gate-FLOOR aim
// (theta=105) from the same orientation, differing only in a seeded ~10 deg
// sideslip: the yaw DELTA must equal the FULL coordination term
// K_coord*(-beta) — pointed yaw cancels (same orientation, same aim) and each
// run subtracts its own feedforward. Mutation: the wrap scales the delta by
// ~the gate floor (0.1x) and the equality fails.
TEST_CASE("MB-rud: coordination rides OUTSIDE the bank-aligned gate") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    const sim::SimState s0 =
        harness::level_state(kAp, 140.0, 2000.0, up, heading);
    const glm::dvec3 nose = s0.orientation * glm::dvec3{0.0, 0.0, -1.0};
    const glm::dvec3 right = s0.orientation * glm::dvec3{1.0, 0.0, 0.0};
    const glm::dvec3 up_b = s0.orientation * glm::dvec3{0.0, 1.0, 0.0};
    // Gate-floor aim: theta = 105 deg, 30 deg off the nose (as above).
    const glm::dvec3 lat =
        std::sin(rad(105.0)) * right + std::cos(rad(105.0)) * up_b;
    const glm::dvec3 target =
        glm::normalize(std::cos(rad(30.0)) * nose + std::sin(rad(30.0)) * lat);

    // Slip variant: same orientation/speed, velocity yawed so beta ~ +10 deg
    // (velocity right of the nose: v_b = V*(sin b, 0, -cos b)).
    sim::SimState s_slip = s0;
    const double b = rad(10.0);
    s_slip.velocity =
        s0.orientation * (140.0 * glm::dvec3{std::sin(b), 0.0, -std::cos(b)});

    auto yaw_minus_ff = [&](const sim::SimState& s) {
        const glm::dvec3 local_up = glm::normalize(s.position);
        const double ff_y =
            sim::body_dir_of(s.orientation,
                             glm::cross(local_up, s.velocity) / kAp.R)
                .y;
        control::Input in;
        in.target_dir_world = target;
        in.throttle = 0.7;
        const control::Output o =
            control::step(s, in, control::reset(), kAp, kCp, kAp.sim_dt);
        return std::pair<double, double>{o.telem.omega_des.y - ff_y,
                                         o.telem.extracted.beta};
    };
    const auto [y_clean, beta_clean] = yaw_minus_ff(s0);
    const auto [y_slip, beta_slip] = yaw_minus_ff(s_slip);
    REQUIRE(std::abs(beta_clean) < 1e-9);
    REQUIRE(beta_slip == Catch::Approx(rad(10.0)).margin(1e-9));
    const double delta = y_slip - y_clean;
    std::printf("[MB-rud coord] delta=%.5f expect=%.5f (beta=%.2f deg)\n",
                delta, -kCp.K_coord * beta_slip, deg(beta_slip));
    CHECK(delta == Catch::Approx(-kCp.K_coord * beta_slip).margin(1e-9));
}

// MB-rud: the PUSH branch's yaw is UNGATED at full scaled law (diff red-team
// P2-1: applying the gate factor in the push branch too passed every test —
// the single-tick push legs all use dead-below aims where demand.y == 0, and
// the AT-15 push legs read pitch/G/edges, never yaw). One tick, an off-axis
// below aim that ARMS push (60 deg down + 15 deg lateral: inside the 22.5 deg
// side cone, |bankErr| = 163 deg > push_gate_bank, below the horizon, ahead
// of the wing-line): the emitted pointed yaw must be the FULL sqrt_law —
// saturated at the shared ceiling — not the gate-floored fraction (a
// gate-in-push mutant emits ~0.1x and fails).
TEST_CASE("MB-rud: push-branch yaw is ungated (full law, shared ceiling)") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    const sim::SimState s0 =
        harness::level_state(kAp, 140.0, 2000.0, up, heading);
    // Body-frame construction: 60 deg pitch-down in the vertical plane, then
    // 15 deg lateral: t_b = (sin15, -sin60*cos15, -cos60*cos15).
    const glm::dvec3 t_b{std::sin(rad(15.0)),
                         -std::sin(rad(60.0)) * std::cos(rad(15.0)),
                         -std::cos(rad(60.0)) * std::cos(rad(15.0))};
    const glm::dvec3 target = s0.orientation * t_b;
    const glm::dvec3 local_up = glm::normalize(s0.position);
    const double ff_y =
        sim::body_dir_of(s0.orientation,
                         glm::cross(local_up, s0.velocity) / kAp.R)
            .y;
    control::Input in;
    in.target_dir_world = target;
    in.throttle = 0.7;
    const control::Output o =
        control::step(s0, in, control::reset(), kAp, kCp, kAp.sim_dt);
    REQUIRE(o.telem.push_mode);  // premise: this aim ARMS push on tick 1
    const double pointed = o.telem.omega_des.y - ff_y;
    std::printf("[MB-rud push-yaw] pointed=%.4f expect=%.4f push=%d\n", pointed,
                -kCp.yaw_max, int(o.telem.push_mode));
    // demand.y = -sin15/sqrt(sin15^2+(sin60 cos15)^2) * acos(cos60 cos15)
    //          = -0.315 rad; K_theta*yaw_scale*0.315 = 1.21 > yaw_max ->
    // saturated at the shared ceiling, UNGATED.
    CHECK(pointed == Catch::Approx(-kCp.yaw_max).margin(1e-9));
}

// MB-rud continuity (plan red-team P0-1): the gate is blend-composed like the
// pitch align gate, so the pointed yaw is CONTINUOUS across err == blend_lo
// even for an aim past knife-edge (theta = 105 deg, where the gate itself sits
// at its yaw_min_frac floor). A bare `have_bank ? gate : 1` steps by
// ~(1-yaw_min_frac) of the pointed yaw across a 0.2 deg error change — the
// McRuer transition trap. Mutation: replace the composed factor with the bare
// gate -> the 5.1 deg tick emits ~0.1x the 4.9 deg tick's yaw and the delta
// blows past the bound (0.28 rad/s vs the honest 0.013).
TEST_CASE("MB-rud: gate is continuous across the FINE->MANEUVER boundary") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    const sim::SimState s0 =
        harness::level_state(kAp, 140.0, 2000.0, up, heading);
    const glm::dvec3 nose = s0.orientation * glm::dvec3{0.0, 0.0, -1.0};
    const glm::dvec3 right = s0.orientation * glm::dvec3{1.0, 0.0, 0.0};
    const glm::dvec3 up_b = s0.orientation * glm::dvec3{0.0, 1.0, 0.0};
    const glm::dvec3 local_up = glm::normalize(s0.position);
    const double ff_y =
        sim::body_dir_of(s0.orientation,
                         glm::cross(local_up, s0.velocity) / kAp.R)
            .y;
    auto pointed_yaw = [&](double off_deg) {
        const glm::dvec3 lat =
            std::sin(rad(105.0)) * right + std::cos(rad(105.0)) * up_b;
        const glm::dvec3 target = glm::normalize(std::cos(rad(off_deg)) * nose +
                                                 std::sin(rad(off_deg)) * lat);
        control::Input in;
        in.target_dir_world = target;
        in.throttle = 0.7;
        const control::Output o =
            control::step(s0, in, control::reset(), kAp, kCp, kAp.sim_dt);
        return o.telem.omega_des.y - ff_y;
    };
    const double y_below = pointed_yaw(4.9);  // blend = 0 (FINE side)
    const double y_above = pointed_yaw(5.1);  // blend ~ 0.002 (MANEUVER side)
    std::printf(
        "[MB-rud continuity] yaw(4.9deg)=%.4f yaw(5.1deg)=%.4f "
        "delta=%.4f\n",
        y_below, y_above, std::abs(y_above - y_below));
    CHECK(std::abs(y_above - y_below) < 0.03);
}

// ---------------------------------------------------------------------------
// MB-right: inverted auto-righting (Chad 2026-07-07: "roll over on bank after
// about 2 s no gross inputs if belly up... slow roll off ailerons"; scoped
// amendment of S7-loop-invert — [auto_level] inverted_delay/inverted_rate).
// Closed-loop on the real spherical plant, belly-up (170 deg bank) with the
// aim parked on the nose:
//   Phase A — within the delay window the old ruling HOLDS: no righting, the
//     plane stays inverted (mutation: `>= inverted_delay` -> `>= 0` fires the
//     roll immediately -> the no-righting-before-delay assert FAILS).
//   Phase B — after the delay the latch fires and the roll is SLOW: every
//     righting tick's |omega_des.z| <= inverted_rate (+ the ~V/R feedforward
//     crumb) (mutation: clamp +/-inverted_rate -> +/-p_max saturates at ~7.6
//     rad/s -> FAILS), and the plane reaches upright and stays (mechanism
//     deleted -> never rights -> FAILS).
//   Phase C — a real mouse deflection CANCELS it (err > blend_hi disarm leg;
//     mutation: drop that leg -> righting stays latched -> FAILS), and the
//     rest timer restarts from zero afterward.
// Knob-off arm: inverted_rate = 0 -> the latch never arms and the plane stays
// inverted for the whole window — the S7-loop-invert behavior preserved
// structurally (the strict-superset proof shape).
// ---------------------------------------------------------------------------
TEST_CASE("MB-right: belly-up rest slow-rights after the delay") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    const sim::SimState s0 = harness::flight_state(
        kAp, 140.0, 3000.0, up, heading, rad(170.0), 0.0, 0.0);
    const int delay_ticks =
        static_cast<int>(kCp.inverted_delay / kAp.sim_dt + 0.5);

    SECTION("arms after the delay, rolls slow, reaches upright") {
        harness::ClosedLoop cl(s0, glm::dvec3{0.0, 0.0, -1.0});
        cl.aim_nose();
        cl.tick(0.7, kAp, kCp, /*grounded=*/true);  // capture held_bank
        // Phase A: strictly inside the delay window (allow a couple of ticks
        // of slack for the rest-condition to establish), NO righting and the
        // plane stays belly-up.
        bool early_righting = false;
        double cpt_at_a_end = 0.0;
        for (int i = 0; i < delay_ticks - 12; ++i) {
            const control::Telemetry t = cl.tick(0.7, kAp, kCp);
            early_righting = early_righting || t.righting;
            cpt_at_a_end = t.extracted.cos_phi_theta;
        }
        CHECK_FALSE(early_righting);
        CHECK(cpt_at_a_end < -0.8);  // still inverted (S7-loop-invert holds)
        // Phase B: the latch fires within ~0.5 s past the delay; while
        // righting, the roll command stays inside the slow clamp; upright is
        // reached and kept.
        bool fired = false;
        bool prev_righting = false;
        double max_righting_roll = 0.0;
        double handoff_roll = -1.0;  // max |roll cmd| over the DONE handoff
                                     // tick + the next 3 (see below)
        int handoff_tick = -1;
        double cpt_final = -1.0;
        int fire_tick = -1;
        for (int i = 0; i < 1200; ++i) {
            const control::Telemetry t = cl.tick(0.7, kAp, kCp);
            REQUIRE(finite_state(cl.state));
            if (t.righting) {
                if (!fired) fire_tick = i;
                fired = true;
                max_righting_roll =
                    std::max(max_righting_roll, std::abs(t.omega_des.z));
            }
            if (prev_righting && !t.righting && handoff_tick < 0) {
                handoff_tick = i;
            }
            // The handoff pin covers the DONE tick AND the next 3: the
            // recapture-delete mutant emits 0 on the DONE tick itself but
            // p_max (~4.5 rad/s) one tick later (held_bank stayed ~0, so the
            // auto-level demand is the whole ~87 deg gap); the honest
            // recaptured tail reads 0 / 0.16 / 0.31 / ~0.47 here.
            if (handoff_tick >= 0 && i <= handoff_tick + 3) {
                handoff_roll = std::max(handoff_roll, std::abs(t.omega_des.z));
            }
            prev_righting = t.righting;
            cpt_final = t.extracted.cos_phi_theta;
        }
        std::printf(
            "[MB-right] fired@+%d ticks, max |roll cmd| %.3f rad/s "
            "(clamp %.3f), final cosPhiTheta %.3f\n",
            fire_tick, max_righting_roll, kCp.inverted_rate, cpt_final);
        CHECK(fired);
        CHECK(fire_tick <= 72);  // ~<=0.6 s past the delay boundary
        // + the curvature-feedforward crumb (|ff| ~ V/R ~ 0.01) — the clamp
        // applies to the pointing roll, ff is added after by design.
        CHECK(max_righting_roll <= kCp.inverted_rate + 0.02);
        // HANDOFF continuity (red-team P1-1/P2-1): the DONE tick and the 3
        // ticks after it must stay inside the slow clamp. Mutations: the old
        // outside-the-apply-block DONE leg emits the branch roll against the
        // decayed setpoint ON the DONE tick (~4.5 rad/s, p_max-clamped) —
        // FAILS; deleting the disarm RECAPTURE emits 0 on the DONE tick but
        // ~4.5 one tick later — the +3 window FAILS it. The later auto-level
        // tail (existing flown behavior, peaks ~2.6 rad/s from ~87 deg) is
        // outside the window and deliberately unbounded here.
        REQUIRE(handoff_roll >= 0.0);  // the handoff was observed
        CHECK(handoff_roll <= kCp.inverted_rate + 0.02);
        CHECK(cpt_final > 0.8);  // rolled up through knife-edge and settled
    }

    SECTION("knife-edge band does not arm (P3-1: cos in (-band, 0))") {
        // OPEN-LOOP on a FIXED state (the S6 latch discipline: a closed
        // chase escapes the boundary before a mutant can fire — the partial
        // wings-leveling rolls the plane upright inside the delay, so a
        // closed-loop version of this leg passes under the very mutant it
        // claims to catch). Iterate control::step on the SAME 91.4 deg-bank
        // state (cos_phi_theta = -band/2, inside the knife-edge fade band):
        // the shipped arm threshold (cos < -band) never accumulates; the
        // mutant (cos < 0) accumulates and FIRES after the delay.
        // flight_state's bank IS the roll about the nose, so cos_phi_theta =
        // cos(bank): bank = acos(-band/2) ~ 91.4 deg exactly.
        const double bank91 = std::acos(-kCp.wings_level_band / 2.0);
        const sim::SimState sk = harness::flight_state(
            kAp, 140.0, 3000.0, up, heading, bank91, 0.0, 0.0);
        control::Input in;
        in.target_dir_world = sk.orientation * glm::dvec3{0.0, 0.0, -1.0};
        in.throttle = 0.7;
        control::Internal internal = control::reset();
        internal.held_bank = bank91;  // as a GROUNDED capture would set it
        bool any_righting = false;
        for (int i = 0; i < delay_ticks + 240; ++i) {
            const control::Output o =
                control::step(sk, in, internal, kAp, kCp, kAp.sim_dt);
            internal = o.internal;
            any_righting = any_righting || o.telem.righting;
        }
        CHECK_FALSE(any_righting);
    }

    SECTION("a real mouse deflection cancels it; the timer restarts") {
        harness::ClosedLoop cl(s0, glm::dvec3{0.0, 0.0, -1.0});
        cl.aim_nose();
        cl.tick(0.7, kAp, kCp, /*grounded=*/true);
        // Arm: rest past the delay until it fires.
        bool fired = false;
        for (int i = 0; i < delay_ticks + 120 && !fired; ++i) {
            fired = cl.tick(0.7, kAp, kCp).righting;
        }
        REQUIRE(fired);
        // Deflect the aim well past blend_hi (15 deg lateral) -> disarm.
        const glm::dvec3 nose = cl.state.orientation * glm::dvec3{0, 0, -1};
        const glm::dvec3 up_b = cl.state.orientation * glm::dvec3{0, 1, 0};
        cl.aim = glm::normalize(glm::angleAxis(-rad(15.0), up_b) * nose);
        const control::Telemetry t1 = cl.tick(0.7, kAp, kCp);
        CHECK_FALSE(t1.righting);
        // Back to rest: the timer must restart — no re-fire inside half the
        // delay window.
        cl.aim_nose();
        bool refired = false;
        for (int i = 0; i < delay_ticks / 2; ++i) {
            refired = refired || cl.tick(0.7, kAp, kCp).righting;
        }
        CHECK_FALSE(refired);
    }

    SECTION("knob-off: inverted_rate = 0 stays inverted (old ruling)") {
        control::ControllerParams cp2 = kCp;
        cp2.inverted_rate = 0.0;
        harness::ClosedLoop cl(s0, glm::dvec3{0.0, 0.0, -1.0});
        cl.aim_nose();
        cl.tick(0.7, kAp, cp2, /*grounded=*/true);
        bool any_righting = false;
        double cpt_final = 0.0;
        for (int i = 0; i < delay_ticks + 600; ++i) {
            const control::Telemetry t = cl.tick(0.7, kAp, cp2);
            any_righting = any_righting || t.righting;
            cpt_final = t.extracted.cos_phi_theta;
        }
        CHECK_FALSE(any_righting);
        CHECK(cpt_final < -0.8);  // still belly-up: S7-loop-invert preserved
    }
}

// ---------------------------------------------------------------------------
// A banked airframe with a small FINE error (not deadzoned): the wings-hold
// rolls back toward level. +20 deg bank (right wing down), heldBank = 0 on the
// fresh internal -> roll LEFT = +omega_z -> Input.roll > 0. (A target exactly
// on the nose would deadzone and zero the pointing term — a small off-nose
// error keeps FINE pursuit live.)
// ---------------------------------------------------------------------------
TEST_CASE("cascade: FINE wings-hold rolls a banked airframe toward level") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    sim::SimState s = harness::flight_state(kAp, 140.0, 2000.0, up, heading,
                                            rad(20.0), 0.0, 0.0);
    const glm::dvec3 nose = s.orientation * glm::dvec3{0.0, 0.0, -1.0};
    const glm::dvec3 right = s.orientation * glm::dvec3{1.0, 0.0, 0.0};
    control::Input in;
    in.target_dir_world = glm::angleAxis(rad(2.0), right) * nose;  // 2 deg up
    in.throttle = 0.7;
    const control::Output o =
        control::step(s, in, control::reset(), kAp, kCp, kAp.sim_dt);
    CHECK(o.telem.extracted.phi == Catch::Approx(rad(20.0)).margin(1e-6));
    CHECK_FALSE(o.telem.deadzoned);
    CHECK(o.inputs.roll > 0.0f);  // roll LEFT toward level
}

// Smooth wings auto-level at rest (§7 Item-2 Part B): with the mouse at rest
// (aim on the nose) and upright, the held-bank wings-hold setpoint decays
// toward 0 and the wings ease to LEVEL — hands-off flight settles
// straight-and-level, not held at a residual bank. The decay covers err <
// blend_lo (not just the deadzone) because the residual bank's turn sweeps the
// aim a few tenths of a degree off the nose; a deadzone-only decay stalls there
// (~7 deg).
TEST_CASE("cascade: wings auto-level to level when the mouse is at rest") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    sim::SimState s0 = harness::flight_state(kAp, 140.0, 3000.0, up, heading,
                                             rad(15.0), 0.0, 0.0);
    harness::ClosedLoop cl(s0, glm::dvec3{0.0, 0.0, -1.0});
    cl.aim_nose();                      // mouse at rest: world aim = the nose
    cl.internal.held_bank = rad(15.0);  // as MANEUVER->FINE capture leaves it
    double phi = rad(15.0), max_roll_rate = 0.0;
    for (int i = 0; i < 600; ++i) {  // 5 s
        const control::Telemetry t = cl.tick(0.7, kAp, kCp);
        REQUIRE(finite_state(cl.state));
        phi = t.extracted.phi;
        max_roll_rate = std::max(max_roll_rate, std::abs(t.omega_des.z));
    }
    std::printf("[autolevel] final phi=%.2f deg  max_roll_rate=%.1f deg/s\n",
                deg(phi), deg(max_roll_rate));
    CHECK(std::abs(phi) < rad(1.0));  // leveled from 15 deg
    // Gentle: the decay-limited peak demand is analytically ~rate*phi0/2 (the
    // wings track the exponential setpoint; measured 22.5 deg/s at rate 3.0,
    // 33.3 at 4.5 — both = rate*15/2). Bound derived from the config the
    // mechanism reads with 1.5x margin (config-relative-bounds lesson: a
    // fixed 30 deg/s constant false-failed the rate 3.0->4.5 retune), PLUS
    // the absolute never-a-hard-slam guard: well under K_phi*phi0, the
    // un-decayed slam this mechanism exists to prevent.
    const double decay_peak = kCp.auto_level_rate * rad(15.0) / 2.0;
    CHECK(max_roll_rate < 1.5 * decay_peak);
    CHECK(max_roll_rate < 0.8 * kCp.K_phi * rad(15.0));
}

// An override SUSPENDS the auto-level (the "unless a key is pressed" rule): a
// held key sets pursuit=false, so held_bank does not decay and the wings are
// not commanded level — the pilot owns the airframe.
TEST_CASE("cascade: a keyboard override suspends the wings auto-level") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    sim::SimState s0 = harness::flight_state(kAp, 140.0, 3000.0, up, heading,
                                             rad(15.0), 0.0, 0.0);
    harness::ClosedLoop cl(s0, glm::dvec3{0.0, 0.0, -1.0});
    cl.aim_nose();
    cl.internal.held_bank = rad(15.0);
    cl.hold_override(0, +1.0);       // hold a PITCH override (not roll)
    for (int i = 0; i < 240; ++i) {  // 2 s held
        cl.tick(0.7, kAp, kCp);
        REQUIRE(finite_state(cl.state));
    }
    // held_bank did NOT decay toward level (auto-level suspended by the
    // override).
    std::printf("[autolevel-ovr] held_bank=%.2f deg (started 15)\n",
                deg(cl.internal.held_bank));
    CHECK(cl.internal.held_bank == Catch::Approx(rad(15.0)).margin(rad(0.5)));
}

// ---------------------------------------------------------------------------
// AT-2 — no oscillation. A 30 deg pitch step at cruise settles with no limit
// cycle: error decays to small and does not sustain oscillation.
// ---------------------------------------------------------------------------
TEST_CASE("AT-2: 30 deg pitch step settles without oscillation") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    double thr = 0.7;
    const sim::SimState s0 =
        harness::level_trim_state(kAp, 140.0, 3000.0, up, heading, &thr);
    harness::ClosedLoop cl(s0, glm::dvec3{0.0, 0.0, -1.0});
    cl.aim_nose();

    // Let it trim for 1 s, then command a 30 deg pitch-up step (rotate the aim
    // about body_right).
    for (int i = 0; i < 120; ++i) cl.tick(thr, kAp, kCp);
    const glm::dvec3 right = cl.state.orientation * glm::dvec3{1.0, 0.0, 0.0};
    cl.aim = glm::normalize(glm::angleAxis(rad(30.0), right) * cl.aim);

    double err_peak = 0.0, err_final = 0.0;
    int reversals = 0, tail_reversals = 0;
    double prev_err = 0.0, prev_derr = 0.0, tail_peak = 0.0;
    // Reversal floor (S-rimshot v2 re-parameterization, red-team F9): count
    // error-slope flips ONLY while the error is ABOVE the park scale
    // (2 x deadzone_hi). The bare count was contaminated by sub-deadzone
    // park taps (the Y3 instrument lesson — 13 micro-flips at ~5e-4 rad on
    // a clean settle); the floor keeps the REAL signals: a limit cycle
    // rings at degree scale, and the deliberate rimshot rebound contributes
    // exactly 2 flips (crossing -> rim -> center).
    const double rev_floor = 2.0 * kCp.deadzone_hi;
    for (int i = 0; i < 720; ++i) {  // 6 s
        const control::Telemetry t = cl.tick(thr, kAp, kCp);
        REQUIRE(finite_state(cl.state));
        err_peak = std::max(err_peak, t.e);
        const double derr = t.e - prev_err;
        if (i > 60 && t.e > rev_floor && derr * prev_derr < 0.0) {
            ++reversals;
            // QUIET TAIL (red-team F9 — the anti-launder pin): a finite
            // rebound finishes early; a sustained hunt (the refractory-
            // failure 7 Hz coast re-engage class, rim-amplitude ~0.5 deg —
            // ABOVE the floor) keeps flipping forever. No above-floor flip
            // may occur in the final 2 s.
            if (i >= 480) ++tail_reversals;
        }
        if (i >= 480) tail_peak = std::max(tail_peak, t.e);
        prev_derr = derr;
        prev_err = t.e;
        err_final = t.e;
    }
    CAPTURE(err_peak, err_final, reversals, tail_reversals, tail_peak);
    CHECK(err_peak > rad(20.0));  // the step really happened
    CHECK(err_final < rad(1.0));  // and it settled
    // Not a limit cycle: legacy settles with <= 3 above-floor flips; the
    // rimshot's one deliberate rebound adds 2 (rim out, center back).
    CHECK(reversals <= 5);
    CHECK(tail_reversals == 0);          // the quiet tail
    CHECK(tail_peak < 2.0 * rev_floor);  // parked, not weaving
}

// ---------------------------------------------------------------------------
// AT-14a — feedforward kills the deadzone limit cycle. 60 s of level flight:
// the transported level aim + curvature feedforward hold the nose parked in
// the deadzone; without feedforward the nose drifts out and hunts forever.
// ---------------------------------------------------------------------------
TEST_CASE("AT-14a: level flight parks in the deadzone (feedforward live)") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    double thr = 0.7;
    const sim::SimState s0 =
        harness::level_trim_state(kAp, 140.0, 3000.0, up, heading, &thr);
    harness::ClosedLoop cl(s0, glm::dvec3{0.0, 0.0, -1.0});
    cl.aim_nose();

    int dz_exits_after_settle = 0;
    bool prev_dz = false;
    double err_after_settle = 0.0;
    double alt_min = 1e9, alt_max = -1e9, ffx_sum = 0.0;
    int ff_n = 0;
    for (int i = 0; i < 7200; ++i) {  // 60 s
        const control::Telemetry t = cl.tick(thr, kAp, kCp);
        REQUIRE(finite_state(cl.state));
        const double alt = sim::altitude(cl.state.position, kAp);
        if (i > 3600) {  // after 30 s settle
            err_after_settle = std::max(err_after_settle, t.e);
            if (prev_dz && !t.deadzoned) ++dz_exits_after_settle;
            alt_min = std::min(alt_min, alt);
            alt_max = std::max(alt_max, alt);
            if (t.deadzoned) {
                ffx_sum += t.omega_des.x;
                ++ff_n;
            }
        }
        prev_dz = t.deadzoned;
    }
    REQUIRE(ff_n > 0);
    const double mean_ffx = ffx_sum / ff_n;
    // Oracle divisor is the ACTUAL orbital radius |position| (= R + altitude),
    // matching the controller's corrected curvature ff (radius-error fix,
    // auto/feel-research). At the ~3000 m park altitude V/|position| is
    // R/(R+3000) = 83% of the old V/R — the honest level-flight curvature.
    const double vr =
        glm::length(cl.state.velocity) / glm::length(cl.state.position);
    std::printf(
        "[AT-14a] err_max=%.6g deg  dz_exits=%d  alt=[%.1f,%.1f]  "
        "mean omega_des.x(dz)=%.6g  -V/R=%.6g\n",
        err_after_settle * 180.0 / kPi, dz_exits_after_settle, alt_min, alt_max,
        mean_ffx, -vr);
    // Pointing error stays bounded right AT the deadzone (never grows to the
    // degrees-scale a feedforward-less controller hunts to): the deadzone is
    // doing its job, not limit-cycling.
    CHECK(err_after_settle < kCp.deadzone_hi * 1.05);
    // Altitude holds tightly — a wrong-sign feedforward would loop or dive.
    CHECK(alt_max - alt_min < 100.0);
    // The smoking gun that feedforward is LIVE and correct: in the deadzone
    // (pointing term zeroed) the ONLY commanded pitch rate is the curvature
    // feedforward — pitch-DOWN at ~V/R. Zero it and level flight limit-cycles.
    CHECK(mean_ffx < 0.0);
    CHECK(mean_ffx == Catch::Approx(-vr).epsilon(0.15));
    // No FAST hunting (the pathological cycle exits the deadzone ~6x/s, SPEC
    // §13; here it is bounded far below that by the feedforward).
    CHECK(dz_exits_after_settle < 60);
    CHECK(sim::altitude(cl.state.position, kAp) > 2000.0);
}

// ---------------------------------------------------------------------------
// AT-17 — the apex / BALLISTIC golden. A scripted zoom to v < v_ballistic:
// NaN-free throughout, the ballistic flag latches, the nose stays
// recoverable (attitude-hold acts), and the cascade re-enters cleanly on the
// way down.
// ---------------------------------------------------------------------------
TEST_CASE("AT-17: hammerhead through the v->0 apex stays sane") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    // Point the nose near-vertical (zoom climb) and aim there; thrust + the
    // pull bleeds speed toward zero at the apex.
    sim::SimState s0 = harness::level_state(kAp, 200.0, 3000.0, up, heading);
    const glm::dvec3 right = s0.orientation * glm::dvec3{1.0, 0.0, 0.0};
    s0.orientation =
        glm::normalize(glm::angleAxis(rad(80.0), right) * s0.orientation);
    s0.velocity = 200.0 * (s0.orientation * glm::dvec3{0.0, 0.0, -1.0});
    s0.last_vhat = glm::normalize(s0.velocity);

    harness::ClosedLoop cl(s0, glm::dvec3{0.0, 0.0, -1.0});
    cl.aim_nose();  // hold the climb attitude

    bool saw_ballistic = false;
    double v_min_seen = 1e9;
    bool prev_ballistic = false;
    int exit_edges = 0;
    for (int i = 0; i < 7200;
         ++i) {  // 60 s: up, over, and back down. v5 rung D (Chad 2026-07-23
                 // arcade energy ruling, T_max 9000->18000): TRIED extending
                 // the window first (70 s, keeping the exact v_ballistic_exit
                 // bound) on the theory that the doubled thrust just delays a
                 // monotone recovery -- MEASURED that it does not: exit speed
                 // at 70 s is 29.9 m/s, WORSE than the 39.8 m/s at 60 s (the
                 // stronger climb makes the post-apex fall a POGO -- the
                 // recovery oscillates, it does not monotonically converge
                 // above threshold with more ticks). A longer window is
                 // therefore not the sharper anti-wedge test -- it is a
                 // fragile bet on which oscillation phase the window ends in.
                 // Reverted to the 60 s window; the velocity leg below is
                 // relaxed to 0.9x instead (director-authorized), which is
                 // honest about what this test actually certifies at this
                 // envelope: a real recovery (finite, in-cascade, well above
                 // the ballistic floor), not a razor's-edge threshold cross
                 // that an oscillating recovery can miss by a hair on either
                 // side of an arbitrary sample tick. Window RECALIBRATED (MB
                 // 2026-07-08, T_max 5800->9000): the stronger nose-up thrust
                 // (T/W 0.31) fights the tail-first fall longer, so the apex
                 // pogo takes ~2x to break and exit ballistic; 30 s read
                 // exit_edges == 0 at that envelope (empirically passed by
                 // 120 s; 60 s held margin). Same class as the AT-15
                 // "calibrated to the untuned table" premises.
        const control::Telemetry t = cl.tick(1.0, kAp, kCp);
        REQUIRE(finite_state(cl.state));
        REQUIRE(std::isfinite(t.e));
        REQUIRE(std::isfinite(t.omega_des.x));
        // On the ballistic->cascade EXIT edge the filtered AoA must be
        // RE-SEEDED to the now-valid raw alpha, not the ~pi tail-slide lie it
        // low-passed below the floor. If the filter kept ingesting alpha ~ pi
        // in ballistic (or froze without reseeding), aoa_filtered here != the
        // raw extracted alpha and the protection clamp would command an
        // uncommanded hardover for ~3 tau. Pinned exactly: on exit, filtered ==
        // raw (both from e.alpha this tick). (Kills "filter runs in ballistic"
        // AND "freeze but no reseed".)
        if (prev_ballistic && !t.ballistic) {
            ++exit_edges;
            // The filter carries the now-valid raw alpha (the reseed), NOT the
            // ~pi it low-passed below the floor. (A recovery can still exit at
            // a genuinely high AoA — a deep-stall tumble at 40 m/s — so the pin
            // is "filtered == the true reading", not a magnitude bound: it is
            // the ACCUMULATED lie that the freeze+reseed removes, not the
            // physics.)
            CHECK(t.aoa_filtered == Catch::Approx(t.extracted.alpha));
        }
        prev_ballistic = t.ballistic;
        saw_ballistic = saw_ballistic || t.ballistic;
        v_min_seen = std::min(v_min_seen, t.extracted.speed);
    }
    CAPTURE(v_min_seen, saw_ballistic, exit_edges);
    // Load-bearing (director ruling, v5 rung D): entered AND exited
    // ballistic -- the honest anti-wedge signature -- REQUIREd, not merely
    // CHECKed, so a future retune that silently stops re-entering the
    // cascade halts this test loud rather than falling through to the
    // (now-relaxed-by-window, not by bound) velocity leg below.
    REQUIRE(v_min_seen < kCp.v_ballistic);  // it really reached the apex
    REQUIRE(saw_ballistic);                 // and the mode engaged
    REQUIRE(exit_edges >= 1);  // and cleanly re-entered the cascade
    // Recovered: finite, airborne, and flying again (out of ballistic) by the
    // end (the exit hysteresis let the cascade back in). v5 rung D
    // (director-authorized): relaxed to 0.9x v_ballistic_exit -- MEASURED
    // the recovery is a POGO (oscillating exit speed, not monotone; see the
    // loop-window comment above), so a razor's-edge exact-threshold sample
    // at an arbitrary tick is not the sharper test; 0.9x still fails a wedge
    // (speed stalled/still falling) while tolerating the oscillation's
    // trough.
    CHECK(glm::length(cl.state.velocity) > 0.9 * kCp.v_ballistic_exit);
    CHECK(sim::altitude(cl.state.position, kAp) > 0.0);
}

// ---------------------------------------------------------------------------
// MB-lean (Chad 2026-07-08: "attracted like a magnet... hold a shallower
// angle of bank with use of rudder at moments of moderate deflection"): in
// FINE the held_bank setpoint decays toward clamp(lean_gain * az, +/-
// lean_max) — az the DE-ROLLED lateral aim azimuth — instead of toward
// level. Legs: (i) sign+magnitude in the linear region, (ii) the cap
// boundary-exact, (iii) knob-off bit-identity vs the old-shape recursion,
// (iv) the closed-loop magnet (the felt fix), (v) inverted rest untouched,
// (vi) the bank-leak de-roll, (vii) sustained shallow bank at moderate
// deflection (the felt complaint #2). Every leg's premises REQUIRE against
// live config (blend_lo is a flagged hidden dependency — a retune trips
// LOUD, never gates the decay off silently).
// ---------------------------------------------------------------------------
TEST_CASE("MB-lean: FINE held_bank leans toward the lateral aim") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};

    SECTION("sign + magnitude (linear region) and the cap (boundary-exact)") {
        // Open-loop on a FIXED level state: az is exact by construction
        // (target = nose yawed about body-up), so the converged held_bank is
        // the config-recomputed clamp itself. Probe angles derived from the
        // live config (the config-relative-bounds lesson): the saturation
        // knee is lean_max/lean_gain — probe strictly inside (0.7x) and
        // strictly past (1.5x) it. The old fixed 3.0/4.5 deg probes were
        // calibrated to gain 8 (knee 3.75 deg) and false-failed gain 10
        // (knee 3.0 — the "linear" probe landed exactly ON the cap).
        const double knee = kCp.lean_max / kCp.lean_gain;  // [rad]
        // The saturated probe needs a window (knee, blend_lo) to exist: at
        // lean_gain 6 the knee EQUALS blend_lo (30/6 = 5 deg) — the cap is
        // unreachable inside FINE (vestigial by design, the blend takes over
        // first), so the cap leg self-skips instead of REQUIRE-tripping on a
        // structurally void premise (Fly 13; the config-relative-bounds
        // lesson's third strike on this fixture).
        const double sat_probe = std::min(1.5 * knee, 0.95 * kCp.blend_lo);
        std::vector<double> probes = {0.7 * knee};
        if (sat_probe > 1.05 * knee) probes.push_back(sat_probe);
        for (const double a : probes) {
            const bool linear_probe = a < knee;
            REQUIRE(a < kCp.blend_lo);  // premise: inside FINE
            const sim::SimState s =
                harness::level_state(kAp, 140.0, 3000.0, up, heading);
            const glm::dvec3 nose = s.orientation * glm::dvec3{0, 0, -1};
            const glm::dvec3 up_b = s.orientation * glm::dvec3{0, 1, 0};
            control::Input in;
            // angleAxis(-a, up_b): a rad RIGHT of the nose (the golden's
            // convention) -> az = +a exactly at zero bank.
            in.target_dir_world =
                glm::normalize(glm::angleAxis(-a, up_b) * nose);
            in.throttle = 0.7;
            control::Internal internal = control::reset();

            const double raw = kCp.lean_gain * a;
            const double L = std::clamp(raw, -kCp.lean_max, kCp.lean_max);
            if (linear_probe) {
                REQUIRE(raw < kCp.lean_max);  // premise: strictly linear
            } else {
                REQUIRE(raw > kCp.lean_max);  // premise: saturated (the cap
                                              // is the oracle, not a band)
            }

            double early_roll = 0.0;
            for (int i = 0; i < 600; ++i) {
                const control::Output o =
                    control::step(s, in, internal, kAp, kCp, kAp.sim_dt);
                internal = o.internal;
                if (i == 10) {
                    // Early: the setpoint walks POSITIVE (right wing down)
                    // and the emitted roll chases it: roll-right = -omega_z.
                    REQUIRE(internal.held_bank > 0.0);
                    early_roll = o.telem.omega_des.z;
                }
            }
            CHECK(early_roll < 0.0);  // rolls RIGHT toward a right-hand aim
            // Converged on the config oracle (tau = 1/rate ~ 0.33 s; 600
            // ticks leaves a ~2.5e-7 relative residual).
            CHECK(internal.held_bank == Catch::Approx(L));
        }
        // Mutations this pair kills (verified post-commit): az sign flip ->
        // held_bank walks NEGATIVE (the magnet repels); lean deleted
        // (decay-to-0) -> held_bank stays 0; clamp dropped -> the 4.5 deg
        // sample converges to gain*az past the cap; one-sided clamp -> the
        // pair straddles it.
    }

    SECTION("knob-off bit-identity: lean_gain = 0 IS the old decay-to-0") {
        // The old code is gone and the shipped golden records the lean_gain=8
        // arm, so the knob-off arm is pinned against a hand-stepped oracle of
        // the OLD expression shape, exact == per tick. On this upright fixed
        // state wings_level_gate == 1.0 exactly (cosPhiTheta = cos 20 deg >>
        // band, smoothstep saturates), so the oracle recursion is
        // b -= gate*rate*dt*b with gate = 1.0 — the identical product tree.
        control::ControllerParams cp2 = kCp;
        cp2.lean_gain = 0.0;
        const sim::SimState s = harness::flight_state(
            kAp, 140.0, 3000.0, up, heading, rad(20.0), 0.0, 0.0);
        const glm::dvec3 nose = s.orientation * glm::dvec3{0, 0, -1};
        const glm::dvec3 right = s.orientation * glm::dvec3{1, 0, 0};
        control::Input in;
        in.target_dir_world =
            glm::normalize(glm::angleAxis(rad(2.0), right) * nose);  // 2 up
        in.throttle = 0.7;
        control::Internal internal = control::reset();
        internal.held_bank = rad(20.0);
        double b = rad(20.0);
        for (int i = 0; i < 200; ++i) {
            const control::Output o =
                control::step(s, in, internal, kAp, cp2, kAp.sim_dt);
            internal = o.internal;
            b -= 1.0 * cp2.auto_level_rate * kAp.sim_dt * b;
            REQUIRE(internal.held_bank == b);  // EXACT — no tolerance
        }
        // Mutation killed: any re-association of the increment (e.g.
        // b*(1-k) + k*target) rounds differently and breaks exact ==.
    }

    SECTION("closed-loop magnet: a small lateral aim is CAPTURED to centre") {
        // The felt fix: pre-lean, the rudder pointing and the coordination
        // term reach a standoff ~0.4-0.5 deg off centre (rudder alone cannot
        // close it); the lean's shallow bank closes it into the deadzone
        // park. Config-relative bound (deadzone landed first by design).
        const glm::dvec3 up_g = glm::normalize(glm::dvec3{1.0, 1.0, 1.0});
        double thr = 0.7;
        const sim::SimState s0 = harness::level_trim_state(
            kAp, 140.0, 3000.0, up_g, {0.0, 0.0, -1.0}, &thr);
        harness::ClosedLoop cl(s0, glm::dvec3{0.0, 0.0, -1.0});
        cl.aim_nose();
        cl.tick(thr, kAp, kCp, /*grounded=*/true);
        const glm::dvec3 nose = cl.state.orientation * glm::dvec3{0, 0, -1};
        const glm::dvec3 up_b = cl.state.orientation * glm::dvec3{0, 1, 0};
        cl.aim = glm::normalize(glm::angleAxis(-rad(2.0), up_b) * nose);
        control::Telemetry t;
        double e_sum = 0.0;
        int n = 0;
        for (int i = 0; i < 1800; ++i) {  // 15 s; tau_lean ~ 2 s
            t = cl.tick(thr, kAp, kCp);
            if (i >= 1440) {  // last 3 s
                e_sum += t.e;
                ++n;
            }
        }
        const double e_mean = e_sum / n;
        std::printf("[MB-lean magnet] steady mean err = %.17g deg\n",
                    e_mean * 57.2957795130823);
        // Mean (not max): the park legitimately cycles up to deadzone_hi
        // before each re-arm tap (the 4b park shape).
        CHECK(e_mean < kCp.deadzone_hi);
        // Mutation killed (RUN, not assumed): lean_gain = 0 parks at the
        // rudder/coordination standoff ~0.4 deg >> deadzone_hi.
    }

    SECTION("inverted rest is untouched (the lean rides wings_level_gate)") {
        // Belly-up rest with a LATERAL aim inside the rest band: az != 0 so
        // the lean WANTS to act, but the gate is exactly 0 past knife-edge —
        // held_bank must stay BIT-unchanged until MB-right arms. Open-loop
        // fixed state (the S6 latch discipline).
        REQUIRE(rad(2.0) < kCp.blend_lo);  // premise: counts as rest
        const sim::SimState s = harness::flight_state(
            kAp, 140.0, 3000.0, up, heading, rad(170.0), 0.0, 0.0);
        const glm::dvec3 nose = s.orientation * glm::dvec3{0, 0, -1};
        const glm::dvec3 up_b = s.orientation * glm::dvec3{0, 1, 0};
        control::Input in;
        in.target_dir_world =
            glm::normalize(glm::angleAxis(-rad(2.0), up_b) * nose);
        in.throttle = 0.7;
        control::Internal internal = control::reset();
        internal.held_bank = rad(170.0);  // as a GROUNDED capture would
        const double b0 = internal.held_bank;
        const int delay_ticks =
            static_cast<int>(kCp.inverted_delay / kAp.sim_dt + 0.5);
        for (int i = 0; i < delay_ticks - 12; ++i) {
            const control::Output o =
                control::step(s, in, internal, kAp, kCp, kAp.sim_dt);
            internal = o.internal;
            REQUIRE_FALSE(o.telem.righting);
            REQUIRE(internal.held_bank == b0);  // BIT-frozen (gate == 0)
        }
        // Mutation killed: the lean applied outside wings_level_gate walks
        // held_bank toward the lean while belly-up.
    }

    SECTION("bank-leak de-roll: a banked VERTICAL error does not lean") {
        // The P1-1 pin: with the plane banked 60 and the aim 4 deg above the
        // nose IN THE WORLD (pure vertical error), the bare body-frame x
        // reads -sin(4)*sin(60) — a phantom ~-3.5 deg azimuth whose lean
        // (~-28 deg) would wag the wings through level. The de-rolled az is
        // 0 EXACTLY (x*cos(phi) + y*sin(phi) cancels by construction), so
        // held_bank decays toward LEVEL like the old auto-level.
        REQUIRE(rad(4.0) < kCp.blend_lo);  // premise: inside FINE
        const sim::SimState s = harness::flight_state(
            kAp, 140.0, 3000.0, up, heading, rad(60.0), 0.0, 0.0);
        const glm::dvec3 nose = s.orientation * glm::dvec3{0, 0, -1};
        const glm::dvec3 lup = sim::local_up(s.position);
        control::Input in;
        in.target_dir_world =
            glm::normalize(std::cos(rad(4.0)) * nose +
                           std::sin(rad(4.0)) * lup);  // 4 deg up, WORLD
        in.throttle = 0.7;
        control::Internal internal = control::reset();
        internal.held_bank = rad(60.0);
        for (int i = 0; i < 600; ++i) {
            const control::Output o =
                control::step(s, in, internal, kAp, kCp, kAp.sim_dt);
            internal = o.internal;
        }
        // Decayed to ~level (60 deg * 0.975^600 ~ 1e-7), NOT to the phantom
        // lean. Margin 1.5 deg: the phantom sits at ~-28 deg, 20x outside.
        CHECK(std::abs(internal.held_bank) < rad(1.5));
        // Mutation killed: az without the de-roll converges to
        // clamp(lean_gain * -sin(4)*sin(60)) ~ -28 deg.
    }

    SECTION("bank-leak de-roll holds at PITCHED attitudes (diff P1-1)") {
        // The phi_full cos/sin de-roll variant is exact ONLY at zero pitch:
        // e.phi folds pitch into the roll gauge (phi_full = asin(sin(roll)*
        // cos(pitch)) at Euler roll+pitch), so at pitch 45 / bank 40 a pure
        // VERTICAL-plane aim leaked ~7.2 deg of phantom lean through it. The
        // shipped axis (cosPhiTheta, sin(e.phi)) == cross(nose_b, up_b) is
        // exact at every attitude (numerator ~7e-17 here). Fixture built by
        // hand — harness::flight_state has no pitch parameter, which is
        // exactly why the zero-pitch leg above could not see this (the
        // "fixture makes the behavior a no-op" class).
        const sim::SimState s0 =
            harness::level_state(kAp, 140.0, 3000.0, up, heading);
        sim::SimState s = s0;
        const glm::dvec3 right0 = s.orientation * glm::dvec3{1, 0, 0};
        s.orientation = glm::normalize(glm::angleAxis(rad(45.0), right0) *
                                       s.orientation);  // pitch up
        const glm::dvec3 nose45 = s.orientation * glm::dvec3{0, 0, -1};
        s.orientation = glm::normalize(glm::angleAxis(rad(40.0), nose45) *
                                       s.orientation);  // roll 40
        const glm::dvec3 nose = s.orientation * glm::dvec3{0, 0, -1};
        s.velocity = 140.0 * nose;
        s.last_vhat = nose;
        // Aim 4 deg off the nose PURELY in the vertical plane span(nose,
        // local_up): zero true horizontal-lateral component, so a correct
        // de-roll leans 0.
        const glm::dvec3 lup = sim::local_up(s.position);
        const glm::dvec3 pv = glm::normalize(lup - glm::dot(lup, nose) * nose);
        control::Input in;
        in.target_dir_world =
            glm::normalize(std::cos(rad(4.0)) * nose + std::sin(rad(4.0)) * pv);
        in.throttle = 0.7;
        control::Internal internal = control::reset();
        // Seed at the current unfolded bank (what any capture would set) so
        // the decay's only job is tracking the lean target.
        const control::Extracted e0 =
            control::extract(s, s.last_vhat, kAp.v_dir_eps);
        internal.held_bank = control::unfold_bank(e0.phi, e0.cos_phi_theta);
        for (int i = 0; i < 600; ++i) {
            const control::Output o =
                control::step(s, in, internal, kAp, kCp, kAp.sim_dt);
            internal = o.internal;
        }
        CHECK(std::abs(internal.held_bank) < rad(1.5));
        // Mutation killed (diff red-team P1-1, re-verified below): the
        // phi_full cos/sin de-roll converges ~7.2 deg here — 5x the margin.
    }

    SECTION("moderate deflection holds a SHALLOW bank (the felt ask)") {
        // A sustained 4.5 deg lateral carrot (re-aimed every tick — the
        // pilot tracking at moderate deflection): with the lean the plane
        // settles a sustained SHALLOW banked turn near the cap; pre-lean it
        // crabbed wings-level (the all-or-nothing complaint's flat half).
        REQUIRE(rad(4.5) < kCp.blend_lo);
        // The expected lean is the CONFIG-RECOMPUTED clamp, not "the cap"
        // (Fly 13: at lean_gain 6 the cap no longer binds at 4.5 deg — the
        // expected sustained bank is the linear 27 deg; the old REQUIRE
        // that the cap binds was a premise calibrated to gain >= 7, the
        // config-relative-bounds class again).
        const double lean_target =
            std::min(kCp.lean_gain * rad(4.5), kCp.lean_max);
        REQUIRE(lean_target > rad(10.0));  // premise: a REAL shallow bank
        double thr = 0.7;
        const sim::SimState s0 =
            harness::level_trim_state(kAp, 140.0, 3000.0, up, heading, &thr);
        harness::ClosedLoop cl(s0, glm::dvec3{0.0, 0.0, -1.0});
        cl.aim_nose();
        cl.tick(thr, kAp, kCp, /*grounded=*/true);
        double phi_sum = 0.0;
        int n = 0, sign_flips = 0;
        double prev_phi = 0.0;
        for (int i = 0; i < 1800; ++i) {  // 15 s
            const glm::dvec3 nose = cl.state.orientation * glm::dvec3{0, 0, -1};
            const glm::dvec3 up_b = cl.state.orientation * glm::dvec3{0, 1, 0};
            cl.aim = glm::normalize(glm::angleAxis(-rad(4.5), up_b) * nose);
            const control::Telemetry t = cl.tick(thr, kAp, kCp);
            if (i >= 600) {  // past the roll-in
                phi_sum += t.extracted.phi;
                ++n;
                if (i > 600 && std::abs(t.extracted.phi) > rad(2.0) &&
                    std::abs(prev_phi) > rad(2.0) &&
                    std::signbit(t.extracted.phi) != std::signbit(prev_phi)) {
                    ++sign_flips;
                }
                prev_phi = t.extracted.phi;
            }
        }
        const double phi_mean = phi_sum / n;
        std::printf("[MB-lean moderate] mean phi = %.2f deg, flips = %d\n",
                    phi_mean * 57.2957795130823, sign_flips);
        // Sustained shallow RIGHT bank: well off level, tracking the
        // config-recomputed lean target, and no wings-wag (back-and-forth).
        // S-wvane 2026-07-11 recalibration (0.5 -> 0.3 of the target,
        // investigated NOT blessed): the fuselage side-force now does part of
        // the turning FLAT, so the settled bank for the same tracking is
        // leaner (mean phi 10.3 deg vs the pre-S-wvane ~15 at target 27).
        // Buttery Rung 1 2026-07-30 re-key (0.3 -> 0.2, measured both arms
        // same table): at lean_gain 8 this 4.5-deg hold sits PAST the new
        // 3.75-deg knee, so lean_target SATURATES at the 30-deg cap while
        // the settled equilibrium measures LEANER (gain 6: 10.22 deg at
        // target 27; gain 8: 8.18 deg at target 30 — the flat-turn channels
        // + the cap-saturation coupling erode the attainment ratio 0.38 ->
        // 0.27). The TRANSIENT (the felt rung) rises +27-33% peak bank in
        // the step probes — this leg pins the sustained-hold MECHANISM, not
        // the transient. 0.2*30 = 6.0 deg keeps honest margin under the
        // measured 8.18.
        // The mechanism pin survives: the lean_gain = 0 mutation still crabs
        // wings-near-level (~0) and fails the bound.
        CHECK(phi_mean > 0.2 * lean_target);
        CHECK(phi_mean < lean_target * 1.3);
        CHECK(sign_flips == 0);
        // Mutation (RUN, not assumed): lean_gain = 0 crabs wings-near-level
        // (mean phi ~ 0) -> the lower bound FAILS.
    }
}

// ---------------------------------------------------------------------------
// Deadzone holds TRIM, not zero (SPEC §9.3): parked on target, the emitted
// Inputs are constant at a nonzero trim (the integrator holds it), never
// zeroed — zeroing manufactures the limit cycle it was meant to kill.
// ---------------------------------------------------------------------------
TEST_CASE("cascade: deadzone holds a nonzero trim deflection") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    double thr = 0.7;
    const sim::SimState s0 =
        harness::level_trim_state(kAp, 140.0, 3000.0, up, heading, &thr);
    harness::ClosedLoop cl(s0, glm::dvec3{0.0, 0.0, -1.0});
    cl.aim_nose();

    control::Telemetry t;
    for (int i = 0; i < 3600; ++i) t = cl.tick(thr, kAp, kCp);  // settle 30 s
    // At the halved deadzone (0.05/0.12, Chad 2026-07-08) the park CYCLES
    // through the lo..hi band (measured: e in [0.049, 0.120] deg, deadzoned
    // ~65% of the last 10 s) — parking at the re-arm boundary with taps IS
    // the deadzone working (4b), so the pin is fraction-based, not
    // every-tick: mostly parked, and on the PARKED ticks the emitted pitch
    // is a NONZERO steady trim ("Inputs at rest are constant at trim, not
    // zero", SPEC 9.3 — zeroing manufactures the limit cycle).
    // RUNG M1 2026-07-11 (circle 0.05/0.08 -> 0.03/0.05, Chad's "nose in the
    // MIDDLE" ruling): lo now sits 0.002 deg above the V=140 cruise pointing
    // equilibrium (~0.028 deg, the V-scaled vertical ff-deficit standoff the
    // PARK instrument exposed), so park episodes are SHORT bursts (measured
    // 27% occupancy, ~0.33 s episodes) — the latch engages briefly, drift
    // unlatches, live pointing (0.1 deg/s scale) re-centers. The trim-hold
    // CONTRACT is unchanged (trim lives in the integrator, latched or not;
    // the constancy pins below still bind); the occupancy floor is now a
    // VACUOUSNESS guard (enough parked ticks to measure), not "mostly
    // parked". Unlike Fly-5's rejected 0.03/0.06 (park fragmented with NO
    // offset gain — the lateral magnet equilibrium sat above the whole
    // circle), this circle buys the offset halving Chad asked for and the
    // equilibrium sits INSIDE the band.
    // The steadiness pin is the settled TAIL of a contiguous park (skip the
    // episode head — the integrator is still decaying the preceding re-arm
    // tap's transient, measured ~2.6e-3 head-inclusive pre-S-dampff).
    // ACROSS parks the trim legitimately tracks the drifting state; the
    // pathological limit cycle this pin exists for is degrees-scale at ~6 Hz.
    // S-dampff (SPEC §0): on a ff'd pitch axis BOTH numbers move honestly —
    // the tap transient is integrator-borne, so it scales by (1 - damp_ff)
    // and at 1.0 decays in ~0.1 s (skip 12, measured tail spread stays
    // <1e-6); and the park episodes SHORTEN (~40 -> ~32 ticks): exact
    // demand-tracking drifts the latched nose at the UNDILUTED vertical
    // ff-deficit rate, so the lo..hi band cycles faster. Same accepted
    // trim-band character, quicker clock. The legacy 30-tick skip is kept
    // for the damp_ff_pitch = 0 A/B arm.
    int dz = 0, ep_len = 0, longest_ep = 0;
    float pmin = 1e9f, pmax = -1e9f, ep_min = 1e9f, ep_max = -1e9f;
    float worst_tail_spread = -1.0f;
    constexpr int kWin = 600;  // 5 s
    const int kEpSkip =
        (kCp.damp_ff_pitch > 0.0) ? 12 : 30;  // per-episode head skip
    const auto fold_episode = [&] {
        if (ep_len > kEpSkip && ep_min <= ep_max)
            worst_tail_spread = std::max(worst_tail_spread, ep_max - ep_min);
        ep_len = 0;
        ep_min = 1e9f;
        ep_max = -1e9f;
    };
    for (int i = 0; i < kWin; ++i) {
        t = cl.tick(thr, kAp, kCp);
        if (!t.deadzoned) {
            fold_episode();
            continue;
        }
        ++dz;
        ++ep_len;
        longest_ep = std::max(longest_ep, ep_len);
        pmin = std::min(pmin, cl.last_inputs.pitch);
        pmax = std::max(pmax, cl.last_inputs.pitch);
        if (ep_len > kEpSkip) {  // settled tail only
            ep_min = std::min(ep_min, cl.last_inputs.pitch);
            ep_max = std::max(ep_max, cl.last_inputs.pitch);
        }
    }
    fold_episode();
    CAPTURE(dz, longest_ep, pmin, pmax, worst_tail_spread);
    CHECK(dz > kWin / 5);  // vacuousness guard (M1: measured 27%, was ~65%)
    REQUIRE(longest_ep > kEpSkip + 5);  // a settled tail exists to pin
    REQUIRE(worst_tail_spread >= 0.0f);
    CHECK(std::abs(0.5f * (pmin + pmax)) > 1e-5f);  // nonzero trim
    CHECK(worst_tail_spread < 1e-3f);  // constant at trim WHILE parked
}

// ---------------------------------------------------------------------------
// Aim-motion gate (rudder-flick Fly 6, SPEC §9.3 as amended 2026-07-10, Chad:
// zoomed slow mouse-up gave "little micro boosts... a slight ascending
// jitter"). The deadzone may latch only after rest_dwell of hand-rest; an
// aim-moved tick unlatches instantly. Legs: (i) the STAIR REPRO — a slow aim
// ramp under the legacy latch relaxation-cycles (parked stretches mid-motion,
// error sawtooths to deadzone_hi), and the SAME ramp under the gate tracks
// continuously (zero parked ticks, error never reaches the old boost
// amplitude) — the mechanism-firing premise and the fix in one differential;
// (ii) trim hold preserved: the hand stops -> the deadzone relatches after
// the dwell; (iii) a strobing hand (tremor) cannot lurch — the unlatched
// pointing at sub-lo error is bounded at the K_theta*lo scale; (iv) rest_dwell
// = 0 is the LEGACY latch bit-identically, aim_moved ignored (knob-off).
// Ramp rate derives from the config: the stair exists only when the tracking
// lag rate/K_theta sits INSIDE the circle (rate < K_theta*lo), which is
// exactly the felt "moving it really slow" regime.
// ---------------------------------------------------------------------------
TEST_CASE("cascade: aim-motion gate kills the slow-tracking stairs") {
    // Self-armed (Fly-7 lesson): the mechanism pins must not depend on the
    // shipped dial — rest_dwell is Chad's feel A/B and may sit at 0 (legacy).
    // S-dampff (SPEC §0): ALSO self-armed to damp_ff_pitch = 0 in BOTH arms —
    // the pitch damping feedforward kills this stair's repro at the ROOT
    // (legacy-arm dz ticks 92 -> 0 at the shipped table: the relaxation cycle
    // needs the chase to transient-OVERSHOOT into the circle and latch
    // mid-motion, and the ff'd chase approaches monotonically). The gate's
    // contract is against the LEGACY latch dynamics — still live on any
    // un-ff'd axis (yaw/roll today) and on the damp_ff_pitch = 0 A/B arm —
    // so the differential pins the mechanism on the dynamics it was built
    // for. Felt bonus, noted for the fly: on a ff'd axis the stair class
    // should be gone even with the gate off.
    control::ControllerParams gated = kCp;
    gated.deadzone_rest_dwell = 0.15;
    gated.damp_ff_pitch = 0.0;
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    double thr = 0.7;
    const sim::SimState s0 =
        harness::level_trim_state(kAp, 140.0, 3000.0, up, heading, &thr);
    // Slow enough that the legacy nose catches INSIDE the circle and parks
    // (the relaxation cycle); fast enough to cross the band in the window.
    const double ramp = 0.6 * kCp.K_theta * kCp.deadzone_lo;  // [rad/s]
    struct Run {
        int dz_ticks = 0;
        double max_err = 0.0;
        bool relatched = false;
    };
    auto run = [&](const control::ControllerParams& cp, bool moving) {
        harness::ClosedLoop cl(s0, glm::dvec3{0.0, 0.0, -1.0});
        cl.aim_nose();
        for (int i = 0; i < 240; ++i) cl.tick(thr, kAp, cp);  // settle 2 s
        Run r;
        cl.aim_moved = moving;
        for (int i = 0; i < 720; ++i) {  // 6 s of slow raise
            const glm::dvec3 fwd = glm::normalize(cl.aim);
            const glm::dvec3 lu = sim::local_up(cl.state.position);
            const glm::dvec3 rt = glm::cross(fwd, lu);
            REQUIRE(glm::length(rt) > 1e-6);  // ramp never nears the zenith
            cl.aim = glm::normalize(
                glm::angleAxis(ramp * kAp.sim_dt, glm::normalize(rt)) * fwd);
            const control::Telemetry t = cl.tick(thr, kAp, cp);
            if (t.deadzoned) ++r.dz_ticks;
            // Steady-ramp error only: the rest park legitimately cycles up
            // to just under hi, so the first second (the entry transient the
            // gate is still draining) would alias the parked peak into the
            // gated arm's measurement.
            if (i >= 120) r.max_err = std::max(r.max_err, t.e);
        }
        cl.aim_moved = false;            // the hand stops
        for (int i = 0; i < 480; ++i) {  // 4 s rest
            if (cl.tick(thr, kAp, cp).deadzoned) {
                r.relatched = true;
                break;
            }
        }
        return r;
    };

    // LEGACY arm (the mechanism-firing premise): the stairs EXIST — parked
    // stretches mid-motion (measured 92 ticks at this ramp), the error
    // sawtoothing toward the re-arm boundary.
    control::ControllerParams legacy = kCp;
    legacy.deadzone_rest_dwell = 0.0;
    legacy.damp_ff_pitch = 0.0;  // legacy DYNAMICS too (S-dampff self-arm)
    const Run stair = run(legacy, /*moving=*/true);  // aim_moved IGNORED
    CHECK(stair.dz_ticks > 50);

    // GATED arm: the SAME slow raise is continuous — never parked while the
    // hand moves, and the error never builds to the re-arm boundary (the old
    // boost amplitude). NOTE the tracking LAG (~0.057 deg here) is plant-set
    // (inner-loop inertia), not deadzone-set — the felt fix is the CONTINUITY
    // (dz == 0: no stop-and-go), not a large amplitude drop; an amplitude-
    // ratio pin would be a fixture-sensitive non-oracle.
    const Run smooth = run(gated, /*moving=*/true);
    CHECK(smooth.dz_ticks == 0);
    CHECK(smooth.max_err < kCp.deadzone_hi * 0.95);

    // Trim hold preserved: at rest the deadzone relatches (both arms).
    CHECK(stair.relatched);
    CHECK(smooth.relatched);
}

TEST_CASE("cascade: aim-motion gate: a strobing hand cannot lurch") {
    // Self-armed (Fly-7): pins the mechanism regardless of the shipped dial.
    control::ControllerParams gated = kCp;
    gated.deadzone_rest_dwell = 0.15;
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    double thr = 0.7;
    const sim::SimState s0 =
        harness::level_trim_state(kAp, 140.0, 3000.0, up, heading, &thr);
    // Twin rest runs from the same settled state: one pure rest, one with a
    // 1-tick aim_moved strobe every 12 ticks (0.1 s < dwell, so the deadzone
    // can never relatch — the worst tremor case). The strobed run's peak
    // rate commands may exceed the parked run's by at most the sub-lo
    // pointing scale — PER AXIS (red-team P2-2): pitch is K_theta*lo, yaw is
    // K_theta*yaw_scale*lo (~2.2x hotter — the honest yaw bound). Both
    // config-derived. A tremor can hold the pointing live, but it cannot
    // manufacture a boost. (Valid below aoa_max: near stall the AoA pushback
    // legitimately runs while unlatched — protection, deliberate.)
    struct Peaks {
        double pitch = 0.0, yaw = 0.0;
    };
    auto peaks = [&](bool strobe) {
        harness::ClosedLoop cl(s0, glm::dvec3{0.0, 0.0, -1.0});
        cl.aim_nose();
        for (int i = 0; i < 240; ++i) cl.tick(thr, kAp, gated);  // settle 2 s
        Peaks p;
        for (int i = 0; i < 480; ++i) {  // 4 s
            cl.aim_moved = strobe && (i % 12 == 0);
            const control::Telemetry t = cl.tick(thr, kAp, gated);
            p.pitch = std::max(p.pitch, std::abs(t.omega_des.x));
            p.yaw = std::max(p.yaw, std::abs(t.omega_des.y));
        }
        return p;
    };
    const Peaks parked = peaks(false);
    const Peaks strobed = peaks(true);
    CHECK(strobed.pitch <= parked.pitch + 1.5 * kCp.K_theta * kCp.deadzone_lo);
    CHECK(strobed.yaw <=
          parked.yaw + 1.5 * kCp.K_theta * kCp.yaw_scale * kCp.deadzone_lo);
}

TEST_CASE("cascade: aim-motion gate relatches after exactly the config dwell") {
    // Self-armed (Fly-7): pins the mechanism regardless of the shipped dial.
    control::ControllerParams gated = kCp;
    gated.deadzone_rest_dwell = 0.15;
    // The DURATION pin (red-team P1-2: a rest_dwell*0.1 mutant survived the
    // whole suite — the dwell's real job is bridging the (N-1) at-rest ticks
    // per frame the fixed-dt accumulator manufactures at low fps, so a
    // too-short effective dwell brings the stairs back with the gate "on").
    // From a latched rest with the error well inside the circle: ONE
    // aim-moved tick must unlatch THAT tick (the firing premise the strobe
    // leg needs — red-team P2-3), and the relatch must take
    // ceil(rest_dwell/sim_dt) rest ticks, config-derived (+/-1 for the
    // binary-vs-decimal dt representation).
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    double thr = 0.7;
    const sim::SimState s0 =
        harness::level_trim_state(kAp, 140.0, 3000.0, up, heading, &thr);
    harness::ClosedLoop cl(s0, glm::dvec3{0.0, 0.0, -1.0});
    cl.aim_nose();
    control::Telemetry t;
    bool settled = false;
    for (int i = 0; i < 4800; ++i) {
        t = cl.tick(thr, kAp, gated);
        if (t.deadzoned && t.e < 0.5 * gated.deadzone_lo) {
            settled = true;
            break;
        }
    }
    REQUIRE(settled);  // premise: latched, err deep inside the circle

    cl.aim_moved = true;
    t = cl.tick(thr, kAp, gated);
    REQUIRE_FALSE(t.deadzoned);  // one moved tick unlatches THAT tick

    cl.aim_moved = false;
    int k = 0;
    while (k < 100) {
        ++k;
        t = cl.tick(thr, kAp, gated);
        if (t.deadzoned) break;
    }
    const int expect =
        static_cast<int>(std::ceil(gated.deadzone_rest_dwell / kAp.sim_dt));
    CHECK(k >= expect - 1);
    CHECK(k <= expect + 1);
}

TEST_CASE("cascade: rest_dwell = 0 is the legacy latch bit-identically") {
    // Knob-off strict superset: with the gate off, an arbitrarily strobing
    // aim_moved must be INVISIBLE — Inputs and the deadzone trace bit-equal
    // to an aim_moved-never run. (The MB-lean lean_gain=0 identity pattern.)
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    double thr = 0.7;
    const sim::SimState s0 =
        harness::level_trim_state(kAp, 140.0, 3000.0, up, heading, &thr);
    control::ControllerParams legacy = kCp;
    legacy.deadzone_rest_dwell = 0.0;
    harness::ClosedLoop a(s0, glm::dvec3{0.0, 0.0, -1.0});
    harness::ClosedLoop b(s0, glm::dvec3{0.0, 0.0, -1.0});
    a.aim_nose();
    b.aim_nose();
    for (int i = 0; i < 600; ++i) {  // 5 s, strobing vs never
        a.aim_moved = (i % 3 == 0);
        b.aim_moved = false;
        const control::Telemetry ta = a.tick(thr, kAp, legacy);
        const control::Telemetry tb = b.tick(thr, kAp, legacy);
        REQUIRE(ta.deadzoned == tb.deadzoned);
        REQUIRE(a.last_inputs.pitch == b.last_inputs.pitch);
        REQUIRE(a.last_inputs.yaw == b.last_inputs.yaw);
        REQUIRE(a.last_inputs.roll == b.last_inputs.roll);
    }
}

// ---------------------------------------------------------------------------
// S-yaw-magnet (SPEC §0 2026-07-10, Chad: "the magnet for yaw isn't strong
// enough to pull it into the middle"): the coordination term fades to
// center_frac at zero pointing error (smoothstep over center_band) so the
// rudder finishes into the circle. Legs: (i) the relief SCALES the
// coordination near center and vanishes far from it (open-loop differential
// vs a frac=1 twin, config-derived expectations); (ii) the CLOSED-LOOP
// centering claim — with the relief the post-flick park settles INSIDE the
// deadzone circle, without it the pointing-vs-coordination equilibrium hangs
// OUTSIDE (the firing premise and the felt fix in one differential);
// (iii) frac = 1 short-circuits (band unread — legacy bit-identity).
// AT-16 (the skid wall this mechanism exists to respect) runs LIVE against
// the committed relief inside the full gate.
// ---------------------------------------------------------------------------
TEST_CASE("S-yaw-magnet: coordination fades near center, full far from it") {
    // Open-loop fixed state with real sideslip: velocity yawed 3 deg off the
    // nose => beta != 0 while the aim sits ON the nose (err ~ 0, deep inside
    // center_band) or 2 deg off (err >> band).
    control::ControllerParams armed = kCp;
    armed.coord_center_frac = 0.25;
    armed.coord_center_band = rad(0.5);
    control::ControllerParams off = kCp;
    off.coord_center_frac = 1.0;
    // S-rimshot v2 isolation: the doctored omega=0-with-beta state trips the
    // universal capture's ENGAGE (the coordination-demand share of w_rel is
    // a phantom closing rate on a rudder that hasn't delivered yet), and at
    // carry = 1 the rate-continuous event emission reproduces measured omega
    // REGARDLESS of coordination — collapsing this leg's differential to 0.
    // This leg pins the yaw-magnet alone; carry = 0 is the sanctioned
    // bit-identical isolation arm (test_capture owns the event's own pins).
    armed.capture_carry = 0.0;
    off.capture_carry = 0.0;
    REQUIRE(rad(2.0) > 2.0 * armed.coord_center_band);  // premise: "far"

    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    sim::SimState s = harness::level_state(kAp, 140.0, 3000.0, up, heading);
    const glm::dvec3 nose = s.orientation * glm::dvec3{0, 0, -1};
    const glm::dvec3 up_b = s.orientation * glm::dvec3{0, 1, 0};
    // Sideslip: rotate the VELOCITY 3 deg about body-up (the aim stays put).
    s.velocity = glm::angleAxis(rad(3.0), up_b) * s.velocity;
    s.last_vhat = glm::normalize(s.velocity);

    auto yaw_cmd = [&](const control::ControllerParams& cp, double aim_off) {
        control::Input in;
        in.target_dir_world =
            glm::normalize(glm::angleAxis(-rad(aim_off), up_b) * nose);
        in.throttle = 0.7;
        const control::Output o =
            control::step(s, in, control::reset(), kAp, cp, kAp.sim_dt);
        return o.telem.omega_des.y;
    };

    // Near center (aim ON the nose — the deadzone latches identically in
    // both arms, which makes this a PURE coordination measurement: the
    // pointing is zeroed, coordination rides outside the deadzone by
    // design). Same state both arms => identical beta, so the delta is
    // EXACTLY the removed coordination: (1 - frac) * K_coord * |beta|,
    // beta ~ 3 deg by construction. Config-derived, tight.
    const double near_armed = yaw_cmd(armed, 0.0);
    const double near_off = yaw_cmd(off, 0.0);
    const double delta_near = std::abs(near_off - near_armed);
    const double expect_near =
        (1.0 - armed.coord_center_frac) * kCp.K_coord * rad(3.0);
    CHECK(delta_near == Catch::Approx(expect_near).epsilon(0.05));

    // MID-BAND (red-team P3-1 — the SHAPE pin the endpoints can't give: a
    // hard-step-at-zero mutant passes both endpoint checks): at aim_off =
    // band/2 the smoothstep is exactly 0.5, so the removed coordination is
    // half the near-center removal. The pointing yaw is identical in both
    // arms at identical err, so the delta is still pure coordination.
    const double mid_off_deg = deg(armed.coord_center_band) / 2.0;
    const double delta_mid =
        std::abs(yaw_cmd(off, mid_off_deg) - yaw_cmd(armed, mid_off_deg));
    CHECK(delta_mid == Catch::Approx(0.5 * expect_near).epsilon(0.05));

    // Far from center (2 deg lateral aim, > 2x band): the relief is gone —
    // the two arms agree bit-for-bit (same floats through the same path).
    const double far_armed = yaw_cmd(armed, 2.0);
    const double far_off = yaw_cmd(off, 2.0);
    CHECK(far_armed == Catch::Approx(far_off).margin(1e-15));
}

TEST_CASE("S-yaw-magnet: the post-flick park settles INSIDE the circle") {
    // The felt claim, closed-loop: hold a small lateral aim (inside FINE,
    // outside the deadzone) and let the cascade settle. WITHOUT the relief
    // the nose parks at the coordination standoff OUTSIDE the circle (the
    // firing premise — Chad's "it hangs"); WITH it the rudder finishes and
    // the deadzone latches.
    control::ControllerParams off = kCp;
    off.coord_center_frac = 1.0;
    REQUIRE(kCp.coord_center_frac < 1.0);  // committed table arms the relief

    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    double thr = 0.7;
    const sim::SimState s0 =
        harness::level_trim_state(kAp, 140.0, 3000.0, up, heading, &thr);

    struct Tail {
        double e_mean = 0.0;
        double e_bleed = 0.0;  // mean e over the crab-bleed window (S-wvane)
        double e_min = 1e9;
        int dz_ticks = 0;
    };
    auto settle = [&](const control::ControllerParams& cp) {
        harness::ClosedLoop cl(s0, glm::dvec3{0.0, 0.0, -1.0});
        cl.aim_nose();
        // The flick: kick the aim 2 deg right ONCE, then hold it (the walk-
        // rounds-onto-target case at the moment the hand stops).
        const glm::dvec3 nose = cl.state.orientation * glm::dvec3{0, 0, -1};
        const glm::dvec3 up_b = cl.state.orientation * glm::dvec3{0, 1, 0};
        cl.aim = glm::normalize(glm::angleAxis(-rad(2.0), up_b) * nose);
        Tail t;
        control::Telemetry tel;
        for (int i = 0; i < 1800; ++i) {  // 15 s
            tel = cl.tick(thr, kAp, cp);
            // S-wvane re-scope (2026-07-11): the BLEED window — the first
            // ~1.2 s after the capture transient, while the flick's crab is
            // alive (tau_beta ~ 1.07 s at V=140). This is where the relief
            // still discriminates; see the banner note below.
            if (i >= 30 && i < 150) {
                t.e_bleed += tel.e / 120.0;
            }
            if (i >= 1440) {  // last 3 s
                t.e_mean += tel.e / 360.0;
                t.e_min = std::min(t.e_min, tel.e);
                if (tel.deadzoned) ++t.dz_ticks;
            }
        }
        return t;
    };

    const Tail hang = settle(off);
    const Tail centered = settle(kCp);
    // SUPERSEDED FIRING PREMISE (S-wvane, SPEC §0 2026-07-11): the original
    // leg pinned "WITHOUT the relief the standoff parks OUTSIDE the circle
    // for the whole window" — that hang was powered by PERSISTENT sideslip
    // (the plant had no lateral beta decay), and the S-wvane side-force
    // bleeds it in ~1 s, so the relief-OFF arm now centers itself within a
    // few seconds (measured e_min 0.030 deg, latches). S-wvane fixed the
    // yaw-hang at the ROOT. The relief's REMAINING job is the BLEED WINDOW:
    // while the crab is alive, full coordination (frac = 1) drags the nose
    // off the aim at the standoff scale; the relief keeps it planted. The
    // discriminator is the bleed-window mean, arm vs arm — the frac -> 1
    // mutation IS the hang arm, so the comparison is self-mutation-verified.
    CHECK(hang.e_bleed > 1.5 * centered.e_bleed);
    // LATE behavior: the CROSSING oracle is dz_ticks (a latch REQUIRES an
    // e < lo crossing to fire — diff red-team P2-2: pinning e_min < lo
    // directly was a knife-edge, passing by 0.04% because the latch freezes
    // the approach AT the boundary by construction; a benign retune parking
    // a hair above lo would flip it with the felt behavior unchanged).
    CHECK(centered.dz_ticks > 60);
    CHECK(centered.e_min < kCp.deadzone_hi);
    CHECK(centered.e_mean < kCp.deadzone_hi);
}

TEST_CASE("S-yaw-magnet: frac = 1 short-circuits (band unread)") {
    // Knob-off identity: with the relief off, the band value is dead code —
    // two absurdly different bands must produce bit-identical traces.
    control::ControllerParams a = kCp, b = kCp;
    a.coord_center_frac = 1.0;
    a.coord_center_band = 0.0;  // would divide-by-zero if read
    b.coord_center_frac = 1.0;
    b.coord_center_band = rad(0.4);
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    double thr = 0.7;
    const sim::SimState s0 =
        harness::level_trim_state(kAp, 140.0, 3000.0, up, heading, &thr);
    harness::ClosedLoop ca(s0, glm::dvec3{0.0, 0.0, -1.0});
    harness::ClosedLoop cb(s0, glm::dvec3{0.0, 0.0, -1.0});
    ca.aim_nose();
    cb.aim_nose();
    const glm::dvec3 nose = ca.state.orientation * glm::dvec3{0, 0, -1};
    const glm::dvec3 up_b = ca.state.orientation * glm::dvec3{0, 1, 0};
    ca.aim = glm::normalize(glm::angleAxis(-rad(1.0), up_b) * nose);
    cb.aim = ca.aim;
    for (int i = 0; i < 600; ++i) {
        ca.tick(thr, kAp, a);
        cb.tick(thr, kAp, b);
        REQUIRE(ca.last_inputs.yaw == cb.last_inputs.yaw);
        REQUIRE(ca.last_inputs.pitch == cb.last_inputs.pitch);
        REQUIRE(ca.last_inputs.roll == cb.last_inputs.roll);
    }
}

// ---------------------------------------------------------------------------
// The astern degeneracy (CLAUDE.md S4a trap): a held aim directly BEHIND the
// nose collapses target_body to (0,0,+1) — the same x^2+y^2 -> 0 that makes
// bank_error assert at the nose. err ~ pi puts blend == 1, so a blend-only
// guard would still evaluate bank_error and abort the assert-live build. The
// cascade must not: it pulls through on the elevator (the +X astern tie-break
// + the near-astern elev latch), roll left to the FINE wings-hold.
// ---------------------------------------------------------------------------
TEST_CASE("cascade: aiming astern does not fire the bank_error assert") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    const sim::SimState s0 =
        harness::level_state(kAp, 140.0, 3000.0, up, heading);
    const glm::dvec3 nose = s0.orientation * glm::dvec3{0.0, 0.0, -1.0};

    // Exactly astern, one tick: the degenerate input. In an assert-live build
    // this aborts if bank_error is evaluated on (0,0,+1).
    control::Input in;
    in.target_dir_world = -nose;  // directly behind
    in.throttle = 0.7;
    const control::Output o =
        control::step(s0, in, control::reset(), kAp, kCp, kAp.sim_dt);
    CHECK(std::isfinite(o.telem.omega_des.x));
    CHECK(o.telem.e == Catch::Approx(kPi).margin(1e-9));
    CHECK(o.inputs.pitch > 0.0f);  // pull-through UP (the +X tie-break)

    // ...and held astern through a closed-loop pursuit (transported every
    // tick, driving through the near-astern latch region): never asserts,
    // stays finite, and the nose actually comes around.
    harness::ClosedLoop cl(s0, -nose);
    for (int i = 0; i < 600; ++i) {
        const control::Telemetry t = cl.tick(0.7, kAp, kCp);
        REQUIRE(finite_state(cl.state));
        REQUIRE(std::isfinite(t.e));
    }
    // 5 s of a max-rate pull swings the nose well off the original astern.
    const control::Telemetry last = cl.tick(0.7, kAp, kCp);
    CHECK(last.e < kPi * 0.9);
}

// ---------------------------------------------------------------------------
// The near-astern elevator-sign LATCH (SPEC §9.3, controller.cpp): across
// straight-behind the shortest-arc pitch sign flips wholesale, so once err
// crosses astern_on the elevator sign is LATCHED and held (through |demand.x|)
// until err falls below astern_off — otherwise the nose reverses at the pole.
// Pinned in isolation: a target 165 deg behind and slightly ABOVE (raw demand
// = pitch-UP, bankErr ~ 0 so the align gate stays open), err inside the latch
// band [160,170). With no latch the elevator follows the raw UP demand; a
// preset -1 latch must OVERRIDE it to pitch-DOWN. A cascade that ignored the
// latch (used raw demand.x) would pitch up in both -> the -1 case fails.
// ---------------------------------------------------------------------------
TEST_CASE("cascade: near-astern elevator-sign latch overrides the raw demand") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    const sim::SimState s0 =
        harness::level_state(kAp, 140.0, 3000.0, up, heading);
    // Body-frame target: 165 deg from the nose (-Z), above (+Y), no lateral.
    const glm::dvec3 tb = glm::normalize(glm::dvec3{0.0, 0.2588, 0.9659});
    control::Input in;
    in.target_dir_world = s0.orientation * tb;
    in.throttle = 0.7;

    auto run = [&](double latch) {
        control::Internal internal = control::reset();
        internal.elev_latch = latch;
        return control::step(s0, in, internal, kAp, kCp, kAp.sim_dt);
    };

    // Premise: err really is in the latch band, and the RAW (unlatched)
    // elevator follows the pitch-UP demand.
    const control::Output raw = run(0.0);
    REQUIRE(raw.telem.e > rad(160.0));
    REQUIRE(raw.telem.e < rad(170.0));
    CHECK(raw.telem.omega_des.x > 0.0);  // unlatched: raw demand (up)

    // The latch owns the sign: -1 forces DOWN despite the up demand; +1 agrees.
    CHECK(run(-1.0).telem.omega_des.x < 0.0);
    CHECK(run(+1.0).telem.omega_des.x > 0.0);
}

// ---------------------------------------------------------------------------
// BALLISTIC gates OFF coordination (SPEC §9.6.4: the tail-slide lies, alpha ~
// 180 / beta unreliable at v ~ 0). Below v_ballistic with a large injected
// sideslip and the target on the nose, yaw must be attitude-hold (~0), NOT the
// coordination command -K_coord*beta. A mutation re-enabling coordination in
// ballistic would command a large yaw here and fail.
// ---------------------------------------------------------------------------
TEST_CASE("AT-17: BALLISTIC gates off yaw coordination") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    // Speed below v_ballistic, a big steady sideslip (beta = 20 deg).
    const double v = 0.5 * kCp.v_ballistic;  // 15 m/s < 30
    sim::SimState s =
        harness::flight_state(kAp, v, 3000.0, up, heading, 0.0, rad(20.0), 0.0);
    control::Input in;
    in.target_dir_world = s.orientation * glm::dvec3{0.0, 0.0, -1.0};  // nose
    in.throttle = 0.0;
    const control::Output o =
        control::step(s, in, control::reset(), kAp, kCp, kAp.sim_dt);
    REQUIRE(o.telem.ballistic);
    REQUIRE(o.telem.extracted.beta == Catch::Approx(rad(20.0)).margin(1e-6));
    // Coordination OFF: yaw is attitude-hold (~0), not -K_coord*beta (~-0.7).
    const double coord_would_be = kCp.K_coord * rad(20.0);
    CHECK(std::abs(o.telem.omega_des.y) < 0.1 * coord_would_be);
}

// ---------------------------------------------------------------------------
// Purity (SPEC §9.7): control::step never mutates its `internal` argument, and
// is a pure function of (state, input, internal) — same inputs, same output.
// ---------------------------------------------------------------------------
TEST_CASE("cascade: step is pure - internal argument never mutated") {
    const glm::dvec3 up{3.0, -2.0, 5.0}, heading{1.0, 1.0, -0.3};
    const sim::SimState s = harness::flight_state(
        kAp, 130.0, 2500.0, up, heading, rad(25.0), rad(4.0), rad(3.0));
    control::Internal internal = control::reset();
    internal.integ = {0.01, -0.02, 0.03};
    internal.held_bank = rad(10.0);
    internal.regime = control::Regime::MANEUVER;
    const control::Internal before = internal;

    control::Input in;
    in.target_dir_world = glm::normalize(glm::dvec3{0.2, 0.3, -0.9});
    in.throttle = 0.6;

    const control::Output a =
        control::step(s, in, internal, kAp, kCp, kAp.sim_dt);
    // The argument is byte-for-byte unchanged (no aliasing into caller state).
    CHECK(before.integ == internal.integ);
    CHECK(before.held_bank == internal.held_bank);
    CHECK(before.regime == internal.regime);
    // Deterministic: the same call again gives the same Inputs.
    const control::Output b =
        control::step(s, in, internal, kAp, kCp, kAp.sim_dt);
    CHECK(a.inputs.pitch == b.inputs.pitch);
    CHECK(a.inputs.yaw == b.inputs.yaw);
    CHECK(a.inputs.roll == b.inputs.roll);
}

// ---------------------------------------------------------------------------
// Anti-windup UNWINDS, not merely freezes (SPEC §9.4). When an axis is
// saturated by a wound-up integrator AND the rate error now OPPOSES the command
// (the plant overshot and is swinging back), the integrator must integrate the
// opposing error to pull the output OUT of saturation — freeze-only pins it at
// the cap and blows through the return swing. REACHABLE at the committed gains
// below ~53 m/s: there the plant authority denom (c_pitch*q_att_floor ~ 1960
// Nm) is below K_wi_pitch*integ_cap (10000), so the capped integral ALONE
// saturates the Input and a small opposing rate makes eo*tau_cmd < 0. Pinned:
// one tick, integ at +cap, a small +pitch rate against a ~0 command.
// ---------------------------------------------------------------------------
TEST_CASE(
    "inner loop: anti-windup unwinds a saturated integrator on reversal") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    // Low speed -> floor-limited authority: the capped integral alone
    // saturates.
    sim::SimState s = harness::level_state(kAp, 25.0, 3000.0, up, heading);
    s.angular_vel.x = 0.02;  // a positive pitch rate (the command opposes it)
    control::Input in;
    in.target_dir_world =
        s.orientation * glm::dvec3{0.0, 0.0, -1.0};  // aim nose
    in.throttle = 0.0;
    control::Internal internal = control::reset();
    // A wound integral — 0.5 rad·s (predates integ_cap 0.5 -> 2.0: the value
    // is NOT the cap anymore, it just needs K_wi*0.5 = 30,000 N·m >> the
    // floored authority ~1,960, true at any loadable table). The S-dampff ff
    // term at V=25 / omega_des ~ 0 adds ~2.5 N·m — inert; the unwind clause
    // under test is damp_ff-independent.
    internal.integ.x = 0.5;

    const control::Output o =
        control::step(s, in, internal, kAp, kCp, kAp.sim_dt);
    // Premise: the pitch Input really is saturated (integral-dominated) while
    // the rate error opposes it (omega_des ~ 0 < the +0.02 rate) — so this tick
    // exercises the saturated branch, not the trivial |Input| < 1 one.
    REQUIRE(o.inputs.pitch == 1.0f);
    // It UNWOUND (integrated the opposing error), not froze at the cap: a
    // freeze-only rule (drop the `|| eo*tau_cmd < 0` term) leaves it AT 0.5.
    CHECK(o.internal.integ.x < 0.5);
}

// ---------------------------------------------------------------------------
// Push-vs-roll GEOMETRY gate (SPEC §9.3, S7-push): a target BELOW the nose and
// ahead of the wing-line (pitch-down angle <= down_enter) PUSHES (negative-G
// pitch, wings held); once it goes behind (> down_exit) it rolls through
// inverted instead. Deterministic on tick 1 from reset: at V=100, all sampled
// below-nose angles sit in the push zone (all entry legs met: |bankErr|=
// 180>120, elev<0, target_body.z <= z_enter); 135 deg below is behind the
// wing-line so the gate stays out. This pins the gate itself (raise
// down_enter past z(135) and it bunts instead of rolling; the round-1 golden
// could not, its script never aims below the nose). Replaces the old
// -G-budget ratio criterion.
//
// v5 RUNG E (Chad 2026-07-23, the knife-edge ruling) FIXTURE RE-DERIVATION:
// `[push_gate] horizon_enter/exit` (1/-1.5 -> 45/40 deg) gates FIRST, on the
// aim's WORLD elevation, before down_enter/down_exit even evaluate. At
// bankErr=180 exactly (x=0, this fixture's whole "below" family) elevation
// == target_body.y == -sin(d) exactly for this unbanked s0 -- UNLIKE the
// AT-15 bank-side-exit/bank-band legs (bank_eff near 90-100), bankErr=180
// has cos(180)=-1, no geometric cap, so simply deepening d is sufficient
// here (no aircraft-banking trick needed). The old shallow sample (6 deg,
// elevation -0.1045) is nowhere near the new -0.7071 depth requirement --
// under the rung-E table a shallow 6 deg-below is flown by the normal
// pointing law, NEVER the push commitment (see the folklore update in the
// AT-15 banner, test_acceptance.cpp). Moved to {50, 60, 85} deg: 50 is 5 deg
// past horizon_enter for margin (elevation -0.766), 60/85 unchanged (already
// comfortably deep: -0.866/-0.996). The "rolls past vertical" arm (135 deg,
// down_exit=96) is UNCHANGED in meaning -- its exclusion is via the z gate,
// independent of the horizon depth.
// ---------------------------------------------------------------------------
TEST_CASE(
    "cascade: push-vs-roll gate noses down below the nose, rolls past "
    "vertical") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    double thr = 0.7;
    const sim::SimState s0 =
        harness::level_trim_state(kAp, 100.0, 3000.0, up, heading, &thr);
    control::Input in;
    in.throttle = thr;
    auto below = [&](double deg) {
        const glm::dvec3 tb = glm::normalize(
            glm::dvec3{0.0, -std::sin(rad(deg)), -std::cos(rad(deg))});
        in.target_dir_world = s0.orientation * tb;
        return control::step(s0, in, control::reset(), kAp, kCp, kAp.sim_dt);
    };

    // S7-push (geometry gate): a below-nose aim NOSES DOWN (push, negative-G
    // pitch) up to the down_enter cutover — 50, 60, 85 deg all push (85 <
    // the 88 deg down_enter, and 50 clears horizon_enter=45 by 5 deg
    // margin). The OLD ratio gate rolled everything past ~7 deg below; this
    // is the "can't nose down without banking over" fix.
    for (double d : {50.0, 60.0, 85.0}) {
        const control::Output push = below(d);
        CHECK(push.telem.push_mode);
        CHECK(push.telem.omega_des.x < 0.0);  // nose down (negative-G pitch)
    }

    // Past vertical (behind the wing-line, > down_exit): the gate stays OUT so
    // the cascade rolls through for the loop (split-S), not an outside bunt.
    CHECK_FALSE(below(135.0).telem.push_mode);
}

// ---------------------------------------------------------------------------
// Rung F "THE SACRED MIDDLE" (v5 kernel-v5-reconcile, Chad 2026-07-24 fly):
// rung E's horizon_enter=45 deg blanketed EVERYTHING below the horizon, so a
// shallow straight-ahead dive (in-plane, 20-40 deg down) could no longer
// pure-pitch -- bank-to-turn rolled him over. His ruling: "when I nose
// straight down I need a wider knife edge... pitch straight down without
// tipping over". Fix: a second entry arm ORed onto the horizon leg --
// tightly in-plane (within side_pure_enter=18 deg of the vertical plane) AND
// below the horizon at ANY depth pure-pitches. F1/F2/F3 below probe exactly
// this arm; F4 (the existing rung-E deep fixtures above, 50/60/85/135) is
// re-run unchanged by construction -- this new arm only ADDS entry paths, it
// narrows nothing the deep fixtures already relied on.
// ---------------------------------------------------------------------------

// F1: a 25-deg-below, tightly IN-PLANE aim (aim_side ~ 0, side_pure_enter=18
// deg gate satisfied) engages push at SHALLOW depth (elevation -sin(25 deg)
// = -0.4226, nowhere near horizon_enter's -0.7071) -- the sacred middle at
// shallow depth, pure pitch-down with no roll commitment. Mutation: delete
// the sacred-middle OR-arm (entry reverts to the bare horizon_enter leg) ->
// this aim no longer reaches -0.7071 and push never engages -> fails.
TEST_CASE(
    "cascade: Rung F sacred middle - shallow in-plane dive pure-pitches "
    "(F1)") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    double thr = 0.7;
    const sim::SimState s0 =
        harness::level_trim_state(kAp, 100.0, 3000.0, up, heading, &thr);
    control::Input in;
    in.throttle = thr;
    // In-plane (x=0): aim_side is EXACTLY 0 by construction (matches the
    // "below" fixture technique above), aim_elev = -sin(25 deg).
    const glm::dvec3 tb = glm::normalize(
        glm::dvec3{0.0, -std::sin(rad(25.0)), -std::cos(rad(25.0))});
    in.target_dir_world = s0.orientation * tb;
    const control::Output o =
        control::step(s0, in, control::reset(), kAp, kCp, kAp.sim_dt);
    CHECK(o.telem.push_mode);
    CHECK(o.telem.omega_des.x < 0.0);  // nose down, pure pitch
}

// F2: a 25-deg-below LATERAL aim -- aim_side past side_pure_enter (18 deg)
// but still under the WIDER side_cone_enter (37.5 deg), so entry's own
// side_cone leg (unchanged, still ANDed onto every arm) would otherwise be
// satisfied -- must NOT engage: the sacred middle is a narrower subset, and
// this aim is shallow (elev -0.44, nowhere near horizon_enter -0.7071)
// AND off-axis (side ~0.47, past side_pure_enter's 0.309), so NEITHER OR-arm
// fires. The roll path (bank-to-turn) owns it instead. Mutation: widen
// side_pure_enter to the outer side_cone_enter value (37.5 deg, sin 0.609)
// -> this aim's side (0.47) falls under the widened threshold -> the sacred
// middle wrongly fires -> fails.
TEST_CASE(
    "cascade: Rung F sacred middle - shallow lateral dive stays on the "
    "roll path (F2)") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    double thr = 0.7;
    const sim::SimState s0 =
        harness::level_trim_state(kAp, 100.0, 3000.0, up, heading, &thr);
    control::Input in;
    in.throttle = thr;
    // off-nose radius c=40 deg, azimuth a=133 deg from straight-down (bank_eff
    // == a == 133 > push_gate_bank=120, so the bank leg is satisfied; only
    // the horizon/side arms are what F2 tests): elevation = sin(c)*cos(a) =
    // -0.4385, side = sin(c)*sin(a) = 0.4703 -- both computed and asserted as
    // premises below via the telemetry-observable fields (bank + push
    // omega sign), same discipline as the AT-15 fixture derivation.
    const double c = rad(40.0), a = rad(133.0);
    const glm::dvec3 tb = glm::normalize(glm::dvec3{
        std::sin(c) * std::sin(a), std::sin(c) * std::cos(a), -std::cos(c)});
    in.target_dir_world = s0.orientation * tb;
    const control::Output o =
        control::step(s0, in, control::reset(), kAp, kCp, kAp.sim_dt);
    CHECK_FALSE(o.telem.push_mode);
}

// F3: hysteresis — open-loop ripple of aim_side across the side_pure band
// (18/23 deg) at shallow depth (elevation held near -sin(25 deg) == the F1
// depth). Starts INSIDE the sacred middle (side well under 18 deg) so push
// engages tick 1, then ripples side_deg in [15,21] -- straddling the ENTER
// threshold (18) but staying well clear of the EXIT threshold (23): a
// properly hysteretic gate never re-exits (<=1 switch total, the initial
// entry), while a collapsed-band mutant (enter==exit==18) exits and
// re-enters every time the ripple crosses 18 (many switches). Mirrors the
// AT-15 bank-band-no-dither technique (test_acceptance.cpp).
TEST_CASE(
    "cascade: Rung F sacred middle - side-band ripple does not dither "
    "(F3)") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    double thr = 0.7;
    const sim::SimState s0 =
        harness::level_trim_state(kAp, 100.0, 3000.0, up, heading, &thr);
    control::Input in;
    in.throttle = thr;
    control::Internal internal = control::reset();
    const double elev = -std::sin(rad(25.0));  // shallow depth, fixed
    const int N = 60;
    int switches = 0, push_ticks = 0;
    bool prev = false;
    for (int i = 0; i < N; ++i) {
        // side_deg(0) = 18 - 3 = 15 (< enter=18, triggers entry tick 1), then
        // ripples [15,21] -- inside the pure band, never reaching exit=23.
        const double side_deg =
            18.0 - 3.0 * std::cos(2.0 * kPi * double(i) / double(N));
        const double side = std::sin(rad(side_deg));
        // z chosen to keep the vector near-unit and comfortably past the
        // down_z_enter threshold (deep off-nose); exact unit length is
        // restored by normalize() below.
        const double z =
            -std::sqrt(std::max(0.0, 1.0 - elev * elev - side * side));
        const glm::dvec3 tb = glm::normalize(glm::dvec3{side, elev, z});
        in.target_dir_world = s0.orientation * tb;
        const control::Output o =
            control::step(s0, in, internal, kAp, kCp, kAp.sim_dt);
        internal = o.internal;
        if (o.telem.push_mode != prev) ++switches;
        prev = o.telem.push_mode;
        if (o.telem.push_mode) ++push_ticks;
    }
    REQUIRE(push_ticks > 0);  // premise: the sacred middle actually engaged
    CHECK(switches <= 1);
}

// ---------------------------------------------------------------------------
// The AoA protection clamp consumes the FILTERED AoA, not the raw one (SPEC
// §9.3b: the low-pass is the sole smoothing exception, and filtering it is the
// whole point of the ballistic freeze+reseed above — a clamp on raw alpha makes
// that fix dead code). Pinned by injecting a filtered AoA past the limit while
// the raw AoA is ~0: the limiter must believe we are stalled, refuse the pull,
// and push DOWN. A raw-alpha consumer would let the up-demand through -> up.
// ---------------------------------------------------------------------------
TEST_CASE("cascade: AoA protection clamps on the FILTERED AoA, not the raw") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    const sim::SimState s0 =
        harness::level_state(kAp, 140.0, 3000.0, up, heading);
    const glm::dvec3 nose = s0.orientation * glm::dvec3{0.0, 0.0, -1.0};
    const glm::dvec3 right = s0.orientation * glm::dvec3{1.0, 0.0, 0.0};
    control::Input in;
    in.target_dir_world = glm::angleAxis(rad(10.0), right) * nose;  // 10 deg up
    in.throttle = 0.7;
    control::Internal internal = control::reset();
    internal.aoa_filtered = rad(40.0);  // >> aoa_max (14 deg): "we are stalled"

    const control::Output o =
        control::step(s0, in, internal, kAp, kCp, kAp.sim_dt);
    // Sanity: the RAW alpha really is ~0 (level, no sideslip) — a raw-alpha
    // consumer would NOT clamp and would let the pull through.
    REQUIRE(std::abs(o.telem.extracted.alpha) < rad(3.0));
    // Reading the filtered 40 deg, the limiter refuses the pull and pushes
    // DOWN.
    CHECK(o.telem.omega_des.x < 0.0);
}

// ---------------------------------------------------------------------------
// S-dampff (SPEC §0/§9.4): the damping FEEDFORWARD completes the plant
// inversion — tau_cmd += damp_ff * damp_axis * q_eff * omega_des_total, the
// KNOWN viscous torque fed forward AT THE DEMAND so the integrator never has
// to source it (its wound preload was carrying the nose past the aim at
// capture — the differential carry-past leg in test_acceptance owns that
// story; THIS leg pins the exact one-tick composition and its q sourcing).
// Which instruments are structurally blind to this term (the MB-atm
// discipline): every omega_des = 0 test (no demand, no ff), AT-18a (measures
// authority from omega = 0 AND recomputes its own tau), AT-12 (torque never
// enters the linear-work gate). So the term gets its own non-vacuous legs:
//   Sample A — 6 km altitude, unsaturated FINE pull: the emitted pitch Input
//     equals the config-recomputed composition through the SAME aero.h
//     primitives (rho_at at ALTITUDE — a sea-level-rho fork in the hoisted
//     q_eff_val separates here), oracle cast mirrored float-for-float
//     (the AT-12 float-seam lesson), omega_des sourced from telemetry (the
//     shipped demand, not a re-derived pointing path). A differential
//     ff-on/ff-off delta REQUIRE keeps the leg non-vacuous even if the
//     oracle expression were ever rewritten alongside a code mutation.
//   Sample B — the q_att_floor arm: at V = 15 the true q (112.5 Pa) sits
//     under the 280 Pa floor, and that state is BALLISTIC by construction
//     (floor crossover 23.7 m/s < v_ballistic 30 — REQUIRE'd as premise, the
//     S4a "know what the fixture builds" discipline; the composition is
//     branch-identical). angular_vel is preset to the probed omega_des so
//     eo == 0 EXACTLY and the emitted Input IS the ff term alone — a bare-q
//     (floorless) fork shifts it 2.5x, a dropped term zeroes it.
// Mutations that must fail: (1) delete the gated add — A's delta + both
// oracles; (2) q_eff_val loses the floor — B; (3) rho_at -> sea-level in the
// hoisted q — A. Verified on a CONFIRMED-fresh binary (stale-relink memory).
// ---------------------------------------------------------------------------
TEST_CASE("cascade: S-dampff one-tick composition + q_eff sourcing") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    // Self-armed (the Fly-7 lesson): the pins must not depend on the shipped
    // damp_ff dials.
    control::ControllerParams on = kCp, off = kCp;
    on.damp_ff_pitch = 1.0;
    off.damp_ff_pitch = 0.0;

    // ---- Sample A: 6 km, FINE 2-deg pull, unsaturated -------------------
    {
        const sim::SimState s0 =
            harness::level_state(kAp, 120.0, 6000.0, up, heading);
        const glm::dvec3 nose = s0.orientation * glm::dvec3{0.0, 0.0, -1.0};
        const glm::dvec3 right = s0.orientation * glm::dvec3{1.0, 0.0, 0.0};
        control::Input in;
        in.target_dir_world = glm::angleAxis(rad(2.0), right) * nose;
        in.throttle = 0.7;
        const control::Output o =
            control::step(s0, in, control::reset(), kAp, on, kAp.sim_dt);
        const control::Output o0 =
            control::step(s0, in, control::reset(), kAp, off, kAp.sim_dt);
        // Premises: a real demand, and an unsaturated emitted input.
        REQUIRE(std::abs(o.telem.omega_des.x) > 0.01);
        REQUIRE(std::abs(o.inputs.pitch) < 1.0f);
        // Non-vacuous: at 6 km the ff term visibly moves the input (~4%).
        REQUIRE(std::abs(o.inputs.pitch - o0.inputs.pitch) > 0.01f);
        // Exact composition oracle — the SAME aero.h primitives, the SAME
        // expression tree, the SAME float cast as the shipped path.
        const double alt = sim::altitude(s0.position, kAp);
        const double qe = sim::q_eff(
            sim::q_dyn(sim::rho_at(alt, kAp), o.telem.extracted.speed), kAp);
        const double eo = o.telem.omega_des.x - s0.angular_vel.x;
        double tau = on.K_w_pitch * eo + on.K_wi_pitch * 0.0;
        tau += on.damp_ff_pitch * kAp.damp_pitch * qe * o.telem.omega_des.x;
        const float expected = static_cast<float>(control::plant_invert(
            tau, kAp.c_pitch, o.telem.extracted.speed, alt, kAp));
        REQUIRE(o.inputs.pitch == expected);
    }

    // ---- Sample B: the q_att_floor / BALLISTIC arm, eo == 0 -------------
    {
        sim::SimState s0 = harness::level_state(kAp, 15.0, 3000.0, up, heading);
        // Premise: the floor genuinely binds at this state.
        REQUIRE(sim::q_dyn(sim::rho_at(3000.0, kAp), 15.0) < kAp.q_att_floor);
        const glm::dvec3 nose = s0.orientation * glm::dvec3{0.0, 0.0, -1.0};
        const glm::dvec3 right = s0.orientation * glm::dvec3{1.0, 0.0, 0.0};
        control::Input in;
        in.target_dir_world = glm::angleAxis(rad(3.0), right) * nose;
        in.throttle = 0.7;
        // Probe pass: read the demand this state produces...
        const control::Output probe =
            control::step(s0, in, control::reset(), kAp, on, kAp.sim_dt);
        REQUIRE(probe.telem.ballistic);
        REQUIRE(std::abs(probe.telem.omega_des.x) > 0.01);
        // ...then preset omega to it: eo == 0 bitwise (the demand is a pure
        // function of orientation/aim/velocity, none of which change), so
        // K_w contributes exactly 0 and the emitted Input IS the ff term.
        s0.angular_vel.x = probe.telem.omega_des.x;
        const control::Output o =
            control::step(s0, in, control::reset(), kAp, on, kAp.sim_dt);
        REQUIRE(o.telem.ballistic);
        REQUIRE(o.telem.omega_des.x == probe.telem.omega_des.x);
        REQUIRE(std::abs(o.inputs.pitch) < 1.0f);
        const double alt = sim::altitude(s0.position, kAp);
        const double qe = sim::q_eff(
            sim::q_dyn(sim::rho_at(alt, kAp), o.telem.extracted.speed), kAp);
        const double eo = o.telem.omega_des.x - s0.angular_vel.x;
        REQUIRE(eo == 0.0);
        double tau = on.K_w_pitch * eo + on.K_wi_pitch * 0.0;
        tau += on.damp_ff_pitch * kAp.damp_pitch * qe * o.telem.omega_des.x;
        const float expected = static_cast<float>(control::plant_invert(
            tau, kAp.c_pitch, o.telem.extracted.speed, alt, kAp));
        REQUIRE(o.inputs.pitch == expected);
        // The pure-ff input is nonzero — a dropped term emits exactly 0 here.
        REQUIRE(std::abs(o.inputs.pitch) > 0.01f);
    }

    // ---- Sample C: the YAW-axis indexing arm (diff red-team P2-1) --------
    // Every other ff leg pins pitch (i = 0), where a damp_ax[i] -> damp_ax[0]
    // or damp_ff[i] -> damp_ff[0] mis-wire is ACCIDENTALLY correct — and the
    // yaw/roll rungs are TOML-only flips, so the mis-arm would ship with no
    // code change and a 41%-low yaw ff (damp_yaw 8.0 vs damp_pitch 4.7), the
    // "silently mis-arms on the planned next rung" class. Self-armed
    // (shipped dials untouched): damp_ff_yaw = 1 with damp_ff_pitch
    // deliberately UNEQUAL (0.25) so a damp_ff[0] fork separates too. Same
    // eo == 0 construction as Sample B, lateral aim -> yaw demand, oracle
    // through damp_yaw / K_w_yaw / c_yaw.
    {
        control::ControllerParams yawed = kCp;
        yawed.damp_ff_pitch = 0.25;
        yawed.damp_ff_yaw = 1.0;
        sim::SimState s0 = harness::level_state(kAp, 15.0, 3000.0, up, heading);
        const glm::dvec3 nose = s0.orientation * glm::dvec3{0.0, 0.0, -1.0};
        const glm::dvec3 up_b = s0.orientation * glm::dvec3{0.0, 1.0, 0.0};
        control::Input in;
        in.target_dir_world = glm::angleAxis(-rad(3.0), up_b) * nose;  // right
        in.throttle = 0.7;
        const control::Output probe =
            control::step(s0, in, control::reset(), kAp, yawed, kAp.sim_dt);
        REQUIRE(probe.telem.ballistic);
        REQUIRE(std::abs(probe.telem.omega_des.y) > 0.01);
        s0.angular_vel.y = probe.telem.omega_des.y;
        const control::Output o =
            control::step(s0, in, control::reset(), kAp, yawed, kAp.sim_dt);
        REQUIRE(o.telem.omega_des.y == probe.telem.omega_des.y);
        REQUIRE(std::abs(o.inputs.yaw) < 1.0f);
        const double alt = sim::altitude(s0.position, kAp);
        const double qe = sim::q_eff(
            sim::q_dyn(sim::rho_at(alt, kAp), o.telem.extracted.speed), kAp);
        const double eo = o.telem.omega_des.y - s0.angular_vel.y;
        REQUIRE(eo == 0.0);
        double tau = yawed.K_w_yaw * eo + yawed.K_wi_yaw * 0.0;
        tau += yawed.damp_ff_yaw * kAp.damp_yaw * qe * o.telem.omega_des.y;
        const float expected = static_cast<float>(control::plant_invert(
            tau, kAp.c_yaw, o.telem.extracted.speed, alt, kAp));
        REQUIRE(o.inputs.yaw == expected);
        REQUIRE(std::abs(o.inputs.yaw) > 0.01f);
    }
}

// ---------------------------------------------------------------------------
// ff-radius CORRECTNESS fix (auto/feel-research): the curvature feedforward
// divisor is the ACTUAL orbital radius |position| = R + altitude, NOT the baked
// planet radius ap.R. Aim ON the nose -> the pointing term is deadzoned to 0,
// so the ONLY commanded rate is the curvature ff: omega_des == body(cross(
// local_up, v)/|position|). Pin its magnitude against |position| at altitude,
// prove it is STRICTLY smaller than the old /R crumb by exactly R/(R+h), and
// prove the h=0 arm is BIT-IDENTICAL to /R (the honest knob-off proof — at
// sea level the two divisors coincide, so shipping the fix cannot move a
// sea-level golden). The park deficit this removes (nose parks V*h/(R*(R+h)*
// K_theta) above the aim in cruise) is re-verifiable in one command:
//   seads_harness step pitch 20 250 2400 "" 3000   (park CROSSES lo post-fix)
// ---------------------------------------------------------------------------
TEST_CASE("cascade: curvature ff divides by |position|, not baked R") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};

    auto ff_x_at = [&](double V, double alt) {
        // Aim on the nose -> deadzone -> omega_des is the pure curvature ff.
        const sim::SimState s0 = harness::level_state(kAp, V, alt, up, heading);
        const glm::dvec3 nose = s0.orientation * glm::dvec3{0.0, 0.0, -1.0};
        control::Input in;
        in.target_dir_world = nose;  // exactly on the nose -> deadzoned
        in.throttle = 0.7;
        const control::Output o =
            control::step(s0, in, control::reset(), kAp, kCp, kAp.sim_dt);
        REQUIRE(o.telem.deadzoned);  // premise: pointing term is zeroed
        return std::make_pair(o.telem.omega_des.x, s0);
    };
    auto ff_x = [&](double alt) { return ff_x_at(200.0, alt); };

    // Oracle: the ff reconstructed with the ACTUAL orbital radius |position|.
    auto ff_oracle = [&](const sim::SimState& s, double radius) {
        const glm::dvec3 lu = sim::local_up(s.position);
        const glm::dvec3 ffw = glm::cross(lu, s.velocity) / radius;
        return sim::body_dir_of(s.orientation, ffw).x;
    };

    // --- Sample at altitude: emitted ff matches /|position|, NOT /R ----------
    {
        const auto [emitted, s] = ff_x(3000.0);
        const double r = glm::length(s.position);
        REQUIRE(r == Catch::Approx(kAp.R + 3000.0).margin(1e-9));
        // Shipped ff == reconstruction at the TRUE radius (bit-tight).
        CHECK(emitted == Catch::Approx(ff_oracle(s, r)).margin(1e-12));
        // ...and it is STRICTLY smaller in magnitude than the old /R crumb by
        // exactly R/(R+h) — the deficit the fix removes.
        const double old_ff = ff_oracle(s, kAp.R);
        REQUIRE(std::abs(old_ff) > 0.0);
        CHECK(std::abs(emitted) < std::abs(old_ff));
        CHECK(emitted / old_ff ==
              Catch::Approx(kAp.R / (kAp.R + 3000.0)).epsilon(1e-9));
    }

    // --- h = 8000 (top of the flyable band): ratio holds R/(R+h) -------------
    // Pins the |position| divisor where h is a LARGE fraction of R, so a
    // partial or clamped correction (e.g. /R + small crumb) that survives the
    // 3 km point separates cleanly here.
    {
        const auto [emitted, s] = ff_x_at(200.0, 8000.0);
        const double r = glm::length(s.position);
        REQUIRE(r == Catch::Approx(kAp.R + 8000.0).margin(1e-9));
        CHECK(emitted == Catch::Approx(ff_oracle(s, r)).margin(1e-12));
        const double old_ff = ff_oracle(s, kAp.R);
        REQUIRE(std::abs(old_ff) > 0.0);
        CHECK(std::abs(emitted) < std::abs(old_ff));
        CHECK(emitted / old_ff ==
              Catch::Approx(kAp.R / (kAp.R + 8000.0)).epsilon(1e-9));
    }

    // --- V = 280 at h = 3000: the ratio is V-INDEPENDENT --------------------
    // The R/(R+h) deficit is a pure geometry factor — it must not change with
    // speed (ff scales with V through cross(local_up, v), but the divisor does
    // not). A mutant that folded V into the divisor would pass at V=200 and
    // fail here.
    {
        const auto [emitted, s] = ff_x_at(280.0, 3000.0);
        const double r = glm::length(s.position);
        REQUIRE(r == Catch::Approx(kAp.R + 3000.0).margin(1e-9));
        CHECK(emitted == Catch::Approx(ff_oracle(s, r)).margin(1e-12));
        const double old_ff = ff_oracle(s, kAp.R);
        REQUIRE(std::abs(old_ff) > 0.0);
        CHECK(std::abs(emitted) < std::abs(old_ff));
        // Same geometry ratio as the V=200 h=3000 block above -> V-independent.
        CHECK(emitted / old_ff ==
              Catch::Approx(kAp.R / (kAp.R + 3000.0)).epsilon(1e-9));
    }

    // --- h = 0 bit-identity arm: |position| == R, fix is a no-op -------------
    {
        const auto [emitted, s] = ff_x(0.0);
        const double r = glm::length(s.position);
        REQUIRE(r == kAp.R);  // exactly, |unit|*R
        // At sea level the corrected divisor and the old ap.R divisor are the
        // SAME value, so the emitted ff is bit-identical to the /R oracle: the
        // fix cannot move a sea-level result (the honest knob-off proof).
        CHECK(emitted == ff_oracle(s, kAp.R));
        CHECK(emitted == ff_oracle(s, r));
    }
}

// ---------------------------------------------------------------------------
// Kernel v5 rung A -- S-truedepth (docs/v5_kernel_handoff.md): the glance
// depth a capture event earns is its own stopping distance under full
// trackable braking, minned with the wall -- rim_live = (depth_frac > 0)
// ? min(rim_frac*circle, depth_frac*glance) : rim_frac*circle, glance =
// w_ev^2/(2*alpha_u) ALREADY computed at ENGAGE (H1: the same derived call).
// These legs mirror test_capture.cpp's S-rimshot drivers (flick_run/Tick/
// first_state), reproduced here (separate translation unit) but recording
// the FULL 2D demand/rate vectors so the oracle can recompute w_ev/glance
// exactly as production does -- config-relative bounds only, every number
// derived from the SAME sim::ang_accel_max_derived call the mechanism uses.
// ---------------------------------------------------------------------------
namespace {

struct DepthTick {
    glm::dvec3 dem{0.0};  // pointing-plane demand (x=pitch,y=yaw), body
    glm::dvec3 w{0.0};    // body angular_vel BEFORE the tick
    glm::dvec3 cff{0.0};  // curvature ff at the pre-tick state (body)
    double speed = 0.0;
    double alt = 0.0;
    control::CaptureState cap = control::CaptureState::IDLE;
    double cap_rim_t = 0.0;  // internal AFTER the tick (event's stored ref)
};

glm::dvec3 depth_curvature_ff_body(const sim::SimState& s) {
    const glm::dvec3 up = glm::normalize(s.position);
    return sim::body_dir_of(
        s.orientation, glm::cross(up, s.velocity) / glm::length(s.position));
}

// Drive the harness step fixture (level trim, GROUNDED spawn tick, 1 s
// settle) then a pitch-up set-jump of step_deg reported through the
// aim_moved seam for exactly one tick -- the S-rimshot flick_run pattern
// (test_capture.cpp), reproduced here with the full vector records T1-T4
// need.
std::vector<DepthTick> depth_flick_run(double step_deg, double V,
                                       const control::ControllerParams& cp,
                                       int ticks, bool yaw_axis = false,
                                       double ease_s = 0.0) {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    double thr = 0.0;
    const sim::SimState s0 =
        harness::level_trim_state(kAp, V, 3000.0, up, heading, &thr);
    harness::ClosedLoop cl(s0, glm::dvec3{0.0, 0.0, -1.0});
    cl.aim_nose();
    cl.tick(thr, kAp, cp, /*grounded=*/true);
    for (int i = 0; i < 120; ++i) cl.tick(thr, kAp, cp);
    // Pitch: rotate the aim about body-right. Yaw: about local_up (the
    // harness run_track convention) — the lateral arrival rides the
    // wny_ev/yaw_coord compensation path the pitch legs cannot see (the
    // "run the instrument on every axis" trap, red-team P2).
    // ease_s > 0: the aim moves as a smoothstep HAND EASE over ease_s
    // seconds instead of an instant flick — an instant lateral flick
    // always arrives wall-hot (the engage-time glance quantizes ~7x
    // between neighboring amplitudes), so the yaw DEPTH arm is reachable
    // only by the eased approach, exactly the hand motion Chad's report
    // describes. The axis is parallel-transported per tick like the
    // harness nudge driver (control::transport_aim on the SAME up pair
    // the tick consumes), and the scripted rate feeds the aim_moved/
    // aim_rate_world seam so the engage ratio reads the honest lockstep.
    const glm::dvec3 flick_axis =
        yaw_axis ? sim::local_up(cl.state.position)
                 : cl.state.orientation * glm::dvec3{1.0, 0.0, 0.0};
    if (ease_s <= 0.0)
        cl.aim =
            glm::normalize(glm::angleAxis(rad(step_deg), flick_axis) * cl.aim);

    glm::dvec3 ease_axis = flick_axis;  // transported per tick while easing
    const int ease_ticks =
        ease_s > 0.0 ? static_cast<int>(std::lround(ease_s / kAp.sim_dt)) : 0;
    double prev_ang = 0.0;

    std::vector<DepthTick> tr;
    tr.reserve(ticks);
    for (int i = 0; i < ticks; ++i) {
        if (ease_ticks > 0 && i < ease_ticks) {
            const glm::dvec3 up_now = sim::local_up(cl.state.position);
            ease_axis = control::transport_aim(ease_axis, cl.prev_up, up_now);
            const double x = static_cast<double>(i + 1) / ease_ticks;
            const double ang = rad(step_deg) * (3.0 * x * x - 2.0 * x * x * x);
            const double dang = ang - prev_ang;
            prev_ang = ang;
            cl.aim = glm::normalize(glm::angleAxis(dang, ease_axis) * cl.aim);
            cl.aim_moved = true;
            cl.aim_rate_world = (dang / kAp.sim_dt) * ease_axis;
        } else {
            cl.aim_moved = (ease_ticks == 0 && i == 0);
            cl.aim_rate_world = glm::dvec3{0.0};
        }
        DepthTick tk;
        tk.dem = control::rotation_demand_body(cl.state.orientation,
                                               glm::normalize(cl.aim));
        tk.w = cl.state.angular_vel;
        tk.cff = depth_curvature_ff_body(cl.state);
        tk.alt = glm::length(cl.state.position) - kAp.R;
        const control::Telemetry t = cl.tick(thr, kAp, cp);
        tk.speed = t.extracted.speed;
        tk.cap = t.capture;
        tk.cap_rim_t = cl.internal.cap_rim_t;  // internal AFTER the tick
        tr.push_back(tk);
    }
    return tr;
}

int depth_first_state(const std::vector<DepthTick>& tr,
                      control::CaptureState st, int from = 0) {
    for (size_t i = static_cast<size_t>(from); i < tr.size(); ++i)
        if (tr[i].cap == st) return static_cast<int>(i);
    return -1;
}

// Max excursion past center (opposite sign from the ENGAGE-tick demand), from
// i_start through the event's own IDLE exit -- the require_traverse far_peak
// pattern (test_capture.cpp), reproduced.
double depth_far_peak(const std::vector<DepthTick>& tr, int i_start) {
    const double sign0 = tr[i_start].dem.x >= 0.0 ? 1.0 : -1.0;
    const int i_ret =
        depth_first_state(tr, control::CaptureState::RETURN, i_start);
    const int i_exit = depth_first_state(tr, control::CaptureState::IDLE,
                                         i_ret >= 0 ? i_ret : i_start);
    const int hi = i_exit >= 0 ? i_exit : static_cast<int>(tr.size());
    double peak = 0.0;
    for (int i = i_start; i < hi; ++i)
        peak = std::max(peak, -tr[i].dem.x * sign0);
    return peak;
}

// The oracle: reproduces w_ev/alpha_u/glance from the ENGAGE-tick's recorded
// state, via the SAME sim::ang_accel_max_derived call the mechanism uses
// (H1 -- never a re-derivation).
struct DepthOracle {
    double ux, uy, w_ev, alpha_u, glance;
};
DepthOracle depth_oracle(const DepthTick& e) {
    const double dlen = std::sqrt(e.dem.x * e.dem.x + e.dem.y * e.dem.y);
    const double ux = e.dem.x / dlen, uy = e.dem.y / dlen;
    const double wnx = e.w.x - e.cff.x, wny_ev = e.w.y - e.cff.y;
    const double w_ev = std::max(wnx * ux + wny_ev * uy, 0.0);
    const double a_p = sim::ang_accel_max_derived(kAp.c_pitch, kAp.I_pitch,
                                                  e.speed, e.alt, kAp);
    const double a_y =
        sim::ang_accel_max_derived(kAp.c_yaw, kAp.I_yaw, e.speed, e.alt, kAp);
    const double alpha_u = std::min(a_p / std::max(std::abs(ux), 1e-9),
                                    a_y / std::max(std::abs(uy), 1e-9));
    return {ux, uy, w_ev, alpha_u, w_ev * w_ev / (2.0 * alpha_u)};
}

}  // namespace

// T1 -- momentum-earned apex: a slow-momentum arrival (V=100, a 4 deg nudge
// -- a V/deflection sweep found this the smallest step that still clears the
// glance_frac x circle DIRECT-SEEK floor, so it genuinely takes the CARRY
// path) engages with glance much less than rim. Mutation this kills:
// min->max in rim_live (swapping the clamp would drive the apex OUT toward
// the wall instead of holding it at the momentum's own depth) -- and, since
// the far excursion is measured through BOTH CARRY and RETURN, a stale
// cp.capture_rim_frac*circle read surviving at the RETURN outbound emission
// site (rather than ns.cap_rim_t) shows up identically: the apex would blow
// back out to about rim_full and fail the < 0.5x bound below.
TEST_CASE(
    "S-truedepth: a slow arrival's apex is its own momentum-earned depth") {
    REQUIRE(kCp.capture_depth_frac > 0.0);  // premise: the shipped dial is live
    // machine retired in the committed table (carry 0); this leg pins the
    // PARKED machinery for the walk-back
    control::ControllerParams cp_arm = kCp;
    cp_arm.capture_carry = 1.0;
    const std::vector<DepthTick> tr = depth_flick_run(4.0, 100.0, cp_arm, 900);
    const int i_carry = depth_first_state(tr, control::CaptureState::CARRY);
    REQUIRE(i_carry >= 0);  // premise: this fixture takes the CARRY path

    const DepthOracle o = depth_oracle(tr[i_carry]);
    const double rim_full = kCp.capture_rim_frac * kCp.capture_circle;
    // Premise: the momentum genuinely earns LESS than the wall (the depth
    // term binds) -- else this exercises the wall-cap arm, not the
    // momentum-earned one (that is T2's job). Config-relative: the earned
    // target is depth_frac x glance (the shipped dial scales the physics
    // depth; the wall still caps it -- min below).
    // Rung A2: the earned target rides the sub-wall CURVE — rim * s^pow on
    // the saturation s = min(1, depth_frac*glance/rim). At pow 1 this is
    // exactly rung A's min(rim, depth*glance).
    const double sat =
        std::min(1.0, kCp.capture_depth_frac * o.glance / rim_full);
    REQUIRE(sat < 1.0);  // premise: the depth term binds (sub-wall)
    const double earned = rim_full * std::pow(sat, kCp.capture_depth_pow);
    CHECK(tr[i_carry].cap_rim_t == Catch::Approx(earned).margin(1e-9));

    const double far_peak = depth_far_peak(tr, i_carry);
    CHECK(far_peak <= std::max(2.0 * earned, rad(0.1)));
    CHECK(far_peak < 0.5 * rim_full);
}

// T2 -- wall preservation: a fast arrival (V=80, a 12 deg step -- the same
// grid class whose engage momentum earns MORE than the wall) still lands the
// apex in the v4 grid band [0.85, 1.15] x rim -- v4 bit-behavior at
// depth_frac >= 1 whenever glance >= rim (the wall wins by construction:
// w_wall = w_ev*sqrt(depth_frac) >= w_ev there). Mutation this kills:
// min->max in rim_live from the OTHER end (T1 pins the momentum-earned side;
// a max() mutant only bites when depth_frac*glance > rim_full -- exactly
// THIS fixture) -- an unclamped max would blow the apex to several times
// rim, failing the upper bound.
TEST_CASE("S-truedepth: a fast arrival still glances dead on the wall") {
    // machine retired in the committed table (carry 0); this leg pins the
    // PARKED machinery for the walk-back
    control::ControllerParams cp_arm = kCp;
    cp_arm.capture_carry = 1.0;
    const std::vector<DepthTick> tr = depth_flick_run(12.0, 80.0, cp_arm, 900);
    const int i_carry = depth_first_state(tr, control::CaptureState::CARRY);
    REQUIRE(i_carry >= 0);

    const DepthOracle o = depth_oracle(tr[i_carry]);
    const double rim_full = kCp.capture_rim_frac * kCp.capture_circle;
    REQUIRE(o.glance >= rim_full);  // premise: momentum exceeds the wall
    CHECK(tr[i_carry].cap_rim_t == Catch::Approx(rim_full).margin(1e-9));

    const double far_peak = depth_far_peak(tr, i_carry);
    REQUIRE(far_peak >= 0.85 * rim_full);
    CHECK(far_peak <= 1.15 * rim_full);
}

// T3 -- knob-off: the SAME slow-momentum scenario as T1, flown twice -- once
// at the shipped depth_frac (the momentum-earned shrink) and once at
// depth_frac = 0 (v4 fixed-rim, structurally OFF). The two arms must
// SEPARATE: off reaches the legacy wall band, on stays under T1's bound.
// Mutation this kills: depth_frac ignored (the dial wired but never gating
// rim_live's arm -- both runs would land at the same apex and the
// separation checks below would fail).
TEST_CASE(
    "S-truedepth: depth_frac 0 is the v4 fixed-rim knob-off, the arms "
    "separate") {
    // machine retired in the committed table (carry 0); this leg pins the
    // PARKED machinery for the walk-back (BOTH arms self-armed — they
    // separate on depth_frac, not on carry)
    control::ControllerParams cp_arm = kCp;
    cp_arm.capture_carry = 1.0;
    control::ControllerParams cp_off = cp_arm;
    cp_off.capture_depth_frac = 0.0;
    const std::vector<DepthTick> tr_on =
        depth_flick_run(4.0, 100.0, cp_arm, 900);
    const std::vector<DepthTick> tr_off =
        depth_flick_run(4.0, 100.0, cp_off, 900);
    const int i_on = depth_first_state(tr_on, control::CaptureState::CARRY);
    const int i_off = depth_first_state(tr_off, control::CaptureState::CARRY);
    REQUIRE(i_on >= 0);
    REQUIRE(i_off >= 0);

    const double rim_full = kCp.capture_rim_frac * kCp.capture_circle;
    const double far_on = depth_far_peak(tr_on, i_on);
    const double far_off = depth_far_peak(tr_off, i_off);
    CHECK(far_off >= 0.85 * rim_full);  // v4 fixed-rim behavior restored
    CHECK(far_on < 0.5 * rim_full);     // T1's shrink still holds
    CHECK(far_on < far_off);            // the arms genuinely SEPARATE
}

// T4 -- stored-reference pin: cap_rim_t is captured ONCE at ENGAGE and does
// NOT shrink (or change at all) as the live rate decays through the event --
// it is the event's own reference (like cap_err0), never re-derived from the
// ticking rate. Driven open-loop (the S6 dwelling-signal discipline: a
// closed-loop chase would pin nothing here, only the exit). Mutation this
// kills: cap_rim_t recomputed live (re-deriving rim_t from the CURRENT
// tick's rate each step would track the injected decay below and this
// exact-equality CHECK would fail every iteration after the first).
TEST_CASE(
    "S-truedepth: cap_rim_t is the event's ENGAGE-time reference, never "
    "re-derived live") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    const sim::SimState s0 =
        harness::level_state(kAp, 140.0, 3000.0, up, heading);
    const glm::dvec3 nose = s0.orientation * glm::dvec3{0.0, 0.0, -1.0};
    const glm::dvec3 right = s0.orientation * glm::dvec3{1.0, 0.0, 0.0};

    control::Internal ic = control::reset();
    ic.capture = control::CaptureState::CARRY;
    ic.cap_ux = 1.0;
    ic.cap_uy = 0.0;
    ic.cap_w_hold = 0.3;
    ic.cap_err0 = rad(10.0);
    // A deliberately-planted apex target strictly BELOW the wall (the
    // momentum-earned shrink this rung ships): a live re-derivation from a
    // LARGER injected rate below would disagree with this stored value.
    ic.cap_rim_t = rad(0.10);
    REQUIRE(ic.cap_rim_t < kCp.capture_rim_frac * kCp.capture_circle);

    control::Input in;
    in.target_dir_world = glm::angleAxis(rad(9.0), right) * nose;
    in.throttle = 0.7;

    double wx = 0.4;  // the live closing rate, made to DECAY tick over tick
    control::Internal cur = ic;
    for (int i = 0; i < 4; ++i) {
        sim::SimState s = s0;
        s.angular_vel.x = wx;
        const control::Output o =
            control::step(s, in, cur, kAp, kCp, kAp.sim_dt);
        REQUIRE(o.telem.capture != control::CaptureState::IDLE);  // still owned
        CHECK(o.internal.cap_rim_t == ic.cap_rim_t);  // UNCHANGED, exactly
        cur = o.internal;
        wx *= 0.5;
    }
}

// T5 -- the YAW axis (red-team P2, the repo's top trap: "run the instrument
// on every axis before committing a cascade mechanism"): a lateral arrival
// rides the wny_ev/yaw_coord compensation path the pitch legs are
// structurally blind to. Fixture found by sweep: a 12 deg yaw flick at V=80
// takes the CARRY path with the depth term binding well under the wall.
// Same contract as T1: cap_rim_t is the config-derived earned depth, the
// apex holds at the momentum's own scale, not the wall. Mutation this
// kills: any yaw-side fork of the rim read (a cp wall read surviving on a
// yaw-reached site) -- and the T1 mutants, on the axis Chad's report named.
TEST_CASE("S-truedepth: the yaw arrival earns its own depth too") {
    REQUIRE(kCp.capture_depth_frac > 0.0);
    // machine retired in the committed table (carry 0); this leg pins the
    // PARKED machinery for the walk-back
    control::ControllerParams cp_arm = kCp;
    cp_arm.capture_carry = 1.0;
    const std::vector<DepthTick> tr =
        depth_flick_run(12.0, 80.0, cp_arm, 900, /*yaw_axis=*/true,
                        /*ease_s=*/0.7);
    const int i_carry = depth_first_state(tr, control::CaptureState::CARRY);
    REQUIRE(i_carry >= 0);  // premise: the lateral arrival takes CARRY

    // BEHAVIORAL contract, direct reads (no recomputed oracle: the engage
    // fires mid-ease, where the controller nets the FILTERED aim rate out
    // of w_ev — a test-side recompute cannot see that filter state; the
    // exact oracle equality is T1's job on the pitch axis). The yaw-side
    // wiring is what this leg pins: a cp-wall read surviving on any
    // yaw-reached rim site gives cap_rim_t == rim_full (kills the first
    // CHECK) and drives the apex to the wall (kills the last).
    const double rim_full = kCp.capture_rim_frac * kCp.capture_circle;
    const double rim_ev = tr[i_carry].cap_rim_t;
    REQUIRE(rim_ev > 0.0);
    CHECK(rim_ev < 0.75 * rim_full);  // the depth arm genuinely bound

    const double far_peak = depth_far_peak(tr, i_carry);
    CHECK(far_peak <= 2.0 * rim_ev);    // apex at the event's own scale
    CHECK(far_peak < 0.85 * rim_full);  // never the manufactured wall bounce
}

// T6 -- rung A2 knob-off + separation: the SAME slow-momentum fixture flown
// at depth_pow = 1 must reproduce rung A's linear law min(rim, depth*glance)
// exactly (the fly fallback is bit-honest), and the shipped pow must land a
// STRICTLY shallower target than pow 1 (the curve genuinely presses the
// small end). Mutation this kills: pow ignored (rim_live stuck at the
// linear law -- the equality leg passes but the separation fails), or the
// pow==1 branch re-deriving through std::pow (a pow(x,1.0) ulp wobble
// breaks the exact-equality leg on a toolchain where it is inexact).
TEST_CASE(
    "S-truedepth A2: pow 1 is the rung-A linear law, the shipped curve "
    "lands shallower") {
    REQUIRE(kCp.capture_depth_pow > 1.0);  // premise: a curve actually ships
    // machine retired in the committed table (carry 0); this leg pins the
    // PARKED machinery for the walk-back (both arms self-armed — they
    // separate on depth_pow, not on carry)
    control::ControllerParams cp_arm = kCp;
    cp_arm.capture_carry = 1.0;
    control::ControllerParams cp_lin = cp_arm;
    cp_lin.capture_depth_pow = 1.0;
    const std::vector<DepthTick> tr_lin =
        depth_flick_run(4.0, 100.0, cp_lin, 900);
    const std::vector<DepthTick> tr_cur =
        depth_flick_run(4.0, 100.0, cp_arm, 900);
    const int i_lin = depth_first_state(tr_lin, control::CaptureState::CARRY);
    const int i_cur = depth_first_state(tr_cur, control::CaptureState::CARRY);
    REQUIRE(i_lin >= 0);
    REQUIRE(i_cur >= 0);

    // The two runs share one trajectory up to the engage tick (the curve
    // only changes the STORED target), so the linear arm's target is the
    // rung-A formula on ITS engage oracle, exactly.
    const DepthOracle o = depth_oracle(tr_lin[i_lin]);
    const double rim_full = kCp.capture_rim_frac * kCp.capture_circle;
    CHECK(tr_lin[i_lin].cap_rim_t ==
          Catch::Approx(std::min(rim_full, kCp.capture_depth_frac * o.glance))
              .margin(1e-9));
    // Separation: the shipped curve presses the sub-wall target under the
    // linear one (strictly -- the whole point of the rung).
    CHECK(tr_cur[i_cur].cap_rim_t < 0.75 * tr_lin[i_lin].cap_rim_t);
}

// ---------------------------------------------------------------------------
// Kernel v5 rung C2b -- S-holdline SAG SERVO (docs/v5_kernel_handoff.md
// "RUNG C: HOLD THE LINE", Chad fly-2). SUPERSEDES the rung-C2 align_f floor
// (director ruling on the AT-12 trace): a pull_floor*w_push floor paid out
// the FULL g-budget at a large bank misalignment to fix a modest sag,
// slingshotting the nose past vertical (AT-12's n_min_sustained cratered to
// -2.7 g at the apex). The replacement is a dedicated proportional servo on
// the sag itself -- through the SAME sqrt_law braking-law family every
// pointing demand uses (K_theta/aB_pitch/w_max_pitch) -- that only RAISES
// the legacy align-faded pitch (max(), never fights a pull already fighting
// harder) and is COMPLETELY INDEPENDENT of bank_eff/align: unlike the
// deleted floor, this servo does not need a "sag window" bank angle to be
// visible, since it never reads target_body/bank_eff at all. Open-loop
// control::step probes, following the file's "signs propagate" one-tick
// style.
//
// Fixture: nose pitched pitch_down_deg BELOW the horizon, then banked
// bank_deg about its OWN axis -- banking rotates only the body right/up
// axes, never the nose vector itself, so nose_elev is set purely by the
// pitch step and is unaffected by the bank. aim = the ORIGINAL level
// heading itself, which sits EXACTLY at the world horizon
// (dot(heading, up) == 0 identically, so aim_elev == 0 exactly and push
// mode's horizon-enter gate -- which requires the aim to be genuinely
// BELOW the horizon -- can never fire here, regardless of elev's sign).
// bank_deg == 70 is kept from the rung-C2 fixture (still a clean near-0
// floor-off baseline: align ~ cos(70)^6 ~ 0.0016) though it is no longer
// load-bearing for the servo itself -- TC1/TC2 still want a near-0 legacy
// arm to show the servo's raise cleanly.
// ---------------------------------------------------------------------------
namespace {

sim::SimState holdline_fixture(double pitch_down_deg, double bank_deg,
                               const glm::dvec3& up,
                               const glm::dvec3& heading) {
    sim::SimState s = harness::level_state(kAp, 140.0, 3000.0, up, heading);
    const glm::dvec3 right0 = s.orientation * glm::dvec3{1.0, 0.0, 0.0};
    // right x nose == up (the body-frame identity, carried by the
    // orientation similarity transform: X x (-Z) == Y) -- so a NEGATIVE
    // angle about right0 pitches the nose BELOW the horizon (nose.up goes
    // negative), a positive angle pitches it above.
    s.orientation = glm::normalize(
        glm::angleAxis(-rad(pitch_down_deg), right0) * s.orientation);
    const glm::dvec3 nose_p = s.orientation * glm::dvec3{0.0, 0.0, -1.0};
    // Bank about the (now-pitched) nose axis: rotates right/up_body only --
    // the nose vector, and therefore nose_elev, is untouched by this step.
    s.orientation =
        glm::normalize(glm::angleAxis(rad(bank_deg), nose_p) * s.orientation);
    const glm::dvec3 nose = s.orientation * glm::dvec3{0.0, 0.0, -1.0};
    s.velocity = 140.0 * nose;
    s.last_vhat = nose;
    return s;
}

control::Output holdline_step_aim(double pitch_down_deg, double bank_deg,
                                  const glm::dvec3& aim,
                                  const control::ControllerParams& cp) {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    const sim::SimState s =
        holdline_fixture(pitch_down_deg, bank_deg, up, heading);
    control::Input in;
    in.target_dir_world = aim;
    in.throttle = 0.7;
    return control::step(s, in, control::reset(), kAp, cp, kAp.sim_dt);
}

control::Output holdline_step(double pitch_down_deg, double bank_deg,
                              const control::ControllerParams& cp) {
    // aim == the original level heading: elevation 0 exactly, azimuth 0
    // (matches the nose's pre-pitch heading), so err tracks pitch_down_deg
    // directly and sag == sin(rad(pitch_down_deg)) exactly.
    const glm::dvec3 heading{0.0, 0.0, -1.0};
    return holdline_step_aim(pitch_down_deg, bank_deg, heading, cp);
}

// Config-recomputed oracle for the servo's sqrt_law demand (H1: the SAME
// derived call the mechanism uses -- sim::ang_accel_max_derived, the exact
// w_max_pitch expression from controller.cpp -- never a re-derived copy).
// sqrt_law itself is private to controller.cpp's anonymous namespace, so its
// three-branch min is replicated here (sign is always + in every leg below,
// sag > 0 by construction). blend/fwd_gate are the rung-C2c MANEUVER-limb +
// forward-hemisphere gates, passed in (recomputed by the caller from
// telemetry's err and a recomputed target_body -- see holdline_target_body).
double holdline_floor_oracle(double sag, double blend, double fwd_gate,
                             const sim::SimState& s,
                             const control::Extracted& e,
                             const sim::AircraftParams& ap,
                             const control::ControllerParams& cp) {
    const double alt = sim::altitude(s.position, ap);
    const double aB_pitch =
        cp.k_b *
        sim::ang_accel_max_derived(ap.c_pitch, ap.I_pitch, e.speed, alt, ap);
    const double v_clamp = std::max(e.speed, cp.v_min);
    const double w_max_pitch = (cp.n_max - e.cos_phi_theta) * ap.g / v_clamp;
    return blend * fwd_gate * cp.pull_floor *
           std::min({cp.K_theta * sag, std::sqrt(2.0 * aB_pitch * sag),
                     w_max_pitch});
}

// target_body recomputed exactly as controller.cpp does (sim::body_dir_of,
// the aim normalized) -- an oracle-side mirror, never the private internal.
glm::dvec3 holdline_target_body(const sim::SimState& s, const glm::dvec3& aim) {
    return sim::body_dir_of(s.orientation, glm::normalize(aim));
}

// blend recomputed exactly as controller.cpp's regime smoothstep (blend_lo,
// blend_hi, err) -- err from telemetry, config-relative bounds.
double holdline_blend(double err, const control::ControllerParams& cp) {
    const double t =
        std::clamp((err - cp.blend_lo) / (cp.blend_hi - cp.blend_lo), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

// Config-recomputed oracle for w_push (H1: seek_law's exact three-branch
// shape, private to controller.cpp's anonymous namespace, replicated here --
// the rung-C2d bound the mechanism applies to the servo). elev = demand.x
// (rotation_demand_body's pitch component, public in controller.h) -- the
// RAW value; the elev-sign latch is not modeled here (none of the S-holdline
// fixtures reach cp.astern_on).
double holdline_w_push_oracle(const sim::SimState& s, const glm::dvec3& aim,
                              const control::Extracted& e,
                              const sim::AircraftParams& ap,
                              const control::ControllerParams& cp) {
    const glm::dvec3 demand = control::rotation_demand_body(s.orientation, aim);
    const double elev = demand.x;
    const double alt = sim::altitude(s.position, ap);
    const double aB_pitch =
        cp.k_b *
        sim::ang_accel_max_derived(ap.c_pitch, ap.I_pitch, e.speed, alt, ap);
    const double v_clamp = std::max(e.speed, cp.v_min);
    const double w_max_pitch = (cp.n_max - e.cos_phi_theta) * ap.g / v_clamp;
    const double a = std::abs(elev);
    const double seek =
        std::max(cp.pursuit_step, cp.K_theta * a * (1.0 + cp.pursuit_expo * a));
    const double m =
        std::min({seek, std::sqrt(2.0 * aB_pitch * a), w_max_pitch});
    return elev >= 0.0 ? m : -m;
}

}  // namespace

// TC1: the servo fires on sag -- a banked-lateral MANEUVER with the nose
// pitched well below the horizon (sag = sin(25 deg) ~ 0.42, deep). Mutation
// this kills: the servo deleted / the sag>0 gate wired to always false.
TEST_CASE("S-holdline: the sag servo fires deep below the aim's line") {
    REQUIRE(kCp.pull_floor > 0.0);  // premise: the shipped dial is live
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    const control::Output on = holdline_step(25.0, 70.0, kCp);
    control::ControllerParams cp_off = kCp;
    cp_off.pull_floor = 0.0;  // the structural OFF arm
    const control::Output off = holdline_step(25.0, 70.0, cp_off);

    // Premises named in the spec: full MANEUVER blend (err > blend_hi ->
    // blend == 1 exactly) and forward-hemisphere fwd_gate == 1 (target_body.z
    // < 0.3 -- err ~85 deg here is mostly LATERAL after the 70-deg bank, the
    // rung's primary use case), and NOT push_mode (aim_elev == 0 fails push's
    // horizon-enter gate by construction).
    REQUIRE(on.telem.regime == control::Regime::MANEUVER);
    REQUIRE(on.telem.e > kCp.blend_hi);  // blend saturated to 1
    REQUIRE_FALSE(on.telem.push_mode);
    const sim::SimState fixture = holdline_fixture(25.0, 70.0, up, heading);
    const glm::dvec3 tb = holdline_target_body(fixture, heading);
    REQUIRE(tb.z < 0.3);  // fwd_gate == 1 premise

    // The floor-off arm reproduces the attribution's measured near-0 dead
    // elevator (align ~ 0 at this bank, no servo to raise it). The real
    // OFF-arm proof is THIS pull_floor = 0 twin, on a real cascade path
    // (folded in from the deleted TC3, P3-1: a locally-constructed ternary
    // never calls control::step and is test theater).
    const double sag = std::sin(rad(25.0));
    const double blend = holdline_blend(on.telem.e, kCp);
    const double fwd_gate = 1.0;  // pinned exactly by the tb.z < 0.3 REQUIRE
    const double oracle_unbounded = holdline_floor_oracle(
        sag, blend, fwd_gate, fixture, on.telem.extracted, kAp, kCp);
    // Rung C2d bound (director ruling, the jink trace): the servo may never
    // exceed w_push, the same-tick fully-aligned budget/brake-composed pull
    // (seek_law, not the servo's own sqrt_law) -- min(unbounded, w_push).
    // MEASURED here: the bound DOES bind on TC1 (reported per the spec, not
    // weakened) -- oracle_unbounded = 1.099422, w_push = 0.691351, bounded
    // oracle = 0.691351, emitted omega_des.x = 0.688940 (>= 0.9*bounded).
    // w_push is the binding branch: this fixture's bank (70 deg, align ~
    // 0.0016) leaves the legacy align-faded pitch far below w_push, so the
    // bound (not the raw sqrt_law servo) sets the ceiling here.
    const double w_push_oracle =
        holdline_w_push_oracle(fixture, heading, on.telem.extracted, kAp, kCp);
    const double oracle = std::min(oracle_unbounded, w_push_oracle);
    REQUIRE(oracle > 0.0);  // premise: the servo demand is a real number here
    CHECK(on.telem.omega_des.x >= 0.9 * oracle);
    // P3-2: the upper bound -- the servo may never EXCEED its own law (the
    // C2d min composed above), independent of the golden. A servo that
    // ignored the w_push cap here would land near oracle_unbounded (1.099)
    // instead, well past 1.01*oracle.
    CHECK(on.telem.omega_des.x <= 1.01 * oracle);
    CHECK(std::abs(off.telem.omega_des.x) < 0.2 * on.telem.omega_des.x);
}

// TC2: no-sag bit-identity -- the SAME banked-lateral shape, but the nose
// pitched ABOVE the horizon (sag < 0): the servo must be COMPLETELY inert,
// bit-identical to the floor-off arm on every axis. This pins the S7-turn2
// no-climb-roll-in preservation (bank_align_power = 6 untouched). Mutation
// this kills: the sag>0 gate dropped (servo applied unconditionally).
TEST_CASE(
    "S-holdline: no sag -> bit-identical to the floor-off twin, all axes") {
    const control::Output on = holdline_step(-10.0, 70.0, kCp);
    control::ControllerParams cp_off = kCp;
    cp_off.pull_floor = 0.0;
    const control::Output off = holdline_step(-10.0, 70.0, cp_off);

    REQUIRE(on.telem.regime == control::Regime::MANEUVER);
    REQUIRE(on.telem.e > kCp.blend_hi);
    REQUIRE_FALSE(on.telem.push_mode);

    CHECK(on.telem.omega_des.x == off.telem.omega_des.x);
    CHECK(on.telem.omega_des.y == off.telem.omega_des.y);
    CHECK(on.telem.omega_des.z == off.telem.omega_des.z);
}

// TC4: proportionality -- sag driven through {0.5, 2, 8, 20} deg. Fixture:
// nose FIXED at pitch_down_deg=20/bank=70 (holdline_fixture, err's dominant
// term is the sweep-INDEPENDENT azimuth below, not the pitch -- unlike
// holdline_step's aim==heading shape, where sag and err are the SAME angle
// and a 0.5 deg sag would sit in FINE, not MANEUVER); the aim carries a
// FIXED 30 deg azimuthal offset off the original heading (independent of
// the elevation sweep) so err stays safely > blend_hi at every sample,
// including the near-zero sag ones, and only the aim's ELEVATION moves to
// hit each target sag exactly (aim_elev = nose_elev + sag, nose_elev fixed
// and bank-invariant). The emitted pitch must be monotone nondecreasing in
// sag AND the small-sag point must sit well under the deep-sag point (no
// bang-bang saturation across the sweep). Mutation this kills:
// sqrt_law(sag,...) replaced by a constant full-scale demand (the
// monotone-but-flat / no-separation failure).
TEST_CASE("S-holdline: the sag servo is proportional, not bang-bang") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    const double pd_fixed = 20.0;
    const double nose_elev0 =
        -std::sin(rad(pd_fixed));  // exact, bank-invariant
    const std::vector<double> sag_degs = {0.5, 2.0, 8.0, 20.0};

    std::vector<double> pitch_x;
    for (double d : sag_degs) {
        const double sag = std::sin(rad(d));
        const double aim_elev = std::clamp(sag + nose_elev0, -1.0, 1.0);
        const double elev_rad = std::asin(aim_elev);
        const glm::dvec3 h_az =
            glm::normalize(glm::angleAxis(rad(30.0), up) * heading);
        const glm::dvec3 aim =
            glm::normalize(std::cos(elev_rad) * h_az + std::sin(elev_rad) * up);
        const control::Output on = holdline_step_aim(pd_fixed, 70.0, aim, kCp);
        REQUIRE(on.telem.regime == control::Regime::MANEUVER);
        REQUIRE(on.telem.e > kCp.blend_hi);  // blend == 1 throughout
        REQUIRE_FALSE(on.telem.push_mode);
        // fwd_gate == 1 throughout too (else the sweep would be confounded
        // by the fade, not just sag): the fixed 30-deg-azimuth/70-deg-bank
        // geometry stays comfortably lateral across the small elevation
        // sweep.
        const sim::SimState fixture =
            holdline_fixture(pd_fixed, 70.0, up, heading);
        REQUIRE(holdline_target_body(fixture, aim).z < 0.3);
        pitch_x.push_back(on.telem.omega_des.x);
    }
    for (size_t i = 1; i < pitch_x.size(); ++i)
        CHECK(pitch_x[i] >= pitch_x[i - 1] - 1e-9);
    // No bang-bang: the smallest sag sits well under the deepest one.
    CHECK(pitch_x.front() < 0.5 * pitch_x.back());
}

// TC5: near-astern SIGN safety, end-to-end (P2-1 red-team fold, fresh-
// context, f523177e9: the ORIGINAL banner here was FALSE -- a
// `fwd_gate := 1.0` mutant still passes all 370 tests incl. the golden,
// because at this geometry w_push itself is NEGATIVE (the elev-sign latch's
// deliberate down pull-through), so the C2d bound `min(servo, w_push)`
// independently caps the servo non-positive, and `omega_des.x < 0.0` is
// satisfied by the curvature feedforward alone regardless of fwd_gate. The
// astern SIGN safety is owned by the C2d min, NOT fwd_gate (fwd_gate's own
// coverage is TC7 below). TC5 still pins the end-to-end sign-survival
// property, but with a TIGHT bound instead of the loose `< 0.0`: emitted
// omega_des.x must equal the pull_floor = 0 twin's, within fp -- the servo
// (whichever gate structurally disarms it here) must be COMPLETELY inert,
// not merely sign-safe. Fixture unchanged: the existing near-astern
// elev-sign-latch test's exact geometry (aim ~165 deg behind the nose,
// target_body = normalize(0, 0.2588, 0.9659), target_body.z ~ 0.97).
TEST_CASE(
    "S-holdline: near-astern, the elev-sign latch's sign survives the servo") {
    REQUIRE(kCp.pull_floor > 0.0);  // premise: the shipped dial is live
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    const sim::SimState s0 =
        harness::level_state(kAp, 140.0, 3000.0, up, heading);
    const glm::dvec3 tb = glm::normalize(glm::dvec3{0.0, 0.2588, 0.9659});
    REQUIRE(tb.z > 0.7);  // premise: fwd_gate == 0 territory
    control::Input in;
    in.target_dir_world = s0.orientation * tb;
    in.throttle = 0.7;
    control::Internal internal = control::reset();
    internal.elev_latch = -1.0;  // forces the down pull-through
    const control::Output on =
        control::step(s0, in, internal, kAp, kCp, kAp.sim_dt);
    control::ControllerParams cp_off = kCp;
    cp_off.pull_floor = 0.0;
    const control::Output off =
        control::step(s0, in, internal, kAp, cp_off, kAp.sim_dt);
    REQUIRE(on.telem.e > rad(160.0));  // premise: really is the latch band
    CHECK(on.telem.omega_des.x < 0.0);
    CHECK(on.telem.omega_des.x == off.telem.omega_des.x);
}

// TC7: the real fwd_gate discriminator (P2-1 fold) -- a near-astern-WINDOW
// fixture with POSITIVE w_push, unlike TC5 (whose w_push happens to be
// negative and so cannot separate fwd_gate from the C2d bound). Built by
// hand rather than reusing TC5's geometry: elev_latch forces demand.x > 0
// (the up pull-through) at an aim placed ~150 deg off the nose with a small
// lateral component (misaligned bank, align ~ 0) so target_body.z lands
// inside the fwd_gate fade-to-zero zone (0.7, 1.0) while sag is deep and
// blend is saturated. If fwd_gate did nothing (mutant: fwd_gate := 1.0),
// the servo would raise pitch toward min(sqrt_law(sag,...), w_push) > the
// legacy align-faded pitch, breaking the equality below.
TEST_CASE(
    "S-holdline: fwd_gate the real discriminator -- positive w_push, "
    "near-astern window") {
    REQUIRE(kCp.pull_floor > 0.0);  // premise: the shipped dial is live
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    const sim::SimState s0 =
        harness::level_state(kAp, 140.0, 3000.0, up, heading);
    // 150 deg off the nose (nose = -Z; target_body.z = -cos(off-nose angle),
    // same convention TC1/TC6 use -- z = -cos(150 deg) = 0.866, inside
    // (0.7,1.0)). BOTH x and y lateral components nonzero: demand.x (elev,
    // feeding w_push) is driven by target_body.Y ONLY (rotation_demand_body
    // = (angle/s)*cross(nose,t) = (angle/s)*(ty,-tx,0) for nose=(0,0,-1) --
    // a y=0 fixture (TC5's shape) gives demand.x == 0 identically, which is
    // exactly why TC5 cannot be reused here). x != y != 0 also keeps
    // bank_error = atan2(x,y) off 0 (align ~ 0.06 at 45 deg), so the align
    // gate is not what is protecting the sign here either.
    const double ang = rad(150.0);
    const glm::dvec3 tb =
        glm::normalize(glm::dvec3{0.15, 0.15, -std::cos(ang)});
    control::Input in;
    in.target_dir_world = s0.orientation * tb;
    in.throttle = 0.7;
    control::Internal internal = control::reset();
    internal.elev_latch = +1.0;  // forces the UP pull-through (demand.x > 0)
    const control::Output on =
        control::step(s0, in, internal, kAp, kCp, kAp.sim_dt);
    control::ControllerParams cp_off = kCp;
    cp_off.pull_floor = 0.0;
    const control::Output off =
        control::step(s0, in, internal, kAp, cp_off, kAp.sim_dt);

    // Premises (spec-required, REQUIREd explicitly):
    REQUIRE(on.telem.regime == control::Regime::MANEUVER);
    REQUIRE(on.telem.e > kCp.blend_hi);  // blend == 1
    REQUIRE_FALSE(on.telem.push_mode);
    const glm::dvec3 body_tb =
        sim::body_dir_of(s0.orientation, glm::normalize(in.target_dir_world));
    REQUIRE(body_tb.z > 0.7);  // fwd_gate fade-to-zero window premise
    REQUIRE(body_tb.z < 1.0);
    const glm::dvec3 e_nose = s0.orientation * glm::dvec3{0.0, 0.0, -1.0};
    const glm::dvec3 e_up = sim::local_up(s0.position);
    const double aim_elev = glm::dot(glm::normalize(in.target_dir_world), e_up);
    const double sag = aim_elev - glm::dot(e_nose, e_up);
    REQUIRE(sag > 0.0);  // premise: sag really is positive here
    // w_push oracle, LATCH-AWARE (unlike holdline_w_push_oracle, which
    // assumes no S-holdline fixture reaches cp.astern_on -- TC7 deliberately
    // does): elev = elev_latch * |demand.x| (mirrors controller.cpp's own
    // latch read exactly), then the same seek_law replica.
    const glm::dvec3 demand =
        control::rotation_demand_body(s0.orientation, in.target_dir_world);
    const double elev = internal.elev_latch * std::abs(demand.x);
    const double alt = sim::altitude(s0.position, kAp);
    const double aB_pitch = kCp.k_b * sim::ang_accel_max_derived(
                                          kAp.c_pitch, kAp.I_pitch,
                                          on.telem.extracted.speed, alt, kAp);
    const double v_clamp = std::max(on.telem.extracted.speed, kCp.v_min);
    const double w_max_pitch =
        (kCp.n_max - on.telem.extracted.cos_phi_theta) * kAp.g / v_clamp;
    const double a = std::abs(elev);
    const double seek = std::max(
        kCp.pursuit_step, kCp.K_theta * a * (1.0 + kCp.pursuit_expo * a));
    const double m =
        std::min({seek, std::sqrt(2.0 * aB_pitch * a), w_max_pitch});
    const double w_push_oracle = elev >= 0.0 ? m : -m;
    REQUIRE(w_push_oracle > 0.0);  // premise: THE discriminator vs TC5 --
                                   // w_push is POSITIVE here (elev_latch=+1
                                   // agrees with the raw up demand), so the
                                   // C2d bound cannot independently zero the
                                   // servo the way it does in TC5.

    // fwd_gate == 0 here (by construction, target_body.z > 0.7): the servo
    // is fully off, so shipped == floor-off, bit-equal.
    CHECK(on.telem.omega_des.x == off.telem.omega_des.x);
}

// TC6: boundary -- fwd_gate == 1 EXACTLY at target_body.z ~ 0.5 (rung D4
// re-derivation, Chad's 2026-07-23 dive report): moved off the OLD
// discriminating window (z ~ 0.174, inside the old (0.3, 0.7) fade) because
// the shipped edges moved to (0.85, 0.95) -- z ~ 0.174 no longer
// discriminates anything (both the shipped D4 edges and the pre-D4 (0.3,0.7)
// edges give fwd_gate == 1 there now; a regression to the old edges would
// pass this test silently). The NEW discriminating window is z inside
// (0.3, 0.85) MINUS the old fade's own interior -- z = 0.5 works cleanly:
// shipped (0.85,0.95) clamps smoothstep to 0 (z < lo) -> fwd_gate == 1
// EXACTLY, while the pre-D4 (0.3,0.7) edges would land smoothstep(0.3,0.7,
// 0.5) at the BAND MIDPOINT (t=0.5, s=0.5) -> fwd_gate == 0.5, a clean 2x
// separation. Fixture: pitch_down_deg = 120 (z = -cos(120 deg) = 0.5
// exactly) at the same bank = 70 (z is bank-INVARIANT -- roll about the
// nose axis only mixes target_body.x/y, never z, so pitch_down_deg alone
// sets z regardless of bank). The emitted floor must equal the FULL
// un-faded servo demand (blend * 1.0 * pull_floor * sqrt_law(...)).
// Mutation this kills: the fade edges reverted to the pre-D4 (0.3, 0.7) --
// would fade this exact case's fwd_gate to 0.5 instead of 1.0. VERIFIED by
// hand-applying that exact mutant (git checkout -- + touch + rm exe +
// confirm relink discipline) and confirming it fails, then reverting.
TEST_CASE(
    "S-holdline: fwd_gate == 1 exactly inside the discriminating window") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    const control::Output on = holdline_step(120.0, 70.0, kCp);
    REQUIRE(on.telem.regime == control::Regime::MANEUVER);
    REQUIRE(on.telem.e > kCp.blend_hi);
    REQUIRE_FALSE(on.telem.push_mode);
    const sim::SimState fixture = holdline_fixture(120.0, 70.0, up, heading);
    const glm::dvec3 tb = holdline_target_body(fixture, heading);
    REQUIRE(tb.z == Catch::Approx(0.5).margin(1e-9));  // the discriminating
                                                       // window this test
                                                       // targets, exactly
    REQUIRE(tb.z < 0.85);  // premise: inside the shipped D4 all-servo zone

    const double sag = std::sin(rad(120.0));
    const double blend = holdline_blend(on.telem.e, kCp);
    const double oracle_unfaded = holdline_floor_oracle(
        sag, blend, /*fwd_gate=*/1.0, fixture, on.telem.extracted, kAp, kCp);
    // Rung C2d bound applies here too (same min(unfaded, w_push) the
    // mechanism composes) -- bound it the same way so this test targets
    // ONLY the fwd_gate edge, not the separate C2d cap.
    const double w_push_oracle =
        holdline_w_push_oracle(fixture, heading, on.telem.extracted, kAp, kCp);
    const double oracle_full = std::min(oracle_unfaded, w_push_oracle);
    // The emitted pitch is AT LEAST the full un-faded oracle (pitch = max
    // with the legacy align term, so >= is the honest bound; a faded mutant
    // would land strictly BELOW oracle_full instead).
    CHECK(on.telem.omega_des.x >= 0.99 * oracle_full);
}

// ---------------------------------------------------------------------------
// Blend-band instrument (jitter_attribution §6.5 pin #2): telem.blend and
// telem.held_bank are honest MIRRORS. The blend mirror is WELDED to the
// config-side holdline_blend oracle (H1: the oracle recomputes from config,
// never reads the mechanism's own mirror — a shape change in either trips
// loud), sampled below / inside / above the band so a regime-bool or
// hardcoded mirror separates at some sample. held_bank must equal the
// RETURNED internal's post-capture/lean value the same tick.
// Mutations this kills (each verified): telem.blend wired to 0.0 (fails the
// 6/12-deg samples); telem.blend wired to (regime==MANEUVER ? 1 : 0) (fails
// the 6-deg mid-band sample, 0.0625 expected); telem.held_bank left default
// while internal leans (fails the FINE sample after 120 ticks).
// ---------------------------------------------------------------------------
TEST_CASE("instrument: telem.blend/held_bank mirror the cascade exactly") {
    const glm::dvec3 up{1.0, 0.0, 0.0};
    const glm::dvec3 heading{0.0, 0.0, -1.0};
    const sim::SimState s0 =
        harness::level_state(kAp, 140.0, 2000.0, up, heading);
    const glm::dvec3 nose = s0.orientation * glm::dvec3{0.0, 0.0, -1.0};
    const glm::dvec3 up_b = s0.orientation * glm::dvec3{0.0, 1.0, 0.0};

    for (const double d : {3.0, 6.0, 12.0}) {
        control::Input in;
        in.target_dir_world = glm::angleAxis(-rad(d), up_b) * nose;
        in.throttle = 0.7;
        const control::Output o =
            control::step(s0, in, control::reset(), kAp, kCp, kAp.sim_dt);
        // Weld: mirror == oracle, exact (identical expression trees).
        CHECK(o.telem.blend == holdline_blend(o.telem.e, kCp));
        // Sanity that the samples straddle the band (non-vacuous placement).
        if (d == 3.0) CHECK(o.telem.blend == 0.0);
        if (d == 6.0) CHECK((o.telem.blend > 0.0 && o.telem.blend < 1.0));
        if (d == 12.0) CHECK(o.telem.blend == 1.0);
        // held_bank mirror == the returned internal, same tick.
        CHECK(o.telem.held_bank == o.internal.held_bank);
    }

    // FINE multi-tick: the lean update moves held_bank off zero; the mirror
    // must track the MOVED value (a default-0 mirror passes the one-tick
    // samples above where held_bank is still ~0 after one lean step from 0).
    control::Input in;
    in.target_dir_world = glm::angleAxis(-rad(3.0), up_b) * nose;
    in.throttle = 0.7;
    control::Internal internal = control::reset();
    control::Output o;
    for (int i = 0; i < 120; ++i) {
        o = control::step(s0, in, internal, kAp, kCp, kAp.sim_dt);
        internal = o.internal;
    }
    REQUIRE(std::abs(o.internal.held_bank) > 1e-3);  // the lean really moved
    CHECK(o.telem.held_bank == o.internal.held_bank);
}

// ===========================================================================
// Blend-band roll TARGET continuity (roll_target_mix — the 5-10 deg roll
// slam; plan: coverage-completion of MB-lean). Oracles are config-recomputed
// (H1): sqrt_law's three-branch min + the smoothstep replicated as the
// S-holdline oracles above do; never the mechanism's own internals.
// ===========================================================================
namespace {

double rtm_sqrt_law(double e, double K, double a_brake, double w_max) {
    const double m = std::min(
        {K * std::abs(e), std::sqrt(2.0 * a_brake * std::abs(e)), w_max});
    return e >= 0.0 ? m : -m;
}
double rtm_smoothstep(double lo, double hi, double x) {
    const double t = std::clamp((x - lo) / (hi - lo), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}
double rtm_aB_roll(const sim::SimState& s, const control::Extracted& e) {
    return kCp.k_b * sim::ang_accel_max_derived(kAp.c_roll, kAp.I_roll, e.speed,
                                                sim::altitude(s.position, kAp),
                                                kAp);
}
// The config-side be_used mix (the mechanism's exact algebra, recomputed
// from telemetry + config): be + mix*gate*(1-blend)*(e_lean - be).
double rtm_be_used(double be, double blend, const control::Extracted& e,
                   const glm::dvec3& tb, double mix) {
    const double phi_full = control::unfold_bank(e.phi, e.cos_phi_theta);
    const double az =
        std::atan2(tb.x * e.cos_phi_theta + tb.y * std::sin(e.phi), -tb.z);
    const double lean_t =
        std::clamp(kCp.lean_gain * az, -kCp.lean_max, kCp.lean_max);
    const double gate = rtm_smoothstep(-kCp.wings_level_band,
                                       kCp.wings_level_band, e.cos_phi_theta);
    const double w_eff = mix * gate;
    return be + w_eff * (1.0 - blend) * ((lean_t - phi_full) - be);
}

}  // namespace

TEST_CASE("roll_target_mix: on-vs-off delta equals the config oracle") {
    const glm::dvec3 up{1.0, 0.0, 0.0};
    const glm::dvec3 heading{0.0, 0.0, -1.0};
    control::ControllerParams cp_on = kCp;
    cp_on.roll_target_mix = 1.0;
    control::ControllerParams cp_off = kCp;
    cp_off.roll_target_mix = 0.0;

    auto run = [&](const sim::SimState& s, double d_deg,
                   const control::ControllerParams& cp) {
        const glm::dvec3 nose = s.orientation * glm::dvec3{0.0, 0.0, -1.0};
        control::Input in;
        // Aim rotated about LOCAL-UP (world lateral) so banked fixtures get
        // a horizontal lateral aim, the A6 geometry.
        in.target_dir_world = glm::normalize(
            glm::angleAxis(-rad(d_deg), sim::local_up(s.position)) * nose);
        in.throttle = 0.7;
        control::Internal ni = control::reset();
        ni.held_bank = rad(5.0);  // != the saturated in-band lean target
        // (clamp(lean_gain*az) == +30 deg for every in-band lateral aim at
        // gain 8), so the frozen-anchor mutant (e_lean keyed to held_bank)
        // SEPARATES from the live-lean anchor at every sample.
        return control::step(s, in, ni, kAp, cp, kAp.sim_dt);
    };

    SECTION("level state: in-band delta == oracle; below-band exact ==") {
        const sim::SimState s = harness::flight_state(kAp, 180.0, 3000.0, up,
                                                      heading, 0.0, 0.0, 0.0);
        {
            const control::Output a = run(s, 3.0, cp_on);
            const control::Output b = run(s, 3.0, cp_off);
            CHECK(a.telem.omega_des == b.telem.omega_des);
            CHECK(a.inputs.roll == b.inputs.roll);
        }
        // Red-team P1-1: at the LEVEL fixture only the low-band samples are
        // non-vacuous — above ~6.6 deg both be (~90 deg) and be_used exceed
        // sqrt_law's p_max/K_phi saturation knee, so delta == oracle == 0
        // pins nothing (a `blend < 0.9` snap-point mutant sailed through).
        // Level keeps the LIVE low-band samples with a non-vacuity REQUIRE;
        // the upper band is pinned in the BANKED section below, where be is
        // unsaturated across the whole band.
        for (const double d : {5.5, 6.0}) {
            const control::Output on = run(s, d, cp_on);
            const control::Output off = run(s, d, cp_off);
            const double blend = holdline_blend(on.telem.e, kCp);
            REQUIRE((blend > 0.0 && blend < 1.0));
            const glm::dvec3 tb = holdline_target_body(
                s, glm::normalize(
                       glm::angleAxis(-rad(d), sim::local_up(s.position)) *
                       (s.orientation * glm::dvec3{0.0, 0.0, -1.0})));
            const double be = control::bank_error(tb);
            const double be_used =
                rtm_be_used(be, blend, on.telem.extracted, tb, 1.0);
            const double aB = rtm_aB_roll(s, on.telem.extracted);
            const double delta_oracle =
                blend * (rtm_sqrt_law(be, kCp.K_phi, aB, kCp.p_max) -
                         rtm_sqrt_law(be_used, kCp.K_phi, aB, kCp.p_max));
            const double delta = on.telem.omega_des.z - off.telem.omega_des.z;
            std::printf(
                "[rtm-oracle] d=%.1f blend=%.4f be=%.3f be_used=%.3f "
                "delta=%.9f oracle=%.9f\n",
                d, blend, be, be_used, delta, delta_oracle);
            REQUIRE(delta != 0.0);  // non-vacuous (the P1-1 tripwire)
            CHECK(delta == Catch::Approx(delta_oracle).margin(1e-12));
        }
    }

    SECTION("banked: the UPPER band is pinned where sqrt_law is unsaturated") {
        // Red-team P1-1 fix: banked 70 deg toward the aim, the lateral
        // target sits mostly along body-up, so |bank_error| ~ 20 deg — well
        // under the p_max/K_phi knee — and the delta oracle is live at EVERY
        // in-band sample. This is the leg that kills a mid-band snap-point
        // mutant (`blend < 1.0` -> `blend < 0.9`: a hard discontinuity at
        // err ~ 8.6 deg — verified failing here, was golden-only before).
        const sim::SimState s = harness::flight_state(
            kAp, 180.0, 3000.0, up, heading, rad(70.0), 0.0, 0.0);
        for (const double d : {5.5, 6.5, 7.5, 8.5, 8.9}) {
            const control::Output on = run(s, d, cp_on);
            const control::Output off = run(s, d, cp_off);
            const double blend = holdline_blend(on.telem.e, kCp);
            REQUIRE((blend > 0.0 && blend < 1.0));
            const glm::dvec3 tb = holdline_target_body(
                s, glm::normalize(
                       glm::angleAxis(-rad(d), sim::local_up(s.position)) *
                       (s.orientation * glm::dvec3{0.0, 0.0, -1.0})));
            const double be = control::bank_error(tb);
            REQUIRE(std::abs(be) < kCp.p_max / kCp.K_phi);  // premise:
                                                            // unsaturated
            const double be_used =
                rtm_be_used(be, blend, on.telem.extracted, tb, 1.0);
            const double aB = rtm_aB_roll(s, on.telem.extracted);
            const double delta_oracle =
                blend * (rtm_sqrt_law(be, kCp.K_phi, aB, kCp.p_max) -
                         rtm_sqrt_law(be_used, kCp.K_phi, aB, kCp.p_max));
            const double delta = on.telem.omega_des.z - off.telem.omega_des.z;
            std::printf(
                "[rtm-banked] d=%.1f blend=%.4f be=%.3f be_used=%.3f "
                "delta=%.9f oracle=%.9f\n",
                d, blend, be, be_used, delta, delta_oracle);
            REQUIRE(delta != 0.0);  // non-vacuous at every sample
            CHECK(delta == Catch::Approx(delta_oracle).margin(1e-12));
        }
    }

    SECTION("knife-edge fade band: w_eff carries wings_level_gate") {
        // The gate-riding pin lives at the KNIFE-EDGE, not full inversion:
        // fully inverted the MB-right righting machinery owns the roll, so
        // an inverted on-vs-off sample is VACUOUS for this factor (a
        // wings_level_gate-dropped mutant survived it — the S7-cam P2
        // "fixture makes the new behavior a no-op" class, found by running
        // the mutant). At bank ~92 deg cos_phi_theta sits strictly INSIDE
        // the fade band, so the honest w_eff = mix * gate is strictly
        // between 0 and mix and the delta oracle (which recomputes the
        // gate) separates a mutant whose effective gate is 1.
        const sim::SimState s = harness::flight_state(
            kAp, 180.0, 3000.0, up, heading, rad(92.0), 0.0, 0.0);
        const control::Output on = run(s, 7.0, cp_on);
        const control::Output off = run(s, 7.0, cp_off);
        const double cpt = on.telem.extracted.cos_phi_theta;
        REQUIRE((cpt > -kCp.wings_level_band && cpt < kCp.wings_level_band));
        const double gate =
            rtm_smoothstep(-kCp.wings_level_band, kCp.wings_level_band, cpt);
        REQUIRE((gate > 0.0 && gate < 1.0));  // premise: strictly in-band
        REQUIRE_FALSE(on.telem.righting);
        const double blend = holdline_blend(on.telem.e, kCp);
        REQUIRE((blend > 0.0 && blend < 1.0));
        const glm::dvec3 tb = holdline_target_body(
            s, glm::normalize(
                   glm::angleAxis(-rad(7.0), sim::local_up(s.position)) *
                   (s.orientation * glm::dvec3{0.0, 0.0, -1.0})));
        const double be = control::bank_error(tb);
        const double be_used =
            rtm_be_used(be, blend, on.telem.extracted, tb, 1.0);
        const double aB = rtm_aB_roll(s, on.telem.extracted);
        const double delta_oracle =
            blend * (rtm_sqrt_law(be, kCp.K_phi, aB, kCp.p_max) -
                     rtm_sqrt_law(be_used, kCp.K_phi, aB, kCp.p_max));
        const double delta = on.telem.omega_des.z - off.telem.omega_des.z;
        std::printf("[rtm-gate] cpt=%.4f gate=%.4f delta=%.9f oracle=%.9f\n",
                    cpt, gate, delta, delta_oracle);
        CHECK(delta == Catch::Approx(delta_oracle).margin(1e-12));
        REQUIRE(delta != 0.0);  // non-vacuous: the mechanism bites here
    }
}

TEST_CASE("roll_target_mix: blend == 1 is bit-untouched (flicks keep v10)") {
    const glm::dvec3 up{1.0, 0.0, 0.0};
    const glm::dvec3 heading{0.0, 0.0, -1.0};
    control::ControllerParams cp_on = kCp;
    cp_on.roll_target_mix = 1.0;
    control::ControllerParams cp_off = kCp;
    cp_off.roll_target_mix = 0.0;
    for (const double bank : {0.0, 180.0}) {
        const sim::SimState s = harness::flight_state(
            kAp, 180.0, 3000.0, up, heading, rad(bank), 0.0, 0.0);
        const glm::dvec3 nose = s.orientation * glm::dvec3{0.0, 0.0, -1.0};
        for (const double d : {10.0, 25.0, 100.0}) {
            control::Input in;
            in.target_dir_world = glm::normalize(
                glm::angleAxis(-rad(d), sim::local_up(s.position)) * nose);
            in.throttle = 0.7;
            control::Internal ni = control::reset();
            ni.held_bank = rad(30.0);
            const control::Output on =
                control::step(s, in, ni, kAp, cp_on, kAp.sim_dt);
            const control::Output off =
                control::step(s, in, ni, kAp, cp_off, kAp.sim_dt);
            REQUIRE(on.telem.blend == 1.0);  // premise: at/above blend_hi
            CHECK(on.telem.omega_des == off.telem.omega_des);
            CHECK(on.inputs.pitch == off.inputs.pitch);
            CHECK(on.inputs.yaw == off.inputs.yaw);
            CHECK(on.inputs.roll == off.inputs.roll);
        }
    }
}

