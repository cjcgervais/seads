#pragma once
// PRECIPITATION placement core (docs/weather_seasons_plan.md W3) — the PURE,
// raylib-free, gate-pinned math for season-gated snow/rain. The raylib billboard
// renderer that consumes it lives in render/precip_draw.{h,cpp} (the seads exe);
// this unit is in seads_render_core so the app-binary-blind gate pins the
// load-bearing invariants headlessly.
//
// THE MECHANISM (Fable-before P1-3/P1-4, load-bearing):
//   - A WORLD-ANCHORED jittered lattice: particle base positions sit on a fixed
//     world cubic grid (integer cell + a deterministic per-cell jitter), so they
//     stay put in world space and STREAM PAST the plane for free as you fly — NOT
//     a camera-frame snow-globe (that reads as wind following you). The world-axis
//     grid is CELL BOOKKEEPING ONLY; it is never a visible direction (the jitter
//     breaks its regularity, and the only visible motion is the fall).
//   - The ONLY visible displacement is the DOWNWARD fall along local_up =
//     normalize(eye) — NO horizontal advection term (Chad's NO-WIND ruling). The
//     fall phase is a pure wrapped fn of t_cel (computed app-side in double).
//   - The BOX follows the eye (snaps to the grid) with a radial boundary alpha
//     fade -> 0 at the box edge, so re-binning as the eye moves is C0 (no pop).
//   - NEVER a tangent basis from a fixed world axis for anything visible (two
//     antipodal degeneracies; GO-ANYWHERE says someone flies through them). Only
//     local_up (radial) is used for motion; the world lattice is invisible.

#include <glm/glm.hpp>

namespace render {

// The integer world cell the eye-following box centers on: round(eye / cell_size)
// per axis. As the eye flies this shifts by whole cells => the box re-bins the
// world-anchored lattice (world positions unchanged; the visible SET changes).
glm::ivec3 precip_center_cell(const glm::dvec3& eye, double cell_size);

// One particle's world position + its fade alpha in [0,1]. `cell` is the integer
// WORLD cell (jitter + fall-phase offset are hashed from it, so the base is
// world-anchored and wind-free); `local_up` is the unit radial (normalize(eye));
// `fall_phase` is the app-owned wrapped fall phase in [0,1). The returned alpha
// is the geometry fade only (radial boundary fade x wrap fade) — the caller
// multiplies in the look opacity and the weather-cell intensity.
struct PrecipSample {
    glm::dvec3 pos;  // world position (jittered base - local_up*fall)
    double alpha;    // geometry fade [0,1] (0 => cull)
};
PrecipSample precip_sample(const glm::ivec3& cell, const glm::dvec3& eye,
                           const glm::dvec3& local_up, double cell_size,
                           double box_half, double wrap_fade, double fall_phase);

}  // namespace render
