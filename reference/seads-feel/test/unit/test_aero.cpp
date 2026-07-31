// Section 2 — the flyable plant (SPEC §7, §15.2).
// T1 units: Cl(alpha) incl. the stall cap, drag ~ v^2, thrust along -Z,
// lift frame/sign, compression ramp, q_att_floor, the v-hat guard — plus
// the behavioral gate claims: stall sinks, zoom-climb trades honestly.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "config/load_aircraft.h"
#include "sim/aero.h"
#include "sim/invariants.h"
#include "sim/state.h"
#include "sim/step.h"
#include "sim/world.h"

#ifdef NDEBUG
#error \
    "SEADS gate requires an assert-live build (SPEC 6.1); configure with CMAKE_BUILD_TYPE=Debug"
#endif

namespace {

constexpr double kPi = 3.14159265358979323846;

const sim::AircraftParams kP =
    cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");

// Level attitude at altitude over the +X pole: body up (+Y_b) -> world +X
// (the local up there), nose (-Z_b) -> world -Z. Columns are the images of
// the body axes; X_b = Y_b x Z_b = -Y_w keeps it right-handed.
sim::SimState level_state_over_x(double alt, double speed) {
    sim::SimState s;
    s.position = {kP.R + alt, 0.0, 0.0};
    const glm::dmat3 m{glm::dvec3{0.0, -1.0, 0.0},  // body X -> world -Y
                       glm::dvec3{1.0, 0.0, 0.0},   // body Y (up) -> world +X
                       glm::dvec3{0.0, 0.0, 1.0}};  // body Z -> world +Z
    s.orientation = glm::normalize(glm::quat_cast(m));
    s.velocity = speed * glm::dvec3{0.0, 0.0, -1.0};  // along the nose
    s.last_vhat = {0.0, 0.0, -1.0};
    return s;
}

}  // namespace

TEST_CASE("aircraft.toml loads, and its numbers hit the SPEC 13 anchors") {
    // Loader strictness is exercised by the fact this didn't throw; anchor
    // the derived quantities the spec calls out so param drift is loud.
    CHECK(kP.R == 15000.0);
    CHECK(kP.mass == 3000.0);
    CHECK(kP.sim_dt == Catch::Approx(1.0 / 120.0).epsilon(1e-9));

    // Stall speed sqrt(2mg / (rho S Cl_max)) ~ 45 m/s (SPEC §13; §7 tune
    // 2026-07-05 raised Cl_max 1.4->1.8, dropping the corner/stall speed from
    // ~51 to ~45.2 m/s — the intended tighter-carve / lower-corner effect).
    const double v_stall =
        std::sqrt(2.0 * kP.mass * kP.g / (kP.rho * kP.S * kP.Cl_max));
    CHECK(v_stall == Catch::Approx(45.2).margin(1.0));

    // T_max (v5 rung D, Chad 2026-07-23 arcade energy ruling): the
    // thrust=drag-at-750km/h sizing story that pinned the SUPERSEDED 9000 N
    // envelope no longer applies -- T_max is now a direct ruling, not a
    // derived anchor. Pin it exactly, and re-derive the drag-consistency leg
    // against the NEW k_induced (0.05 -> 0.015) via the airframe's absolute
    // MINIMUM drag D_min = 2*W*sqrt(Cd0*k_induced) (the Cl that minimizes
    // Cd0 + k*Cl^2 per unit lift, i.e. d/dCl = 0 -> Cl* = sqrt(Cd0/k), D_min
    // = q*S*(Cd0+k*Cl*^2) = 2*W*sqrt(Cd0*k) after substituting q*S*Cl* =
    // W/Cl* -- the SAME quantity MB-atm's service-ceiling anchor uses, kept
    // as ONE formula, not two): T_max must clear D_min with real headroom
    // (an arcade engine that couldn't even beat its own best-case drag would
    // never climb). Measured: D_min = 1139.6 N (matches the MB-atm anchor's
    // own printout, 1139.8), T_max/D_min ~ 15.8x.
    CHECK(kP.T_max == 18000.0);
    const double D_min =
        2.0 * kP.mass * kP.g * std::sqrt(kP.Cd0 * kP.k_induced);
    CHECK(kP.T_max > 10.0 * D_min);

    // Floor crossover sqrt(2 q_att_floor / rho) ~ 24 m/s (HARNESS AT-18 note).
    CHECK(std::sqrt(2.0 * kP.q_att_floor / kP.rho) ==
          Catch::Approx(24.0).margin(1.0));
}

TEST_CASE("Cl(alpha) is linear then hard-capped at +/-Cl_max") {
    CHECK(sim::lift_coeff(0.0, kP) == 0.0);
    CHECK(sim::lift_coeff(0.1, kP) == Catch::Approx(0.5));
    CHECK(sim::lift_coeff(-0.1, kP) == Catch::Approx(-0.5));
    CHECK(sim::lift_coeff(kP.Cl_max / kP.Cl_alpha, kP) ==
          Catch::Approx(kP.Cl_max));
    CHECK(sim::lift_coeff(0.5, kP) == kP.Cl_max);    // past stall: capped
    CHECK(sim::lift_coeff(-0.5, kP) == -kP.Cl_max);  // inverted stall too
    CHECK(sim::lift_coeff(1.5, kP) == kP.Cl_max);  // stays capped, no fold-back
}

