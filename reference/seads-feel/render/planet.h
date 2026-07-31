#pragma once

#include <glm/vec3.hpp>

#include "raylib.h"
#include "render/sphere_param.h"

// Textured, DEM-displaced planet (render-only; the physics crash surface is
// STILL the perfect sphere at R — SPEC §6, altitude = length(position) - R).
// All relief lives in the visible mesh; nothing here flows back into sim/.
//
// Geometry is a CUSTOM CUBESPHERE (6 face meshes): uniform texel density and
// NO pole singularity, unlike a UV sphere (equirectangular-on-UV pole pinch).
// raylib's Mesh.indices is unsigned short (<=65536 verts), so each cube face
// is its own mesh; all six share one material/shader.
//
// The equirectangular color map is sampled PER-FRAGMENT from the interpolated
// surface direction (u = atan2(z,x), v = asin(y)) with seam-safe textureGrad
// gradients — so there is no vertex UV seam and the poles are exact. The
// lat/long graticule is drawn in the same fragment shader (coplanar, welded to
// the surface) instead of a wire shell floated proud of it.

namespace render {

struct Planet {
    bool ok = false;
    Mesh faces[6] = {};    // cubesphere faces (own vertex/index buffers)
    Material mat = {};     // shared: lighting+graticule shader + color map
    Texture2D color = {};  // equirectangular Earth albedo
    Shader shader = {};
    int loc_sun_dir = -1;
    int loc_grid_color = -1;
    int loc_u_offset = -1;
    double R = 0.0;
    // The persistent PROCESSED height field (post-blur), the SINGLE SOURCE the
    // mesh was built from and the future props / airstrip ground-contact query
    // read (docs/world_build_plan.md §1/§2). Retained after the raylib DEM
    // buffer is freed — the H1 anti-fork. Empty when !ok.
    HeightField height;
};

// Loads color+DEM from asset_dir, builds the displaced cubesphere. relief_scale
// is the metres of radial displacement at full-white DEM (255); ocean (0) stays
// at R. subdiv is vertices-per-edge per face (<=256 for the ushort index cap).
// dem_blur_radius softens sub-quad DEM detail before it reaches the normals
// (coupled to relief_scale; a heavier displacement needs a heavier blur).
// Returns Planet{ok=false} on any asset/GPU failure (caller falls back).
// u_offset rotates the equirectangular map in longitude (fraction of 360°) so
// chosen terrain sits under a given surface point — e.g. steep mountains under
// the spawn sub-point for legible relief.
Planet load_planet(const char* asset_dir, double R, double relief_scale,
                   int subdiv, double u_offset, int dem_blur_radius);

void unload_planet(Planet& p);

// Draws the six faces eye-relative (world re-based to the eye, matching the
// double->float seam in draw.cpp). sun_dir is the world-space light travel
// direction (from the sun toward the scene).
void draw_planet_mesh(const Planet& p, const glm::dvec3& eye,
                      const glm::vec3& sun_dir);

}  // namespace render
