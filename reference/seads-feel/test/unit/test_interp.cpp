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
