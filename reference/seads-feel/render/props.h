#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "raylib.h"
#include "world/props.h"

// SEADS render props — the S2 boreal tree scatter DRAW path
// (stereoscope-sudbury).
//
// The PLACEMENT math is world/props.* (PURE, gate-tested headlessly). This file
// is the raylib side: a procedural mono cross-quad conifer mesh, a per-chunk
// instance cache (deterministic → built once when a chunk first becomes
// visible), and ONE rlDrawMeshInstanced over the merged visible set. The scene
// is authored MONO (grayscale) so the S1 post pass tones it silver and the
// planes stay the only chroma; the geometry is opaque and writes the depth FBO
// (S6 DoF).
//
// Draw is eye-relative (the double→float rebase the planet uses) and runs right
// after draw_water on the same frame. A runtime instances>1 assert guards the
// count==1 instancing trap the green gate is blind to.

namespace render {

// Render-only look dials (config/world.toml [trees]) — NOT placement (that is
// world::TreeParams). No bare numeric look-constants in the GLSL/here.
struct PropLook {
    float base_width_m =
        4.5f;               // conifer base half-spread (crown width at scale 1)
    float ambient = 0.16f;  // mono ambient floor (feeds the silver post)
    float diffuse = 0.72f;  // mono lambert gain against the sun
    float fade_frac =
        0.16f;  // scale-fade band as a fraction of the effective range
};

struct PropRenderer {
    bool ok = false;
    Shader shader = {};
    Mesh mesh = {};  // procedural cross-quad conifer (mono, opaque, 3 planes)
    Material mat = {};
    world::DensityField
        density;  // assets/sudbury_treedensity.png (equirect L8)
    world::TreeParams params;
    PropLook look;
    // T6 scatter exclusion: portal cut disks, copied from TreeBuildParams.
    // Passed to place_chunk so trees never appear inside the terrain holes.
    std::vector<CutDisk> cuts;
    // S2b (WINTER_LAW sec2.4b): the trail/road corridor tree mask. Chad flew S2
    // and found trees standing in the snowmobile trails. Null lines =>
    // bit-identical to the pre-corridor scatter, exactly like empty cuts.
    world::CorridorMask corridor;

    // Per-chunk instance cache: each chunk's WORLD-ABSOLUTE float16 transforms
    // (16 floats/instance), placed + packed ONCE when the chunk first comes
    // into view and reused forever (deterministic scatter). World-absolute
    // means the moving camera NEVER invalidates a cached buffer — the per-frame
    // rebase is a single uEye uniform in the vertex shader, so a boundary
    // crossing only places the FEW new chunks + concatenates cached bytes (no
    // 320k recompute).
    std::unordered_map<std::uint32_t, std::vector<float>> cache;
    std::vector<std::uint32_t> ready;  // last uploaded ready set (sorted keys)

    // PERSISTENT VRAM instance buffer: the concatenated visible chunks,
    // uploaded only when the ready set changes (crossing a boundary), never per
    // frame. raylib's DrawMeshInstanced re-creates + re-streams the whole
    // buffer every call; this doesn't. All the instance data lives in VRAM.
    unsigned int instance_vbo = 0;  // rlgl VBO id (0 = not yet created)
    int instance_cap = 0;           // instances the VBO can hold (grow-only)
    int instance_count = 0;         // instances to draw
    std::vector<float> upload;      // scratch: the concatenated ready buffers

    int loc_sun = -1, loc_fade_start = -1, loc_fade_end = -1, loc_eye = -1;
    bool warned_empty = false;
};

// Loads the density PNG + builds the conifer mesh/shader. ok=false if the
// raster is missing (no trees — a logged, flagged degraded mode, never a silent
// crash). `cuts` (default empty) are portal cut disks forwarded to
// place_chunk so trees are excluded from inside terrain holes.
PropRenderer build_props(const std::string& asset_dir,
                         const world::TreeParams& p, const PropLook& look,
                         const std::vector<CutDisk>& cuts = {},
                         const world::CorridorMask& corridor = {});
void unload_props(PropRenderer& r);

// Draws all visible trees in ONE instanced call. hf is the planet's height
// field (placement anti-float). Eye-relative. No-op if !ok or nothing is
// visible.
void draw_props(PropRenderer& r, const HeightField& hf,
                const glm::vec3& sun_dir, const glm::dvec3& eye);

}  // namespace render
