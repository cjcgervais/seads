// BFM rung R1 (docs/bandit_bfm_r1_spec.md): the dwell-latched dogfight state
// machine (drone/bfm.h) and its wiring into drone::tick.
//
// DISCIPLINE, from the S6 lesson: transition predicates are driven on FIXED
// SYNTHETIC STATES (open loop), never inferred from a closed-loop chase — a
// closed loop moves every input at once, so a passing chase proves nothing
// about WHICH predicate fired. The one closed-loop leg (the yo-yo energy trade)
// first REQUIREs that the mode was actually entered, so it can never pass
// vacuously.
//
// TEST_CASE names are STRICTLY ASCII: a non-ASCII name silently never runs
// under ctest (this repo has been bitten multiple times).

#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "config/load_aircraft.h"
#include "config/load_scenario.h"
#include "drone/bfm.h"
#include "drone/drone.h"
#include "drone/maverick.h"
#include "sim/aero.h"
#include "sim/world.h"

#ifdef NDEBUG
#error "SEADS gate requires an assert-live build (SPEC 6.1)"
#endif

namespace {

const sim::AircraftParams kAp =
    cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");

constexpr double kPi = 3.14159265358979323846;

glm::dvec3 nose_of(const sim::SimState& s) {
    return s.orientation * glm::dvec3{0.0, 0.0, -1.0};
}
glm::dvec3 right_of(const sim::SimState& s) {
    return s.orientation * glm::dvec3{1.0, 0.0, 0.0};
}

bool finite3(const glm::dvec3& v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

// The BfmDials aggregate this tree's drone::tick builds each engaged tick
// (drone/drone.h): pursue_k_az/k_el/max_bank/max_gamma/lead_speed/lead_max_s/
// speed/pursue_speed_bump, straight off DroneParams. Kept as a helper so every
// test builds it the SAME way the shipped wiring does.
bfm::BfmDials dials_of(const drone::DroneParams& dp) {
    return bfm::BfmDials{dp.pursue_k_az,
                         dp.pursue_k_el,
                         dp.pursue_max_bank,
                         dp.pursue_max_gamma,
                         dp.pursue_lead_speed,
                         dp.pursue_lead_max_s,
                         dp.speed,
                         dp.pursue_speed_bump};
}

// A wildly-detuned dial set: every BFM knob moved far off its default, so the
// off-arm differential proves the DIALS are dead, not merely that two identical
// structs behave identically.
bfm::BfmParams detuned() {
    bfm::BfmParams b;
    b.enabled = false;  // the whole point
    b.attack_range_m = 9000.0;
    b.attack_release_frac = 3.0;
    b.lag_dist_m = 2000.0;
    b.lag_off_lo = 1.0 * kPi / 180.0;
    b.lag_off_hi = 5.0 * kPi / 180.0;
    b.yoyo_closure_mps = 1.0;
    b.yoyo_angle = 1.0 * kPi / 180.0;
    b.yoyo_arm_s = 0.01;
    b.yoyo_gamma = 70.0 * kPi / 180.0;
    b.yoyo_bank_frac = 0.05;
    b.yoyo_time_s = 30.0;
    b.yoyo_exit_closure_mps = -500.0;
    b.extend_energy_m = 1.0;
    b.extend_gamma = -40.0 * kPi / 180.0;
    b.extend_min_s = 0.1;
    b.extend_max_s = 60.0;
    b.reenter_energy_m = 9999.0;
    b.frustration_s = 0.1;
    b.intercept_speed_bump = 400.0;
    b.min_dwell_s = 0.01;
    b.yoyo_cooldown_s = 50.0;  // FIX-D
    b.climb_abort_t = 0.98;    // FIX-A (arm side is structural — no dial)
    return b;
}

// FIX-8: a small helper so the yoyo energy-trade leg and its control arm
// (identical setup, yoyo_gamma zeroed) run the EXACT same geometry/loop.
struct YoyoTradeResult {
    bool entered = false;
    double alt_in = 0.0, alt_out = 0.0, v_in = 0.0, v_out = 0.0;
    double max_aoa = 0.0;
    long long yoyo_ticks = 0;
    glm::dvec3 final_pos{0.0};
};

YoyoTradeResult run_yoyo_trade(const drone::DroneParams& dp) {
    drone::DroneState d = drone::spawn_drone(kAp, dp, 0, 1);
    d.engaged = true;
    // Fast bandit (excess closure is the whole premise of a yo-yo).
    d.curr.velocity = glm::normalize(d.curr.velocity) * 200.0;
    // Start the fight already joined so the closed loop is short and
    // readable; the Intercept->Offensive handover is pinned open-loop above.
    d.bfm.mode = bfm::BfmState::Mode::Offensive;

    const glm::dvec3 dir = std::cos(55.0 * kPi / 180.0) * nose_of(d.curr) +
                           std::sin(55.0 * kPi / 180.0) * right_of(d.curr);
    sim::SimState player;
    player.position = d.curr.position + dir * 600.0;
    player.orientation = d.curr.orientation;
    player.velocity = glm::cross(sim::local_up(player.position), dir) * 100.0;

    YoyoTradeResult r;
    for (int t = 0; t < 12 * 120; ++t) {
        drone::tick(d, kAp, dp, nullptr, &player);
        player.position += player.velocity * kAp.sim_dt;
        const glm::dvec3 vb = sim::body_dir_of(d.curr.orientation,
                                               sim::current_vhat(d.curr, kAp));
        r.max_aoa = std::max(r.max_aoa, std::abs(sim::alpha_of(vb)));
        const bool in_yoyo = d.bfm.mode == bfm::BfmState::Mode::Yoyo;
        if (in_yoyo && !r.entered) {
            r.entered = true;
            r.v_in = glm::length(d.curr.velocity);
            r.alt_in = sim::altitude(d.curr.position, kAp);
        }
        if (in_yoyo) {
            ++r.yoyo_ticks;
            r.v_out = glm::length(d.curr.velocity);
            r.alt_out = sim::altitude(d.curr.position, kAp);
        }
        if (r.entered && !in_yoyo) break;
    }
    r.final_pos = d.curr.position;
    return r;
}

}  // namespace

// ---------------------------------------------------------------------------
// 1. OFF-ARM DIFFERENTIAL. The default is OFF, and while it is off EVERY dial
// is dead: a bandit flown with the detuned dial set must produce a
// bit-identical trajectory and fire intent to one flown with the defaults.
// MUTATION: read any bfm dial outside the `if (dp.bfm.enabled)` branch (or
// default enabled=true)
// -> the trajectories fork within a few ticks and the bit-exact compare fails.
// ---------------------------------------------------------------------------
TEST_CASE("bfm: default is OFF and every dial is dead while disabled") {
    REQUIRE_FALSE(drone::DroneParams{}.bfm.enabled);
    REQUIRE_FALSE(bfm::BfmParams{}.enabled);

    drone::DroneParams dp_a;         // stock defaults, bfm off
    drone::DroneParams dp_b = dp_a;  // same, but every bfm dial detuned
    dp_b.bfm = detuned();
    REQUIRE_FALSE(dp_b.bfm.enabled);

    drone::DroneState a = drone::spawn_drone(kAp, dp_a, 0, 1);
    a.engaged = true;
    drone::DroneState b = a;

    // A player crossing in front of the bandit, advanced by hand each tick (the
    // drone tick never moves the player).
    sim::SimState player;
    player.position =
        a.curr.position + nose_of(a.curr) * 900.0 + right_of(a.curr) * 300.0;
    player.velocity = right_of(a.curr) * 90.0;
    player.orientation = a.curr.orientation;

    for (int t = 0; t < 600; ++t) {
        drone::tick(a, kAp, dp_a, nullptr, &player);
        drone::tick(b, kAp, dp_b, nullptr, &player);
        player.position += player.velocity * kAp.sim_dt;
        REQUIRE(a.curr.position == b.curr.position);  // bit-exact, not Approx
        REQUIRE(a.curr.velocity == b.curr.velocity);
        REQUIRE(a.curr.orientation == b.curr.orientation);
        REQUIRE(a.wants_fire == b.wants_fire);
    }
    // The machine never even ran.
    REQUIRE(b.bfm.mode == bfm::BfmState::Mode::Intercept);
    REQUIRE(b.bfm.mode_ticks == 0);
    REQUIRE(b.bfm.arm_ticks == 0);
    // Non-vacuous: the run must actually have been a flying ENGAGED pursuit
    // that MANEUVERED (a pair of parked, identical no-ops would compare equal
    // for free).
    REQUIRE(a.age_ticks == 600);
    REQUIRE(a.engaged);
    REQUIRE(glm::length(a.curr.velocity) > 1.0);
    REQUIRE(glm::length(a.curr.position - a.prev.position) > 0.0);
    REQUIRE(glm::dot(nose_of(a.curr), nose_of(a.prev)) < 1.0);  // it turned
}

// ---------------------------------------------------------------------------
// 2a. MIN-DWELL LATCH (open loop). A mode whose exit predicate holds from its
// very first tick still flies min_dwell_s before it is allowed to leave.
// MUTATION: drop the `may_exit` guard -> the exit happens on tick 1 and the
// count collapses from a real dwell to ~0.
// ---------------------------------------------------------------------------
TEST_CASE(
    "bfm: min dwell holds a mode for exactly ceil(min_dwell_s/dt) ticks") {
    drone::DroneParams dp;
    bfm::BfmParams bp;
    bp.enabled = true;
    const bfm::BfmDials dl = dials_of(dp);
    const double dt = kAp.sim_dt;

    drone::DroneState d = drone::spawn_drone(kAp, dp, 0, 1);
    // FIXED synthetic geometry: well inside attack_range from tick 0, so the
    // Intercept -> Offensive predicate is true on every single call.
    sim::SimState player;
    player.position = d.curr.position + nose_of(d.curr) * 400.0;
    player.orientation = d.curr.orientation;
    REQUIRE(glm::length(player.position - d.curr.position) < bp.attack_range_m);

    bfm::BfmState st;
    long long flown = 0;
    for (int t = 0; t < 5000; ++t) {
        bfm::bfm_step(st, d.curr, player, bp, dl, kAp, dt);
        if (st.mode != bfm::BfmState::Mode::Intercept) break;
        ++flown;
    }
    const long long expect = static_cast<long long>(
        std::ceil(bp.min_dwell_s / dt));  // config-derived
    REQUIRE(expect > 1);                  // non-vacuous: a real dwell
    REQUIRE(st.mode == bfm::BfmState::Mode::Offensive);  // it DID transition
    REQUIRE(flown - expect <= 1);
    REQUIRE(expect - flown <= 1);
}

// ---------------------------------------------------------------------------
// 2b. YO-YO ARM COUNTER (open loop). The overshoot predictor must be SUSTAINED:
// a predicate that flickers on alternate ticks never arms, while a continuous
// hold arms after exactly yoyo_arm_s. MUTATION: fire the yo-yo on the raw
// predicate (no arm counter) -> the oscillating leg enters Yoyo on tick 1.
// ---------------------------------------------------------------------------
TEST_CASE("bfm: the yoyo arm counter debounces an oscillating predictor") {
    drone::DroneParams dp;
    bfm::BfmParams bp;
    bp.enabled = true;
    const bfm::BfmDials dl = dials_of(dp);
    const double dt = kAp.sim_dt;

    drone::DroneState d = drone::spawn_drone(kAp, dp, 0, 1);
    d.curr.velocity = glm::dvec3{0.0};  // open loop: the bandit is a fixed eye
    // 60 deg off the nose, in the local-horizontal plane so the two altitudes
    // (and therefore e_delta) stay close and the Extend predicate stays quiet.
    const glm::dvec3 dir = std::cos(60.0 * kPi / 180.0) * nose_of(d.curr) +
                           std::sin(60.0 * kPi / 180.0) * right_of(d.curr);
    sim::SimState
        hot;  // closure 60 > 40, angle off 60 deg > 45 -> predicate TRUE
    hot.position = d.curr.position + dir * 500.0;
    hot.velocity = -dir * 60.0;
    hot.orientation = d.curr.orientation;
    sim::SimState cold = hot;  // same geometry, receding -> predicate FALSE
    cold.velocity = dir * 60.0;

    // Sanity on the synthetic states (non-vacuous: the predicate really does
    // differ between them).
    const bfm::BfmGeom gh = bfm::bfm_geom(d.curr, hot, kAp);
    const bfm::BfmGeom gc = bfm::bfm_geom(d.curr, cold, kAp);
    REQUIRE(gh.closure > bp.yoyo_closure_mps);
    REQUIRE(gh.angle_off > bp.yoyo_angle);
    REQUIRE(gc.closure < bp.yoyo_closure_mps);
    REQUIRE(gh.e_delta > -bp.extend_energy_m);  // Extend must stay quiet

    SECTION("oscillating every other tick never arms") {
        bfm::BfmState st;
        st.mode = bfm::BfmState::Mode::Offensive;
        for (int t = 0; t < 2000; ++t) {  // < frustration_s (20 s = 2400 ticks)
            bfm::bfm_step(st, d.curr, (t % 2 == 0) ? hot : cold, bp, dl, kAp,
                          dt);
            REQUIRE(st.mode == bfm::BfmState::Mode::Offensive);
            REQUIRE(st.arm_ticks <= 1);  // reset on every failing tick
        }
    }

    SECTION("a continuous hold arms after yoyo_arm_s") {
        bfm::BfmState st;
        st.mode = bfm::BfmState::Mode::Offensive;
        long long flown = 0;
        for (int t = 0; t < 2000; ++t) {
            bfm::bfm_step(st, d.curr, hot, bp, dl, kAp, dt);
            if (st.mode != bfm::BfmState::Mode::Offensive) break;
            ++flown;
        }
        const long long expect =
            static_cast<long long>(std::ceil(bp.yoyo_arm_s / dt));
        REQUIRE(expect > 1);
        REQUIRE(st.mode == bfm::BfmState::Mode::Yoyo);
        REQUIRE(flown - expect <= 1);
        REQUIRE(expect - flown <= 1);
        // Yo-yo entry beats the min dwell (it has its own counter) — that is
        // the documented exception, and this pins it.
        REQUIRE(expect <
                static_cast<long long>(std::ceil(bp.min_dwell_s / dt)));
    }
}

// ---------------------------------------------------------------------------
// 2c. ATTACK-RANGE HYSTERESIS (open loop). A range rippling across
// attack_range_m produces exactly ONE transition; going back to Intercept
// requires crossing attack_range_m * attack_release_frac. MUTATION LEVER: set
// attack_release_frac to 1.0 and the SAME ripple flips back — the second
// SECTION proves the ripple leg is discriminating, not merely quiet.
// ---------------------------------------------------------------------------
TEST_CASE("bfm: attack range release is hysteretic, not a single threshold") {
    drone::DroneParams dp;
    const bfm::BfmDials dl = dials_of(dp);
    const double dt = kAp.sim_dt;

    drone::DroneState d = drone::spawn_drone(kAp, dp, 0, 1);
    d.curr.velocity = glm::dvec3{0.0};
    const glm::dvec3 dir = nose_of(d.curr);

    // Ripple the range +/-60 m around attack_range_m (1200), well below the
    // 1680 m release. A player parked on the nose keeps every other predicate
    // (closure ~0, angle off ~0, e_delta ~0) far from its threshold.
    auto run = [&](double release_frac) {
        bfm::BfmParams bp;
        bp.enabled = true;
        bp.attack_release_frac = release_frac;
        bfm::BfmState st;
        int transitions = 0;
        bfm::BfmState::Mode last = st.mode;
        // 2000 ticks (16.7 s) — deliberately SHORT of frustration_s (20 s), so
        // the only transitions this leg can see are the range ones.
        for (int t = 0; t < 2000; ++t) {
            const double r = bp.attack_range_m + ((t % 2 == 0) ? -60.0 : 60.0);
            sim::SimState player;
            player.position = d.curr.position + dir * r;
            player.orientation = d.curr.orientation;
            bfm::bfm_step(st, d.curr, player, bp, dl, kAp, dt);
            if (st.mode != last) {
                ++transitions;
                last = st.mode;
            }
        }
        return transitions;
    };

    SECTION("committed table: one transition, then it stays Offensive") {
        REQUIRE(run(1.4) == 1);
    }
    SECTION("mutation lever: release_frac 1.0 makes the ripple chatter") {
        REQUIRE(run(1.0) > 1);
    }
}

// ---------------------------------------------------------------------------
// 3. YO-YO ENERGY EXCHANGE (closed loop, non-vacuous). Forced overshoot
// geometry: a fast bandit at a big angle off a crossing player. REQUIRE the
// machine actually enters Yoyo, then pin the TRADE across the dwell — speed
// down, altitude up — and pin that the soft AoA limiter still holds through it
// (the yo-yo commands a hard climb; if it could depart the aircraft the whole
// maneuver would be a suicide). MUTATION: command a level or diving gamma in
// Yoyo -> the altitude assert fails; drop aoa_protect -> the stall bound fails.
// ---------------------------------------------------------------------------
TEST_CASE("bfm: the yoyo trades speed for altitude inside the stall envelope") {
    drone::DroneParams dp;
    dp.turn_time = 0.0;
    dp.bfm.enabled = true;

    const YoyoTradeResult real = run_yoyo_trade(dp);

    // NON-VACUOUS: the maneuver must have actually happened, for a real dwell.
    REQUIRE(real.entered);
    REQUIRE(real.yoyo_ticks >=
            static_cast<long long>(std::ceil(dp.bfm.min_dwell_s / kAp.sim_dt)));
    // The trade itself.
    REQUIRE(real.alt_out > real.alt_in);
    REQUIRE(real.v_out < real.v_in);
    // The limiter holds through the climb (same 1.4x allowance as the pursuit
    // AoA test in test_drone.cpp: the overshoot past the 0.95x cap is ZOH lag).
    const double stall_a = kAp.Cl_max / kAp.Cl_alpha;
    REQUIRE(real.max_aoa < 1.4 * stall_a);
    REQUIRE(finite3(real.final_pos));

    // FIX-8 (control arm, non-discriminating test P3): identical setup, but
    // the commanded yoyo climb is ZEROED at the struct level (no loader
    // involved) -- a "yoyo" that never commands any climb is just a plain
    // unloaded turn, which should gain far less altitude than the real
    // yoyo_gamma climb. Without this arm, the original leg would pass even if
    // the Yoyo command dropped its gamma override entirely, as long as SOME
    // maneuver still gained a little altitude by chance.
    drone::DroneParams dp2 = dp;
    dp2.bfm.yoyo_gamma = 0.0;
    const YoyoTradeResult control = run_yoyo_trade(dp2);
    REQUIRE(control.entered);  // non-vacuous: the control arm also reaches Yoyo

    const double real_gain = real.alt_out - real.alt_in;
    const double control_gain = control.alt_out - control.alt_in;
    REQUIRE(real_gain > control_gain + 10.0);  // a real, non-noise margin
}

// ---------------------------------------------------------------------------
// FIX-B (P2, gamma clamp untested/inert). bfm.h's Yoyo command clamps
// bp.yoyo_gamma into [-dl.max_gamma, dl.max_gamma] at the call site (FIX-3,
// round 2) — implemented but never actually exercised at a value that would
// escape the envelope. Direct construction: yoyo_gamma way past max_gamma,
// forced Yoyo, one step -> the commanded gamma is clamped exactly to
// dl.max_gamma. MUTATION: drop the clamp -> cmd.target_gamma reads the raw
// (out-of-envelope) yoyo_gamma instead.
// ---------------------------------------------------------------------------
TEST_CASE("bfm: the yoyo gamma command clamps into the pursuit envelope") {
    drone::DroneParams dp;
    bfm::BfmParams bp;
    bp.enabled = true;
    bp.yoyo_gamma = 80.0 * kPi / 180.0;  // deliberately past max_gamma
    bfm::BfmDials dl = dials_of(dp);
    dl.max_gamma = 0.524;
    const double dt = kAp.sim_dt;
    REQUIRE(bp.yoyo_gamma > dl.max_gamma);  // non-vacuous: really escapes

    drone::DroneState d = drone::spawn_drone(kAp, dp, 0, 1);
    sim::SimState player;
    player.position = d.curr.position + nose_of(d.curr) * 500.0;
    player.orientation = d.curr.orientation;
    player.velocity = glm::dvec3{0.0};

    bfm::BfmState st;
    st.mode = bfm::BfmState::Mode::Yoyo;
    const bfm::BfmCmd cmd = bfm::bfm_step(st, d.curr, player, bp, dl, kAp, dt);
    REQUIRE(cmd.target_gamma == dl.max_gamma);
}

// FIX-B loader rejection leg lives in test_load_scenario.cpp (bfm_yoyo_gamma_
// deg = 31.0 > pursue_max_gamma_deg 30) — the existing FIX-3 check, still
// untested until this round.

// ---------------------------------------------------------------------------
// 4. LAG BLEND SHAPE. w is 0 at lag_off_lo (boundary-exact), 1 at lag_off_hi,
// and monotone between, so the aim point slides from the LEAD point to the LAG
// point without a snap. Probed at 25% into the band, NOT the midpoint — the
// midpoint is smoothstep's flip fixed point (w = 0.5 there for any lo/hi), so a
// midpoint-only probe passes even for a plain linear ramp. MUTATION: swap
// smoothstep for a step at the midpoint -> the 25% probe reads 0, not ~0.16.
// ---------------------------------------------------------------------------
TEST_CASE("bfm: the lag blend is boundary exact and monotone across the band") {
    const bfm::BfmParams bp;
    const double lo = bp.lag_off_lo, hi = bp.lag_off_hi;
    REQUIRE(hi > lo);  // non-vacuous band

    REQUIRE(bfm::smoothstep(lo, hi, lo) == 0.0);  // exact, not Approx
    REQUIRE(bfm::smoothstep(lo, hi, hi) == 1.0);
    REQUIRE(bfm::smoothstep(lo, hi, lo - 1.0) == 0.0);  // clamped below
    REQUIRE(bfm::smoothstep(lo, hi, hi + 1.0) == 1.0);  // clamped above
    const double q = lo + 0.25 * (hi - lo);
    const double wq = bfm::smoothstep(lo, hi, q);
    REQUIRE(wq > 0.0);
    REQUIRE(wq < 0.5);  // eased in, NOT linear (linear = 0.25)
    REQUIRE(wq == Catch::Approx(0.15625).margin(1e-12));  // 0.25^2*(3-0.5)

    // The aim point the Offensive branch builds from w slides monotonically
    // from lead_point toward lag_point.
    drone::DroneParams dp;
    drone::DroneState d = drone::spawn_drone(kAp, dp, 0, 1);
    sim::SimState player;
    player.position = d.curr.position + nose_of(d.curr) * 800.0;
    player.velocity = right_of(d.curr) * 100.0;
    const glm::dvec3 lead = bfm::lead_point(
        d.curr, player, dp.pursue_lead_speed, dp.pursue_lead_max_s);
    const glm::dvec3 lag = bfm::lag_point(player, bp.lag_dist_m);
    REQUIRE(glm::length(lag - lead) > 1.0);  // non-vacuous: they differ

    double prev = glm::length(lead - lag);  // distance still to travel
    for (int i = 1; i <= 20; ++i) {
        const double x = lo + (hi - lo) * (static_cast<double>(i) / 20.0);
        const double w = bfm::smoothstep(lo, hi, x);
        const glm::dvec3 aim = lead + (lag - lead) * w;
        const double remaining = glm::length(aim - lag);
        REQUIRE(remaining < prev);  // strictly monotone toward the lag point
        prev = remaining;
    }
    REQUIRE(prev == Catch::Approx(0.0).margin(1e-9));  // arrives AT lag at hi
}

// ---------------------------------------------------------------------------
// 5. DEGENERACY. A coincident player produces a zero command with the mode HELD
// and no NaN; a near-stationary player has no meaningful "behind", so the lag
// point falls back to his position instead of normalizing a ~zero velocity.
// MUTATION: drop either guard -> normalize(0) = NaN reaches sim::step and the
// whole flight becomes NaN.
// ---------------------------------------------------------------------------
TEST_CASE("bfm: degenerate geometry is a NaN-safe hold") {
    drone::DroneParams dp;
    bfm::BfmParams bp;
    bp.enabled = true;
    const bfm::BfmDials dl = dials_of(dp);
    drone::DroneState d = drone::spawn_drone(kAp, dp, 0, 1);

    SECTION("coincident player: zero command, guns cold, mode held") {
        sim::SimState coincident;
        coincident.position = d.curr.position;
        bfm::BfmState st;
        st.mode = bfm::BfmState::Mode::Offensive;
        st.mode_ticks = 7;
        const bfm::BfmCmd c =
            bfm::bfm_step(st, d.curr, coincident, bp, dl, kAp, kAp.sim_dt);
        REQUIRE(c.target_bank == 0.0);
        REQUIRE(c.target_gamma == 0.0);
        REQUIRE_FALSE(c.guns_hot);
        REQUIRE(std::isfinite(c.speed_target));
        REQUIRE(c.speed_target > 0.0);  // never a stall order
        REQUIRE(st.mode == bfm::BfmState::Mode::Offensive);  // HELD
        REQUIRE(st.mode_ticks == 7);                         // not advanced
        const bfm::BfmGeom g = bfm::bfm_geom(d.curr, coincident, kAp);
        REQUIRE(g.degenerate);
        REQUIRE(finite3(g.los));
    }

    SECTION("near-stationary player: the lag point guard holds") {
        sim::SimState still;
        still.position = d.curr.position + nose_of(d.curr) * 500.0;
        still.velocity = glm::dvec3{0.0, 0.0, 0.0};
        REQUIRE(bfm::lag_point(still, bp.lag_dist_m) == still.position);
        still.velocity = nose_of(d.curr) * 0.5;  // below the 1 m/s guard
        REQUIRE(bfm::lag_point(still, bp.lag_dist_m) == still.position);
        // Just above the guard the lag point IS displaced (non-vacuous).
        still.velocity = nose_of(d.curr) * 50.0;
        REQUIRE(glm::length(bfm::lag_point(still, bp.lag_dist_m) -
                            still.position) ==
                Catch::Approx(bp.lag_dist_m).margin(1e-9));

        still.velocity = glm::dvec3{0.0};
        bfm::BfmState st;
        st.mode = bfm::BfmState::Mode::Offensive;
        const bfm::BfmCmd c =
            bfm::bfm_step(st, d.curr, still, bp, dl, kAp, kAp.sim_dt);
        REQUIRE(std::isfinite(c.target_bank));
        REQUIRE(std::isfinite(c.target_gamma));
        REQUIRE(std::isfinite(c.speed_target));
    }
}

// ---------------------------------------------------------------------------
// 6. LEAD POINT SINGLE SOURCE. bfm::lead_point matches the hand-computed
// first-order solution (including the cap), and drone::pursue's bank IS
// maverick::steer_bank fed the SAME bfm::lead_point aim point — bit-for-bit, no
// second copy of the deflection lead. (pursue()'s own bank/gamma steering law
// on THIS tree is unchanged by this port — see drone/drone.h:560 — but its lead
// point now comes from bfm::lead_point, the single source this leg pins.)
// MUTATION: re-derive the lead inside bfm, or re-inline it in pursue() -> this
// compare diverges the moment either copy is retuned.
// ---------------------------------------------------------------------------
TEST_CASE("bfm: lead_point is the single-source deflection solution") {
    drone::DroneParams dp;
    drone::DroneState d = drone::spawn_drone(kAp, dp, 0, 1);

    sim::SimState player;  // crossing target
    player.position = d.curr.position + nose_of(d.curr) * 1200.0;
    player.velocity = right_of(d.curr) * 120.0;

    const double range = glm::length(player.position - d.curr.position);
    const double raw_t = range / dp.pursue_lead_speed;
    const double t_lead = std::min(raw_t, dp.pursue_lead_max_s);
    REQUIRE(t_lead > 0.1);     // non-vacuous lead
    REQUIRE(t_lead == raw_t);  // sanity: this scenario is below the cap
    const glm::dvec3 expect =
        player.position + (player.velocity - d.curr.velocity) * t_lead;
    const glm::dvec3 got = bfm::lead_point(d.curr, player, dp.pursue_lead_speed,
                                           dp.pursue_lead_max_s);
    REQUIRE(glm::length(got - expect) == Catch::Approx(0.0).margin(1e-9));
    REQUIRE(glm::length(got - player.position) > 1.0);  // it really led

    // The CAP: a lead_max_s below the solved lead time truncates it exactly.
    const double cap = 0.5 * raw_t;
    const glm::dvec3 capped =
        bfm::lead_point(d.curr, player, dp.pursue_lead_speed, cap);
    const glm::dvec3 expect_cap =
        player.position + (player.velocity - d.curr.velocity) * cap;
    REQUIRE(glm::length(capped - expect_cap) ==
            Catch::Approx(0.0).margin(1e-9));
    // ...and a cap ABOVE the solved time is inert.
    REQUIRE(bfm::lead_point(d.curr, player, dp.pursue_lead_speed,
                            10.0 * raw_t) == got);
    // lead_speed <= 1 -> pure boresight.
    REQUIRE(bfm::lead_point(d.curr, player, 0.0, 0.0) == player.position);

    // FIX-9 (lead_max_s semantics): lead_max_s <= 0 means UNCAPPED, not
    // "cap to zero" -- the intentional semantics change from the old inline
    // `std::min(t_lead, lead_max_s)` call sites this was ported from (a bare
    // min() with a 0 cap would truncate t_lead to 0, collapsing to boresight).
    // Pinned against a huge cap standing in for "no cap".
    const glm::dvec3 got_uncapped =
        bfm::lead_point(d.curr, player, dp.pursue_lead_speed, 1.0e9);
    REQUIRE(bfm::lead_point(d.curr, player, dp.pursue_lead_speed, 0.0) ==
            got_uncapped);

    // drone::pursue()'s bank IS maverick::steer_bank fed the SAME
    // bfm::lead_point aim point, bit-exact — the single-source pin.
    // game-AI-R4: pursue STEERS at the gun-scale-CAPPED lead
    // (pursue_steer_lead_max_s — the full ballistic lead is a navigation
    // target displaced opposite the shooter's own velocity, measured as a
    // permanent pointing bias); the FIRE gate alone keeps the full lead.
    // This scenario's raw_t (2.0 s) exceeds the 1.5 s steer cap, so the leg
    // genuinely discriminates the capped path.
    REQUIRE(raw_t > dp.pursue_steer_lead_max_s);
    const glm::dvec3 steer_aim = bfm::lead_point(
        d.curr, player, dp.pursue_lead_speed,
        std::min(dp.pursue_lead_max_s, dp.pursue_steer_lead_max_s));
    const glm::dvec3 to_hat = glm::normalize(steer_aim - d.curr.position);
    const double expect_bank = maverick::steer_bank(
        d.curr, to_hat, dp.pursue_k_az, dp.pursue_max_bank);
    const drone::PursueCmd pc = drone::pursue(d.curr, player, dp);
    REQUIRE(pc.target_bank == expect_bank);  // bit-exact
}

// ---------------------------------------------------------------------------
// FIX-1a. THE YOYO COOLDOWN (open loop). The red-team's probe geometry: a
// player fixed 60 deg off nose, closing 60 m/s, e_delta near 0 -- a predictor
// held hot FOREVER. Without the cooldown, the machine would leave Yoyo (on the
// yoyo_time_s timeout, since closure never drops below the exit threshold),
// immediately re-satisfy the overshoot predictor, and re-enter Yoyo after only
// yoyo_arm_s -- a limit cycle that spends almost no time actually shooting.
// MUTATION: remove the cooldown -> re-entry happens after yoyo_arm_s only,
// well under min_dwell_s.
// ---------------------------------------------------------------------------
TEST_CASE(
    "bfm: the yoyo cooldown guarantees a guns-hot spell before re-entry") {
    drone::DroneParams dp;
    bfm::BfmParams bp;
    bp.enabled = true;
    const bfm::BfmDials dl = dials_of(dp);
    const double dt = kAp.sim_dt;

    drone::DroneState d = drone::spawn_drone(kAp, dp, 0, 1);
    d.curr.velocity = glm::dvec3{0.0};
    const glm::dvec3 dir = std::cos(60.0 * kPi / 180.0) * nose_of(d.curr) +
                           std::sin(60.0 * kPi / 180.0) * right_of(d.curr);
    sim::SimState hot;
    hot.position = d.curr.position + dir * 500.0;
    hot.velocity = -dir * 60.0;
    hot.orientation = d.curr.orientation;

    const bfm::BfmGeom gh = bfm::bfm_geom(d.curr, hot, kAp);
    REQUIRE(gh.closure > bp.yoyo_closure_mps);
    REQUIRE(gh.angle_off > bp.yoyo_angle);
    REQUIRE(gh.closure > bp.yoyo_exit_closure_mps);  // stays "hot" through Yoyo
    REQUIRE(gh.e_delta > -bp.extend_energy_m);       // Extend stays quiet

    bfm::BfmState st;
    st.mode = bfm::BfmState::Mode::Offensive;

    // Drive it into the first Yoyo.
    bool entered_yoyo = false;
    for (int t = 0; t < 2000 && !entered_yoyo; ++t) {
        bfm::bfm_step(st, d.curr, hot, bp, dl, kAp, dt);
        entered_yoyo = st.mode == bfm::BfmState::Mode::Yoyo;
    }
    REQUIRE(entered_yoyo);

    // Ride the Yoyo out. Closure never drops below the exit threshold (the
    // hot fixture), so the ONLY exit is the yoyo_time_s timeout.
    bool exited_yoyo = false;
    for (int t = 0; t < 2000 && !exited_yoyo; ++t) {
        bfm::bfm_step(st, d.curr, hot, bp, dl, kAp, dt);
        exited_yoyo = st.mode == bfm::BfmState::Mode::Offensive;
    }
    REQUIRE(exited_yoyo);
    REQUIRE(st.yoyo_cooldown > 0);  // the FIX-1 cooldown armed on the exit

    // Count ticks flown in Offensive before the predictor re-arms Yoyo.
    long long flown = 0;
    while (st.mode == bfm::BfmState::Mode::Offensive && flown < 2000) {
        bfm::bfm_step(st, d.curr, hot, bp, dl, kAp, dt);
        ++flown;
    }
    REQUIRE(st.mode == bfm::BfmState::Mode::Yoyo);  // it DID re-arm eventually
    const long long min_dwell =
        static_cast<long long>(std::ceil(bp.min_dwell_s / dt));
    REQUIRE(min_dwell > 1);  // non-vacuous floor
    REQUIRE(flown >= min_dwell);
}

// ---------------------------------------------------------------------------
// FIX-1b. FRUSTRATION MEASURES THE WHOLE FIGHT (open loop). The same hot-cycle
// geometry as FIX-1a: without fight_ticks, a bandit ping-ponging Offensive<->
// Yoyo would have its frustration clock (mode_ticks) reset on every Yoyo
// entry and NEVER reach frustration_s. MUTATION: frustration keyed to
// mode_ticks (as before FIX-1) never fires within this bound.
// ---------------------------------------------------------------------------
TEST_CASE(
    "bfm: frustration is measured against the whole Offensive+Yoyo fight") {
    drone::DroneParams dp;
    bfm::BfmParams bp;
    bp.enabled = true;
    const bfm::BfmDials dl = dials_of(dp);
    const double dt = kAp.sim_dt;

    drone::DroneState d = drone::spawn_drone(kAp, dp, 0, 1);
    d.curr.velocity = glm::dvec3{0.0};
    const glm::dvec3 dir = std::cos(60.0 * kPi / 180.0) * nose_of(d.curr) +
                           std::sin(60.0 * kPi / 180.0) * right_of(d.curr);
    sim::SimState hot;
    hot.position = d.curr.position + dir * 500.0;
    hot.velocity = -dir * 60.0;
    hot.orientation = d.curr.orientation;
    REQUIRE(bfm::bfm_geom(d.curr, hot, kAp).e_delta > -bp.extend_energy_m);

    bfm::BfmState st;
    st.mode = bfm::BfmState::Mode::Offensive;
    const long long frustration_ticks =
        static_cast<long long>(std::ceil(bp.frustration_s / dt));
    REQUIRE(frustration_ticks > 1);  // non-vacuous

    bool entered_extend = false;
    long long steps = 0;
    const long long cap = frustration_ticks * 3;  // generous ceiling
    for (; steps < cap; ++steps) {
        bfm::bfm_step(st, d.curr, hot, bp, dl, kAp, dt);
        if (st.mode == bfm::BfmState::Mode::Extend) {
            entered_extend = true;
            break;
        }
    }
    REQUIRE(entered_extend);
    // Within ~1.5x frustration_s of actual fight time -- a slack bound (the
    // Extend check only fires from within an Offensive tick, so an in-flight
    // Yoyo stint can push the crossing a bit past the raw threshold).
    REQUIRE(static_cast<double>(steps) <=
            1.5 * static_cast<double>(frustration_ticks));
}

// ---------------------------------------------------------------------------
// FIX-2a. GUNS_HOT VETO THROUGH drone::tick. pursue() itself would fire at
// this geometry (REQUIRE'd first, non-vacuous); only Offensive is allowed to
// let that through drone::tick's d.wants_fire. MUTATION: drop the
// `bc.guns_hot &&` term -> the three cold cases fire too.
// ---------------------------------------------------------------------------
TEST_CASE("bfm: guns_hot veto reaches drone::tick's wants_fire") {
    drone::DroneParams dp;
    dp.bfm.enabled = true;
    drone::DroneState base = drone::spawn_drone(kAp, dp, 0, 1);
    base.engaged = true;

    sim::SimState player;
    player.position = base.curr.position + nose_of(base.curr) * 300.0;
    player.orientation = base.curr.orientation;
    player.velocity = glm::dvec3{0.0};

    // Non-vacuous: pursue()'s OWN fire decision is true at this geometry.
    REQUIRE(drone::pursue(base.curr, player, dp).fire);

    auto run_one = [&](bfm::BfmState::Mode mode) {
        drone::DroneState d = base;
        d.bfm = bfm::BfmState{};
        d.bfm.mode = mode;
        drone::tick(d, kAp, dp, nullptr, &player);
        return d.wants_fire;
    };

    REQUIRE_FALSE(run_one(bfm::BfmState::Mode::Intercept));
    REQUIRE_FALSE(run_one(bfm::BfmState::Mode::Yoyo));
    REQUIRE_FALSE(run_one(bfm::BfmState::Mode::Extend));
    REQUIRE(run_one(bfm::BfmState::Mode::Offensive));
}

// ---------------------------------------------------------------------------
// FIX-2b. EXTEND OPEN-LOOP: command pin + exit timing. Forces entry via a
// synthetic e_delta well below -extend_energy_m, pins the Extend command
// exactly, then (e_delta held deep-negative forever) pins the exit at the
// extend_max_s TIMEOUT. The FIX-7 sibling below pins the RECOVERY exit.
// ---------------------------------------------------------------------------
TEST_CASE(
    "bfm: Extend command is pinned and the deep-deficit exit is the timeout") {
    drone::DroneParams dp;
    bfm::BfmParams bp;
    bp.enabled = true;
    const bfm::BfmDials dl = dials_of(dp);
    const double dt = kAp.sim_dt;

    drone::DroneState d = drone::spawn_drone(kAp, dp, 0, 1);
    d.curr.velocity = glm::dvec3{0.0};

    // Player much higher + faster: e_delta = (alt_s - alt_p) - Vp^2/(2g), deep
    // negative regardless of the airframe's own g.
    sim::SimState player;
    player.position = d.curr.position +
                      sim::local_up(d.curr.position) * 5000.0 +
                      nose_of(d.curr) * 100.0;
    player.velocity = nose_of(d.curr) * 300.0;
    player.orientation = d.curr.orientation;
    const bfm::BfmGeom g0 = bfm::bfm_geom(d.curr, player, kAp);
    REQUIRE(g0.e_delta < -bp.extend_energy_m);  // non-vacuous: really deep
    // FIX-F1: this geometry's range is ALSO past Offensive's own range-
    // release radius (attack_range_m * attack_release_frac) — i.e. the
    // Intercept-release predicate is true here too. The entry below still
    // resolves to Extend, not Intercept, because the Offensive transition
    // switch checks Extend's predicate (energy/frustration) BEFORE the
    // range-release check in its else-if chain — Extend-before-range-release
    // precedence, not this fixture happening to dodge the range check.
    REQUIRE(g0.range > bp.attack_range_m * bp.attack_release_frac);

    bfm::BfmState st;
    st.mode = bfm::BfmState::Mode::Offensive;
    const long long min_dwell =
        static_cast<long long>(std::ceil(bp.min_dwell_s / dt));
    bfm::BfmCmd cmd;
    long long flown = 0;
    for (int t = 0; t < 5000; ++t) {
        cmd = bfm::bfm_step(st, d.curr, player, bp, dl, kAp, dt);
        if (st.mode != bfm::BfmState::Mode::Offensive) break;
        ++flown;
    }
    REQUIRE(st.mode == bfm::BfmState::Mode::Extend);
    REQUIRE(flown >= min_dwell - 1);

    // Pin the Extend command (the tick it was entered already commands it).
    REQUIRE(cmd.target_bank == 0.0);
    REQUIRE(cmd.target_gamma == bp.extend_gamma);
    REQUIRE_FALSE(cmd.guns_hot);
    REQUIRE(cmd.speed_target ==
            Catch::Approx(dl.speed + bp.intercept_speed_bump));

    // Held deep-negative forever: the ONLY exit is the extend_max_s timeout.
    long long extend_ticks = 0;
    for (int t = 0; t < 5000; ++t) {
        bfm::bfm_step(st, d.curr, player, bp, dl, kAp, dt);
        if (st.mode != bfm::BfmState::Mode::Extend) break;
        ++extend_ticks;
    }
    REQUIRE(st.mode == bfm::BfmState::Mode::Intercept);
    const long long expect_max =
        static_cast<long long>(std::ceil(bp.extend_max_s / dt));
    REQUIRE(extend_ticks - expect_max <= 1);
    REQUIRE(expect_max - extend_ticks <= 1);
}

// ---------------------------------------------------------------------------
// FIX-7 sibling. Same Extend entry, but the player then RECOVERS to a shallow
// deficit (e_delta just above -reenter_energy_m -- still a net deficit, NOT a
// surplus, the point of the RECOVERY-semantics fix): exit fires by recovery,
// strictly before the extend_max_s timeout. MUTATION: the old
// `e_delta > +reenter_energy_m` comparison never fires here (a shallow
// deficit is not a surplus) and the leg would fall through to the timeout.
// ---------------------------------------------------------------------------
TEST_CASE(
    "bfm: extend exits by RECOVERY once the deficit shallows, before the "
    "timeout") {
    drone::DroneParams dp;
    bfm::BfmParams bp;
    bp.enabled = true;
    const bfm::BfmDials dl = dials_of(dp);
    const double dt = kAp.sim_dt;

    drone::DroneState d = drone::spawn_drone(kAp, dp, 0, 1);
    d.curr.velocity = glm::dvec3{0.0};

    sim::SimState deep;  // forces entry into Extend
    deep.position = d.curr.position + sim::local_up(d.curr.position) * 5000.0 +
                    nose_of(d.curr) * 100.0;
    deep.velocity = nose_of(d.curr) * 300.0;
    deep.orientation = d.curr.orientation;
    REQUIRE(bfm::bfm_geom(d.curr, deep, kAp).e_delta < -bp.extend_energy_m);

    bfm::BfmState st;
    st.mode = bfm::BfmState::Mode::Offensive;
    for (int t = 0; t < 5000 && st.mode == bfm::BfmState::Mode::Offensive;
         ++t) {
        bfm::bfm_step(st, d.curr, deep, bp, dl, kAp, dt);
    }
    REQUIRE(st.mode == bfm::BfmState::Mode::Extend);

    // A recovered player: bandit is now only ~100 m below the player -- a
    // shallow deficit comfortably between -reenter_energy_m (-200) and 0.
    sim::SimState recovered;
    recovered.position = d.curr.position + nose_of(d.curr) * 400.0 +
                         sim::local_up(d.curr.position) * 100.0;
    recovered.velocity = glm::dvec3{0.0};
    recovered.orientation = d.curr.orientation;
    const bfm::BfmGeom gr = bfm::bfm_geom(d.curr, recovered, kAp);
    REQUIRE(gr.e_delta > -bp.reenter_energy_m);
    REQUIRE(gr.e_delta < 0.0);  // still a net DEFICIT, not a surplus

    const long long min_dwell =
        static_cast<long long>(std::ceil(bp.extend_min_s / dt));
    const long long max_dwell =
        static_cast<long long>(std::ceil(bp.extend_max_s / dt));
    long long flown = 0;
    bool exited = false;
    for (int t = 0; t < 5000; ++t) {
        bfm::bfm_step(st, d.curr, recovered, bp, dl, kAp, dt);
        ++flown;
        if (st.mode != bfm::BfmState::Mode::Extend) {
            exited = true;
            break;
        }
    }
    REQUIRE(exited);
    REQUIRE(st.mode == bfm::BfmState::Mode::Intercept);
    REQUIRE(flown >= min_dwell);
    REQUIRE(flown < max_dwell);  // RECOVERY, not the timeout
}

// ---------------------------------------------------------------------------
// FIX-2c. RESPAWN RESET. A killed engaged bandit reborn kilometres away must
// not resume a half-flown yo-yo / stale cooldown / fight clock from its
// previous life.
// ---------------------------------------------------------------------------
TEST_CASE("bfm: respawn_in_place resets every bfm state field") {
    drone::DroneParams dp;
    drone::DroneState d = drone::spawn_drone(kAp, dp, 0, 1);
    d.bfm.mode = bfm::BfmState::Mode::Yoyo;
    d.bfm.mode_ticks = 99;
    d.bfm.arm_ticks = 5;
    d.bfm.fight_ticks = 500;
    d.bfm.yoyo_cooldown = 7;

    drone::respawn_in_place(d, kAp, dp);

    REQUIRE(d.bfm.mode == bfm::BfmState::Mode::Intercept);
    REQUIRE(d.bfm.mode_ticks == 0);
    REQUIRE(d.bfm.arm_ticks == 0);
    REQUIRE(d.bfm.fight_ticks == 0);
    REQUIRE(d.bfm.yoyo_cooldown == 0);
}

// ---------------------------------------------------------------------------
// FIX-A + round-4 (P1, air-abort flapping / arm-side inversion). The yo-yo's
// air logic is TWO-sided: arming is gated STRUCTURALLY on climb_arm_ok (the
// "air-seek dive is off" bool drone::tick computes as atm_frac >=
// avoid_air_frac_full); an ACTIVE yoyo aborts only when the ceiling fade
// ramp climb_t drops below climb_abort_t (0.25). A ripple of the arm gate
// must never arm the predictor, a sustained arm-ok hold must, and — the
// actual point of the hysteresis gap — an active yoyo whose climb_t dips
// into the band (arm gate lost, but climb_t still above abort_t) must NOT
// abort: it completes on its normal exit. MUTATION: abort on the arm gate
// (or raise abort_t to 1.0) -> the no-abort REQUIRE below fails.
// ---------------------------------------------------------------------------
TEST_CASE(
    "bfm: the air arm gate ripples without arming, "
    "arms on a steady hold, and does not abort a dip inside the band") {
    drone::DroneParams dp;
    bfm::BfmParams bp;
    bp.enabled = true;
    bfm::BfmDials dl = dials_of(dp);
    const double dt = kAp.sim_dt;
    REQUIRE(bp.climb_abort_t < 1.0);  // non-vacuous band below the structural
                                      // arm boundary (climb_t == 1.0 exactly)

    drone::DroneState d = drone::spawn_drone(kAp, dp, 0, 1);
    d.curr.velocity = glm::dvec3{0.0};
    const glm::dvec3 dir = std::cos(60.0 * kPi / 180.0) * nose_of(d.curr) +
                           std::sin(60.0 * kPi / 180.0) * right_of(d.curr);
    sim::SimState
        hot;  // closure 60 > 40, angle off 60 deg > 45 -> predicate TRUE
    hot.position = d.curr.position + dir * 500.0;
    hot.velocity = -dir * 60.0;
    hot.orientation = d.curr.orientation;
    const bfm::BfmGeom gh = bfm::bfm_geom(d.curr, hot, kAp);
    REQUIRE(gh.closure > bp.yoyo_closure_mps);
    REQUIRE(gh.angle_off > bp.yoyo_angle);
    REQUIRE(gh.closure > bp.yoyo_exit_closure_mps);  // stays hot through Yoyo
    REQUIRE(gh.e_delta > -bp.extend_energy_m);       // Extend stays quiet

    bfm::BfmState st;
    st.mode = bfm::BfmState::Mode::Offensive;

    // Ripple the STRUCTURAL arm gate every tick: arm_ticks resets on every
    // gate-closed tick, so the arm counter can never accumulate to
    // yoyo_arm_s's dwell threshold.
    for (int t = 0; t < 2000; ++t) {
        dl.climb_arm_ok = (t % 2 != 0);
        bfm::bfm_step(st, d.curr, hot, bp, dl, kAp, dt);
        REQUIRE(st.mode == bfm::BfmState::Mode::Offensive);
        REQUIRE(st.arm_ticks <= 1);
    }

    // A steady hold with the arm gate OPEN does arm and enter Yoyo.
    dl.climb_arm_ok = true;
    dl.climb_t = 1.0;
    bool entered_yoyo = false;
    for (int t = 0; t < 2000 && !entered_yoyo; ++t) {
        bfm::bfm_step(st, d.curr, hot, bp, dl, kAp, dt);
        entered_yoyo = st.mode == bfm::BfmState::Mode::Yoyo;
    }
    REQUIRE(entered_yoyo);
    REQUIRE(st.mode_ticks == 1);  // the entering tick already flew it once

    // Now the active yoyo LOSES the arm gate AND its climb_t dips into the
    // band (still above abort_t=0.25) for the rest of the climb — it must
    // NOT abort: held there (geometry stays hot -> closure never drops below
    // yoyo_exit_closure_mps), the ONLY possible exit is the yoyo_time_s
    // TIMEOUT, a normal closure, never the abort bypass (proven by the tick
    // count landing at the real timeout, not near-immediately after the dip).
    dl.climb_arm_ok =
        false;  // the arm gate closing must NOT abort an active yoyo
    dl.climb_t = 0.4;
    long long flown = 0;
    for (int t = 0; t < 2000; ++t) {
        bfm::bfm_step(st, d.curr, hot, bp, dl, kAp, dt);
        ++flown;
        if (st.mode != bfm::BfmState::Mode::Yoyo) break;
    }
    REQUIRE(st.mode == bfm::BfmState::Mode::Offensive);
    // mode_ticks was already 1 when this loop started (the entering tick), so
    // this loop's own tick count lands at dwell_ticks(yoyo_time_s, dt) + 1.
    const long long expect_timeout =
        static_cast<long long>(std::ceil(bp.yoyo_time_s / dt)) + 1;
    const long long diff = flown > expect_timeout ? flown - expect_timeout
                                                  : expect_timeout - flown;
    REQUIRE(diff <= 1);
    REQUIRE(st.yoyo_cooldown > 0);
}

// ---------------------------------------------------------------------------
// FIX-A (P1, air-abort flapping), sibling leg. An active yoyo whose climb_t
// drops BELOW climb_abort_t (deep in the ceiling fade) aborts on the very
// next tick — bypassing may_exit, arming the FIX-1/FIX-D cooldown on the way
// out. MUTATION: drop the abort -> mode stays Yoyo.
// ---------------------------------------------------------------------------
TEST_CASE(
    "bfm: an active yoyo aborts the next step once climb_t drops "
    "below climb_abort_t") {
    drone::DroneParams dp;
    bfm::BfmParams bp;
    bp.enabled = true;
    bfm::BfmDials dl = dials_of(dp);
    dl.climb_t = 0.1;  // < bp.climb_abort_t (0.25)
    const double dt = kAp.sim_dt;
    REQUIRE(dl.climb_t < bp.climb_abort_t);

    drone::DroneState d = drone::spawn_drone(kAp, dp, 0, 1);
    d.curr.velocity = glm::dvec3{0.0};
    const glm::dvec3 dir = std::cos(60.0 * kPi / 180.0) * nose_of(d.curr) +
                           std::sin(60.0 * kPi / 180.0) * right_of(d.curr);
    sim::SimState hot;
    hot.position = d.curr.position + dir * 500.0;
    hot.velocity = -dir * 60.0;
    hot.orientation = d.curr.orientation;

    bfm::BfmState st;
    st.mode =
        bfm::BfmState::Mode::Yoyo;  // as if armed just before the air thinned
    bfm::bfm_step(st, d.curr, hot, bp, dl, kAp, dt);
    REQUIRE(st.mode == bfm::BfmState::Mode::Offensive);
    REQUIRE(st.yoyo_cooldown > 0);
}

// ===========================================================================
// ★★★ RUNG S3-GUNS — THE UN-FADE, ITS SCOPE, AND WHY IT SHIPS OFF.
//
// Chad, 2026-08-26: "attack me ... stop nerfing them in secret", and on what to
// give back for it: "Nothing - leave it at full strength."
//
// MEASURED across all 12 tapes: an enemy holding a player slot sits 86.7-89.8%
// of that time beyond fire_range_max (900 m) with a median closure of -1 to
// -7 m/s. The binding gate is arithmetic: the ruled 265 m/s chase ceiling was
// faded by align^2 before use, giving 85 + 180*align^2 = a measured median
// 144 m/s (n=1671 out-of-band Intercept samples, tape 12) against Chad's own
// measured median 262 m/s. bfm_intercept_chase_unfaded repeals that.
//
// ⚠ AND IT SHIPS false, ON MEASUREMENT: probe P-A, the only fixture that counts
// rounds aimed at the player, reads 54 -> 23 rounds and 16 -> 2 hits with this
// key ON (noise floor 0: +/-1e-9 on two in-path dials moves it 0). The faster
// command arrives with more energy than the turn can spend and never converts
// the closure into a firing solution -- closest approach 16.96 m -> 110.58 m.
// This leg pins the mechanism and its scope for the day that is ruled on.
//
// THREE CLAUSES, and the second and third are the ones that matter — a fix
// that also sped up the FLEE mode would break Chad's ruled asymmetry (pursuit
// unlimited, EVADE CONTAINED) with nobody noticing.
// MUTATION (run, went RED): point Intercept back at chase_ceiling() -> clause 1
//   reads 121 not 265; give chase_ceiling the un-fade instead -> clause 2's
//   Extend command jumps and goes RED.
// ===========================================================================
TEST_CASE("S3-GUNS: the un-fade lifts INTERCEPT only and only outside the merge") {
    const cfg::ScenarioParams scen =
        cfg::load_scenario_toml(SEADS_CONFIG_DIR "/scenario.toml", kAp);
    const drone::DroneParams dp = scen.drone;
    REQUIRE(dp.bfm.intercept_speed_mps > 0.0);
    // ⚠ The SHIPPED value is deliberately NOT asserted here. It ships false,
    // because probe P-A measured the un-fade at -57% rounds at the player and
    // -87% hits (the table is in config/scenario.toml beside the key). This leg
    // pins the MECHANISM and its SCOPE, both arms, so the day Chad rules it on
    // the Extend clause is already standing guard. Do not weld a ruling to it.
    INFO("shipped bfm_intercept_chase_unfaded = "
         << dp.bfm.intercept_chase_unfaded);

    // A poorly-aligned stern chase well OUTSIDE the merge: align^2 = 0.20 is
    // inside the measured tape band (p50 0.15-0.37), and 3000 m > the shipped
    // bfm_attack_range_m. Player running fast and level, which is exactly the
    // geometry the tapes say never closes.
    const double align2 = 0.20;
    const double range_out = dp.bfm.attack_range_m + 800.0;

    const auto command = [&](bool unfaded, bfm::BfmState::Mode mode,
                             double range) {
        bfm::BfmParams bp = dp.bfm;
        bp.intercept_chase_unfaded = unfaded;
        drone::DroneParams d_dp = dp;
        drone::DroneState d = drone::spawn_drone(kAp, d_dp, 0, 1);
        sim::SimState player;
        player.position = d.curr.position + nose_of(d.curr) * range;
        player.orientation = d.curr.orientation;
        player.velocity = glm::normalize(d.curr.velocity) * 262.0;
        bfm::BfmDials dl = dials_of(d_dp);
        // drone::tick pre-fades the pursuit bump by the SAME align^2 it hands
        // in (drone/drone.h) — reproduce that wiring exactly, never a bare bump.
        dl.pursue_speed_bump = d_dp.pursue_speed_bump * align2;
        dl.align2 = align2;
        bfm::BfmState st;
        st.mode = mode;
        return bfm::bfm_step(st, d.curr, player, bp, dl, kAp, kAp.sim_dt)
            .speed_target;
    };

    // (1) INTERCEPT, outside the merge: the ceiling rises to the ruled dial.
    const double icept_off =
        command(false, bfm::BfmState::Mode::Intercept, range_out);
    const double icept_on =
        command(true, bfm::BfmState::Mode::Intercept, range_out);
    INFO("Intercept off=" << icept_off << " on=" << icept_on);
    CHECK(icept_off < 200.0);  // the nerf-in-effect, as measured
    CHECK(icept_on == Catch::Approx(dp.bfm.intercept_speed_mps));
    CHECK(icept_on > icept_off);

    // (2) EXTEND, the FLEE mode, SAME geometry: MUST NOT MOVE. This is Chad's
    //     ruled asymmetry in a CHECK — a bandit may chase him anywhere, but it
    //     may not flee from him any faster than it does today.
    const double ext_off =
        command(false, bfm::BfmState::Mode::Extend, range_out);
    const double ext_on = command(true, bfm::BfmState::Mode::Extend, range_out);
    INFO("Extend off=" << ext_off << " on=" << ext_on);
    CHECK(ext_on == ext_off);
    CHECK(ext_on < dp.bfm.intercept_speed_mps);

    // (3) INSIDE the merge nothing moves at all: the corner-speed law
    //     ("chase fast, turn slow") is untouched.
    const double in_off = command(false, bfm::BfmState::Mode::Intercept,
                                  dp.bfm.attack_range_m * 0.5);
    const double in_on = command(true, bfm::BfmState::Mode::Intercept,
                                 dp.bfm.attack_range_m * 0.5);
    CHECK(in_on == in_off);
}