TEST_CASE("load_factor is q*S*Cl/(m*g), signed, and matches the controller") {
    // The single-source true n (SPEC §16 CQ1). Independent literal math here
    // pins the moved expression regardless of the controller golden (S2: a
    // golden only pins the code that recorded it).
    const double V = 150.0;
    const double a = 0.08;  // below the stall cap
    const double q = 0.5 * kP.rho * V * V;
    const double expect = q * kP.S * (kP.Cl_alpha * a) / (kP.mass * kP.g);
    CHECK(sim::load_factor(a, V, 0.0, 0.0, kP) ==
          Catch::Approx(expect).epsilon(1e-12));

    // Zero AoA -> zero lift -> zero n. Inverted AoA -> negative n (signed
    // credit, never |Cl|). Level cruise sits noticeably below 1 g on R = 15 km
    // (SPEC §13: ~0.81 g) but that is the balance, not this raw lift number.
    CHECK(sim::load_factor(0.0, V, 0.0, 0.0, kP) == 0.0);
    CHECK(sim::load_factor(-a, V, 0.0, 0.0, kP) ==
          Catch::Approx(-expect).epsilon(1e-12));

    // The stall cap folds into n: past-stall AoA saturates at q*S*Cl_max/(mg).
    const double n_cap = q * kP.S * kP.Cl_max / (kP.mass * kP.g);
    CHECK(sim::load_factor(0.5, V, 0.0, 0.0, kP) ==
          Catch::Approx(n_cap).epsilon(1e-12));

    // n scales as V^2 at fixed AoA (q ~ V^2).
    CHECK(sim::load_factor(a, 2.0 * V, 0.0, 0.0, kP) ==
          Catch::Approx(4.0 * expect).epsilon(1e-12));
}

TEST_CASE(
    "compression ramp: full below v_full, linear to min_frac at redline, "
    "clamped") {
    CHECK(sim::delta_max_eff(0.0, kP) == 1.0);
    CHECK(sim::delta_max_eff(kP.v_full, kP) == 1.0);
    const double mid = 0.5 * (kP.v_full + kP.v_redline);
    CHECK(sim::delta_max_eff(mid, kP) ==
          Catch::Approx(0.5 * (1.0 + kP.min_frac)));
    CHECK(sim::delta_max_eff(kP.v_redline, kP) == kP.min_frac);
    CHECK(sim::delta_max_eff(kP.v_redline * 2.0, kP) == kP.min_frac);
}

TEST_CASE("q_eff floors at q_att_floor and tracks q above it") {
    CHECK(sim::q_eff(0.0, kP) == kP.q_att_floor);
    CHECK(sim::q_eff(kP.q_att_floor * 0.5, kP) == kP.q_att_floor);
    CHECK(sim::q_eff(kP.q_att_floor * 3.0, kP) == kP.q_att_floor * 3.0);
}

TEST_CASE("thrust acts along body -Z, scaled by throttle, from any attitude") {
    // From rest (q = 0: no lift, no drag) over a pole, arbitrary attitude,
    // engine already spooled (state.throttle = command, so the slew holds):
    // one tick must apply exactly dt * (g*(-up) + T_max*throttle/m * nose).
    const glm::dquat q0 =
        glm::angleAxis(0.7, glm::normalize(glm::dvec3{0.3, 1.0, 0.2}));
    sim::SimState s;
    s.position = {0.0, kP.R + 1500.0, 0.0};
    s.orientation = q0;
    s.throttle = 1.0;

    sim::Inputs in{};
    in.throttle = 1.0f;
    const double dt = kP.sim_dt;
    const sim::SimState s1 = sim::step(s, in, kP, dt);

    const glm::dvec3 up = glm::normalize(s.position);
    const glm::dvec3 nose = q0 * glm::dvec3{0.0, 0.0, -1.0};
    const glm::dvec3 expected_dv =
        dt * (kP.g * (-up) + (kP.T_max / kP.mass) * nose);
    CHECK(glm::length(s1.velocity - expected_dv) < 1e-12);

    // Half throttle -> half thrust impulse.
    s.throttle = 0.5;
    in.throttle = 0.5f;
    const sim::SimState s_half = sim::step(s, in, kP, dt);
    const glm::dvec3 expected_half =
        dt * (kP.g * (-up) + (0.5 * kP.T_max / kP.mass) * nose);
    CHECK(glm::length(s_half.velocity - expected_half) < 1e-12);
    CHECK(s_half.throttle == 0.5);
}

