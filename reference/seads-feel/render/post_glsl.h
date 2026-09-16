#pragma once

// The S1 "stereoscope" POST-PROCESS fragment shader source (docs/
// stereoscope_s1_plan.md). Kept in the PURE render core (raylib-free, like
// scatter_glsl.cpp / star_glsl.cpp) so the headless asset validator
// (test_asset_validator) can gate the generated body — assert its declared
// uniforms are EXACTLY the allowlist and no time/clock token smuggles a feel
// clock into render/ (the grain seed is uFrameCount, an app-owned COSMETIC
// counter, not a clock read in render/).
//
// The chain (Fable-BEFORE 2026-07-10, SOUND-WITH-FIXES, all P0/P1 folded):
//   FXAA -> display-space contrast sigmoid (NO ACES) -> silver split-tone
//   (saturation-gated: mono->silver, planes pass through) -> warm halation
//   (screen blend) -> vignette -> film grain (terminal, animated).
// Everything works in ONE space: display-referred. The scene RT is RGBA16F
// (banding on smooth mono gradients) with alpha RESERVED for a future S6
// transmittance-grade; the depth attachment is a real DEPTH24 texture, sampled
// here only by the debug-viz path (uDebugMode==1) that proves the FBO is alive
// in S1 — the S6 DoF proper reconstructs RADIAL distance via invProj (the
// off-center frustum breaks the symmetric form; Fable Q7).
//
// Pairs with raylib's DEFAULT fullscreen vertex shader (it emits fragTexCoord +
// fragColor, which this FS uses) — no custom VS, so no undefined-FS-input trap.

namespace render {

extern const char* const kPostFS;  // the fullscreen post-process fragment shader

}  // namespace render
