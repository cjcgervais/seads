#pragma once

// ★ ROAD-REPAIR (2026-09-09) — THE SNOW SPARKLE, IN ONE PLACE.
//
// Chad, from the road: "the sparkle drops out at every road edge." It did, and
// the reason was structural rather than tuned: the snow sparkle lived inside
// the planet fragment shader and NOWHERE ELSE, so every surface drawn ON TOP
// of the planet along a road — the ribbon deck, and above all the SF2 snowbank
// strips, which are metres-wide bands of drawn SNOW running down both sides of
// every plowed street — was unlit, un-sparkled, flat white. Driving a road
// meant driving a corridor with the winter deliberately switched off in it.
//
// The fix could not be "write the sparkle again in the bank shader": two
// copies of a look Chad has signed drift the first time either is dialled, and
// this tree's standing law is one function, two consumers, zero fork
// (WINTER_LAW). So the lattice and the glint are THIS string, concatenated
// into both the planet FS (render/planet.cpp) and the bank FS
// (render/ribbons.cpp). Same cells, same hash, same anti-alias fades, same
// distance cutoff — a bank crest and the field beside it glint off ONE lattice
// and cannot separate.
//
// It declares NO uniforms (the asset-validator discipline kScatterGLSL keeps):
// every dial is a parameter, so a consumer chooses its own amplitude/exponent
// source without this file knowing about either shader's uniform block.
//
// ⚠ snow_sparkle_glint() calls fwidth(). Per the GLSL spec a derivative in
// non-uniform control flow is undefined, so CALL IT IN UNIFORM CONTROL FLOW —
// outside any branch that can differ between neighbouring fragments. Both
// shipped consumers call it at the top level of main(), which is the same
// discipline planet.cpp already had to keep for the lake glint.

namespace render {

// p_hash13 + snow_sparkle_cell/_host/_facet/_glint.
extern const char* const kSnowSparkleGLSL;

}  // namespace render
