// Component damage model (combat/damage.h, Fable-vetted 2026-07-14). The damage
// transforms are PURE, DETERMINISTIC, and IDENTITY at zero damage — each test
// names the defect it would catch. The firewall (zero damage == bit-identical
// frozen flight) is pinned both here (the transforms) and in test_combat (the
// app-level memcmp).

#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <cstring>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "combat/damage.h"
#include "config/load_aircraft.h"
#include "config/load_controller.h"
#include "sim/step.h"

#ifdef NDEBUG
#error "SEADS gate requires an assert-live build (SPEC 6.1)"
#endif

namespace {
const sim::AircraftParams kAp =
    cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");
const control::ControllerParams kCp =
    cfg::load_controller_toml(SEADS_CONFIG_DIR "/controller.toml", kAp);
}  // namespace

// ZERO-DAMAGE BIT-IDENTITY (the firewall): a pristine DamageState leaves every
// transformed field bit-for-bit equal to base, damage_zero() true, roll_bias 0.
// MUTATION: any transform that isn't identity at 1.0 (e.g. a stray +epsilon) ->
// a golden moves.
TEST_CASE("damage: zero damage is bit-identical (the firewall seam)") {
    combat::DamageState d;  // all 1.0
    combat::DamageParams dp;
    REQUIRE(combat::damage_zero(d));

    const sim::AircraftParams a = combat::apply_damage_aircraft(kAp, d, dp);
    REQUIRE(std::memcmp(&a, &kAp, sizeof(sim::AircraftParams)) == 0);

    const control::ControllerParams c = combat::apply_damage_controller(kCp, d, dp);
    REQUIRE(std::memcmp(&c, &kCp, sizeof(control::ControllerParams)) == 0);

    REQUIRE(combat::roll_bias(d, dp, 1.0) == 0.0);
    REQUIRE(combat::roll_bias(d, dp, 4.0) == 0.0);
    REQUIRE_FALSE(combat::is_dead(d));
    REQUIRE(combat::summary_hp(d) == Catch::Approx(100.0));
}

// P0-1 SIGN: roll +1 = roll LEFT (sim/state.h). A broken LEFT wing loses lift on
// the left -> rolls LEFT -> needs a POSITIVE roll input bias. Broken right ->
// negative. MUTATION: (wing_left - wing_right) instead of (right - left) -> the
// plane rolls toward the HEALTHY wing (the packet's original bug).
TEST_CASE("damage: roll bias sign points toward the broken wing") {
    combat::DamageParams dp;
    combat::DamageState left;  left.wing_left = 0.4;   // left wing hurt
    combat::DamageState right; right.wing_right = 0.4; // right wing hurt
    REQUIRE(combat::roll_bias(left, dp, 2.0) > 0.0);   // roll LEFT (+input)
    REQUIRE(combat::roll_bias(right, dp, 2.0) < 0.0);  // roll RIGHT (-input)
    // Symmetric damage -> no net bias.
    combat::DamageState both; both.wing_left = 0.4; both.wing_right = 0.4;
    REQUIRE(combat::roll_bias(both, dp, 2.0) == Catch::Approx(0.0));
}

// P0-2 UNTRIMMABLE: the bias is LOAD-PROPORTIONAL so the rate-PI integrator can't
// trim it away — a hard pull surges it. It rises with |n| up to n_ref, floored at
// 0.25 (a small always-on lean), and never exceeds the ±0.5 cap. MUTATION: a
// constant bias (no n term) -> the surge-on-pull feel is gone.
TEST_CASE("damage: roll bias scales with load factor, floored and capped") {
    combat::DamageParams dp;
    combat::DamageState d; d.wing_left = 0.0 + 1e-9;  // near-broken left wing (max asym)
    const double b_1g = combat::roll_bias(d, dp, 1.0);
    const double b_hi = combat::roll_bias(d, dp, dp.roll_bias_n_ref);  // full
    const double b_lo = combat::roll_bias(d, dp, 0.0);                 // floored
    REQUIRE(b_hi > b_1g);                        // harder pull -> more roll
    REQUIRE(b_1g > b_lo);
    REQUIRE(b_lo == Catch::Approx(0.5 * 0.25));  // floor: gain*asym*0.25
    REQUIRE(b_hi <= 0.5 + 1e-12);                // never past the cap
    // Cap holds even at absurd load.
    REQUIRE(combat::roll_bias(d, dp, 100.0) == Catch::Approx(0.5));
}

