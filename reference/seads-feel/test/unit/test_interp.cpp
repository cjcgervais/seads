// Render-side state interpolation (SPEC 10: rendering interpolates between
// the last two fixed-dt states). Draw-only, but it is the mechanism under
// AT-9's smoothness claim — endpoints, direction (prev -> curr, not
// swapped), slerp proportionality, and newest-wins scalars all pinned here.

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>

#include "render/interp.h"
#include "sim/state.h"

namespace {

double quat_angle_between(const glm::dquat& a, const glm::dquat& b) {
    const glm::dquat d = glm::inverse(a) * b;
    return 2.0 * std::acos(std::min(std::abs(d.w), 1.0));
}

}  // namespace

TEST_CASE("interpolate: endpoints, direction, and slerp proportionality",
          "[render][interp]") {
    sim::SimState prev;
    prev.position = {15000.0, 200.0, -40.0};
    prev.velocity = {0.0, 0.0, -140.0};
    prev.orientation = glm::dquat{1.0, 0.0, 0.0, 0.0};
    prev.throttle = 0.25;

    sim::SimState curr = prev;
    curr.position = {15004.0, 208.0, -56.0};
    curr.velocity = {1.0, 2.0, -141.0};
    // 0.2 rad about a fixed body axis: slerp angle must scale linearly.
    curr.orientation = glm::normalize(
        prev.orientation *
        glm::angleAxis(0.2, glm::normalize(glm::dvec3{1.0, 2.0, 0.5})));
    curr.throttle = 0.75;
    curr.last_vhat = glm::normalize(glm::dvec3{0.1, 0.0, -1.0});

    SECTION("alpha 0 is prev, alpha -> 1 is curr (direction not swapped)") {
        const sim::SimState a = render::interpolate(prev, curr, 0.0);
        REQUIRE(glm::length(a.position - prev.position) < 1e-12);
        REQUIRE(quat_angle_between(a.orientation, prev.orientation) < 1e-6);
        const sim::SimState b = render::interpolate(prev, curr, 1.0);
        REQUIRE(glm::length(b.position - curr.position) < 1e-12);
        REQUIRE(quat_angle_between(b.orientation, curr.orientation) < 1e-6);
    }

    SECTION("position is the exact chord point at alpha 0.25") {
        const sim::SimState m = render::interpolate(prev, curr, 0.25);
        const glm::dvec3 expected =
            prev.position + 0.25 * (curr.position - prev.position);
        REQUIRE(glm::length(m.position - expected) < 1e-12);
    }

    SECTION(
        "orientation angle scales linearly with alpha (slerp, and the "
        "right way round)") {
        const double full =
            quat_angle_between(prev.orientation, curr.orientation);
        for (double alpha : {0.25, 0.5, 0.75}) {
            const sim::SimState m = render::interpolate(prev, curr, alpha);
            const double from_prev =
                quat_angle_between(prev.orientation, m.orientation);
            REQUIRE(std::abs(from_prev - alpha * full) < 1e-6);
        }
    }

    SECTION("non-interpolated fields are newest-wins (draw-only scalars)") {
        const sim::SimState m = render::interpolate(prev, curr, 0.5);
        REQUIRE(m.throttle == curr.throttle);
        REQUIRE(glm::length(m.last_vhat - curr.last_vhat) == 0.0);
    }
}

// ★ CAM-SMOOTH (2026-09-08): the sled and the walker get the same draw-side
// blend. These pin the contract that main.cpp's draw states rely on: endpoints
// the right way round, chord position, slerp on the sled, a UNIT heading on
// the walker, and newest-wins on the discrete fields.
TEST_CASE("interpolate_sled: endpoints, chord, slerp, newest-wins",
          "[render][interp][sled]") {
    sim::SledState prev;
    prev.position = {15000.0, 3.0, -2.0};
    prev.velocity = {0.0, 0.0, -12.0};
    prev.orientation = glm::dquat{1.0, 0.0, 0.0, 0.0};
    prev.susp_x[0] = 0.02;
    prev.rider_lat_m = -0.1;
    prev.steer_actual = -0.5;
    prev.rolled = false;

    sim::SledState curr = prev;
    curr.position = {15000.0, 3.0, -2.1};
    curr.velocity = {0.0, 0.0, -12.5};
    curr.orientation = glm::normalize(
        prev.orientation *
        glm::angleAxis(0.1, glm::normalize(glm::dvec3{0.0, 1.0, 0.0})));
    curr.susp_x[0] = 0.06;
    curr.rider_lat_m = 0.1;
    curr.steer_actual = 0.5;
    curr.rolled = true;

    SECTION("alpha 0 is prev, alpha 1 is curr") {
        const sim::SledState a = render::interpolate_sled(prev, curr, 0.0);
        REQUIRE(glm::length(a.position - prev.position) < 1e-12);
        REQUIRE(quat_angle_between(a.orientation, prev.orientation) < 1e-6);
        REQUIRE(a.susp_x[0] == prev.susp_x[0]);
        const sim::SledState b = render::interpolate_sled(prev, curr, 1.0);
        REQUIRE(glm::length(b.position - curr.position) < 1e-12);
        REQUIRE(quat_angle_between(b.orientation, curr.orientation) < 1e-6);
    }
    SECTION("mid-tick is the chord midpoint and half the rotation") {
        const sim::SledState m = render::interpolate_sled(prev, curr, 0.5);
        REQUIRE(std::abs(m.position.z - (-2.05)) < 1e-12);
        REQUIRE(std::abs(quat_angle_between(prev.orientation, m.orientation) -
                         0.05) < 1e-6);
        REQUIRE(std::abs(m.susp_x[0] - 0.04) < 1e-12);
        REQUIRE(std::abs(m.rider_lat_m - 0.0) < 1e-12);
        REQUIRE(std::abs(m.steer_actual - 0.0) < 1e-12);
        REQUIRE(m.rolled == curr.rolled);  // discrete: newest wins
    }
}

