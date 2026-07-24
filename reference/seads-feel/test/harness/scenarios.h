#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "sim/params.h"
#include "sim/state.h"

// The Section-2 golden flight (HARNESS §3 T2): ONE fixed start state and
// ONE fixed input script, shared by the golden test (compare) and the
// harness (record / fly modes) so a re-record can never drift from what
// the test replays. Changing ANYTHING here moves the golden — that halts
// the loop until the move is confirmed intentional.

namespace harness {

constexpr int kGoldenTicks = 1200;  // 10 s at 1/120
constexpr int kGoldenCheckpointEvery = 300;

// Level at 2 km over the +X pole, 140 m/s: body up (+Y_b) -> world +X
// (local up there), nose (-Z_b) -> world -Z, velocity along the nose.
inline sim::SimState golden_start(const sim::AircraftParams& p) {
    sim::SimState s;
    s.position = {p.R + 2000.0, 0.0, 0.0};
    const glm::dmat3 m{glm::dvec3{0.0, -1.0, 0.0},  // body X -> world -Y
                       glm::dvec3{1.0, 0.0, 0.0},   // body Y (up) -> world +X
                       glm::dvec3{0.0, 0.0, 1.0}};  // body Z -> world +Z
    s.orientation = glm::normalize(glm::quat_cast(m));
    s.velocity = 140.0 * glm::dvec3{0.0, 0.0, -1.0};
    s.last_vhat = {0.0, 0.0, -1.0};
    return s;
}

// Full throttle throughout; a pull, a rolling pull, a skid, then hands-off.
// Exercises thrust, lift incl. transients, induced drag, all three torque
// axes, and the compression ramp as speed builds.
inline sim::Inputs golden_input(int tick) {
    sim::Inputs in{};
    in.throttle = 1.0f;
    if (tick < 240) {
        // 0-2 s: accelerate, hands off
    } else if (tick < 480) {
        in.pitch = 0.4f;  // 2-4 s: pull
    } else if (tick < 720) {
        in.pitch = 0.1f;  // 4-6 s: rolling pull (roll-right = negative)
        in.roll = -0.6f;
    } else if (tick < 960) {
        in.yaw = 0.3f;  // 6-8 s: skid (nose-left)
    }
    // 8-10 s: neutral stick
    return in;
}

}  // namespace harness
