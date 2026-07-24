#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace sim {

// The controller<->plant seam (SPEC §5). Axis semantics follow the body
// frame sign table (SPEC §7: +X right, +Y up, -Z forward): a positive input
// commands positive torque about that body axis, so
//   pitch +1 = pitch up   (+omega_x)
//   yaw   +1 = nose LEFT  (+omega_y; yaw-right is NEGATIVE input)
//   roll  +1 = roll LEFT  (+omega_z; roll-right is NEGATIVE input)
// Stick/key ergonomics live in input/ (Section 3+) — the plant stays
// sign-trivial: tau_axis = c_axis * max(q, q_att_floor) * delta_max_eff(V) *
// Input.
struct Inputs {
    float pitch = 0.0f;     // [-1, 1], full commanded deflection at +/-1
    float roll = 0.0f;      // [-1, 1]
    float yaw = 0.0f;       // [-1, 1]
    float throttle = 0.0f;  // [0, 1], passthrough (SPEC §9.8)
    // MB-flaps (SPEC §0): pilot-managed lift/drag devices, passthrough like
    // throttle (the instructor never manages them); the PLANT owns the deploy
    // slew toward these targets. flap_cmd is a deflection FRACTION (0 = clean,
    // the combat detent ~0.5, 1 = landing); gear_cmd 0 = up, 1 = down.
    // Defaulted 0 => every pre-flap Inputs (goldens, drones, tests) commands
    // a clean airframe, bit-identically (strict superset).
    float flap_cmd = 0.0f;  // [0, 1] commanded flap deflection fraction
    float gear_cmd = 0.0f;  // [0, 1] commanded gear extension
};

// SPEC §7 state, world Cartesian, planet center at origin (never
// lat/lon/heading). Double precision on purpose: at |position| ~ 15 km, float
// ulp (~2 mm) is the same order as one tick of gravity displacement (g*dt^2 ~
// 0.7 mm) — a float point at low speed cannot even move. Render casts down at
// the seam.
struct SimState {
    glm::dvec3 position{0.0};                    // world [m]
    glm::dvec3 velocity{0.0};                    // world [m/s]
    glm::dquat orientation{1.0, 0.0, 0.0, 0.0};  // body->world, unit
    glm::dvec3 angular_vel{0.0};                 // body frame [rad/s]
    double throttle = 0.0;                       // [0, 1]
    // Plant-side v-hat guard (SPEC §7 / §9.6 channel 1): last valid unit
    // velocity direction, world frame, held below v_dir_eps. The sim owns
    // this copy; the controller keeps its OWN (the seam forbids sharing).
    // Default = -Z, the identity orientation's nose.
    glm::dvec3 last_vhat{0.0, 0.0, -1.0};
    // MB-flaps: ACTUAL device positions, slewed by the plant toward the
    // commanded targets (the throttle-slew pattern — deploy is physics, not
    // UI). 0 = clean/up; force-only consumers (lift shift + drag tax), the
    // torque/authority model never reads them (AT-18 untouched).
    double flap = 0.0;  // [0, 1] flap deflection fraction
    double gear = 0.0;  // [0, 1] gear extension fraction
};

}  // namespace sim
