#pragma once
// S4 building massing (stereoscope-sudbury): real OSM footprints extruded into
// flat / gabled / hipped prisms, draped on the planet's own height field. Consumes
// the baked prism batches in render/sudbury_gis.gen.h (unit dirs + per-vertex height
// above terrain + a per-building tone jitter). MONO, sun-lit (the S1 silver post
// tones it; the planes stay the only chroma). render/ reads state, writes nothing,
// reads no clock — house law.
#include <glm/vec3.hpp>
#include <vector>

#include "raylib.h"
#include "render/sphere_param.h"  // HeightField (radius_at — the shared drape)
#include "world/buildings.h"      // R4f collision prisms (the COLL section)

namespace render {

// R4f — the building COLLISION prisms for sim::Environment.obstacles: parses
// the SAME sudbury_buildings.bin the massing renders from (COLL section:
// per-footprint center dir + equiv-area radius + height; one bake, one truth)
// and builds the world::BuildingColliders spatial index ONCE (function-local
// static — the Environment holds the pointer for the app's life). Pure CPU
// parse, no GL (callable before the first frame, right where main.cpp builds
// env). Returns nullptr when the bin is missing/malformed/lock-stale or holds
// no colliders — buildings stay ghosts, the same graceful path as the massing
// renderer. inflate_reserve_m must be >= the query-time [buildings] inflate_m
// (the span-inserted index covers radius + reserve).
const world::BuildingColliders* building_colliders(double inflate_reserve_m,
                                                   double R_planet);

// FELT look dials, from config/world.toml [buildings] via draw.cpp (no bare look-
// constants in GLSL — house law). Values are mono luminance fractions [0,1].
struct BuildingLook {
    float wall_val = 0.42f;     // vertical-wall base value (mono)
    float roof_val = 0.62f;     // roof/cap base value (mono; lighter than walls)
    float ambient = 0.35f;      // flat ambient term
    float diffuse = 0.75f;      // N·L sun term gain
    float height_scale = 1.0f;  // vertical exaggeration of the baked heights
    // Night window lights (emissive on walls when the sun is down). The warm-white
    // tint is low-saturation so the S1 post keeps it silver; window_bright=0 = off.
    glm::vec3 window_color{1.0f, 0.98f, 0.95f};  // near-white glow (low-sat -> stays silver)
    float window_bright = 0.9f;    // emissive strength
    float window_lit_frac = 0.5f;  // fraction of a lit building's windows on
};

// Stage-2 per-batch cull cone (from the .bin's spatial tile bound): the runtime
// skips a batch whose cone is entirely beyond the horizon/range. Heroes + an old
// no-bound .bin get half_angle = pi (never culled).
struct BatchCull {
    glm::dvec3 center_dir{0.0, 0.0, 1.0};
    double half_angle = 3.14159265358979324;
};

struct BuildingSurfaces {
    bool ok = false;
    Shader shader{};
    Material mat{};
    std::vector<Mesh> meshes;       // one per baked GisBuildingBatch (+ glTF heroes)
    std::vector<BatchCull> culls;   // parallel to meshes — the horizon/distance cull cone
    double planet_R = 0.0;          // hf.R, for the per-frame angular horizon reach
    double cull_h_top = 800.0;      // horizon top-allowance (m): relief + tallest town
                                    // building*height_scale (Fable P2: NOT a bare const —
                                    // a re-bake/height dial must not erode the false-cull margin)
    BuildingLook look;
    int loc_wall = -1, loc_roof = -1, loc_amb = -1, loc_diff = -1;
    int loc_sun = -1, loc_eye = -1;
    int loc_wincol = -1, loc_winbright = -1, loc_winlit = -1;
};

// Build one draped mesh per baked batch. Every vertex sits at
// dir*(radius_at(dir) + h*height_scale) — the SAME height field the terrain mesh /
// trees / ribbons use, so a building never floats/sinks (anti-float). The base ring
// is baked slightly below terrain so wall feet emerge cleanly. texcoords.x carries
// the per-building tone jitter.
BuildingSurfaces build_building_surfaces(const HeightField& hf,
                                         const BuildingLook& look);
void unload_building_surfaces(BuildingSurfaces& b);

// Draw the prisms. Opaque, depth-on (writes the S6 depth FBO); run after the
// planet + water + ribbons and before the translucent props. Eye-relative model
// matrix (the double->float seam); sun_dir is the world light-travel dir (sun ->
// scene), eye is the world eye position (for the FS up reconstruction). Culling is
// OFF (the FS derives a flat facet normal + flips it eye-ward), so face winding is
// not visually load-bearing.
void draw_building_surfaces(BuildingSurfaces& b, const glm::vec3& sun_dir,
                            const glm::dvec3& eye);

}  // namespace render
