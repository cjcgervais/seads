#pragma once

// The aurora-borealis CURTAIN GLSL (docs/little_planet_plan.md Stage 8, pulled
// forward). Gemini-generated (gemini-3.1-pro-preview, recipe
// tools/gemini/recipes/aurora_glsl.txt; provenance generated/gen_log.jsonl),
// reviewed + single-sourced here in the PURE render core (raylib-free) so the
// headless asset validator gates it — the same treatment as kScatterGLSL.
//
// It defines ONE function:
//   vec3 aurora_curtain(float rho, float az, vec2 phaseSC, float ovalCenter,
//                       float ovalWidth, float curtainScale, vec3 tintLow,
//                       vec3 tintHigh)
// the raw green->teal curtain color at one aurora-shell point. It declares NO
// uniform, uses NO clock and NO atan; animation rides phaseSC = vec2(sin phi,
// cos phi) via the sin(k*az+phi) angle-addition identity, so it is EXACTLY
// 2*pi-periodic in phi (no strobe on wrap — Fable-after P1-3). The sky FS
// aurora() wrapper does the ray-shell geometry (mirroring render::
// aurora_shell_colatitude) + the night/haze gates + the overall intensity.
//
// Concatenated into the sky FS AFTER kSkyGLSL, BEFORE the aurora() wrapper.

namespace render {

extern const char* const kAuroraGLSL;

}  // namespace render