TEST_CASE("throttle slews toward the command in the plant (SPEC 9.8)") {
    // Engine spool is physics: a 0 -> 1 command must ramp at
    // throttle_slew_rate, hit the target exactly, and never overshoot.
    sim::SimState s = level_state_over_x(2000.0, 100.0);
    REQUIRE(s.throttle == 0.0);

    sim::Inputs in{};
    in.throttle = 1.0f;
    const double per_tick = kP.throttle_slew_rate * kP.sim_dt;

    sim::SimState cur = sim::step(s, in, kP, kP.sim_dt);
    CHECK(cur.throttle == Catch::Approx(per_tick).epsilon(1e-12));

    double prev = cur.throttle;
    for (int i = 0; i < 240; ++i) {  // 2 s >> full sweep time
        cur = sim::step(cur, in, kP, kP.sim_dt);
        CHECK(cur.throttle >= prev);  // monotone toward target
        CHECK(cur.throttle <= 1.0);   // never overshoots
        prev = cur.throttle;
    }
    CHECK(cur.throttle == 1.0);  // reaches the target exactly (clamped step)

    // And back down: command 0 from full.
    in.throttle = 0.0f;
    cur = sim::step(cur, in, kP, kP.sim_dt);
    CHECK(cur.throttle == Catch::Approx(1.0 - per_tick).epsilon(1e-12));
}

TEST_CASE(
    "rotational integrator is semi-implicit (quaternion uses NEW omega)") {
    // Mirror of the translational one-tick tripwire: from zero omega with
    // full pitch, the quaternion must already rotate by (tau/I)*dt^2 THIS
    // tick (new omega); the explicit-order mutation (old omega) rotates by
    // exactly zero — a 4.6e-4 rad discriminant vs ~1e-10 integration error.
    sim::SimState s = level_state_over_x(2000.0, 150.0);
    sim::Inputs in{};
    in.pitch = 1.0f;
    const sim::SimState s1 = sim::step(s, in, kP, kP.sim_dt);

    const double tau = kP.c_pitch * sim::q_eff(sim::q_dyn(kP.rho, 150.0), kP) *
                       sim::delta_max_eff(150.0, kP);
    const double expected_angle = tau / kP.I_pitch * kP.sim_dt * kP.sim_dt;
    REQUIRE(expected_angle > 1e-4);  // discriminant far above tolerances

    const double d =
        std::min(1.0, std::abs(glm::dot(s.orientation, s1.orientation)));
    const double angle = 2.0 * std::acos(d);
    CHECK(angle == Catch::Approx(expected_angle).epsilon(1e-6));

    // And the rotation is pitch-up: delta quat axis along body +X.
    const glm::dquat dq = glm::inverse(s.orientation) * s1.orientation;
    CHECK(dq.x > 0.0);
    CHECK(std::abs(dq.x) > 100.0 * (std::abs(dq.y) + std::abs(dq.z)));
}

TEST_CASE("drag opposes velocity and scales as v^2 (alpha = 0 isolates Cd0)") {
    // Velocity exactly along the nose: alpha = 0, Cl = 0, no lift, no
    // induced drag. Gravity is radial (-X here), so the tangential (+Z)
    // velocity change is pure parasitic drag.
    auto tangential_dv = [&](double V) {
        const sim::SimState s = level_state_over_x(2000.0, V);
        const sim::SimState s1 = sim::step(s, {}, kP, kP.sim_dt);
        return (s1.velocity - s.velocity)
            .z;  // drag pushes +Z (opposes -Z motion)
    };
    const double dv_50 = tangential_dv(50.0);
    const double dv_100 = tangential_dv(100.0);
    CHECK(dv_50 > 0.0);
    CHECK(dv_100 / dv_50 == Catch::Approx(4.0).epsilon(1e-12));
    // And the magnitude is q*S*Cd0/m exactly.
    const double q50 = sim::q_dyn(kP.rho, 50.0);
    CHECK(dv_50 == Catch::Approx(kP.sim_dt * q50 * kP.S * kP.Cd0 / kP.mass)
                       .epsilon(1e-12));
}

TEST_CASE(
    "lift is perpendicular to velocity, in the body-vertical plane, "
    "correct sign") {
    // Identity orientation at the +X pole; velocity tilted for alpha = +0.1
    // (nose above velocity): v_b = (0, -sin a, -cos a). Independent literal
    // math here on purpose — this pins the wiring in step(), not aero.h.
    const double a = 0.1;
    const double V = 100.0;
    sim::SimState s;
    s.position = {kP.R + 2000.0, 0.0, 0.0};
    s.velocity = V * glm::dvec3{0.0, -std::sin(a), -std::cos(a)};
    s.last_vhat = glm::normalize(s.velocity);

    const sim::SimState s1 = sim::step(s, {}, kP, kP.sim_dt);

    const glm::dvec3 vhat = glm::normalize(s.velocity);
    const double q = 0.5 * kP.rho * V * V;
    const double Cl = kP.Cl_alpha * a;  // 0.5, below the cap
    const glm::dvec3 lift_dir{0.0, std::cos(a),
                              -std::sin(a)};  // perp to v, "up-ish"
    const double Cd = kP.Cd0 + kP.k_induced * Cl * Cl;
    const glm::dvec3 expected_accel =
        kP.g * glm::dvec3{-1.0, 0.0, 0.0} +
        (q * kP.S * Cl * lift_dir - q * kP.S * Cd * vhat) / kP.mass;
    const glm::dvec3 dv = s1.velocity - s.velocity;
    CHECK(glm::length(dv - kP.sim_dt * expected_accel) <
          1e-12 * glm::length(kP.sim_dt * expected_accel));

    // Sanity on the frame claims themselves.
    CHECK(std::abs(glm::dot(lift_dir, vhat)) < 1e-15);     // perpendicular
    CHECK(glm::dot(lift_dir, glm::dvec3{0, 1, 0}) > 0.9);  // toward body up
}

