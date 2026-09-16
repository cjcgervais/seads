#pragma once

#include <glm/vec3.hpp>
#include <vector>

#include "raylib.h"

// Dedicated flat lake mirror surfaces (stereoscope-sudbury, post-S0-REV2).
//
// Chad flew S0-REV2 (2026-07-10): the major landable lakes read as "straight-
// edged cover strips" because the coarse ~120 m terrain MESH facets the shore
// cliff across the flat lake at grazing (landing) angles. Fix = render each
// major lake as its OWN smooth mirror surface at the water altitude, DECOUPLED
// from the terrain mesh. The bake (offline_tool/sudbury_water.py) tessellates
// each lake's real OSM outline into unit sphere dirs + a triangle list, emitted
// into render/sudbury_gis.gen.h. Here we upload one raylib Mesh per lake at
// radius elev_m + lift and draw it with the PLANET's own shader+material
// (uForceWater=1) so the mirror look cannot fork from the planet limb (Fable
// ★3 / H1: aurora, sky_aerial, and every sky/scatter uniform are shared by
// construction — same GL program, bound the same frame by draw_planet_mesh).

namespace render {

struct Planet;  // draw uses its shader + material (fwd-declared; full type in .cpp)

struct WaterSurfaces {
    bool ok = false;
    std::vector<Mesh> meshes;  // one per lake (own vertex/index buffers)
};

// Builds one lake mesh per kSudburyWaterLakes entry. Every vertex sits at radius
// elev_m + lift_m (Fable ★4: the lift beats z-fighting with the flattened
// lakebed + covers the coarse shore facets); normals are radial. Returns
// ok=false (empty) if the bake emitted no lakes.
WaterSurfaces build_water_surfaces(double lift_m);

void unload_water_surfaces(WaterSurfaces& w);

// Draws the lake mirror meshes. MUST run immediately after draw_planet_mesh on
// the same frame — it reuses the planet's program + material and relies on all
// the sky/scatter/moon/aurora uniforms draw_planet_mesh just bound. Flips
// uForceWater to 1 (then back to 0) and applies a slope-scaled polygon offset so
// the water wins the depth tie at grazing range. Eye-relative (MatrixTranslate
// (-eye), the identical double->float seam the planet uses). SEADS_NO_WATER=1
// bypasses (A/B). No-op if either the planet or the water meshes failed to build.
void draw_water_surfaces(const Planet& p, const WaterSurfaces& w,
                         const glm::dvec3& eye);

}  // namespace render
