#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "sim/state.h"

// Render-side interpolation between the last two fixed-dt states (SPEC §10:
// "rendering interpolates between the last two states"). STRICTLY one-way:
// the result is drawn and discarded — it never re-enters sim/ or (later)
// control/, so it is not a smoothing stage in any signal path (HARNESS §6
// loop-integrity: the smoothed thing here is pixels, not a basis).

namespace render {

inline sim::SimState interpolate(const sim::SimState& prev,
                                 const sim::SimState& curr, double alpha) {
    sim::SimState out = curr;  // scalars/guards: newest wins, draw-only
    out.position = glm::mix(prev.position, curr.position, alpha);
    out.velocity = glm::mix(prev.velocity, curr.velocity, alpha);
    out.orientation = glm::slerp(prev.orientation, curr.orientation, alpha);
    return out;
}

}  // namespace render
