#pragma once

#include <cassert>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "sim/state.h"

// SPEC §6.1 sphere invariants — asserted every tick in debug builds.
// (CMAKE_BUILD_TYPE=Debug leaves NDEBUG undefined, so assert() is live.)

namespace sim {

inline bool is_finite(const glm::dvec3& v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

inline bool is_finite(const glm::dquat& q) {
    return std::isfinite(q.w) && std::isfinite(q.x) && std::isfinite(q.y) &&
           std::isfinite(q.z);
}

// The gravity actually applied must equal -normalize(position), recomputed
// here independently of how the caller obtained it. Tautological against
// today's step() by construction — it exists as the tripwire for any future
// code that sneaks in a fixed down axis.
// R5: an ACTIVE GravityField legitimately reaches exact 0.0 in the clipped
// far tail (sim/fields.h kGravTailCutX) — direction is meaningless there, so
// the caller passes tapered_field and the direction leg is guarded on a
// nonzero magnitude. The caller scopes the flag to alt > h_g0 (step.cpp):
// with env.grav null OR in the fight band the magnitude is always p.g > 0,
// so the relaxed branches are UNREACHABLE there — the tripwire keeps its
// Section-1 strength everywhere except the taper region itself.
inline void assert_gravity_radial(const glm::dvec3& position,
                                  const glm::dvec3& gravity_used,
                                  bool tapered_field = false) {
    (void)position;
    (void)gravity_used;
    (void)tapered_field;
    assert(glm::length(position) > 0.0);
    assert(tapered_field || glm::length(gravity_used) > 0.0);
    if (glm::length(gravity_used) > 0.0) {
        assert(glm::dot(glm::normalize(gravity_used),
                        -glm::normalize(position)) > 1.0 - 1e-12);
    }
}

// The velocity change the integrator ACTUALLY applied must be radial — this
// checks what happened, not what the caller claims (the Section-1 red-team
// lesson: a mutated integration line slips past an assert that only inspects
// the handed-in vector). Since Section 2 this fires only on the pure-ballistic
// path (no wing, no thrust) — step() gates it on S == 0 && thrust == 0 — so
// ballistic runs keep the full-strength tripwire.
// R5: same taper guard as assert_gravity_radial — the ballistic path
// (S == 0 && thrust == 0) is exactly where an escaped plane drifts through
// the clipped tail, so dv = g*dt*dir reaches exact zero there too. Null
// path unreachable-relaxed for the same reason.
inline void assert_applied_impulse_radial(const glm::dvec3& position,
                                          const glm::dvec3& vel_before,
                                          const glm::dvec3& vel_after,
                                          bool tapered_field = false) {
    (void)position;
    (void)vel_before;
    (void)vel_after;
    (void)tapered_field;
    const glm::dvec3 dv = vel_after - vel_before;
    assert(tapered_field || glm::length(dv) > 0.0);
    if (glm::length(dv) > 0.0) {
        assert(glm::dot(glm::normalize(dv), -glm::normalize(position)) >
               1.0 - 1e-12);
    }
}

// No NaN in any state field; orientation renormalized (unit) every tick;
// position never at the planet center (normalize(0) is the NaN seed);
// the held v-hat guard stays a unit vector (it feeds atan2 and force dirs).
inline void assert_state_valid(const SimState& s) {
    (void)s;
    assert(is_finite(s.position) && is_finite(s.velocity) &&
           is_finite(s.angular_vel) && is_finite(s.orientation));
    assert(std::isfinite(s.throttle));
    // MB-flaps: slewed device positions stay in [0,1] (the slew clamps
    // toward a clamped command, so an escape is a code bug, not a tune).
    assert(s.flap >= 0.0 && s.flap <= 1.0);
    assert(s.gear >= 0.0 && s.gear <= 1.0);
    assert(is_finite(s.last_vhat));
    assert(std::abs(glm::length(s.last_vhat) - 1.0) < 1e-9);
    assert(glm::length(s.position) > 0.0);
    assert(std::abs(glm::length(s.orientation) - 1.0) < 1e-12);
}

}  // namespace sim
