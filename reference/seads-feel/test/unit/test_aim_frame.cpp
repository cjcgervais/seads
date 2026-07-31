// Section 5 — the aim/camera frame (input/aim_frame.h, SPEC §9.1/§9.2).
// The one new caller-side piece the app-wiring adds: a quaternion carrying the
// aim AND the camera-up together. These pin, executably:
//   - mouse signs COMPOSE with AT-0 (dx-right -> yaw-right, dy-down ->
//   pitch-down)
//   - the RAW basis ruling (§9.1): the dx axis is the frame's own up, which
//     stays clean at a zenith aim where a local_up axis would degenerate
//   - transport holonomy (RA8 null-test discipline): a great-circle lap returns
//     to identity AND a banked small circle accumulates enclosed area/R² — the
//     SAME transport §6's AT-14 will grade, de-risked here
//   - reseed seeds camera-up from local_up (not body_up); snap keeps carried up

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <glm/gtc/quaternion.hpp>

#include "config/load_aircraft.h"
#include "control/controller.h"
#include "input/aim_frame.h"
#include "test/harness/injector.h"

#ifdef NDEBUG
#error \
    "SEADS gate requires an assert-live build (SPEC 6.1); configure with CMAKE_BUILD_TYPE=Debug"
#endif

namespace {

const sim::AircraftParams kP =
    cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");

constexpr double kPi = 3.14159265358979323846;
inline double rad(double deg) { return deg * kPi / 180.0; }

struct WorldFrame {
    const char* name;
    glm::dvec3 up;
    glm::dvec3 heading;
};
const WorldFrame kFrames[] = {
    {"pole+X", {1.0, 0.0, 0.0}, {0.0, 0.0, -1.0}},
    {"skew", {3.0, -2.0, 5.0}, {1.0, 1.0, -0.3}},
};

glm::dvec3 nose_of(const sim::SimState& s) {
    return s.orientation * glm::dvec3{0.0, 0.0, -1.0};
}
glm::dvec3 body_right_of(const sim::SimState& s) {
    return s.orientation * glm::dvec3{1.0, 0.0, 0.0};
}
glm::dvec3 body_up_of(const sim::SimState& s) {
    return s.orientation * glm::dvec3{0.0, 1.0, 0.0};
}

}  // namespace

// ---------------------------------------------------------------------------
// Mouse -> aim signs, composed through AT-0's rotation_demand_body. In LEVEL
// flight the frame axes coincide with the body axes, so the mouse->body->omega
// mapping is unambiguous; a banked frame is separated in the transport/reseed
// cases below. Two world frames so no axis aliases a world axis.
// ---------------------------------------------------------------------------
TEST_CASE("aim_frame: mouse signs compose to the SPEC 7 omega signs") {
    for (const auto& f : kFrames) {
        CAPTURE(f.name);
        const sim::SimState s =
            harness::level_state(kP, 140.0, 2000.0, f.up, f.heading);
        const glm::dvec3 local_up = glm::normalize(s.position);

        input::AimFrame af;
        af.reseed(s.orientation, local_up);
        // Reseed put forward on the nose and up on local_up.
        REQUIRE(glm::dot(af.forward(), nose_of(s)) == Catch::Approx(1.0));
        REQUIRE(glm::dot(af.up(), local_up) == Catch::Approx(1.0));

        const double sens = rad(2.0);  // per unit mouse delta, small

        // Mouse RIGHT (dx > 0): aim moves right of the nose -> yaw-right (-y).
        input::AimFrame right_aim = af;
        right_aim.apply_mouse(+1.0, 0.0, sens);
        CHECK(glm::dot(right_aim.forward(), body_right_of(s)) > 0.0);
        const glm::dvec3 dr =
            control::rotation_demand_body(s.orientation, right_aim.forward());
        CHECK(dr.y < 0.0);  // yaw-right (SPEC §7)

        // Mouse DOWN (dy > 0): aim moves below the nose -> pitch-down (-x).
        input::AimFrame down_aim = af;
        down_aim.apply_mouse(0.0, +1.0, sens);
        CHECK(glm::dot(down_aim.forward(), body_up_of(s)) < 0.0);
        const glm::dvec3 dd =
            control::rotation_demand_body(s.orientation, down_aim.forward());
        CHECK(dd.x < 0.0);  // pitch-down (SPEC §7)

        // Mouse UP (dy < 0): the mirror — pitch-up (+x).
        input::AimFrame up_aim = af;
        up_aim.apply_mouse(0.0, -1.0, sens);
        const glm::dvec3 du =
            control::rotation_demand_body(s.orientation, up_aim.forward());
        CHECK(du.x > 0.0);  // pitch-up
    }
}

