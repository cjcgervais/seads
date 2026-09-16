#pragma once

#include <cassert>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "sim/params.h"
#include "sim/state.h"
#include "sim/step.h"

// Step injector (HARNESS §4): characterize the plant by driving crafted
// states through the REAL sim::step() — there is no separate rig to audit.
// Section 2 capability: peak angular acceleration per axis (the "alpha_max"
// the braking law will consume in Section 4), measured for AT-18a.
// Section 4a capability: crafted-state builders (known bank / sideslip /
// AoA at arbitrary points on the sphere) for AT-0's injected-state and
// through-extraction sign cases. The builders construct geometry from raw
// rotations and raw velocity components — NEVER through control/extract.h
// or sim::alpha_of/beta_of — so a sign error in the code under test cannot
// cancel out of the fixture (the "instrument must not share the mechanism's
// bugs" rule, CLAUDE.md S1/S3).

namespace harness {

// Level flight over an arbitrary point: body up = local up = normalize(
// up_dir), nose along `heading` Gram-Schmidt'd into the tangent plane,
// velocity along the nose. Right-handed body frame (SPEC §7): X = Y x Z
// with Z = -nose, so body_right = heading x up.
inline sim::SimState level_state(const sim::AircraftParams& p, double V,
                                 double altitude, const glm::dvec3& up_dir,
                                 const glm::dvec3& heading) {
    const glm::dvec3 u = glm::normalize(up_dir);
    const glm::dvec3 t_raw = heading - glm::dot(heading, u) * u;
    assert(glm::length(t_raw) > 1e-9);  // heading must not be radial
    const glm::dvec3 t = glm::normalize(t_raw);
    const glm::dvec3 right = glm::cross(t, u);
    sim::SimState s;
    s.position = (p.R + altitude) * u;
    s.orientation = glm::normalize(glm::quat_cast(glm::dmat3{right, u, -t}));
    s.velocity = V * t;
    s.last_vhat = t;
    return s;
}

// Level state rolled about its own nose axis by `bank` (+ = right wing
// down, SPEC §7: rolling right-hand about the nose direction carries body
// +X below the horizon), then given a velocity direction with the requested
// sideslip/AoA IN BODY COMPONENTS: v_body = normalize(tan(beta), -tan(alpha),
// -1) — beta > 0 puts velocity along +body_right (right of nose), alpha > 0
// puts it below body -Z (nose above velocity). atan2 of those components
// recovers beta/alpha exactly, but the fixture never calls the recovery
// functions — tests assert the physical predicates independently.
inline sim::SimState flight_state(const sim::AircraftParams& p, double V,
                                  double altitude, const glm::dvec3& up_dir,
                                  const glm::dvec3& heading, double bank,
                                  double beta, double alpha) {
    sim::SimState s = level_state(p, V, altitude, up_dir, heading);
    const glm::dvec3 nose = s.orientation * glm::dvec3{0.0, 0.0, -1.0};
    s.orientation = glm::normalize(glm::angleAxis(bank, nose) * s.orientation);
    const glm::dvec3 v_body =
        glm::normalize(glm::dvec3{std::tan(beta), -std::tan(alpha), -1.0});
    const glm::dvec3 v_dir = s.orientation * v_body;
    s.velocity = V * v_dir;
    s.last_vhat = v_dir;
    return s;
}

enum class Axis { pitch, yaw, roll };

// Full deflection on one axis from omega = 0 at speed V, one tick through
// the real plant: measured accel = omega_after / dt. From rest the damping
// term contributes exactly zero, so this reads the pure authority model —
// any mismatch with the analytic c*max(q,floor)*delta(V)/I means dynamic
// pressure was applied twice or the params forked (H1 / AT-18a).
// `env` (R4, the R3-review P1): no default — the instrument must probe the
// SAME world the plant flies or the measurement forks from the sim (null =
// the v3 plant, which is what the AT-18a authority checks grade today).
inline double measure_ang_accel_max(const sim::AircraftParams& p, double V,
                                    Axis axis, double alt,
                                    const sim::Environment* env) {
    sim::SimState s;
    s.position = {p.R + alt, 0.0, 0.0};  // local up = +X, at altitude (MB-atm)
    s.velocity = V * glm::dvec3{0.0, 0.0, -1.0};  // along the identity nose
    s.last_vhat = {0.0, 0.0, -1.0};

    sim::Inputs in{};
    switch (axis) {
        case Axis::pitch:
            in.pitch = 1.0f;
            break;
        case Axis::yaw:
            in.yaw = 1.0f;
            break;
        case Axis::roll:
            in.roll = 1.0f;
            break;
    }

    const sim::SimState s1 = sim::step(s, in, p, env, p.sim_dt);
    const double omega = axis == Axis::pitch ? s1.angular_vel.x
                         : axis == Axis::yaw ? s1.angular_vel.y
                                             : s1.angular_vel.z;
    return omega / p.sim_dt;
}

}  // namespace harness