// ENGINE: thrust scales with engine health; 0 = dead-stick (T_max 0). Nothing
// else on the airframe changes for engine damage alone.
TEST_CASE("damage: engine damage scales thrust to zero, touches nothing else") {
    combat::DamageParams dp;
    combat::DamageState d; d.engine = 0.5;
    const sim::AircraftParams a = combat::apply_damage_aircraft(kAp, d, dp);
    REQUIRE(a.T_max == Catch::Approx(kAp.T_max * 0.5));
    REQUIRE(a.Cl_max == kAp.Cl_max);   // wings untouched
    REQUIRE(a.c_roll == kAp.c_roll);
    combat::DamageState dead; dead.engine = 0.0;
    REQUIRE(combat::apply_damage_aircraft(kAp, dead, dp).T_max == 0.0);
}

// P0-6 STALL-ALPHA INVARIANT: Cl_max and Cl_alpha scale by the SAME factor, so
// the stall AoA (Cl_max/Cl_alpha) is unchanged — the loader's aoa_max <= stall
// alpha guard (bypassed at runtime) stays true. MUTATION: scale only Cl_max ->
// the ratio moves and the AoA protector chases a boundary that shifted.
TEST_CASE("damage: wing damage keeps the stall-alpha ratio invariant") {
    combat::DamageParams dp;
    combat::DamageState d; d.wing_left = 0.5; d.wing_right = 0.5;
    const sim::AircraftParams a = combat::apply_damage_aircraft(kAp, d, dp);
    REQUIRE(a.Cl_max < kAp.Cl_max);        // lift dropped
    REQUIRE(a.Cl_alpha < kAp.Cl_alpha);
    REQUIRE(a.Cl_max / a.Cl_alpha ==
            Catch::Approx(kAp.Cl_max / kAp.Cl_alpha));  // ratio invariant
    REQUIRE(a.c_roll < kAp.c_roll);        // sluggish roll
    REQUIRE(a.Cd0 > kAp.Cd0);              // draggier
}

// P0-5/Q6 FLOORS: the WORST legal partial damage keeps the plane controllable —
// authority never divides by zero (plant_invert denominator) and stays above the
// Fable floors, drag stays capped. MUTATION: drop a floor -> c_roll -> ~0 ->
// plant_invert blows up / pointing dies.
TEST_CASE("damage: max legal damage stays above the controllability floors") {
    combat::DamageParams dp;
    combat::DamageState d;  // maximal PARTIAL damage (0 would be death, excluded)
    d.wing_left = 1e-6; d.wing_right = 1e-6; d.structure = 1e-6; d.engine = 0.0;
    const sim::AircraftParams a = combat::apply_damage_aircraft(kAp, d, dp);
    REQUIRE(a.c_pitch >= kAp.c_pitch * dp.c_pitchyaw_floor - 1e-9);
    REQUIRE(a.c_yaw >= kAp.c_yaw * dp.c_pitchyaw_floor - 1e-9);
    REQUIRE(a.c_roll >= kAp.c_roll * dp.c_roll_floor - 1e-9);
    REQUIRE(a.c_pitch > 0.0);   // plant_invert denominator stays finite
    REQUIRE(a.c_yaw > 0.0);
    REQUIRE(a.c_roll > 0.0);
    REQUIRE(a.Cl_max >= kAp.Cl_max * dp.cl_floor - 1e-9);
    REQUIRE(a.Cd0 <= kAp.Cd0 * dp.cd0_cap_mult + 1e-9);
    // Whitelisted fields NEVER move (Fable Q6): q_att_floor, damp_*, mass, I_*,
    // the speed ramp, sim_dt, world.
    REQUIRE(a.q_att_floor == kAp.q_att_floor);
    REQUIRE(a.damp_roll == kAp.damp_roll);
    REQUIRE(a.mass == kAp.mass);
    REQUIRE(a.I_roll == kAp.I_roll);
    REQUIRE(a.v_redline == kAp.v_redline);
    REQUIRE(a.g == kAp.g);
}

