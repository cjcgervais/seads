#pragma once
// PRECIPITATION renderer (docs/weather_seasons_plan.md W3) — the raylib billboard
// particle pass that draws the pure world-anchored field (render/precip.h). Lives
// in the seads exe (like render/lights). render/ reads state, writes nothing,
// reads no clock — the phase/intensity are app-owned inputs. Mono/silver: snow
// white, rain grey (no new chroma).

#include <glm/vec3.hpp>
#include <vector>

#include "raylib.h"
#include "render/camera.h"  // CameraPose (streak orientation from the view basis)
#include "render/season.h"  // Season (the precip gate)

namespace render {

// The look (config/world.toml [precip] via draw.cpp — no bare look constants in
// GLSL). The two kinds share the lattice; season selects.
struct PrecipLook {
    // Lattice / box (shared by both kinds).
    float cell_size_m = 3.0f;  // world grid spacing (flake spacing, m)
    float box_half_m = 21.0f;  // half-extent of the eye-following box (fade radius, m)
    float wrap_fade = 0.12f;   // vertical wrap fade fraction (C0 hides the fall reset)
    glm::vec3 color{0.90f, 0.93f, 0.97f};  // near-white mono flake/streak tint
    // Snow (Winter): round soft flecks, slow.
    float snow_size_m = 0.28f;   // flake radius (m)
    float snow_opacity = 0.75f;  // alpha at full intensity
    // Rain (Spring): thin streaks elongated along local_up, fast.
    float rain_size_m = 0.05f;    // streak half-width (m)
    float rain_streak_m = 1.30f;  // streak half-length along local_up (m)
    float rain_opacity = 0.55f;   // alpha at full intensity (grey read)
};

struct PrecipRenderer {
    bool ok = false;
    Shader shader{};
    Material mat{};
    Mesh mesh{};  // (2H+1)^3 billboard quads, vertices+colors updated per frame
    PrecipLook look;
    int half_cells = 0;      // H (>= box_half/cell + 1.5 so the fade sphere is
                             // fully inside the block; clamped to one ushort batch)
    double fade_radius_m = 0.0;  // effective boundary-fade radius = min(box_half,
                                 // (H-1.5)*cell) so EVERY faded flake is in-block
                                 // (Fable W3 P0: no re-bin pop / no clamp wall)
    int particle_count = 0;  // (2H+1)^3
    int loc_upview = -1, loc_halfw = -1, loc_halfl = -1, loc_color = -1;
    std::vector<float> verts;         // 3 * 4 * particle_count (per-frame scratch)
    std::vector<unsigned char> cols;  // 4 * 4 * particle_count (RGBA, a = fade)
};

PrecipRenderer build_precip_renderer(const PrecipLook& look);
void unload_precip_renderer(PrecipRenderer& r);

// Draw the precip for this frame. `season` gates the kind (Winter=snow /
// Spring=rain / else no-op); `intensity` = weather_cell(eyeDir) in [0,1] (no-op
// at ~0 — clear air / between cells); `fall_phase` is the app-owned wrapped phase.
// Alpha-blended, depth-test ON (terrain/planes occlude), depth-write OFF; runs in
// the translucent tier after the opaque bodies.
void draw_precip_renderer(PrecipRenderer& r, const CameraPose& pose, Season season,
                          double intensity, double fall_phase);

}  // namespace render