TEST_CASE("interpolate_walker: chord position, unit heading, newest mode",
          "[render][interp][walker]") {
    sim::WalkerState prev;
    prev.pos = {15000.0, 0.0, 0.0};
    prev.vel = {0.0, 0.0, -1.0};
    prev.heading = {0.0, 0.0, -1.0};
    prev.mode = sim::WalkerMode::Riding;
    sim::WalkerState curr = prev;
    curr.pos = {15000.0, 0.0, -0.02};
    curr.heading = glm::normalize(glm::dvec3{1.0, 0.0, -1.0});
    curr.mode = sim::WalkerMode::Afoot;

    const sim::WalkerState m = render::interpolate_walker(prev, curr, 0.5);
    REQUIRE(std::abs(m.pos.z - (-0.01)) < 1e-12);
    REQUIRE(std::abs(glm::length(m.heading) - 1.0) < 1e-12);
    // Blended heading lies between the two (both have negative z, positive
    // x only on curr): x in (0, curr.x), z < 0.
    REQUIRE(m.heading.x > 0.0);
    REQUIRE(m.heading.x < curr.heading.x);
    REQUIRE(m.heading.z < 0.0);
    REQUIRE(m.mode == curr.mode);
    // A degenerate (antiparallel) blend falls back to the newest heading.
    curr.heading = {0.0, 0.0, 1.0};
    const sim::WalkerState d = render::interpolate_walker(prev, curr, 0.5);
    REQUIRE(glm::length(d.heading - curr.heading) < 1e-12);
}

// ★ CAM-SMOOTH red-team F1/F3 (2026-09-09): the camera lag's feed-forward is
// the fixed point of THIS recurrence, not a nearby one. At constant velocity
// the anchor must sit ON the body for every frame dt (30/60/120 Hz and the
// 0.25 s hitch clamp), and tau == 0 must be the welded camera exactly.
TEST_CASE("cam_lag_step: zero steady-state lag at constant velocity",
          "[render][interp][cam]") {
    const glm::dvec3 v{20.0, 0.0, -3.0};
    for (double dt : {1.0 / 120.0, 1.0 / 60.0, 1.0 / 30.0, 0.25}) {
        for (double tau : {0.0, 0.06, 0.5}) {
            glm::dvec3 p{100.0, 5.0, 7.0};
            glm::dvec3 a = p;  // seeded on the body
            for (int i = 0; i < 400; ++i) {
                p += v * dt;
                a = render::cam_lag_step(a, p, v, dt, tau);
            }
            const double err = glm::length(a - p);
            INFO("dt=" << dt << " tau=" << tau << " err=" << err);
            REQUIRE(err < 1e-9);
        }
    }
}

TEST_CASE("cam_lag_step: tau 0 is welded, a bump is filtered and stable",
          "[render][interp][cam]") {
    const glm::dvec3 p{1.0, 2.0, 3.0}, a{50.0, 0.0, 0.0}, v{9.0, 9.0, 9.0};
    REQUIRE(glm::length(render::cam_lag_step(a, p, v, 1.0 / 60.0, 0.0) - p) ==
            0.0);
    // A 0.3 m step with zero velocity (a pure bump): the anchor moves toward
    // it by kp < 1, never past it, at any dt including the 0.25 s clamp.
    for (double dt : {1.0 / 120.0, 1.0 / 60.0, 0.25}) {
        const glm::dvec3 body{0.0, 0.3, 0.0};
        const glm::dvec3 nxt =
            render::cam_lag_step(glm::dvec3{0.0}, body, glm::dvec3{0.0}, dt, 0.06);
        REQUIRE(nxt.y > 0.0);
        REQUIRE(nxt.y <= 0.3 + 1e-12);
        REQUIRE(std::isfinite(nxt.y));
    }
}
