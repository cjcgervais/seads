#pragma once

// RUNG E4 — VISIBILITY (docs/ENEMY_AI_E1_E2_SPEC.md, Chad 2026-08-20: "they are
// hard to see especially in the white scatter light, their tag and color is
// also a bit hard to see").
//
// The PURE core of the three E4 legs: one POD of dials plus three tiny
// functions. Raylib-free (glm + std only) so the gate can pin them headlessly
// in seads_render_core — this is the plane_viz.h / vortex.h pattern.
//
// NO CLOCK, NO RNG. The glint phase is a function of FrameInfo::frame_count
// (the app-owned monotonic render-frame ordinal that already seeds the S1 post
// grain) and the drone's index. Same frame + same index => same answer, always.
//
// THE KNOB-OFF CONTRACT is the load-bearing property of this header, because
// the atmosphere/scatter frame it sits in front of is FLOWN-APPROVED SIGNED
// WORK. Every function below returns the identity at its off value, by EARLY
// RETURN (not by arithmetic that happens to land on 1.0):
//   aircraft_haze_frac  = 1.0  -> env_wash_scale() == 1.0 exactly, every range
//   enemy_tint_shift    = 0.0  -> team_color.h enemy_legibility_tint() == base
//   tag_outline_px      = 0.0  -> no outline draw at all
//   tag_min_px          <= the base font size -> the base font size
//   glint_size_m        = 0.0  -> glint_radius_m() == 0 => nothing drawn
// With config/world.toml's [plane_legibility] at those values the frame is
// today's frame.
//
// ★ WHAT THE "HAZE" SEAM ACTUALLY TURNED OUT TO BE — read this before retuning.
// The spec says "drones receive only this fraction of the atmospheric
// scatter/haze mix at range". MEASURED, the drone meshes receive ZERO
// atmospheric haze: sky_aerial() (render/sky.cpp:137) is called ONLY by the
// planet FS (render/planet.cpp:604) and the lake surfaces. Aircraft are drawn
// by the mirror shader (render/rig.cpp mirror_fs_source), which has no aerial
// term at all. What actually washes a plane out in the white scatter light is
// the shader's ENVIRONMENT REFLECTION:
//
//     render/rig.cpp:281
//     vec3 col = mix(body, envCol, u_reflectivity * (1.0 - fres));
//
// envCol is the GRAYSCALE planet albedo cubemap. Over a white winter world in
// scatter light envCol is near-white, so at reflectivity 0.45 nearly half the
// plane's body colour is replaced by "the ambient white of the world" — the
// mirror finish literally paints the haze onto the aircraft. That is the seam
// this leg attenuates, and it is why the dial keeps the spec's NAME
// (aircraft_haze_frac) but is documented here as an ENV-WASH fraction. It is
// applied per-draw-call to DRONES ONLY (draw.cpp's drone loop), so the player's
// signed hero-plane look and every other pass are untouched.
//
// RANGE GRADING: the wash is what you notice at range, and the close-up mirror
// look is approved art, so the fraction fades IN with distance — full today's
// look at the eye, the full fraction at/beyond haze_full_range_m.

#include <algorithm>
#include <cmath>

namespace render {

// The [plane_legibility] dials, defaulted to their OFF values so a FrameInfo
// that nobody fills is today's frame (the non-conquest / test / probe paths).
struct LegibilityParams {
    // E4.1 — the env-wash (see the banner). 1.0 = today, bit-identical.
    double aircraft_haze_frac = 1.0;
    // Distance [m] at which aircraft_haze_frac is fully applied. At the eye the
    // scale is 1.0 (today's approved close-up mirror); it smoothsteps in.
    double haze_full_range_m = 0.0;

