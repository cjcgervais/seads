#pragma once
// CC1 — Superstack smoke (Living Copper Cliff, docs/copper_cliff_plan.md). A mono
// drifting plume off the Copper Cliff Superstack top, drawn as CAMERA-FACING alpha
// billboard puffs whose positions are a PURE function of a wrapped phase the APP
// computes from t_cel in DOUBLE (render/ never sees raw t_cel — house law + Fable
// P1: a float32 t_cel stutters at ~10 h). Mono/silver: the S1 post silvers it (NOT
// a chroma exception — that is CC3's hot slag). render/ reads state, writes nothing,
// reads no clock.
#include <glm/vec3.hpp>
#include <vector>

#include "raylib.h"
#include "render/sphere_param.h"  // HeightField (radius_at — the shared drape)

namespace render {

// FELT dials from config/world.toml [smoke] via draw.cpp (no bare look constants in
// GLSL). Mono grey (low-sat -> silvered by the post).
struct SmokeLook {
    int puffs = 28;                    // billboard count rising up the column
    glm::vec3 color{0.60f, 0.60f, 0.62f};  // mono grey plume (low-sat -> silver)
    float rise_m = 900.0f;             // vertical rise over one puff life
    float drift_m = 560.0f;            // downwind horizontal drift over a life
    float r0_m = 26.0f;                // puff radius at birth (m)
    float r1_m = 155.0f;               // puff radius at death — expands as it rises
    float opacity = 0.42f;             // peak per-puff alpha
    float jitter_m = 16.0f;            // fixed per-puff lateral scatter (breaks the line)
    glm::vec3 wind{1.0f, 0.15f, 0.0f}; // world wind hint (projected to the stack tangent)
};

struct SmokeRenderer {
    bool ok = false;
    Shader shader{};
    Material mat{};
    Mesh mesh{};                 // puffs*4 verts, DYNAMIC — rebuilt each frame (few puffs)
    SmokeLook look;
    glm::dvec3 anchor_top{};     // stack-top world position = dir*(radius_at(dir)+H)
    glm::dvec3 up{};             // stack radial (unit)
    glm::dvec3 wtan{};           // wind projected to the tangent plane (unit)
    glm::dvec3 wtan2{};          // the other tangent (for the fixed jitter)
    std::vector<float> vbuf;     // scratch: positions (rebuilt per frame)
    std::vector<float> nbuf;     // scratch: normal.xyz = (size_m, alpha, 0) per vert
    std::vector<int> order;      // scratch: back-to-front puff order
    int loc_color = -1;
};

// Build the plume renderer. anchor_dir = the Superstack axis unit dir (computed
// offline through the LOCKED projection); stack_h = the stack height [m]. The top
// drape is read from the SAME height field as the terrain/buildings (anti-float).
SmokeRenderer build_smoke_renderer(const HeightField& hf, const SmokeLook& look,
                                   const glm::dvec3& anchor_dir, double stack_h);
void unload_smoke_renderer(SmokeRenderer& r);

// Draw the plume. `phase` = frac(t_cel*rate) in [0,1), computed in DOUBLE by the
// app (finished phase, never raw t_cel). Alpha-blended, depth-test ON (terrain
// occludes the base) / depth-write OFF, puffs sorted back-to-front by view depth.
// No-op if !ok.
void draw_smoke_renderer(SmokeRenderer& r, double phase, const glm::dvec3& eye);

}  // namespace render
