#pragma once

// Feature B: Dual wingtip AIRSHOW SMOKE trailing off both wingtips.
// Persistent colored smoke always-on so the plane is trackable from any
// distance. Mirrors vortex.h exactly: pure trail bookkeeping (no raylib),
// app/main.cpp owns a WingtipSmokeTrails, updates per render frame from the
// interpolated draw state, and render::draw_frame draws the puffs.
// READ-ONLY off SimState — nothing here feeds input/control/sim (SPEC §5).

#include <algorithm>
#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>

#include "sim/state.h"

namespace render {

// Deterministic per-puff hash -> [0,1] (no clock; turbulence must be
// frame-rate independent and reproducible). k selects an independent channel.
inline float smoke_hash(std::uint32_t seed, std::uint32_t k) {
    std::uint32_t h = seed * 747796405u + 2891336453u + k * 668265263u;
    h ^= h >> 15;
    h *= 2246822519u;
    h ^= h >> 13;
    return static_cast<float>(h & 0xFFFFFFu) / static_cast<float>(0xFFFFFF);
}

inline constexpr double kSmokeLife     = 4.0;   // [s] puff lifetime (long trail)
inline constexpr double kSmokeEmitDt   = 0.010; // [s] emit interval (dense, gap-free)
inline constexpr double kSmokeTipRight = 5.4;   // [m] wingtip +X (match vortex)
inline constexpr double kSmokeTipUp    = -0.1;  // [m] +Y
inline constexpr double kSmokeTipAft   = 0.6;   // [m] +Z (behind CG, a bit aft of vortex)
inline constexpr double kSmokeR0       = 0.30;  // [m] stream radius at birth
inline constexpr double kSmokeRGrow    = 0.9;   // [m] slow expansion over life
inline constexpr std::size_t kSmokeMax = 400;   // puffs per tip (ring buffer)

struct WingtipSmokePuff {
    glm::dvec3 pos{0.0};
    double age = 0.0;
    std::uint32_t seed = 0;  // deterministic turbulence seed (draw-side)
};

struct WingtipSmokeTrails {
    std::vector<WingtipSmokePuff> left;
    std::vector<WingtipSmokePuff> right;
    double emit_accum = 0.0;
    std::uint32_t emit_count = 0;  // monotonic puff counter -> per-puff seed
};

// Advance ages, expire dead puffs, and (if emit) accumulate the inter-frame
// timer and emit puffs at each wingtip. Rate-based emission = frame-rate
// independent, gap-free at any speed.
inline void wingtip_smoke_update(WingtipSmokeTrails& t,
                                 const sim::SimState& draw_state, bool emit,
                                 double frame_dt) {
    // Age and expire both sides.
    const auto age_side = [&](std::vector<WingtipSmokePuff>& v) {
        for (WingtipSmokePuff& p : v) p.age += frame_dt;
        v.erase(std::remove_if(v.begin(), v.end(),
                               [](const WingtipSmokePuff& p) {
                                   return p.age >= kSmokeLife;
                               }),
                v.end());
    };
    age_side(t.left);
    age_side(t.right);

    if (emit) {
        t.emit_accum += frame_dt;
        while (t.emit_accum >= kSmokeEmitDt) {
            t.emit_accum -= kSmokeEmitDt;
            // Emit one puff at each wingtip in world space.
            for (double side : {-1.0, +1.0}) {
                const glm::dvec3 tip =
                    draw_state.position +
                    draw_state.orientation *
                        glm::dvec3{side * kSmokeTipRight, kSmokeTipUp,
                                   kSmokeTipAft};
                WingtipSmokePuff puff{tip, 0.0, ++t.emit_count};
                auto& vec = (side < 0.0) ? t.left : t.right;
                vec.push_back(puff);
                if (vec.size() > kSmokeMax) vec.erase(vec.begin());
            }
        }
    } else {
        t.emit_accum = 0.0;
    }
}

// Clear on respawn so a dead life's smoke doesn't hang over the fresh spawn.
inline void wingtip_smoke_reset(WingtipSmokeTrails& t) {
    t.left.clear();
    t.right.clear();
    t.emit_accum = 0.0;
    t.emit_count = 0;
}

}  // namespace render
