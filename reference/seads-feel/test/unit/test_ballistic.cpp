// Section 1 — the ballistic point (SPEC §15.1).
// Gate: orbits/falls correctly; §6.1 asserts green; no fixed down axis.
// Since Section 2 the plant carries aero forces; these tests fly a
// zero-aero airframe (S = 0, T_max = 0, c = damp = 0) so every ballistic
// claim — and the applied-impulse-radial tripwire in step() — stays exact.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <random>
#include <vector>

#include "sim/aero.h"
#include "sim/invariants.h"
#include "sim/state.h"
#include "sim/step.h"
#include "sim/world.h"

// The gate's "asserts green" claim is vacuous if the §6.1 invariants are
// compiled out — refuse to be built without them.
#ifdef NDEBUG
#error \
    "SEADS gate requires an assert-live build (SPEC 6.1); configure with CMAKE_BUILD_TYPE=Debug"
#endif

namespace {

constexpr double kDt = 1.0 / 120.0;  // fixed sim tick (SPEC §7)

// R = 15 km, g = 9.81; no wing, no engine, no control authority. mass and
// inertias are nonzero only to keep the force->accel divisions defined.
sim::AircraftParams ballistic_params() {
    sim::AircraftParams p;
    p.R = 15000.0;
    p.g = 9.81;
    p.rho = 1.0;
    p.sim_dt = kDt;
    p.mass = 1.0;
    p.I_pitch = p.I_yaw = p.I_roll = 1.0;
    p.v_full = 1.0;
    p.v_redline = 2.0;
    p.min_frac = 1.0;
    p.v_dir_eps = 1.0;
    return p;
}

const sim::AircraftParams kWorld = ballistic_params();

// Deterministic scatter of unit directions (fixed seed — no flaky tests).
std::vector<glm::dvec3> scattered_directions(int n) {
    std::mt19937 rng(20260703);
    std::uniform_real_distribution<double> dist(-1.0, 1.0);
    std::vector<glm::dvec3> dirs;
    while (static_cast<int>(dirs.size()) < n) {
        glm::dvec3 v{dist(rng), dist(rng), dist(rng)};
        const double len = glm::length(v);
        if (len > 0.1 && len < 1.0) dirs.push_back(v / len);
    }
    return dirs;
}

sim::SimState make_state(glm::dvec3 pos, glm::dvec3 vel) {
    sim::SimState s;
    s.position = pos;
    s.velocity = vel;
    return s;
}

}  // namespace

TEST_CASE("gravity points at the planet center from every position") {
    // Canonical axes — a fixed -Y (or any fixed axis) fails five of six.
    const double r = kWorld.R + 1500.0;
    struct Case {
        glm::dvec3 pos, expected_gravity_dir;
    };
    const Case cases[] = {
        {{+r, 0, 0}, {-1, 0, 0}}, {{-r, 0, 0}, {+1, 0, 0}},
        {{0, +r, 0}, {0, -1, 0}}, {{0, -r, 0}, {0, +1, 0}},  // poles included
        {{0, 0, +r}, {0, 0, -1}}, {{0, 0, -r}, {0, 0, +1}},
    };
    for (const auto& c : cases) {
        const glm::dvec3 gd = sim::gravity_dir(c.pos);
        CHECK(glm::length(gd - c.expected_gravity_dir) < 1e-12);
    }

    // ...and from a scatter of positions at varied radii.
    for (const auto& dir : scattered_directions(100)) {
        for (double radius : {kWorld.R * 0.5, kWorld.R, kWorld.R + 3000.0}) {
            const glm::dvec3 pos = dir * radius;
            CHECK(glm::dot(sim::gravity_dir(pos), -glm::normalize(pos)) >
                  1.0 - 1e-12);
            CHECK(sim::altitude(pos, kWorld) ==
                  Catch::Approx(radius - kWorld.R).margin(1e-6));
        }
    }
}

