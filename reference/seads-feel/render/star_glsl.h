#pragma once

// The star pass's HAND-AUTHORED GLSL (docs/little_planet_plan.md Stage 4), kept
// as const-char strings in the PURE render core (no raylib) so the headless
// asset validator can gate their uniform sets — the same clock/state-backdoor
// guard the Gemini-generated kScatterGLSL gets. stars.cpp (raylib-facing)
// assembles the full FS as: "#version 330" + kSkyUniformsGLSL + kScatterGLSL +
// kSkyGLSL + kStarFsBody (the star-local uniforms + main, which calls the shared
// sky_luminance for extinction).

namespace render {

// The star vertex shader: billboards each catalog star in VIEW space. matModel
// (= the DrawMesh transform = the sky wheel), matView, matProjection are
// raylib-auto-uploaded by name. Allowed uniforms: {matModel, matView,
// matProjection, uRStar, uStarSizeView}.
extern const char* const kStarVS;

// The star fragment shader is assembled as: "#version 330" + kStarFsDecls +
// kSkyUniformsGLSL + kScatterGLSL + kSkyGLSL + kStarFsMain. The DECLS
// (varyings + star-local uniforms + out) come BEFORE the shared sky GLSL because
// the shared sky_luminance references uHorizonElev — so it must be declared
// first, exactly as sky.cpp / planet.cpp declare it ahead of kSkyGLSL. The MAIN
// comes AFTER (it calls sky_luminance). Allowed uniforms: {uSunDir, uUp, uEyeAlt,
// uHorizonElev, uStarBrightness, uMagRef, uGlareCos, uWashLum}.
extern const char* const kStarFsDecls;
extern const char* const kStarFsMain;

}  // namespace render