// ---------------------------------------------------------------------------
// S-wvane (SPEC §0, Chad RULED 2026-07-11 "forget the crabbing, I don't want
// the cocking — snap to the centre of the mouse aim every time"): fuselage
// side-force from sideslip + its drag price. F = -q*S*Cy_beta*sin(b)cos(b)
// along side_axis (body_right projected perp to v); Cd += Cd_beta*sin^2(b).
// A crabbed airframe straightens FLAT (tau_beta = 2m/(rho*V*S*Cy_beta)).
// BLIND-INSTRUMENT WALK (the MB-atm lesson, named not assumed): the force
// scales with the SAME q local as lift/drag (altitude thinning inherited);
// load_factor / the HUD G meter are LIFT-only and structurally blind to the
// lateral shove (deliberate scope); AT-12's linear-work gate is blind to the
// side force BY SCOPE (workless, perp to the pre-tick v — the lift argument)
// but DOES see the Cd_beta drag (its mirror carries the term);
// drone/drone.h flies this same plant — its kCoordP beta-null is cooperative
// in sign (the fleet turns ~14% gentler, bound recalibrated in test_drone).
// REAR HEMISPHERE honesty (plan-audit P2-4): for 90 < |b| < 180 the force
// pushes |b| toward 90 (the sign flip below is PINNED) — a q-small geometry;
// this is a shape pin, NOT a "tail-slides recover nose-first" claim.
// Geometry used throughout: identity orientation at the +X pole (nose world
// -Z, body_right +X), crabbed velocity v_b = (sin b, 0, -cos b) => the exact
// side_axis is (cos b, 0, sin b) and every oracle below is closed-form.
// ---------------------------------------------------------------------------
TEST_CASE("S-wvane: sideslip side-force - sign, rate, shape, worklessness") {
    REQUIRE(kP.Cy_beta > 0.0);  // committed table arms the mechanism
    REQUIRE(kP.Cd_beta > 0.0);

    const double V = 140.0;
    const double q = sim::q_dyn(kP.rho, V);
    auto crabbed = [&](double beta_deg) {
        const double b = beta_deg * kPi / 180.0;
        sim::SimState s;
        s.position = {kP.R + 2000.0, 0.0, 0.0};
        s.velocity = V * glm::dvec3{std::sin(b), 0.0, -std::cos(b)};
        s.last_vhat = glm::normalize(s.velocity);
        return s;
    };
    // One-tick velocity differential between the live table and a knob-off
    // copy — every other force expression is IDENTICAL, so the difference
    // isolates the gated term exactly (the MB-lean differential proof shape).
    auto dv_of = [&](const sim::SimState& s, const sim::AircraftParams& a,
                     const sim::AircraftParams& b) {
        return sim::step(s, {}, a, a.sim_dt).velocity -
               sim::step(s, {}, b, b.sim_dt).velocity;
    };
    sim::AircraftParams p_noCy = kP;
    p_noCy.Cy_beta = 0.0;  // isolates the SIDE FORCE (Cd_beta stays live)
    sim::AircraftParams p_noCd = kP;
    p_noCd.Cd_beta = 0.0;  // isolates the DRAG PRICE (Cy_beta stays live)

    SECTION("restoring sign BOTH ways + the exact rate oracle") {
        for (const double bdeg : {9.0, -9.0}) {
            const double b = bdeg * kPi / 180.0;
            const sim::SimState s = crabbed(bdeg);
            const glm::dvec3 dvs = dv_of(s, kP, p_noCy);
            // Restoring: beta > 0 (velocity right of nose) => the force
            // pushes the velocity's body-X component DOWN, and vice versa.
            // Mutation this kills: a flipped side_axis (pro-crab feedback).
            CHECK(dvs.x * b < 0.0);
            // Magnitude: |F|dt/m = q*S*Cy*|sin b cos b|*dt/m, closed form.
            const double expect = kP.sim_dt * q * kP.S * kP.Cy_beta *
                                  std::abs(std::sin(b) * std::cos(b)) /
                                  kP.mass;
            CHECK(glm::length(dvs) == Catch::Approx(expect).epsilon(1e-9));
            // Full direction oracle: -sign(b)*|F|*(cos b, 0, sin b)/m*dt.
            const glm::dvec3 dir{std::cos(b), 0.0, std::sin(b)};
            const glm::dvec3 expect_v =
                -(kP.sim_dt * q * kP.S * kP.Cy_beta * std::sin(b) *
                  std::cos(b) / kP.mass) *
                dir;
            CHECK(glm::length(dvs - expect_v) < 1e-9 * glm::length(expect_v));
        }
    }

    SECTION("shape pins: the sin(b)cos(b) law, off the symmetric points") {
        // beta = 45: sin*cos = 0.5 EXACTLY — a bare-linear-beta mutant
        // (F ~ b) predicts 0.785*q*S*Cy, distinct by 57%. (The MB-flaps
        // P1-1 lesson: pin where the candidate mutants SEPARATE.)
        const glm::dvec3 dv45 = dv_of(crabbed(45.0), kP, p_noCy);
        CHECK(glm::length(dv45) ==
              Catch::Approx(kP.sim_dt * q * kP.S * kP.Cy_beta * 0.5 / kP.mass)
                  .epsilon(1e-9));
        // beta = 135 (rear hemisphere): sin*cos = -0.5 — the force REVERSES
        // (pushes |b| toward 90, plan-audit P2-4) and the x-component flips
        // sign vs the 45-deg case. Kills an abs()/front-hemisphere-only
        // mutant that every |b| < 90 leg is blind to (the S4a lesson).
        const glm::dvec3 dv135 = dv_of(crabbed(135.0), kP, p_noCy);
        CHECK(dv45.x < 0.0);
        CHECK(dv135.x > 0.0);
        CHECK(glm::length(dv135) ==
              Catch::Approx(kP.sim_dt * q * kP.S * kP.Cy_beta * 0.5 / kP.mass)
                  .epsilon(1e-9));
        // beta = 90 exactly: velocity along body X — the shared lift-axis
        // guard closes (lift_axis_len ~ 0) and the side force is
        // STRUCTURALLY zero there, which is also the shape's own zero
        // (sin*cos = 0): the guard's cutoff and the law's node coincide.
        const glm::dvec3 dv90 = dv_of(crabbed(90.0), kP, p_noCy);
        CHECK(glm::length(dv90) == 0.0);
    }

    SECTION("workless: the side impulse is perpendicular to the pre-tick v") {
        const sim::SimState s = crabbed(9.0);
        const glm::dvec3 dvs = dv_of(s, kP, p_noCy);
        // RELATIVE bound (plan-audit P2-2): a literal == 0.0 fails on
        // triple-product fp noise; the honest pin is perpendicularity to
        // machine precision. End-to-end this is what keeps AT-12's energy
        // gate at rel ~1e-13 with the mechanism LIVE.
        CHECK(std::abs(glm::dot(dvs, s.velocity)) <
              1e-12 * glm::length(dvs) * glm::length(s.velocity));
    }

    SECTION("the drag price: Cd_beta*sin^2(b), along -vhat, closed form") {
        const double b = 9.0 * kPi / 180.0;
        const sim::SimState s = crabbed(9.0);
        const glm::dvec3 dvd = dv_of(s, kP, p_noCd);
        const glm::dvec3 vhat = glm::normalize(s.velocity);
        const double sb = std::sin(b);
        const double expect =
            -kP.sim_dt * q * kP.S * kP.Cd_beta * sb * sb / kP.mass;
        CHECK(glm::dot(dvd, vhat) == Catch::Approx(expect).epsilon(1e-9));
        // Pure drag: no lateral leakage (the side force was live in BOTH
        // arms and cancelled exactly). Bound at 1e-11 (diff red-team P2-1):
        // the arm-difference add-rounding noise floor is ~5e-15 — a 1e-12
        // bound sat ~3.5x above it with zero worst-case margin (the S4a
        // toolchain-luck class); a real drag-along-nose mutant leaks
        // sin(9 deg) ~ 0.16 of |expect|, seven orders above this bound.
        CHECK(glm::length(dvd - glm::dot(dvd, vhat) * vhat) <
              1e-11 * std::abs(expect));
    }

    SECTION("knob-off contains no residue (bit-level differential)") {
        // Both knobs 0 on a CRABBED state: the one-tick dv equals the
        // hand-composed PRE-S-wvane force sum (alpha = 0 here, so lift and
        // induced drag vanish and the oracle is exact: gravity + Cd0 drag).
        sim::AircraftParams p_off = kP;
        p_off.Cy_beta = 0.0;
        p_off.Cd_beta = 0.0;
        const sim::SimState s = crabbed(9.0);
        const sim::SimState s1 = sim::step(s, {}, p_off, p_off.sim_dt);
        const glm::dvec3 vhat = glm::normalize(s.velocity);
        const glm::dvec3 expect_dv =
            p_off.sim_dt * (p_off.g * glm::dvec3{-1.0, 0.0, 0.0} -
                            (q * p_off.S * p_off.Cd0 / p_off.mass) * vhat);
        CHECK(glm::length((s1.velocity - s.velocity) - expect_dv) <
              1e-12 * glm::length(expect_dv));
    }

    SECTION("the felt decay: a 9-deg crab bleeds at tau_beta (differential)") {
        // Free-run 0.5 s from the crabbed state, live vs Cy-off. Gravity
        // shifts beta identically in both arms (the confound), so the
        // DIFFERENCE isolates the bleed: expected gap ~ b0*(1 - e^(-t/tau)),
        // tau = 2m/(rho*V*S*Cy) = 1.07 s at V=140 => gap ~ 3.4 deg. Mutation
        // this kills: Cy -> 0 in step (gap collapses to 0).
        constexpr int kBleedTicks = 60;
        auto beta_after = [&](const sim::AircraftParams& p) {
            sim::SimState s = crabbed(9.0);
            for (int i = 0; i < kBleedTicks; ++i)
                s = sim::step(s, {}, p, p.sim_dt);
            return sim::beta_of(
                sim::body_dir_of(s.orientation, glm::normalize(s.velocity)));
        };
        const double tau =
            2.0 * kP.mass / (kP.rho * V * kP.S * kP.Cy_beta);
        // Exponent time derived from the ACTUAL ticks run (diff red-team
        // P3-2: a welded 0.5 s silently absorbs a sim_dt retune inside the
        // 0.4 slop; kBleedTicks*dt keeps oracle and run in lockstep).
        const double expected_gap =
            (9.0 * kPi / 180.0) *
            (1.0 - std::exp(-(kBleedTicks * kP.sim_dt) / tau));
        const double gap = beta_after(p_noCy) - beta_after(kP);
        CHECK(gap == Catch::Approx(expected_gap).epsilon(0.4));
    }
}