TEST_CASE(
    "integrator is semi-implicit Euler (position uses the NEW velocity)") {
    // One tick from rest: semi-implicit gives dx = g*dt^2; explicit Euler
    // gives dx = 0. This is the integrator-order regression tripwire.
    // (Double-precision state is what makes this observable at all: the
    // 6.8e-4 m drop is below float ulp at 17 km.)
    const glm::dvec3 p0{kWorld.R + 2000.0, 0.0, 0.0};
    const sim::SimState s1 =
        sim::step(make_state(p0, {0, 0, 0}), {}, kWorld, nullptr, kDt);

    const double expected_drop = kWorld.g * kDt * kDt;
    // margin, not a tight epsilon: the drop is a catastrophically cancelled
    // ~6.8e-4 at |p| = 17 km (half-ulp there is 1.8e-12). Explicit Euler
    // yields exactly 0, so the tripwire keeps full power at this tolerance.
    CHECK(p0.x - s1.position.x == Catch::Approx(expected_drop).margin(1e-9));
    CHECK(s1.position.y == 0.0);
    CHECK(s1.position.z == 0.0);
    CHECK(-s1.velocity.x == Catch::Approx(kWorld.g * kDt).epsilon(1e-12));
}

TEST_CASE("a point at rest falls radially, from any position (no fixed down)") {
    // Drop from rest at scattered orientations of the start position: the
    // fall must track -local_up(start) everywhere. A hardcoded down axis
    // cannot pass this from more than one direction.
    auto dirs = scattered_directions(12);
    dirs.insert(
        dirs.end(),
        {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}});
    for (const auto& dir : dirs) {
        sim::SimState s = make_state(dir * (kWorld.R + 2000.0), {0, 0, 0});
        const glm::dvec3 start = s.position;
        const int ticks = 120;  // 1 s
        for (int i = 0; i < ticks; ++i) s = sim::step(s, {}, kWorld, nullptr, kDt);

        const glm::dvec3 disp = s.position - start;
        const double dropped = glm::length(disp);
        // Direction: radially inward.
        CHECK(glm::dot(disp / dropped, -glm::normalize(start)) > 1.0 - 1e-12);
        // Magnitude: ~ 1/2 g t^2 (semi-implicit offset ~ 1/2 g dt t is
        // inside the 2% tolerance at t = 1 s).
        const double t = ticks * kDt;
        CHECK(dropped == Catch::Approx(0.5 * kWorld.g * t * t).epsilon(0.02));
        // Speed: ~ g t.
        CHECK(glm::length(s.velocity) ==
              Catch::Approx(kWorld.g * t).epsilon(0.01));
    }
}

TEST_CASE(
    "tangential launch at orbital speed holds radius, plane, and closes") {
    // Constant-magnitude central gravity: circular orbit at radius r needs
    // v = sqrt(g*r). The checks divide the labor: radius-hold + closure
    // kill tangent-plane integration (a flat trajectory's |p| grows as
    // sqrt(r^2 + s^2)); the plane check kills out-of-plane force (any
    // fixed-axis gravity component). Run in an oblique orbital plane on
    // purpose: axis-aligned planes can hide fixed-axis bugs.
    const double r = kWorld.R + 2000.0;
    const glm::dvec3 up = glm::normalize(glm::dvec3{0.30, -0.70, 0.65});
    const glm::dvec3 tangent =
        glm::normalize(glm::cross(glm::dvec3{0.20, 0.90, -0.40}, up));
    const glm::dvec3 p0 = up * r;
    const double v_orbit = std::sqrt(kWorld.g * r);
    const glm::dvec3 v0 = tangent * v_orbit;
    const glm::dvec3 plane_normal = glm::normalize(glm::cross(p0, v0));

    const double period = 2.0 * glm::pi<double>() * std::sqrt(r / kWorld.g);
    const int ticks = static_cast<int>(period / kDt) + 1;

    sim::SimState s = make_state(p0, v0);
    double max_radius_err = 0.0;
    double max_plane_err = 0.0;
    for (int i = 0; i < ticks; ++i) {
        s = sim::step(s, {}, kWorld, nullptr, kDt);
        max_radius_err =
            std::max(max_radius_err, std::abs(glm::length(s.position) - r));
        max_plane_err = std::max(max_plane_err,
                                 std::abs(glm::dot(s.position, plane_normal)));
    }

    CAPTURE(max_radius_err, max_plane_err, glm::length(s.position - p0));
    // Semi-implicit Euler wobble at omega*dt ~ 2e-4 predicts ~2 m radial
    // oscillation; the plane error is pure fp noise in a central field.
    CHECK(max_radius_err < 10.0);  // metres, of r = 17 km
    CHECK(max_plane_err < 1e-3);   // metres out of plane over a full lap
    CHECK(glm::length(s.position - p0) < 100.0);  // lap closure, ~107 km flown
    CHECK(sim::is_finite(s.position));
    CHECK(sim::is_finite(s.velocity));
}

