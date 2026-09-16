#pragma once

#include <glm/vec3.hpp>
#include <vector>

#include "raylib.h"
#include "render/sphere_param.h"  // HeightField + facet_radius_at (the R4d drape) + CutDisk

// Rivers as MIRROR water (stereoscope-sudbury). Chad flew the first cut (bright-silver
// draped ribbons) — "look like white lines, not rivers" — and wants the LAKE mirror
// finish: a black mirror reflecting the sky (+ a procedural star sparkle) that RUNS INTO
// the lakes. The river GEOMETRY is the baked kind-3 GisRibbonPath draped strips; here we
// upload one raylib Mesh per river path DRAPED on the SHARED height field
// (radius_at(dir)+lift, the same anti-float field the terrain/roads/trees use) with a
// RADIAL normal (water is locally horizontal), and draw it with the PLANET's own
// program (uForceWater=1) exactly like the lake surfaces (render/water_surface.*). So a
// river shades identically to the planet's own water pixels — the mirror cannot fork
// from the limb (H1). The star sparkle lives ONCE in the shared planet water branch
// (render/planet.cpp), so lakes AND rivers reflect it.
//
// T24-river (Chad, 2026-07-22, the tail of the T24 fly: "there is another black
// line there" over the Errington entrance — a BLACK MIRROR river strip, the one
// ribbon kind T24 skipped: build_ribbon_surfaces explicitly excludes kind==3 so
// rivers never routed through render/ribbon_clip.h's excavation clip). Rivers now
// take the SAME cuts list + the SAME ribbon_indices_outside_cuts (pure,
// RAYLIB-free, already pinned by test_sudbury_gis.cpp's T24 case across every
// path kind incl. rivers) as the road/trail ribbons — no forked cut math.

namespace render {

struct Planet;  // draw uses its shader + material (fwd-declared; full type in .cpp)

struct RiverSurfaces {
    bool ok = false;
    std::vector<Mesh> meshes;  // one per baked kind-3 river path (own vtx/idx buffers)
};

// Build one draped mesh per baked kind-3 (river) GisRibbonPath. Every vertex sits at
// facet_radius_at(dir, subdiv, tiles) + lift_m — the RENDERED terrain surface, same
// R4d conformance as the road ribbons (radius_at-drape floated/buried the strip by
// the facet-vs-field gap, metres on rough terrain); lift_m is a small clearance that
// beats z-fighting. Normals are radial (the mirror normal). subdiv/tiles must be the
// live [planet] mesh build values. `cuts` is the SAME excavation cut-disk list the
// terrain/roads/trees/trails already drop triangles against (single-sourced from
// app/main.cpp's planet_cuts) — a river path hovering across the Errington/Murray
// excavation or the trench steps is clipped exactly like a road; empty cuts is the
// bit-identical no-op (T24-river). Returns ok=false (empty) if the bake emitted no
// river paths (roads-only / degraded waterway fetch) or every river path was fully
// swallowed by a cut.
RiverSurfaces build_river_surfaces(const HeightField& hf, double lift_m, int subdiv,
                                   int tiles, const std::vector<CutDisk>& cuts);

void unload_river_surfaces(RiverSurfaces& r);

// Draw the river mirror meshes. MUST run immediately after draw_planet_mesh on the same
// frame — it reuses the planet's program + material and relies on all the sky/scatter/
// moon/aurora uniforms draw_planet_mesh just bound (uForceWater=1, then back to 0).
// Slope-scaled polygon offset + eye-relative model matrix (the double->float seam), like
// the lakes. SEADS_NO_RIVERS=1 bypasses (A/B). No-op if the planet or the rivers failed.
void draw_river_surfaces(const Planet& p, const RiverSurfaces& r,
                         const glm::dvec3& eye);

}  // namespace render