TEST_CASE("velocity along body X (pure sideslip) produces zero lift, no NaN") {
    // Identity orientation over the -Y pole: body +X (world +X) is
    // tangential there, so velocity along body X is a legal flight state.
    sim::SimState s;
    s.position = {0.0, -(kP.R + 2000.0), 0.0};
    s.velocity = 60.0 * glm::dvec3{1.0, 0.0, 0.0};  // along body +X exactly
    s.last_vhat = {1.0, 0.0, 0.0};

    const sim::SimState s1 = sim::step(s, {}, kP, kP.sim_dt);
    CHECK(sim::is_finite(s1.velocity));
    // No lift: the only tangential-X force is drag, opposing +X motion.
    CHECK((s1.velocity - s.velocity).x < 0.0);
    // Nothing pushed it out of the X/-Y plane (lift would have).
    CHECK(s1.velocity.z == Catch::Approx(0.0).margin(1e-12));
}

TEST_CASE("v-hat guard: held below v_dir_eps, updated above (SPEC 7)") {
    sim::SimState s;
    s.position = {kP.R + 2000.0, 0.0, 0.0};
    s.velocity = {0.4, 0.0, 0.0};  // below v_dir_eps = 1
    const sim::SimState s1 = sim::step(s, {}, kP, kP.sim_dt);
    CHECK(glm::length(s1.last_vhat - glm::dvec3{0.0, 0.0, -1.0}) < 1e-15);

    sim::SimState s2 = s1;
    s2.velocity = {5.0, 0.0, 0.0};  // above eps: guard must track
    const sim::SimState s3 = sim::step(s2, {}, kP, kP.sim_dt);
    CHECK(glm::length(s3.last_vhat - glm::dvec3{1.0, 0.0, 0.0}) < 1e-15);
}

