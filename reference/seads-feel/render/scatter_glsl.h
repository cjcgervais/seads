#pragma once

// The atmospheric-scatter GLSL function (docs/little_planet_plan.md Stage 3),
// the FIRST Gemini-asset-factory output ingested into a SEADS shader. Kept in
// the PURE render core (raylib-free, like render/rig.cpp's mirror shader
// source) so the headless asset validator (test_asset_validator) can gate the
// generated body — assert it declares NO uniform of its own (the clock/state
// backdoor guard) — with no GL context. sky.cpp string-concatenates it into
// BOTH the sky FS and the planet FS, AFTER the shared [atmosphere] uniform
// block (kSkyUniforms GLSL) it references, so scatter() is defined before
// sky_color() calls it.
//
// The body is Gemini-drafted (gemini-2.5-pro) + Fable-vetted (signs/domain
// SOUND, P0 horizon zero-gate, ×0.25/×0.5 scaling, 1/4pi) + Opus-reviewed (the
// draft's fixed-axis (0,1,0) view term was corrected to the sphere-seam local
// `up`, added as a parameter). It DECLARES no uniforms — it references the
// [atmosphere] block's u_rayleigh / u_mie / u_scatterStrength / u_mieG /
// uMieTintDay (the last from the S-airdome kAirFieldUniformsGLSL block,
// concatenated before this one — see render/air_field_glsl.h).
//
// REWRITTEN (docs/bubble_atmosphere_spec.md §1.4, 2026-08-09): the master gate
// is now `airAmt` (an render::air_optical_depth() amount, [0,1)) instead of
// the old `hazeAmount` weather scalar — the same signature-and-final-multiply
// shape, a different upstream signal. Rayleigh dropped its rim-only
// horizonW weighting (space-first is now bubble-relaxed: the zenith inside a
// dome IS blue) in favor of a real (3/16pi)(1+cos^2) phase function; Mie
// gained a day-tint (near-white) blended to the existing sunset orange.

namespace render {

extern const char* const kScatterGLSL;  // the vec3 scatter(...) function

}  // namespace render