TEST_CASE("sub-orbital tangential launch falls; super-orbital climbs") {
    const double r = kWorld.R + 2000.0;
    const glm::dvec3 p0{0.0, r, 0.0};  // start over a pole, on purpose
    const double v_orbit = std::sqrt(kWorld.g * r);

    auto run = [&](double speed_factor) {
        sim::SimState s = make_state(p0, {speed_factor * v_orbit, 0.0, 0.0});
        for (int i = 0; i < 120 * 30; ++i) s = sim::step(s, {}, kWorld, nullptr, kDt);
        return glm::length(s.position);
    };

    CHECK(run(0.7) < r - 500.0);  // too slow: falls toward center
    CHECK(run(1.3) > r + 500.0);  // too fast: climbs away
}

TEST_CASE("quaternion integration is body-frame Hamilton order, correct sign") {
    // Red-team P1: from an identity start with constant omega, q and omega
    // commute — order and sign errors are mathematically unobservable. Pin
    // them with a NON-identity start: q0 = 90 deg about world Y, omega about
    // body X. Body-frame convention demands q(t) = q0 * exp(t/2 * omega);
    // the world-frame mutation gives exp(t/2 * omega) * q0, the sign flip
    // gives q0 * exp(-t/2 * omega) — both land far from q_expected.
    const glm::dquat q0 =
        glm::angleAxis(glm::half_pi<double>(), glm::dvec3{0, 1, 0});
    const double w = 0.5;  // rad/s about body X
    sim::SimState s = make_state({kWorld.R + 2000.0, 0, 0}, {0, 0, -100.0});
    s.orientation = q0;
    s.angular_vel = {w, 0, 0};

    const int ticks = 120;  // 1 s
    for (int i = 0; i < ticks; ++i) s = sim::step(s, {}, kWorld, nullptr, kDt);

    const double t = ticks * kDt;
    const glm::dquat q_expected =
        q0 * glm::angleAxis(w * t, glm::dvec3{1, 0, 0});
    // First-order-integrator phase error at omega*dt ~ 4e-3 is ~1e-6 rad;
    // the mutations miss by ~0.1..1.0 in |dot|.
    CHECK(std::abs(glm::dot(s.orientation, q_expected)) > 1.0 - 1e-9);
}