TEST_CASE("below stall speed the aircraft sinks, full back-stick or not") {
    // 40 m/s < v_stall ~ 51: max available lift q*S*Cl_max < mg, so the
    // radial velocity must go negative and altitude must drop — the stall
    // half of the Section-2 gate ("stall behaves").
    sim::SimState s = level_state_over_x(2000.0, 40.0);
    const double alt0 = sim::altitude(s.position, kP);

    sim::Inputs in{};
    in.pitch = 1.0f;  // haul back as hard as raw mode allows
    double vr_1s = 0.0;
    for (int i = 0; i < 240; ++i) {  // 2 s
        s = sim::step(s, in, kP, kP.sim_dt);
        if (i == 119) vr_1s = glm::dot(s.velocity, glm::normalize(s.position));
    }
    CHECK(vr_1s < -1.0);  // sinking at 1 s despite full pitch-up
    CHECK(sim::altitude(s.position, kP) < alt0 - 5.0);
}

TEST_CASE(
    "zoom climb trades speed for altitude honestly (energy never appears)") {
    // 200 m/s, throttle 0, pull up 2.5 s then neutral: altitude rises,
    // speed falls, and mechanical energy 0.5 v^2 + g*alt only dissipates
    // (drag) — the zoom half of the Section-2 gate.
    sim::SimState s = level_state_over_x(2000.0, 200.0);
    const double alt0 = sim::altitude(s.position, kP);
    auto energy = [&](const sim::SimState& st) {
        return 0.5 * glm::dot(st.velocity, st.velocity) +
               kP.g * sim::altitude(st.position, kP);
    };
    const double e0 = energy(s);

    double e_prev = e0;
    double max_gain_per_tick = 0.0;
    for (int i = 0; i < 600; ++i) {  // 5 s
        sim::Inputs in{};
        if (i < 300) in.pitch = 0.5f;
        s = sim::step(s, in, kP, kP.sim_dt);
        const double e = energy(s);
        max_gain_per_tick = std::max(max_gain_per_tick, e - e_prev);
        e_prev = e;
    }

    CHECK(sim::altitude(s.position, kP) > alt0 + 100.0);  // it climbed
    CHECK(glm::length(s.velocity) < 180.0);               // it paid in speed
    CHECK(e_prev < e0 - 500.0);                           // drag dissipated
    // Per-tick: semi-implicit wobble is << 1 J/kg; anything larger means
    // the plant manufactured energy.
    CHECK(max_gain_per_tick < 1.0);
}

