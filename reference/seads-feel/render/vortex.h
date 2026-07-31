#pragma once

#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>

#include "sim/state.h"

// MB-7c (iii): trailing wingtip vortices near stall AoA / high G ("to show
// stall" — Chad-ruled Q6). PURE trail bookkeeping (no raylib): app/main.cpp
// owns a VortexTrails, updates it per RENDER frame from the interpolated
// draw state + the shared flight readout, and render::draw_frame draws the
// segments. READ-ONLY off SimState (the S8 gunsight firewall): nothing here
// feeds input/control/sim. Cosmetic per-frame state is legal render-side
// smoothing (off the mouse->aim loop, RA9).
//
// Intensity ramps in over the last ~25% of the AoA margin toward aoa_max
// (the protection clamp's own limit, passed in from controller config — the
// shared-readout discipline: the dial, the clamp, and the vortices agree on
// where "near stall" is) and over G beyond kVortexGOnset. No latch — a
// continuous fade (the gate rule targets discrete switches; this is art).

namespace render {

inline constexpr double kVortexAoAFrac = 0.75;   // intensity 0 below this*aoa
inline constexpr double kVortexGOnset = 4.0;     // [g] G-driven onset
inline constexpr double kVortexGFull = 7.0;      // [g] G fully lit
inline constexpr double kVortexMinSpeed = 45.0;  // [m/s] no vortices at ~no q
inline constexpr double kVortexLife = 1.1;       // [s] point lifetime
inline constexpr double kVortexTipRight = 5.4;   // [m] body +X of the tip
inline constexpr double kVortexTipUp = -0.1;     // [m] body +Y
inline constexpr double kVortexTipAft = 0.3;     // [m] body +Z (behind CG)
inline constexpr std::size_t kVortexMax = 240;   // points per tip (ring)

// One trail point: a world position, its age, and the spawn intensity.
struct VortexPoint {
    glm::dvec3 pos{0.0};
    double age = 0.0;
    double strength = 0.0;  // [0,1] at spawn; draw alpha = strength*(1-age/L)
};

struct VortexTrails {
    std::vector<VortexPoint> left;
    std::vector<VortexPoint> right;
};

// Intensity [0,1] from the SHARED readout quantities (velocity-relative AoA,
// true n) — never a re-derived attitude proxy (the flat-instrument rule).
// aoa_max is the controller's protection limit [rad].
inline double vortex_strength(double aoa, double aoa_max, double load_factor,
                              double speed) {
    if (speed < kVortexMinSpeed || aoa_max <= 0.0) return 0.0;
    const double a = std::abs(aoa) / aoa_max;  // fraction of the AoA margin
    const double sa =
        std::clamp((a - kVortexAoAFrac) / (1.0 - kVortexAoAFrac), 0.0, 1.0);
    const double sg = std::clamp((std::abs(load_factor) - kVortexGOnset) /
                                     (kVortexGFull - kVortexGOnset),
                                 0.0, 1.0);
    return std::max(sa, sg);
}

// Advance ages, expire dead points, and (if strength > 0) append this
// frame's tip positions. `state` is the INTERPOLATED draw state (cosmetic).
inline void vortex_update(VortexTrails& t, const sim::SimState& state,
                          double strength, double frame_dt) {
    const auto tick_side = [&](std::vector<VortexPoint>& v, double side) {
        for (VortexPoint& p : v) p.age += frame_dt;
        v.erase(std::remove_if(
                    v.begin(), v.end(),
                    [](const VortexPoint& p) { return p.age >= kVortexLife; }),
                v.end());
        if (strength > 0.0) {
            const glm::dvec3 tip =
                state.position +
                state.orientation * glm::dvec3{side * kVortexTipRight,
                                               kVortexTipUp, kVortexTipAft};
            v.push_back({tip, 0.0, strength});
            if (v.size() > kVortexMax) v.erase(v.begin());
        }
    };
    tick_side(t.left, -1.0);
    tick_side(t.right, +1.0);
}

// Clear on respawn/mode toggle — a dead life's vortices must not hang in the
// air over the fresh spawn.
inline void vortex_reset(VortexTrails& t) {
    t.left.clear();
    t.right.clear();
}

}  // namespace render
