#pragma once

// The AIR FIELD shared GLSL (docs/bubble_atmosphere_spec.md §1.1/§1.2): the
// [atmosphere] AirField uniform block + the pure functions air_at() (a
// line-by-line float transliteration of render/air_field.h's air_at, the
// spec's H1 fence #2) and air_optical_depth() (the uniform-step trapezoid
// march that makes the dome VISIBLE, §1.2). Kept in the PURE render core
// (raylib-free, like render/scatter_glsl.h) so the headless source validator
// can gate it with no GL context.
//
// Concatenation order (sky.cpp / planet.cpp / stars.cpp all follow this):
// "#version 330" + <shader-local decls, incl. uUp/uEyeAlt> + kSkyUniformsGLSL
// + kAirFieldUniformsGLSL + kScatterGLSL + kAirFieldGLSL + kSkyGLSL + <the
// shader's own main()>. kAirFieldGLSL's air_eye_pos()/air_sky_max_dist()
// helpers reference uUp/uEyeAlt/uAirPlanetR/uAirMarchMaxM by name (the same
// "reference the block, don't parametrize" convention kSkyGLSL already uses
// for uHorizonElev), so every consumer must declare uUp/uEyeAlt with those
// exact names before this block — true today in sky.cpp, planet.cpp, and
// star_glsl.cpp's kStarFsDecls.

namespace render {

extern const char* const kAirFieldUniformsGLSL;
extern const char* const kAirFieldGLSL;  // air_at, air_optical_depth, helpers

}  // namespace render