// ---------------------------------------------------------------------------
// The RAW-basis ruling (SPEC §9.1): the dx rotation axis is the aim frame's
// OWN up, not local_up — because local_up degenerates exactly at the zenith
// aim this representation exists to keep legal. Pin it: with the aim pitched to
// near-vertical (forward ~ local_up), a mouse-yaw about the frame-up moves the
// aim cleanly, while the same rotation about local_up would be a near no-op.
// ---------------------------------------------------------------------------
TEST_CASE("aim_frame: mouse-yaw stays live at a zenith aim (raw frame basis)") {
    const sim::SimState s = harness::level_state(
        kP, 140.0, 2000.0, {1.0, 0.0, 0.0}, {0.0, 0.0, -1.0});
    const glm::dvec3 local_up = glm::normalize(s.position);

    input::AimFrame af;
    af.reseed(s.orientation, local_up);
    // Pitch the aim UP ~88 deg (mouse up = dy < 0) so forward ~ local_up.
    af.apply_mouse(0.0, -1.0, rad(88.0));
    REQUIRE(glm::dot(glm::normalize(af.forward()), local_up) > 0.999);

    const glm::dvec3 before = af.forward();
    input::AimFrame yawed = af;
    yawed.apply_mouse(+1.0, 0.0, rad(5.0));  // frame-up axis: MOVES the aim
    CHECK(glm::length(yawed.forward() - before) > 0.05);

    // The banned local_up axis would barely move a forward that is ~parallel
    // to it — the degeneracy the ruling avoids.
    const glm::dvec3 wrong =
        glm::normalize(glm::angleAxis(-rad(5.0), local_up) * before);
    CHECK(glm::length(wrong - before) < 0.01);
}

// ---------------------------------------------------------------------------
// Transport holonomy (RA8): a great-circle lap of the up vector returns the
// frame to identity (2pi about a fixed axis) — the NULL test — while a banked
// small circle accumulates the enclosed solid angle 2pi(1 - cos theta0). One
// without the other blesses a broken transport.
// ---------------------------------------------------------------------------
TEST_CASE("aim_frame: great-circle up-lap returns to identity (null test)") {
    // up traverses a planar great circle: transport axis is constant, so the
    // composed rotation is exactly 2pi about it -> identity.
    input::AimFrame af;
    af.reseed(glm::dquat{1.0, 0.0, 0.0, 0.0}, glm::dvec3{1.0, 0.0, 0.0});
    const glm::dvec3 f0 = af.forward();
    const glm::dvec3 u0 = af.up();

    const int N = 720;
    glm::dvec3 up_prev{1.0, 0.0, 0.0};
    for (int i = 1; i <= N; ++i) {
        const double phi = 2.0 * kPi * i / N;
        const glm::dvec3 up_cur{std::cos(phi), std::sin(phi), 0.0};  // about +Z
        af.transport(up_prev, up_cur);
        up_prev = up_cur;
    }
    CHECK(glm::length(af.forward() - f0) < 1e-9);
    CHECK(glm::length(af.up() - u0) < 1e-9);
}

