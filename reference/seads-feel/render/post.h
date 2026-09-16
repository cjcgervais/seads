#pragma once

#include <glm/glm.hpp>

// S1 stereoscope POST pass (docs/stereoscope_s1_plan.md). Owns the scene FBO —
// an RGBA16F color texture (Fable P1: smooth mono sky/haze gradients band under
// a >1-slope sigmoid in RGBA8; alpha RESERVED for a future S6 transmittance
// grade) + a real DEPTH_COMPONENT24 TEXTURE (Fable P1: raylib's default FBO
// depth is a renderbuffer, unsamplable) — and resolves it through the
// post-process FS (render::kPostFS). The scene renders into this FBO, then a
// fullscreen pass applies FXAA/tone/split-tone/halation/vignette/grain, then the
// HUD draws RAW on top (SPEC §9.2 — no grain on the reticle).
//
// This is the ONLY raylib in the post path; the FS body + look math are the pure
// (validator-gated) render::kPostFS. All look DIALS come from config/world.toml
// [tone] -> PostParams (no bare look-constants here).

namespace render {

// Look dials, mapped by the app from cfg::WorldParams::tone (display-space).
struct PostParams {
    float contrast = 1.0f;
    float lift = 0.0f;
    float grain = 0.0f;
    glm::vec3 split_shadow{1.0f};
    glm::vec3 split_hi{1.0f};
    // ★ S1b-3 WINTER NIGHT. split_hi is deliberately WARM ([1.07,1.01,0.90] --
    // the flown silver look), and for low-saturation pixels the split-tone
    // REPLACES hue with Y*tint rather than multiplying it. So once winter night
    // snow became bright, it came out CREAM, and no amount of tinting the light
    // itself could fix it (measured: a cool night light moved R/B only
    // 1.129 -> 1.116, while neutralising split_hi gave 0.972).
    //
    // split_hi_night is what the highlight tint becomes at full night. DAYLIGHT
    // IS UNTOUCHED -- Chad approved that look, and the blend is driven purely by
    // sun elevation, so at day this is bit-identity.
    glm::vec3 split_hi_night{1.0f};
    float sat_c0 = 0.0f;
    float sat_c1 = 1.0f;
    float sat_dark = 0.04f;
    float halation = 0.0f;
    float halation_threshold = 1.0f;
    glm::vec3 halation_tint{1.0f};
    float vignette = 0.0f;
    float fxaa = 0.0f;
    // S6 far-field-only depth of field (view-space m / screen px). dof_radius=0
    // disables (SEADS_NO_DOF / [dof] enabled=0). focus_start < focus_end always.
    float focus_start = 0.0f;
    float focus_end = 1.0f;
    float dof_radius = 0.0f;
    float sky_coc = 0.0f;
    // S6 "printed card" halftone/dither MODE (off by default -> bit-exact identity).
    // halftone_mode: 0 off · 1 AM halftone dots · 2 ordered (Bayer) dither. The mono
    // silver world becomes a screen-locked print; the planes keep their color (sat
    // gate). [halftone] section / SEADS_HALFTONE env; halftone_scale loader-floored.
    int halftone_mode = 0;
    float halftone_scale = 4.0f;
    float halftone_angle = 0.7853982f;  // ~45deg classic single-ink screen angle
    float halftone_soft = 1.0f;
    float halftone_ink = 0.05f;
    float halftone_grain_mul = 0.25f;
};

// Store the look dials (once, from the app). Cheap; no GL touched.
void set_post_params(const PostParams& p);

// Per-frame night amount [0,1] (0 = day => split_hi used exactly as configured,
// 1 = full night => split_hi_night). Set from sun elevation by the caller; the
// post pass reads no clock and knows nothing about the sky.
void set_post_night(float night_amt);

// ★★ THE SC1 HELMET FOG IS GONE (Chad's ruling, 2026-08-17: "no more frost
// just turn it off ... but the temperature mechanic that affect the speed of
// the ride kernel should be untouched, Just how it affects my view").
//
// There is no `set_post_fog`, no `uFog`, no bypass flag and no 'H' key any
// more. The whole view-side effect is deleted rather than defaulted off: a
// dormant blinder behind a live toggle had already misfired once (the toggle
// was seeded from the bypass, so the FIRST press turned frost ON, and the
// state was invisible until a tune key armed the readout).
//
// ★ WHAT SURVIVES, DELIBERATELY AND UNTOUCHED: the COLD MECHANIC. `world::h`,
// `world::air_temp_c` and `world::frost` are exactly as they were, cold still
// hardens the snowpack and still makes the machine faster, and `sled_frost`
// still drives the dash-plate rime art. This ruling was about the view, not
// the ride -- do not "tidy" world/cold.* on the strength of this note.

// Redirect rendering into the scene FBO (creates/resizes it to sw*sh on demand)
// and clear it. Call INSIDE BeginDrawing, before BeginMode3D. No-op-safe if the
// FBO can't be built (returns false; caller renders to the backbuffer instead).
bool post_begin(int sw, int sh);

// End the FBO capture and resolve it to the backbuffer through kPostFS.
// frame_count = an app-owned cosmetic counter (the animated-grain seed; render/
// reads no clock). debug_mode: 0 normal, 1 linearized-depth viz, 2 sat mask.
void post_end_and_resolve(int sw, int sh, int frame_count, int debug_mode);

// Free GL resources (window teardown). Safe to call when nothing was built.
void unload_post();

}  // namespace render