// PILOT gain: one uniform factor across the pointing + rate gains, floored at
// pilot_gain_floor so a dead pilot's cascade is sloppy-but-stable (both loader
// guards are degree-1 homogeneous, so a uniform scale preserves them). MUTATION:
// a SPLIT factor (K_w vs K_theta scaled differently) -> critical damping breaks.
TEST_CASE("damage: pilot damage scales the cascade gains uniformly and floored") {
    combat::DamageParams dp;
    REQUIRE(combat::pilot_gain(1.0, dp) == Catch::Approx(1.0));
    REQUIRE(combat::pilot_gain(0.0, dp) == Catch::Approx(dp.pilot_gain_floor));
    combat::DamageState d; d.pilot = 0.0;
    const control::ControllerParams c = combat::apply_damage_controller(kCp, d, dp);
    const double g = dp.pilot_gain_floor;
    REQUIRE(c.K_theta == Catch::Approx(kCp.K_theta * g));
    REQUIRE(c.K_phi == Catch::Approx(kCp.K_phi * g));
    REQUIRE(c.K_w_pitch == Catch::Approx(kCp.K_w_pitch * g));
    REQUIRE(c.K_w_roll == Catch::Approx(kCp.K_w_roll * g));
    REQUIRE(c.K_wi_yaw == Catch::Approx(kCp.K_wi_yaw * g));
    // The load-time critical-damping guard K_w >= 4*I*K_theta and the ZOH ceiling
    // K_w*dt/I <= 0.5 still hold on the scaled gains (a uniform scale can only
    // improve the ZOH margin for g <= 1).
    REQUIRE(c.K_w_pitch >= 4.0 * kAp.I_pitch * c.K_theta - 1e-6);
    REQUIRE(c.K_w_pitch * kAp.sim_dt / kAp.I_pitch <= 0.5 + 1e-9);
    // K_aoa (stall protection) is left CRISP — the safety net survives a hurt pilot.
    REQUIRE(c.K_aoa == kCp.K_aoa);
}

// Q4 ROUTING: a hit is routed to a component by the extent-normalized body-frame
// impact geometry (+X = right wing, -Z = nose/engine, +Z = tail/structure, center
// = pilot+structure). Deterministic. MUTATION: raw argmax -> wings starve.
TEST_CASE("damage: hits route to the right component by body geometry") {
    combat::DamageParams dp;
    const glm::dquat q{1, 0, 0, 0};  // identity: body == world
    const glm::dvec3 center{0.0};
    const double R = 9.0, ke = 20.0;

    auto route = [&](const glm::dvec3& impact) {
        combat::DamageState d;
        combat::route_damage(d, dp, q, impact, center, R, ke);
        return d;
    };
    // Right wing: +X well beyond route_wing_frac*R.
    { auto d = route({0.6 * R, 0.0, 0.0});
      CHECK(d.wing_right < 1.0); CHECK(d.wing_left == 1.0);
      CHECK(d.engine == 1.0); CHECK(d.pilot == 1.0); }
    // Left wing: -X.
    { auto d = route({-0.6 * R, 0.0, 0.0});
      CHECK(d.wing_left < 1.0); CHECK(d.wing_right == 1.0); }
    // Engine: forward (-Z) on the centerline.
    { auto d = route({0.0, 0.0, -0.6 * R});
      CHECK(d.engine < 1.0); CHECK(d.wing_left == 1.0); CHECK(d.structure == 1.0); }
    // Structure: aft (+Z).
    { auto d = route({0.0, 0.0, 0.6 * R});
      CHECK(d.structure < 1.0); CHECK(d.engine == 1.0); CHECK(d.pilot == 1.0); }
    // Center / cockpit: pilot (35%) + structure (65%), split deterministically.
    { auto d = route({0.0, 0.0, 0.0});
      CHECK(d.pilot < 1.0); CHECK(d.structure < 1.0);
      const double dp_pilot = 1.0 - d.pilot, dp_struct = 1.0 - d.structure;
      CHECK(dp_pilot == Catch::Approx((ke / dp.component_hp) * dp.route_center_pilot));
      CHECK(dp_struct < dp_pilot * 3.0);  // structure got the larger 65% share
      CHECK(dp_struct > dp_pilot);        // ...but is > pilot's 35%
    }
    // Determinism: same hit twice -> same result.
    combat::DamageState a, b;
    combat::route_damage(a, dp, q, {0.6 * R, 0, 0}, center, R, ke);
    combat::route_damage(b, dp, q, {0.6 * R, 0, 0}, center, R, ke);
    CHECK(a.wing_right == b.wing_right);

    // Routing is in the BODY frame, so it follows the plane's ATTITUDE — a hit
    // from the plane's own right always damages the RIGHT wing, however it is
    // rolled. MUTATION: use `orientation` instead of `inverse(orientation)` ->
    // this mis-routes on any rolled/inverted plane (identity passes it blind).
    for (double roll : {0.5, 3.14159, -1.2}) {
        const glm::dquat rq =
            glm::angleAxis(roll, glm::dvec3{0, 0, -1});  // roll about the nose
        const glm::dvec3 body_right = rq * glm::dvec3{1, 0, 0};  // world dir of +X
        combat::DamageState d;
        combat::route_damage(d, dp, rq, center + body_right * (0.6 * R), center, R,
                             ke);
        CHECK(d.wing_right < 1.0);   // the plane's own right wing, whatever the roll
        CHECK(d.wing_left == 1.0);
    }
}

