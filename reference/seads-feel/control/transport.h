#pragma once

#include <cassert>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

// Aim carry — parallel transport (SPEC §9.1 / §9.2 / §6): every tick, every
// mode, rotate the world-frame aim vector by the quaternion taking
// local_up(t-dt) to local_up(t). A world-frozen aim on R = 15 km drifts off
// the horizon in seconds; this keeps a "level" aim level. The CAMERA is
// carried by the IDENTICAL quaternion (frame-carried holonomy, §9.2) — so
// transport_rotation is exported for the render side to reuse the exact same
// rotation, never a re-derived one.
//
// PURE, in control/ (no I/O). The rotation is the shortest arc between two
// unit up-vectors; over one tick the two are nearly parallel (V/R per second),
// so the small-angle path dominates and the degenerate guards are just NaN
// safety, not hot code.

namespace control {

// Shortest-arc quaternion carrying up_prev onto up_cur (both need not be
// pre-normalized). Identity when they coincide.
inline glm::dquat transport_rotation(const glm::dvec3& up_prev,
                                     const glm::dvec3& up_cur) {
    const glm::dvec3 a = glm::normalize(up_prev);
    const glm::dvec3 b = glm::normalize(up_cur);
    const glm::dvec3 axis = glm::cross(a, b);
    const double s = glm::length(axis);
    const double d = glm::dot(a, b);
    // Coincident (or numerically so): no rotation. s ~ sin(angle); one tick's
    // angle is ~V/(R*rate), never near pi, so the antiparallel branch (which
    // would need an arbitrary perpendicular axis) is unreachable in flight.
    if (s < 1e-12) {
        // Coincident (d > 0) -> identity. ANTIPARALLEL (d < 0) is a different
        // beast: the shortest arc is a 180 deg turn about an ARBITRARY
        // perpendicular axis, and returning identity would silently no-op a
        // real half-turn. It is unreachable across one flight tick (the two
        // ups differ by ~V/R), so rather than pick an arbitrary axis we assert
        // the per-tick contract — a caller that spans a near-antiparallel gap
        // (a respawn teleport, a debug camera jump) is misusing this and must
        // not get a silent wrong answer.
        assert(d > 0.0);
        return glm::dquat(1.0, 0.0, 0.0, 0.0);
    }
    const double angle = std::atan2(s, d);
    return glm::angleAxis(angle, axis / s);
}

// Transport a world-frame aim unit vector across one tick.
inline glm::dvec3 transport_aim(const glm::dvec3& aim,
                                const glm::dvec3& up_prev,
                                const glm::dvec3& up_cur) {
    return glm::normalize(transport_rotation(up_prev, up_cur) * aim);
}

}  // namespace control