// ===========================================================================
// MB-atm — the atmosphere taper (SPEC §0, 2026-07-08): the SOFT ceiling.
// atm_frac(h) = exp(-max(0, h-taper_alt)^2 / (2 sigma^2)) thins density AND
// thrust together above the fight band. Shape pins against the config-
// recomputed formula (the independent oracle — a code-side shape mutant
// diverges from it); the knob-off arm; and the SERVICE-CEILING design anchor
// (T_max*f(8km) ~ minimum drag — the SPEC-13-anchor style tripwire: a
// T_max/sigma retune that silently moves the ceiling trips loud).
// ===========================================================================
TEST_CASE("MB-atm: taper shape, knob-off arm, and the service-ceiling anchor") {
    // Exactly 1 at and below the taper (the fight band is untouched — this is
    // also why NO golden moves under MB-atm: they all fly below taper_alt).
    CHECK(sim::atm_frac(0.0, kP) == 1.0);
    CHECK(sim::atm_frac(kP.atm_taper_alt, kP) == 1.0);
    CHECK(sim::atm_frac(kP.atm_taper_alt - 1.0, kP) == 1.0);

    // Above: strictly decreasing, matching the formula recomputed here from
    // config (mutation: a linear-falloff or exponential-from-sea-level
    // rewrite diverges at these samples).
    auto expected = [&](double h) {
        const double d = h - kP.atm_taper_alt;
        return std::exp(-d * d /
                        (2.0 * kP.atm_taper_sigma * kP.atm_taper_sigma));
    };
    double prev = 1.0;
    for (double h : {5000.0, 6000.0, 7000.0, 8000.0}) {
        const double f = sim::atm_frac(h, kP);
        CHECK(f == Catch::Approx(expected(h)).epsilon(1e-12));
        CHECK(f < prev);
        prev = f;
    }

    // Knob-off arm: sigma = 0 -> structurally OFF (1 at every altitude).
    sim::AircraftParams off = kP;
    off.atm_taper_sigma = 0.0;
    CHECK(sim::atm_frac(20000.0, off) == 1.0);

    // SERVICE-CEILING anchor (v5 rung D, Chad 2026-07-23 arcade energy
    // ruling): the ceiling is where the lapsed thrust T_max*f(h) meets the
    // airframe's MINIMUM drag D_min = 2*W*sqrt(Cd0*k) (rho-independent — at
    // altitude the plane flies faster for the same q, so D_min is where
    // level flight becomes just-possible; the SAME formula the SPEC-13
    // T_max anchor now uses, kept as one derivation). Under the OLD envelope
    // (T_max=9000) this welded to ~8 km; T_max=18000 does not change D_min
    // (an airframe drag property, untouched this rung) but DOES halve the
    // thrust fraction f the ceiling needs (f = D_min/T_max), which pushes
    // the ceiling to a HIGHER altitude on the SAME Gaussian taper — the
    // arcade engine flies higher before it runs out of thrust. Re-derived to
    // solve for h directly from config (COMPUTED, not a welded 8 km):
    // f(h) = exp(-(h-taper_alt)^2/(2 sigma^2)) = D_min/T_max
    //   => h = taper_alt + taper_sigma*sqrt(2*ln(T_max/D_min))
    // Anchors the DESIGN against any T_max/sigma/Cd0/k retune (recalibrate
    // knowingly, not by accident) at whatever ceiling the CURRENT table
    // implies -- measured h_ceiling ~ 9.5 km here (rose ~1.5 km under this
    // ruling; taper_sigma is Chad's dial if the world design needs the
    // ceiling welded back down near 8 km).
    const double D_min =
        2.0 * kP.mass * kP.g * std::sqrt(kP.Cd0 * kP.k_induced);
    REQUIRE(kP.T_max > D_min);  // premise: thrust can even reach the ceiling
    const double h_ceiling =
        kP.atm_taper_alt +
        kP.atm_taper_sigma * std::sqrt(2.0 * std::log(kP.T_max / D_min));
    // Cross-check: the DERIVED h_ceiling, fed back through the sim's own
    // atm_frac, reproduces T_max*f(h) == D_min (a shape mismatch between
    // this formula and sim::atm_frac's real implementation would diverge
    // here, not just silently mis-anchor the ceiling).
    const double T_at_ceiling = kP.T_max * sim::atm_frac(h_ceiling, kP);
    CHECK(T_at_ceiling == Catch::Approx(D_min).epsilon(1e-6));
    // The design anchor itself: today's table's ceiling sits ~9.5 km, a
    // ~1.5 km rise from the pre-rung-D ~8 km welded anchor.
    CHECK(h_ceiling == Catch::Approx(9500.0).margin(500.0));
}