    // E4.2 — tag legibility.
    double tag_outline_px = 0.0;  // dark halo ring radius [px]; 0 = today
    double tag_min_px = 0.0;      // on-screen tag size floor [px]; <= base font
                                  // = today
    double enemy_tint_shift = 0.0;  // [0,1] toward the deep red; 0 = today
    // Alpha floor for the tag INSIDE its fade band (Fable adjudication, rung
    // E4: the 4->9 km 200->0 alpha fade is the bigger half of "the tag is
    // hard to see" at the 2-4 km ranges Chad flies). Applied only while the
    // legacy fade is still > 0 -- the 9 km cutoff and the ghost fix stand;
    // this floors the fade, it never resurrects a tag. 0 = today.
    double tag_min_alpha = 0.0;  // [0,255] fade floor; 0 = today's fade

    // E4.3 — engagement glint.
    double glint_size_m = 0.0;    // world radius [m]; 0 = OFF (nothing drawn)
    double glint_range_m = 0.0;   // only within this slant range of the eye
    double glint_min_mrad = 0.0;  // angular floor [mrad] so a 4 km contact's
                                  // glint does not fall under one pixel
    int glint_period_frames = 0;  // blink period [frames]; <= 0 = OFF
    double glint_duty = 0.0;      // lit fraction of the period [0,1]; 0 = OFF
};

// E4.1: the multiplier applied to the mirror shader's u_reflectivity for ONE
// drone at slant range `dist_m`.
//
// OFF-IDENTITY IS AN EARLY RETURN, not arithmetic: frac >= 1.0 returns exactly
// 1.0 for every distance, so `reflectivity * scale` is the same float the
// signed build uploads. Pinned by test_plane_legibility.
inline double aircraft_env_wash_scale(double dist_m, double frac,
                                      double full_range_m) {
    if (!(frac < 1.0)) return 1.0;              // off (and NaN-safe)
    const double f = std::max(0.0, frac);       // clamp a fat-fingered negative
    if (!(full_range_m > 0.0)) return f;        // no ramp: apply everywhere
    const double t =
        std::min(1.0, std::max(0.0, dist_m / full_range_m));
    const double s = t * t * (3.0 - 2.0 * t);   // C1 smoothstep, no pop
    return 1.0 + (f - 1.0) * s;
}

// E4.2: the on-screen tag font size. `base_px` is the shipped constant
// (draw.cpp kLabelFontSize); a floor at or below it changes nothing.
inline int tag_font_px(int base_px, double min_px) {
    if (!(min_px > static_cast<double>(base_px))) return base_px;  // off
    return static_cast<int>(std::lround(min_px));
}

// E4.3: is this drone's glint LIT on this frame?
//
// Deterministic and de-synchronised: the per-drone offset is index * a prime,
// so a 5-ship does not strobe as one block. period <= 0 or duty <= 0 is OFF.
// Negative frame_count is folded back into range (the app's counter is
// monotonic from 0, but a probe may pass anything).
inline bool glint_lit(int frame_count, int drone_index, int period_frames,
                      double duty) {
    if (period_frames <= 0 || !(duty > 0.0)) return false;  // off
    const long long p = period_frames;
    const long long raw =
        static_cast<long long>(frame_count) +
        static_cast<long long>(drone_index) * 7919LL;  // prime => de-sync
    const long long phase = ((raw % p) + p) % p;
    const long long lit = static_cast<long long>(
        std::floor(std::min(1.0, duty) * static_cast<double>(p)));
    return phase < lit;
}

// E4.3: the glint's drawn radius [m] at slant range `dist_m`.
//
// A fixed-metre bead vanishes exactly where it is needed (a 0.9 m sphere at
// 4 km subtends ~0.2 mrad — well under a pixel at any sane FOV), so the radius
// is floored by an ANGULAR size. 0 metres and 0 mrad both mean OFF: returns 0
// and the caller draws nothing.
inline double glint_radius_m(double dist_m, double size_m, double min_mrad) {
    if (!(size_m > 0.0)) return 0.0;  // off
    const double ang = std::max(0.0, min_mrad) * 1e-3 * std::max(0.0, dist_m);
    return std::max(size_m, ang);
}

}  // namespace render