TEST_CASE("aim_frame: banked small circle accumulates area/R^2 holonomy") {
    const double theta0 = rad(30.0);  // colatitude from the +Z pole
    input::AimFrame af;
    // Seed forward off the pole so the frame is well-conditioned; up starts on
    // the small circle at phi = 0.
    const glm::dvec3 u_start{std::sin(theta0), 0.0, std::cos(theta0)};
    af.reseed(glm::dquat{1.0, 0.0, 0.0, 0.0}, glm::dvec3{1.0, 0.0, 0.0});
    // Re-seat onto u_start via a first transport from the reseed up.
    af.transport(af.up(), u_start);
    const glm::dquat q_start = af.q;

    const int N = 4000;
    glm::dvec3 up_prev = u_start;
    for (int i = 1; i <= N; ++i) {
        const double phi = 2.0 * kPi * i / N;
        const glm::dvec3 up_cur{std::sin(theta0) * std::cos(phi),
                                std::sin(theta0) * std::sin(phi),
                                std::cos(theta0)};
        af.transport(up_prev, up_cur);
        up_prev = up_cur;
    }
    // up returned to u_start; the residual frame rotation is the holonomy.
    const glm::dquat q_rel = af.q * glm::inverse(q_start);
    const double angle = 2.0 * std::acos(std::min(1.0, std::abs(q_rel.w)));
    const double expected = 2.0 * kPi * (1.0 - std::cos(theta0));  // ~0.84 rad
    CHECK(angle == Catch::Approx(expected).margin(2e-3));
    CHECK(angle > 0.5);  // tens of degrees — the gameplay phenomenon, not noise

    // Sign of the holonomy, not just its size (P3a). Loop closure forces each
    // step's shortest arc to map up_{i-1}->up_i exactly, so q_rel FIXES u_start
    // and its axis is +-u_start REGARDLESS of any transport-magnitude bug —
    // this check therefore isolates exactly ONE degree of freedom: the sign bit
    // that the scalar |angle| and the 2pi-symmetric null test structurally
    // cannot express, and on which the camera-roll direction rests. Not
    // redundant.
    //   The sense (derived, triple-checked vs Foucault + the theta0->{0,pi/2}
    // limits): the +phi loop encloses the +Z cap on its LEFT, giving a POSITIVE
    // (right-hand) rotation about the OUTWARD normal at the base point, so
    // axis = +u_start (NOT -u_start). This +1 reading is valid ONLY while the
    // solid angle Omega = 2pi(1-cos theta0) < pi (theta0 < 60deg): past that
    // the canonical (w>=0) representative is the 2pi-Omega complement about
    // -u_start with the transport still CORRECT, so a larger loop would misread
    // as a sign bug. The REQUIRE below trips loud if theta0 ever crosses that
    // domain.
    //   Canonicalize q_rel to w>=0 (double cover) so its vector part
    // sin(angle/2)*axis, angle in (0,pi), points along the true
    // positive-rotation axis. Reversing the transport (cross(b,a) for
    // cross(a,b)) flips the reading to -u_start (dot -> -0.65); that crude
    // mutant also perturbs |angle|, but the sign pin uniquely owns the class
    // that flips circulation while PRESERVING magnitude + up-tracking (e.g. a
    // parasitic per-step twist about the live up).
    REQUIRE(2.0 * kPi * (1.0 - std::cos(theta0)) <
            kPi);  // sign-domain: +u_start
    glm::dquat qc = q_rel;
    if (qc.w < 0.0) qc = -qc;
    const glm::dvec3 axis{qc.x, qc.y, qc.z};
    CHECK(glm::dot(glm::normalize(axis), u_start) ==
          Catch::Approx(1.0).margin(1e-3));
}

// ---------------------------------------------------------------------------
// reseed vs snap: reseed seeds camera-up from local_up even when banked (the
// §9.2 "starts at local_up" seed, NOT body_up); snap keeps the carried up so
// camera roll is continuous (no surprise leveling — CLAUDE.md 4d). Plus
// orthonormality survives a long mixed run.
// ---------------------------------------------------------------------------
TEST_CASE("aim_frame: reseed uses local_up; snap keeps the carried up") {
    // 50-deg banked flight: local_up and body_up genuinely separate (the S3
    // "which up" discipline — a body_up seed would pass every level test).
    const sim::SimState s =
        harness::flight_state(kP, 140.0, 2000.0, {1.0, 0.0, 0.0},
                              {0.0, 0.0, -1.0}, rad(50.0), 0.0, 0.0);
    const glm::dvec3 local_up = glm::normalize(s.position);
    REQUIRE(glm::dot(local_up, body_up_of(s)) < 0.9);  // fixture really banked

    input::AimFrame af;
    af.reseed(s.orientation, local_up);
    CHECK(glm::dot(af.forward(), nose_of(s)) == Catch::Approx(1.0));
    CHECK(glm::dot(af.up(), local_up) == Catch::Approx(1.0));  // local_up
    CHECK(glm::dot(af.up(), body_up_of(s)) < 0.9);             // NOT body_up

    // Drive the aim off the nose and carry the frame-up away from local_up.
    af.apply_mouse(+1.0, -0.6, rad(20.0));
    const glm::dvec3 carried_up = af.up();
    af.snap_forward_to_nose(s.orientation);
    CHECK(glm::dot(af.forward(), nose_of(s)) == Catch::Approx(1.0));  // -> nose
    // Up re-orthogonalized against the new forward but NOT snapped to local_up:
    // it stays near the carried up (roll continuity), far from a local_up snap.
    CHECK(glm::dot(af.up(), carried_up) > 0.9);
}

TEST_CASE("aim_frame: stays orthonormal over a long mixed run") {
    input::AimFrame af;
    af.reseed(glm::dquat{1.0, 0.0, 0.0, 0.0}, glm::dvec3{1.0, 0.0, 0.0});
    glm::dvec3 up_prev{1.0, 0.0, 0.0};
    for (int i = 1; i <= 5000; ++i) {
        const double phi = 0.01 * i;
        const glm::dvec3 up_cur =
            glm::normalize(glm::dvec3{std::cos(phi), std::sin(phi), 0.3});
        af.transport(up_prev, up_cur);
        af.apply_mouse(0.7, -0.3, rad(1.0));
        up_prev = up_cur;
    }
    CHECK(glm::length(af.q) == Catch::Approx(1.0).epsilon(1e-9));  // unit quat
    CHECK(glm::length(af.forward()) == Catch::Approx(1.0).epsilon(1e-9));
    CHECK(glm::abs(glm::dot(af.forward(), af.up())) < 1e-9);  // orthogonal
    CHECK(glm::abs(glm::dot(af.forward(), af.right())) < 1e-9);
    CHECK(glm::abs(glm::dot(af.up(), af.right())) < 1e-9);
}

