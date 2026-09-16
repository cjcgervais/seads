#pragma once
// PRECIPITATION renderer (docs/weather_seasons_plan.md W3) — the raylib billboard
// particle pass that draws the pure world-anchored field (render/precip.h). Lives
// in the seads exe (like render/lights). render/ reads state, writes nothing,
// reads no clock — the phase/intensity are app-owned inputs. Mono/silver: snow
// white, rain grey (no new chroma).
//
// AS-3 (atmosphere rung 2026-09-12): the app builds TWO of these — a NEAR
// lattice (small flakes, tight box) and a FAR "veil" (bigger flakes, wide box,
// low opacity) — drawn far-then-near so the snow has DEPTH instead of being a
// uniform globe of dots. One renderer TYPE, two configurations: there is no
// second code path to fork.

#include <glm/glm.hpp>
#include <glm/vec3.hpp>
#include <vector>

#include "raylib.h"
#include "render/camera.h"  // CameraPose (streak orientation from the view basis)
#include "render/precip.h"  // PrecipSdf (the injected AS-1 underground gate)
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
    // --- AS-3 look terms. Every one is OFF at its default here. ---
    float size_var = 0.0f;     // per-flake size  x [1-v, 1+v]
    float alpha_var = 0.0f;    // per-flake alpha x [1-v, 1]
    float density_exp = 0.0f;   // hashed per-cell cull exponent (0 = keep all)
    float density_soft = 0.0f;  // width in keep_p a culled cell fades in over
    float inner_fade_m = 0.0f;  // radial hole around the eye (0 = off) -- the
                                // FAR veil's big flakes must not reach the face
    float sway_m = 0.0f;       // zero-mean lateral sway amplitude (m)
    float rim_dark = 1.0f;     // edge darkening of the flake (1 = off); the
                               // white-on-white contrast dial (mono, no chroma)
    // --- AS-1: the underground gate band (m). 0 = a hard step at the wall. ---
    float rock_band_m = 2.0f;
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
    int loc_upview = -1, loc_halfw = -1, loc_halfl = -1, loc_color = -1,
        loc_rim = -1;
    std::vector<float> verts;         // 3 * 4 * particle_count (per-frame scratch)
    std::vector<unsigned char> cols;  // 4 * 4 * particle_count (RGBA: r = the
                                      // per-flake size multiplier / 2, a = fade)
    // AS-1 instrumentation (read by the app's perf line; render/ writes no clock
    // itself — these are plain counters set by the last draw).
    int last_sdf_calls = 0;  // rock-SDF evaluations actually PAID last draw
    int last_drawn = 0;      // compacted quads actually drawn last frame
    // AS-1 — THE WORLD-ANCHORED ROCK CACHE. The tunnel-net SDF is an ~80-segment
    // swept-ellipse scan: measured at ~1.6 us a call on this box, so testing
    // every visible flake every frame cost 16 ms at a mouth. But the lattice is
    // WORLD-ANCHORED: a cell keeps its world position for as long as it is in
    // the box, and rock does not move. So the answer is cached per WORLD CELL,
    // in a toroidal (mod span) table keyed by the absolute cell index — a box
    // shift of one cell then re-evaluates only the entering slab instead of the
    // whole block, and a stationary eye pays nothing at all.
    //
    // The cached value is the SDF at the cell's CENTRE, not at the flake's
    // current position (the spec's sanctioned per-cell fallback): a flake is
    // within ~1.4 cells of its centre, so the gate's edge is soft at the cell
    // scale — which is the flake spacing, i.e. the resolution the field has
    // anyway.
    std::vector<glm::ivec3> rock_key;  // absolute cell cached in each slot
    std::vector<float> rock_sd;        // its signed distance (m)
    bool rock_cache_armed = false;     // false => every slot is stale
    const void* rock_cache_ctx = nullptr;  // the net the cache was built against
};

PrecipRenderer build_precip_renderer(const PrecipLook& look);
void unload_precip_renderer(PrecipRenderer& r);

// Draw the precip for this frame. `season` gates the kind (Winter=snow /
// Spring=rain / else no-op); `intensity` = the snowfall field at the eye in
// [0,1] (no-op at ~0 — clear air / between cells); `fall_phase` is the app-owned
// wrapped phase.
//
// AS-1: `rock_sdf`/`rock_ctx` inject the tunnel-net signed distance (see
// render/precip.h). rock_sdf == nullptr => bit-identical to no gate at all.
// Alpha-blended, depth-test ON (terrain/planes occlude), depth-write OFF; runs in
// the translucent tier after the opaque bodies.
// `ground_r` is the planet radius of the local terrain under the eye (metres);
// it is the second half of the underground gate (see precip_rock_alpha).
// ground_r <= 0 disables the terrain term, leaving the SDF alone.
void draw_precip_renderer(PrecipRenderer& r, const CameraPose& pose, Season season,
                          double intensity, double fall_phase,
                          PrecipSdf rock_sdf = nullptr,
                          const void* rock_ctx = nullptr, double ground_r = 0.0);

}  // namespace render