// Q5 DEATH: a broken wing / dead pilot / dead structure is a KILL; a dead ENGINE
// is NOT (you glide). MUTATION: kill on engine==0 -> engine damage instakills.
TEST_CASE("damage: death only on pilot/structure/wing loss, not a dead engine") {
    combat::DamageState eng; eng.engine = 0.0;
    REQUIRE_FALSE(combat::is_dead(eng));   // dead engine -> glide, not death
    combat::DamageState pil; pil.pilot = 0.0;
    REQUIRE(combat::is_dead(pil));
    combat::DamageState str; str.structure = 0.0;
    REQUIRE(combat::is_dead(str));
    combat::DamageState wl; wl.wing_left = 0.0;
    REQUIRE(combat::is_dead(wl));          // a wing broke OFF
    combat::DamageState wr; wr.wing_right = 0.0;
    REQUIRE(combat::is_dead(wr));
}

// A DAMAGED plane genuinely flies differently — the complement of the firewall
// test. Feed a nominal vs a damaged airframe the SAME stick from the SAME state
// and require the trajectories DIVERGE (else "damage changes flight feel" is a
// no-op). MUTATION: apply_damage_aircraft returns base -> no divergence.
TEST_CASE("damage: a damaged airframe diverges from the nominal one") {
    sim::SimState s;
    s.position = {kAp.R + 2000.0, 0.0, 0.0};
    s.orientation = glm::dquat{1, 0, 0, 0};
    s.velocity = {0.0, 0.0, -140.0};
    s.last_vhat = glm::normalize(s.velocity);
    s.throttle = 1.0;

    combat::DamageParams dp;
    combat::DamageState d; d.engine = 0.3; d.wing_left = 0.3; d.structure = 0.5;
    const sim::AircraftParams a_dmg = combat::apply_damage_aircraft(kAp, d, dp);

    sim::Inputs in; in.throttle = 1.0f; in.pitch = 0.3f;
    sim::SimState nom = s, dmg = s;
    for (int i = 0; i < 240; ++i) {  // 2 s
        nom = sim::step(nom, in, kAp, nullptr, kAp.sim_dt);
        dmg = sim::step(dmg, in, a_dmg, nullptr, kAp.sim_dt);
    }
    // Less thrust + less lift + more drag -> a materially different trajectory.
    REQUIRE(glm::length(dmg.position - nom.position) > 10.0);
    REQUIRE(glm::length(dmg.velocity) < glm::length(nom.velocity));  // slower (drag+thrust)
}
