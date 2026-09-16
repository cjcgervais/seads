#pragma once
// ★★★ L1 — PUTTING THE MAN DOWN (docs/PLAN_20260901_game_loop_millwright.md
// §3).
//
// A STUB OWNED BY THE `loop` LANE, and it says so out loud. R4e (the r4a lane)
// owns the real dismount -- the animated step off the running board, the pose
// that arrives standing rather than appearing standing. Until it lands, this is
// the whole of "he is beside the machine now": a position, a heading, and the
// mode asked for BY ENUMERATOR NAME.
//
// ⚠ `sim/walker.*` IS FROZEN SURFACE (r4a's message, adef92479). This file
// writes only the fields that surface publishes -- `mode`, `pos`, `heading` --
// and it re-initialises from a default-constructed `WalkerState` so a field
// added to the kernel tomorrow arrives at ITS OWN default here rather than at
// whatever the last fall left behind. Enumerating the fields to clear would be
// a transcribed second copy of a struct we do not own.
//
// ⚠ AND `WalkerMode::Afoot` IS WRITTEN BY NAME, NEVER BY VALUE. The enum is
// `: int` and its members are ordered by the fall sequence; the day a stage is
// inserted, a literal here would put the man face-down in the snow with no test
// able to see it.

#include <glm/glm.hpp>

#include "sim/walker.h"

namespace app {

// Place the man on his feet at `pos_w`, facing `heading_w`.
//
//   pos_w      WORLD, and already grounded by the caller -- the drive surface
//              is a snowpack query and this header does not own one. `sim/
//              walker.cpp` re-projects him onto `drive_r + lie_clearance_m` on
//              his very first step, so a caller that is a few centimetres out
//              is corrected rather than wrong forever.
//   heading_w  WORLD, tangentialised here (a heading handed in from a body
//              frame on a 15 km ball is not tangent at his feet).
inline void walker_place_afoot(sim::WalkerState& w, const glm::dvec3& pos_w,
                               const glm::dvec3& heading_w) {
    w = sim::WalkerState{};
    w.mode = sim::WalkerMode::Afoot;  // BY NAME
    w.pos = pos_w;
    w.vel = glm::dvec3{0.0};
    glm::dvec3 h = heading_w;
    const double r = glm::length(pos_w);
    if (r > 0.0) {
        const glm::dvec3 up = pos_w / r;
        h = h - glm::dot(h, up) * up;
    }
    const double hl = glm::length(h);
    // No usable heading -> leave it zero. `step_walker` seeds any tangent on
    // its first step rather than normalising a zero vector; inventing a
    // direction here would be a second answer to a question the kernel already
    // answers (sim/walker.cpp, the locomotion block).
    w.heading = hl > 1.0e-9 ? h / hl : glm::dvec3{0.0};
}

}  // namespace app