TEST_CASE(
    "rotation is semi-implicit: orientation uses the NEW angular velocity "
    "(M07)") {
    // The constant-omega Hamilton test above CANNOT see the rotational
    // integrator-order fork: with no torque, next.angular_vel ==
    // state.angular_vel, so sourcing the orientation update from the OLD omega
    // (explicit Euler for rotation) is bit-identical there. Drive a nonzero
    // net torque so next.angular_vel != state.angular_vel over one tick, then
    // check the post-tick orientation against the closed form built with the
    // NEW omega — the semi-implicit source step.cpp must use.
    //
    // Authority-only airframe: q_att_floor gives a nonzero Q at rest (no
    // speed needed), a pitch input gives a pitch-up torque, and damping on a
    // seeded angular_vel adds a second torque contribution — so the new omega
    // separates from the old on ALL three axes tested.
    sim::AircraftParams p = ballistic_params();
    p.q_att_floor = 100.0;  // [Pa] nonzero Q at rest -> nonzero authority
    p.c_pitch = 2.0;
    p.c_yaw = 1.5;
    p.c_roll = 1.0;
    p.damp_pitch = 0.4;
    p.damp_yaw = 0.4;
    p.damp_roll = 0.4;
    p.I_pitch = 1.3;  // non-unit inertias: a per-axis I mismatch is not hidden
    p.I_yaw = 0.9;
    p.I_roll = 1.1;
    // v_full/v_redline/min_frac from ballistic_params keep delta_max_eff == 1
    // at rest (speed 0 <= v_full = 1), so the torque is the clean
    // c*Q*Input - damp*Q*omega.

    // NON-IDENTITY start (Section-1 lesson: q0 and omega commute along an
    // identity-start trajectory, hiding order/sign errors) — 90 deg about
    // world Y, same discipline as the Hamilton test above.
    const glm::dquat q0 =
        glm::angleAxis(glm::half_pi<double>(), glm::dvec3{0, 1, 0});
    sim::SimState s = make_state({kWorld.R + 2000.0, 0, 0}, {0, 0, 0});
    s.orientation = q0;
    s.angular_vel = {0.10, -0.07, 0.05};  // seed all three so damping bites

    sim::Inputs in;
    in.pitch = 0.6f;
    in.yaw = -0.5f;
    in.roll = 0.4f;

    const sim::SimState s1 = sim::step(s, in, p, nullptr, kDt);

    // Oracle: replicate step.cpp's torque + omega update (this is the omega
    // COMPUTATION, which the M07 mutant does not touch), then build the
    // quaternion from that NEW omega. The mutant sources the quaternion from
    // the OLD s.angular_vel instead.
    const double Q = sim::q_eff(sim::q_dyn(p.rho, 0.0), p);  // = q_att_floor
    const glm::dvec3 tau{
        p.c_pitch * Q * static_cast<double>(in.pitch) -
            p.damp_pitch * Q * s.angular_vel.x,
        p.c_yaw * Q * static_cast<double>(in.yaw) -
            p.damp_yaw * Q * s.angular_vel.y,
        p.c_roll * Q * static_cast<double>(in.roll) -
            p.damp_roll * Q * s.angular_vel.z,
    };
    const glm::dvec3 omega_new =
        s.angular_vel +
        kDt * glm::dvec3{tau.x / p.I_pitch, tau.y / p.I_yaw, tau.z / p.I_roll};

    // The NEW omega must actually differ from the OLD (else the mutant is
    // invisible, exactly the constant-omega blind spot).
    REQUIRE(glm::length(omega_new - s.angular_vel) > 1e-6);

    const glm::dquat omega_q{0.0, omega_new.x, omega_new.y, omega_new.z};
    const glm::dquat q_expected =
        glm::normalize(q0 + 0.5 * kDt * (q0 * omega_q));

    // Bit-tight: this is a one-tick, in-double, first-principles reconstruction
    // of the shipped update — no integration error accumulates.
    CHECK(std::abs(glm::dot(s1.orientation, q_expected)) > 1.0 - 1e-14);

    // Cross-check the discriminating power: the OLD-omega (explicit Euler for
    // rotation) quaternion — what the M07 mutant produces — is measurably far
    // from the shipped one. This is the exact separation the mutation-verify
    // exercises.
    const glm::dquat omega_q_old{0.0, s.angular_vel.x, s.angular_vel.y,
                                 s.angular_vel.z};
    const glm::dquat q_explicit =
        glm::normalize(q0 + 0.5 * kDt * (q0 * omega_q_old));
    CHECK(std::abs(glm::dot(s1.orientation, q_explicit)) < 1.0 - 1e-9);
}

TEST_CASE("orientation stays unit-norm over many ticks of tumbling") {
    sim::SimState s = make_state({kWorld.R + 2000.0, 0, 0}, {0, 0, -100.0});
    s.angular_vel = {0.5, -0.3, 0.2};  // rad/s tumble, all three axes
    const glm::dquat q0 = s.orientation;
    for (int i = 0; i < 120 * 60; ++i) {
        s = sim::step(s, {}, kWorld, nullptr, kDt);
        REQUIRE(std::abs(glm::length(s.orientation) - 1.0) < 1e-12);
    }
    REQUIRE(sim::is_finite(s.orientation));
    // The integration is not a no-op: the body actually rotated.
    REQUIRE(std::abs(glm::dot(s.orientation, q0)) < 0.999);
}