// ===========================================================================
// S7-hrz primitives (docs/horizon_recovery_plan.md): the horizon-recovery
// gauge move. roll_about_forward must leave the aim DIRECTION invariant (the
// instructor cannot see the roll), and up_misalignment must be the exact
// capture such that roll_about_forward(up_misalignment(u)) rights the horizon
// — including beyond +/-90 deg (the S4a "angle primitives need a beyond-90
// case" lesson), at the exact-180 antiparallel end (deterministic, no NaN —
// the S7-cam "guard BOTH ends" lesson), and at the zenith (exactly 0.0).
// ===========================================================================
TEST_CASE("aim_frame: roll_about_forward is a gauge move (forward invariant)") {
    const glm::dvec3 up{0.0, 1.0, 0.0};
    input::AimFrame af;
    af.reseed(glm::dquat{1.0, 0.0, 0.0, 0.0}, up);
    af.apply_mouse(+0.8, -0.4, rad(30.0));  // arbitrary non-level attitude
    const glm::dvec3 f0 = af.forward();
    const glm::dvec3 u0 = af.up();

    af.roll_about_forward(rad(37.0));
    // The aim direction is untouched (to fp): one roll < 1e-14, and the up
    // actually rolled by the commanded angle about forward (right-handed).
    CHECK(glm::length(af.forward() - f0) < 1e-14);
    CHECK(glm::dot(af.up(), u0) == Catch::Approx(std::cos(rad(37.0))));
    CHECK(glm::dot(f0, glm::cross(u0, af.up())) ==
          Catch::Approx(std::sin(rad(37.0))));

    // Accumulated over a full recovery's worth of ticks the drift stays
    // negligible (the plan's "forward bit-identical" diagnostic, honest form).
    for (int i = 0; i < 500; ++i) af.roll_about_forward(rad(0.36));
    CHECK(glm::length(af.forward() - f0) < 1e-11);
}

TEST_CASE("aim_frame: up_misalignment round-trips at any roll angle") {
    const glm::dvec3 up{0.0, 1.0, 0.0};
    // Include beyond +/-90 (120, 200 -> short way is -160) and both signs.
    for (double roll_deg : {5.0, 60.0, 120.0, 179.0, 200.0, 300.0, -45.0}) {
        input::AimFrame af;
        af.reseed(glm::dquat{1.0, 0.0, 0.0, 0.0}, up);
        af.roll_about_forward(rad(roll_deg));
        const double m = af.up_misalignment(up);
        CHECK(std::abs(m) <= kPi + 1e-12);  // always the short way
        af.roll_about_forward(m);
        CHECK(glm::dot(af.up(), up) == Catch::Approx(1.0).margin(1e-12));
    }
}

TEST_CASE("aim_frame: up_misalignment guards both degenerate ends") {
    const glm::dvec3 up{0.0, 1.0, 0.0};

    SECTION("exact antiparallel (post-split-S 180): deterministic, no NaN") {
        input::AimFrame af;
        af.reseed(glm::dquat{1.0, 0.0, 0.0, 0.0}, up);
        af.roll_about_forward(kPi);  // up exactly inverted
        const double m1 = af.up_misalignment(up);
        const double m2 = af.up_misalignment(up);
        REQUIRE(std::isfinite(m1));
        CHECK(std::abs(m1) == Catch::Approx(kPi).margin(1e-9));
        CHECK(m1 == m2);  // same state -> same answer, run to run
        af.roll_about_forward(m1);
        CHECK(glm::dot(af.up(), up) == Catch::Approx(1.0).margin(1e-9));
    }

    SECTION("zenith aim (forward along ref_up): exactly 0.0, and rolled too") {
        input::AimFrame af;
        af.reseed(glm::dquat{1.0, 0.0, 0.0, 0.0}, up);
        af.apply_mouse(0.0, -kPi / 2.0, 1.0);  // pitch the aim straight up
        REQUIRE(glm::dot(af.forward(), up) > 0.9999);
        CHECK(af.up_misalignment(up) == 0.0);
        // Still 0 with the frame ALSO rolled (the guard is on forward, and it
        // must not be fooled by a rolled up).
        af.roll_about_forward(rad(77.0));
        CHECK(af.up_misalignment(up) == 0.0);
    }
}