// MB-atm: the PLANT actually thins — same state and inputs, 2 km vs 7 km:
// lift, drag, and thrust all scale by atm_frac (one tick, force-level
// separations; a mutant that thins q but not thrust — or thrust but not q —
// fails the matching leg). Torque thinning is pinned by AT-18a's 7 km leg.
TEST_CASE("MB-atm: the plant thins lift AND thrust above the taper") {
    // RED-TEAM P1 REWRITE (MB-atm diff red-team): the first version flew
    // velocity exactly along the nose -> alpha = 0 -> Cl = 0 -> the "lift
    // thins" check was 0 == f*0 (and read the wrong axis) — a no-lift-
    // thinning mutant survived the whole suite (the S7-cam "fixture made
    // the new behavior a no-op" class, recurring). Now: a real AoA (0.06
    // rad, below stall) so lift is genuinely nonzero (REQUIRE — the S3
    // "assert the point MOVED" discipline), and EVERY component of the
    // net aero+thrust accel scales by atm_frac (lift along body +Y, the
    // thrust-drag axis along -Z; gravity subtracted exactly, identical
    // direction at both radii). Mutation: sea-level lift at altitude ->
    // the y-component fails by 1/f ~ 2.3x.
    auto one_tick_dv = [&](double alt) {
        sim::SimState s;
        s.position = {kP.R + alt, 0.0, 0.0};
        const double a = 0.06;  // AoA [rad], below stall (0.36)
        s.velocity =
            140.0 * glm::normalize(glm::dvec3{0.0, -std::tan(a), -1.0});
        s.last_vhat = glm::normalize(s.velocity);
        s.orientation = glm::dquat{1.0, 0.0, 0.0, 0.0};
        s.throttle = 1.0;  // engine already spooled
        sim::Inputs in{};
        in.throttle = 1.0f;
        const sim::SimState n = sim::step(s, in, kP, kP.sim_dt);
        return (n.velocity - s.velocity) / kP.sim_dt -
               kP.g * sim::gravity_dir(s.position);  // net aero+thrust accel
    };
    const double f7 = sim::atm_frac(7000.0, kP);
    REQUIRE(f7 < 0.6);  // premise: 7 km is genuinely thin at the shipped table
    const glm::dvec3 a2 = one_tick_dv(2000.0);
    const glm::dvec3 a7 = one_tick_dv(7000.0);
    REQUIRE(std::abs(a2.y) > 1.0);  // lift genuinely nonzero (non-vacuity)
    REQUIRE(std::abs(a2.z) > 1.0);  // thrust-drag axis nonzero
    CHECK(a7.y == Catch::Approx(f7 * a2.y).epsilon(1e-9));  // LIFT thins
    CHECK(a7.z == Catch::Approx(f7 * a2.z).epsilon(1e-9));  // thrust-drag thins
}

// MB-atm (red-team P2-1): the DAMPING torque thins with the atmosphere too —
// tau_d = -damp * q_eff(thinned q) * omega. AT-18a measures from omega = 0
// (damping-free by design) and AT-12 reads linear work only, so a fork that
// damps with sea-level q at altitude (2.3x over-damped up high — "floatier"
// inverted) survived the suite until this leg. One tick, zero inputs, spinning
// state, 2 km vs 7 km at V where the thinned q still clears q_att_floor:
// per-axis delta-omega scales exactly by atm_frac.
TEST_CASE("MB-atm: angular damping thins with the atmosphere") {
    auto one_tick_dw = [&](double alt) {
        sim::SimState s;
        s.position = {kP.R + alt, 0.0, 0.0};
        s.velocity = 150.0 * glm::dvec3{0.0, 0.0, -1.0};
        s.last_vhat = {0.0, 0.0, -1.0};
        s.orientation = glm::dquat{1.0, 0.0, 0.0, 0.0};
        s.angular_vel = {0.5, 0.4, 0.6};
        sim::Inputs in{};  // zero deflection, zero throttle: damping only
        const sim::SimState n = sim::step(s, in, kP, kP.sim_dt);
        return n.angular_vel - s.angular_vel;
    };
    const double f7 = sim::atm_frac(7000.0, kP);
    // Premise: the thinned q still clears the authority floor (else the
    // floor, not the atmosphere, sets the damping and the ratio breaks).
    REQUIRE(f7 * 0.5 * kP.rho * 150.0 * 150.0 > kP.q_att_floor);
    const glm::dvec3 d2 = one_tick_dw(2000.0);
    const glm::dvec3 d7 = one_tick_dw(7000.0);
    for (int i = 0; i < 3; ++i) {
        REQUIRE(std::abs(d2[i]) > 1e-6);  // damping genuinely acting
        CHECK(d7[i] == Catch::Approx(f7 * d2[i]).epsilon(1e-9));
    }
}
