#pragma once

#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "sim/aero.h"
#include "sim/state.h"

// THE shared state extraction (SPEC §9.7): every phi/beta/cosPhiTheta/AoA
// the controller, HUD, app loop, or harness ever reads comes THROUGH THIS
// FILE — the same code compiled into all of them, never two
// implementations. A sign flip here is the positive-feedback failure class;
// AT-0's through-extraction cases pin it against glm reality (HARNESS §5).
//
// Frame rules (SPEC §6.1 / §9.3): local_up = normalize(position), recomputed
// on every call — never cached, never a global axis. The alpha/beta
// expressions are sim/aero.h's (single source, H1); the v-hat guard is
// sim::guarded_dir on the CONTROLLER'S OWN held copy, passed in by the
// caller (SPEC §9.6 channel 1 — the seam forbids reading SimState.last_vhat).
//
// control/ is PURE (SPEC §5): no I/O, no clock, and it may include sim/ but
// never render/ or input/.

namespace control {

struct Extracted {
    glm::dvec3 local_up;    // normalize(position), this call
    glm::dvec3 body_right;  // body +X in world
    glm::dvec3 body_up;     // body +Y in world
    glm::dvec3 nose;        // body -Z in world
    glm::dvec3 vhat;        // guarded velocity direction, world — the caller
                            // stores this back as its held copy
    double speed = 0.0;     // |velocity|, unguarded (guards are per-use:
                            // vMin floors divisions, v_dir_eps floors v-hat)
    double phi = 0.0;       // bank, + = right wing down (SPEC §7)
    double cos_phi_theta = 0.0;  // dot(body_up, local_up), SIGNED gravity
                                 // credit — never clamp >= 0 (SPEC §9.3)
    double alpha = 0.0;  // RAW AoA, + = nose above velocity; the filtered
                         // copy (sole smoothing exception) is controller
                         // internal state, not extraction
    double beta = 0.0;   // sideslip, + = velocity right of nose
};

inline Extracted extract(const sim::SimState& s, const glm::dvec3& last_vhat,
                         double v_dir_eps) {
    Extracted e;
    e.local_up = glm::normalize(s.position);
    e.body_right = s.orientation * glm::dvec3{1.0, 0.0, 0.0};
    e.body_up = s.orientation * glm::dvec3{0.0, 1.0, 0.0};
    e.nose = s.orientation * glm::dvec3{0.0, 0.0, -1.0};

    e.speed = glm::length(s.velocity);
    e.vhat = sim::guarded_dir(s.velocity, last_vhat, v_dir_eps);

    // phi = -asin(dot(body_right, local_up)) (SPEC §9.3): right wing DOWN
    // means body_right dips below the horizon, dot < 0, phi > 0. The clamp
    // guards asin against |dot| = 1 + ulp, not against sign.
    e.phi =
        -std::asin(std::clamp(glm::dot(e.body_right, e.local_up), -1.0, 1.0));
    e.cos_phi_theta = glm::dot(e.body_up, e.local_up);

    const glm::dvec3 v_body_dir = sim::body_dir_of(s.orientation, e.vhat);
    e.alpha = sim::alpha_of(v_body_dir);
    e.beta = sim::beta_of(v_body_dir);
    return e;
}

}  // namespace control
