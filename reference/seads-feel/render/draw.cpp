#include "render/draw.h"

#include <algorithm>
#include <array>
#include <cctype>  // toupper for the HUD season tag (W1)
#include <cmath>
#include <cstdio>
#include <cstdlib>  // getenv/atoi for the SEADS_POST_DEBUG viz gate
#include <cstring>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <string>

#include "combat/fx_curves.h"  // pure hit-FX curves (spark/fireball/debris) — Fable 2026-07-13
#include "raylib.h"
#include "raymath.h"  // MatrixFrustum (off-center lens-shift projection)
#include "render/bubble_map.h"  // M-KEY map: aeqd projection + ellipse boundary sampler (pure)
#include "render/buildings.h"  // S4 building massing (extruded OSM footprints)
#include "render/lights.h"     // street-lamp night point lights
#include "render/map_font.h"   // M-KEY map: the shared Arial face (S-mapread)
#include "render/map_screen.h"  // ★ L4: the M-KEY chart, its own TU
#include "render/map_style.h"  // M-KEY map: config-driven colours/sizes/declutter
#include "render/orient_cues.h"  // S-cues (comfort): ghost horizon + bank arc
#include "render/planet.h"
#include "render/sled_marker.h"  // R4a: the machine beacon (Chad 2026-08-25)
#include "render/post.h"  // S1 stereoscope post pass (scene FBO + resolve)
#include "render/precip_draw.h"  // W3 season-gated snow/rain renderer
#include "render/probe.h"
#include "render/props.h"  // S2 boreal tree scatter (instanced cross-quads)
#include "render/pump_frame.h"  // S-pumpcube: the neon team wire cube around a pump
#include "render/readout.h"
#include "render/ribbons.h"  // S3 draped linework ribbons (roads / snowmobile trails)
#include "render/rider_pose.h"  // SUDBURIAN R1a: the pure rider pose math
#include "render/rig.h"  // Fleet Rig hierarchy + glm->raylib bridge (rig-A; rig-D port: Bf 109 F-4 node table + law)
#include "render/river_surfaces.h"  // rivers as mirror water (same planet program)
#include "render/slag.h"            // CC3 slag pour (Living Copper Cliff)
#include "render/flak_gunner.h"  // F-POSE: the Sudburian on the manned gun
#include "render/rider_pose.h"  // SUDBURIAN R1a: the pure rider pose math
#include "render/sled_model.h"      // GLTF wiring rung: the Blender hero sled
#include "render/sled_plumes_draw.h"  // R5 rows 4/5/6: roost/exhaust/breath
#include "render/sting_model.h"  // ST-5: the hero Sting GLB, primitive fallback below
#include "render/launcher_model.h"  // ST-5 phase D: the gunstock launcher + its stations
#include "render/sting_deploy.h"    // ST-5 phase D: the deploy blend's pure key math
#include "sim/walker.h"             // ST-5 phase D: the afoot shoulder anchor
#include "render/smoke.h"  // CC1 Superstack smoke plume (Living Copper Cliff)
#include "render/snow_patch.h"  // SF3-A: the rider snow patch geometry
#include "render/team_color.h"  // S-mapteam: the ONE faction palette (map + world)
#include "render/team_kit.h"    // player_kit(): the side's four colours
#include "render/tourist_map.h"  // M-KEY map: greyscale basemap layer under the tactical overlay
#include "render/train.h"          // CC2 slag-pot trains (Living Copper Cliff)
#include "render/tunnel.h"         // T2 Errington tunnel greybox interior
#include "render/tunnel_mesh.h"    // collar_reach() (pure, no raylib)
#include "render/water_surface.h"  // dedicated flat lake mirror surfaces
#include "rlgl.h"
#include "sim/world.h"
#include "world/faction_bubbles.h"  // M-KEY map: baked ellipse centers/axes
#include "world/tunnel_geo.h"       // M-KEY map: the two tunnel mouth dirs
#include "world/tunnel_net.h"  // world:: cut-radius helpers (T4a per-mouth)

namespace render {

namespace {

// The seam: re-base to the camera eye in double, THEN cast (see draw.h).
Vector3 rel(const glm::dvec3& world, const glm::dvec3& eye) {
    const glm::dvec3 r = world - eye;
    return Vector3{static_cast<float>(r.x), static_cast<float>(r.y),
                   static_cast<float>(r.z)};
}

// ── Tracer streak mesh (perf) ──────────────────────────────────────────────
// ONE pre-allocated dynamic mesh + ONE DrawMesh renders the ENTIRE projectile
// pool, replacing the ~3 immediate-mode primitives (2 DrawCylinderEx + 1
// DrawSphere) the old draw_pool emitted PER round. At ~260 concurrent tracers
// that was ~780 CPU-built primitive submissions per frame while hosing the sky
// — the sky-fire stutter that grows as the trigger is held. Now: CPU
// billboarding — each comet layer is a camera-facing quad, expanded along
// cross(view_dir, streak_dir) at build time, per-vertex color (unlit default
// material) under the same additive blend. Buffers are sized once for the
// pool's high-water mark and only GROW (never per-frame heap churn); the draw
// uploads and draws just the used sub-range. render/ never writes sim state.
constexpr int kTracerQuadsPerRound = 3;  // soft glow + hot core + white head
// ushort index ceiling: cap_rounds * quads * 4 verts must stay < 65536.
constexpr int kTracerMaxRounds = 65535 / (kTracerQuadsPerRound * 4);  // 5461

struct TracerMesh {
    Mesh mesh{};
    Material mat{};
    bool mat_ready = false;
    std::vector<float> verts;         // 3 floats * vertexCount (per frame)
    std::vector<unsigned char> cols;  // 4 bytes  * vertexCount (per frame)
    int cap_rounds = 0;               // capacity in rounds (across both pools)
    int used_quads = 0;               // quads filled this frame
    bool ok = false;
};
TracerMesh g_tracers;

// Grow-only: (re)allocate the mesh + CPU buffers when the pool's live count
// first exceeds the current capacity. Never shrinks; a no-op once warmed up
// (so zero per-frame heap allocation on the steady-state firing loop).
void ensure_tracer_mesh(int need_rounds) {
    if (g_tracers.ok && g_tracers.cap_rounds >= need_rounds) return;
    // First allocation covers a whole sustained battery burst in ONE shot: the
    // config high-water is ~74 rounds/s * 10 s tracer lifetime = ~740 live
    // rounds, so 1024 clears it with no mid-burst mesh grow (the doubling below
    // stays as the safety net for future config changes).
    int cap = g_tracers.cap_rounds > 0 ? g_tracers.cap_rounds : 1024;
    while (cap < need_rounds) cap *= 2;
    if (cap > kTracerMaxRounds)
        cap = kTracerMaxRounds;                    // excess clamped in push
    if (g_tracers.ok) UnloadMesh(g_tracers.mesh);  // material is reused
    Mesh m{};
    m.vertexCount = cap * kTracerQuadsPerRound * 4;
    m.triangleCount = cap * kTracerQuadsPerRound * 2;
    m.vertices =
        static_cast<float*>(MemAlloc(sizeof(float) * 3 * m.vertexCount));
    m.texcoords =
        static_cast<float*>(MemAlloc(sizeof(float) * 2 * m.vertexCount));
    m.colors = static_cast<unsigned char*>(
        MemAlloc(sizeof(unsigned char) * 4 * m.vertexCount));
    m.indices = static_cast<unsigned short*>(
        MemAlloc(sizeof(unsigned short) * 3 * m.triangleCount));
    // Static index buffer (two tris per quad); texcoords fixed at 0 (default
    // material samples the 1x1 white map -> final color == vertexColor).
    const int quads = cap * kTracerQuadsPerRound;
    for (int q = 0; q < quads; ++q) {
        const unsigned short b = static_cast<unsigned short>(q * 4);
        const int ti = q * 6;
        m.indices[ti + 0] = b;
        m.indices[ti + 1] = b + 1;
        m.indices[ti + 2] = b + 2;
        m.indices[ti + 3] = b;
        m.indices[ti + 4] = b + 2;
        m.indices[ti + 5] = b + 3;
    }
    for (int i = 0; i < m.vertexCount; ++i) {
        m.vertices[i * 3 + 0] = m.vertices[i * 3 + 1] = m.vertices[i * 3 + 2] =
            0.0f;
        m.texcoords[i * 2 + 0] = m.texcoords[i * 2 + 1] = 0.0f;
        m.colors[i * 4 + 0] = m.colors[i * 4 + 1] = m.colors[i * 4 + 2] =
            m.colors[i * 4 + 3] = 0;
    }
    UploadMesh(&m, /*dynamic=*/true);
    g_tracers.mesh = m;
    if (!g_tracers.mat_ready) {
        g_tracers.mat = LoadMaterialDefault();  // unlit vertex-color path
        g_tracers.mat_ready = true;
    }
    g_tracers.verts.assign(static_cast<std::size_t>(3) * m.vertexCount, 0.0f);
    g_tracers.cols.assign(static_cast<std::size_t>(4) * m.vertexCount, 0);
    g_tracers.cap_rounds = cap;
    g_tracers.ok = true;
}

// Append one quad (winding p0-p1-p2, p0-p2-p3) into this frame's buffer.
inline void push_tracer_quad(const Vector3& p0, const Vector3& p1,
                             const Vector3& p2, const Vector3& p3,
                             const Color& c) {
    TracerMesh& t = g_tracers;
    if (t.used_quads >= t.cap_rounds * kTracerQuadsPerRound) return;  // clamp
    const int base = t.used_quads * 4;
    const Vector3 P[4] = {p0, p1, p2, p3};
    for (int v = 0; v < 4; ++v) {
        const int vi = base + v;
        t.verts[vi * 3 + 0] = P[v].x;
        t.verts[vi * 3 + 1] = P[v].y;
        t.verts[vi * 3 + 2] = P[v].z;
        t.cols[vi * 4 + 0] = c.r;
        t.cols[vi * 4 + 1] = c.g;
        t.cols[vi * 4 + 2] = c.b;
        t.cols[vi * 4 + 3] = c.a;
    }
    ++t.used_quads;
}

// ── Generic camera-facing quad batch (perf) ────────────────────────────────
// One pre-allocated dynamic mesh + ONE DrawMesh for an arbitrary list of thin
// billboard quads, same discipline as TracerMesh but NOT tied to the tracer's
// rounds/quads-per-round layout and kept as a SEPARATE instance so it can run
// under its OWN blend/depth state. Used by the wingtip-vortex overlay, which
// draws alpha-blended with depth-WRITE ON (the tracer path is additive,
// write-off) — so the two must not share a buffer. Grow-only; render/ never
// writes sim state.
struct QuadBatch {
    Mesh mesh{};
    Material mat{};
    bool mat_ready = false;
    bool ok = false;
    int cap_quads = 0;   // capacity in quads
    int used_quads = 0;  // quads filled this frame
    std::vector<float> verts;
    std::vector<unsigned char> cols;
};
// ushort index ceiling: cap_quads * 4 verts must stay < 65536.
constexpr int kQuadBatchMax = 65535 / 4;  // 16383

void ensure_quad_batch(QuadBatch& q, int need_quads) {
    if (q.ok && q.cap_quads >= need_quads) return;
    int cap = q.cap_quads > 0 ? q.cap_quads : 512;
    while (cap < need_quads) cap *= 2;
    if (cap > kQuadBatchMax) cap = kQuadBatchMax;  // excess clamped in push
    if (q.ok) UnloadMesh(q.mesh);                  // material is reused
    Mesh m{};
    m.vertexCount = cap * 4;
    m.triangleCount = cap * 2;
    m.vertices =
        static_cast<float*>(MemAlloc(sizeof(float) * 3 * m.vertexCount));
    m.texcoords =
        static_cast<float*>(MemAlloc(sizeof(float) * 2 * m.vertexCount));
    m.colors = static_cast<unsigned char*>(
        MemAlloc(sizeof(unsigned char) * 4 * m.vertexCount));
    m.indices = static_cast<unsigned short*>(
        MemAlloc(sizeof(unsigned short) * 3 * m.triangleCount));
    for (int qi = 0; qi < cap; ++qi) {
        const unsigned short b = static_cast<unsigned short>(qi * 4);
        const int ti = qi * 6;
        m.indices[ti + 0] = b;
        m.indices[ti + 1] = b + 1;
        m.indices[ti + 2] = b + 2;
        m.indices[ti + 3] = b;
        m.indices[ti + 4] = b + 2;
        m.indices[ti + 5] = b + 3;
    }
    for (int i = 0; i < m.vertexCount; ++i) {
        m.vertices[i * 3 + 0] = m.vertices[i * 3 + 1] = m.vertices[i * 3 + 2] =
            0.0f;
        m.texcoords[i * 2 + 0] = m.texcoords[i * 2 + 1] = 0.0f;
        m.colors[i * 4 + 0] = m.colors[i * 4 + 1] = m.colors[i * 4 + 2] =
            m.colors[i * 4 + 3] = 0;
    }
    UploadMesh(&m, /*dynamic=*/true);
    q.mesh = m;
    if (!q.mat_ready) {
        q.mat = LoadMaterialDefault();  // unlit vertex-color path
        q.mat_ready = true;
    }
    q.verts.assign(static_cast<std::size_t>(3) * m.vertexCount, 0.0f);
    q.cols.assign(static_cast<std::size_t>(4) * m.vertexCount, 0);
    q.cap_quads = cap;
    q.ok = true;
}

inline void push_quad(QuadBatch& q, const Vector3& p0, const Vector3& p1,
                      const Vector3& p2, const Vector3& p3, const Color& c) {
    if (q.used_quads >= q.cap_quads) return;  // clamp (excess dropped)
    const int base = q.used_quads * 4;
    const Vector3 P[4] = {p0, p1, p2, p3};
    for (int v = 0; v < 4; ++v) {
        const int vi = base + v;
        q.verts[vi * 3 + 0] = P[v].x;
        q.verts[vi * 3 + 1] = P[v].y;
        q.verts[vi * 3 + 2] = P[v].z;
        q.cols[vi * 4 + 0] = c.r;
        q.cols[vi * 4 + 1] = c.g;
        q.cols[vi * 4 + 2] = c.b;
        q.cols[vi * 4 + 3] = c.a;
    }
    ++q.used_quads;
}

// Upload the used sub-range and draw it with one DrawMesh (culling off so the
// billboards read from either winding). Caller owns the blend/depth state.
inline void flush_quad_batch(QuadBatch& q) {
    if (!q.ok || q.used_quads <= 0) return;
    const int used_verts = q.used_quads * 4;
    UpdateMeshBuffer(q.mesh, 0, q.verts.data(),
                     static_cast<int>(sizeof(float) * 3 * used_verts), 0);
    UpdateMeshBuffer(q.mesh, 3, q.cols.data(),
                     static_cast<int>(sizeof(unsigned char) * 4 * used_verts),
                     0);
    const int full_v = q.mesh.vertexCount;
    const int full_t = q.mesh.triangleCount;
    q.mesh.vertexCount = used_verts;
    q.mesh.triangleCount = q.used_quads * 2;
    rlDisableBackfaceCulling();
    DrawMesh(q.mesh, q.mat, MatrixIdentity());
    rlEnableBackfaceCulling();
    q.mesh.vertexCount = full_v;
    q.mesh.triangleCount = full_t;
}

QuadBatch g_vortex_quads;

// S-cues palette (comfort program): a warm SLAG-ORANGE family so the
// orientation cues read as the smelter-lit HUD they are, single-sourced HERE so
// no color literal scatters through the glue. kCueSlag = the BRIGHT primary
// (main ghost-horizon line + bank pointer); kCueEmber = the DIMMER member (bank
// arc frame + ticks + the pitch-ladder rungs). Alpha is filled per-draw from
// the config dials (kept from the existing cue_* alphas). RGB only here.
constexpr unsigned char kCueSlagR = 255, kCueSlagG = 140, kCueSlagB = 50;
constexpr unsigned char kCueEmberR = 220, kCueEmberG = 110, kCueEmberB = 45;

// Cubesphere build params, sourced from config/world.toml via
// set_planet_build_params() (called once at startup from app/main.cpp) so no
// bare geometry numbers live here (docs/world_build_plan.md §1). Defaults (in
// draw.h) reproduce the pre-config planet.
PlanetBuildParams g_planet_cfg{};

// Textured, DEM-displaced cubesphere (render/planet.*). Built once on the
// first frame (needs a live GL context — draw_planet only runs inside
// BeginMode3D, well after InitWindow). Relief is render-only; the physics
// crash surface is still the perfect sphere at R (SPEC §6). HOISTED to file
// scope (was a static local in draw_planet) so the Fleet Rig mirror can sample
// its albedo cubemap READ-ONLY (rig-A.2; fleet_rig_plan.md). draw_planet runs
// before draw_aircraft each frame, so the cubemap is valid by the time the
// fleet samples it.
Planet g_planet;
bool g_planet_tried = false;

const Planet& ensure_planet(const sim::AircraftParams& params) {
    if (!g_planet_tried) {
        g_planet_tried = true;
        // Geometry knobs come from config/world.toml (g_planet_cfg, set once at
        // startup); u_offset still puts the steep equatorial relief under the
        // +X spawn sub-point for a legible opening view (until Sudbury replaces
        // it, P1).
        g_planet = load_planet(
            SEADS_ASSET_DIR, params.R, g_planet_cfg.relief_scale,
            g_planet_cfg.subdiv, g_planet_cfg.tiles, g_planet_cfg.u_offset,
            g_planet_cfg.dem_blur_radius, g_planet_cfg.cubemap_size,
            g_planet_cfg.procedural, g_planet_cfg.water_reflectivity,
            g_planet_cfg.water_sparkle, g_planet_cfg.cuts);
        // ★ R1 (BLOCK-VP1): bind the fold the planet was JUST built with, so
        // every drape -- roads, banks, rivers -- sits on the surface actually
        // on screen. Without this they keep draping on the bare DEM and end up
        // BURIED by exactly the fold amount (~0.77 m in bush) the moment the
        // mesh rises. Rebuilt here rather than carried out of load_planet,
        // because it must point at the STATIC planet's rasters: the local one
        // inside load_planet dies with the return.
        if (g_planet.ok && g_planet.fold_active) {
            static world::SnowpackField s_drawn_fold;
            s_drawn_fold = world::SnowpackField{};
            s_drawn_fold.p = g_planet.fold_params;
            // R2 SWEEP (SEADS_MASK_M): the corridor mask width is the one
            // dial trading a float halo around every road against the mesh
            // aliasing a 6 m corridor it cannot resolve. Tunable so the
            // trade can be MEASURED rather than argued.
            if (const char* mm = std::getenv("SEADS_MASK_M"))
                s_drawn_fold.p.draw_mask_m = std::atof(mm);
            if (const char* wd = std::getenv("SEADS_WATER_DILATE_M"))
                s_drawn_fold.p.draw_water_dilate_m = std::atof(wd);
            s_drawn_fold.hf = &g_planet.height;
            if (!g_planet.landmask.empty())
                s_drawn_fold.landmask = &g_planet.landmask;
            if (!g_planet.barren.empty())
                s_drawn_fold.barren = &g_planet.barren;
            s_drawn_fold.lines =
                &sudbury_linework(params.R, g_planet_cfg.u_offset);
            set_drawn_fold(&s_drawn_fold);
        }
        // Reflected-star dials (lakes + rivers, single-sourced in the water
        // branch).
        g_planet.water_star_reflect =
            static_cast<float>(g_planet_cfg.water_star_reflect);
        g_planet.water_star_density =
            static_cast<float>(g_planet_cfg.water_star_density);
    }
    return g_planet;
}

//   relief_scale = metres at full-white DEM (Everest-class peaks read on a

// ★ SF3-A THE RIDER SNOW PATCH. State lives here, beside the planet it shares
// a material with. The sources are bound from app/ (the set_bank_snow_sources
// precedent) because world/ is render-free by law and render/ has no world.
namespace {
const world::SnowpackField* g_patch_field = nullptr;
int g_patch_subdiv = 0;
int g_patch_tiles = 1;
SnowPatchParams g_patch_params;
SnowPatchBuild g_patch_build;
SnowPatchGL g_patch_gl;
std::vector<float> g_patch_pos, g_patch_nrm, g_patch_uv;
std::vector<unsigned short> g_patch_idx;

// One refresh cycle = n_side/rows_per_frame frames. The patch is at most that
// stale, which at 96/8 = 12 frames and 30 m/s is 6 m of travel on a 120 m
// patch -- invisible, and it costs a BOUNDED ~0.8 ms/frame instead of the
// measured 9.8 ms a full rebuild would cost.
void snow_patch_tick(const Planet& planet, const glm::dvec3& eye) {
    if (g_patch_field == nullptr || g_patch_field->hf == nullptr) return;
    // A/B kill switch, the SEADS_NO_M2 precedent: this is a visible-feel
    // change and Chad judges it against its own absence.
    static const bool off = std::getenv("SEADS_NO_SNOWPATCH") != nullptr;
    if (off) return;
    // DIAGNOSTIC ONLY: SEADS_PATCH_LIFT exaggerates the interior clearance so
    // the patch's screen footprint is unmistakable. Not a shipped dial.
    static const char* lift_s = std::getenv("SEADS_PATCH_LIFT");
    if (lift_s != nullptr) g_patch_params.lift_m = std::atof(lift_s);

    const glm::dvec3 anchor = glm::normalize(eye);
    if (g_patch_build.n_side == 0)
        snow_patch_begin(g_patch_build, g_patch_params, anchor,
                         g_patch_field->hf->R, g_patch_field);
    snow_patch_step(g_patch_build, g_patch_params, *g_patch_field->hf,
                    g_patch_subdiv, g_patch_tiles, *g_patch_field);
    if (g_patch_build.complete()) {
        snow_patch_emit(g_patch_build, g_patch_params, g_patch_pos, g_patch_nrm,
                        g_patch_uv);
        if (g_patch_idx.empty())
            g_patch_idx = snow_patch_indices(g_patch_params.n_side);
        snow_patch_gl_upload(g_patch_gl, g_patch_params.n_side, g_patch_pos,
                             g_patch_nrm, g_patch_uv, g_patch_idx);
        if (std::getenv("SEADS_PATCH_DEBUG") != nullptr) {
            const double drift = glm::length(g_patch_build.frame.c - anchor) *
                                 g_patch_field->hf->R;
            std::printf(
                "PATCH upload: anchor drift %.2f m, eye_alt %.2f m, span %.1f "
                "m\n",
                drift, glm::length(eye) - g_patch_field->hf->R,
                g_patch_params.cell_m * (g_patch_params.n_side - 1));
        }
        // Re-anchor at where the rider is NOW, not where this pass started.
        snow_patch_begin(g_patch_build, g_patch_params, anchor,
                         g_patch_field->hf->R, g_patch_field);
    }
    draw_snow_patch(planet, g_patch_gl, eye);
}
}  // namespace

//   15 km toy planet); subdiv = cubesphere verts/edge/face.
void draw_planet(const sim::AircraftParams& params, const glm::dvec3& eye,
                 const render::ShadowSet& shadows, const glm::vec3& sun_dir,
                 const AtmosphereParams& atm,
                 const glm::vec3& moon_dir, float moon_fill, float moon_sparkle,
                 const AuroraParams& aurora, const SnowParams& snow,
                 const AirField& air) {
    const Planet& planet = ensure_planet(params);

    if (planet.ok) {
        // sun_dir is the celestial sun (Stage 2, frozen); up/eye_alt drive the
        // shared kSkyGLSL aerial perspective so the limb matches the sky.
        const double r = glm::length(eye);
        const glm::vec3 up = glm::vec3(eye / r);
        draw_planet_mesh(planet, eye, sun_dir, up,
                         static_cast<float>(r - params.R), atm, moon_dir,
                         moon_fill, moon_sparkle, aurora, snow, air, shadows);
        snow_patch_tick(planet, eye);
        return;
    }
    // Fallback: assets or GPU features unavailable — keep the sim flyable with
    // the original untextured sphere (no floating wire shell).
    const Vector3 center = rel(glm::dvec3{0.0}, eye);
    DrawSphereEx(center, static_cast<float>(params.R), 64, 96,
                 Color{72, 108, 58, 255});
}

// Dedicated flat lake mirror surfaces (render/water_surface.*). Built once from
// the baked outlines (render/sudbury_gis.gen.h) after the planet exists; drawn
// with the planet's OWN program (uForceWater=1) so the mirror look is single-
// sourced with the planet limb. MUST be called right after draw_planet each
// frame (it reuses the sky/scatter/moon/aurora uniforms draw_planet just
// bound).
WaterSurfaces g_water;
bool g_water_tried = false;
RiverSurfaces g_rivers;  // rivers as mirror water (drawn right after the lakes)
bool g_rivers_tried = false;

// S2 boreal trees — config (POD from draw.h) + the lazily-built renderer. Built
// on the first frame that draws them (needs a live GL context + the planet's
// height field). No-op when disabled or the density raster is missing.
TreeBuildParams g_tree_cfg{};
PropRenderer g_props;
bool g_props_tried = false;

void draw_trees(const glm::dvec3& eye, const glm::vec3& sun_dir) {
    if (!g_tree_cfg.enabled || !g_planet.ok) return;
    static const bool no_trees = std::getenv("SEADS_NO_TREES") != nullptr;
    if (no_trees) return;  // A/B bypass (smoke + Chad's fly), read once
    if (!g_props_tried) {
        g_props_tried = true;
        world::TreeParams tp;
        tp.cells_per_face = g_tree_cfg.cells_per_face;
        tp.chunk_cells = g_tree_cfg.chunk_cells;
        tp.gain = g_tree_cfg.density_gain;
        tp.min_scale = g_tree_cfg.min_scale;
        tp.max_scale = g_tree_cfg.max_scale;
        tp.tree_height_m = g_tree_cfg.tree_height_m;
        tp.render_range_m = g_tree_cfg.render_range_m;
        tp.seed = g_tree_cfg.seed;
        PropLook look;
        look.base_width_m = static_cast<float>(g_tree_cfg.base_width_m);
        look.ambient = static_cast<float>(g_tree_cfg.ambient);
        look.diffuse = static_cast<float>(g_tree_cfg.diffuse);
        look.fade_frac = static_cast<float>(g_tree_cfg.fade_frac);
        // ★ S2b (WINTER_LAW §2.4b, Chad's S2 fly: "snow machine trails still
        // have trees on it"): hand the scatter the promoted linework so the
        // trail and road corridors clear the canopy. The same network the
        // snowpack corridor query reads, so the cleared gap is exactly the
        // drawn ribbon width (INV-6) and the two can never disagree.
        world::CorridorMask corridor;
        corridor.lines = &sudbury_linework(g_planet.R, g_planet_cfg.u_offset);
        corridor.margin_m = g_tree_cfg.corridor_margin_m;
        if (corridor.lines->empty() || corridor.margin_m < 0.0)
            corridor.lines =
                nullptr;  // bit-identical to the pre-corridor scatter
        // ★ SF1 (§3.6b): the snow-mountain footprint clears the canopy like
        // the corridors do. Same params + heightfield the snowpack drives on
        // (one function, two consumers, zero fork); disabled hill => null =>
        // bit-identical scatter.
        corridor.hill = &g_tree_cfg.snowhill;
        corridor.hill_hf = g_tree_cfg.snowhill_hf;
        if (!g_tree_cfg.snowhill.enabled || corridor.hill_hf == nullptr)
            corridor.hill = nullptr;
        TraceLog(LOG_INFO, "SNOWHILL MASK: %s (enabled %d, hf %s)",
                 corridor.hill != nullptr ? "ON" : "OFF",
                 g_tree_cfg.snowhill.enabled ? 1 : 0,
                 corridor.hill_hf != nullptr ? "set" : "null");
        g_props =
            build_props(SEADS_ASSET_DIR, tp, look, g_tree_cfg.cuts, corridor);
    }
    draw_props(g_props, g_planet.height, sun_dir, eye);
}

void draw_water(const glm::dvec3& eye) {
    if (!g_planet.ok)
        return;  // no planet (Earth stand-in / GPU fail) -> no lakes
    if (!g_water_tried) {
        g_water_tried = true;
        g_water = build_water_surfaces(g_planet_cfg.water_surface_lift_m);
    }
    draw_water_surfaces(g_planet, g_water, eye);
    // Rivers as mirror water — same planet program, drawn right after the lakes
    // so they read identically (run INTO the lakes, overlap is invisible).
    // Draped on the shared height field (lifted like the lakes to beat terrain
    // z-fight).
    if (!g_rivers_tried) {
        g_rivers_tried = true;
        // T24-river: same single-source excavation cut list the terrain/roads/
        // trees already drop against (g_planet_cfg.cuts == app/main.cpp's
        // planet_cuts) — a river path over the Errington/Murray pit or the
        // trench steps now clips exactly like a road.
        g_rivers = build_river_surfaces(
            g_planet.height, g_planet_cfg.water_surface_lift_m,
            g_planet_cfg.subdiv, g_planet_cfg.tiles, g_planet_cfg.cuts);
    }
    draw_river_surfaces(g_planet, g_rivers, eye);
}

// S3 draped linework ribbons — config POD + lazily-built renderer, built on the
// first frame that draws them (needs a live GL context + the planet's height
// field for the drape). Drawn after the opaque planet+water and before the
// translucent props. No-op when disabled or the bake emitted no paths.
RibbonBuildParams g_ribbon_cfg{};
RibbonSurfaces g_ribbons;
bool g_ribbons_tried = false;

// SF2-BANKS: the oreo snowbank strips -- built lazily like the ribbons, AFTER
// the snow field binds (g_bank_snow, late via set_bank_snow_sources).
BankBuildCfg g_bank_cfg{};
const world::SnowpackField* g_bank_snow = nullptr;
BankSurfaces g_banks;
bool g_banks_tried = false;

void draw_ribbons(const glm::dvec3& eye, float winter,
                  const BankLight& bank_light) {
    if (!g_ribbon_cfg.enabled || !g_planet.ok) return;
    if (!g_ribbons_tried) {
        g_ribbons_tried = true;
        RibbonLook look;
        look.lift_m = static_cast<float>(g_ribbon_cfg.lift_m);
        look.max_seg_m = g_ribbon_cfg.max_seg_m;
        look.max_tr_m = g_ribbon_cfg.max_tr_m;
        // ★ ROAD-REPAIR F2: the junction cut. SEADS_NO_JUNCTION_CUT is honoured
        // once in app/main.cpp (the SEADS_NO_APRON / SEADS_NO_DECK_YIELD
        // pattern), so the value that arrives here is already the live one.
        look.junction_cut_m = g_ribbon_cfg.junction_cut_m;
        // ★ ROAD-REPAIR F3: the over-bank bias. SEADS_OVER_BANK_BIAS is
        // honoured once in app/main.cpp (the SEADS_RIBBON_MAXSEG pattern),
        // so the value that arrives here is already the live one.
        look.over_bank_bias = g_ribbon_cfg.over_bank_bias;
        // ★ ROAD-REPAIR rung AA: the centreline anti-alias. The
        // SEADS_LINE_AA kill is honoured in render/ribbons.cpp beside
        // SEADS_NO_LINE, the arm it replaces.
        look.line_aa = static_cast<float>(g_ribbon_cfg.line_aa);
        look.road_bed = glm::vec3(g_ribbon_cfg.road_bed);
        look.road_line = glm::vec3(g_ribbon_cfg.road_line);
        look.road_center_frac =
            static_cast<float>(g_ribbon_cfg.road_center_frac);
        look.road_dash_m = static_cast<float>(g_ribbon_cfg.road_dash_m);
        look.road_gap_m = static_cast<float>(g_ribbon_cfg.road_gap_m);
        look.road_mottle = static_cast<float>(g_ribbon_cfg.road_mottle);
        look.road_mottle_frac =
            static_cast<float>(g_ribbon_cfg.road_mottle_frac);
        look.road_fade = static_cast<float>(g_ribbon_cfg.road_fade);
        look.trail_color = glm::vec3(g_ribbon_cfg.trail_color);
        look.trail_mottle = static_cast<float>(g_ribbon_cfg.trail_mottle);
        look.trail_winter_color = glm::vec3(g_ribbon_cfg.trail_winter_color);
        look.trail_corduroy = static_cast<float>(g_ribbon_cfg.trail_corduroy);
        look.trail_corduroy_m =
            static_cast<float>(g_ribbon_cfg.trail_corduroy_m);
        // R4d: the drape conforms to the LIVE mesh build (subdiv/tiles) — the
        // same values ensure_planet fed fill_face. T24: and yields to the
        // SAME excavation cut disks (the ribbon clip).
        g_ribbons =
            build_ribbon_surfaces(g_planet.height, look, g_planet_cfg.subdiv,
                                  g_planet_cfg.tiles, g_ribbon_cfg.cuts);
    }
    draw_ribbon_surfaces(g_ribbons, eye, winter);
    // ★ SF2-BANKS (§3.6c): the banks the sled already drives, made visible.
    // Built once the snow field is bound; lift single-sourced from the
    // ribbons' (F8), cuts shared (F4), drawn with the ribbons' xf block (F7).
    if (g_bank_cfg.enabled && g_bank_snow != nullptr && !g_banks_tried) {
        g_banks_tried = true;
        BankBuildParams bp;
        bp.station_m = g_bank_cfg.station_m;
        bp.skirt_m = g_bank_cfg.skirt_m;
        bp.skirt_bury_m = g_bank_cfg.skirt_bury_m;
        bp.lift_m = static_cast<float>(g_ribbon_cfg.lift_m);
        bp.min_amp_m = g_bank_cfg.min_amp_m;
        // ★ ROAD-REPAIR: the two continuity dials, from [bank_mesh].
        bp.skirt_rings = g_bank_cfg.skirt_rings;
        bp.chord_tol_m = g_bank_cfg.chord_tol_m;
        bp.junction_station_m = g_bank_cfg.junction_station_m;
        // ★ ROAD-REPAIR ONAPING RUNG 2: the drawn apron. SEADS_NO_APRON is
        // honoured where the POD is FILLED (app/main.cpp), not here, so the
        // seat A/B and the census A/B are the same switch read once.
        bp.apron_m = g_bank_cfg.apron_m;
        bp.apron_tol_m = g_bank_cfg.apron_tol_m;
        bp.apron_min_drop_m = g_bank_cfg.apron_min_drop_m;
        // ★ ROAD-REPAIR F1: the deck yield. SEADS_NO_DECK_YIELD is honoured
        // where the POD is FILLED (app/main.cpp), the apron kill's exact
        // shape, so the seat A/B is ONE switch read ONCE.
        bp.deck_yield_m = g_bank_cfg.deck_yield_m;
        BankLook bl;
        bl.speckle_density = static_cast<float>(g_bank_cfg.speckle_density);
        bl.speckle_dark = static_cast<float>(g_bank_cfg.speckle_dark);
        // ★ ROAD-REPAIR rung AA: the speckle Nyquist gate.
        bl.speckle_aa = static_cast<float>(g_bank_cfg.speckle_aa);
        bl.crest_smudge = static_cast<float>(g_bank_cfg.crest_smudge);
        g_banks = build_bank_surfaces(g_planet.height, g_planet_cfg.subdiv,
                                      g_planet_cfg.tiles, *g_bank_snow, bp, bl,
                                      g_ribbon_cfg.cuts);
    }
    draw_bank_surfaces(g_banks, eye, bank_light);
}

// S4 building massing — config POD + lazily-built renderer, built on the first
// frame that draws it (needs a live GL context + the planet's height field for
// the drape). Drawn after the opaque planet+water+ribbons and before the
// translucent props. No-op when disabled or the bake emitted no batches.
BuildingBuildParams g_building_cfg{};
BuildingSurfaces g_buildings;
bool g_buildings_tried = false;

void draw_buildings(const glm::dvec3& eye, const glm::vec3& sun_dir) {
    if (!g_building_cfg.enabled || !g_planet.ok) return;
    if (!g_buildings_tried) {
        g_buildings_tried = true;
        BuildingLook look;
        look.wall_val = static_cast<float>(g_building_cfg.wall_val);
        look.roof_val = static_cast<float>(g_building_cfg.roof_val);
        look.ambient = static_cast<float>(g_building_cfg.ambient);
        look.diffuse = static_cast<float>(g_building_cfg.diffuse);
        look.height_scale = static_cast<float>(g_building_cfg.height_scale);
        look.window_color = glm::vec3(g_building_cfg.window_color);
        look.window_bright = static_cast<float>(g_building_cfg.window_bright);
        look.window_lit_frac =
            static_cast<float>(g_building_cfg.window_lit_frac);
        g_buildings = build_building_surfaces(g_planet.height, look);
    }
    draw_building_surfaces(g_buildings, sun_dir, eye);
}

// Street-lamp night point lights — config POD + lazily-built renderer, built on
// the first frame that draws them (needs a live GL context + the planet's
// height field). Additive glows over the town at night; no-op when disabled or
// none were baked.
LampBuildParams g_lamp_cfg{};
LampRenderer g_lamps;
bool g_lamps_tried = false;

void draw_lamps(const glm::dvec3& eye, const glm::vec3& sun_dir,
                float fovy_deg) {
    if (!g_lamp_cfg.enabled || !g_planet.ok) return;
    static const bool no_lamps = std::getenv("SEADS_NO_LAMPS") != nullptr;
    if (no_lamps) return;  // A/B bypass (smoke + Chad's fly), read once
    if (!g_lamps_tried) {
        g_lamps_tried = true;
        LampLook look;
        look.color = glm::vec3(g_lamp_cfg.color);
        look.brightness = static_cast<float>(g_lamp_cfg.brightness);
        look.core_bright = static_cast<float>(g_lamp_cfg.core_bright);
        look.core_sharp = static_cast<float>(g_lamp_cfg.core_sharp);
        look.size_m = static_cast<float>(g_lamp_cfg.size_m);
        look.min_px = static_cast<float>(g_lamp_cfg.min_px);
        g_lamps = build_lamp_renderer(g_planet.height, look);
    }
    draw_lamp_renderer(g_lamps, sun_dir, eye, fovy_deg, GetScreenHeight());
}

// Precipitation (W3) — config POD + lazily-built billboard particle system,
// built on the first frame that draws it (needs a live GL context).
// Season-gated (Winter=snow / Spring=rain / else dry) and weather-gated (only
// under an active W2 cell). Alpha, depth-test on / write off; runs in the
// translucent tier.
PrecipBuildParams g_precip_cfg{};
PrecipRenderer g_precip;       // NEAR: small flakes, tight box
PrecipRenderer g_precip_veil;  // AS-3 FAR "veil": big soft flakes, deep box
bool g_precip_tried = false;

// AS-1 — the injected tunnel-net SDF. render/precip.h takes a plain callable so
// seads_render_core never learns about world/; this thunk is the ONLY place the
// two meet.
//
// It forwards to ROOFED_signed_distance, not signed_distance. The question the
// snow asks is "is there rock ABOVE this flake", and the full SDF cannot answer
// it: it mins in the Murray bowl, the Errington entry pit and the approach
// trench, which are OPEN CUTS — sky above, by construction. Gating on the full
// SDF deleted the snowfall inside the open pits, which main draws. The roofed
// query is additive (world/tunnel_net.cpp); signed_distance, contains and
// app::inside_tunnel are untouched, so the camera and the crash yield still see
// exactly what they saw.
double tunnel_sdf_thunk(const void* ctx, const glm::dvec3& p) {
    return static_cast<const world::TunnelNet*>(ctx)->roofed_signed_distance(p);
}

void draw_precip(const CameraPose& pose, Season season, double intensity,
                 double phase, double phase_far, const world::TunnelNet* net,
                 double ground_r) {
    if (!g_precip_cfg.enabled) return;
    static const bool no_precip = std::getenv("SEADS_NO_PRECIP") != nullptr;
    if (no_precip) return;  // A/B bypass (smoke + Chad's fly), read once
    if (!g_precip_tried) {
        g_precip_tried = true;
        PrecipLook look;
        look.cell_size_m = static_cast<float>(g_precip_cfg.cell_size_m);
        look.box_half_m = static_cast<float>(g_precip_cfg.box_half_m);
        look.wrap_fade = static_cast<float>(g_precip_cfg.wrap_fade);
        look.color = glm::vec3(g_precip_cfg.color);
        look.snow_size_m = static_cast<float>(g_precip_cfg.snow_size_m);
        look.snow_opacity = static_cast<float>(g_precip_cfg.snow_opacity);
        look.rain_size_m = static_cast<float>(g_precip_cfg.rain_size_m);
        look.rain_streak_m = static_cast<float>(g_precip_cfg.rain_streak_m);
        look.rain_opacity = static_cast<float>(g_precip_cfg.rain_opacity);
        look.size_var = static_cast<float>(g_precip_cfg.size_var);
        look.alpha_var = static_cast<float>(g_precip_cfg.alpha_var);
        look.density_exp = static_cast<float>(g_precip_cfg.density_exp);
        look.density_soft = static_cast<float>(g_precip_cfg.density_soft);
        look.sway_m = static_cast<float>(g_precip_cfg.sway_m);
        look.rim_dark = static_cast<float>(g_precip_cfg.rim_dark);
        look.rock_band_m = static_cast<float>(g_precip_cfg.rock_band_m);
        g_precip = build_precip_renderer(look);
        if (g_precip_cfg.veil_enabled) {
            // ONE renderer TYPE, a second configuration — never a second code
            // path. The veil is the same lattice at a wider cell, a deeper box,
            // bigger softer flakes and a lower opacity, so distance reads as
            // distance instead of as "the same dots, dimmer".
            PrecipLook far_look = look;
            far_look.cell_size_m =
                static_cast<float>(g_precip_cfg.veil_cell_size_m);
            far_look.box_half_m =
                static_cast<float>(g_precip_cfg.veil_box_half_m);
            far_look.snow_size_m = static_cast<float>(g_precip_cfg.veil_size_m);
            far_look.snow_opacity =
                static_cast<float>(g_precip_cfg.veil_opacity);
            far_look.sway_m = static_cast<float>(g_precip_cfg.veil_sway_m);
            far_look.inner_fade_m =
                static_cast<float>(g_precip_cfg.veil_inner_fade_m);
            g_precip_veil = build_precip_renderer(far_look);
        }
    }
    const PrecipSdf sdf = net != nullptr ? &tunnel_sdf_thunk : nullptr;
    const void* ctx = net;
    // FAR first, then NEAR: depth-write is off in this pass, so the draw order
    // IS the composite order and the near flakes must land on top of the veil.
    // The veil is snow only — a rain "veil" would be grey streaks at 70 m, which
    // reads as fog, and W3's rain look was signed without one.
    if (g_precip_veil.ok && season == Season::Winter)
        draw_precip_renderer(g_precip_veil, pose, season, intensity, phase_far,
                             sdf, ctx, ground_r);
    draw_precip_renderer(g_precip, pose, season, intensity, phase, sdf, ctx,
                         ground_r);
}

// CC1 Superstack smoke (Living Copper Cliff) — config POD + lazily-built plume,
// built on the first frame that draws it (needs a live GL context + the height
// field). The Superstack axis unit dir is computed OFFLINE through the LOCKED
// aeqd projection (offline_tool: geo_to_dir(HERO_SUPERSTACK), which is where
// the baked hero mesh's cap vertex sits) so the plume can never fork from the
// hero mesh; H is the same 381 m the hero uses. Re-derive if the projection
// lock ever changes.
SmokeBuildParams g_smoke_cfg{};
SmokeRenderer g_smoke;
bool g_smoke_tried = false;
constexpr glm::dvec3 kSuperstackDir{0.26561950711033627, -0.55885612114843386,
                                    0.7855737478412762};
constexpr double kSuperstackH = 381.0;

void draw_smoke(const glm::dvec3& eye, double phase) {
    if (!g_smoke_cfg.enabled || !g_planet.ok) return;
    static const bool no_smoke = std::getenv("SEADS_NO_SMOKE") != nullptr;
    if (no_smoke) return;  // A/B bypass (smoke shot + Chad's fly), read once
    if (!g_smoke_tried) {
        g_smoke_tried = true;
        SmokeLook look;
        look.puffs = g_smoke_cfg.puffs;
        look.color = glm::vec3(g_smoke_cfg.color);
        look.rise_m = static_cast<float>(g_smoke_cfg.rise_m);
        look.drift_m = static_cast<float>(g_smoke_cfg.drift_m);
        look.r0_m = static_cast<float>(g_smoke_cfg.r0_m);
        look.r1_m = static_cast<float>(g_smoke_cfg.r1_m);
        look.opacity = static_cast<float>(g_smoke_cfg.opacity);
        look.jitter_m = static_cast<float>(g_smoke_cfg.jitter_m);
        look.wind = glm::vec3(g_smoke_cfg.wind);
        g_smoke = build_smoke_renderer(g_planet.height, look, kSuperstackDir,
                                       kSuperstackH);
    }
    draw_smoke_renderer(g_smoke, phase, eye);
}

// CC3/CC4 slag config POD (declared here; the shared ridge frame ensure_ridge()
// below reads its ridge dials, and the CC2 train reads the ridge to drape its
// rails on the new crest — Fable F3 single-source: the ridge mesh AND the track
// share one h(z)).
SlagBuildParams g_slag_cfg{};
SlagRenderer g_slag;
bool g_slag_tried = false;

// The shared CC4 slag ridge frame (built once from the [slag] ridge dials + the
// height field). Both draw_slag (the ridge mesh/lava) and draw_train (the crest
// rail-drape) use it. Returns nullptr when slag is off (config or the
// SEADS_NO_SLAG A/B) so the train doesn't ride an invisible crest.
SlagRidge g_ridge{};
bool g_ridge_ready = false;
const SlagRidge* ensure_ridge() {
    static const bool no_slag = std::getenv("SEADS_NO_SLAG") != nullptr;
    if (no_slag || !g_slag_cfg.enabled || !g_planet.ok) return nullptr;
    if (!g_ridge_ready) {
        g_ridge_ready = true;
        SlagLook look;
        look.mound_height_m = static_cast<float>(g_slag_cfg.mound_height_m);
        look.ridge_length_m = static_cast<float>(g_slag_cfg.ridge_length_m);
        look.crest_width_m = static_cast<float>(g_slag_cfg.crest_width_m);
        look.face_angle_deg = static_cast<float>(g_slag_cfg.face_angle_deg);
        look.benches =
            g_slag_cfg
                .benches;  // MUST match the drawn mesh (rails drape on g_ridge)
        g_ridge = make_slag_ridge(g_planet.height, look);
    }
    return g_ridge.ok ? &g_ridge : nullptr;
}

// CC2 slag-pot trains (Living Copper Cliff) — config POD + lazily-built
// renderer (needs a live GL context + the height field for the draped spur).
// Opaque, drawn after the buildings; no-op when disabled.
TrainBuildParams g_train_cfg{};
TrainRenderer g_train;
bool g_train_tried = false;

void draw_train(const glm::dvec3& eye, const glm::vec3& sun_dir, double phase) {
    if (!g_train_cfg.enabled || !g_planet.ok) return;
    static const bool no_train = std::getenv("SEADS_NO_TRAIN") != nullptr;
    if (no_train) return;  // A/B bypass (smoke shot + Chad's fly), read once
    // The shared ridge routes the dump leg along the crest AND arms the CC5
    // pot-tip + lava coupling (nullptr when slag is off -> the plain CC2 closed
    // spur).
    const SlagRidge* ridge = ensure_ridge();
    if (!g_train_tried) {
        g_train_tried = true;
        TrainLook look;
        look.pots = g_train_cfg.pots;
        look.car_gap_m = static_cast<float>(g_train_cfg.car_gap_m);
        look.tip_span_m = static_cast<float>(g_train_cfg.tip_span_m);
        look.lift_m = static_cast<float>(g_train_cfg.lift_m);
        look.color = glm::vec3(g_train_cfg.color);
        look.ambient = static_cast<float>(g_train_cfg.ambient);
        look.diffuse = static_cast<float>(g_train_cfg.diffuse);
        g_train = build_train_renderer(g_planet.height, look, ridge);
    }
    // CC4/CC5: the rails ride the ridge crest + the pots tip over the dump
    // edge.
    draw_train_renderer(g_train, phase, sun_dir, eye, ridge);
}

// CC3 slag pour (Living Copper Cliff) — the marquee: dark ridge + tipping pot +
// the warm-chroma lava cascade. Config POD + lazily-built renderer (needs a
// live GL context + the height field). Opaque, after the buildings/train; no-op
// when disabled.
void draw_slag(const glm::dvec3& eye, const glm::vec3& sun_dir, double phase,
               bool train_feeds_pour) {
    if (!g_slag_cfg.enabled || !g_planet.ok) return;
    static const bool no_slag = std::getenv("SEADS_NO_SLAG") != nullptr;
    if (no_slag) return;  // A/B bypass (smoke shot + Chad's fly), read once
    if (!g_slag_tried) {
        g_slag_tried = true;
        SlagLook look;
        look.mound_radius_m = static_cast<float>(g_slag_cfg.mound_radius_m);
        look.mound_top_r_m = static_cast<float>(g_slag_cfg.mound_top_r_m);
        look.mound_height_m = static_cast<float>(g_slag_cfg.mound_height_m);
        look.ridge_length_m = static_cast<float>(g_slag_cfg.ridge_length_m);
        look.crest_width_m = static_cast<float>(g_slag_cfg.crest_width_m);
        look.face_angle_deg = static_cast<float>(g_slag_cfg.face_angle_deg);
        look.benches =
            g_slag_cfg
                .benches;  // same profile as g_ridge (rails/lava single-source)
        look.mound_color = glm::vec3(g_slag_cfg.mound_color);
        look.ambient = static_cast<float>(g_slag_cfg.ambient);
        look.diffuse = static_cast<float>(g_slag_cfg.diffuse);
        look.rivers = g_slag_cfg.rivers;
        look.river_halfwidth_m =
            static_cast<float>(g_slag_cfg.river_halfwidth_m);
        look.glow = static_cast<float>(g_slag_cfg.glow);
        look.night_boost = static_cast<float>(g_slag_cfg.night_boost);
        look.lift_m = static_cast<float>(g_slag_cfg.lift_m);
        g_slag = build_slag_renderer(g_planet.height, look);
    }
    // CC5: when the TRAIN feeds the pour, its pots supply the tipping — drop
    // CC3's own static rim pot so there aren't two (the plan's CC5 removal).
    draw_slag_renderer(g_slag, phase, sun_dir, eye, !train_feeds_pour);
}

// --- Fleet Rig (rig-A.2; fleet_rig_plan.md) --------------------------------
// The mirror-finish fragment shader source lives in the PURE core
// (render::mirror_fs_source, rig.cpp) so the rig-A.3 asset validator can pin
// its declared uniforms against an allowlist headlessly.
//
// The FELT mirror knobs + sun, resolved per frame from FrameInfo (config).
struct FleetDrawParams {
    glm::vec3 sun_dir{0.0f};       // world light-travel dir (sun -> scene)
    float reflectivity = 0.45f;    // env-reflection strength [0,1]
    float fresnel_power = 3.0f;    // Fresnel rim exponent (>0)
    glm::vec3 player_color{1.0f};  // hero plane chroma
    glm::vec3 bandit_color{1.0f};  // bandit plane chroma
    // rig-B: surface-deflection magnitudes (radians) + the prop blur-disc
    // opacity range (idle..full-throttle). The gear extension + prop throttle
    // are read per plane from its SimState, not here.
    render::DeflectGains gains{};
    float prop_disc_alpha = 0.30f;
    float prop_idle_alpha = 0.06f;
    // Feature A: the active visibility params (Mirror = original look by
    // default).
    render::VizParams viz{};
};

// The rig meshes + mirror shader/material, built once on a live GL context
// (lazy, like the planet). rig holds the rest-pose model-space node world
// matrices — STATIC in rig-A (surface deflection + prop are rig-B). Falls back
// (ok=false) to the flat DrawCube aircraft on any GL/shader failure so the sim
// stays flyable.
struct FleetRig {
    bool ok = false;
    bool tried = false;
    Mesh meshes[render::kNodeCount] = {};
    // rig-D D.3: the loaded GLB Models are kept alive for the program lifetime
    // — meshes[i] aliases models[i].meshes[0], so UnloadModel would free the
    // GPU buffers we draw from (Fable D.3 lifetime rule). model_loaded[i] gates
    // the one UnloadModel we DO own vs the GenMeshCube fallback (which is not a
    // Model).
    Model models[render::kNodeCount] = {};
    bool model_loaded[render::kNodeCount] = {};
    Shader shader = {};
    Material mat = {};  // Mirror: pop-art monochrome-saturation mirror finish
    Material glass_mat = {};  // Glass: translucent canopy (back-to-front pass)
    Material matte_mat = {};  // Matte: non-mirror diffuse (pilot, tyres)
    // rig-B: a plain translucent material (raylib default shader) for the prop
    // blur disc — drawn AFTER the opaque rig with depth-write off (Fable C6).
    Material prop_mat = {};
    int loc_color = -1, loc_sun = -1, loc_fresnel = -1, loc_refl = -1;
    // Feature A viz uniforms.
    int loc_body_floor = -1, loc_emissive = -1, loc_white_mix = -1,
        loc_rim = -1, loc_rim_white = -1, loc_desat = -1;
    render::Rig rig;
};
FleetRig g_fleet;

// The material a node draws with, routed by its authored MaterialClass (rig-D
// D.3). ONE switch — the single source of the mirror/glass/matte routing.
Material& fleet_material(render::MaterialClass cls) {
    switch (cls) {
        case render::MaterialClass::Glass:
            return g_fleet.glass_mat;
        case render::MaterialClass::Matte:
            return g_fleet.matte_mat;
        case render::MaterialClass::Mirror:
        default:
            return g_fleet.mat;
    }
}

void ensure_fleet() {
    if (g_fleet.tried) return;
    g_fleet.tried = true;
    g_fleet.rig = render::build_aircraft_rig();
    const auto& specs = render::aircraft_node_specs();
    // rig-D D.3: load the real Bf 109 F-4 mesh per node from assets/bf109/.
    // Each GLB was exported export_yup=False (SEADS frame preserved: nose stays
    // on -Z, no raylib axis conversion) with an identity node transform (all
    // transforms applied in Blender), so model.meshes[0] is already in the
    // node's local frame. If a GLB is missing/empty, fall back to the
    // placeholder GenMeshCube for THAT node so a single bad asset can't blank
    // the aircraft.
    for (int i = 0; i < render::kNodeCount; ++i) {
        const char* key = specs[i].mesh_key;
        bool loaded = false;
        if (key != nullptr && key[0] != '\0') {
            char path[256];
            std::snprintf(path, sizeof path, "%s/bf109/%s.glb", SEADS_ASSET_DIR,
                          key);
            if (FileExists(path)) {
                Model m = LoadModel(path);
                if (m.meshCount >= 1 && m.meshes != nullptr) {
                    g_fleet.models[i] = m;
                    g_fleet.model_loaded[i] = true;
                    g_fleet.meshes[i] = m.meshes[0];
                    loaded = true;
                } else {
                    UnloadModel(m);  // empty/degenerate GLB
                }
            }
        }
        if (!loaded) {
            const glm::vec3 d =
                specs[i].box_dims;  // shape in the mesh (scl==1)
            g_fleet.meshes[i] = GenMeshCube(d.x, d.y, d.z);
            TraceLog(LOG_WARNING,
                     "FLEET: node %d ('%s') GLB missing; cube fallback", i,
                     key);
        }
    }
    g_fleet.shader = LoadShaderFromMemory(render::mirror_vs_source(),
                                          render::mirror_fs_source());
    g_fleet.loc_color = GetShaderLocation(g_fleet.shader, "u_planeColor");
    g_fleet.loc_sun = GetShaderLocation(g_fleet.shader, "u_sunDir");
    g_fleet.loc_fresnel = GetShaderLocation(g_fleet.shader, "u_fresnelPower");
    g_fleet.loc_refl = GetShaderLocation(g_fleet.shader, "u_reflectivity");
    // Feature A: visibility-cycle uniforms.
    g_fleet.loc_body_floor = GetShaderLocation(g_fleet.shader, "u_bodyFloor");
    g_fleet.loc_emissive = GetShaderLocation(g_fleet.shader, "u_emissive");
    g_fleet.loc_white_mix = GetShaderLocation(g_fleet.shader, "u_whiteMix");
    g_fleet.loc_rim = GetShaderLocation(g_fleet.shader, "u_rimGain");
    g_fleet.loc_rim_white = GetShaderLocation(g_fleet.shader, "u_rimWhite");
    g_fleet.loc_desat = GetShaderLocation(g_fleet.shader, "u_desat");
    // A compile failure leaves raylib's default program (our uniforms absent):
    // loc_color == -1 => fall back to DrawCube.
    if (g_fleet.shader.id == 0 || g_fleet.loc_color < 0) {
        TraceLog(LOG_WARNING,
                 "FLEET: mirror shader unavailable; DrawCube fallback");
        return;
    }
    // Wire the cubemap sampler by hand so DrawMesh binds MATERIAL_MAP_CUBEMAP
    // to it (LoadShader only auto-locates texture0..2), mirroring planet.cpp.
    g_fleet.shader.locs[SHADER_LOC_MAP_CUBEMAP] =
        GetShaderLocation(g_fleet.shader, "env");
    g_fleet.mat = LoadMaterialDefault();
    g_fleet.mat.shader = g_fleet.shader;
    // Matte (default shader): the pilot bust + rubber tyres read as form, not
    // chrome — a mid-dark neutral so they sit INSIDE the mirror airframe.
    g_fleet.matte_mat = LoadMaterialDefault();
    g_fleet.matte_mat.maps[MATERIAL_MAP_DIFFUSE].color = Color{38, 40, 44, 255};
    // Glass (default shader, translucent): the canopy — a cool tint at low
    // alpha, drawn in the back-to-front translucent pass so the pilot shows
    // through.
    g_fleet.glass_mat = LoadMaterialDefault();
    g_fleet.glass_mat.maps[MATERIAL_MAP_DIFFUSE].color =
        Color{150, 175, 195, 90};
    // rig-B prop disc: default-shader material, neutral gray; per-draw alpha is
    // set on its diffuse color (throttle-scaled). Symmetric disc, no spin.
    g_fleet.prop_mat = LoadMaterialDefault();
    g_fleet.prop_mat.maps[MATERIAL_MAP_DIFFUSE].color = Color{40, 44, 50, 255};
    g_fleet.ok = true;
    TraceLog(LOG_INFO, "FLEET: Bf 109 rig built (%d nodes, real meshes)",
             render::kNodeCount);
}

// glm::mat4 -> raylib Matrix. to_ray_fields returns raylib FIELD-DECLARATION
// order (m0,m4,m8,m12,m1,...); designated initializers pin field<-value so a
// raylib struct reorder can't silently transpose the matrix (Fable C1).
Matrix to_ray(const glm::mat4& g) {
    const std::array<float, 16> f = render::to_ray_fields(g);
    return Matrix{.m0 = f[0],
                  .m4 = f[1],
                  .m8 = f[2],
                  .m12 = f[3],
                  .m1 = f[4],
                  .m5 = f[5],
                  .m9 = f[6],
                  .m13 = f[7],
                  .m2 = f[8],
                  .m6 = f[9],
                  .m10 = f[10],
                  .m14 = f[11],
                  .m3 = f[12],
                  .m7 = f[13],
                  .m11 = f[14],
                  .m15 = f[15]};
}

// The landing-gear group (rig-D): struts + wheels + doors + tailwheel. Hidden
// together when the gear is retracted (state.gear <= eps). Doors parent to the
// fuselage (bay-edge hinge, Fable P1-3), so membership is by index, not
// subtree.
bool is_gear_node(int i) {
    switch (i) {
        case render::kLeftGearDoor:
        case render::kLeftGearStrut:
        case render::kLeftWheel:
        case render::kRightGearDoor:
        case render::kRightGearStrut:
        case render::kRightWheel:
        case render::kTailWheel:
            return true;
        default:
            return false;
    }
}

// The articulated mirror aircraft (SPEC §7 body frame: +X right, +Y up, -Z
// forward). Each rig node draws its GenMeshCube through the eye-relative
// body-to-world transform composed with the node's model-space world matrix.
// `enemy` picks the bandit chroma (the world art direction wants the planes as
// the ONLY color; player = hero, bandit = crimson). `deflect` are the COMMANDED
// control Inputs that pose the ailerons/elevator/rudder (rig-B); the gear
// unfolds from state.gear. The PROP is NOT drawn here — it is a translucent
// disc drawn in a SECOND pass (draw_prop) after ALL opaque bodies (Fable C6: a
// depth-write-off disc drawn per-plane is overwritten by a farther plane's
// opaque body drawn later). Falls back to flat DrawCube if the mirror shader is
// unavailable.
// `enemy` selects the VIZ mode (bandits always draw Mirror; the player gets
// whatever mode N is cycled to). `ally_livery` is the S-mapteam separation of
// concerns: a PLAYER-FACTION maverick is a drone (so enemy=true for viz) but
// must wear the ALLY BLUE, never the slag orange (Chad: "make sure all allies
// planes ... are colored blue ... in game"). Defaulted false => every existing
// call site is bit-identical.
void draw_aircraft(const sim::SimState& state, const glm::dvec3& eye,
                   const FleetDrawParams& fp, const sim::Inputs& deflect = {},
                   bool enemy = false, double scale = 1.0,
                   float wheel_roll_rad = 0.0f, bool ally_livery = false,
                   // RUNG E4.1 (VISIBILITY): multiplier on the mirror shader's
                   // env-reflection strength for THIS plane. 1.0 (the default,
                   // and what the player pass always passes) is bit-identical
                   // to the signed build — the drone loop is the only caller
                   // that ever passes anything else. See the seam banner in
                   // render/plane_legibility.h: the grayscale env reflection at
                   // line 281 of render/rig.cpp is what paints the white
                   // scatter light onto an aircraft.
                   float env_wash_scale = 1.0f) {
    ensure_fleet();
    const Vector3 p = rel(state.position, eye);

    // Fall back to DrawCube if the mirror shader is unavailable OR the planet
    // (and thus its albedo cubemap the mirror samples) failed to load — a dark
    // mirror over the fallback green sphere reads worse than the flat livery
    // (Fable after-consult). draw_planet ran before this, so g_planet is built.
    if (!g_fleet.ok || !g_planet.ok) {
        // Fallback: the flat DrawCube aircraft — keeps the sim flyable with no
        // floating geometry.
        const glm::mat4 body_to_world =
            glm::mat4_cast(glm::quat(state.orientation));
        rlPushMatrix();
        rlTranslatef(p.x, p.y, p.z);
        rlMultMatrixf(glm::value_ptr(body_to_world));
        const float s = static_cast<float>(scale);
        rlScalef(s, s, s);
        const Color fuse =
            enemy ? Color{188, 74, 42, 255} : Color{90, 96, 104, 255};
        const Color wing =
            enemy ? Color{158, 58, 32, 255} : Color{70, 76, 84, 255};
        const Color tail =
            enemy ? Color{158, 58, 32, 255} : Color{70, 76, 84, 255};
        const Color fin =
            enemy ? Color{206, 96, 54, 255} : Color{104, 110, 118, 255};
        const Color nose =
            enemy ? Color{240, 208, 72, 255} : Color{190, 60, 50, 255};
        DrawCube(Vector3{0.0f, 0.0f, -0.8f}, 1.2f, 1.3f, 8.5f, fuse);
        DrawCube(Vector3{0.0f, -0.1f, 0.2f}, 11.0f, 0.25f, 2.3f, wing);
        DrawCube(Vector3{0.0f, 0.2f, 3.6f}, 4.2f, 0.2f, 1.3f, tail);
        DrawCube(Vector3{0.0f, 1.0f, 3.7f}, 0.2f, 1.8f, 1.4f, fin);
        DrawCube(Vector3{0.0f, 0.0f, -5.2f}, 0.7f, 0.7f, 1.0f, nose);
        rlPopMatrix();
        return;
    }

    // Eye-relative body-to-world: translate(pos - eye) * R(orientation) *
    // scale. Children are metre-scale local, so this float composition is
    // precision-safe (Fable C4 — the big ~km translation is done in rel(), in
    // double, before the cast).
    const glm::mat4 body_to_world =
        glm::translate(glm::mat4(1.0f), glm::vec3(p.x, p.y, p.z)) *
        glm::mat4_cast(glm::quat(state.orientation)) *
        glm::scale(glm::mat4(1.0f), glm::vec3(static_cast<float>(scale)));

    // Per-plane uniforms. The env cubemap is the planet albedo (built by
    // draw_planet, which runs before this each frame; the !g_planet.ok gate
    // above guarantees it is valid here), sampled read-only.
    g_fleet.mat.maps[MATERIAL_MAP_CUBEMAP].texture = g_planet.cubemap;
    const glm::vec3 col =
        (enemy && !ally_livery) ? fp.bandit_color : fp.player_color;
    const float c3[3] = {col.x, col.y, col.z};
    const float s3[3] = {fp.sun_dir.x, fp.sun_dir.y, fp.sun_dir.z};
    SetShaderValue(g_fleet.shader, g_fleet.loc_color, c3, SHADER_UNIFORM_VEC3);
    SetShaderValue(g_fleet.shader, g_fleet.loc_sun, s3, SHADER_UNIFORM_VEC3);
    SetShaderValue(g_fleet.shader, g_fleet.loc_fresnel, &fp.fresnel_power,
                   SHADER_UNIFORM_FLOAT);
    // Feature A: enemy planes always use Mirror (original look); player gets
    // the active viz mode. refl_scale modulates the config reflectivity.
    const render::VizParams& vp =
        enemy ? render::viz_params(render::PlaneViz::Mirror) : fp.viz;
    // E4.1: env_wash_scale == 1.0f leaves this expression EXACTLY as it was
    // (x * 1.0f is the identity for every finite float), so the knob-off frame
    // is the signed frame down to the uploaded uniform.
    const float refl_scaled = fp.reflectivity * vp.refl_scale * env_wash_scale;
    SetShaderValue(g_fleet.shader, g_fleet.loc_refl, &refl_scaled,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(g_fleet.shader, g_fleet.loc_body_floor, &vp.body_floor,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(g_fleet.shader, g_fleet.loc_emissive, &vp.emissive,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(g_fleet.shader, g_fleet.loc_white_mix, &vp.white_mix,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(g_fleet.shader, g_fleet.loc_rim, &vp.rim_gain,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(g_fleet.shader, g_fleet.loc_rim_white, &vp.rim_white,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(g_fleet.shader, g_fleet.loc_desat, &vp.desat,
                   SHADER_UNIFORM_FLOAT);

    // rig-B: pose the driven surfaces from the commanded Inputs + the actual
    // gear extension, then recompute the node world matrices. The shared rig is
    // fully re-posed per call (player, then each drone), so single-threaded
    // reuse is safe — no leakage between planes.
    render::apply_deflection(g_fleet.rig, deflect,
                             static_cast<float>(state.gear), fp.gains,
                             wheel_roll_rad);

    const auto& specs = render::aircraft_node_specs();
    for (int i = 0; i < render::kNodeCount; ++i) {
        // The prop BLADES draw as a translucent disc in draw_prop's 2nd pass;
        // the solid spinner still draws here.
        if (i == render::kPropBlades) continue;
        // The GLASS canopy is translucent — deferred to the back-to-front pass
        // (draw_prop) so the mirror body behind it composites correctly (D.3).
        if (specs[i].material == render::MaterialClass::Glass) continue;
        // The whole gear group is drawn only while extended (state.gear slews
        // 0->1); at 0 it is tucked in-bay and hidden (no landing in flight).
        if (is_gear_node(i) && state.gear <= 1e-3) continue;
        const glm::mat4 m = body_to_world * g_fleet.rig[i].world;
        DrawMesh(g_fleet.meshes[i], fleet_material(specs[i].material),
                 to_ray(m));
    }
}

// rig-B/D.3: the per-plane TRANSLUCENT surfaces — the propeller blur disc AND
// the glass canopy — drawn in a SEPARATE pass AFTER all opaque bodies (Fable
// C6: a depth-write-off surface is overwritten by a farther plane's later
// opaque body). Alpha blending on, depth-WRITE off (caller wraps the whole pass
// in BeginBlendMode/rlDisableDepthMask), so overlapping discs, canopies, and
// the bodies behind them composite. Both nodes are static (rest world
// constant); only the plane's body_to_world differs. The prop is a
// throttle-scaled opacity (time-free: opacity, not a spinning phase).
void draw_prop(const sim::SimState& state, const glm::dvec3& eye,
               const FleetDrawParams& fp, double scale = 1.0) {
    if (!g_fleet.ok || !g_planet.ok) return;  // fallback aircraft has no prop
    const Vector3 p = rel(state.position, eye);
    const glm::mat4 body_to_world =
        glm::translate(glm::mat4(1.0f), glm::vec3(p.x, p.y, p.z)) *
        glm::mat4_cast(glm::quat(state.orientation)) *
        glm::scale(glm::mat4(1.0f), glm::vec3(static_cast<float>(scale)));
    // Glass canopy first (it sits behind the prop disc from most angles; the
    // depth-write-off pass makes intra-plane order cosmetic, front-lit anyway).
    const glm::mat4 gm = body_to_world * g_fleet.rig[render::kCanopy].world;
    DrawMesh(g_fleet.meshes[render::kCanopy],
             fleet_material(render::MaterialClass::Glass), to_ray(gm));
    // Opacity fills in with throttle (idle floor .. full-throttle disc).
    const float t = static_cast<float>(glm::clamp(state.throttle, 0.0, 1.0));
    const float a =
        fp.prop_idle_alpha + (fp.prop_disc_alpha - fp.prop_idle_alpha) * t;
    Color& c = g_fleet.prop_mat.maps[MATERIAL_MAP_DIFFUSE].color;
    c.a = static_cast<unsigned char>(glm::clamp(a, 0.0f, 1.0f) * 255.0f);
    const glm::mat4 m = body_to_world * g_fleet.rig[render::kPropBlades].world;
    DrawMesh(g_fleet.meshes[render::kPropBlades], g_fleet.prop_mat, to_ray(m));
}

// --- v5 HUD RESTYLE (Chad 2026-07-23): the neon aim reticle, the red
// crosshair nose marker, and the "scared mouse" off-screen/near-screen aim
// marker. Pure 2D overlay, render-only (reads FrameInfo/state, draws
// pixels) — no sim/control/config coupling except the documented
// [push_gate] horizon_enter constant pair below (a NAMED, not derived,
// coupling — render has no ControllerParams to read it live).
namespace aim_buddy {

// --- SKIN SYSTEM (Chad 2026-07-23: "Lets make it a little green caspery
// ghost guy translucent with sunglasses and bald head. It will become an
// economy item when game is published."). Mascot skins are a PLANNED
// published-game economy item — the shape exists in code today even though
// only one skin ships: `kAimBuddySkin` is the one-line swap an artist/design
// pass will later drive from a player selection instead of a compile-time
// constant. Every retired skin is kept FULLY compilable (never deleted) so
// swapping back, or adding a skin picker later, is a one-line change, not an
// archaeology dig.
enum class BuddySkin { kMouse, kGhost };
constexpr BuddySkin kAimBuddySkin = BuddySkin::kGhost;  // the SHIPPED skin

// --- FEAR SOURCE (render-side, read-only): how close the TRUE aim is to
// diving through the split-S knife edge. Two hardcoded elevation
// thresholds — render cannot read [push_gate] live (no ControllerParams
// here), so these are a DOCUMENTED constant pair coupled to
// config/controller.toml's [push_gate] horizon_enter (45 deg below horizon
// today): if that knife edge is ever retuned, move kMouseFearCommitDeg with
// it. kMouseFearStartDeg is a render-only "getting nervous" lead-in, not a
// kernel dial.
// Rung F "THE SACRED MIDDLE" note (v5 kernel-v5-reconcile): the controller
// now ALSO engages push via a narrower in-plane arm (any depth below the
// horizon, within side_pure_enter of the vertical plane) — so a straight-
// down IN-PLANE dive can commit to push WITHOUT crossing this 45-deg LATERAL
// commitment line. That is intentional, not a miss: the ghost's fear read
// stays keyed to the LATERAL knife edge (the real "am I about to roll over
// sideways" scare, still gated by horizon_enter/side_cone), and a straight-
// down dive is covered instead by the BRACE-FOR-IMPACT read below (deck
// proximity / time-to-impact), which fires independently of this fear axis.
constexpr double kMouseFearStartDeg = 15.0;   // [deg below horizon] nervous
constexpr double kMouseFearCommitDeg = 45.0;  // [deg below horizon] the
                                              // commit line == [push_gate]
                                              // horizon_enter
const double kMouseFearStart = -std::sin(kMouseFearStartDeg * PI / 180.0);
const double kMouseFearCommit = -std::sin(kMouseFearCommitDeg * PI / 180.0);

// Linear-parameter smoothstep between two edges that need not be ordered
// low->high (kMouseFearStart > kMouseFearCommit: elevation DECREASES as the
// dive deepens) — the usual t=(x-e0)/(e1-e0) formula is direction-agnostic,
// only the clamp+cubic-ease need the explicit form (glm::smoothstep assumes
// edge0<edge1).
double smoothstep01(double edge0, double edge1, double x) {
    const double denom = edge1 - edge0;
    double t = std::abs(denom) > 1e-12 ? (x - edge0) / denom
                                       : (x >= edge1 ? 1.0 : 0.0);
    t = std::clamp(t, 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

// --- SPLASH one-shot (the FleetRig static-persistent-state pattern): the
// ~2 s panic transformation the first time fear01 commits to the knife
// edge, re-arming only after fear relaxes back below kSplashRearm. Kept as
// its own tiny struct + constants so a future artist pass can swap the
// splash BODY (the panic-form draw below) without touching this trigger.
constexpr float kSplashFear = 0.95f;
constexpr float kSplashRearm = 0.5f;
constexpr double kSplashDuration = 2.0;  // [s]

// --- RELIEF one-shot (Chad 2026-07-23: "also relief at safety"): a ~1.5 s
// exhale/sag beat that plays once fear01 has fallen back below
// kReliefFear having previously exceeded kReliefHighWater, re-arming only
// after fear01 climbs back above kReliefRearm. Same hysteretic-latch shape
// as the splash above, folded into the same persistent struct (the
// FleetRig static-persistent-state pattern — one glyph, one state block).
constexpr float kReliefHighWater = 0.7f;
constexpr float kReliefFear = 0.25f;
constexpr float kReliefRearm = 0.5f;
constexpr double kReliefDuration = 1.5;  // [s]
struct SplashState {
    float last_fear = 0.0f;
    double splash_start_s = -1e18;  // far past = never fired
    bool armed = true;

    // Relief tracking (independent latch, shares nothing with the splash
    // trigger above so a panic-splash and a later relief beat can never
    // fight over one bit of state).
    float fear_high_water = 0.0f;
    double relief_start_s = -1e18;  // far past = never fired
    bool relief_armed = true;
};
SplashState g_splash;

// Outline-then-fill helpers: draw the shape slightly larger in BLACK first
// (the "embossed... outlined in black" ask), then the real color on top.
void circle_outlined(Vector2 c, float r, Color col, float outline_px) {
    DrawCircleV(c, r + outline_px, BLACK);
    DrawCircleV(c, r, col);
}
void ellipse_outlined(Vector2 c, float rx, float ry, Color col,
                      float outline_px) {
    DrawEllipse(static_cast<int>(c.x), static_cast<int>(c.y), rx + outline_px,
                ry + outline_px, BLACK);
    DrawEllipse(static_cast<int>(c.x), static_cast<int>(c.y), rx, ry, col);
}

// The scared-mouse glyph itself (Chad: "a little mouse... bright green and
// easy to see embossed in white outlined in black and stylish accents like
// sunglasses and buckteeth" + the split-S fear ramp + the 2 s splash).
// Drawn UPRIGHT always — `dir_unit` feeds ONLY the short pointer tick (a
// legible "which way" cue), never a rotation of the mouse itself (Chad
// asked for a legible glyph, not a spinning one). `scale` is an ADDITIVE
// parameter beyond the ask's literal 4-arg signature (default 1.0 = the
// ~32 px off-screen size) so the SAME function serves the smaller on-screen
// docked variant (~22 px, scale ~0.69) without duplicating the art.
// RETIRED (2026-07-23, superseded by kGhost) — kept fully compilable and
// selectable as skin kMouse; every mechanism it defined (fear bands, splash,
// tremble, pointer tick) is shared/mirrored by draw_aim_ghost below.
void draw_aim_mouse(Vector2 anchor, Vector2 dir_unit, float fear01,
                    double now_s, float scale = 1.0f) {
    fear01 = std::clamp(fear01, 0.0f, 1.0f);

    // Splash trigger/re-arm (edge-detected via the `armed` latch: it can
    // only be true again once fear01 has actually dropped below
    // kSplashRearm, so a fear01 that hovers just under kSplashFear cannot
    // re-fire the pulse every frame).
    if (fear01 >= kSplashFear && g_splash.armed) {
        g_splash.splash_start_s = now_s;
        g_splash.armed = false;
    }
    if (fear01 < kSplashRearm) g_splash.armed = true;
    g_splash.last_fear = fear01;
    const double splash_t = (now_s - g_splash.splash_start_s) / kSplashDuration;
    const bool splashing = splash_t >= 0.0 && splash_t < 1.0;

    // TREMBLE (display-only wobble; deterministic in now_s, per-axis phase
    // offset so it reads as a shake, not a diagonal slide) — only in the
    // fully-scared band.
    Vector2 a = anchor;
    if (fear01 > 0.7f) {
        a.x += static_cast<float>(std::sin(now_s * 40.0)) * fear01 * 2.0f;
        a.y += static_cast<float>(std::cos(now_s * 40.0 + 1.3)) * fear01 * 2.0f;
    }

    // The splash PULSE: the glyph scales up ~1.5x and back over the window
    // (sin(pi*t) peaks at t=0.5).
    float s = scale;
    if (splashing) {
        const float pt = static_cast<float>(std::clamp(splash_t, 0.0, 1.0));
        s *= 1.0f + 0.5f * std::sin(PI * pt);
    }

    // The splash RINGS: 2-3 expanding, fading screen-space circles centered
    // on the glyph, drawn FIRST (behind the mouse).
    if (splashing) {
        constexpr int kRings = 3;
        for (int i = 0; i < kRings; ++i) {
            const double ring_t = std::clamp(splash_t - i * 0.12, 0.0, 1.0);
            if (ring_t <= 0.0) continue;
            const float radius =
                (14.0f + static_cast<float>(ring_t) * 46.0f) * scale;
            const unsigned char al =
                static_cast<unsigned char>((1.0 - ring_t) * 180.0);
            DrawCircleLinesV(a, radius, Color{60, 255, 120, al});
        }
    }

    constexpr Color kBody{57, 255, 60, 255};     // bright neon-green body
    constexpr Color kWhite{250, 250, 250, 255};  // emboss highlights
    const float outline = 1.7f * s;

    // Tail: a curling flick off the body's lower-right (a few segments),
    // drawn UNDER the body.
    {
        const Vector2 t0{a.x + 8.0f * s, a.y + 2.0f * s};
        const Vector2 t1{a.x + 14.0f * s, a.y - 1.0f * s};
        const Vector2 t2{a.x + 12.0f * s, a.y - 7.0f * s};
        const Vector2 t3{a.x + 6.0f * s, a.y - 6.0f * s};
        DrawLineEx(t0, t1, 2.6f * s, BLACK);
        DrawLineEx(t1, t2, 2.6f * s, BLACK);
        DrawLineEx(t2, t3, 2.6f * s, BLACK);
        DrawLineEx(t0, t1, 1.4f * s, kBody);
        DrawLineEx(t1, t2, 1.4f * s, kBody);
        DrawLineEx(t2, t3, 1.4f * s, kBody);
    }

    // Body (a squat ellipse) + head (a circle) + two round ears.
    const Vector2 body_c{a.x, a.y - 6.0f * s};
    const Vector2 head_c{a.x, a.y - 16.0f * s};
    const Vector2 ear_l{a.x - 6.0f * s, a.y - 22.5f * s};
    const Vector2 ear_r{a.x + 6.0f * s, a.y - 22.5f * s};
    const float body_rx = 9.0f * s, body_ry = 7.0f * s;
    const float head_r = 8.0f * s;
    const float ear_r_px = 4.5f * s;

    ellipse_outlined(body_c, body_rx, body_ry, kBody, outline);
    circle_outlined(ear_l, ear_r_px, kBody, outline);
    circle_outlined(ear_r, ear_r_px, kBody, outline);
    circle_outlined(head_c, head_r, kBody, outline);

    // WHITE EMBOSS: a highlight crescent (a shaded pie wedge, upper-left of
    // the head) + thin white rim arcs on the ears.
    DrawCircleSector(Vector2{head_c.x - 2.0f * s, head_c.y - 2.0f * s},
                     head_r * 0.55f, 200.0f, 260.0f, 8, kWhite);
    DrawRing(ear_l, ear_r_px - 1.6f * s, ear_r_px - 0.4f * s, 0.0f, 360.0f, 16,
             Color{250, 250, 250, 200});
    DrawRing(ear_r, ear_r_px - 1.6f * s, ear_r_px - 0.4f * s, 0.0f, 360.0f, 16,
             Color{250, 250, 250, 200});

    // --- FACE, by fear band ---
    const Vector2 eye_l{head_c.x - 3.4f * s, head_c.y - 1.0f * s};
    const Vector2 eye_r{head_c.x + 3.4f * s, head_c.y - 1.0f * s};

    if (fear01 < 0.3f) {
        // COOL: sunglasses on, level.
        const Rectangle band{head_c.x - 6.5f * s, head_c.y - 2.6f * s,
                             13.0f * s, 3.4f * s};
        DrawRectangleRounded(band, 0.6f, 6, BLACK);
        circle_outlined(eye_l, 2.6f * s, BLACK, 1.0f * s);
        circle_outlined(eye_r, 2.6f * s, BLACK, 1.0f * s);
        DrawCircleV(Vector2{eye_l.x - 0.8f * s, eye_l.y - 0.8f * s}, 0.7f * s,
                    kWhite);  // glints
        DrawCircleV(Vector2{eye_r.x - 0.8f * s, eye_r.y - 0.8f * s}, 0.7f * s,
                    kWhite);
    } else {
        // Eyes visible (white sclera + black pupil), bigger the more
        // scared.
        const float eye_r_px = (fear01 < 0.7f ? 3.0f : 4.0f) * s;
        circle_outlined(eye_l, eye_r_px, kWhite, 1.0f * s);
        circle_outlined(eye_r, eye_r_px, kWhite, 1.0f * s);
        DrawCircleV(eye_l, eye_r_px * 0.5f, BLACK);
        DrawCircleV(eye_r, eye_r_px * 0.5f, BLACK);

        if (fear01 < 0.7f) {
            // NERVOUS: the shades SLIP — drawn lower + skewed, via
            // DrawRectanglePro's rotation.
            Rectangle band{head_c.x, head_c.y + 2.0f * s, 13.0f * s, 3.2f * s};
            Vector2 origin{6.5f * s, 1.6f * s};
            DrawRectanglePro(band, origin, 12.0f, BLACK);
        } else {
            // SCARED: glasses FLUNG — a tiny pair offset up and away,
            // tilted, as if knocked off.
            Rectangle band{head_c.x - 10.0f * s, head_c.y - 16.0f * s, 9.0f * s,
                           2.4f * s};
            Vector2 origin{4.5f * s, 1.2f * s};
            DrawRectanglePro(band, origin, -35.0f, BLACK);
        }

        // Mouth: a small black O behind the buckteeth, bigger when scared.
        const float mouth_r = (fear01 < 0.7f ? 2.0f : 3.4f) * s;
        DrawCircleV(Vector2{head_c.x, head_c.y + 5.5f * s}, mouth_r, BLACK);

        // Sweat drops: 1-2 nervous, more scared — small blue-white
        // teardrops off the temple.
        const int drops = fear01 < 0.7f ? 2 : 3;
        for (int i = 0; i < drops; ++i) {
            const Vector2 dp{head_c.x + (8.5f + 3.0f * i) * s,
                             head_c.y - (6.0f - 3.0f * i) * s};
            DrawCircleV(dp, 1.6f * s, Color{210, 235, 255, 230});
            DrawTriangle(
                Vector2{dp.x - 1.6f * s, dp.y}, Vector2{dp.x + 1.6f * s, dp.y},
                Vector2{dp.x, dp.y - 3.2f * s}, Color{210, 235, 255, 230});
        }
    }

    // BUCKTEETH: two small white rounded rects below the mouth, a thin
    // black split between them. Drawn LAST (over the mouth/glasses) so
    // they always read as the mouse's signature.
    {
        const float ty = head_c.y + 5.0f * s;
        const Rectangle tl{head_c.x - 3.0f * s, ty, 2.6f * s, 4.0f * s};
        const Rectangle tr{head_c.x + 0.4f * s, ty, 2.6f * s, 4.0f * s};
        DrawRectangleRounded(tl, 0.4f, 4, kWhite);
        DrawRectangleRounded(tr, 0.4f, 4, kWhite);
        DrawLineEx(Vector2{head_c.x, ty}, Vector2{head_c.x, ty + 4.0f * s},
                   1.0f * s, BLACK);
    }

    // Pointer tick: a short green stroke from the glyph toward `dir_unit`
    // (off-screen: toward the screen edge the true aim sits beyond;
    // on-screen docked: toward the reticle it accompanies) — keeps the
    // direction legible without rotating the art.
    const float dlen =
        std::sqrt(dir_unit.x * dir_unit.x + dir_unit.y * dir_unit.y);
    if (dlen > 1e-6f) {
        const Vector2 ud{dir_unit.x / dlen, dir_unit.y / dlen};
        const Vector2 p0{a.x + ud.x * 18.0f * s, a.y + ud.y * 18.0f * s};
        const Vector2 p1{a.x + ud.x * 27.0f * s, a.y + ud.y * 27.0f * s};
        DrawLineEx(p0, p1, 2.2f * s, kBody);
    }
}

// Animated flying sweat: 1-3 small blue-white droplets that spawn at the
// crown and arc off the head (a simple flung parabola driven by
// fmod(now_s, ~0.8s) per-drop phase), count/size scaling with fear01. A
// no-op below kSweatStartFear so the cool band reads "on watch," not
// "sweating buckets."
constexpr float kSweatStartFear = 0.15f;
constexpr double kSweatPeriod = 0.8;  // [s] one flung-off flight
void draw_ghost_sweat(Vector2 head_c, float head_r, float fear01, double now_s,
                      float s) {
    if (fear01 < kSweatStartFear) return;
    int drops = 1;
    if (fear01 >= 0.45f) drops = 2;
    if (fear01 >= 0.7f) drops = 3;
    for (int i = 0; i < drops; ++i) {
        const double phase =
            std::fmod(now_s + i * 0.29, kSweatPeriod) / kSweatPeriod;
        const float pd = static_cast<float>(phase);
        const float side = (i % 2 == 0) ? -1.0f : 1.0f;
        const Vector2 crown{head_c.x + side * 1.5f * s,
                            head_c.y - head_r * 0.85f};
        // Flung sideways off the crown, arcing down (a small parabola).
        const float dx = side * (2.5f + 6.0f * pd) * s;
        const float dy = (-2.0f * pd + 7.0f * pd * pd) * s;
        const Vector2 p{crown.x + dx, crown.y + dy};
        const float r = (0.9f + 0.5f * fear01) * s;
        const unsigned char al =
            static_cast<unsigned char>((1.0f - pd) * 220.0f);
        DrawCircleV(p, r, Color{210, 235, 255, al});
        DrawTriangle(Vector2{p.x - r, p.y}, Vector2{p.x + r, p.y},
                     Vector2{p.x, p.y - r * 2.0f}, Color{210, 235, 255, al});
    }
}

// The PANIC form (replaces the old scale-pulse+rings splash entirely,
// Chad: "make the ghost have a cooler form"): a WAIL-STREAK transformation
// over the g_splash ~2 s window, `t01` = 0..1 through it.
//   t 0.00-0.25  the body stretches vertically ~1.6x, eyes clench then fly
//                open huge, mouth becomes a big screaming O.
//   t 0.25-0.75  full wail: the hem streams into a flowing comet tail,
//                violent tremble, shed wisp particles, flickering scream
//                lines off the mouth.
//   t 0.75-1.00  snap back: one overshoot bounce to normal, wisps fade.
// Owns its own full body draw (skin-local — draw_aim_mouse's splash is
// untouched, its ring/pulse code was never a shared helper).
void draw_ghost_panic(Vector2 anchor, float t01, double now_s, float scale) {
    t01 = std::clamp(t01, 0.0f, 1.0f);
    const Color kGhostFill{57, 255, 60, 110};
    const Color kGhostRim{57, 255, 60, 200};
    constexpr Color kWhite{250, 250, 250, 255};

    float stretch, intensity, eye_open, mouth_open, tail_amt;
    if (t01 < 0.25f) {
        const float u = t01 / 0.25f;
        const float ease = u * u * (3.0f - 2.0f * u);
        stretch = 1.0f + 0.6f * ease;
        eye_open = u < 0.5f ? 0.0f : (u - 0.5f) / 0.5f;  // clench, fly open
        mouth_open = u;
        tail_amt = u * 0.3f;
        intensity = u * 0.4f;
    } else if (t01 < 0.75f) {
        stretch = 1.6f;
        eye_open = 1.0f;
        mouth_open = 1.0f;
        tail_amt = 1.0f;
        intensity = 1.0f;
    } else {
        const float u = (t01 - 0.75f) / 0.25f;
        stretch = 1.0f + 0.6f * (1.0f - u) +
                  0.12f * std::sin(u * PI * 2.0f) * (1.0f - u);  // one bounce
        eye_open = 1.0f - u;
        mouth_open = 1.0f - u;
        tail_amt = 1.0f - u;
        intensity = 1.0f - u;
    }

    const float s = scale;

    // Violent tremble, peaking in the wail phase.
    Vector2 a = anchor;
    a.x += static_cast<float>(std::sin(now_s * 55.0)) * intensity * 3.0f * s;
    a.y +=
        static_cast<float>(std::cos(now_s * 55.0 + 0.7)) * intensity * 2.0f * s;

    const float head_r = 9.0f * s;
    const float body_half_w = 9.5f * s * (1.0f - 0.15f * tail_amt);
    const Vector2 head_c{a.x, a.y - 15.0f * s * stretch};
    const float body_top = head_c.y;
    const float hem_y = a.y + 9.0f * s * stretch;
    const float rim_px = 1.2f * s;

    DrawCircleV(head_c, head_r + rim_px, kGhostRim);
    DrawRectangle(static_cast<int>(a.x - body_half_w - rim_px),
                  static_cast<int>(body_top),
                  static_cast<int>(2.0f * (body_half_w + rim_px)),
                  static_cast<int>(hem_y - body_top), kGhostRim);
    DrawCircleV(head_c, head_r, kGhostFill);
    DrawRectangle(static_cast<int>(a.x - body_half_w),
                  static_cast<int>(body_top),
                  static_cast<int>(2.0f * body_half_w),
                  static_cast<int>(hem_y - body_top), kGhostFill);

    // Hem -> comet tail: 3 trailing wisps (sin-driven flutter) once the
    // wail has started to build, plus shed wisp particles peeling off and
    // fading (each its own fmod(now_s, ~0.6s) phase).
    if (tail_amt > 0.05f) {
        constexpr int kWisps = 3;
        for (int i = 0; i < kWisps; ++i) {
            const float wt = (i + 0.5f) / static_cast<float>(kWisps);
            const float x0 = a.x - body_half_w + 2.0f * body_half_w * wt;
            const float len = (10.0f + 14.0f * tail_amt) * s;
            const float flutter =
                static_cast<float>(std::sin(now_s * 10.0 + i * 2.1)) * 4.0f *
                s * tail_amt;
            const Vector2 p0{x0, hem_y};
            const Vector2 p1{x0 + flutter * 0.5f, hem_y + len * 0.5f};
            const Vector2 p2{x0 + flutter, hem_y + len};
            DrawLineEx(p0, p1, 3.4f * s, kGhostRim);
            DrawLineEx(p1, p2, 2.6f * s, kGhostFill);
        }

        constexpr int kParticles = 5;
        for (int i = 0; i < kParticles; ++i) {
            constexpr double kPartPeriod = 0.6;
            const double phase =
                std::fmod(now_s + i * 0.13, kPartPeriod) / kPartPeriod;
            const float along = static_cast<float>(phase);
            const float px =
                a.x + (i % 2 == 0 ? -1.0f : 1.0f) * (3.0f + 10.0f * along) * s;
            const float py = hem_y + along * 22.0f * s * tail_amt;
            const unsigned char al =
                static_cast<unsigned char>((1.0f - along) * 140.0f * tail_amt);
            DrawCircleV(Vector2{px, py}, 1.6f * s, Color{120, 255, 130, al});
        }
    }

    // Arms flung up in fright.
    const Vector2 arm_l{a.x - body_half_w - 1.0f * s,
                        head_c.y + 6.0f * s * stretch};
    const Vector2 arm_r{a.x + body_half_w + 1.0f * s,
                        head_c.y + 6.0f * s * stretch};
    const float arm_r_px = 3.0f * s;
    DrawCircleV(arm_l, arm_r_px + rim_px, kGhostRim);
    DrawCircleV(arm_r, arm_r_px + rim_px, kGhostRim);
    DrawCircleV(arm_l, arm_r_px, kGhostFill);
    DrawCircleV(arm_r, arm_r_px, kGhostFill);

    // Eyes: clenched shut -> flown open huge.
    const Vector2 eye_l{head_c.x - 3.4f * s, head_c.y - 1.0f * s};
    const Vector2 eye_r{head_c.x + 3.4f * s, head_c.y - 1.0f * s};
    if (eye_open < 0.5f) {
        DrawLineEx(Vector2{eye_l.x - 2.5f * s, eye_l.y},
                   Vector2{eye_l.x + 2.5f * s, eye_l.y}, 1.6f * s, BLACK);
        DrawLineEx(Vector2{eye_r.x - 2.5f * s, eye_r.y},
                   Vector2{eye_r.x + 2.5f * s, eye_r.y}, 1.6f * s, BLACK);
    } else {
        const float eo = (eye_open - 0.5f) / 0.5f;
        const float eye_r_px = (3.5f + 2.0f * eo) * s;
        DrawCircleV(eye_l, eye_r_px, kWhite);
        DrawCircleV(eye_r, eye_r_px, kWhite);
        DrawCircleV(eye_l, eye_r_px * 0.45f, BLACK);
        DrawCircleV(eye_r, eye_r_px * 0.45f, BLACK);
    }

    // Mouth: screaming O, with flickering scream lines during the full wail.
    if (mouth_open > 0.05f) {
        const float mouth_r = (1.0f + 2.6f * mouth_open) * s;
        const Vector2 mouth_c{head_c.x, head_c.y + 5.5f * s * stretch};
        DrawCircleV(mouth_c, mouth_r, BLACK);
        DrawRing(mouth_c, mouth_r * 0.55f, mouth_r * 0.7f, 0.0f, 360.0f, 10,
                 Color{80, 20, 20, 200});

        if (tail_amt > 0.6f && std::sin(now_s * 30.0) > 0.0) {
            constexpr int kLines = 3;
            for (int i = 0; i < kLines; ++i) {
                const float ang = (-40.0f + 40.0f * i) * PI / 180.0f;
                const float sn = static_cast<float>(std::sin(ang));
                const float cs = static_cast<float>(std::cos(ang));
                const Vector2 p0{mouth_c.x + sn * mouth_r * 1.3f,
                                 mouth_c.y + cs * mouth_r * 1.3f};
                const Vector2 p1{mouth_c.x + sn * mouth_r * 2.1f,
                                 mouth_c.y + cs * mouth_r * 2.1f};
                DrawLineEx(p0, p1, 1.4f * s, kWhite);
            }
        }
    }
}

// The translucent green ghost glyph (Chad: "a little green caspery ghost
// guy translucent with sunglasses and bald head" — the future economy-item
// skin). Reuses the SAME fear/splash/tremble machinery as draw_aim_mouse
// (g_splash, kSplashFear/kSplashRearm/kSplashDuration, the fear01 bands, the
// pointer tick) — only the ART changes, PLUS its own panic form and relief
// beat (both skin-local, not shared with draw_aim_mouse). Generic
// friendly-ghost silhouette (round bald head, no ears, stubby side nubs for
// arms, body tapering to a wavy scalloped hem, no legs) — deliberately NOT
// a copy of any specific copyrighted character's exact face/proportions.
// BRACE-FOR-IMPACT (Chad 2026-07-24: "it would be also cool if [the ghost]
// braced for impact if I ... am looping when I get close to the deck ... on
// downward AoA im seeing the little guy getting ready to accept his fate"
// then relief on the pull-up). `brace01` is a SEPARATE render-only read from
// AGL + descent rate (computed at the call site, draw_frame) — a scarier
// TIER than the split-S lateral fear01 above, keyed to "about to hit the
// ground", not "about to roll over sideways". `bracing` OVERRIDES the
// fear-tier face/pose whenever it reads scarier than the lateral fear; the
// RELIEF one-shot below fires off the HIGH-WATER of whichever read was
// scarier (max(fear01,brace01)) so pulling up out of either kind of scare
// gets the same exhale beat.
void draw_aim_ghost(Vector2 anchor, Vector2 dir_unit, float fear01,
                    float brace01, double now_s, float scale = 1.0f) {
    fear01 = std::clamp(fear01, 0.0f, 1.0f);
    brace01 = std::clamp(brace01, 0.0f, 1.0f);
    const float active01 = std::max(fear01, brace01);
    const bool bracing = brace01 > fear01;

    // Splash (panic) trigger/re-arm — identical mechanism to draw_aim_mouse,
    // shared g_splash (only one skin is ever live at a time, kAimBuddySkin).
    // Keyed on active01 so a brace-only scare (deck rush with a shallow
    // lateral aim) can ALSO trip the panic splash.
    if (active01 >= kSplashFear && g_splash.armed) {
        g_splash.splash_start_s = now_s;
        g_splash.armed = false;
    }
    if (active01 < kSplashRearm) g_splash.armed = true;
    g_splash.last_fear = active01;
    const double splash_t = (now_s - g_splash.splash_start_s) / kSplashDuration;
    const bool splashing = splash_t >= 0.0 && splash_t < 1.0;

    // RELIEF trigger/re-arm (Chad: "also relief at safety") — a separate
    // hysteretic latch keyed off a HIGH-WATER read of active01 (fear OR
    // brace, whichever scared him more) so it only fires after a real scare,
    // not a brief blip through the low band.
    g_splash.fear_high_water = std::max(g_splash.fear_high_water, active01);
    if (active01 < kReliefFear && g_splash.fear_high_water > kReliefHighWater &&
        g_splash.relief_armed) {
        g_splash.relief_start_s = now_s;
        g_splash.relief_armed = false;
        g_splash.fear_high_water = 0.0f;
    }
    if (active01 > kReliefRearm) g_splash.relief_armed = true;
    const double relief_t = (now_s - g_splash.relief_start_s) / kReliefDuration;
    const bool relieving = !splashing && relief_t >= 0.0 && relief_t < 1.0;

    // TREMBLE — same shake as the mouse, only in the fully-scared band, and
    // only OUTSIDE panic (the panic form has its own, stronger tremble).
    // Bracing trembles HARDER (1.5x) — the deck-rush scare reads heavier
    // than the lateral one.
    Vector2 a = anchor;
    if (!splashing && active01 > 0.7f) {
        const float amp = bracing ? 1.5f : 1.0f;
        a.x +=
            static_cast<float>(std::sin(now_s * 40.0)) * active01 * 2.0f * amp;
        a.y += static_cast<float>(std::cos(now_s * 40.0 + 1.3)) * active01 *
               2.0f * amp;
    }

    float s = scale;
    if (relieving) {
        // A slow exhale: squash to ~0.92x then recover over the beat.
        const float rt = static_cast<float>(std::clamp(relief_t, 0.0, 1.0));
        const float squash =
            1.0f - 0.08f * std::sin(PI * std::min(rt / 0.4f, 1.0f));
        s *= squash;
    } else if (bracing) {
        // Crouched — compressed toward the deck, ~0.85x height.
        s *= 0.85f;
    }

    if (splashing) {
        draw_ghost_panic(a, static_cast<float>(std::clamp(splash_t, 0.0, 1.0)),
                         now_s, s);
    } else {
        // Body alpha PULSES at ~6 Hz in the fully-scared band ("flickering
        // with fright"); a steady MORE TRANSLUCENT 85 otherwise (was 140 —
        // Chad: "I need it more translucent").
        unsigned char body_alpha = 85;
        if (active01 > 0.7f) {
            const double pulse = std::sin(now_s * 2.0 * PI * 6.0);
            body_alpha = static_cast<unsigned char>(
                std::clamp(85.0 + 35.0 * pulse, 0.0, 255.0));
        }
        const Color kGhostFill{57, 255, 60, body_alpha};  // translucent green
        const Color kGhostRim{57, 255, 60, 150};  // soft rim, still legible
        constexpr Color kWhite{250, 250, 250, 255};

        // Geometry: bald round head merging into a tapering body, no ears,
        // no legs.
        const Vector2 head_c{a.x, a.y - 15.0f * s};
        const float head_r = 9.0f * s;
        const float body_half_w = 9.5f * s;
        const float body_top = head_c.y;
        const float hem_y = a.y + 9.0f * s;
        const float rim_px = 1.2f * s;

        // Hem wave speed/amplitude by fear band (Chad: "hem waves gently"
        // cool, "waves faster" nervous/scared).
        float wave_speed = 3.0f, wave_amp = 1.0f;
        if (active01 >= 0.3f && active01 < 0.7f) {
            wave_speed = 6.0f;
            wave_amp = 1.5f;
        } else if (active01 >= 0.7f) {
            wave_speed = 9.0f;
            wave_amp = 2.0f;
        }

        // RIM first (soft, slightly larger), FILL on top — the ghost's
        // "soft" outline in place of the mouse's hard black one.
        DrawCircleV(head_c, head_r + rim_px, kGhostRim);
        DrawRectangle(static_cast<int>(a.x - body_half_w - rim_px),
                      static_cast<int>(body_top),
                      static_cast<int>(2.0f * (body_half_w + rim_px)),
                      static_cast<int>(hem_y - body_top), kGhostRim);
        DrawCircleV(head_c, head_r, kGhostFill);
        DrawRectangle(static_cast<int>(a.x - body_half_w),
                      static_cast<int>(body_top),
                      static_cast<int>(2.0f * body_half_w),
                      static_cast<int>(hem_y - body_top), kGhostFill);

        // WAVY SCALLOPED HEM: 4 bumps, each independently phased so the hem
        // reads as a traveling wave, not a rigid bounce.
        constexpr int kScallops = 4;
        const float scallop_r = (2.0f * body_half_w / kScallops) * 0.62f;
        for (int i = 0; i < kScallops; ++i) {
            const float t = (i + 0.5f) / static_cast<float>(kScallops);
            const float x = a.x - body_half_w + 2.0f * body_half_w * t;
            const float phase = i * 1.3f;
            const float wave =
                static_cast<float>(std::sin(now_s * wave_speed + phase)) *
                wave_amp * s;
            const Vector2 sc{x, hem_y + wave};
            DrawCircleSector(sc, scallop_r + rim_px, 0.0f, 180.0f, 10,
                             kGhostRim);
            DrawCircleSector(sc, scallop_r, 0.0f, 180.0f, 10, kGhostFill);
        }

        // Stubby side-nub arms (no hands/fingers — a friendly, generic
        // silhouette). BRACING pulls them up and IN, in front of the face
        // (Chad: "getting ready to accept his fate") instead of resting at
        // the sides.
        const Vector2 arm_l =
            bracing
                ? Vector2{head_c.x - 4.5f * s, head_c.y - 2.0f * s}
                : Vector2{a.x - body_half_w - 1.0f * s, head_c.y + 6.0f * s};
        const Vector2 arm_r =
            bracing
                ? Vector2{head_c.x + 4.5f * s, head_c.y - 2.0f * s}
                : Vector2{a.x + body_half_w + 1.0f * s, head_c.y + 6.0f * s};
        const float arm_r_px = 3.0f * s;
        DrawCircleV(arm_l, arm_r_px + rim_px, kGhostRim);
        DrawCircleV(arm_r, arm_r_px + rim_px, kGhostRim);
        DrawCircleV(arm_l, arm_r_px, kGhostFill);
        DrawCircleV(arm_r, arm_r_px, kGhostFill);

        // Faint white emboss crescent on the head + a bald-crown specular
        // arc.
        DrawCircleSector(Vector2{head_c.x - 2.0f * s, head_c.y - 2.0f * s},
                         head_r * 0.55f, 200.0f, 260.0f, 8,
                         Color{250, 250, 250, 120});
        DrawRing(Vector2{head_c.x, head_c.y - 3.0f * s}, head_r * 0.35f,
                 head_r * 0.42f, 300.0f, 340.0f, 8, Color{255, 255, 255, 190});

        // --- FACE, by fear band (the ONE opaque element: sunglasses) ---
        const Vector2 eye_l{head_c.x - 3.4f * s, head_c.y - 1.0f * s};
        const Vector2 eye_r{head_c.x + 3.4f * s, head_c.y - 1.0f * s};

        if (relieving) {
            // RELIEF: eyes close to happy arcs, the sunglasses SLIDE BACK
            // ON (descend from above the head to seated over the beat), one
            // last big sweat drop rolls off, and a tiny 'phew' puff.
            const float rt = static_cast<float>(std::clamp(relief_t, 0.0, 1.0));

            DrawRing(Vector2{eye_l.x, eye_l.y + 1.0f * s}, 2.0f * s, 2.8f * s,
                     200.0f, 340.0f, 8, BLACK);
            DrawRing(Vector2{eye_r.x, eye_r.y + 1.0f * s}, 2.0f * s, 2.8f * s,
                     200.0f, 340.0f, 8, BLACK);

            const float seat_y = head_c.y - 2.6f * s;
            const float start_y = head_c.y - head_r * 2.2f;
            const float band_y =
                start_y + (seat_y - start_y) * std::min(rt / 0.7f, 1.0f);
            const Rectangle band{head_c.x - 6.5f * s, band_y, 13.0f * s,
                                 3.4f * s};
            DrawRectangleRounded(band, 0.6f, 6, BLACK);

            if (rt < 0.8f) {
                const float dp = rt / 0.8f;
                const Vector2 p{head_c.x + head_r * 0.95f,
                                head_c.y - head_r * 0.3f + dp * head_r * 1.6f};
                const unsigned char al =
                    static_cast<unsigned char>((1.0f - dp) * 230.0f);
                DrawCircleV(p, 1.8f * s, Color{210, 235, 255, al});
            }

            constexpr int kPuffs = 3;
            const Vector2 mouth_c{head_c.x, head_c.y + 5.0f * s};
            for (int i = 0; i < kPuffs; ++i) {
                const float pt = std::clamp(rt * 1.3f - i * 0.15f, 0.0f, 1.0f);
                if (pt <= 0.0f) continue;
                const Vector2 p{mouth_c.x + (i - 1) * 2.0f * s,
                                mouth_c.y - pt * 10.0f * s};
                const unsigned char al =
                    static_cast<unsigned char>((1.0f - pt) * 200.0f);
                DrawCircleV(p, (1.0f + pt) * 1.0f * s,
                            Color{255, 255, 255, al});
            }
        } else if (bracing) {
            // BRACE FOR IMPACT (Chad: "getting ready to accept his fate" —
            // played half-comic): shades gone (sunglasses would read wrong
            // here), eyes CLENCHED SHUT (two diagonal line pairs per eye, an
            // X), mouth a tiny grim flat line, extra sweat (forced to the
            // max drop count). At brace01 > 0.9 the fate is ACCEPTED: eyes
            // open back to a calm level line and a tiny halo arc appears
            // above the head — the resigned-but-at-peace beat.
            if (brace01 > 0.9f) {
                DrawLineEx(Vector2{eye_l.x - 2.6f * s, eye_l.y},
                           Vector2{eye_l.x + 2.6f * s, eye_l.y}, 1.4f * s,
                           BLACK);
                DrawLineEx(Vector2{eye_r.x - 2.6f * s, eye_r.y},
                           Vector2{eye_r.x + 2.6f * s, eye_r.y}, 1.4f * s,
                           BLACK);
                // Tiny halo arc, floating just above the crown.
                DrawRing(Vector2{head_c.x, head_c.y - head_r * 1.9f},
                         head_r * 0.85f, head_r * 0.95f, 200.0f, 340.0f, 12,
                         Color{255, 235, 140, 200});
            } else {
                // Clenched eyes: an X of two short diagonal strokes, each.
                const float k = 2.6f * s;
                DrawLineEx(Vector2{eye_l.x - k, eye_l.y - k},
                           Vector2{eye_l.x + k, eye_l.y + k}, 1.4f * s, BLACK);
                DrawLineEx(Vector2{eye_l.x - k, eye_l.y + k},
                           Vector2{eye_l.x + k, eye_l.y - k}, 1.4f * s, BLACK);
                DrawLineEx(Vector2{eye_r.x - k, eye_r.y - k},
                           Vector2{eye_r.x + k, eye_r.y + k}, 1.4f * s, BLACK);
                DrawLineEx(Vector2{eye_r.x - k, eye_r.y + k},
                           Vector2{eye_r.x + k, eye_r.y - k}, 1.4f * s, BLACK);
            }
            // Grim tiny mouth line (both sub-tiers — the resigned set jaw).
            DrawLineEx(Vector2{head_c.x - 2.2f * s, head_c.y + 5.5f * s},
                       Vector2{head_c.x + 2.2f * s, head_c.y + 5.5f * s},
                       1.3f * s, BLACK);
            draw_ghost_sweat(head_c, head_r, 0.8f, now_s, s);  // extra sweat
        } else if (fear01 < 0.3f) {
            // COOL: sunglasses on, level-ish — a touch of apprehension from
            // fear01 >= 0.15 (Chad: "also relief at safety" implies he
            // should read as ON WATCH even here, not oblivious): the
            // glasses TILT slightly and one small sweat drop shows.
            float tilt = 0.0f;
            if (fear01 >= kSweatStartFear) {
                tilt = (fear01 - kSweatStartFear) / (0.3f - kSweatStartFear) *
                       7.0f;
            }
            const Rectangle band{head_c.x, head_c.y - 2.6f * s, 13.0f * s,
                                 3.4f * s};
            const Vector2 origin{6.5f * s, 1.7f * s};
            DrawRectanglePro(band, origin, tilt, BLACK);
            circle_outlined(eye_l, 2.6f * s, BLACK, 1.0f * s);
            circle_outlined(eye_r, 2.6f * s, BLACK, 1.0f * s);
            DrawCircleV(Vector2{eye_l.x - 0.8f * s, eye_l.y - 0.8f * s},
                        0.7f * s, kWhite);
            DrawCircleV(Vector2{eye_r.x - 0.8f * s, eye_r.y - 0.8f * s},
                        0.7f * s, kWhite);
            draw_ghost_sweat(head_c, head_r, fear01, now_s, s);
        } else {
            // Wide white eyes, pupils biased LOW (looking DOWN at what
            // scares him — the dive/ground) + worried eyebrow arcs.
            const float eye_r_px = (fear01 < 0.7f ? 3.0f : 4.0f) * s;
            const float look_down = eye_r_px * 0.35f;
            DrawCircleV(eye_l, eye_r_px, kWhite);
            DrawCircleV(eye_r, eye_r_px, kWhite);
            DrawCircleV(Vector2{eye_l.x, eye_l.y + look_down}, eye_r_px * 0.5f,
                        BLACK);
            DrawCircleV(Vector2{eye_r.x, eye_r.y + look_down}, eye_r_px * 0.5f,
                        BLACK);

            DrawLineEx(
                Vector2{eye_l.x - 3.2f * s, eye_l.y - eye_r_px - 1.4f * s},
                Vector2{eye_l.x + 1.4f * s, eye_l.y - eye_r_px - 3.0f * s},
                1.3f * s, BLACK);
            DrawLineEx(
                Vector2{eye_r.x + 3.2f * s, eye_r.y - eye_r_px - 1.4f * s},
                Vector2{eye_r.x - 1.4f * s, eye_r.y - eye_r_px - 3.0f * s},
                1.3f * s, BLACK);

            if (fear01 < 0.7f) {
                // NERVOUS: the shades SLIP down off the eyes.
                Rectangle band{head_c.x, head_c.y + 2.0f * s, 13.0f * s,
                               3.2f * s};
                Vector2 origin{6.5f * s, 1.6f * s};
                DrawRectanglePro(band, origin, 12.0f, BLACK);
            } else {
                // SCARED: shades FLUNG — a tiny pair knocked off, up and
                // away.
                Rectangle band{head_c.x - 10.0f * s, head_c.y - 16.0f * s,
                               9.0f * s, 2.4f * s};
                Vector2 origin{4.5f * s, 1.2f * s};
                DrawRectanglePro(band, origin, -35.0f, BLACK);
            }

            // Mouth: a small black O, big O when scared.
            const float mouth_r = (fear01 < 0.7f ? 2.0f : 3.4f) * s;
            DrawCircleV(Vector2{head_c.x, head_c.y + 5.5f * s}, mouth_r, BLACK);
            draw_ghost_sweat(head_c, head_r, fear01, now_s, s);
        }
    }

    // Pointer tick — identical in role to the mouse's (direction legible
    // without rotating the art). Uses the ORIGINAL anchor `a` and `scale`
    // (not the panic/relief-perturbed `s`) so it stays a stable, legible
    // direction cue through every state.
    const Color kTickRim{57, 255, 60, 200};
    const float dlen =
        std::sqrt(dir_unit.x * dir_unit.x + dir_unit.y * dir_unit.y);
    if (dlen > 1e-6f) {
        const Vector2 ud{dir_unit.x / dlen, dir_unit.y / dlen};
        const Vector2 p0{a.x + ud.x * 18.0f * scale,
                         a.y + ud.y * 18.0f * scale};
        const Vector2 p1{a.x + ud.x * 27.0f * scale,
                         a.y + ud.y * 27.0f * scale};
        DrawLineEx(p0, p1, 2.2f * scale, kTickRim);
    }
}

// The skin-dispatching entry point every call site uses. `kAimBuddySkin`
// picks the shipped skin; every branch stays compilable so a future
// economy-item picker can switch on a player selection instead.
void draw_aim_buddy(Vector2 anchor, Vector2 dir_unit, float fear01,
                    float brace01, double now_s, float scale = 1.0f) {
    switch (kAimBuddySkin) {
        case BuddySkin::kMouse:
            // The retired mouse skin never grew a brace pose (only kGhost is
            // shipped) — folded into its existing fear read via max() so it
            // at least reads SOMETHING scarier during a deck rush, rather
            // than silently dropping the signal.
            draw_aim_mouse(anchor, dir_unit, std::max(fear01, brace01), now_s,
                           scale);
            return;
        case BuddySkin::kGhost:
            draw_aim_ghost(anchor, dir_unit, fear01, brace01, now_s, scale);
            return;
    }
}

}  // namespace aim_buddy

}  // namespace

void set_planet_build_params(const PlanetBuildParams& p) { g_planet_cfg = p; }

const world::HeightField* planet_heightfield(
    const sim::AircraftParams& params) {
    const Planet& p = ensure_planet(params);
    return (p.ok && !p.height.px.empty()) ? &p.height : nullptr;
}
const world::Raster8* planet_landmask(const sim::AircraftParams& params) {
    const Planet& p = ensure_planet(params);
    return p.landmask.empty() ? nullptr : &p.landmask;
}
const world::Raster8* planet_barren(const sim::AircraftParams& params) {
    const Planet& p = ensure_planet(params);
    return p.barren.empty() ? nullptr : &p.barren;
}
const world::LineNetwork* planet_linework(const sim::AircraftParams& params) {
    const Planet& p = ensure_planet(params);
    if (!p.ok) return nullptr;
    const world::LineNetwork& net =
        sudbury_linework(params.R, g_planet_cfg.u_offset);
    return net.empty() ? nullptr : &net;
}
void set_tree_build_params(const TreeBuildParams& p) { g_tree_cfg = p; }
// FLAK gun-pad clearings (Chad 2026-08-29: the relocated hilltop guns stood
// INSIDE the boreal scatter -- the sight view was a wall of conifers). These
// are TREE-ONLY cuts: they must NOT join planet_cuts, which also punches
// terrain holes (the portal surgery list). Appended AFTER the flak site
// search runs (it needs the heightfield, which is built after the tree
// config is set); legal because the scatter builds lazily on the first
// frame -- the set_tree_snowhill late-bind precedent.
void add_tree_cuts(const std::vector<CutDisk>& extra) {
    g_tree_cfg.cuts.insert(g_tree_cfg.cuts.end(), extra.begin(),
                           extra.end());
}
void set_tree_snowhill(const world::SnowhillParams& hill,
                       const world::HeightField* hf) {
    g_tree_cfg.snowhill = hill;
    g_tree_cfg.snowhill_hf = hf;
}
void set_bank_build_params(const BankBuildCfg& c) { g_bank_cfg = c; }
void set_bank_snow_sources(const world::SnowpackField* snow) {
    g_bank_snow = snow;
}

// ★ SF3-A: the rider snow patch reads the same field. Defined HERE, at true
// render:: scope beside its sibling -- the patch state above lives in this
// file's anonymous namespace, which a later function in the same TU can
// still reach, but the SETTER must have external linkage for app/ to call.
void set_snow_patch_sources(const world::SnowpackField* field, int subdiv,
                            int tiles) {
    g_patch_field = field;
    g_patch_subdiv = subdiv;
    g_patch_tiles = tiles;
}
void set_ribbon_build_params(const RibbonBuildParams& p) { g_ribbon_cfg = p; }
void set_building_build_params(const BuildingBuildParams& p) {
    g_building_cfg = p;
}
void set_lamp_build_params(const LampBuildParams& p) { g_lamp_cfg = p; }
void set_precip_build_params(const PrecipBuildParams& p) { g_precip_cfg = p; }
void set_smoke_build_params(const SmokeBuildParams& p) { g_smoke_cfg = p; }
void set_train_build_params(const TrainBuildParams& p) { g_train_cfg = p; }
void set_slag_build_params(const SlagBuildParams& p) { g_slag_cfg = p; }

void init_draw() {
    // 15 km planet: raylib's default far plane (1000 m) would cull the
    // world. Horizon slant range tops out ~sqrt(h(2R+h)); 60 km covers any
    // survivable altitude. Near at 2 m — depth precision scales linearly
    // with it, and nothing renders closer than the chase offset anyway; at
    // 0.5 m the far half of the sphere z-fought its own wire overlay.
    rlSetClipPlanes(2.0, 60000.0);
}

// Cosmetic viewport frame (the "custom frame for the graphics"): a thin HUD
// bezel with corner brackets and a title strip. Pure 2D overlay — reads state
// only through the info/readout already computed. Kept faint so it never
// competes with the reticle/nose-marker pair.
void draw_bezel(int sw, int sh, Season season, bool escape_sky,
                bool bubble_live) {
    const Color edge = {120, 200, 255, 70};      // faint cyan frame
    const Color bracket = {150, 220, 255, 190};  // brighter corner accents
    constexpr int m = 16;                        // inset margin
    constexpr int L = 30;                        // corner bracket length
    constexpr float t = 2.0f;                    // bracket thickness

    DrawRectangleLines(m, m, sw - 2 * m, sh - 2 * m, edge);

    const float x0 = static_cast<float>(m), y0 = static_cast<float>(m);
    const float x1 = static_cast<float>(sw - m),
                y1 = static_cast<float>(sh - m);
    // Four L-shaped corner brackets.
    DrawLineEx({x0, y0}, {x0 + L, y0}, t, bracket);
    DrawLineEx({x0, y0}, {x0, y0 + L}, t, bracket);
    DrawLineEx({x1, y0}, {x1 - L, y0}, t, bracket);
    DrawLineEx({x1, y0}, {x1, y0 + L}, t, bracket);
    DrawLineEx({x0, y1}, {x0 + L, y1}, t, bracket);
    DrawLineEx({x0, y1}, {x0, y1 - L}, t, bracket);
    DrawLineEx({x1, y1}, {x1 - L, y1}, t, bracket);
    DrawLineEx({x1, y1}, {x1, y1 - L}, t, bracket);

    // Title along the bottom edge (clear of the top-left flight HUD). The
    // WEATHER SEASON tag (W1) rides here uppercased — a draw-only read of the
    // app-chosen season so Chad can confirm the random draw / static override.
    std::string title = "SEADS  ·  GREATER SUDBURY  ·  R=15 km  ·  ";
    for (const char* c = season_name(season); *c; ++c)
        title +=
            static_cast<char>(std::toupper(static_cast<unsigned char>(*c)));
    // R5b: the GravityField state rides the same strip (Chad's ask — "was
    // the escape on?" must read on sight, exactly like the season tag).
    if (escape_sky) title += "  ·  ESCAPE SKY";
    // R6: the AtmosphereField state rides the same strip (same discipline —
    // "is the bubble on?" reads on sight). The air-density PLATE below the
    // flaps gauge is the read-on-sight cue a pilot must act on; this is the
    // at-a-glance "the field is live at all" confirm.
    if (bubble_live) title += "  ·  BUBBLE";
    DrawText(title.c_str(), m + 10, sh - m - 18, 14, Color{170, 210, 245, 190});
}

// MB HUD attitude dial (evolves the MB-7c AoA dial, Chad 2026-07-08): the
// needle now reads PITCH ATTITUDE — the nose's angle to the local horizon, a
// read that HOLDS its value (the AoA delta zeroed out in steady flight) —
// and an outer ROLL ARC carries a full-range bank pointer (the ADI-style
// bank indicator). Stall awareness moves to a slim AoA strip on the gauge's
// LEFT (inside the roll ring it would collide with the bank pointer exactly
// at knife-edge reads). Reads ONLY the shared flight_readout + config limits
// on FrameInfo (the flat-instrument rule); hidden until aoa_max is set.
void draw_attitude_dial(const FlightReadout& r, const FrameInfo& info, int sw,
                        int sh) {
    if (info.aoa_max <= 0.0) return;
    const float cx = static_cast<float>(sw) - 150.0f;
    const float cy = 0.5f * static_cast<float>(sh);
    const float R = 108.0f;      // big, per the MB-7c ruling
    const float Rb = R + 22.0f;  // the roll-arc ring

    // Pitch needle geometry: fixed +/-90 deg span mapped onto the +/-70 deg
    // screen sweep; phi = needle angle from horizontal-left, + = up.
    constexpr double kSweep = 70.0 * PI / 180.0;
    constexpr double kHalfPi = 0.5 * PI;
    const auto phi_of = [&](double pitch) {
        const double t = std::clamp((pitch + kHalfPi) / PI, 0.0, 1.0);
        return -kSweep + 2.0 * kSweep * t;
    };
    const auto pt = [&](double phi, float rad) {
        return Vector2{cx - rad * static_cast<float>(std::cos(phi)),
                       cy - rad * static_cast<float>(std::sin(phi))};
    };

    // Reference ticks every 30 deg of pitch; the horizon (0) drawn heavier —
    // it is THE reference the read holds against.
    for (const double d : {-90.0, -60.0, -30.0, 0.0, 30.0, 60.0, 90.0}) {
        const double p = phi_of(d * PI / 180.0);
        const bool horizon = d == 0.0;
        DrawLineEx(
            pt(p, R - (horizon ? 16.0f : 10.0f)), pt(p, R + 6.0f),
            horizon ? 3.0f : 2.0f,
            horizon ? Color{225, 240, 255, 220} : Color{200, 225, 245, 150});
    }

    // The pitch needle — the one big moving element (white; the stall
    // coloring lives on the AoA strip now).
    const double np = phi_of(r.pitch_attitude);
    DrawLineEx(pt(np, 18.0f), pt(np, R - 14.0f), 4.0f, RAYWHITE);
    DrawCircleV(Vector2{cx, cy}, 5.0f, Color{200, 225, 245, 200});

    // Roll arc: bank angle psi measured from 12 o'clock, + = right wing down
    // = clockwise on screen (turn-coordinator convention — the pointer moves
    // WITH the bank; flight-log question if Chad reads it backwards).
    // pointer = r.bank_full, the FULL +/-180 read — NEVER the folded r.bank
    // (bank 100 would read 80). Past +/-90 the pointer swings below the
    // horizontal and inverted sits at 6 o'clock: real attitudes, by design.
    const auto ptb = [&](double psi, float rad) {
        return Vector2{cx + rad * static_cast<float>(std::sin(psi)),
                       cy - rad * static_cast<float>(std::cos(psi))};
    };
    // Faint ring context across the upright half, ticks at 0/30/60/90.
    {
        const double p0 = -kHalfPi, p1 = kHalfPi;
        const int n = 48;
        for (int i = 0; i < n; ++i) {
            const double u0 = p0 + (p1 - p0) * (static_cast<double>(i) / n);
            const double u1 = p0 + (p1 - p0) * (static_cast<double>(i + 1) / n);
            DrawLineEx(ptb(u0, Rb), ptb(u1, Rb), 2.0f,
                       Color{120, 200, 255, 60});
        }
    }
    for (const double d : {-90.0, -60.0, -30.0, 0.0, 30.0, 60.0, 90.0}) {
        const double psi = d * PI / 180.0;
        const bool top = d == 0.0;
        DrawLineEx(ptb(psi, Rb - 6.0f), ptb(psi, Rb + 6.0f), top ? 3.0f : 2.0f,
                   top ? Color{225, 240, 255, 220} : Color{200, 225, 245, 150});
    }
    // The bank pointer: a bold tick riding the ring at bank_full.
    DrawLineEx(ptb(r.bank_full, Rb - 12.0f), ptb(r.bank_full, Rb + 6.0f), 4.0f,
               Color{140, 235, 160, 255});

    // Labels + live numerics under the pivot: pitch big, bank beneath it.
    constexpr double kRadToDeg = 57.2957795130823;
    char t[24];
    std::snprintf(t, sizeof t, "%+3.0f", r.pitch_attitude * kRadToDeg);
    DrawText("PITCH", static_cast<int>(cx) - 26, static_cast<int>(cy) + 16, 18,
             Color{170, 210, 245, 200});
    DrawText(t, static_cast<int>(cx) - 20, static_cast<int>(cy) + 36, 20,
             RAYWHITE);
    std::snprintf(t, sizeof t, "BANK %+4.0f", r.bank_full * kRadToDeg);
    DrawText(t, static_cast<int>(cx) - 44, static_cast<int>(cy) + 60, 16,
             Color{140, 235, 160, 220});

    // Slim AoA stall strip on the LEFT of the gauge: the old dial's exact
    // display span and zone colors, vertical (up = more AoA). The dial, the
    // vortices, and the protection clamp all stay keyed to the same aoa_max.
    const double disp_max = std::max(info.stall_alpha, info.aoa_max) * 1.15;
    const double disp_min = -info.aoa_max_neg * 1.30;
    const float xs = cx - Rb - 24.0f;  // clear of the roll ring
    const float half_h = 100.0f;
    const auto y_of = [&](double a) {
        const double u =
            std::clamp((a - disp_min) / (disp_max - disp_min), 0.0, 1.0);
        return cy + half_h - 2.0f * half_h * static_cast<float>(u);
    };
    const auto seg = [&](double a0, double a1, Color col) {
        const float y1 = y_of(a0), y0 = y_of(a1);  // y grows down
        DrawRectangleRec(Rectangle{xs, y0, 6.0f, y1 - y0}, col);
    };
    const double amber_from = kVortexAoAFrac * info.aoa_max;
    seg(disp_min, amber_from, Color{110, 220, 140, 130});
    seg(amber_from, info.aoa_max, Color{240, 190, 70, 180});
    seg(info.aoa_max, disp_max, Color{235, 80, 60, 200});
    // Current-AoA caret (the old needle's warm-up color logic).
    Color ccol = RAYWHITE;
    if (std::abs(r.aoa) >= info.aoa_max)
        ccol = Color{245, 90, 70, 255};
    else if (r.aoa >= amber_from)
        ccol = Color{245, 200, 90, 255};
    const float yc = y_of(r.aoa);
    DrawTriangle(Vector2{xs - 9.0f, yc - 5.0f}, Vector2{xs - 9.0f, yc + 5.0f},
                 Vector2{xs - 1.0f, yc}, ccol);
    DrawText("AoA", static_cast<int>(xs) - 12,
             static_cast<int>(cy + half_h) + 8, 14, Color{170, 210, 245, 180});
}

// MB-7c (iv): the energy cluster — big alt/speed/G with a speed-TREND cue
// (chevrons: green climbing the energy hill, red bleeding). Bottom-left, top
// of the status stack (flaps/gear + telemetry sit below it, bezel title last).
void draw_energy_cluster(const FlightReadout& r, const FrameInfo& info,
                         int sh) {
    const int x = 34;
    const int y = sh - 232;
    char t[48];
    std::snprintf(t, sizeof t, "%3.0f", r.speed);
    DrawText("SPD", x, y, 18, Color{170, 210, 245, 200});
    DrawText(t, x + 52, y - 10, 38, RAYWHITE);
    DrawText("m/s", x + 130, y + 8, 16, Color{170, 210, 245, 170});
    // Trend chevrons: 1..3 by |dV/dt|, up = gaining. Deadband 0.4 m/s^2 so
    // level cruise shows calm.
    const double tr = info.speed_trend;
    const int n = tr > 0.4    ? std::min(3, 1 + static_cast<int>(tr / 2.0))
                  : tr < -0.4 ? -std::min(3, 1 + static_cast<int>(-tr / 2.0))
                              : 0;
    for (int i = 0; i < std::abs(n); ++i) {
        const float bx = static_cast<float>(x + 180);
        const float by = static_cast<float>(y + 12 - 8 * i);
        // Up-chevron (gaining): base corners LOW (+5, screen y down), apex
        // HIGH (-5). s = +1 gaining / -1 bleeding flips it.
        const float s = n > 0 ? 1.0f : -1.0f;
        const Color c =
            n > 0 ? Color{110, 230, 140, 220} : Color{240, 110, 90, 220};
        DrawLineEx({bx, by + 5 * s}, {bx + 7, by - 5 * s}, 3.0f, c);
        DrawLineEx({bx + 7, by - 5 * s}, {bx + 14, by + 5 * s}, 3.0f, c);
    }
    std::snprintf(t, sizeof t, "%5.0f", r.altitude);
    DrawText("ALT", x, y + 46, 18, Color{170, 210, 245, 200});
    DrawText(t, x + 52, y + 38, 30, Color{225, 235, 250, 235});
    DrawText("m", x + 148, y + 50, 16, Color{170, 210, 245, 170});
    std::snprintf(t, sizeof t, "%+4.1f", r.load_factor);
    DrawText("G", x, y + 86, 18, Color{170, 210, 245, 200});
    DrawText(t, x + 52, y + 78, 30,
             std::abs(r.load_factor) > 7.0 ? Color{245, 200, 90, 240}
                                           : Color{225, 235, 250, 235});
}

// MB HUD status stack (Chad 2026-07-08: "I cannot even read the HUD at the
// top of the screen"): the flight telemetry line + a labeled FLAPS/GEAR
// block, bottom-left above the bezel title, under the energy cluster. Labels
// read the COMMANDED detents (FrameInfo, the same latch the sim command
// reads); deploy bars read the slewed plant positions and show only while
// the device is in motion toward its target.
void draw_status_stack(const FlightReadout& r, const sim::SimState& state,
                       const FrameInfo& info, int sh) {
    constexpr double kRadToDeg = 57.2957795130823;
    const int x = 34;
    char line[160];

    // Deploy bar: outline + fill fraction, drawn only mid-slew.
    const auto bar = [&](int y, double pos, double target) {
        if (std::abs(pos - target) <= 0.01) return;
        const float bx = static_cast<float>(x + 150);
        const float by = static_cast<float>(y + 5);
        DrawRectangleLines(static_cast<int>(bx), static_cast<int>(by), 80, 8,
                           Color{170, 210, 245, 200});
        DrawRectangleRec(
            Rectangle{bx + 1.0f, by + 1.0f,
                      78.0f * static_cast<float>(std::clamp(pos, 0.0, 1.0)),
                      6.0f},
            Color{240, 190, 70, 220});
    };

    // GEAR line (always shown). FLAPS moved to its own color-coded gauge low
    // and centered (draw_flaps_indicator) — Chad's "better flaps indicator"
    // ask.
    std::snprintf(line, sizeof line, "GEAR %s",
                  info.gear_down_cmd ? "DOWN" : "UP");
    DrawText(line, x, sh - 88, 18, Color{225, 235, 250, 235});
    bar(sh - 88, state.gear, info.gear_down_cmd ? 1.0 : 0.0);

    // The telemetry line (moved from the unreadable top strip). BANK prints
    // the FULL-RANGE read — the folded phi is the exact lie the roll arc
    // exists to fix.
    std::snprintf(line, sizeof line,
                  "%s  V %5.1f m/s  ALT %6.0f m  THR %3.0f%%  G %+4.1f  "
                  "AoA %+5.1f  BANK %+5.0f  %d fps",
                  info.raw_mode ? "RAW  " : "INSTR", r.speed, r.altitude,
                  state.throttle * 100.0, r.load_factor, r.aoa * kRadToDeg,
                  r.bank_full * kRadToDeg, info.fps);
    DrawText(line, x, sh - 60, 18, RAYWHITE);
}

// A color-coded flaps gauge, centered low on the screen (Chad's "better flaps
// indicator down low, color coded"). ALWAYS visible (the old status-stack bar
// only appeared mid-slew). The fill LENGTH is the live slewed deflection
// (state.flap, 0..1); the color is the COMMANDED detent — green CLEAN / amber
// COMBAT / red LANDING — so a glance reads both "what I selected" and "how far
// it's actually deployed". Tick marks mark the three detent positions (CLEAN 0,
// COMBAT params.flap_combat, LANDING 1). Read-only, like the rest of the HUD.
void draw_flaps_indicator(const sim::SimState& state,
                          const sim::AircraftParams& params,
                          const FrameInfo& info, int sw, int sh) {
    const int w = 220, h = 12;
    const int x = (sw - w) / 2;  // centered horizontally
    const int y = sh - 92;       // low, above the bezel title (~sh-36)
    const float fx = static_cast<float>(x), fy = static_cast<float>(y);
    const double combat =
        std::clamp(static_cast<double>(params.flap_combat), 0.0, 1.0);

    // Detent color: the traffic-light cue. Green = clean/fast, amber = combat
    // (turn-fight), red = full landing flap (slow, high drag — a caution).
    const Color green{120, 225, 140, 255};
    const Color amber{245, 195, 75, 255};
    const Color red{240, 95, 70, 255};
    const Color detent = info.flap_mode <= 0   ? green
                         : info.flap_mode == 1 ? amber
                                               : red;

    const int fs = 18;

    // ★★ R1c MODE LEAK (Chad, drive report 2026-08-17: "there is HUD bar for
    // flaps right bottom centre occluding my view of the feet"). The flaps
    // gauge sits at sh-92, which in drive mode is directly over the running
    // boards and the rider's boots -- and it is an AIRCRAFT instrument: a
    // snowmachine has no flaps. The sled dash below was already written as "a
    // SUBTRACTION from the aircraft HUD"; this is the half of that subtraction
    // that was never wired. Gated on `info.sled_active`, which app/main.cpp
    // sets to exactly `drive_mode && sled_seeded` -- the same predicate the
    // dash itself uses, so the two can never disagree.
    //
    // SCOPE, deliberately narrow: this hides aircraft-only READOUTS in drive
    // mode. It is not the mode manager (§6/R5) and it changes no state, no
    // input routing and no plant.
    if (!info.sled_active) {
        // Backing plate + track outline.
        DrawRectangleRec(Rectangle{fx - 3, fy - 3, w + 6.0f, h + 6.0f},
                         Color{15, 22, 30, 175});
        DrawRectangleLines(x - 3, y - 3, w + 6, h + 6,
                           Color{150, 220, 255, 150});

        // Live deflection fill (dim track behind it so 0% still reads as a
        // gauge).
        DrawRectangleRec(
            Rectangle{fx, fy, static_cast<float>(w), static_cast<float>(h)},
            Color{40, 55, 70, 160});
        const float frac = static_cast<float>(std::clamp(state.flap, 0.0, 1.0));
        DrawRectangleRec(Rectangle{fx, fy, w * frac, static_cast<float>(h)},
                         detent);

        // Detent tick marks (CLEAN / COMBAT / LANDING), each in its zone color.
        const auto tick = [&](double f, Color c) {
            const int tx = x + static_cast<int>(std::lround(f * w));
            DrawLineEx(Vector2{static_cast<float>(tx), fy - 4},
                       Vector2{static_cast<float>(tx), fy + h + 4}, 2.0f, c);
        };
        tick(0.0, green);
        tick(combat, amber);
        tick(1.0, red);

        // Label above the gauge: "FLAPS  <DETENT>  NN%", tinted to the detent
        // color, centered over the track.
        char line[96];
        std::snprintf(line, sizeof line, "FLAPS  %s  %d%%",
                      flap_mode_label(info.flap_mode),
                      static_cast<int>(std::lround(
                          std::clamp(state.flap, 0.0, 1.0) * 100.0)));
        const int tw = MeasureText(line, fs);
        DrawText(line, (sw - tw) / 2, y - 26, fs, detent);
    }

    // ★ WINTER S2 SURFACE PLATE (WINTER_LAW §2.3). Sits above the flaps gauge,
    // in the R5c "world-state a pilot must ACT on belongs in a plate" slot.
    // Reads the app-computed world::SnowpackField sample under the aircraft's
    // ground track: the surface CLASS and the snow DEPTH there. This is the
    // rung's fly instrument -- the field is invisible until the sled exists,
    // so the only way to judge f() before S3 is to read it while flying over
    // known ground (a lake, a plowed road, a groomed trail, a bare crest).
    if (info.surface_label != nullptr) {
        char sline[128];
        if (info.surface_corridor_m >= 0.0 &&
            info.surface_near_label != nullptr)
            // ★ The corridor's OWN depth, in brackets, so it can be read
            // WITHOUT flying down a 5 m ribbon at 150 m/s (Chad's fly-1 F3).
            std::snprintf(
                sline, sizeof sline, "%s   SNOW %.2f m   %s %.0f m (%.2f m)",
                info.surface_label, info.snow_depth_m, info.surface_near_label,
                info.surface_corridor_m, info.surface_corridor_depth_m);
        else
            std::snprintf(sline, sizeof sline, "%s   SNOW %.2f m",
                          info.surface_label, info.snow_depth_m);
        // Colour by DEPTH, not by class: how buried you are is the read that
        // matters, and it is legible at a glance without learning a legend.
        const Color scol =
            info.snow_depth_m < 0.15
                ? Color{240, 190, 70, 235}  // bare / hard pack
                : (info.snow_depth_m > 1.5 ? Color{120, 170, 245, 235}  // deep
                                           : Color{225, 235, 250, 225});
        const int stw = MeasureText(sline, fs);
        DrawText(sline, (sw - stw) / 2, y - 48, fs, scol);
    }

    // ★★ WINTER S3 THE SLED DASH (§3). Deliberately a SUBTRACTION from the
    // aircraft HUD: no airspeed, no flaps, no altitude -- those instruments are
    // meaningless on a snowmachine and printing them would make the mode switch
    // read as a camera change rather than as a different machine. What a rider
    // needs is ground speed, what is under the skis, and how the machine is
    // carrying itself.
    if (info.sled_note != nullptr) {
        const int ntw = MeasureText(info.sled_note, fs + 2);
        const int nx = (sw - ntw) / 2, ny = sh / 2 + 60;
        DrawRectangleRec(
            Rectangle{static_cast<float>(nx - 10), static_cast<float>(ny - 6),
                      static_cast<float>(ntw + 20),
                      static_cast<float>(fs + 14)},
            Color{12, 16, 26, 190});
        DrawText(info.sled_note, nx, ny, fs + 2, Color{240, 200, 110, 240});
    }
    // ★ L1: the interact prompt, one line under the note slot so the two never
    // overwrite each other (a refusal and an offer are different sentences and
    // both are worth reading).
    if (info.interact_prompt != nullptr) {
        const int pw = MeasureText(info.interact_prompt, fs + 2);
        // ★ sting lane, fly-7 (Chad): "push aside the hud for press o to
        // mount or dismount for flak gun" — the GUN prompts ("O  MAN THE
        // GUN" / "O  LEAVE THE GUN") sat centred right on the gun and the
        // man; they now live at the left edge, the sting gauge panel's own
        // column. Every OTHER site prompt (pump fix, mounts) stays centred
        // exactly as the loop lane shipped it.
        const bool gun_prompt = info.interact_prompt[0] == 'O' &&
                                info.interact_prompt[1] == ' ';
        const int px = gun_prompt ? 24 : (sw - pw) / 2;
        const int py = sh / 2 + 60 + (fs + 20);
        DrawRectangleRec(
            Rectangle{static_cast<float>(px - 10), static_cast<float>(py - 6),
                      static_cast<float>(pw + 20),
                      static_cast<float>(fs + 14)},
            Color{12, 16, 26, 190});
        DrawText(info.interact_prompt, px, py, fs + 2,
                 Color{170, 230, 200, 245});
    }
    // ★ L2: THE WRENCH IS TURNING. One line under the prompt, in the same
    // stack, so "U STOP FIXING" and "FIXING PUMP 37%" read as one thing and not
    // two competing captions. Negative frac = not repairing = nothing drawn.
    if (info.repair_frac >= 0.0) {
        const double f =
            info.repair_frac > 1.0 ? 1.0 : info.repair_frac;
        char rb[48];
        // ★ L10: the noun comes from the app, which is the only thing that
        // knows which job this is. A null pointer draws the pump caption
        // rather than a crash -- the field is defaulted, so null means an
        // app that predates the field.
        std::snprintf(rb, sizeof(rb), "FIXING %s  %d%%",
                      info.repair_what != nullptr ? info.repair_what : "PUMP",
                      static_cast<int>(f * 100.0));
        const int rw = MeasureText(rb, fs + 2);
        const int barw = 220;
        const int boxw = (rw > barw ? rw : barw);
        const int rx = (sw - boxw) / 2;
        const int ry = sh / 2 + 60 + 2 * (fs + 20);
        DrawRectangleRec(
            Rectangle{static_cast<float>(rx - 10), static_cast<float>(ry - 6),
                      static_cast<float>(boxw + 20),
                      static_cast<float>(fs + 26)},
            Color{12, 16, 26, 190});
        DrawText(rb, rx + (boxw - rw) / 2, ry, fs + 2,
                 Color{240, 210, 130, 245});
        // The bar itself: an empty track and the filled span, so a fix that has
        // barely started still SHOWS that it started.
        const int by = ry + fs + 6;
        DrawRectangleRec(Rectangle{static_cast<float>(rx),
                                   static_cast<float>(by),
                                   static_cast<float>(boxw), 6.0f},
                         Color{50, 60, 80, 220});
        DrawRectangleRec(
            Rectangle{static_cast<float>(rx), static_cast<float>(by),
                      static_cast<float>(boxw * f), 6.0f},
            Color{240, 210, 130, 245});
    }
    if (info.sled_active) {
        const int sx = 24;
        const int sy0 = sh / 2 - 60;
        int sy = sy0;
        const Color ink{225, 235, 250, 230};
        // ★ SC1 (WINTER_LAW §3.7, spec §4): one extra reserved line-slot (the
        // SOFT SNOW note, a FIXED dash position -- unlike sled_note, never a
        // wall-clock transient) grows the plate.
        const float plate_h = 132.0f + (fs + 8);
        DrawRectangleRec(
            Rectangle{static_cast<float>(sx - 10), static_cast<float>(sy - 10),
                      250.0f, plate_h},
            Color{12, 16, 26, 170});
        char l[128];
        std::snprintf(l, sizeof l, "%3.0f km/h", info.sled_speed_ms * 3.6);
        DrawText(l, sx, sy, fs + 8, ink);
        sy += fs + 14;
        // ★ THE DASH THERMOMETER (spec §4). Hidden whole when [cold]
        // enabled=false -- P2-4: an instrument must not read a confident
        // constant.
        if (info.sled_cold_valid) {
            std::snprintf(l, sizeof l, "%s   %.2f m   %.0f C",
                          info.sled_surface ? info.sled_surface : "?",
                          info.sled_depth_m, info.sled_air_temp_c);
        } else {
            std::snprintf(l, sizeof l, "%s   %.2f m",
                          info.sled_surface ? info.sled_surface : "?",
                          info.sled_depth_m);
        }
        DrawText(l, sx, sy, fs, ink);
        sy += fs + 8;
        // SOFT SNOW heads-up: warm daytime, flaps-amber, a FIXED slot (the
        // layout below never shifts whether it is lit or not).
        if (info.sled_soft_note) {
            DrawText("SOFT SNOW", sx, sy, fs, Color{245, 195, 75, 220});
        }
        sy += fs + 8;
        // ★ PLANE is a BAR, not a light. "Still plowing some" (§2.2a) is a
        // ruling about a continuous quantity, and a lamp that switches on at
        // some threshold would satisfy the word "planing" while destroying the
        // only thing the rider is actually being asked to feel.
        DrawText("PLANE", sx, sy, fs, ink);
        const int bx0 = sx + 62, bw = 150;
        DrawRectangleLines(bx0, sy, bw, fs, Color{90, 105, 130, 200});
        const int fill = static_cast<int>(
            std::clamp(info.sled_plane_frac, 0.0, 1.0) * (bw - 2));
        DrawRectangle(bx0 + 1, sy + 1, fill, fs - 2, Color{120, 200, 255, 210});
        sy += fs + 8;
        // ★ THE ROOST PRODUCT (§3.5a), shown because it IS the thrust budget:
        // it roars while you are buying thrust and dies when there is nothing
        // left to throw. At S5 the bar and the spray are the same number.
        DrawText("ROOST", sx, sy, fs, ink);
        DrawRectangleLines(bx0, sy, bw, fs, Color{90, 105, 130, 200});
        const int rfill = static_cast<int>(
            std::clamp(info.sled_roost_flux, 0.0, 1.0) * (bw - 2));
        DrawRectangle(bx0 + 1, sy + 1, rfill, fs - 2,
                      Color{245, 215, 130, 210});
        sy += fs + 8;
        // Per-ski suspension travel, LEFT and RIGHT separately -- the readout
        // of §2.4c.1's ruling. When one ski is on a bank and the other is not,
        // these bars must disagree; that disagreement IS the tilt.
        std::snprintf(l, sizeof l, "SUSP  L %.03f  R %.03f  T %.03f",
                      info.sled_susp_x[0], info.sled_susp_x[1],
                      info.sled_susp_x[2]);
        DrawText(l, sx, sy, fs, ink);
        // ★ D3.ii THE WEIGHT DOT: the rider's mass displacement, the analog
        // input Chad could not see on drive 1. Reads sled_weight_x/y --
        // rider_lat/fwd NORMALIZED to the LIVE reach box (app computes it,
        // the box opens as you stand). +LEFT draws LEFT, +fwd draws UP; the
        // rim brightens green with stand_frac so seated vs stood is
        // readable at a glance. Mouse capture stays RELATIVE deltas per
        // §9d (the mousepad-runs-out ruling): this dot, not a cursor, is
        // the feedback.
        // *** SK-1c: the corner box is now the FALLBACK presentation. It is
        // drawn only when the x-ray grid is off (H toggles), so Chad can A/B
        // the two on one drive. Restyled slag orange to match the x-ray so
        // the comparison is presentation, not palette.
        if (!info.sled_hud_xray) {
            const int wb = 56;  // box edge [px]
            const int wx0 = sx + 252, wy0 = sy0 - 10;
            DrawRectangle(wx0, wy0, wb + 20, wb + fs + 18,
                          Color{12, 16, 26, 170});
            DrawText("WEIGHT", wx0 + 10, wy0 + 6, fs - 2, ink);
            const int bx = wx0 + 10, by = wy0 + fs + 10;
            const unsigned char sg =
                static_cast<unsigned char>(120 + 100 * info.sled_stand_frac);
            DrawRectangleLines(bx, by, wb, wb,
                               Color{90,
                                     static_cast<unsigned char>(
                                         105 + 60 * info.sled_stand_frac),
                                     130, sg});
            const int cx = bx + wb / 2, cy = by + wb / 2;
            DrawLine(bx + 2, cy, bx + wb - 2, cy, Color{90, 105, 130, 120});
            DrawLine(cx, by + 2, cx, by + wb - 2, Color{90, 105, 130, 120});
            const float half = (wb - 8) * 0.5f;
            const float dx = static_cast<float>(
                -std::clamp(info.sled_weight_x, -1.0, 1.0) * half);
            const float dy = static_cast<float>(
                -std::clamp(info.sled_weight_y, -1.0, 1.0) * half);
            DrawCircleV(Vector2{cx + dx, cy + dy}, 5.0f,
                        Color{245, 215, 130, 235});
        }
        // *** SK-1c THE BOTTOM BAR -- Chad's alternative presentation,
        // VERBATIM: "either across the bottom of the screen awesomeness or
        // through the sudburian xray style visibility". Same three reads as the
        // x-ray grid (weight, steering deflection, handlebar angle) in slag
        // orange, so the A/B is presentation only. Live when the x-ray is
        // toggled OFF (H).
        if (!info.sled_hud_xray) {
            const int SW = GetScreenWidth(), SH = GetScreenHeight();
            const int bw = SW * 3 / 5, bh = 54;
            const int bx0 = (SW - bw) / 2, by0 = SH - bh - 26;
            // Same theme table as the x-ray panel, so the A/B stays
            // presentation-only and neither view can fork the palette.
            const auto bcol = [](const glm::dvec3& c, unsigned char a) {
                const auto q = [](double v) {
                    return static_cast<unsigned char>(
                        std::lround(std::min(1.0, std::max(0.0, v)) * 255.0));
                };
                return Color{q(c.x), q(c.y), q(c.z), a};
            };
            const glm::dvec3 cb = render::team_colors().ally;   // grid blue
            const glm::dvec3 cs = render::team_colors().enemy;  // slag orange
            const Color slag = bcol(cb, 220);  // the GRID is blue here too
            DrawRectangle(bx0, by0, bw, bh, Color{10, 8, 6, 150});
            // the moving grid, sheared by weight -- the same idea as the back
            // panel, flattened: verticals lean with lateral weight.
            const double wx = std::clamp(info.sled_weight_x, -1.0, 1.0);
            const double wy = std::clamp(info.sled_weight_y, -1.0, 1.0);
            const int NV = 17;
            for (int i = 0; i <= NV; ++i) {
                const double u = -1.0 + 2.0 * i / NV;
                const double lean = -wx * 12.0 * (1.0 - 0.4 * std::fabs(u));
                const int xt =
                    bx0 + static_cast<int>((u * 0.5 + 0.5) * bw + lean);
                const int xb =
                    bx0 + static_cast<int>((u * 0.5 + 0.5) * bw - lean);
                DrawLine(xt, by0 + 4, xb, by0 + bh - 4,
                         bcol(cb, static_cast<unsigned char>(
                                      70 + 40 * (1.0 - std::fabs(u)))));
            }
            const int midy = by0 + bh / 2 + static_cast<int>(-wy * 10.0);
            DrawLine(bx0 + 4, midy, bx0 + bw - 4, midy, slag);
            // STEERING DEFLECTION / HANDLEBARS: REMOVED, same ruling as the
            // x-ray panel -- Chad, 2026-08-25: "the handlebars arent necessary
            // anymore." Both presentations now read WEIGHT TRANSFER only, which
            // keeps the H toggle an honest A/B of one instrument.
            const int ccx = bx0 + bw / 2, ccy = by0 + bh / 2;
            // ★ THE WEIGHT BALL -- slag orange, glowing, over the blue grid.
            // 2D has no additive pass here, so the bloom is drawn as falling
            // alpha shells; same read as the x-ray node.
            {
                const Vector2 nv{static_cast<float>(ccx - wx * bw * 0.42),
                                 static_cast<float>(ccy - wy * bh * 0.34)};
                DrawCircleV(nv, 11.0f, bcol(cs, 45));
                DrawCircleV(nv, 7.0f, bcol(cs, 110));
                DrawCircleV(nv, 4.5f, bcol(cs, 255));
                DrawCircleV(nv, 2.0f,
                            bcol(cs + glm::dvec3(0.0, 0.25, 0.45), 255));
            }
            DrawText("WEIGHT", bx0 + 8, by0 - 14, 10, slag);
        }
        if (info.sled_rolled) {
            const char* rt = "ROLLED";
            DrawText(rt, sx, sy + fs + 10, fs + 4, Color{240, 90, 70, 240});
        }
        // ★ FROST (WINTER_LAW §3.7, spec §4): frost = smoothstep(t_night_c,
        // t_snap_c, t), NO negation -- a rim of speckle growing from the
        // glyph edges around the dash text, driven by info.sled_frost, NOT a
        // separate animation clock. 2D primitives only (DrawBillboard renders
        // nothing in this build). A FIXED perimeter of candidate points
        // (hashed off their own index, never off info.frame_count for
        // POSITION -- only for which ones are currently "on" and for the
        // wisp drift) around the plate rect, so the speckle reads as frost
        // crystallizing on the glass rather than noise.
        if (info.sled_frost > 0.0) {
            const Rectangle plate{static_cast<float>(sx - 10),
                                  static_cast<float>(sy0 - 10), 250.0f,
                                  plate_h};
            auto hash1 = [](unsigned int n) {
                n = (n ^ 61u) ^ (n >> 16);
                n *= 9u;
                n ^= n >> 4;
                n *= 0x27d4eb2du;
                n ^= n >> 15;
                return n;
            };
            constexpr int kSpeckle = 48;
            const float perim = 2.0f * (plate.width + plate.height);
            const Color frostc{215, 232, 245, 0};
            for (int i = 0; i < kSpeckle; ++i) {
                const unsigned int hp =
                    hash1(static_cast<unsigned int>(i) * 2654435761u);
                // Walk the fixed hashed point around the rect's perimeter.
                float t = (hp % 10000) / 10000.0f * perim;
                float px, py;
                if (t < plate.width) {
                    px = plate.x + t;
                    py = plate.y;
                } else if (t < plate.width + plate.height) {
                    px = plate.x + plate.width;
                    py = plate.y + (t - plate.width);
                } else if (t < 2.0f * plate.width + plate.height) {
                    px = plate.x + plate.width -
                         (t - plate.width - plate.height);
                    py = plate.y + plate.height;
                } else {
                    px = plate.x;
                    py = plate.y + plate.height -
                         (t - 2.0f * plate.width - plate.height);
                }
                // Each speckle "opens" once frost clears its own hashed
                // threshold -- a growing rim, not an all-or-nothing flip.
                const float thresh =
                    (hash1(static_cast<unsigned int>(i) * 3u + 7u) % 1000) /
                    1000.0f;
                if (static_cast<float>(info.sled_frost) < thresh) continue;
                const float r =
                    1.0f + 2.0f * static_cast<float>(info.sled_frost);
                const unsigned char a = static_cast<unsigned char>(
                    180.0f *
                    std::min(1.0f, static_cast<float>(info.sled_frost) * 1.4f));
                DrawCircle(static_cast<int>(px), static_cast<int>(py), r,
                           Color{frostc.r, frostc.g, frostc.b, a});
            }
            // Rising sublimation wisps, frost > 0.7 only -- a handful of
            // small rects drifting up off the plate, phase off
            // info.frame_count (the SAME app-owned cosmetic counter the post
            // grain uses; no clock read here).
            if (info.sled_frost > 0.7) {
                constexpr int kWisps = 5;
                for (int i = 0; i < kWisps; ++i) {
                    const unsigned int h =
                        hash1(static_cast<unsigned int>(i) * 97u + 11u);
                    const float x0 =
                        plate.x + (h % 1000) / 1000.0f * plate.width;
                    const float period = 90.0f + static_cast<float>(h % 40);
                    const float phase =
                        std::fmod(
                            static_cast<float>(info.frame_count) + (h % 90),
                            period) /
                        period;  // 0..1 rise cycle
                    const float wy =
                        plate.y + plate.height - phase * (plate.height + 20.0f);
                    const unsigned char wa = static_cast<unsigned char>(
                        140.0f * (1.0f - phase) *
                        std::min(1.0f,
                                 (static_cast<float>(info.sled_frost) - 0.7f) /
                                     0.3f));
                    if (wa == 0) continue;
                    DrawRectangle(static_cast<int>(x0), static_cast<int>(wy), 2,
                                  5, Color{225, 238, 248, wa});
                }
            }
        }
    }

    // R4-FLY-6 BRAKE tag (Chad: "is the brake stuck?" must read on sight):
    // lit beside the gauge whenever the plant was fed a live wheel brake —
    // read from the EMITTED Inputs (info.player_inputs, the RA9 display
    // copy of what sim::step consumed), so the tag can never disagree with
    // the wheels. Absent when released: tag visible == brake applied.
    // ★ R1c: aircraft-only, and it is anchored to the flaps gauge that drive
    // mode no longer draws -- a WHEEL brake tag beside an invisible gauge.
    const float wb = info.sled_active ? 0.0f : info.player_inputs.wheel_brake;
    if (wb > 0.0f) {
        const char* tag = "BRAKE";
        const int btw = MeasureText(tag, fs);
        const int bx = x - 3 - 20 - btw - 6;  // plate left of the gauge
        const int by = y + h / 2 - fs / 2;
        DrawRectangleRec(
            Rectangle{static_cast<float>(bx - 6), static_cast<float>(by - 4),
                      static_cast<float>(btw + 12), static_cast<float>(fs + 8)},
            Color{60, 18, 12, 200});
        DrawRectangleLines(bx - 6, by - 4, btw + 12, fs + 8, red);
        DrawText(tag, bx, by, fs, red);
    }

    // R4-FLY-7 legibility ("brake works properly now but it won't let me
    // take off or taxi" — a hard-brake test breaks the prop BY DESIGN, and a
    // dead engine is a dead stick with almost no on-screen signal): an
    // ENGINE OUT plate right of the gauge whenever the damage model holds
    // the engine at zero. Same read-on-sight class as the BRAKE tag; reads
    // the same value the damage panel's ENG bar shows.
    // ★★★ L10b (Chad 2026-09-08: "yes make the plate visible afoot too").
    // NO LONGER AIRCRAFT-ONLY, and the BRAKE tag above deliberately still is.
    // The two were gated together under R1c and they are not the same kind of
    // thing: a WHEEL brake tag beside an invisible flaps gauge is an aircraft
    // READOUT with no meaning off the aeroplane, while ENGINE OUT is a
    // standing FACT about the aeroplane parked over there -- and since L10 it
    // is a fact you act on precisely when you are NOT in it. Hiding it from
    // the man walking up to fix it was the one place it was needed most.
    //
    // ⚠ THE ANCHOR IS UNCHANGED IN EVERY MODE. `x/y/w/h` are pure functions of
    // the screen size, computed above whether or not the gauge is drawn, so
    // the plate sits in the same place afoot as it does in the cockpit -- one
    // place to learn, not two. The slot is otherwise empty off the aeroplane
    // (that is what R1c freed), and the sled dash is far left at x = 24.
    if (info.show_engine_out && info.player_hp_max > 0.0 &&
        info.dmg_engine <= 0.0) {
        const char* etag = "ENGINE OUT";
        const int etw = MeasureText(etag, fs);
        const int ex = x + w + 3 + 20 + 6;  // plate right of the gauge
        const int ey = y + h / 2 - fs / 2;
        DrawRectangleRec(
            Rectangle{static_cast<float>(ex - 6), static_cast<float>(ey - 4),
                      static_cast<float>(etw + 12), static_cast<float>(fs + 8)},
            Color{60, 18, 12, 200});
        DrawRectangleLines(ex - 6, ey - 4, etw + 12, fs + 8, red);
        DrawText(etag, ex, ey, fs, red);
    }

    // R5e ESCAPE legibility (Chad fly-2/-4): a plate centered UNDER the
    // gauge whenever the escape sky is LIVE. Amber ESCAPE SKY = the field
    // is on, you are bound. Red NO RETURN = the tick's claim latch: E_spec
    // crossed 0 and app::tick severed the controls that same tick — the
    // plate states a fact about the airframe, never a projection (the fly-3
    // "I dove down and recovered" contradiction is unrepresentable).
    if (info.escape_sky) {
        const char* gtag = info.escape_claimed ? "NO RETURN" : "ESCAPE SKY";
        const Color gcol = info.escape_claimed ? red : amber;
        const int gtw = MeasureText(gtag, fs);
        const int gx = (sw - gtw) / 2;
        const int gy = y + h + 10;  // clears the bezel title (~sh-34)
        DrawRectangleRec(
            Rectangle{static_cast<float>(gx - 6), static_cast<float>(gy - 3),
                      static_cast<float>(gtw + 12), static_cast<float>(fs + 6)},
            info.escape_claimed ? Color{60, 18, 12, 200}
                                : Color{50, 40, 12, 200});
        DrawRectangleLines(gx - 6, gy - 3, gtw + 12, fs + 6, gcol);
        DrawText(gtag, gx, gy, fs, gcol);
    }

    // R6 AIR plate (Chad's bubble-edge fly cue): the LOCAL air fraction under
    // the gauge whenever the AtmosphereField is LIVE. Green = full breathable
    // bubble air, amber = thinning at the edge, red THIN AIR = near vacuum
    // (lift/thrust/authority nearly gone — the soft wall). Reads the spatial
    // atm_frac_at the plant actually flies, so the number can never disagree
    // with the felt sag. Stacked one row below the escape slot (both can be
    // live at once: bubble edge + escape ceiling).
    if (info.bubble_live) {
        const int pct = static_cast<int>(info.air_frac * 100.0 + 0.5);
        char atag[24];
        std::snprintf(atag, sizeof atag,
                      info.air_frac < 0.15 ? "THIN AIR" : "AIR %d%%", pct);
        const Color acol = info.air_frac < 0.15  ? red
                           : info.air_frac < 0.6 ? amber
                                                 : Color{120, 220, 140, 240};
        const int atw = MeasureText(atag, fs);
        const int ax = (sw - atw) / 2;
        const int ay = y + h + 10 + (fs + 9);  // one row below the escape slot
        DrawRectangleRec(
            Rectangle{static_cast<float>(ax - 6), static_cast<float>(ay - 3),
                      static_cast<float>(atw + 12), static_cast<float>(fs + 6)},
            Color{18, 34, 26, 200});
        DrawRectangleLines(ax - 6, ay - 3, atw + 12, fs + 6, acol);
        DrawText(atag, ax, ay, fs, acol);
    }
}

void draw_frame(const sim::SimState& state, const sim::AircraftParams& params,
                const CameraPose& pose, const FrameInfo& info,
                const sim::Environment* env) {
    // T25 stutter attribution: lap markers for the app-owned profiler
    // (SEADS_PROF). No-op (nullptr) in every normal run.
    const auto pmark = [&](const char* n) {
        if (info.prof_mark != nullptr) info.prof_mark(n);
    };
    Camera3D cam{};
    cam.position = Vector3{0.0f, 0.0f, 0.0f};  // eye-relative world
    cam.target = rel(pose.target, pose.eye);
    cam.up =
        Vector3{static_cast<float>(pose.up.x), static_cast<float>(pose.up.y),
                static_cast<float>(pose.up.z)};
    // Current FOV (RMB-zoom eases it). The off-center frustum (line ~157) and
    // the reticle projection (line ~196) both read cam.fovy below, so this one
    // assignment carries the zoom to all three consistently.
    cam.fovy = static_cast<float>(info.fovy_deg);
    cam.projection = CAMERA_PERSPECTIVE;

    BeginDrawing();
    // S1 stereoscope post pass (docs/stereoscope_s1_plan.md): capture the 3D
    // scene into the RGBA16F + depth-texture FBO (render/post), then resolve it
    // through the B&W silver-print chain. If disabled (SEADS_NO_POST) or the
    // FBO failed to build, the scene renders straight to the backbuffer with
    // the legacy clear (a logged, flagged fallback — never silent). The HUD
    // ALWAYS draws RAW after the resolve (SPEC §9.2 — no grain on the
    // reticle/loop).
    const int post_sw = GetScreenWidth();
    const int post_sh = GetScreenHeight();
    const bool post_captured = post_begin(post_sw, post_sh);
    if (!post_captured) ClearBackground(Color{18, 24, 44, 255});

    // F-POSE TRUE-EYE SIGHT VIEW: the manned head camera sits on the sight
    // axis kSightEyeBackM behind st_eye (Chad's 2026-08-30 fly report --
    // the old 2.9 m pull-back floated his mitts), where the global 2 m
    // near plane would clip his arms, the grips and the whole sight
    // assembly. Swap in a close near plane for
    // THIS 3D pass only and restore the init_draw() default right after
    // EndMode3D -- the HUD is 2D and every other frame's pass sees 2 m
    // untouched. 0.15 m: the grips sit ~0.33 m from st_eye, the mitts wrap
    // toward the eye, and the rear peep (0.10 m out) clips away exactly
    // like a real peep blurs at the eye. Depth precision costs ~13x at the
    // far plane vs near=2 -- certified by the sight-view smoke shot (the
    // horizon/sphere wire is the known z-fight victim [init_draw]).
    // ★ POLISH FLY-2: the SHOULDERED LAUNCHER VIEWS join the flak sight on the
    // close near plane. Both sit ~1.5 m off the man -- inside the 2 m plane --
    // which is literally Chad's "looking right into the body of the sudburian":
    // the near plane cutting his torso and leaving the hollow interior facing
    // the camera. Same 0.15 m plane, same already-certified depth trade.
    const bool sight_near =
        (info.flak_manned && !info.flak_cam_external) || info.sting_close_cam;
    if (sight_near) rlSetClipPlanes(0.15, 60000.0);
    BeginMode3D(cam);
    // Vertical lens shift (SPEC §9.2 framing): override raylib's symmetric
    // projection with an off-center frustum so the whole 3D scene slides down
    // (the reticle overlay below subtracts the SAME shift). The camera's look
    // direction is untouched — only the projection's vertical center moves.
    if (info.lens_shift_ndc != 0.0) {
        const double aspect3d = static_cast<double>(GetScreenWidth()) /
                                static_cast<double>(GetScreenHeight());
        const double nearZ = rlGetCullDistanceNear();
        const FrustumBounds fb =
            off_center_frustum(static_cast<double>(cam.fovy) * PI / 180.0,
                               aspect3d, nearZ, info.lens_shift_ndc);
        // rlSetMatrixProjection is a no-op under a GL 1.1 build (rlgl gates it
        // on GRAPHICS_API_OPENGL_33/ES2); desktop raylib defaults to GL33 so
        // this is live. Under GL11 the 3D scene would stay symmetric while the
        // reticle still shifts — decoupled. Not reachable in the shipped
        // desktop build.
        rlSetMatrixProjection(MatrixFrustum(fb.l, fb.r, fb.b, fb.t, nearZ,
                                            rlGetCullDistanceFar()));
    }
    // Sky pass (Stage 2): a fullscreen quad drawn FIRST (depth-test off) whose
    // per-pixel view ray = the frustum corner rays, computed from the SAME fovy
    // + lens shift the scene uses so the horizon tracks under zoom/shift (P0).
    // Then the planet mesh draws over it and occludes the below-horizon sky.
    static SkyRenderer sky = load_sky();
    // EYE-RELATIVE sun light-travel direction (Stage 3a, plan "Sun disc"):
    // place the finite sun at sun_pos = (dir-to-sun)*distance, then take the
    // direction from THAT point to the eye. -info.sun_dir is the
    // origin-relative dir to the sun; the ~2 deg origin-vs-eye parallax at 450
    // km is what would else flash the mirror lakes beside the disc. The SAME
    // sun_f feeds the sky pass AND the planet aerial below, so the sky and the
    // ground limb cannot fork (trap-1).
    const glm::dvec3 to_sun = -glm::normalize(info.sun_dir);
    const glm::dvec3 sun_pos =
        to_sun * static_cast<double>(info.sun.distance_m);
    const glm::vec3 sun_f = glm::vec3(glm::normalize(pose.eye - sun_pos));
    const double eye_r = glm::length(pose.eye);
    const glm::vec3 up_f = glm::vec3(pose.eye / eye_r);
    const float eye_alt = static_cast<float>(eye_r - params.R);
    // Horizon dip: the limb is acos(R/|eye|) below level; the sky rim anchors
    // to it so the atmosphere stays a thin arc on the limb (same op
    // draw_planet_mesh runs).
    const float horizon_elev =
        static_cast<float>(-std::acos(std::min(1.0, params.R / eye_r)));
    // Moon (Stage 5): a pure world direction at infinity (no eye-relative
    // needed — unlike the finite sun). moon_fill is the PHASE-SHAPED moonlight
    // gain (0 at new moon so the dark side stays dark; ~ground_gain at full),
    // the same scalar feeding the disc's phase and the ground's 2nd light.
    const glm::vec3 moon_f = glm::vec3(glm::normalize(info.moon_dir));
    const float moon_fill =
        info.moon.ground_gain *
        glm::smoothstep(info.moon.fill_lo, 1.0f,
                        static_cast<float>(info.moon_phase_frac));
    if (sky.ok) {
        const double aspect = static_cast<double>(GetScreenWidth()) /
                              static_cast<double>(GetScreenHeight());
        const glm::dvec3 cam_forward = glm::normalize(pose.target - pose.eye);
        glm::dvec3 corners[4];
        frustum_corner_rays(static_cast<double>(cam.fovy) * PI / 180.0, aspect,
                            info.lens_shift_ndc, cam_forward, pose.up, corners);
        // Clamp the eye INSIDE the aurora shell (Fable red-team P1-2): a
        // ballistic zoom above aurora_height_m, or a mis-set height, would
        // break the interior-single-root precondition and smear the aurora.
        // Enforcing it at the seam (not assuming it) keeps the ray-shell
        // well-posed always.
        const float aur_eye_r =
            std::min(static_cast<float>(eye_r), info.aurora.shell_r_m * 0.999f);
        draw_sky(sky, corners, sun_f, up_f, eye_alt, horizon_elev,
                 info.atmosphere, info.sun, moon_f, info.moon, info.aurora,
                 info.aurora_phase_sc, aur_eye_r, info.air_field);
    }
    // Star field (Stage 4): drawn AFTER the sky quad, BEFORE the planet
    // (additive, depth off — the planet then overdraws below-horizon stars for
    // free, the sun-disc pattern). Lazily built once from the derived celestial
    // basis (the catalog's inertial dirs); the sky WHEEL rotates the whole
    // field about Polaris each frame. Skipped if the caller never built cel.
    if (info.celestial != nullptr) {
        static StarRenderer stars = load_stars(*info.celestial, info.stars);
        draw_stars(stars, info.sky_wheel, sun_f, up_f, eye_alt, horizon_elev,
                   info.stars, info.atmosphere, static_cast<float>(cam.fovy),
                   GetScreenHeight(), moon_f, info.moon.ang_radius_rad,
                   info.air_field);
    }
    pmark("sky_stars");
    // ★ BY FIELD NAME, NOT BY POSITION. This was a 13-element positional brace
    // init; R3 inserted two members into SnowParams and every field after the
    // insertion point silently shifted one slot -- caught here only because the
    // types happened to disagree (vec3 into float). A same-typed insertion
    // would have compiled and quietly fed snow_sparkle into barren_face_dark.
    SnowParams snow_params;
    snow_params.cover = info.winter_snow;
    snow_params.albedo = info.snow_albedo;
    snow_params.slope_lo = info.snow_slope_lo;
    snow_params.ice_albedo = info.ice_albedo;
    snow_params.ice_reflect_frac = info.ice_reflect_frac;
    snow_params.ice_glint_frac = info.ice_glint_frac;
    snow_params.night_glow = info.night_glow;
    snow_params.ice_night_reflect_frac = info.ice_night_reflect_frac;
    snow_params.snow_sparkle = info.snow_sparkle;
    snow_params.snow_sparkle_sharp = info.snow_sparkle_sharp;
    snow_params.barren_face_dark = info.barren_face_dark;
    snow_params.barren_face_dark_hi_deg = info.barren_face_dark_hi_deg;
    snow_params.barren_face_mottle = info.barren_face_mottle;
    // ★ R3 live sweep (PgUp/PgDn in app/main.cpp, seeded from
    // SEADS_R3_FULLDEPTH). <= 0 keeps the SnowParams ship value, so this line
    // is identity until Chad actually moves the dial.
    if (info.r3_full_depth > 0.0f) snow_params.full_depth = info.r3_full_depth;
    // depth_mix / full_depth keep their SnowParams defaults. ★ SINCE CHAD'S
    // 2026-08-27 RULING depth_mix DEFAULTS TO 1.0 -- the depth-keyed exposure
    // is what SHIPS, and SEADS_SNOWDEPTH=0 is what DISARMS it
    // (planet.cpp:1328). It is still clamped to 0 at the draw for any planet
    // whose mesh carries no depth channel, so this line cannot arm it against
    // Earth or NO_SNOWFOLD.
    // ★ R5 row 9 — SHADOWS ON SNOW: the caster proxies, rebased to the eye IN
    // DOUBLE here (the file-header seam: a float cast of raw 15 km world
    // coordinates quantizes at ~2 mm and would shimmer a flare-height
    // penumbra of 0.06 m) and capped at the shader's uniform budget. An empty
    // list — the shipped default, the SEADS_SHADOWS=0 kill, and every caller
    // that never fills it (tests, probe, harness) — leaves count 0, which is
    // the shader's bit-identical early-out. tan_sun is derived from the SAME
    // SunParams.ang_radius_rad the drawn disc uses ([celestial]
    // sun_angular_diameter_deg 1.40 — the shipped 4x sun, never the
    // consult's 0.35), so the penumbra and the disc can never disagree.
    render::ShadowSet shadow_set;
    shadow_set.count = static_cast<int>(
        std::min(info.shadow_casters.size(),
                 static_cast<std::size_t>(render::kMaxShadowCasters)));
    for (int ci = 0; ci < shadow_set.count; ++ci) {
        const render::ShadowCaster& c = info.shadow_casters[ci];
        shadow_set.a[ci] = glm::vec4(glm::vec3(c.a - pose.eye),
                                     static_cast<float>(c.radius));
        shadow_set.b[ci] = glm::vec4(glm::vec3(c.b - pose.eye), 0.0f);
    }
    shadow_set.strength = info.shadow_strength;
    shadow_set.tint = info.shadow_tint;
    shadow_set.tan_sun =
        std::tan(static_cast<float>(info.sun.ang_radius_rad));
    draw_planet(params, pose.eye, shadow_set, sun_f, info.atmosphere, moon_f,
                moon_fill, info.moon.sparkle_sharpness, info.aurora,
                snow_params, info.air_field);
    pmark("planet");
    // Dedicated flat lake mirror surfaces, immediately after the planet so they
    // reuse its just-bound sky/scatter/moon/aurora uniforms (no fork with the
    // limb) and overdraw the coarse shore facets that mis-render the big lakes.
    draw_water(pose.eye);
    pmark("water");
    // S3 draped linework ribbons (roads / snowmobile trails) — after the opaque
    // planet+water (writes depth for S6), before the translucent props. Roads
    // read mono (silvered by the S1 post); the trail is the light-green accent.
    // ★ ROAD-REPAIR: the SF2 bank strips are lit and sparkled now, and every
    // dial they use is taken HERE from the value draw_planet was handed on the
    // line above -- one light, two consumers. A bank cannot be at a different
    // time of day than the field it sits in, and the sparkle cannot stop at
    // the road edge, because neither surface owns a light of its own.
    BankLight bank_light;
    bank_light.sun_dir = sun_f;
    bank_light.moon_dir = moon_f;
    bank_light.moon_fill = moon_fill;
    bank_light.day_gain = info.atmosphere.ground_day_gain;
    bank_light.night_fill = info.atmosphere.night_fill_min;
    bank_light.night_glow = snow_params.night_glow;
    bank_light.snow_sparkle = snow_params.snow_sparkle;
    bank_light.snow_sparkle_sharp = snow_params.snow_sparkle_sharp;
    // The SAME SEADS_SPARKLE dial the planet multiplies by (render/planet.h):
    // SEADS_SPARKLE=0 has to kill the glint on the bank too, or the A/B arm is
    // not an A/B arm.
    bank_light.sun_sparkle = sun_sparkle_dial() * snow_params.sun_sparkle;
    bank_light.sun_sparkle_sharp = snow_params.sun_sparkle_sharp;
    bank_light.winter = info.winter_snow;
    draw_ribbons(pose.eye, info.winter_snow, bank_light);
    draw_buildings(pose.eye, sun_f);
    pmark("ribbons_bldgs");
    // CC2 slag-pot trains: a mono steel loco + pots on the draped Copper Cliff
    // spur, opaque (writes the S6 depth) after the buildings; pose is the
    // FINISHED wrapped phase the app derived from t_cel (render/ reads no
    // clock).
    draw_train(pose.eye, sun_f, info.train_phase);
    // CC3/CC5 slag pour: the marquee — dark ridge + the warm-chroma lava
    // cascade. Opaque after the buildings/train (the lava's high chroma
    // survives the S1 split- tone). CC5 SINGLE CLOCK: when the train is
    // crest-routed the pour phase is derived from the train HEAD
    // (train_dump_phase) so the tipping pots FEED the pour; a negative sentinel
    // means uncoupled -> fall back to the standalone CC3 cycle.
    const double dump_ph = train_dump_phase(g_train, info.train_phase);
    const bool fed = dump_ph >= 0.0;
    draw_slag(pose.eye, sun_f, fed ? dump_ph : info.pour_phase, fed);
    pmark("train_slag");
    // T2 Errington tunnel greybox interior: opaque (writes the S6 depth), after
    // the slag. Built lazily FROM the T1 net (single-source with the collision
    // volume); no-op when info.tunnel_net is null. Drapes its mouth collars on
    // the planet height field so the funnels meet the portal holes cut in the
    // terrain mesh.
    // Per-mouth collar outer radii (T4a): derived from the terrain grid so each
    // funnel spans the full jagged cut edge (collar_reach from tunnel_mesh.h,
    // same formula the test uses). Errington = the tube-scaled portal hole;
    // Murray = the OPEN-PIT bowl opening. The cut radii mirror main.cpp's
    // planet_cuts derivation exactly (world:: single-source helpers).
    const double err_cut =
        info.tunnel_net
            ? world::errington_cut_radius(*info.tunnel_net, kMouthCutFactor)
            : 0.0;
    const double mur_cut =
        info.tunnel_net ? world::murray_cut_radius(info.tunnel_net->bowl.bowl_r)
                        : 0.0;
    const double err_collar = collar_reach(err_cut, g_planet_cfg.subdiv,
                                           g_planet_cfg.tiles, params.R);
    const double mur_collar = collar_reach(mur_cut, g_planet_cfg.subdiv,
                                           g_planet_cfg.tiles, params.R);
    // T5d: the terrain cell arc = the radial ring spacing for the terrain-
    // hugging collar/bowl-rim band (kills the portal strobe). SAME formula
    // collar_reach() consumes (one definition), so a subdiv/tiles retune moves
    // the ring density with the collar reach.
    const double cell_arc =
        terrain_cell_arc(g_planet_cfg.subdiv, g_planet_cfg.tiles, params.R);
    draw_tunnel(pose.eye, sun_f, info, g_planet.ok ? &g_planet.height : nullptr,
                err_collar, mur_collar, cell_arc);
    pmark("tunnel_walls");
    // S2 boreal trees — instanced mono cross-quads on the terrain, after the
    // opaque planet+water (depth-on, writes the S6 depth FBO) and before the
    // translucent prop discs. Placement reads the planet's OWN height field
    // (anti-float). No-op when [trees].enabled is false or the raster is
    // absent.
    draw_trees(pose.eye, sun_f);
    pmark("trees");
    // Fleet Rig mirror finish (rig-A.2): the config chroma + mirror knobs + the
    // frozen sun. Drawn AFTER the planet so the albedo cubemap it samples is
    // built (draw_planet's ensure_planet ran above).
    // rig-B: deflection magnitudes are config DEGREES -> radians at this render
    // boundary (angles are radians internally). Direction is structural.
    const float kD2R = static_cast<float>(3.14159265358979323846 / 180.0);
    const render::DeflectGains gains{
        static_cast<float>(info.rig_aileron_deg) * kD2R,
        static_cast<float>(info.rig_elevator_deg) * kD2R,
        static_cast<float>(info.rig_rudder_deg) * kD2R,
        static_cast<float>(info.rig_gear_deploy_deg) * kD2R};
    const FleetDrawParams fp{sun_f,
                             static_cast<float>(info.rig_reflectivity),
                             static_cast<float>(info.rig_fresnel_power),
                             info.rig_player_color,
                             info.rig_bandit_color,
                             gains,
                             static_cast<float>(info.rig_prop_disc_alpha),
                             static_cast<float>(info.rig_prop_idle_alpha),
                             render::viz_params(info.plane_viz)};
    // Opaque pass: player + every drone, each posed by its OWN commanded Inputs
    // (player_inputs / drone_inputs[i]; a missing/short drone_inputs draws at
    // rest — e.g. the frozen probe target). Gear reads each plane's state.gear.
    draw_aircraft(state, pose.eye, fp, info.player_inputs, /*enemy=*/false,
                  /*scale=*/1.0,
                  static_cast<float>(info.player_wheel_roll_rad));
    for (std::size_t i = 0; i < info.drones_draw.size(); ++i) {
        // CONQUEST no-respawn: a killed wreck (drones_alive[i]==0) stops
        // rendering. Empty drones_alive => every drone drawn (bit-identical).
        if (i < info.drones_alive.size() && info.drones_alive[i] == 0) continue;
        const sim::Inputs di =
            i < info.drone_inputs.size() ? info.drone_inputs[i] : sim::Inputs{};
        // S-mapteam: livery follows the HOSTILITY flag (drones_friendly, the
        // same single source the ALLY/BANDIT tag reads — raw faction would
        // paint a furball-hostile teammate blue). Empty list (non-conquest) =>
        // everyone is a bandit, bit-identical to before.
        const bool ally_plane =
            i < info.drones_friendly.size() && info.drones_friendly[i] != 0;
        // ★★★ ALREADY ABSOLUTE, no edit needed here: ally_livery=true routes
        // this plane to fp.player_color, which is rig_player_color = the
        // player's OWN kit livery (render/team_kit.h) -- and the kit's plane
        // colour is his faction's colour on both sides (Valley player blue,
        // Central City player slag orange). So a friendly maverick already
        // wears his side's hue whichever side that is. The HOSTILE half stays
        // the E4.2 legibility red, which is a HOSTILITY cue, not a faction
        // one: at the shipped enemy_tint_shift = 1.0 it was never either team
        // hue. Checked, not assumed -- see draw_aircraft's colour line.
        // RUNG E4.1 VISIBILITY: attenuate the mirror finish's grayscale
        // env-reflection wash on DRONE meshes only, fading in with slant range
        // (the approved close-up look is untouched at the merge). The player
        // pass above never passes this, and no terrain/sky/water/prop pass can
        // see it. aircraft_haze_frac = 1.0 => scale == 1.0 exactly => today.
        const double drone_dist =
            glm::length(info.drones_draw[i].position - pose.eye);
        const float wash = static_cast<float>(render::aircraft_env_wash_scale(
            drone_dist, info.legibility.aircraft_haze_frac,
            info.legibility.haze_full_range_m));
        draw_aircraft(info.drones_draw[i], pose.eye, fp, di, /*enemy=*/true,
                      info.drone_scale, /*wheel_roll_rad=*/0.0f, ally_plane,
                      wash);
        // RUNG E4.3 ENGAGEMENT GLINT: a nav-light bead on ENEMY drones inside
        // engage range, so a contact that is only a few pixels of grey mirror
        // still announces itself. REAL GEOMETRY (DrawSphere) — DrawBillboard
        // silently renders nothing in this engine (a documented house trap).
        // Deterministic: the phase is FrameInfo::frame_count (the app-owned
        // render-frame ordinal that already seeds the post grain) offset by the
        // drone INDEX, so there is no clock and no rng, and the fleet does not
        // strobe in unison. glint_size_m = 0 => radius 0 => nothing drawn.
        if (!ally_plane && drone_dist <= info.legibility.glint_range_m) {
            const double gr =
                render::glint_radius_m(drone_dist, info.legibility.glint_size_m,
                                       info.legibility.glint_min_mrad);
            if (gr > 0.0 &&
                render::glint_lit(info.frame_count, static_cast<int>(i),
                                  info.legibility.glint_period_frames,
                                  info.legibility.glint_duty)) {
                // Sit the bead just above the canopy along the plane's own up
                // axis so it reads as ON the aircraft from every aspect.
                const glm::dvec3 up_body =
                    glm::dquat(info.drones_draw[i].orientation) *
                    glm::dvec3(0.0, 1.0, 0.0);
                const glm::dvec3 gpos = info.drones_draw[i].position +
                                        up_body * (1.1 * info.drone_scale);
                // The bead wears the SHIFTED enemy hue (one authority, the
                // same function the livery and the tag use), with a small
                // near-white core so it reads as a LIGHT, not as paint.
                const glm::dvec3 gc = render::enemy_legibility_tint(
                    render::team_colors().enemy,
                    info.legibility.enemy_tint_shift);
                const auto q = [](double v) {
                    return static_cast<unsigned char>(
                        std::lround(std::min(1.0, std::max(0.0, v)) * 255.0));
                };
                DrawSphere(rel(gpos, pose.eye), static_cast<float>(gr),
                           Color{q(gc.x), q(gc.y), q(gc.z), 255});
                DrawSphere(rel(gpos, pose.eye), static_cast<float>(gr * 0.45),
                           Color{255, 240, 230, 255});
            }
        }
    }
    // ★ STING RPAS (the sting lane): the player's FPV interceptor, primitive-
    // built until its GLB rung — the Wild Hornets Sting silhouette: a bullet
    // body, the camera/charge DOME riding its back (the identifying feature),
    // four rotor arms. REAL GEOMETRY throughout (DrawBillboard renders
    // nothing in this engine — the documented house trap). ALLY BLUE by the
    // standing livery ruling: a player-faction machine wears the ally blue,
    // never the slag orange — one authority, render::team_colors().
    if (info.sting_flying) {
        const glm::dvec3 sp_ = info.sting_draw.position;
        const glm::dmat3 sR = glm::mat3_cast(info.sting_draw.orientation);
        // ST-5: the hero GLB, when it exists and SEADS_STING_MODEL != 0,
        // replaces every primitive below outright (ready() only goes true
        // once the model has actually LOADED, the flak_gunner discipline --
        // a missing/kill-switched GLB leaves the primitives drawing exactly
        // as they always have, bit-identical).
        if (render::sting_model_ready()) {
            render::sting_model_draw(sp_, sR, pose.eye, 1.0f,
                                     info.sting_prop_phase,
                                     info.sting_prop_rate);
        } else {
        const auto sq = [](double v) {
            return static_cast<unsigned char>(
                std::lround(std::min(1.0, std::max(0.0, v)) * 255.0));
        };
        // ★★★ The greybox fallback for the SAME player RPAS as the hero
        // model above: his own side's hue, absolute. Named `blue` still,
        // because it is blue for the shipped Valley player and renaming it
        // would churn a dozen draw calls below for nothing.
        const glm::dvec3 ab =
            render::faction_color(info.conquest_player_faction);
        const Color blue{sq(ab.x), sq(ab.y), sq(ab.z), 255};
        const Color slate{40, 46, 58, 255};
        // Bullet body: fat tail to fine nose along body -Z (nose = -Z;
        // length 1.2, tail +0.55 .. nose -0.65).
        DrawCylinderEx(rel(sp_ + sR * glm::dvec3(0.0, 0.0, 0.55), pose.eye),
                       rel(sp_ + sR * glm::dvec3(0.0, 0.0, -0.65), pose.eye),
                       0.16f, 0.06f, 10, blue);
        // The dome on its back.
        DrawSphere(rel(sp_ + sR * glm::dvec3(0.0, 0.18, 0.05), pose.eye),
                   0.13f, slate);
        // ★ fly-2 (Chad): "the x-frame of the quad needs to align
        // perpendicular to the length of the bullet fuselage" — the four
        // arms radiate in the X/Y plane at ONE forward station (rotor discs
        // facing along the body axis, a missile-style tractor quad), not
        // flat in the horizontal like a camera drone.
        constexpr double kArmZ = -0.25;  // the forward-third station
        for (int ax = -1; ax <= 1; ax += 2) {
            for (int ay = -1; ay <= 1; ay += 2) {
                const glm::dvec3 tip =
                    sp_ + sR * glm::dvec3(0.34 * ax, 0.34 * ay, kArmZ);
                DrawCylinderEx(
                    rel(sp_ + sR * glm::dvec3(0.0, 0.0, kArmZ), pose.eye),
                    rel(tip, pose.eye), 0.035f, 0.035f, 6, slate);
                DrawSphere(rel(tip, pose.eye), 0.07f, blue);
            }
        }
        // ★ fly-2 (Chad): "short stubby wings protruding from the start of
        // the last 1/3 of its length" — the rear third begins at z = +0.15
        // (nose -0.65 + 2/3 of 1.2); two swept stubs off the flanks there.
        for (int ax = -1; ax <= 1; ax += 2) {
            DrawCylinderEx(
                rel(sp_ + sR * glm::dvec3(0.10 * ax, 0.0, 0.15), pose.eye),
                rel(sp_ + sR * glm::dvec3(0.44 * ax, 0.0, 0.26), pose.eye),
                0.06f, 0.025f, 6, blue);
        }
        }
    }
    // CONQUEST pump world markers (placeholder-grade, opaque depth-writers so
    // terrain occludes them). SURFACE pumps carry a TALL vertical beacon
    // column along local-up so they're "marked" and legible from km away; a
    // DEAD pump draws dark/extinguished. Empty conquest_pumps => nothing new
    // drawn.
    //
    // S-mapteam (Chad 2026-08-09): the pump BODY is the medical-oxygen GREEN,
    // exactly matching its map glyph — a pump reads the same in the sky as on
    // the chart. OWNERSHIP moved to the BEACON (the tall column + lamp) in the
    // owner's team colour, mirroring the map's owner RING.
    //
    // ⚠ THE PENDULUM HAS SWUNG BOTH WAYS HERE -- recorded so nobody swings it
    // back by accident. This comment used to end: "the old VALLEY-cool /
    // SUDBURY-warm absolute tint was not player-relative, so an ally pump
    // could read warm for one side and cool for the other", and the beacon was
    // made player-relative on that reasoning. Chad flew a SUDBURY player for
    // the first time on 2026-09-10 (the spawn menu's side stage is what made
    // that reachable at all) and ruled the other way, verbatim: "Sudbury
    // always has to be the orange team." So the beacon is ABSOLUTE again --
    // render::faction_color, one authority for every side-coded surface. The
    // "which pump is MINE" question the old reasoning cared about is answered
    // by the map's SHAPE language and its FIX/ATTACK labels, not by hue.
    const auto pcol = [](const glm::dvec3& c, unsigned char a) {
        const auto q = [](double v) {
            return static_cast<unsigned char>(
                std::lround(std::min(1.0, std::max(0.0, v)) * 255.0));
        };
        return Color{q(c.x), q(c.y), q(c.z), a};
    };
    const Color c_pump_body = pcol(render::team_colors().objective, 255);
    for (const FrameInfo::ConquestPump& pu : info.conquest_pumps) {
        const glm::dvec3 up = glm::normalize(pu.pos);
        // ★★★ ABSOLUTE BY FACTION (Chad 2026-09-10: "Sudbury always has to
        // be the orange team"). Was `pu.faction == player_faction ? ally :
        // enemy`, which turned the Valley orange the first time a player
        // flew for Sudbury. Identical output for the shipped Valley player.
        const Color owner_c = pcol(render::faction_color(pu.faction), 255);
        const Color tint = pu.alive ? c_pump_body : Color{60, 64, 70, 255};
        const Vector3 base = rel(pu.pos, pose.eye);
        if (pu.surface) {
            // ★ THE BODY STANDS ON THE GROUND (Chad 2026-09-06: "the location
            // is still too high above me ... it needs to come down"). The
            // logical point `pos` is 10 m up its mast (the raiders' target,
            // app/spawn_policy.h kSurfacePumpMastM); the 24 x 30 m cube drawn
            // there floated with its floor 5 m over the snow and read as
            // unreachable from a man's eye. The pump HOUSE is now a 12 m x
            // 10 m x 12 m block whose floor is the terrain under it, and the
            // beacon column rises from the same floor. The FOOT is where the
            // reach law measures to (app/interact.h), so the U prompt lights
            // at the wall he can see.
            const glm::dvec3 foot_w =
                glm::length(pu.foot) > 1e-9 ? pu.foot : pu.pos;
            const Vector3 house = rel(foot_w + up * 5.0, pose.eye);
            DrawCube(house, 12.0f, 10.0f, 12.0f, tint);
            DrawCubeWires(house, 12.0f, 10.0f, 12.0f, Color{20, 24, 28, 200});
            const Vector3 footv = rel(foot_w, pose.eye);
            const glm::dvec3 top_w = foot_w + up * 450.0;  // tall beacon column
            const Vector3 top = rel(top_w, pose.eye);
            // The beacon carries OWNERSHIP (team colour), the body carries
            // WHAT IT IS (oxygen green) — same split as the map glyph + ring.
            DrawCylinderEx(footv, top, 4.0f, 2.5f, 8,
                           pu.alive ? owner_c : tint);
            if (pu.alive) DrawSphere(top, 14.0f, owner_c);  // beacon lamp
        } else {
            DrawCube(base, 24.0f, 30.0f, 24.0f, tint);
            DrawCubeWires(base, 24.0f, 30.0f, 24.0f, Color{20, 24, 28, 200});
        }
        if (!pu.surface && pu.alive) {
            // 2026-07-25 fly-2 ruling B: deep pumps must read in a lamp-lit
            // BLACK chamber (Chad's "no pumps in the black stope" report was
            // partly a legibility problem too) — bigger + brighter than the
            // old plain 16 m sphere. Two nested opaque spheres (no blend
            // state to manage here): a larger tinted shell + a small
            // near-white hot core. Hit radius_m (25 m) is UNCHANGED — this is
            // draw-only prominence.
            const Color bright = owner_c;
            // kDeepPumpShellM is the ONE copy of this radius — the loader's
            // pump_frame clearance bound reads the same constant, so the neon
            // frame can never be sized into the body (H1).
            DrawSphere(base, static_cast<float>(render::kDeepPumpShellM),
                       tint);                 // deep-pump beacon shell
            DrawSphere(base, 13.0f, bright);  // bright inner core
        }
    }
    // FLAK (F-LOAD/DRAW, docs/FLAK_GUN_SPEC.md §7): the Oerlikons on their
    // pumps' threat flanks -- opaque depth-writing meshes in the same pass as
    // the pump markers, so terrain occludes them and the ADDITIVE glows below
    // still composite over them. Empty vector => zero calls (off-arm).
    // THE GUN'S OWN SIGNAL (Chad 2026-08-27: "I couldnt find either of the
    // guns from the air. It needs its own signal"). A SEARCHLIGHT UP-CONE:
    // narrow at the gun, opening skyward -- the inverse of the sled's ruled
    // down-cone (render/sled_marker.h), so the three machine signals stay
    // distinct: pump = tall column + lamp, sled = blue light pooling DOWN,
    // flak = a searchlight beam UP (the one shape that already MEANS
    // anti-aircraft). Owner team colour, like the pump beacon. STEADY -- no
    // blink, no clock (the sled ruling). Real geometry, never DrawBillboard
    // (the house trap). 120 m: read from the raid envelope (250-400 m AGL
    // inside 1 km) without becoming the pump column's rejected "wall" (450
    // m). SUPPRESSED for the manned gun -- its gunner does not need to find
    // it, and a vertical beam would glare through the high-elevation sight.
    // Heights/radii are FLY DIALS.
    for (std::size_t fi = 0; fi < info.flak_guns.size(); ++fi) {
        const FlakDraw& fg = info.flak_guns[fi];
        flak_model_draw(fg, pose.eye);
        if (static_cast<int>(fi) == info.flak_manned_gun) {
            // F-POSE: the Sudburian ON the manned gun (crouch-curve pose,
            // render/flak_gunner.cpp). Drawn ONLY from outside him -- the
            // free-look pullout, or a SEADS_FLAKCAM smoke -- carried by
            // flak_gunner_alpha, which is 0 down the sight (Chad's
            // 2026-08-31 ruling: the sight picture stays clean).
            if (flak_gunner_enabled() && info.flak_gunner_alpha > 0.0f)
                flak_gunner_draw(fg, pose.eye, info.flak_gunner_alpha);
            continue;
        }
        constexpr double kSignalTopM = 120.0;   // beam length up local up
        constexpr float kSignalBaseR = 0.35f;   // at the gun
        constexpr float kSignalTopR = 5.5f;     // at the top (the spread)
        const glm::dvec3 fup = glm::normalize(fg.up);
        // ★★★ ABSOLUTE BY FACTION -- the gun's beacon says WHOSE GROUND it
        // stands on, and that does not change with who is looking at it.
        const glm::dvec3 oc3 = render::faction_color(fg.faction);
        const auto fq = [](double v) {
            return static_cast<unsigned char>(
                std::lround(std::min(1.0, std::max(0.0, v)) * 255.0));
        };
        const Color oc{fq(oc3.x), fq(oc3.y), fq(oc3.z), 255};
        const Vector3 b = rel(fg.pos + fup * 2.2, pose.eye);
        const Vector3 t = rel(fg.pos + fup * kSignalTopM, pose.eye);
        DrawCylinderEx(b, t, kSignalBaseR, kSignalTopR, 10, oc);
        // close-range lamp over the breech, so the walk-up has a point light
        DrawSphere(rel(fg.pos + fup * 2.2, pose.eye), 0.8f, oc);
    }
    // Street-lamp night glows: ADDITIVE, so drawn AFTER every opaque
    // depth-writer (planet/buildings/trees/aircraft) or a later opaque draw
    // farther than a lamp would paint over the already-composited glow —
    // tree-shaped bites, lamps winking behind planes (Fable-AFTER P1-1).
    // Depth-test on (terrain occludes), write off.
    pmark("aircraft");
    draw_lamps(pose.eye, sun_f, static_cast<float>(cam.fovy));
    // Tunnel gaslamp glows (T4b, P2-1): ADDITIVE, same pass as street lamps —
    // AFTER all opaque depth-writers (incl. aircraft/trees) so an opaque body
    // cannot overpaint a nearer tunnel glow. draw_tunnel() above drew only the
    // opaque wall/mesh pass; the lamp glows are deferred here.
    draw_tunnel_lamps(pose.eye, sun_f, info);
    // S-pumpcube (Chad 2026-08-09): "the green pumps inside the black stope
    // need a wire cube that is neon slag and opposing blue of the same team
    // color codes to frame in that pump otherwise they dont apper claimed by
    // either side in the black stope".
    //
    // Drawn HERE, in the additive pass, for the same reason the street lamps
    // are (P1-1): after EVERY opaque depth-writer, or a later opaque body
    // farther than the frame paints over the already-composited glow. Depth
    // TEST stays on so the stope rock still occludes it; depth WRITE is off so
    // the nested shells cannot z-fight each other or the pump body.
    //
    // NEON = additive + self-lit. The arena has no sun and no lamp on the
    // pumps, so a SHADED wireframe is invisible down there — which is exactly
    // the defect being reported. Additive over black can only brighten.
    // Thickness comes from nested shells, NOT rlSetLineWidth: GL core profile
    // clamps line width to 1, so a width-based glow silently ships as a
    // hairline on this hardware.
    //
    // COST: 12 edges * `layers` shells * (2 deep pumps by default) = 72
    // DrawLine3D calls per frame at the shipped table, all of which rlgl folds
    // into its ONE batched line buffer. No shader is built here, ever — the
    // frame rides raylib's default unlit path, so there is no per-pump material
    // and nothing to recompile per frame (the T-thread lamp-storm lesson).
    if (render::pump_frame_style().enabled && !info.conquest_pumps.empty()) {
        const render::PumpFrameStyle& pfs = render::pump_frame_style();
        BeginBlendMode(BLEND_ADDITIVE);
        rlDisableDepthMask();  // depth TEST stays on; WRITE off
        for (const FrameInfo::ConquestPump& pu : info.conquest_pumps) {
            if (!render::pump_frame_applies(pfs, pu.surface)) continue;
            const glm::dvec3 fr_up = glm::normalize(pu.pos);
            // Cube basis. The "forward" axis is the underground RUN (this pump
            // toward the far tunnel mouth), projected off local-up: a frame
            // aligned with the chamber the pilot flies through reads as
            // deliberate, where a world-axis cube would look randomly tilted.
            // Never a cached or fixed world axis — recomputed from pu.pos each
            // frame, with a spanned fallback if the reference degenerates
            // (the "mutually-perpendicular candidates" rule, S7-cam P3).
            const glm::dvec3 ref = glm::normalize(world::kTunnelMouthMurray) +
                                   glm::normalize(world::kTunnelMouthErrington);
            glm::dvec3 fr_fwd = ref - glm::dot(ref, fr_up) * fr_up;
            if (glm::length(fr_fwd) < 1e-3) {
                // Degenerate: span from a stable non-parallel candidate pair.
                const glm::dvec3 alt = (std::fabs(fr_up.x) < 0.9)
                                           ? glm::dvec3{1.0, 0.0, 0.0}
                                           : glm::dvec3{0.0, 1.0, 0.0};
                fr_fwd = alt - glm::dot(alt, fr_up) * fr_up;
            }
            fr_fwd = glm::normalize(fr_fwd);
            const glm::dvec3 fr_right =
                glm::normalize(glm::cross(fr_fwd, fr_up));
            const glm::dvec3 fcol = render::pump_frame_color(
                pfs, pu.faction, info.conquest_player_faction, pu.alive,
                render::team_colors().ally, render::team_colors().enemy);
            for (int L = 0; L < pfs.layers; ++L) {
                const double h = pfs.half_extent_m -
                                 static_cast<double>(L) * pfs.layer_step_m;
                // Inner shells are the hot core, outer ones the falloff halo;
                // additively they compose into a glow with a bright centre.
                const double w =
                    pfs.brightness /
                    (1.0 + static_cast<double>(L) * static_cast<double>(L));
                const auto q = [](double v) {
                    return static_cast<unsigned char>(
                        std::lround(std::min(1.0, std::max(0.0, v)) * 255.0));
                };
                const Color lc{q(fcol.x * w), q(fcol.y * w), q(fcol.z * w),
                               255};
                // 8 corners in the pump's own frame.
                Vector3 c[8];
                for (int i = 0; i < 8; ++i) {
                    const double sx = (i & 1) ? h : -h;
                    const double sy = (i & 2) ? h : -h;
                    const double sz = (i & 4) ? h : -h;
                    c[i] =
                        rel(pu.pos + fr_right * sx + fr_up * sy + fr_fwd * sz,
                            pose.eye);
                }
                static const int kEdges[12][2] = {
                    {0, 1}, {2, 3}, {4, 5}, {6, 7},   // along right
                    {0, 2}, {1, 3}, {4, 6}, {5, 7},   // along up
                    {0, 4}, {1, 5}, {2, 6}, {3, 7}};  // along fwd
                for (const auto& e : kEdges) DrawLine3D(c[e[0]], c[e[1]], lc);
            }
        }
        rlEnableDepthMask();
        EndBlendMode();
    }
    pmark("lamps");
    // CC1 Superstack smoke: a mono alpha plume off the stack top. After the
    // opaque depth-writers (terrain occludes the base) and the additive lamps;
    // manages its own alpha blend + depth-write-off + back-to-front puff sort.
    // Phase is the FINISHED wrapped scalar the app derived from t_cel (render/
    // reads no clock).
    draw_smoke(pose.eye, info.smoke_phase);
    pmark("smoke");
    // Precipitation (W3): season-gated snow/rain, only under an active W2
    // weather cell. World-anchored jittered lattice, box follows the eye, fall
    // along local_up ONLY (no wind). Alpha, depth-test on / write off; after
    // the opaque bodies + smoke, before the prop discs. Phase + intensity are
    // app-owned.
    // AS-1: the underground gate needs BOTH the tunnel SDF and the local terrain
    // radius (a negative tunnel SDF alone also covers open air over the shallow
    // arena). Same single-source terrain query the brace-for-impact AGL read
    // above uses -- world::HeightField::radius_at -- so the snow and the ground
    // can never disagree about where the surface is.
    {
        const double r_eye_p = glm::length(pose.eye);
        const glm::dvec3 up_eye =
            r_eye_p > 0.0 ? pose.eye / r_eye_p : glm::dvec3(0, 1, 0);
        const double precip_ground_r =
            (env != nullptr && env->ground != nullptr)
                ? env->ground->radius_at(up_eye)
                : params.R;
        draw_precip(pose, info.season, info.precip_intensity, info.precip_phase,
                    info.precip_phase_far, info.tunnel_net, precip_ground_r);
    }
    pmark("precip");
    // Translucent prop pass: ALL blur discs AFTER ALL opaque bodies (Fable C6),
    // alpha-blended with depth-WRITE off so a farther body can't overwrite a
    // nearer disc and overlapping discs composite.
    BeginBlendMode(BLEND_ALPHA);
    rlDisableDepthMask();
    draw_prop(state, pose.eye, fp);
    for (std::size_t di = 0; di < info.drones_draw.size(); ++di) {
        if (di < info.drones_alive.size() && info.drones_alive[di] == 0)
            continue;  // CONQUEST wreck: no prop
        draw_prop(info.drones_draw[di], pose.eye, fp, info.drone_scale);
    }
    rlEnableDepthMask();
    EndBlendMode();
    // MB-7c (iii): wingtip vortex trails — fading world-space streamers off
    // the tips near stall AoA / high G. Read-only cosmetic overlay; points
    // are re-based to the eye at draw time (the double->float seam).
    if (info.vortices != nullptr) {
        // Batched: every trail segment becomes ONE thin camera-facing quad in a
        // shared grow-only mesh (was up to ~478 immediate-mode DrawLine3D calls
        // per frame during a sustained high-G pull). One UpdateMeshBuffer + one
        // DrawMesh renders both tips. Per-segment color/alpha/fade preserved.
        // Line width: DrawLine3D is a screen-constant 1 px; here the quad half-
        // width is derived from the segment-midpoint view distance and the FOV
        // so it reads ~1.4 px at any range (a hair wider than 1 px so a thin
        // streak stays legible instead of shimmering to sub-pixel; kVortexLineW
        // below). Same state the old line block inherited from the prop pass:
        // alpha blend, depth-TEST on, depth-WRITE on.
        std::size_t vneed = 0;
        if (info.vortices->left.size() > 1)
            vneed += info.vortices->left.size() - 1;
        if (info.vortices->right.size() > 1)
            vneed += info.vortices->right.size() - 1;
        if (vneed > 0) {
            ensure_quad_batch(g_vortex_quads, static_cast<int>(vneed));
            g_vortex_quads.used_quads = 0;
            constexpr float kVortexLineW = 1.4f;  // target on-screen px width
            const float tan_half =
                std::tan(static_cast<float>(cam.fovy) * 0.5f * PI / 180.0f);
            const float inv_h =
                1.0f / static_cast<float>(std::max(1, GetScreenHeight()));
            const float wpx =
                kVortexLineW * tan_half * inv_h;  // half-w per unit dist
            const auto build_trail = [&](const std::deque<VortexPoint>& tr) {
                for (std::size_t i = 1; i < tr.size(); ++i) {
                    const VortexPoint& a = tr[i - 1];
                    const VortexPoint& b = tr[i];
                    // Break across respawn/teleport gaps.
                    if (glm::length(b.pos - a.pos) > 30.0) continue;
                    const double fade =
                        b.strength * (1.0 - b.age / kVortexLife);
                    if (fade <= 0.0) continue;
                    const unsigned char al =
                        static_cast<unsigned char>(200.0 * fade);
                    const Color c{225, 240, 255, al};
                    const Vector3 A = rel(a.pos, pose.eye);
                    const Vector3 B = rel(b.pos, pose.eye);
                    const glm::vec3 Av(A.x, A.y, A.z), Bv(B.x, B.y, B.z);
                    const glm::vec3 seg = Bv - Av;
                    const float seglen = glm::length(seg);
                    if (seglen < 1e-6f) continue;
                    const glm::vec3 mid = 0.5f * (Av + Bv);
                    const float dmid = glm::length(mid);
                    if (dmid < 1e-6f) continue;
                    const glm::vec3 view = mid / dmid;  // eye->midpoint dir
                    glm::vec3 perp = glm::cross(seg / seglen, view);
                    const float pl = glm::length(perp);
                    if (pl < 1e-6f) continue;  // segment end-on to eye: skip
                    perp /= pl;
                    const glm::vec3 off =
                        perp * (wpx * dmid);  // half-width world
                    const Vector3 p0{Av.x + off.x, Av.y + off.y, Av.z + off.z};
                    const Vector3 p1{Bv.x + off.x, Bv.y + off.y, Bv.z + off.z};
                    const Vector3 p2{Bv.x - off.x, Bv.y - off.y, Bv.z - off.z};
                    const Vector3 p3{Av.x - off.x, Av.y - off.y, Av.z - off.z};
                    push_quad(g_vortex_quads, p0, p1, p2, p3, c);
                }
            };
            build_trail(info.vortices->left);
            build_trail(info.vortices->right);
            if (g_vortex_quads.used_quads > 0) {
                // Preserve the old DrawLine3D state exactly: alpha blend, depth
                // test on, depth WRITE ON (inherited from the prop pass's
                // rlEnableDepthMask + EndBlendMode) — made explicit here.
                BeginBlendMode(BLEND_ALPHA);
                flush_quad_batch(g_vortex_quads);
                EndBlendMode();
            }
        }
    }
    // Feature A halo (Halo mode): a 3D RAINBOW SEMICIRCLE ARC lying in the WING
    // plane (normal = the aircraft's body-up), so it BANKS WITH THE AIRCRAFT
    // and reads as an arc in perspective. The arc spans 180deg with its MIDDLE
    // on the nose direction (+forward), so the nose points at the centre of the
    // arc. World-space; the radius grows mildly with distance so it downscales
    // ~25% LESS than pure perspective (stays visible from afar). Additive +
    // whitened.
    if (info.plane_viz == render::PlaneViz::Halo) {
        const glm::dvec3 n =
            glm::normalize(state.orientation * glm::dvec3(0.0, 1.0, 0.0));
        const glm::dvec3 u =
            glm::normalize(state.orientation * glm::dvec3(1.0, 0.0, 0.0));
        // v = cross(body_up, body_right) = -Z = the NOSE (forward) direction,
        // so the arc's apex (ang = 90deg) sits exactly on the nose.
        const glm::dvec3 v = glm::normalize(glm::cross(n, u));
        const double dist = glm::length(state.position - pose.eye);
        // Distance law: grow the arc's RADIUS *and* TUBE THICKNESS with
        // distance (sc = (d/35)^0.6), so it downscales less than perspective
        // (stays visible) but not so little it looks huge from afar, AND the
        // tube never thins to a sub-pixel broken-up thread.
        const double sc = std::pow(std::max(1.0, dist) / 35.0, 0.6);
        const double R = 11.0 * sc;
        // Opacity RISES with distance so the arc reads from far away; a touch
        // more in freelook (dollied-back views).
        float dist_boost = static_cast<float>(
            std::min(2.6, std::sqrt(std::max(1.0, dist / 34.0))));
        if (info.freelook) dist_boost *= 1.2f;
        const int N = 48;  // segments across the 180deg arc
        // The SPECTRUM runs across the arc's WIDTH as concentric bands (red
        // ..violet stacked radially), and EACH band runs the WHOLE length of
        // the arc — like a real rainbow. Outer loop = band (one colour, one
        // radius), inner loop = the arc sweep.
        const int Nb = 28;                   // many thin bands => smooth blend
        const double band_total = 2.6 * sc;  // radial width of the rainbow (m)
        const float band_tube =
            static_cast<float>((band_total / (Nb - 1)) * 1.15);  // overlap
        const auto hue = [](float f) -> glm::vec3 {
            const float hh = f * 6.0f;
            const float xx = 1.0f - std::fabs(std::fmod(hh, 2.0f) - 1.0f);
            if (hh < 1) return {1, xx, 0};
            if (hh < 2) return {xx, 1, 0};
            if (hh < 3) return {0, 1, xx};
            if (hh < 4) return {0, xx, 1};
            if (hh < 5) return {xx, 0, 1};
            return {1, 0, xx};
        };
        // CONTRAST-ADAPTIVE rainbow, two passes: an ADDITIVE pass adds light so
        // the arc shows bright/white against the DARK sky; a MULTIPLY pass
        // darkens so it shows as a DARK rainbow against a LIGHT background. The
        // bands overlap and are numerous, so the spectrum blends smoothly.
        for (int pass = 0; pass < 2; ++pass) {
            const bool mult = (pass == 1);
            BeginBlendMode(mult ? BLEND_MULTIPLIED : BLEND_ADDITIVE);
            rlDisableDepthMask();
            for (int k = 0; k < Nb; ++k) {
                const float frac = static_cast<float>(k) / (Nb - 1);
                const glm::vec3 base = hue(frac * 0.78f);  // red..violet
                const double Rk = R + (frac - 0.5) * band_total;
                glm::dvec3 prev(0.0);
                for (int i = 0; i <= N; ++i) {
                    const double ang = (PI * i) / N;  // 180deg semicircle
                    const glm::dvec3 p =
                        state.position +
                        (std::cos(ang) * u + std::sin(ang) * v) * Rk;
                    if (i > 0) {
                        const float h = (static_cast<float>(i) - 0.5f) / N;
                        const float posfade = std::pow(
                            std::sin(h * static_cast<float>(PI)), 0.7f);
                        glm::vec3 rgb;
                        unsigned char al;
                        if (!mult) {
                            // Whitened + additive => bright white-ish rainbow
                            // on black; alpha carries the
                            // apex-bright/ends-fade. Lowered whitening: 0.25
                            // (was 0.45) — less milky white blended in, a purer
                            // band color.
                            rgb = glm::mix(base, glm::vec3(1.0f), 0.25f);
                            // Four 25% cuts from the original 24 (24*0.75^4).
                            al = static_cast<unsigned char>(std::max(
                                0.0f, std::min(255.0f, 7.59375f * posfade *
                                                           dist_boost)));
                        } else {
                            // Multiply toward WHITE at ends (no-op) and toward
                            // the band colour at the apex => darkens a light
                            // background into a dark rainbow. Alpha is ignored
                            // by BLEND_MULTIPLIED, so the fade rides in the
                            // colour. Four 25% cuts from the original 0.24
                            // (*0.75^4).
                            rgb = glm::mix(glm::vec3(1.0f), base,
                                           posfade * 0.0759375f);
                            al = 255;
                        }
                        const Color c{
                            static_cast<unsigned char>(rgb.x * 255.0f),
                            static_cast<unsigned char>(rgb.y * 255.0f),
                            static_cast<unsigned char>(rgb.z * 255.0f), al};
                        DrawCylinderEx(rel(prev, pose.eye), rel(p, pose.eye),
                                       band_tube, band_tube, 5, c);
                    }
                    prev = p;
                }
            }
            rlEnableDepthMask();
            EndBlendMode();
        }
    }
    // Feature B: dual wingtip smoke — a CONTINUOUS tapered tube (not discrete
    // balls). Consecutive puffs are connected by tapered cylinders so the wake
    // reads as one flowing contrail; a wide faint outer tube + a denser inner
    // core give soft edges. NEON ORANGE, "like bright hot slag" (Chad
    // 2026-08-06 — the old orange-cooling-to-soot-with-black-slag read was
    // hard to see): ADDITIVE blend so the trail GLOWS, the body stays hot
    // red-orange over its whole life instead of cooling to soot, and the slag
    // flecks are the BRIGHTEST segments (white-hot pour, not black clinker).
    // Depth-TEST on (terrain occludes), depth-WRITE off. READ-ONLY:
    // no clock (age passed in), turbulence is a pure function of the puff seed.
    if (info.wingtip_smoke != nullptr) {
        BeginBlendMode(BLEND_ADDITIVE);
        rlDisableDepthMask();
        // ★★★ THE THREE STOPS COME FROM THE TEAM KIT (render/team_kit.h),
        // NOT FROM LITERALS HERE. Valley is these exact three values, moved
        // into the kit byte for byte; Central City is the same ramp rotated
        // 180 deg in hue (Chad 2026-09-10: "smoke blue for plane"). A literal
        // left here would be a fourth authority on the player's colour and
        // would silently disagree with his plane the day a side is added.
        const TeamKit kit = player_kit();
        const glm::vec3 fresh = glm::vec3(kit.smoke_fresh);
        const glm::vec3 aged = glm::vec3(kit.smoke_aged);
        const glm::vec3 slag = glm::vec3(kit.smoke_slag);
        // LOW-frequency drift (sin of the seed) so neighbouring puffs curl
        // together into a smooth wave, not an independent-jitter zigzag.
        const auto center = [&](const WingtipSmokePuff& p, float lf) {
            const float s = static_cast<float>(p.seed);
            // Low-freq curl (smooth wave) + a higher-freq DISTORTION so the
            // tube warps and neighbouring segments blend together instead of
            // stacking into clean discs (the "balls"). Both grow with age.
            const glm::dvec3 d{std::sin(s * 0.35f), std::sin(s * 0.31f + 1.7f),
                               std::sin(s * 0.29f + 3.3f)};
            const glm::dvec3 d2{std::sin(s * 1.7f + 0.5f),
                                std::sin(s * 1.9f + 2.1f),
                                std::sin(s * 1.3f + 4.0f)};
            return p.pos + (d * 3.0 + d2 * 1.1) * static_cast<double>(lf);
        };
        // SMOOTH radius (age only, no per-puff size jitter) so the tube is an
        // even stream, not a lumpy string of bulges that reads as balls.
        const auto radius = [&](const WingtipSmokePuff&, float lf) {
            return static_cast<float>(kSmokeR0 + kSmokeRGrow * lf);
        };
        const auto draw_smoke_side =
            [&](const std::vector<WingtipSmokePuff>& puffs) {
                for (std::size_t i = 1; i < puffs.size(); ++i) {
                    const WingtipSmokePuff& a = puffs[i - 1];
                    const WingtipSmokePuff& b = puffs[i];
                    const float la = static_cast<float>(a.age / kSmokeLife);
                    const float lb = static_cast<float>(b.age / kSmokeLife);
                    const glm::dvec3 ca = center(a, la), cb = center(b, lb);
                    if (glm::length(cb - ca) > 15.0) continue;  // break gaps
                    const float lf = 0.5f * (la + lb);
                    const float env = (1.0f - lf) * (1.0f - lf);
                    const bool is_slag = smoke_hash(b.seed, 5) < 0.28f;
                    const glm::vec3 col =
                        is_slag ? slag : glm::mix(fresh, aged, lf);
                    const auto tint = [&](float amul) {
                        return Color{static_cast<unsigned char>(col.x * 255.0f),
                                     static_cast<unsigned char>(col.y * 255.0f),
                                     static_cast<unsigned char>(col.z * 255.0f),
                                     static_cast<unsigned char>(std::max(
                                         0.0f, std::min(255.0f, amul * env)))};
                    };
                    const Vector3 A = rel(ca, pose.eye), B = rel(cb, pose.eye);
                    const float ra = radius(a, la), rb = radius(b, lb);
                    // Alphas raised for the additive neon read (were 20/52
                    // under alpha-blend — faint by design when the trail was
                    // meant to read as smoke; now it reads as a pour).
                    DrawCylinderEx(A, B, ra * 1.5f, rb * 1.5f, 8, tint(36.0f));
                    DrawCylinderEx(A, B, ra, rb, 8, tint(110.0f));
                }
            };
        draw_smoke_side(info.wingtip_smoke->left);
        draw_smoke_side(info.wingtip_smoke->right);
        rlEnableDepthMask();
        EndBlendMode();
    }
    // Feature A halo (Halo mode): a 3D RAINBOW SEMICIRCLE ARC lying in the WING
    // plane (normal = the aircraft's body-up), so it BANKS WITH THE AIRCRAFT
    // and reads as an arc in perspective. The arc spans 180deg with its MIDDLE
    // on the nose direction (+forward), so the nose points at the centre of the
    // arc. World-space; the radius grows mildly with distance so it downscales
    // ~25% LESS than pure perspective (stays visible from afar). Additive +
    // whitened.
    if (info.plane_viz == render::PlaneViz::Halo) {
        const glm::dvec3 n =
            glm::normalize(state.orientation * glm::dvec3(0.0, 1.0, 0.0));
        const glm::dvec3 u =
            glm::normalize(state.orientation * glm::dvec3(1.0, 0.0, 0.0));
        // v = cross(body_up, body_right) = -Z = the NOSE (forward) direction,
        // so the arc's apex (ang = 90deg) sits exactly on the nose.
        const glm::dvec3 v = glm::normalize(glm::cross(n, u));
        const double dist = glm::length(state.position - pose.eye);
        // Distance law: grow the arc's RADIUS *and* TUBE THICKNESS with
        // distance (sc = (d/35)^0.6), so it downscales less than perspective
        // (stays visible) but not so little it looks huge from afar, AND the
        // tube never thins to a sub-pixel broken-up thread.
        const double sc = std::pow(std::max(1.0, dist) / 35.0, 0.6);
        const double R = 11.0 * sc;
        // Opacity RISES with distance so the arc reads from far away; a touch
        // more in freelook (dollied-back views).
        float dist_boost = static_cast<float>(
            std::min(2.6, std::sqrt(std::max(1.0, dist / 34.0))));
        if (info.freelook) dist_boost *= 1.2f;
        const int N = 48;  // segments across the 180deg arc
        // The SPECTRUM runs across the arc's WIDTH as concentric bands (red
        // ..violet stacked radially), and EACH band runs the WHOLE length of
        // the arc — like a real rainbow. Outer loop = band (one colour, one
        // radius), inner loop = the arc sweep.
        const int Nb = 28;                   // many thin bands => smooth blend
        const double band_total = 2.6 * sc;  // radial width of the rainbow (m)
        const float band_tube =
            static_cast<float>((band_total / (Nb - 1)) * 1.15);  // overlap
        const auto hue = [](float f) -> glm::vec3 {
            const float hh = f * 6.0f;
            const float xx = 1.0f - std::fabs(std::fmod(hh, 2.0f) - 1.0f);
            if (hh < 1) return {1, xx, 0};
            if (hh < 2) return {xx, 1, 0};
            if (hh < 3) return {0, 1, xx};
            if (hh < 4) return {0, xx, 1};
            if (hh < 5) return {xx, 0, 1};
            return {1, 0, xx};
        };
        // CONTRAST-ADAPTIVE rainbow, two passes: an ADDITIVE pass adds light so
        // the arc shows bright/white against the DARK sky; a MULTIPLY pass
        // darkens so it shows as a DARK rainbow against a LIGHT background. The
        // bands overlap and are numerous, so the spectrum blends smoothly.
        for (int pass = 0; pass < 2; ++pass) {
            const bool mult = (pass == 1);
            BeginBlendMode(mult ? BLEND_MULTIPLIED : BLEND_ADDITIVE);
            rlDisableDepthMask();
            for (int k = 0; k < Nb; ++k) {
                const float frac = static_cast<float>(k) / (Nb - 1);
                const glm::vec3 base = hue(frac * 0.78f);  // red..violet
                const double Rk = R + (frac - 0.5) * band_total;
                glm::dvec3 prev(0.0);
                for (int i = 0; i <= N; ++i) {
                    const double ang = (PI * i) / N;  // 180deg semicircle
                    const glm::dvec3 p =
                        state.position +
                        (std::cos(ang) * u + std::sin(ang) * v) * Rk;
                    if (i > 0) {
                        const float h = (static_cast<float>(i) - 0.5f) / N;
                        const float posfade = std::pow(
                            std::sin(h * static_cast<float>(PI)), 0.7f);
                        glm::vec3 rgb;
                        unsigned char al;
                        if (!mult) {
                            // Whitened + additive => bright white-ish rainbow
                            // on black; alpha carries the
                            // apex-bright/ends-fade.
                            rgb = glm::mix(base, glm::vec3(1.0f), 0.45f);
                            al = static_cast<unsigned char>(std::max(
                                0.0f, std::min(255.0f,
                                               24.0f * posfade * dist_boost)));
                        } else {
                            // Multiply toward WHITE at ends (no-op) and toward
                            // the band colour at the apex => darkens a light
                            // background into a dark rainbow. Alpha is ignored
                            // by BLEND_MULTIPLIED, so the fade rides in the
                            // colour.
                            rgb = glm::mix(glm::vec3(1.0f), base,
                                           posfade * 0.24f);
                            al = 255;
                        }
                        const Color c{
                            static_cast<unsigned char>(rgb.x * 255.0f),
                            static_cast<unsigned char>(rgb.y * 255.0f),
                            static_cast<unsigned char>(rgb.z * 255.0f), al};
                        DrawCylinderEx(rel(prev, pose.eye), rel(p, pose.eye),
                                       band_tube, band_tube, 5, c);
                    }
                    prev = p;
                }
            }
            rlEnableDepthMask();
            EndBlendMode();
        }
    }
    // Tracer bolts (rig-D guns): a glowing comet per active projectile — a soft
    // wide outer glow, a hot tapered core, and a white-hot head — all ADDITIVE
    // (hot emitters that punch through scene luminance). Radius scales with
    // view distance (near-constant screen size, floored) so a bolt reads at 20
    // m off the muzzle AND at 500 m downrange, where a fixed world size would
    // vanish to sub-pixel. Streak points backward along -vel; the stream of
    // consecutive rounds traces the arc. Color by kind (20 mm warmer + bigger
    // than 7.92); alpha burns out over the last 30% of tracer lifetime.
    // Depth-TEST on (terrain occludes), depth-WRITE off (bolts composite, never
    // occlude). READ-ONLY: no clock (age is tick-derived, passed in), no
    // writes.
    if ((info.projectiles != nullptr || info.enemy_projectiles != nullptr ||
         info.flak_projectiles != nullptr ||
         !info.flak_ai_projectiles.empty() ||
         info.cosmetic_projectiles != nullptr) &&
        info.tracer_lifetime_s > 0.0) {
        BeginBlendMode(BLEND_ADDITIVE);
        rlDisableDepthMask();
        const double burn_start = info.tracer_lifetime_s * 0.70;
        // Draw a whole projectile pool as comet tracers. `enemy` forces the
        // hostile tint (bandit combat AI); player rounds tint by kind. The body
        // is otherwise identical for every pool (one code path, single-source).
        // tint_mode: 0 = player pool (tint by round kind); 1 = the enemy pool,
        // FORCED to the hostile tint (legacy, bit-identical); 2 = RUNG S3-GUNS
        // the cosmetic pool, tinted PER ROUND off Projectile::friendly — this
        // pool carries allied AI-vs-AI fire as well as hostile, and allied fire
        // drawn as incoming reads as a bug on screen.
        //
        // FLAK STAGE B: `flak_pool` swaps in the flak_tracer_* config family
        // (redder, wider, LONGER, brighter) so the ground gun's fire reads as
        // something being WALKED onto a target rather than as more of the
        // fighter's rounds. It is four parameters on one shared path -- the
        // aircraft look is untouched, and with the shipped multipliers the
        // plane's own arithmetic is bit-identical (x 1.0 is exact in IEEE).
        //
        // ★ MERGE NOTE (2026-09-02, flak -> main): main refactored this
        // lambda's `bool enemy` into `int tint_mode` (S3-GUNS' cosmetic pool)
        // in the same window the flak lane added `flak_pool`. They are
        // ORTHOGONAL -- tint_mode picks WHOSE round it is, flak_pool picks
        // WHICH LOOK the pool draws with -- so both survive, and the flak
        // pools pass tint_mode 0 (their tint comes from flak_pool, not from
        // the round kind).
        const auto draw_pool = [&](const std::vector<weapon::Projectile>& pool,
                                   int tint_mode, bool flak_pool = false) {
            const float lum_mult = flak_pool ? info.flak_tracer_lum_mult : 1.0f;
            const double len_m =
                info.tracer_len_m *
                (flak_pool ? static_cast<double>(info.flak_tracer_len_mult)
                           : 1.0);
            for (const weapon::Projectile& p : pool) {
                if (!p.active) continue;
                const bool enemy =
                    tint_mode == 1 || (tint_mode == 2 && !p.friendly);
                if (p.age >= info.tracer_lifetime_s) continue;
                const double spd = glm::length(p.vel);
                if (spd < 1e-6) continue;
                const double fade =
                    p.age < burn_start
                        ? 1.0
                        : 1.0 - (p.age - burn_start) /
                                    (info.tracer_lifetime_s - burn_start);
                const float f = static_cast<float>(std::max(0.0, fade));
                const bool cannon = (p.kind == weapon::Round::Cannon20mm);
                const glm::vec3& tint =
                    flak_pool ? info.flak_tracer_rgb
                              : (enemy ? info.tracer_enemy_rgb
                                       : (cannon ? info.tracer_cannon_rgb
                                                 : info.tracer_mg_rgb));
                const float base = flak_pool
                                       ? info.flak_tracer_base
                                       : (enemy ? info.tracer_enemy_base
                                                : (cannon
                                                       ? info.tracer_cannon_base
                                                       : info.tracer_mg_base));
                const glm::dvec3 dir = p.vel / spd;
                // Angular (screen-constant) sizing from config — no bare
                // numbers.
                const double dist = glm::length(p.pos - pose.eye);
                if (dist < 1e-6)
                    continue;  // round at the eye: no billboard basis
                const double r_core =
                    std::max(static_cast<double>(info.tracer_r_core_min),
                             info.tracer_r_core_frac * dist) *
                    base;
                const double r_glow = r_core * info.tracer_r_glow_mult;
                // ★★★ THE STREAK MAY NOT OUTRUN THE ROUND. Clamped to the
                // distance actually flown (combat::tracer_tail_m) -- without
                // it a fresh round paints its whole 16.8 m core / 28.6 m glow
                // backwards THROUGH the muzzle, which on a ground gun is a
                // second tracer fired at the gunner's own face (Chad,
                // 2026-08-31). On an aeroplane the same overhang was always
                // there and always clipped behind the camera.
                const double core_m =
                    combat::tracer_tail_m(len_m, spd, p.age);
                const double glow_m = combat::tracer_tail_m(
                    len_m * info.tracer_glow_len_mult, spd, p.age);
                const Vector3 head = rel(p.pos, pose.eye);
                const Vector3 tail_core = rel(p.pos - dir * core_m, pose.eye);
                const Vector3 tail_glow = rel(p.pos - dir * glow_m, pose.eye);

                // Slag modulation: deterministic dark flecks along the comet
                // body. Uses p.age and projectile world position for variation
                // — NO wall-clock. A sine over world-space position along the
                // comet axis produces dark "chunks" at a fixed world period
                // (tracer_slag_period_m), simulating cooled black slag breaking
                // up the glow. slag=0 → no fleck, slag=slag_dark → black fleck.
                // Period is config-driven (no bare numbers).
                const double slag_phase =
                    (info.tracer_slag_period_m > 0.0f)
                        ? std::fmod(
                              std::abs(glm::dot(p.pos, dir)) +
                                  static_cast<double>(p.age) * spd,
                              static_cast<double>(info.tracer_slag_period_m)) /
                              info.tracer_slag_period_m
                        : 0.0;
                // Map slag_phase [0,1] -> a fleck weight in [0, slag_dark]:
                // sin²(π*phase) peaks near the middle of each period, dips to
                // ~0 at the edges. This gives bright-core streaks with dark
                // inter-fleck gaps.
                const double sin_v =
                    std::sin(slag_phase * 3.14159265358979323846);
                const float slag_weight = static_cast<float>(
                    info.tracer_slag_dark * (1.0 - sin_v * sin_v));
                // Luminance scale: hot glow is dimmed proportionally by slag
                // fraction.
                const float slag_lum_scale = 1.0f - slag_weight;

                const auto C = [&](float lum, float a) {
                    const float l = lum * slag_lum_scale * lum_mult;
                    return Color{static_cast<unsigned char>(
                                     std::min(255.0f, tint.r * 255.0f * l)),
                                 static_cast<unsigned char>(
                                     std::min(255.0f, tint.g * 255.0f * l)),
                                 static_cast<unsigned char>(
                                     std::min(255.0f, tint.b * 255.0f * l)),
                                 static_cast<unsigned char>(
                                     std::min(255.0f, 255.0f * a * f))};
                };
                // CPU billboard basis: perpendicular to BOTH the eye ray and
                // the streak, so the flat quad faces the camera and runs along
                // the comet. Degenerate only when the streak is end-on to the
                // eye (cross ≈ 0) — then any perpendicular of dir will do
                // (the streak reads as a near-dot, orientation irrelevant).
                const glm::dvec3 vdir = (p.pos - pose.eye) / dist;
                glm::dvec3 perp = glm::cross(vdir, dir);
                const double pl = glm::length(perp);
                if (pl < 1e-6) {
                    const glm::dvec3 seed = std::abs(dir.x) < 0.9
                                                ? glm::dvec3(1.0, 0.0, 0.0)
                                                : glm::dvec3(0.0, 1.0, 0.0);
                    perp = glm::normalize(glm::cross(dir, seed));
                } else {
                    perp /= pl;
                }
                const glm::vec3 pf(perp);
                const glm::vec3 uf(glm::cross(vdir, perp));  // head-square axis
                // A tapered streak quad: 0 width at the tail, `wh` at the head
                // (reproduces DrawCylinderEx startRadius 0 -> endRadius r as a
                // flat billboard). The tail edge collapses to a point, so it is
                // one visible triangle (the second index-tri is degenerate).
                const auto streak = [&](const Vector3& T, const Vector3& H,
                                        float wh, const Color& c) {
                    const Vector3 p0{H.x + pf.x * wh, H.y + pf.y * wh,
                                     H.z + pf.z * wh};
                    const Vector3 p3{H.x - pf.x * wh, H.y - pf.y * wh,
                                     H.z - pf.z * wh};
                    push_tracer_quad(p0, T, T, p3, c);
                };
                // Comet layers: glow → core → head (all config-driven).
                streak(tail_glow, head, static_cast<float>(r_glow),
                       C(info.tracer_glow_lum, info.tracer_glow_alpha));
                streak(tail_core, head, static_cast<float>(r_core),
                       C(info.tracer_core_lum, info.tracer_core_alpha));
                // White-hot head: a small camera-facing square (was
                // DrawSphere).
                {
                    const float rh =
                        static_cast<float>(r_core * info.tracer_head_r_mult);
                    const Vector3 h0{head.x + (pf.x + uf.x) * rh,
                                     head.y + (pf.y + uf.y) * rh,
                                     head.z + (pf.z + uf.z) * rh};
                    const Vector3 h1{head.x + (pf.x - uf.x) * rh,
                                     head.y + (pf.y - uf.y) * rh,
                                     head.z + (pf.z - uf.z) * rh};
                    const Vector3 h2{head.x + (-pf.x - uf.x) * rh,
                                     head.y + (-pf.y - uf.y) * rh,
                                     head.z + (-pf.z - uf.z) * rh};
                    const Vector3 h3{head.x + (-pf.x + uf.x) * rh,
                                     head.y + (-pf.y + uf.y) * rh,
                                     head.z + (-pf.z + uf.z) * rh};
                    push_tracer_quad(
                        h0, h1, h2, h3,
                        C(info.tracer_head_lum, info.tracer_head_alpha));
                }
            }
        };  // draw_pool
        // Size (grow-only) the shared mesh for both pools' live capacity, then
        // fill it from the pure pool iteration (lifetime logic untouched).
        std::size_t need = 0;
        if (info.projectiles != nullptr) need += info.projectiles->size();
        if (info.flak_projectiles != nullptr)
            need += info.flak_projectiles->size();
        for (const std::vector<weapon::Projectile>* aip :
             info.flak_ai_projectiles)
            if (aip != nullptr) need += aip->size();
        if (info.enemy_projectiles != nullptr)
            need += info.enemy_projectiles->size();
        if (info.cosmetic_projectiles != nullptr)
            need += info.cosmetic_projectiles->size();
        if (need > 0) ensure_tracer_mesh(static_cast<int>(need));
        g_tracers.used_quads = 0;
        if (g_tracers.ok) {
            if (info.projectiles != nullptr) draw_pool(*info.projectiles, 0);
            if (info.flak_projectiles != nullptr)
                draw_pool(*info.flak_projectiles, 0,
                          /*flak_pool=*/true);  // FLAK: its OWN tracer look
            // STAGE D: the AI guns' streams, same look, same path.
            for (const std::vector<weapon::Projectile>* aip :
                 info.flak_ai_projectiles)
                if (aip != nullptr) draw_pool(*aip, 0, /*flak_pool=*/true);
            if (info.enemy_projectiles != nullptr)
                draw_pool(*info.enemy_projectiles, 1);
            if (info.cosmetic_projectiles != nullptr)
                draw_pool(*info.cosmetic_projectiles, 2);
        }
        // One upload + one DrawMesh for the whole pool (skip if nothing live).
        if (g_tracers.ok && g_tracers.used_quads > 0) {
            const int used_verts = g_tracers.used_quads * 4;
            UpdateMeshBuffer(g_tracers.mesh, 0, g_tracers.verts.data(),
                             static_cast<int>(sizeof(float) * 3 * used_verts),
                             0);
            UpdateMeshBuffer(
                g_tracers.mesh, 3, g_tracers.cols.data(),
                static_cast<int>(sizeof(unsigned char) * 4 * used_verts), 0);
            const int full_v = g_tracers.mesh.vertexCount;
            const int full_t = g_tracers.mesh.triangleCount;
            g_tracers.mesh.vertexCount =
                used_verts;  // draw only the used range
            g_tracers.mesh.triangleCount = g_tracers.used_quads * 2;
            // Billboards can wind either way -> culling off; additive + depth
            // test on / write off matches the old immediate-mode block exactly.
            rlDisableBackfaceCulling();
            DrawMesh(g_tracers.mesh, g_tracers.mat, MatrixIdentity());
            rlEnableBackfaceCulling();
            g_tracers.mesh.vertexCount = full_v;
            g_tracers.mesh.triangleCount = full_t;
        }
        rlEnableDepthMask();
        EndBlendMode();
    }

    // Combat FX (kill-loop) + touchdown bursts, age-driven additive.
    // No clock; every sub-particle is a PURE closed-form function of
    // (f.seed, index, f.age, f.energy01) — HitSpark/Explosion sample
    // combat/fx_curves.h (Fable 2026-07-13, the single source for those
    // curves); the R4-FLY-6 Touchdown puff ring is closed-form inline in its
    // branch below (same discipline, no history). This loop only turns the
    // samples into additive spheres/streaks. Mirrors the tracer-comet
    // discipline.
    if (info.combat_fx != nullptr) {
        // linear-RGB [0,1] + a luminance gain -> a clamped raylib Color. The
        // additive blend means brightness rides in both the RGB and the alpha.
        const auto fx_color = [](const glm::dvec3& c, double gain,
                                 double alpha) -> Color {
            const auto ch = [](double v) -> unsigned char {
                return static_cast<unsigned char>(std::clamp(v, 0.0, 1.0) *
                                                  255.0);
            };
            return Color{ch(c.r * gain), ch(c.g * gain), ch(c.b * gain),
                         ch(alpha)};
        };
        BeginBlendMode(BLEND_ADDITIVE);
        rlDisableDepthMask();
        for (const combat::Fx& f : *info.combat_fx) {
            if (!f.active) continue;
            if (f.kind == combat::FxKind::HitSpark) {
                // Spark burst: N sub-sparks sprayed back along the reflected
                // shot direction (f.dir), each a small additive puff whose
                // brightness/size/color follow the pure age curve. Screen-space
                // size floor (tracer discipline) keeps a distant hit legible.
                const int n = combat::spark_count(f.energy01);
                for (int i = 0; i < n; ++i) {
                    const combat::SparkSample s = combat::spark_particle(
                        f.seed, i, f.age, f.energy01, f.dir);
                    if (s.brightness <= 0.0) continue;
                    const glm::dvec3 wpos = f.pos + s.offset;
                    const double dist = glm::length(wpos - pose.eye);
                    const float r = static_cast<float>(
                        std::max(s.size_m,
                                 info.tracer_r_core_frac * dist *
                                     2.0));  // angular floor
                    DrawSphere(rel(wpos, pose.eye), r,
                               fx_color(s.color, s.brightness,
                                        std::min(1.0, s.brightness)));
                }
            } else if (f.kind == combat::FxKind::Touchdown) {
                // R4-FLY-6 touchdown burst ("something to indicate
                // touchdown"): a ring of soft puffs kicked outward at the
                // wheels on the capture tick, tinted by surface (dust /
                // lake splash / winter ice), scaled by touchdown energy
                // (f.energy01) and the [fx] intensity dial. Closed-form in
                // (seed, i, age) like the sparks: no history, no clock.
                const double life = combat::kTouchdownLifetime;
                const double u = std::clamp(f.age / life, 0.0, 1.0);
                const glm::dvec3 up = f.dir;  // spawn passed local_up
                const glm::dvec3 ref = std::abs(up.y) < 0.9
                                           ? glm::dvec3{0.0, 1.0, 0.0}
                                           : glm::dvec3{1.0, 0.0, 0.0};
                const glm::dvec3 t1 = glm::normalize(glm::cross(up, ref));
                const glm::dvec3 t2 = glm::cross(up, t1);
                const glm::dvec3 tint =
                    f.variant == 1   ? glm::dvec3{0.50, 0.68, 0.85}   // splash
                    : f.variant == 2 ? glm::dvec3{0.85, 0.95, 1.00}   // ice
                                     : glm::dvec3{0.55, 0.46, 0.33};  // dust
                const double inten =
                    std::clamp(info.touchdown_fx_intensity, 0.0, 10.0);
                const int n = static_cast<int>(std::lround(
                    (10.0 + 16.0 * f.energy01) * std::min(inten, 2.0)));
                for (int i = 0; i < n; ++i) {
                    // deterministic per-puff params (golden-angle ring + a
                    // seed-hashed jitter — the spark lattice discipline)
                    const double h =
                        0.5 + 0.5 * std::sin(f.seed * 0.7311 + i * 12.9898);
                    const double ang = i * 2.39996 + f.seed * 0.113;
                    // outward kick decaying under the same-ish drag feel as
                    // the debris; splash/ice fly a touch faster than dust
                    const double kick = (2.5 + 7.0 * f.energy01) *
                                        (0.55 + 0.45 * h) *
                                        (f.variant == 0 ? 1.0 : 1.35);
                    const double rise = (0.8 + 2.2 * f.energy01) *
                                        (0.4 + 0.6 * h) * (1.0 - 0.6 * u);
                    const double reach = kick * u * life * (1.0 - 0.45 * u);
                    const glm::dvec3 wpos =
                        f.pos +
                        (std::cos(ang) * t1 + std::sin(ang) * t2) * reach +
                        up * (rise * u * life);
                    const double size = (0.35 + 1.9 * u) *
                                        (0.5 + 0.9 * f.energy01) *
                                        std::min(inten, 2.0);
                    const double b = (1.0 - u) * (1.0 - u) *
                                     (0.35 + 0.45 * f.energy01) *
                                     std::min(inten, 1.5);
                    if (b <= 0.0) continue;
                    const double dist = glm::length(wpos - pose.eye);
                    const float r = static_cast<float>(std::max(
                        size, info.tracer_r_core_frac * dist));  // ang floor
                    DrawSphere(rel(wpos, pose.eye), r,
                               fx_color(tint, b, std::min(1.0, b)));
                }
            } else if (f.kind == combat::FxKind::FlakPuff) {
                // FLAK BURST, additive half: the ~100 ms detonation flash of
                // one 20 mm HE shell. This is the ONLY part of the puff that
                // belongs in an additive pass -- it is genuinely emissive.
                // The smoke ball is DARK and additive blending cannot darken,
                // so it is drawn below in the alpha pass instead (see the
                // FlakPuffState comment in combat/fx_curves.h).
                const combat::FlakPuffState fp2 =
                    combat::flak_puff_state(f.age, f.energy01, f.variant);
                if (fp2.core_brightness > 0.0) {
                    const double dist = glm::length(f.pos - pose.eye);
                    const float r = static_cast<float>(
                        std::max(fp2.core_radius_m,
                                 info.tracer_r_core_frac * dist * 2.0));
                    DrawSphere(rel(f.pos, pose.eye), r,
                               fx_color(fp2.core_color, fp2.core_brightness,
                                        std::min(1.0, fp2.core_brightness)));
                }
            } else {
                // Kill fireball: one saturating-expansion sphere (white-hot ->
                // orange -> dark smoke) ...
                const combat::FireballState fb =
                    combat::fireball_state(f.age, f.energy01);
                if (fb.alpha > 0.0) {
                    DrawSphere(
                        rel(f.pos, pose.eye), static_cast<float>(fb.radius_m),
                        fx_color(fb.color, 0.5 + 0.5 * fb.core_brightness,
                                 fb.alpha));
                }
                // ... plus M debris chunks on a full Fibonacci sphere, each a
                // short base->tip streak (a 50 ms closed-form trail, no history
                // buffer) that arcs under the fx_tick-consistent drag+gravity.
                const glm::dvec3 fpl = f.pos;
                const double up_len = glm::length(fpl);
                const glm::dvec3 up =
                    up_len > 1e-9 ? fpl / up_len : glm::dvec3{0.0, 1.0, 0.0};
                for (int k = 0; k < combat::kDebrisCount; ++k) {
                    const combat::DebrisSample d = combat::debris_particle(
                        f.seed, k, f.age, f.energy01, up);
                    if (d.brightness <= 0.0) continue;
                    const glm::dvec3 tip = f.pos + d.offset;
                    const combat::DebrisSample d0 = combat::debris_particle(
                        f.seed, k, std::max(0.0, f.age - 0.05), f.energy01, up);
                    const glm::dvec3 base = f.pos + d0.offset;  // 50 ms trail
                    DrawLine3D(rel(base, pose.eye), rel(tip, pose.eye),
                               fx_color(d.color, d.brightness, d.brightness));
                }
            }
        }
        rlEnableDepthMask();
        EndBlendMode();
        // FLAK BURST, alpha half (docs/FLAK_GUN_SPEC.md §2.2 item 5). A
        // SECOND pass, in BLEND_ALPHA, because the loop above is ADDITIVE and
        // additive light cannot make a dark thing: a flak puff's entire
        // character is that it is a black-grey stain ON the sky, and drawn
        // additively it would come out as a pale smudge that vanishes over
        // snow. Depth-TESTED (a ridge in front must occlude it) but not
        // depth-WRITING, so overlapping puffs -- the curtain -- composite
        // instead of one punching a hole in the next.
        //
        // Five lobes on the golden-angle lattice, not one sphere: a single
        // ball reads as a solid object, and the whole point of this FX is gas.
        // Closed-form in (seed, i, age) like every other curve in this file;
        // no history, no RNG.
        BeginBlendMode(BLEND_ALPHA);
        rlDisableDepthMask();
        for (const combat::Fx& f : *info.combat_fx) {
            if (!f.active || f.kind != combat::FxKind::FlakPuff) continue;
            const combat::FlakPuffState fp2 =
                combat::flak_puff_state(f.age, f.energy01, f.variant);
            if (fp2.smoke_alpha <= 0.0) continue;
            constexpr int kLobes = 5;
            for (int i = 0; i < kLobes; ++i) {
                const double j = static_cast<double>(f.seed + i);
                const double h1 = combat::fx_h1(j), h2 = combat::fx_h2(j);
                // Fibonacci-sphere direction for the lobe centre, pushed out a
                // fraction of the ball radius so the silhouette is lumpy.
                const double z =
                    1.0 - 2.0 * (static_cast<double>(i) + 0.5) / kLobes;
                const double sr = std::sqrt(std::max(0.0, 1.0 - z * z));
                const double phi = combat::kFxGoldenAngle * j;
                const glm::dvec3 dir{sr * std::cos(phi), z, sr * std::sin(phi)};
                const double off = fp2.smoke_radius_m * (0.20 + 0.35 * h1);
                const glm::dvec3 wpos = f.pos + dir * off;
                const double size = fp2.smoke_radius_m * (0.55 + 0.35 * h2);
                const double dist = glm::length(wpos - pose.eye);
                const float r = static_cast<float>(
                    std::max(size, info.tracer_r_core_frac * dist));
                // Per-lobe alpha well under the ball's, because five
                // overlapping translucent spheres accumulate: the CENTRE
                // (where they all overlap) is what must land at smoke_alpha,
                // not each shell.
                const double a = fp2.smoke_alpha * 0.42;
                DrawSphere(rel(wpos, pose.eye), r,
                           fx_color(fp2.smoke_color, 1.0, a));
            }
        }
        rlEnableDepthMask();
        EndBlendMode();
    }
    // ══ FLAK STAGE B: THE MUZZLE FLASH + THE SNOW BLAST ═════════════════
    // (docs/FLAK_GUN_SPEC.md, the immersion ladder 2/4.) Both are COSMETIC
    // and read-only: two app-stepped envelopes (render/flak_gun.h "STAGE B",
    // fed the undrained spawned_accum exactly like the recoil) turned into
    // geometry here. Closed-form in (shot count, index) -- no clock, no
    // history, no RNG -- the house FX discipline.
    //
    // Drawn HERE and not up with the gun mesh for the P1-1 reason: this is an
    // ADDITIVE glow plus an ALPHA cloud, and either drawn before a later
    // opaque depth-writer gets painted over by anything farther away. Depth
    // TEST stays on (a ridge occludes them); depth WRITE is off.
    //
    // Only the MANNED gun can fire, so only it can carry either effect; a
    // zero envelope draws nothing at all, which is the off-arm.
    // STAGE D: hoisted into a lambda so the AI-manned guns get the IDENTICAL
    // flash + blast off their OWN envelopes (FlakDraw::flash/blast/shot_seq).
    // One code path -- a distant gun's flash cannot drift from the player's.
    const auto draw_flak_muzzle_fx = [&](const FlakDraw& fg, double f_flash,
                                         double f_blast, int f_shot_seq) {
        if (!(f_flash > 0.0 || f_blast > 0.0)) return;
        flak::Stations fst;
        if (flak_stations(fst)) {
            const flak::MountFrame mf =
                flak::make_mount_frame(fg.pos, fg.up, fg.fwd0);
            flak::Pose fps;
            fps.train_rad = fg.train_rad;
            fps.elev_rad = fg.elev_rad;
            const auto fkc = [](const glm::dvec3& c, double gain,
                                double alpha) -> Color {
                const auto ch = [](double v) -> unsigned char {
                    return static_cast<unsigned char>(
                        std::clamp(v, 0.0, 1.0) * 255.0);
                };
                return Color{ch(c.r * gain), ch(c.g * gain), ch(c.b * gain),
                             ch(alpha)};
            };
            // ── MUZZLE FLASH, additive: a white-hot ball at the bell plus a
            // short gas cone down the bore. ★ THE TRUE MUZZLE, not the rest
            // pose: st_muzzle is carried through the train AND elevation
            // kinematics, and because that station is a child of flak_cradle
            // (NOT of the sliding flak_gun node) it does not carry the recoil
            // slide by itself -- so the recoil is subtracted in CRADLE space
            // here, and the flash sits on the metal the player can see.
            const flak::FlashState fl = flak::flash_state(f_flash);
            if (fl.brightness > 0.0) {
                const glm::dvec3 mm =
                    fst.muzzle - glm::dvec3(0.0, 0.0, fg.recoil_m);
                const glm::dvec3 mw =
                    flak::cradle_station_world(mf, fst, fps, mm);
                const glm::dvec3 bore = flak::bore_dir_world(mf, fps);
                BeginBlendMode(BLEND_ADDITIVE);
                rlDisableDepthMask();
                // The cone first, so the ball composites on top of its base.
                // DrawCylinderEx (real geometry -- DrawBillboard renders
                // NOTHING in this engine, the house trap).
                DrawCylinderEx(rel(mw, pose.eye),
                               rel(mw + bore * fl.cone_len_m, pose.eye),
                               static_cast<float>(fl.cone_radius_m), 0.02f, 8,
                               fkc(fl.color, fl.brightness * 0.55,
                                   std::min(1.0, fl.brightness * 0.55)));
                DrawSphere(rel(mw + bore * 0.10, pose.eye),
                           static_cast<float>(fl.star_radius_m),
                           fkc(fl.color, fl.brightness,
                               std::min(1.0, fl.brightness)));
                // A small over-driven white core inside the ball: the eye
                // reads a gun flash as a hard point with a soft halo, not as
                // one uniform blob.
                DrawSphere(rel(mw + bore * 0.06, pose.eye),
                           static_cast<float>(fl.star_radius_m * 0.42),
                           fkc(glm::dvec3(1.0, 0.98, 0.92), fl.brightness * 1.4,
                               std::min(1.0, fl.brightness * 1.2)));
                rlEnableDepthMask();
                EndBlendMode();
            }
            // ── SNOW BLAST, alpha: the muzzle gases lifting the packed pad.
            // ALPHA and not additive for the same reason the flak puff's
            // smoke half is (see above): this is a dense white-grey cloud
            // over snow, and additive light cannot make a thing that reads as
            // matter. Lobes on the golden-angle lattice, FLATTENED -- wide in
            // the pad plane, low in the vertical -- because a blast cloud
            // spreads along the ground and a round ball would read as a
            // smoke bomb. Depth-tested, no depth write, so lobes composite.
            //
            // ★ ANCHORED AT THE PAD: the position swings with TRAIN but is
            // NOT carried by elevation (train_station_world, model (0,0,d) --
            // ground level by construction). The cue is the blast hitting the
            // snow the gun stands on. Elevation only WEAKENS it, through
            // blast_elev_gain (1.0 to the 45 deg knee, falling to 0.15 at the
            // +87 stop), because above the knee the blast really does go
            // skyward.
            const flak::BlastState bs =
                flak::blast_state(f_blast, fg.elev_rad);
            if (bs.alpha > 0.0) {
                const glm::dvec3 ctr = flak::train_station_world(
                    mf, fps, glm::dvec3(0.0, 0.0, bs.dist_m));
                const glm::dquat tq = flak::train_quat(fps.train_rad);
                const glm::dvec3 fwd = flak::model_dir_to_world(
                    mf, tq * glm::dvec3(0.0, 0.0, 1.0));
                const glm::dvec3 rgt = flak::model_dir_to_world(
                    mf, tq * glm::dvec3(1.0, 0.0, 0.0));
                const glm::dvec3 upv = mf.up;
                constexpr int kBlastLobes = 9;
                BeginBlendMode(BLEND_ALPHA);
                rlDisableDepthMask();
                for (int i = 0; i < kBlastLobes; ++i) {
                    // The phase is the SHOT COUNT, never a clock: the cloud
                    // boils while he fires and freezes as it fades, and a
                    // smoke run reproduces exactly.
                    const double j =
                        static_cast<double>(i) +
                        static_cast<double>(f_shot_seq % 977);
                    const double h1 = flak::fk_h1(j);
                    const double h2 = flak::fk_h2(j);
                    const glm::dvec3 wpos =
                        ctr + rgt * ((h1 * 2.0 - 1.0) * bs.spread_m) +
                        fwd * ((h2 * 2.0 - 1.0) * bs.spread_m * 0.55) +
                        upv * ((0.25 + 0.55 * h1) * bs.height_m);
                    const double size = bs.lobe_r_m * (0.65 + 0.45 * h2);
                    const double dist = glm::length(wpos - pose.eye);
                    const float r = static_cast<float>(
                        std::max(size, info.tracer_r_core_frac * dist));
                    // Per-lobe alpha well under the cloud's: nine overlapping
                    // translucent spheres accumulate, and it is the CENTRE
                    // that must land at bs.alpha, not each shell.
                    DrawSphere(rel(wpos, pose.eye), r,
                               fkc(bs.color, 1.0, bs.alpha * 0.30));
                }
                rlEnableDepthMask();
                EndBlendMode();
            }
        }
    };
    if (info.flak_manned_gun >= 0 &&
        info.flak_manned_gun < static_cast<int>(info.flak_guns.size()))
        draw_flak_muzzle_fx(info.flak_guns[info.flak_manned_gun],
                            info.flak_flash, info.flak_blast,
                            info.flak_shot_seq);
    // STAGE D: every OTHER gun draws off its own envelopes (all zero unless an
    // AI gunner is firing it -- the off-arm).
    for (std::size_t fi = 0; fi < info.flak_guns.size(); ++fi) {
        if (static_cast<int>(fi) == info.flak_manned_gun) continue;
        const FlakDraw& fg = info.flak_guns[fi];
        draw_flak_muzzle_fx(fg, fg.flash, fg.blast, fg.shot_seq);
    }

    // ── STAGE C: THE SPENT BRASS (immersion ladder 3/4). One tiny bright
    // cylinder per case. ★ DrawCylinderEx and not a billboard: DrawBillboard
    // renders NOTHING in this raylib (the standing render note), and a sprite
    // could not lie flat on the pad anyway -- these have an AXIS, and the
    // pile reads because a hundred of them lie every which way.
    //
    // The pool is app-owned and app-stepped; this pass is READ-ONLY and
    // world-anchored, so it draws whether or not he is still on the gun (the
    // pile outlives the fight -- that is the point) and costs exactly nothing
    // before the first round leaves the barrel.
    if (info.flak_brass != nullptr && !info.flak_brass->cases.empty()) {
        constexpr double kBrassDrawMaxM = 260.0;  // past this it is one pixel
        const double half = flak::kBrassLenM * 0.5;
        const float rad = static_cast<float>(flak::kBrassRadM);
        BeginBlendMode(BLEND_ALPHA);
        for (const flak::BrassCase& k : info.flak_brass->cases) {
            if (!k.active) continue;
            const double d = glm::length(k.pos - pose.eye);
            if (d > kBrassDrawMaxM) continue;
            const double a = flak::brass_alpha(k.age_s);
            if (!(a > 0.0)) continue;
            // Bright cartridge brass, dimmed slightly as it fades out rather
            // than only going transparent (a case does not turn to glass).
            const auto ch = [](double v) -> unsigned char {
                return static_cast<unsigned char>(
                    std::clamp(v, 0.0, 1.0) * 255.0);
            };
            const Color col{ch(0.88 * (0.55 + 0.45 * a)),
                            ch(0.69 * (0.55 + 0.45 * a)),
                            ch(0.28 * (0.55 + 0.45 * a)), ch(a)};
            DrawCylinderEx(rel(k.pos - k.axis * half, pose.eye),
                           rel(k.pos + k.axis * half, pose.eye), rad, rad, 6,
                           col);
        }
        EndBlendMode();
    }
    // Fleet Rig translucent pass: ALL blur discs + glass canopies AFTER ALL
    // opaque bodies, alpha-blended with depth-WRITE off so a farther body
    // can't overwrite a nearer disc/canopy and overlapping ones composite.
    BeginBlendMode(BLEND_ALPHA);
    rlDisableDepthMask();
    draw_prop(state, pose.eye, fp);
    for (std::size_t di = 0; di < info.drones_draw.size(); ++di) {
        if (di < info.drones_alive.size() && info.drones_alive[di] == 0)
            continue;  // CONQUEST wreck: no prop
        draw_prop(info.drones_draw[di], pose.eye, fp, info.drone_scale);
    }
    rlEnableDepthMask();
    EndBlendMode();

    // ★★ WINTER S3 THE MACHINE. A stand-in body -- the Blender hero sled is a
    // later art rung -- but it is NOT a stand-in POSE: the chassis rides the
    // kernel's orientation and each ski/track is drawn at its OWN suspension
    // extension, read from sim::SledState::susp_x. That matters more than the
    // shape does. §2.4c.1 ruled suspension travel is REAL STATE and not an
    // animation clip, and the only way to keep that honest is for the visual to
    // have no travel channel of its own to drift from: when the bank-side ski
    // compresses first, you are watching the number the contact solver used.
    // DrawBillboard renders nothing in this raylib build (the wingtip-smoke
    // finding), so this is cubes and cylinders on purpose.
    //
    // ★ GLTF WIRING RUNG (D1): the Blender hero sled draws INSTEAD of the
    // stand-in when the GLB is present -- same pose, same kernel numbers,
    // every moving part a function of SledState (render/sled_model.cpp).
    // SEADS_SLED_PLACEHOLDER=1 forces the stand-in for A/B comparison, and
    // a missing GLB falls back to it rather than to an invisible machine.
    // ★ R4a THE MACHINE MARKER (Chad 2026-08-25: "a marker marks the
    // snowmachine that is easy to see"). Drawn BEFORE the machine so the
    // column cannot z-fight the hull it rises out of.
    //
    // ⚠ NOT YET WIRED TO ITS REAL TRIGGER. It wants `!rider_attached`, which
    // does not exist (R4a Phase 1 item 3), and the ON-FOOT state it serves is
    // R5+ "scoped, not yet specified". Until then SEADS_SLED_MARKER=1 forces
    // it on so Chad can rule the LOOK. Absent the env var this is a no-op and
    // the shipped frame is bit-identical -- the beacon cannot move a golden.
    {
        static const bool force_marker =
            std::getenv("SEADS_SLED_MARKER") != nullptr;
        const render::MarkerGeom mg =
            render::marker_geom(info.sled_pos, info.sled_active && force_marker);
        if (mg.visible) {
            // A BLUE CONE shining DOWN, apex 10 m up, pooling on the machine
            // (Chad 2026-08-25). Real geometry -- DrawBillboard renders nothing
            // in this raylib build, a documented house trap. STEADY: no blink,
            // no pulse, so nothing here reads a clock or a frame counter.
            //
            // ★ ADDITIVE, DEPTH-WRITE OFF -- the house idiom for anything that
            // is LIGHT rather than an object (same as the pump neon frame).
            // Drawn opaque it was a solid painted wedge that BLOTTED OUT the
            // machine it exists to point at; additive brightens what is behind
            // it instead of replacing it, which is what a beam does.
            BeginBlendMode(BLEND_ADDITIVE);
            rlDisableDepthMask();  // depth TEST stays on; WRITE off
            const Color beam{18, 44, 78, 255};
            DrawCylinderEx(rel(mg.apex, pose.eye), rel(mg.base, pose.eye),
                           static_cast<float>(mg.apex_radius_m),
                           static_cast<float>(mg.base_radius_m), 16, beam);
            rlEnableDepthMask();
            EndBlendMode();
        }
    }
    // ★★★ ST-5 PHASE D2 -- THE LAUNCHER FRAME, SOLVED BEFORE THE MAN IS
    // POSED (Chad 2026-09-04, VERBATIM: "onto the hand hook to make exception
    // and grasp the stock of the launcher").
    //
    // ⚠ THIS ORDER IS THE WHOLE RUNG. v1 solved the launcher AFTER
    // sled_model_draw, because the shouldered key anchors on the posed rider's
    // neck and only that call publishes it -- which is exactly why v1 had to
    // leave both hands on the handlebars: a weld cannot be handed a target
    // that does not exist yet, and welding to a stale one stretches an arm.
    // So the frame is computed HERE, once, and the two consumers below take
    // THE SAME VARIABLE: the hand targets that go into the sled draw, and the
    // launcher that is drawn after it. Bit-equal by construction, not by two
    // copies of one formula that can drift apart.
    //
    // ⚠ AND THE ANCHOR IS ONE FRAME OLD, DELIBERATELY. `sled_model_rider_back`
    // is last frame's, so what is cached across the draw is the SCALAR height
    // of his neck over the machine's own frame origin -- a body-frame number,
    // measured on the frame whose origin it belongs to. Caching the world
    // POINT instead would smear the machine's travel into it (0.33 m at
    // 20 m/s and 60 Hz); the scalar moves only as far as the suspension and
    // his lean move it, and the lag is invisible anyway because the hands and
    // the drawn launcher share the SAME frame and move together.
    bool sting_have = false;
    render::sting::Frame sting_cur;
    glm::dvec3 sting_origin(0.0), sting_up_w(0.0, 1.0, 0.0);
    double sting_shoulder_up = 0.0;
    // [0] = his LEFT hand on the FORWARD grip, [1] = his RIGHT on the REAR
    // pistol grip: the trigger hand is the rear one, and on a gunstock both
    // grips are on the centreline so there is no left/right to pair by side.
    glm::dvec3 sting_hand_pos[2]{};
    glm::dvec3 sting_hand_lat(0.0);
    float sting_hand_w[2] = {0.0f, 0.0f};
    // ⚠ THE CACHE IS A FULL BODY-FRAME VECTOR, NOT A HEIGHT. The collapse
    // defect (Chad: "collapsed over ... looked broken") was a scalar cache:
    // height alone loses the neck's fore/aft station -- +0.297 m AFT of the
    // machine origin in the riding hunch -- so the shoulder anchor floated
    // 0.30 m ahead of the man and the left grip landed 1.05 m from a 0.61 m
    // arm. Body-frame keeps the anti-smear law (machine travel between
    // frames cannot leak in); the vector keeps the man's actual station.
    static glm::dvec3 s_sting_neck_local(0.0);
    static bool s_sting_neck_have = false;
    if (info.sting_deploy > 0.0f && render::launcher_model_ready()) {
        const render::sting::Stations& lst = render::launcher_stations();
        const float te = render::sting::ease(info.sting_deploy);
        // The man's own frame: +X his right, +Y local up, -Z the way he
        // faces. One convention for both stances, so no sign can disagree
        // between the seat and the boots.
        glm::dmat3 mframe(1.0);
        glm::dvec3 neck_local(0.0);
        if (info.sting_seated) {
            // SEATED: the machine IS the frame, and `sled_pos` is the same
            // kernel CG the game-loop lane quotes its launch origin over.
            sting_origin = info.sled_pos;
            mframe = info.sled_basis;
            // Prefer the rider the rig actually posed: his neck is measured,
            // the constant is measured too (indy650 rest skeleton) but only
            // the live one follows the suspension and his lean.
            neck_local = s_sting_neck_have ? s_sting_neck_local
                                           : render::sting::kSeatedNeckLocal;
            sting_shoulder_up = neck_local.y - render::sting::kShoulderDropM;
            sting_have = info.sled_active;
        } else if (info.sled_walker != nullptr) {
            // AFOOT: his boots are the origin, local up is the radial (the
            // world is a sphere), and the heading is flattened into the
            // tangent plane so a walker on a slope does not tilt the tube.
            sting_origin = info.sled_walker->pos;
            const glm::dvec3 up_w = glm::normalize(sting_origin);
            glm::dvec3 fwd = info.sled_walker->heading -
                             glm::dot(info.sled_walker->heading, up_w) * up_w;
            const double fl = glm::length(fwd);
            if (fl > 1e-6) {
                fwd /= fl;
                const glm::dvec3 rt = glm::cross(fwd, up_w);
                mframe = glm::dmat3(rt, up_w, -fwd);
                sting_shoulder_up = render::sting::kAfootShoulderUpM;
                sting_have = true;
            }
        }
        if (sting_have) {
            sting_up_w = glm::dvec3(mframe[1]);
            // THE SHOULDERED KEY. The rail lies along the aim, and the STOCK
            // -- not the origin -- is what gets welded to him: `pos` is
            // solved so st_shoulder lands on the shoulder anchor. The anchor
            // rides the AIM's right axis rather than the machine's, so
            // swinging the aim across the sky reads as him turning into it
            // instead of the tube scything through his head.
            render::sting::Frame shouldered;
            shouldered.basis =
                render::sting::frame_from_aim(info.sting_aim_dir, sting_up_w);
            // SEATED, the anchor is the MAN: neck carried to world with its
            // fore/aft term intact, dropped to the shoulder, out along the
            // aim's right. AFOOT there is no posed neck to read and the
            // walker's boots-origin height is the whole story.
            const glm::dvec3 anchor_base =
                info.sting_seated
                    ? sting_origin +
                          mframe * glm::dvec3(neck_local.x,
                                              neck_local.y -
                                                  render::sting::kShoulderDropM,
                                              neck_local.z)
                    : sting_origin + sting_up_w * sting_shoulder_up;
            const glm::dvec3 anchor =
                anchor_base +
                glm::dvec3(shouldered.basis[0]) * render::sting::kShoulderOutM;
            shouldered.pos = anchor - shouldered.basis * lst.shoulder;

            // The two carried keys live in the man's frame; lift them out.
            const auto to_world = [&](const render::sting::Frame& f) {
                render::sting::Frame w;
                w.pos = sting_origin + mframe * f.pos;
                w.basis = mframe * f.basis;
                return w;
            };
            if (info.sting_seated) {
                // Seat-relative ABSOLUTE keys (render/sting_deploy.h): the
                // old shoulder-relative pair was standing-calibrated and,
                // hung off the real 0.51 m seated shoulder, put the stow key
                // inside the track for the first 1.3 s.
                const render::sting::KeyBlend kb = render::sting::key_blend(te);
                const render::sting::Frame stowed =
                    to_world(render::sting::seated_stowed());
                const render::sting::Frame pulled =
                    to_world(render::sting::seated_pulled());
                sting_cur =
                    kb.leg == 0
                        ? render::sting::blend_frame(stowed, pulled, kb.w)
                        : render::sting::blend_frame(pulled, shouldered, kb.w);
            } else {
                // AFOOT is one leg: nothing to pull it out from under, so he
                // simply raises it from the low carry onto his shoulder.
                sting_cur = render::sting::blend_frame(
                    to_world(render::sting::local_stowed(sting_shoulder_up)),
                    shouldered, te);
            }
            // THE HAND TARGETS. Pure reads of the frame just solved plus the
            // asset's own stations -- and the WEIGHTS are pure functions of
            // the same blend (render/sting_deploy.h), which is what lets the
            // one-frame CUT snap both hands back to the bars with no second
            // piece of state to agree with.
            //
            // ⚠ SEATED ONLY (and the `if` below enforces it). Afoot the arms
            // are the walker's own procedural spread in render/sled_model.cpp,
            // not the handlebar weld this hook substitutes into, so there is
            // nothing here to release; the man afoot carries the launcher and
            // his hands are the open debt.
            sting_hand_pos[0] = sting_cur.pos + sting_cur.basis * lst.grip_l;
            sting_hand_pos[1] = sting_cur.pos + sting_cur.basis * lst.grip_r;
            // Both grips are cylinders ACROSS the launcher, so their axis is
            // its own lateral -- square to the fire axis, which is what turns
            // his knuckles down the barrel. The weld picks the end.
            sting_hand_lat = glm::dvec3(sting_cur.basis[0]);
            sting_hand_w[0] = render::sting::grip_weight_l(te);
            sting_hand_w[1] = render::sting::grip_weight_r(te);
        }
    }
    bool sled_hero_drawn = false;
    if (info.sled_active) {
        static const bool force_placeholder =
            std::getenv("SEADS_SLED_PLACEHOLDER") != nullptr;
        if (!force_placeholder) {
            render::SledRig srig;
            srig.steer = static_cast<float>(info.sled_steer);
            srig.right_push = static_cast<float>(info.sled_right_push);
            float susp_x_f[3], susp_v_f[3];
            for (int i = 0; i < 3; ++i) {
                susp_x_f[i] = static_cast<float>(info.sled_susp_x[i]);
                susp_v_f[i] = static_cast<float>(info.sled_susp_v[i]);
                srig.susp_m[i] = susp_x_f[i];
            }
            // ★ SUDBURIAN R1a, DEFECT 8: the rider's terrain-reaction channel,
            // composed by the pure render::rider_absorb() over kernel state
            // only. No accumulator, no frame time -- absorb is a function of
            // this tick's SledState, so it replays bit-exact from a tape.
            srig.absorb = render::rider_absorb(
                susp_x_f, susp_v_f, static_cast<float>(info.sled_rest));
            srig.lean_lat_m = static_cast<float>(info.sled_rider_lat_m);
            srig.lean_up_m = static_cast<float>(info.sled_rider_up_m);
            srig.lean_fwd_m = static_cast<float>(info.sled_rider_fwd_m);
            // ★ R2c-5: the control-input pose. throttle -> right elbow down +
            // hand up; brake -> left elbow up + hand down over the bar.
            srig.throttle = static_cast<float>(info.sled_throttle);
            srig.brake = static_cast<float>(info.sled_brake);
            srig.cam_yaw = static_cast<float>(info.sled_cam_yaw);
            srig.cam_pitch = static_cast<float>(info.sled_cam_pitch);
            // The channel smoke test (GLTF_WIRING_HANDOFF §2 acceptance):
            // SEADS_SLED_RIG_SMOKE="steer,sL,sR,sT,lat,up,fwd,absorb,thr,brk"
            // overrides the rig channels so a screenshot certifies each
            // binding's sign and magnitude without driving there.
            static const char* rig_smoke = std::getenv("SEADS_SLED_RIG_SMOKE");
            if (rig_smoke != nullptr) {
                // ★ R2c-5 EXTENDS THE SMOKE STRING to ten fields: the two
                // control inputs are the channels this rung adds, so they get
                // certified the same way every other binding was. An old
                // eight-field string still parses -- the two new fields stay 0,
                // which is the honest reading of "this string does not name
                // them" and is exactly the pose the rest visual was signed at.
                float v[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
                std::sscanf(rig_smoke, "%f,%f,%f,%f,%f,%f,%f,%f,%f,%f", &v[0],
                            &v[1], &v[2], &v[3], &v[4], &v[5], &v[6], &v[7],
                            &v[8], &v[9]);
                srig.steer = v[0];
                srig.susp_m[0] = v[1];
                srig.susp_m[1] = v[2];
                srig.susp_m[2] = v[3];
                srig.lean_lat_m = v[4];
                srig.lean_up_m = v[5];
                srig.lean_fwd_m = v[6];
                srig.absorb = v[7];
                srig.throttle = v[8];
                srig.brake = v[9];
            }
            // ★ THE SCARF CHANNEL (SCARF_SPEC §4). The kernel's own velocity,
            // rotated WORLD -> BODY by the transpose of the body->world basis
            // this same frame. Computed, never a sign written down.
            srig.vel_body_mps =
                glm::vec3(glm::transpose(info.sled_basis) * info.sled_vel_ms);
            srig.ticks = info.sled_ticks;
            srig.dt_s = static_cast<float>(info.sled_dt_s);
            // ★ R4a: angular_vel is ALREADY body frame (sim/sled.h), so unlike
            // the velocity above it needs no rotation -- do not "fix" it with
            // a transpose that would silently mirror the swing.
            srig.omega_body_rps = glm::vec3(info.sled_omega_body_rps);
            srig.epoch = info.sled_epoch;
            // ★★★ R4a: the mass budget the rider-load model is baked
            // from. Straight reads of sim::SledParams -- the one-number rule.
            srig.air_s = static_cast<float>(info.sled_air_s);
            srig.air_grace_s = static_cast<float>(info.sled_air_grace_s);
            srig.hull_engage = static_cast<float>(info.sled_hull_engage);
            srig.susp_sum_m = static_cast<float>(info.sled_susp_sum_m);
            srig.rolled = info.sled_rolled;
            // ★★★ R4a §7.3 STAGE 4: whether he is still holding the bars, and
            // the snow he lands in if he is not.
            srig.grip_attached = info.sled_grip_attached;
            srig.walker = info.sled_walker;
            srig.gait = info.sled_gait;
            // ★★★ G2i / G2j: the hop height and the work pose, straight
            // across -- render authors none of it.
            srig.hop_height_m = static_cast<float>(info.sled_hop_height_m);
            srig.work_on = info.sled_work_on;
            srig.work_phase = static_cast<float>(info.sled_work_phase);
            srig.work_blow_on = info.sled_work_blow_on;
            srig.work_blow = static_cast<float>(info.sled_work_blow);
            srig.rider_mass_kg = info.sled_rider_mass_kg;
            srig.mass_kg = info.sled_mass_kg;
            srig.cg_height_m = info.sled_cg_height_m;
            // SEADS_SCARF_VEL="vx,vy,vz" is a BODY-FRAME VELOCITY in m/s (so
            // forward travel at 8 m/s is "0,0,-8"), mirroring
            // SEADS_SLED_RIG_SMOKE: it lets a --smoke shot certify the scarf at
            // 0 / 8 / 20 m/s and in a sideslip without driving there.
            static const char* scarf_vel = std::getenv("SEADS_SCARF_VEL");
            if (scarf_vel != nullptr) {
                float v[3] = {0.0f, 0.0f, 0.0f};
                std::sscanf(scarf_vel, "%f,%f,%f", &v[0], &v[1], &v[2]);
                srig.vel_body_mps = glm::vec3(v[0], v[1], v[2]);
            }
            // ★★★ ST-5 THE HAND HOOK. The launcher frame solved above, read
            // back out as two world points and a weight per hand; at weight 0
            // -- every frame with the sting stowed -- render/sled_model.cpp
            // does not evaluate one term of it and the signed riding pose is
            // bit-identical. SEATED ONLY: afoot there is no handlebar weld to
            // substitute into.
            if (sting_have && info.sting_seated) {
                for (int h = 0; h < 2; ++h) {
                    srig.hand_grip[h].pos = sting_hand_pos[h];
                    srig.hand_grip[h].lat = sting_hand_lat;
                    srig.hand_grip[h].weight = sting_hand_w[h];
                }
                // ★★★ ST-5 THE SWEEP -- "the sudburian shall follow the aim
                // ... they shall twist head and torso in addition to the
                // arms" (Chad 2026-09-05). The SAME GATE as the hands, on
                // purpose: the trunk turns for exactly the frames the hands
                // are on the launcher, and both fall away together on the
                // cut.
                //
                // ⚠ THE PROJECTION LIVES HERE AND NOT IN sled_model.cpp.
                // `info.sting_aim_dir` is a WORLD ray; the pose needs two
                // angles in the MAN's frame. Resolving it here keeps the
                // draw-side rule intact (render/sled_model.cpp never reads a
                // mode, a stance or a world aim -- it is handed numbers) and
                // it re-uses the very basis the launcher was solved over two
                // hundred lines up, so the tube and the chest cannot end up
                // with two opinions about which way the machine faces.
                //
                // ⚠ NOT `sting_cur.basis`. The drawn launcher's frame eases
                // out of the STOWED key through the sweep, so early in the
                // draw it points at the tunnel; the aim ray is where he is
                // actually looking the whole time, and the twist has to lead
                // the tube rather than chase it.
                const double aim_l = glm::length(info.sting_aim_dir);
                if (aim_l > 1e-9) {
                    const glm::dvec3 aim = info.sting_aim_dir / aim_l;
                    // The sled basis is (+X right, +Y up, -Z forward): the
                    // kernel body frame, which is the frame the aim's own
                    // azimuth was accumulated over in app/main.cpp.
                    const double a_r =
                        glm::dot(aim, glm::dvec3(info.sled_basis[0]));
                    const double a_u =
                        glm::dot(aim, glm::dvec3(info.sled_basis[1]));
                    const double a_f =
                        -glm::dot(aim, glm::dvec3(info.sled_basis[2]));
                    // atan2(right, forward): POSITIVE TO HIS RIGHT, the sign
                    // render::sting::twist_rad documents its negation
                    // against.
                    srig.sting_aim_az =
                        static_cast<float>(std::atan2(a_r, a_f));
                    srig.sting_aim_el = static_cast<float>(
                        std::atan2(a_u, std::sqrt(a_f * a_f + a_r * a_r)));
                }
            }
            sled_hero_drawn =
                render::sled_model_draw(info.sled_pos, info.sled_basis,
                                        pose.eye, sun_f, info.sled_cg_h, srig);
            // ★★★ ST-5: STASH HIS NECK FOR NEXT FRAME'S LAUNCHER ANCHOR, as
            // a BODY-FRAME HEIGHT measured against the very origin it was
            // posed over -- so the machine's travel between frames cannot
            // leak into it. Read at the top of the next frame; see the lag
            // note there.
            {
                const render::RiderBack& rb = render::sled_model_rider_back();
                if (rb.valid) {
                    const glm::dvec3 d = rb.neck - info.sled_pos;
                    s_sting_neck_local =
                        glm::dvec3(glm::dot(d, glm::dvec3(info.sled_basis[0])),
                                   glm::dot(d, glm::dvec3(info.sled_basis[1])),
                                   glm::dot(d, glm::dvec3(info.sled_basis[2])));
                    s_sting_neck_have = true;
                }
            }
        }
    }
    // ★★★ ST-5 PHASE D -- THE SEAT DEPLOY, DRAWN (Chad 2026-09-04: "press P
    // on the snowmachine ... deployment from the seat to the Sudburian's
    // hands to take aim"). Three seconds, stowed beside the tunnel -> pulled
    // out to his right -> stock on his shoulder along the aim.
    //
    // ⚠ THE FRAME IS NOT SOLVED HERE ANY MORE. It is `sting_cur`, computed
    // BEFORE sled_model_draw so the same frame could be handed to the hand
    // weld (see "ST-5 PHASE D2" above), and this block DRAWS it. That is the
    // point of the restructure: the launcher his hands are on and the
    // launcher on the screen are one variable, so they cannot disagree. The
    // draw still happens after the machine for z-order and for the loaded
    // Sting on the rail, but nothing is recomputed to do it.
    //
    // ⚠ THE SWEEP GOES AROUND HIS SILHOUETTE. Both intermediate keys sit
    // outboard of his right shoulder and the rise happens THERE, so the tube
    // never crosses his chest, his chin or the windshield -- the failure a
    // straight stowed->shouldered lerp produces every time. The clearance is
    // asserted in test/unit/test_sting_pose.cpp, not left to this comment.
    //
    // A missing launcher.glb draws NOTHING (there was never a primitive
    // launcher to fall back to), so the whole block is bit-identical to the
    // frame before this rung until the asset lands. Same for a man afoot
    // before the sled is seeded: `sled_walker` is only published inside
    // `sled_active`, so there is no anchor and nothing is drawn.
    //
    // ⚠ THE OPEN SEAM THAT REMAINS (the v1 seam is CLOSED -- his hands let
    // go of the bars and take the grips): AFOOT, his arms are the walker's
    // procedural balance spread, not the handlebar weld, so the man on his
    // feet still carries the launcher without gripping it. That is a
    // different mechanism in a different branch of render/sled_model.cpp and
    // it is deliberately left for a rung of its own.
    if (sting_have) {
        const render::sting::Stations& lst = render::launcher_stations();
        render::launcher_model_draw(sting_cur.pos, sting_cur.basis, pose.eye);

        // THE LOADED RIG. The Sting rides its rail, tail at st_muzzle, so
        // what leaves his shoulder is visibly the thing that flies. Drawn
        // for the WHOLE deploy rather than from a late threshold: a drone
        // appearing on the rail part way through the swing is a pop, and
        // he does not pull an empty launcher out from under the seat.
        // Skipped once it is flying (that draw owns it) or spent.
        if (!info.sting_flying && info.sting_left > 0 &&
            render::sting_model_ready()) {
            const glm::dvec3 fire = -glm::dvec3(sting_cur.basis[2]);
            const glm::dvec3 muzzle_w =
                sting_cur.pos + sting_cur.basis * lst.muzzle;
            // The rail spin-up rides the SAME phase (app-side it is stepped
            // at the slow rail rate while the blend finishes), so the props
            // are already turning as it comes off his shoulder.
            render::sting_model_draw(muzzle_w + fire * 0.55, sting_cur.basis,
                                     pose.eye, 1.0f, info.sting_prop_phase,
                                     info.sting_prop_rate);
        }
        // THE LAUNCH ORIGIN, MEASURED (the game-loop packet §2: "measure
        // yours on the posed rider and tell us the number"). Off by
        // default -- one getenv, read once.
        static const bool deploy_debug =
            std::getenv("SEADS_STING_DEPLOY_DEBUG") != nullptr;
        if (deploy_debug && info.sting_deploy > 0.999f) {
            const glm::dvec3 muzzle_w =
                sting_cur.pos + sting_cur.basis * lst.muzzle;
            TraceLog(LOG_INFO,
                     "STING DEPLOY: %s  shoulder %.3f m  st_muzzle %.3f m "
                     "over the frame origin (stations %s, grip w %.2f/%.2f)",
                     info.sting_seated ? "seated" : "afoot", sting_shoulder_up,
                     glm::dot(muzzle_w - sting_origin, sting_up_w),
                     lst.from_glb ? "GLB" : "nominal",
                     static_cast<double>(sting_hand_w[0]),
                     static_cast<double>(sting_hand_w[1]));
        }
        // ★ G2g FOOTSTEPS (Chad: "introduce footsteps where he walks in the
        // snow"). App owns the ring (stance-entry pins, snow only); this
        // pass just draws it: a heel disc and a toe disc per print, laid
        // along his heading at the plant, a hair above the sampled ground.
        // REAL GEOMETRY (DrawCylinderEx) -- DrawBillboard renders nothing
        // in this renderer (render/slag.cpp's recorded lesson).
        if (info.footprints != nullptr && info.footprint_n > 0) {
            const Color print_c{52, 56, 66, 255};  // packed-snow shadow
            for (int k = 0; k < info.footprint_n; ++k) {
                const FrameInfo::Footprint& fp2 = info.footprints[k];
                const double pr = glm::length(fp2.pos);
                if (!(pr > 0.0)) continue;
                const glm::dvec3 up = fp2.pos / pr;
                glm::dvec3 fw = glm::dvec3(fp2.fwd);
                fw -= glm::dot(fw, up) * up;
                const double fl = glm::length(fw);
                if (fl < 1.0e-6) continue;
                fw /= fl;
                const glm::dvec3 heel = fp2.pos - fw * 0.08 + up * 0.012;
                const glm::dvec3 toe = fp2.pos + fw * 0.10 + up * 0.012;
                DrawCylinderEx(rel(heel, pose.eye),
                               rel(heel + up * 0.015, pose.eye), 0.10f, 0.10f,
                               8, print_c);
                DrawCylinderEx(rel(toe, pose.eye),
                               rel(toe + up * 0.015, pose.eye), 0.085f,
                               0.085f, 8, print_c);
            }
        }
    }
    // *** SK-1c THE SUDBURIAN-BACK HUD (Chad 2026-08-25, VERBATIM): "put the
    // dot on the sudburians back, make it a dynamic moving grid and make it
    // stylized and easy to read, Able to see the handlebars through sudburians
    // back like like lidar and you can see turing deflection visually by making
    // it slag orange and neon emmitting light particles we can pick up through
    // the sudburian somehow (not literally but visually like HUD style)."
    //
    // X-RAY by construction: depth test OFF + additive, so it reads THROUGH the
    // rider without a second depth pass. Anchored to the SLED BODY FRAME (not a
    // bone) so it cannot fight the rig; it rides his back because that is where
    // the frame puts it. Slag orange has in-world precedent (render/slag.cpp).
    // ALL of it is a read of kernel state -- info.sled_steer is steer_ACTUAL,
    // the slewed value, never the raw command, or the bars would lie for the
    // 0.5 s of the slew.
    if (info.sled_active && info.sled_hud_xray) {
        const glm::dvec3 sp = info.sled_pos;
        const glm::dmat3 sR = info.sled_basis;
        const glm::dvec3 bx = sR[0], by = sR[1], bz = sR[2];
        // ★ THE THEME PAIR, NOT TYPED HEX (Chad: "the themed color code
        // graphified into this codebase"). render::team_colors() is this
        // codebase's ONE cross-layer hue table (map + world + liveries), and
        // its two signal hues are an EXACT complement pair by construction:
        // `ally` = kComplementBlue, `enemy` = kSlagOrange (the pour). Grid =
        // blue, weight node = slag orange, per his 2026-08-25 call. Reading the
        // TABLE, not the constants, is why there is no second palette to fork
        // (H1) -- retint it once and this HUD follows.
        const auto rgb01 = [](const glm::dvec3& c, unsigned char a) {
            const auto q = [](double v) {
                return static_cast<unsigned char>(
                    std::lround(std::min(1.0, std::max(0.0, v)) * 255.0));
            };
            return Color{q(c.x), q(c.y), q(c.z), a};
        };
        const glm::dvec3 c_blue = render::team_colors().ally;
        const glm::dvec3 c_slag = render::team_colors().enemy;
        const Color grid_blue = rgb01(c_blue, 255);
        const Color grid_blue_dim = rgb01(c_blue, 150);
        const Color slag = rgb01(c_slag, 255);
        // The glow core: the same hue lifted toward white, never a typed
        // colour -- a retint of the table carries the core with it.
        const Color slag_hot = rgb01(c_slag + glm::dvec3(0.0, 0.25, 0.45), 255);
        // ORDER IS LOAD-BEARING (audit defect 4): BeginBlendMode FLUSHES the
        // pending rlgl batch, so it must run while depth is still ENABLED --
        // otherwise whatever earlier geometry is sitting in the batch gets
        // flushed depth-less. Exit already unwinds in the mirror order.
        BeginBlendMode(BLEND_ADDITIVE);
        rlDisableDepthTest();
        // --- the grid ON THE SWEATER ----------------------------------------
        // Chad, 2026-08-25: "make it pasted to his white/grey sweater.
        // Currently it is on his head. make it slightly bigger to fit the
        // whole back."  It WAS on his head: the old anchor was
        // sp + by*(cg_h + 0.34), and sp is ALREADY the CG (cg_h above the
        // surface), so cg_h was counted twice -- 1.47 m up, which is his
        // helmet. The panel now rides the rig's own posed torso frame
        // (render::sled_model_rider_back(), the SAME pelvis/neck/back-normal
        // the scarf's §3b plane runs on), so it leans, twists and bobs with
        // him instead of with the chassis. Fallback keeps the body frame --
        // with the double-count removed -- when the hero rig did not pose.
        const render::RiderBack& rb = render::sled_model_rider_back();
        glm::dvec3 back;
        glm::dvec3 gax, gup;  // panel width axis, panel up axis (unit)
        double gw, gh;
        if (rb.valid) {
            const glm::dvec3 spine = rb.neck - rb.pelvis;
            const double spine_len = glm::length(spine);
            gup = spine_len > 1e-6 ? spine / spine_len : glm::dvec3(by);
            const glm::dvec3 nrm(rb.back_normal);
            // Width across the shoulders, re-orthogonalised against the spine
            // so the panel is square on a twisted torso.
            glm::dvec3 w = glm::cross(gup, nrm);
            gax = glm::length(w) > 1e-6 ? glm::normalize(w)
                                        : glm::dvec3(rb.width_axis);
            // FIT THE WHOLE BACK: the panel spans the pelvis->neck run itself
            // (bounded, so a degenerate pose cannot inflate it), and stands
            // off the measured suit surface so it lies ON the sweater.
            gh = std::clamp(spine_len * 0.92, 0.34, 0.62);
            gw = gh * 0.86;
            back = rb.pelvis + gup * (spine_len * 0.52) +
                   nrm * static_cast<double>(rb.back_surface + 0.02);
        } else {
            back = sp + by * 0.62 + bz * 0.16;  // no double-counted cg_h
            gax = bx;
            gup = by;
            gw = 0.44;
            gh = 0.52;
        }
        const int NX = 7, NY = 6;
        // DYNAMIC: the whole panel shears with the rider weight, so the grid
        // IS the dot -- displacement you read as a shape, not a pip in a box.
        const double wx = info.sled_weight_x, wy = info.sled_weight_y;
        auto gpt = [&](double u, double v) {
            // u,v in [-1,1]; the shear grows toward the panel edges so the
            // centre stays a readable reference while the rim carries the news.
            const double sxp = -wx * 0.16 * (1.0 - 0.35 * std::fabs(v));
            const double syp = wy * 0.13 * (1.0 - 0.35 * std::fabs(u));
            return back + gax * (u * gw * 0.5 + sxp) +
                   gup * (v * gh * 0.5 + syp);
        };
        for (int i = 0; i <= NX; ++i) {
            const double u = -1.0 + 2.0 * i / NX;
            for (int j = 0; j < NY; ++j) {
                const double v0 = -1.0 + 2.0 * j / NY;
                const double v1 = -1.0 + 2.0 * (j + 1) / NY;
                DrawLine3D(rel(gpt(u, v0), pose.eye), rel(gpt(u, v1), pose.eye),
                           grid_blue);
            }
        }
        for (int j = 0; j <= NY; ++j) {
            const double v = -1.0 + 2.0 * j / NY;
            for (int i = 0; i < NX; ++i) {
                const double u0 = -1.0 + 2.0 * i / NX;
                const double u1 = -1.0 + 2.0 * (i + 1) / NX;
                DrawLine3D(rel(gpt(u0, v), pose.eye), rel(gpt(u1, v), pose.eye),
                           grid_blue_dim);
            }
        }
        // ★ THE WEIGHT BALL -- slag orange and GLOWING against the blue grid.
        // The glow is three concentric shells under the additive blend already
        // in force: each shell adds light, so the falloff IS a bloom without a
        // second pass, a sprite, or a shader. Radii/alphas are a geometric
        // series so the core stays hot while the halo stays translucent.
        {
            const glm::dvec3 node = gpt(-wx * 0.8, wy * 0.8);
            const Vector3 nv = rel(node, pose.eye);
            DrawSphere(nv, 0.062f, rgb01(c_slag, 40));
            DrawSphere(nv, 0.042f, rgb01(c_slag, 90));
            DrawSphere(nv, 0.026f, slag);
            DrawSphere(nv, 0.014f, slag_hot);  // the white-hot core
        }
        // --- the handlebars: REMOVED ----------------------------------------
        // Chad, 2026-08-25: "the handlebars arent necessary anymore." They were
        // the x-ray's steering read; with steer decoupled from lean (SK-1d) the
        // panel is a WEIGHT-TRANSFER instrument and nothing else. Deleted, not
        // hidden behind a flag -- a dead branch is a fork waiting to happen.
        // --- the "neon particles we can pick up through the sudburian" -------
        // Deterministic motes on the grid plane, drifting with the weight; a
        // HUD-style shimmer, not a physical emitter. Keyed on the CUMULATIVE
        // sim tick so it is frame-rate independent (AT-9 discipline) and never
        // a wall clock. NOT sled_ticks: that is the ticks consumed THIS FRAME
        // (a constant 2 at steady 60/120), which froze the motes and made the
        // little motion left frame-rate DEPENDENT -- audit item 1.
        {
            const int kMotes = 14;
            const double ph = static_cast<double>(info.sled_tick_no) * 0.02;
            for (int m = 0; m < kMotes; ++m) {
                const double f = static_cast<double>(m) / kMotes;
                const double a = ph + f * 6.2831853;
                const double u = std::sin(a * 1.7 + f * 3.1) * 0.9;
                const double v = std::sin(a) * 0.9;
                DrawSphere(rel(gpt(u, v), pose.eye), 0.010f, grid_blue);
            }
        }
        EndBlendMode();
        rlEnableDepthTest();
    }
    if (info.sled_active && !sled_hero_drawn) {
        const glm::dvec3 sp = info.sled_pos;
        const glm::dmat3 sR = info.sled_basis;
        const glm::dvec3 bx = sR[0], by = sR[1], bz = sR[2];
        const Color hull{196, 74, 52, 255};  // a red machine on white ground
        const Color runner{40, 44, 52, 255};
        const double my = -(info.sled_cg_h - info.sled_rest);
        auto patch_world = [&](const glm::dvec3& mount, int i) {
            // mount, then DOWN the chassis axis by the live extension:
            // rest - compression. Real state, straight onto the screen.
            const double hang = info.sled_rest - info.sled_susp_x[i];
            return sp + sR * mount - by * hang;
        };
        const glm::dvec3 skiL(-0.5 * info.sled_stance, my, -info.sled_ski_fwd);
        const glm::dvec3 skiR(0.5 * info.sled_stance, my, -info.sled_ski_fwd);
        const glm::dvec3 trk(0.0, my, info.sled_track_aft);
        // Chassis: a wedge of two boxes sitting above the patches.
        DrawCubeV(rel(sp + by * 0.10 - bz * 0.15, pose.eye),
                  Vector3{0.62f, 0.42f, 1.70f}, hull);
        DrawCubeV(rel(sp + by * 0.42 + bz * 0.25, pose.eye),
                  Vector3{0.52f, 0.30f, 0.70f}, hull);
        // Handlebars, so the machine has a front you can read at a glance.
        DrawCylinderEx(rel(sp + by * 0.58 - bz * 0.55 - bx * 0.28, pose.eye),
                       rel(sp + by * 0.58 - bz * 0.55 + bx * 0.28, pose.eye),
                       0.035f, 0.035f, 6, runner);
        // The three contact patches, each at its own extension.
        for (int i = 0; i < 3; ++i) {
            const glm::dvec3 m = (i == 0) ? skiL : (i == 1) ? skiR : trk;
            const glm::dvec3 c = patch_world(m, i);
            const double half_len = (i == 2) ? 0.61 : 0.48;
            DrawCubeV(rel(c, pose.eye),
                      Vector3{i == 2 ? 0.38f : 0.135f, 0.10f,
                              static_cast<float>(2.0 * half_len)},
                      runner);
            // The strut, drawn between the mount and the patch: its LENGTH is
            // the compression, so the asymmetry is visible without a gauge.
            DrawCylinderEx(rel(sp + sR * m, pose.eye), rel(c, pose.eye), 0.03f,
                           0.03f, 5, Color{150, 155, 165, 255});
        }
    }
    // ★ DRIVE-2: CARBIDE SPARKS on bare hard surface (Chad: "going on the
    // road, I don't see sparks"). First pass of the PACKET_B §11 spark
    // line: the carbide runners grinding plowed road / bare rock throw
    // short additive streaks from each SKI contact, scaled by ground
    // speed. Render-only, hashed off frame_count -- no particle state, no
    // kernel feedback. The S5 roost rung replaces this with the real
    // roost-coupled system.
    if (info.sled_active && info.sled_surface != nullptr &&
        (std::strcmp(info.sled_surface, "ROAD") == 0 ||
         std::strcmp(info.sled_surface, "ROCK") == 0) &&
        info.sled_speed_ms > 3.0) {
        const glm::dvec3 sp = info.sled_pos;
        const glm::dmat3 sR = info.sled_basis;
        const glm::dvec3 by = sR[1];
        const double my = -(info.sled_cg_h - info.sled_rest);
        const double spd01 = std::min(1.0, info.sled_speed_ms / 25.0);
        const int n_per = 3 + static_cast<int>(5.0 * spd01);
        BeginBlendMode(BLEND_ADDITIVE);
        for (int s = 0; s < 2; ++s) {
            const glm::dvec3 mount((s == 0 ? -0.5 : 0.5) * info.sled_stance, my,
                                   -info.sled_ski_fwd);
            const glm::dvec3 c =
                sp + sR * mount - by * (info.sled_rest - info.sled_susp_x[s]);
            for (int i = 0; i < n_per; ++i) {
                // cheap per-frame hash: position-stable within a frame,
                // fresh scatter every frame (sparks live one frame)
                const unsigned h =
                    static_cast<unsigned>((info.frame_count * 2654435761u) ^
                                          (s * 97u) ^ (i * 131071u));
                const double r1 = ((h >> 3) & 1023) / 1023.0;
                const double r2 = ((h >> 13) & 1023) / 1023.0;
                const double r3 = ((h >> 23) & 511) / 511.0;
                // streak: backward along +bz (behind the ski), fanned
                // sideways and up, longer with speed
                const glm::dvec3 dir = glm::normalize(
                    sR * glm::dvec3(0.35 * (r1 - 0.5), 0.15 + 0.45 * r2,
                                    0.6 + 0.4 * r3));
                const double len = (0.15 + 0.55 * r2) * (0.4 + 0.6 * spd01);
                const glm::dvec3 a = c + sR * glm::dvec3(0.0, 0.02, 0.0);
                const unsigned char al =
                    static_cast<unsigned char>(140 + 100 * r1);
                DrawCylinderEx(rel(a, pose.eye), rel(a + dir * len, pose.eye),
                               0.010f, 0.004f, 4,
                               r3 > 0.6 ? Color{255, 240, 170, al}
                                        : Color{255, 160, 60, al});
            }
        }
        EndBlendMode();
    }
    // ★ R5 rows 4/5/6 — ROOST / EXHAUST / RIDER BREATH. One sorted billboard
    // batch (render/sled_plumes_draw.cpp), drawn LAST in the world pass so the
    // depth test has the machine, rider and snow in the buffer to occlude
    // against; depth-write stays off so puffs never punch holes in each other.
    // App owns the trails (info.sled_plumes, the wingtip_smoke precedent);
    // this call reads them and touches nothing.
    if (info.sled_plumes != nullptr) {
        static SledPlumesRenderer g_sled_plumes_r;
        draw_sled_plumes(g_sled_plumes_r, *info.sled_plumes, pose.eye);
    }
    EndMode3D();
    // Restore the init_draw() clip planes the moment the sight pass ends.
    if (sight_near) rlSetClipPlanes(2.0, 60000.0);

    // Resolve the captured scene FBO through the post chain onto the
    // backbuffer. SEADS_POST_DEBUG: 0 normal · 1 linearized-depth viz (proves
    // the depth-tex FBO is alive) · 2 split-tone saturation mask (mono->black,
    // chroma planes-> white) · 3 S6 DoF circle-of-confusion viz (black=crisp
    // near/mid, white=full far blur — proves the far-only CoC ramp).
    if (post_captured) {
        // ★ SC1 (WINTER_LAW §3.7, SC1_COLD_SPEC.md §2): the night-blend
        // computation that used to live HERE (sun elevation at the camera
        // EYE, dot(-sun_travel, up)) is now computed ONCE by app/main.cpp,
        // "up at the PLAYER" instead of the eye, and fed to set_post_night
        // directly (render/celestial.h carries the authorised-exception
        // ruling for this seam) — so it can ALSO feed world::air_temp_c off
        // the exact same scalar (one sun, one night, one temperature).
        // render/ no longer computes a sun-elevation scalar of its own; this
        // call site only resolves the post chain.
        static const int post_debug = [] {
            const char* e = std::getenv("SEADS_POST_DEBUG");
            return e ? std::atoi(e) : 0;
        }();
        post_end_and_resolve(post_sw, post_sh, info.frame_count, post_debug);
    }

    // Correct-frame HUD (SPEC §12): the readouts come through the ONE shared
    // render::flight_readout — velocity-relative AoA, local_up-aware bank/G.
    // The telemetry line itself lives in the bottom-left status stack now
    // (Chad: the top strip was unreadable); only the help line stays up top.
    // R6: env-aware so the G-tape reads the SPATIAL density the plant flies —
    // at a bubble edge a stale altitude n over-reads by 1/u exactly where the
    // pilot judges the wall (adversarial P1-2). Null env => the altitude n.
    const FlightReadout r = flight_readout(state, env, params);
    DrawText(info.raw_mode
                 ? "RAW: S/W pitch  A/D roll  Q/E yaw  Shift/Ctrl throttle  "
                   "RMB stick   [F1] instructor"
                 : "MOUSE aim   S/W A/D Q/E override   SPACE freelook   "
                   "Shift/Ctrl throttle   [F1] raw",
             12, 12, 16, Color{200, 200, 200, 180});
    // R4-FLY-7 thumb-binding diagnosis: the raylib code of every held mouse
    // button, live. Windows drivers disagree which code a physical thumb
    // fires — this line is how [input] thumb_*_button_* gets set right
    // (read the number here, edit game.toml, re-run). Nothing held = blank.
    if (info.mouse_buttons_held != 0) {
        char mb[64];
        int off = std::snprintf(mb, sizeof mb, "MB held:");
        for (int b = 0;
             b <= 7 && off > 0 && off < static_cast<int>(sizeof mb) - 4; ++b) {
            if (info.mouse_buttons_held & (1 << b)) {
                off += std::snprintf(mb + off, sizeof mb - off, " %d", b);
            }
        }
        DrawText(mb, 12, 34, 16, Color{255, 210, 120, 200});
    }
    // BUILD STAMP (Chad 2026-07-23, the "am I flying the right kernel?"
    // ambiguity — never again): the TREE identity, drawn always. Every
    // worktree's build says which kernel it is; a fly report without this
    // stamp visible is a report about an unknown kernel. Bottom-right is
    // free of any other tunnel HUD element (checked: the MB-held diagnostic
    // above lives top-left at (12,34); nothing else draws in this corner).
    DrawText("KERNEL v5 [reconcile]", GetScreenWidth() - 232,
             GetScreenHeight() - 26, 16, Color{255, 190, 60, 200});

    // ★ R3 SATURATION SWEEP READOUT (Chad, 2026-08-27: "I didnt see what the
    // ladder was at"). He flew the SEADS_R3_FULLDEPTH ladder and could not tell
    // which rung was on screen -- an A/B whose arms are indistinguishable is
    // not a measurement, and every verdict taken from it is unattributable. The
    // value is now ON SCREEN, never only in a console line he cannot see while
    // driving. Same DrawText idiom as every other debug readout in this file.
    // Hidden at the ship value so it costs a clean screenshot nothing.
    if (info.r3_full_depth > 0.0f) {
        char r3[96];
        std::snprintf(r3, sizeof r3, "R3 full_depth  %.2f m   [PgUp/PgDn]",
                      static_cast<double>(info.r3_full_depth));
        const int fs = 18;
        const int w = MeasureText(r3, fs);
        DrawRectangle(10, 54, w + 16, fs + 10, Color{12, 16, 26, 190});
        DrawRectangleLines(10, 54, w + 16, fs + 10, Color{90, 105, 130, 200});
        DrawText(r3, 18, 59, fs, Color{170, 225, 255, 240});
    }

    // ★ LIGHT TUNE PLATE (Chad 2026-08-17). Stacked directly ABOVE the build
    // stamp in the same otherwise-empty bottom-right corner, in the same
    // DrawText idiom as every other debug readout in this file (no new text
    // system). Drawn ONLY once info.tune_armed -- i.e. only after Chad has
    // pressed one of the tune keys -- so a pristine launch is pixel-identical
    // to the shipped look. The numbers printed here are exactly the numbers to
    // paste into config/world.toml; that is the whole point of the plate.
    if (info.tune_armed) {
        const int tfs = 16;
        const int row = tfs + 6;
        const int tw = 320;
        const int th = row * 6 + 14;
        const int tx = GetScreenWidth() - tw - 12;
        const int ty = GetScreenHeight() - 34 - th;
        DrawRectangle(tx, ty, tw, th, Color{12, 16, 26, 190});
        DrawRectangleLines(tx, ty, tw, th, Color{90, 105, 130, 200});
        const int lx = tx + 10;
        int ly = ty + 7;
        DrawText("LIGHT TUNE", lx, ly, tfs, Color{255, 190, 60, 220});
        ly += row;
        const Color sel{255, 235, 170, 245};  // selected row
        const Color dim{170, 190, 215, 190};  // unselected rows
        char tl[128];
        std::snprintf(tl, sizeof tl, "%s ground_day_gain    %.3f",
                      info.tune_dial == 0 ? ">" : " ", info.tune_day_gain);
        DrawText(tl, lx, ly, tfs, info.tune_dial == 0 ? sel : dim);
        ly += row;
        std::snprintf(tl, sizeof tl, "%s night_glow  x%.3f",
                      info.tune_dial == 1 ? ">" : " ", info.tune_glow_mul);
        DrawText(tl, lx, ly, tfs, info.tune_dial == 1 ? sel : dim);
        ly += row;
        // The triple itself, because THAT is what world.toml line 322 wants.
        std::snprintf(tl, sizeof tl, "   = [%.3f, %.3f, %.3f]",
                      info.tune_glow.r, info.tune_glow.g, info.tune_glow.b);
        DrawText(tl, lx, ly, tfs, info.tune_dial == 1 ? sel : dim);
        ly += row;
        std::snprintf(tl, sizeof tl, "%s moon.ground_gain   %.3f",
                      info.tune_dial == 2 ? ">" : " ", info.tune_moon_gain);
        DrawText(tl, lx, ly, tfs, info.tune_dial == 2 ? sel : dim);
        ly += row;
        // The FROST row is gone with the frost (Chad, 2026-08-17). The row is
        // kept as the key legend so the plate's height and layout do not move.
        DrawText("[L] dial   [,] [.]  -/+", lx, ly, tfs,
                 Color{170, 190, 215, 190});
    }

    // F9 RECORD (v5 kernel-v5-reconcile): a red dot + "REC" top-right while
    // capturing (clear of the bandit-free top strip and the bottom-right
    // build stamp above), plus a 3 s fading "SAVED ..." toast once F9 stops.
    // Pure HUD read of FrameInfo — draw_frame never touches the recorder.
    if (info.recording) {
        const int rx = GetScreenWidth() - 90;
        const int ry = 16;
        const double pulse = 0.5 + 0.5 * std::sin(GetTime() * 2.0 * PI * 2.0);
        const unsigned char al =
            static_cast<unsigned char>(160.0 + 95.0 * pulse);
        DrawCircleV(Vector2{static_cast<float>(rx), static_cast<float>(ry + 7)},
                    6.0f, Color{230, 40, 40, al});
        DrawText("REC", rx + 14, ry, 16, Color{230, 60, 60, 230});
    }
    if (info.rec_saved_at_s > -1e17) {
        const double t = (GetTime() - info.rec_saved_at_s) / 3.0;
        if (t >= 0.0 && t < 1.0) {
            char msg[96];
            std::snprintf(msg, sizeof msg, "SAVED %s", info.rec_saved_name);
            const unsigned char al =
                static_cast<unsigned char>((1.0 - t) * 230.0);
            const int mw = MeasureText(msg, 16);
            DrawText(msg, GetScreenWidth() - mw - 16, 38, 16,
                     Color{120, 230, 140, al});
        }
    }

    // Reticle/nose-marker pair (SPEC §9.2): the on-screen gap IS the
    // controller's error — to within S-reticle's hard-capped display ease
    // (info.reticle_dir; the raw aim is untouched upstream). Instructor mode
    // only — raw mode has no aim state.
    if (!info.raw_mode) {
        const int sw = GetScreenWidth();
        const int sh = GetScreenHeight();
        const double fovy_rad = static_cast<double>(cam.fovy) * PI / 180.0;
        const double aspect = static_cast<double>(sw) / sh;
        const glm::dvec3 cam_forward = glm::normalize(pose.target - pose.eye);
        // FLOAT pixel coords (S-reticle): the smoothed sub-pixel motion must
        // not be re-quantized by an int cast; reticle, freelook ring, and
        // nose marker all take the same float path so the §9.2 pair degrades
        // symmetrically. Same lens-shift subtraction as before (NDC +y = up).
        const auto to_pxf = [&](const ScreenPoint& p) {
            return Vector2{static_cast<float>((p.x * 0.5 + 0.5) * sw),
                           static_cast<float>(
                               (0.5 - (p.y - info.lens_shift_ndc) * 0.5) * sh)};
        };
        // REC-6 — STYLIZED COCKPIT FRAME (the steady-state REST FRAME, drawn
        // UNDER all the HUD cues below): a subtle, SCREEN-ANCHORED peripheral
        // canopy interior. It is STATIC (does NOT ride the scene / lens shift —
        // that is the whole point: a stationary peripheral reference suppresses
        // vection). NDC -> pixels WITHOUT the lens-shift subtraction (unlike
        // to_pxf). alpha 0 => SKIPPED (strict superset). Dark steel-blue base
        // with an ember-orange accent on the INNER line of each doubled strut.
        if (info.cue_cockpit_alpha > 0.0) {
            const auto to_px_static = [&](const glm::dvec2& n) {
                return Vector2{static_cast<float>((n.x * 0.5 + 0.5) * sw),
                               static_cast<float>((0.5 - n.y * 0.5) * sh)};
            };
            const unsigned char a = static_cast<unsigned char>(
                std::clamp(info.cue_cockpit_alpha, 0.0, 1.0) * 255.0);
            const Color steel{60, 75, 95, a};                          // base
            const Color ember{kCueEmberR, kCueEmberG, kCueEmberB, a};  // accent
            const CockpitFrame fr = cockpit_frame(aspect);
            for (int i = 0; i < fr.count; ++i) {
                DrawLineEx(to_px_static(fr.segs[i].a),
                           to_px_static(fr.segs[i].b), 1.5f,
                           fr.segs[i].inner ? ember : steel);
            }
        }
        // S-cues (comfort program): peripheral, world-stable orientation cues,
        // drawn UNDER the reticle/nose/pipper layer (they are the backdrop the
        // pilot glances at, never at the reticle). alpha 0 => SKIPPED entirely,
        // so the shipped default frame is bit-identical (strict superset).
        // Cue A — GHOST HORIZON: the local LEVEL line (⊥ local_up), recomputed
        // fresh from normalize(position) every frame (SPEC §6.1, never cached),
        // with a center exclusion gap (nothing where the pilot aims).
        if (info.cue_horizon_alpha > 0.0) {
            // local_up taken at the AIRCRAFT (state.position), matching the
            // control::extract phi frame — NOT the eye at chase distance. The
            // two differ by ~0.1° at chase range on R=15 km; using the airframe
            // point keeps the ghost line's "up" the same up the bank arc reads.
            const glm::dvec3 local_up = glm::normalize(state.position);
            const GhostHorizonResult gh = ghost_horizon_segments(
                cam_forward, pose.up, local_up, fovy_rad, aspect,
                info.lens_shift_ndc, info.cue_horizon_gap_frac);
            const unsigned char a = static_cast<unsigned char>(
                std::clamp(info.cue_horizon_alpha, 0.0, 1.0) * 255.0);
            // v5 WHITE-INNER / ORANGE-OUTER (Chad 2026-07-23: "white inner
            // and orange outer so a little thicker, subtle cue" — replaces
            // the flat SLAG-ORANGE main line with a classic outlined-line
            // read: a thicker, dimmer orange stroke drawn FIRST, then a
            // thinner white stroke on top). The ladder rungs below stay the
            // single-tone DIMMER ember, unchanged — the outline is only on
            // the primary horizon line so it doesn't compete with the
            // reticle.
            const Color hc_outer{
                kCueSlagR, kCueSlagG, kCueSlagB,
                static_cast<unsigned char>(a * 0.5)};  // subtle, kept dim
            const Color hc_inner{255, 255, 255, a};    // white inner stroke
            // Draw a set of NDC segments (with sky-side ticks) at a stroke.
            const auto stroke_segs = [&](const GhostHorizonResult& g,
                                         const Color& col, float th) {
                for (int i = 0; i < g.count; ++i) {
                    const ScreenPoint pa{g.segs[i].a.x, g.segs[i].a.y, true};
                    const ScreenPoint pb{g.segs[i].b.x, g.segs[i].b.y, true};
                    DrawLineEx(to_pxf(pa), to_pxf(pb), th, col);
                    // Sky-side tick: points toward projected +local_up (UP
                    // upright, DOWN inverted) — resolves the line's up/down
                    // ambiguity. Same alpha as the line.
                    const ScreenPoint tr{g.segs[i].tick_root.x,
                                         g.segs[i].tick_root.y, true};
                    const ScreenPoint tk{g.segs[i].tick.x, g.segs[i].tick.y,
                                         true};
                    DrawLineEx(to_pxf(tr), to_pxf(tk), th, col);
                }
            };
            stroke_segs(gh, hc_outer, 3.0f);  // orange outer, thicker, first
            stroke_segs(gh, hc_inner, 1.5f);  // white inner, on top

            // GHOST PITCH LADDER (the angle indicator): SHORT rungs at ±15/±30/
            // ±45° elevation, in the DIMMER ember at 0.7x the horizon alpha, so
            // they read as ladder rungs backing the main line. Gated by the
            // SAME cue_horizon_alpha dial (the ladder is part of the horizon
            // cue — no new dial). Each rung reuses the ghost-horizon math via
            // ghost_ladder (elev=0 special case IS the horizon); out-of-view
            // rungs emit 0 segments.
            const unsigned char la = static_cast<unsigned char>(
                std::clamp(info.cue_horizon_alpha, 0.0, 1.0) * 0.7 * 255.0);
            const Color lc{kCueEmberR, kCueEmberG, kCueEmberB, la};
            constexpr double kDeg = 3.14159265358979323846 / 180.0;
            for (double elev_deg : {15.0, -15.0, 30.0, -30.0, 45.0, -45.0}) {
                const GhostHorizonResult rung =
                    ghost_ladder(cam_forward, pose.up, local_up, fovy_rad,
                                 aspect, info.lens_shift_ndc,
                                 info.cue_horizon_gap_frac, elev_deg * kDeg);
                stroke_segs(rung, lc, 1.5f);
            }
        }
        // Cue B — BANK ARC (Falcon 4.0 convention): a small arc at the TOP
        // center with ticks at 0/±10/±20/±30/±45/±60° and a moving pointer
        // showing aircraft bank φ (from the SHARED extraction via
        // flight_readout, single source, SPEC §7). Screen-anchored periphery.
        if (info.cue_bank_arc_alpha > 0.0) {
            // FULL-RANGE bank (±π), reusing the readout `r` above: the folded
            // ±90° phi reads WINGS LEVEL while inverted (readout.h: never feed
            // a roll gauge phi). bank_full unfolds through 180° so the pointer
            // pegs hard-over past the scale instead of snapping to center.
            const double phi = r.bank_full;
            const BankArcGeometry ba = bank_arc_geometry(
                phi, /*arc_span_deg=*/60.0,
                /*arc_sweep_rad=*/1.2217);  // 1.2217 rad = 70° total sweep
            const unsigned char a = static_cast<unsigned char>(
                std::clamp(info.cue_bank_arc_alpha, 0.0, 1.0) * 255.0);
            // SLAG-ORANGE recolor: the arc frame + ticks are the DIMMER ember;
            // the moving bank pointer is the BRIGHT slag (matches the horizon
            // line — the two primary attitude reads share the bright tone).
            const Color ac{kCueEmberR, kCueEmberG, kCueEmberB, a};
            const Color pc{kCueSlagR, kCueSlagG, kCueSlagB, a};
            // Layout: pivot below the top edge, arc radius in pixels. The pure
            // dir vectors are in +y-DOWN screen coords, so pivot + r*dir lands
            // the ticks along the top arc.
            const float cx = static_cast<float>(sw) * 0.5f;
            const float radius = static_cast<float>(sh) * 0.14f;
            const float pivot_y = static_cast<float>(sh) * 0.03f + radius;
            const Vector2 pivot{cx, pivot_y};
            for (int i = 0; i < ba.tick_count; ++i) {
                const BankTick& tk = ba.ticks[i];
                const float rin = tk.major ? radius - 12.0f : radius - 7.0f;
                const Vector2 p0{pivot.x + static_cast<float>(tk.dir.x) * rin,
                                 pivot.y + static_cast<float>(tk.dir.y) * rin};
                const Vector2 p1{
                    pivot.x + static_cast<float>(tk.dir.x) * radius,
                    pivot.y + static_cast<float>(tk.dir.y) * radius};
                DrawLineEx(p0, p1, tk.major ? 2.0f : 1.5f, ac);
            }
            // The moving pointer (a caret from the arc inward toward the
            // pivot).
            const Vector2 tip{pivot.x + static_cast<float>(ba.pointer_dir.x) *
                                            (radius + 2.0f),
                              pivot.y + static_cast<float>(ba.pointer_dir.y) *
                                            (radius + 2.0f)};
            const Vector2 base{pivot.x + static_cast<float>(ba.pointer_dir.x) *
                                             (radius - 16.0f),
                               pivot.y + static_cast<float>(ba.pointer_dir.y) *
                                             (radius - 16.0f)};
            DrawLineEx(base, tip, 3.0f, pc);
        }

        // S-carets (comfort program, REC-2): screen-edge threat indicators for
        // OFF-SCREEN drones/bandits — a small peripheral caret pointing where
        // to TURN to face the threat, so multi-bogey awareness needs no
        // disorienting freelook excursion. Pure HUD read of the SAME drone data
        // the gunsight uses (info.drones_draw + pose.eye); no new state. alpha
        // 0
        // => the whole pass is SKIPPED (strict superset). Drawn UNDER the
        // reticle layer (peripheral backdrop, never at the reticle — S-retclamp
        // lesson: a cue where the pilot AIMS is in the loop).
        if (info.cue_caret_alpha > 0.0 && !info.drones_draw.empty()) {
            const unsigned char a = static_cast<unsigned char>(
                std::clamp(info.cue_caret_alpha, 0.0, 1.0) * 255.0);
            const Color cc{240, 120, 90,
                           a};  // threat tint (low-sat red-orange)
            constexpr double kEdgeMarginNdc = 0.06;  // inset from the very edge
            constexpr double kCaretHystNdc = 0.05;   // P1-3 anti-strobe band
            // P1-3: per-fleet-slot caret shown-state, so the on_screen boundary
            // is HYSTERETIC (the gunsight target-pick precedent — every
            // instrument-selection gate is hysteretic). This is display-only
            // cosmetic state (like the planet/sun statics above); it feeds no
            // control/sim/aim path. Keyed by drones_draw index; sized
            // generously and reset if the fleet count ever exceeds it.
            constexpr int kMaxCaretSlots = 64;
            static std::array<bool, kMaxCaretSlots> s_caret_shown{};
            // P1-2: skip the engaged target ONLY when its pipper actually drew
            // — engaged AND the lead projects in front (the pipper only draws
            // in_front). An engaged bandit BEHIND you draws NO pipper, so it
            // must still get a caret (the worst SA hole). Identity skip by
            // index, not a lead-cone angle match.
            const bool pipper_drew =
                info.gunsight_active && info.gunsight_has_target &&
                project_dir(info.gunsight_lead, cam_forward, pose.up, fovy_rad,
                            aspect)
                    .in_front;
            const int engaged = pipper_drew ? info.gunsight_target_index : -1;
            const int nd = static_cast<int>(info.drones_draw.size());
            for (int di = 0; di < nd; ++di) {
                if (di == engaged) continue;  // its pipper diamond covers it
                // CONQUEST no-respawn: a killed wreck draws NO threat caret
                // (2026-07-26 ghost fix — see the BANDIT-tag loop below).
                if (di < static_cast<int>(info.drones_alive.size()) &&
                    info.drones_alive[di] == 0)
                    continue;
                const sim::SimState& dstate = info.drones_draw[di];
                const glm::dvec3 to = dstate.position - pose.eye;
                const double rng = glm::length(to);
                if (!(rng > 1e-6)) continue;
                const glm::dvec3 dir = to / rng;
                const bool prev_shown =
                    di < kMaxCaretSlots ? s_caret_shown[di] : false;
                const EdgeCaret ec =
                    edge_caret(dir, cam_forward, pose.up, fovy_rad, aspect,
                               info.lens_shift_ndc, kEdgeMarginNdc, prev_shown,
                               kCaretHystNdc);
                if (di < kMaxCaretSlots) s_caret_shown[di] = !ec.on_screen;
                if (ec.on_screen) continue;  // visible in view: no caret needed
                // Nearer = bigger (faint 1/r scale, clamped): a 1500 m
                // reference reads full size, clamped to [0.6, 1.6] so far/near
                // stay legible.
                const double sc = std::clamp(1500.0 / rng, 0.6, 1.6);
                const float len = static_cast<float>(16.0 * sc);  // px, tip len
                const float wid =
                    static_cast<float>(9.0 * sc);  // px, base half
                const ScreenPoint ep{ec.edge.x, ec.edge.y, true};
                const Vector2 base_px = to_pxf(ep);
                // Pointing direction: ec.angle is DISPLAY-NDC (+y up); pixel y
                // is DOWN, so negate the y-component for the on-screen vector.
                const double ca = std::cos(ec.angle);
                const double sa = std::sin(ec.angle);
                const Vector2 pdir{static_cast<float>(ca),
                                   static_cast<float>(-sa)};
                const Vector2 perp{-pdir.y, pdir.x};
                // A filled triangle: tip along pdir, base straddling perp.
                const Vector2 tip{base_px.x + pdir.x * len,
                                  base_px.y + pdir.y * len};
                Vector2 bl{base_px.x - perp.x * wid, base_px.y - perp.y * wid};
                Vector2 br{base_px.x + perp.x * wid, base_px.y + perp.y * wid};
                // raylib backface-culls: keep the SAME screen winding as the
                // working AoA triangle above (signed area < 0 in +y-down px
                // coords) regardless of which way the caret points.
                const float area = (bl.x - tip.x) * (br.y - tip.y) -
                                   (bl.y - tip.y) * (br.x - tip.x);
                if (area > 0.0f) std::swap(bl, br);
                DrawTriangle(tip, bl, br, cc);
            }
        }

        const glm::dvec3 nose = state.orientation * glm::dvec3{0.0, 0.0, -1.0};
        const ScreenPoint ret = project_dir(info.reticle_dir, cam_forward,
                                            pose.up, fovy_rad, aspect);
        const ScreenPoint nos =
            project_dir(nose, cam_forward, pose.up, fovy_rad, aspect);

        // v5 NOSE CROSSHAIR (Chad 2026-07-23: "my dot a crosshair and bright
        // red"): four short open-center strokes along the screen axes,
        // replacing the old green dot. Open center so it never occludes the
        // pixel it marks.
        // ★ STING: while the drone flies, the AIRCRAFT's pair stands down —
        // the man is not in that cockpit, and two nose/reticle pairs on one
        // screen is exactly the ambiguity §9.2 exists to prevent. The Sting
        // draws its OWN pair (same styles) in its HUD block below.
        if (nos.in_front && !info.sting_flying) {
            const Vector2 nc = to_pxf(nos);
            constexpr Color kNoseRed{255, 40, 40, 235};
            constexpr float kNoseIn = 3.0f, kNoseOut = 9.0f;
            DrawLineEx({nc.x + kNoseIn, nc.y}, {nc.x + kNoseOut, nc.y}, 2.0f,
                       kNoseRed);
            DrawLineEx({nc.x - kNoseIn, nc.y}, {nc.x - kNoseOut, nc.y}, 2.0f,
                       kNoseRed);
            DrawLineEx({nc.x, nc.y + kNoseIn}, {nc.x, nc.y + kNoseOut}, 2.0f,
                       kNoseRed);
            DrawLineEx({nc.x, nc.y - kNoseIn}, {nc.x, nc.y - kNoseOut}, 2.0f,
                       kNoseRed);
        }
        // v5 NEON RETICLE (Chad 2026-07-23: "my circle bigger and more neon
        // green"): radius 9->14 px, two concentric strokes (13.5/14.5) so it
        // reads thick/glowy with no shader. KERNEL COUPLING: [capture]
        // circle_deg in config/controller.toml is DERIVED from this drawn
        // radius (see that key's comment — it was 9 px = 0.551 deg; it is
        // NOT retuned here, a kernel dial is Chad's alone). The toml carries
        // a matching note that the drawn ring (14 px, ~0.86 deg) is now
        // deliberately a bit larger than the flown 0.55-deg capture circle,
        // pending Chad's ruling.
        constexpr Color kAimNeon{57, 255, 60, 255};
        if (ret.in_front && !info.sting_flying) {
            const Vector2 rc = to_pxf(ret);
            // v5 NEON RETICLE (Chad 2026-07-23 restyle, superseding the prior
            // 2026-07-15 "aimer matches the plane's livery" ruling): a
            // brighter, thicker double-stroke green ring reads better than
            // the single livery-tinted circle it replaces — see the
            // kAimNeon/circle_deg kernel-coupling note above this block.
            DrawCircleLinesV(rc, 13.5f, kAimNeon);
            DrawCircleLinesV(rc, 14.5f, kAimNeon);
            // Freelook: the reticle nests in a cursor ring — the "aim locked"
            // cue (SPEC §9.2). The held aim sits inside the mouse cursor.
            if (info.freelook) {
                DrawCircleLinesV(rc, 24.0f, Color{255, 220, 120, 200});
            }
        }

        // v5 SPLIT-S FEAR (Chad: "gets scared the closer you go to the split
        // s point"): a render-only proximity read on the TRUE aim (the same
        // direction the reticle projects, info.reticle_dir) vs the local
        // horizon, smoothstepped between the two named elevation thresholds
        // above. Computed UNCONDITIONALLY (both the on-screen docked mouse
        // and the off-screen mouse read it) — cheap, and the split-S dive is
        // usually flown with the aim well within the visible box, so gating
        // it on off_screen would silently mute the mouse-glyph's whole
        // purpose.
        const glm::dvec3 fear_local_up = glm::normalize(state.position);
        const glm::dvec3 fear_aim_dir = glm::normalize(info.reticle_dir);
        const double fear_elev = glm::dot(fear_aim_dir, fear_local_up);
        const float fear01 = static_cast<float>(
            aim_buddy::smoothstep01(aim_buddy::kMouseFearStart,
                                    aim_buddy::kMouseFearCommit, fear_elev));
        const double now_s = GetTime();

        // BRACE-FOR-IMPACT (Chad 2026-07-24): a SEPARATE render-only scare
        // read keyed to deck proximity + closing rate, not the split-S
        // lateral knife edge above. AGL from the SAME single-source terrain
        // query sim::ground_contact uses (world::HeightField::radius_at) —
        // env->ground when live, else the bare-sphere radius params.R (the
        // "R3 all-null" fallback, same discipline as the crash surface).
        // descent_rate is the closing speed toward the LOCAL ground plane
        // (positive while sinking); tti (time-to-impact) ramps the brace in
        // from 4.5 s out to fully braced under ~1.5 s. Climbing/level flight
        // (descent_rate <= 0) reads 0 — no brace on a pull-up or a level
        // pass, only while actually diving at the deck.
        const double ground_r = (env != nullptr && env->ground != nullptr)
                                    ? env->ground->radius_at(fear_local_up)
                                    : params.R;
        const double agl_m = glm::length(state.position) - ground_r;
        const double descent_rate = -glm::dot(state.velocity, fear_local_up);
        double brace01_d = 0.0;
        if (descent_rate > 0.0 && agl_m > 0.0) {
            const double tti = agl_m / std::max(descent_rate, 1.0);
            brace01_d = 1.0 - aim_buddy::smoothstep01(1.5, 4.5, tti);
        }
        const float brace01 =
            static_cast<float>(std::clamp(brace01_d, 0.0, 1.0));

        // v5 ON-SCREEN scared mouse (Chad's fear read is valuable on-screen
        // too, not just as the off-screen direction marker): when the aim is
        // ON screen AND fear01 > 0.3, dock a SMALL (~22 px) scared mouse just
        // outside the reticle ring, bottom-right of it. Below 0.3 on-screen,
        // no mouse at all — the "cool mouse" ONLY ever appears as the
        // off-screen direction marker (policy: Chad framed the mouse as the
        // off-screen marker; the docked variant is purely the fear cue).
        if (ret.in_front && (fear01 > 0.3f || brace01 > 0.3f)) {
            const Vector2 rc = to_pxf(ret);
            const Vector2 dock{rc.x + 26.0f, rc.y + 20.0f};
            const Vector2 to_ret{rc.x - dock.x, rc.y - dock.y};  // tiny tick
                                                                 // back at
                                                                 // the ring
            aim_buddy::draw_aim_buddy(dock, to_ret, fear01, brace01, now_s,
                                      /*scale=*/0.69f);
        }

        // v5 OFF-SCREEN AIM MARKER (Chad 2026-07-23: originally "a little red
        // arrow"; RESTYLED same day into the scared-mouse glyph — "my marker
        // for off screen a little mouse... embossed in white outlined in
        // black and stylish accents like sunglasses and buckteeth"): when
        // the TRUE aim is off screen (behind the camera, or in front but
        // outside the visible box), the mouse appears at the screen edge,
        // upright, pointing (via its short tick) toward the aim. PURE
        // DISPLAY ADDITION — the reticle is never moved, clamped, or
        // substituted (the S-retclamp lesson stands: a pinned edge MARKER
        // replacing the reticle under-reports the error; the mouse ADDS the
        // missing read while the true reticle stays raw). Direction from the
        // SHARED camera screen basis (never a re-derived projection — the
        // projection-basis-fork lesson); this is a raw orthographic
        // perpendicular-component direction (dot with right/up, NO
        // perspective divide by the forward component), which is why it
        // stays correct even when dot(aim, forward) < 0 (behind the camera):
        // a perspective-divided x/y WOULD flip sign there, but this raw
        // (dx, dy) = (dot(aim,right), dot(aim,up)) is exactly sin/cos of the
        // aim's azimuth about the camera forward at every angle, continuous
        // through the +/-90 deg boundary — see the investigation note in the
        // handoff for the worked examples.
        {
            const bool off_screen = !ret.in_front || std::abs(ret.x) > 1.0 ||
                                    std::abs(ret.y - info.lens_shift_ndc) > 1.0;
            const ScreenBasis sb = camera_screen_basis(cam_forward, pose.up);
            if (off_screen && sb.ok) {
                const glm::dvec3 aim_n = glm::normalize(info.reticle_dir);
                const double dx = glm::dot(aim_n, sb.r);
                const double dy = glm::dot(aim_n, sb.u);
                const double dlen = std::sqrt(dx * dx + dy * dy);
                if (dlen > 1e-6) {
                    // Screen-space direction (y down) from center toward the
                    // aim; anchor the mouse on the screen rect inset by a
                    // margin (sized for the ~32 px glyph, not the old 8 px
                    // arrow half-width).
                    const double ux = dx / dlen, uy = -dy / dlen;
                    const double cx = sw * 0.5, cy = sh * 0.5;
                    const double margin = 34.0;
                    double t = 1e18;
                    if (std::abs(ux) > 1e-9)
                        t = std::min(t, (sw * 0.5 - margin) / std::abs(ux));
                    if (std::abs(uy) > 1e-9)
                        t = std::min(t, (sh * 0.5 - margin) / std::abs(uy));
                    const Vector2 anchor{static_cast<float>(cx + ux * t),
                                         static_cast<float>(cy + uy * t)};
                    const Vector2 dir_unit{static_cast<float>(ux),
                                           static_cast<float>(uy)};
                    aim_buddy::draw_aim_buddy(anchor, dir_unit, fear01, brace01,
                                              now_s);
                }
            }
        }

        // Fixed boresight reticle (Task B, iter-7 Model A): the HARMONIZED
        // gun sightline — the body-frame direction the canted rounds cross at
        // convergence range. This is the authentic Bf 109 Revi fixed pip:
        // locked to the airframe, stable in steady flight. Visually distinct
        // from the §9.2 white aim circle (flight-control feedback) and the lead
        // diamond (deflection aid). Drawn as a bold cyan cross-hair pip.
        // Single-sourced with weapon::harmonization_rise via gunsight_boresight
        // set in app/main.cpp from orient*(0,0,-1) — the (0,0,-conv) sightline
        // the rounds cross, transformed by the shooter orientation.
        if (info.gunsight_active) {
            const ScreenPoint bp =
                project_dir(info.gunsight_boresight, cam_forward, pose.up,
                            fovy_rad, aspect);
            if (bp.in_front) {
                const Vector2 bc = to_pxf(bp);
                const float fx = bc.x;
                const float fy = bc.y;
                constexpr float arm = 6.0f;  // cross arm half-length
                constexpr float gap = 2.0f;  // center gap (open pip)
                const Color boreCol{80, 210, 240,
                                    230};  // cyan: distinct from white reticle
                                           // and green/amber diamond
                DrawLineEx({fx - arm, fy}, {fx - gap, fy}, 1.5f, boreCol);
                DrawLineEx({fx + gap, fy}, {fx + arm, fy}, 1.5f, boreCol);
                DrawLineEx({fx, fy - arm}, {fx, fy - gap}, 1.5f, boreCol);
                DrawLineEx({fx, fy + gap}, {fx, fy + arm}, 1.5f, boreCol);
                DrawCircleLinesV(bc, 3.5f, boreCol);  // small center ring
            }
        }

        // Lead-computer pipper (S8-drone): the meter's snapshot (computed once
        // per sim tick in app::tick) says WHERE to aim for a hit on the engaged
        // bandit; here we only PROJECT that world direction and draw it.
        //
        // Chad's 2-state cue (2026-07-13): the diamond is WHITE while you are
        // OFF the solution, and snaps RED the instant your aim sits over the
        // computed lead point (gunsight_on_target — which already requires a
        // VALID, in-range, in-cone solution, so RED means "guns on, fire NOW").
        // Off-solution or out-of-envelope both read WHITE — the diamond only
        // goes hot when a shot fired now would connect. (This replaces the
        // older green/amber/red 3-state; out_of_envelope is folded into "stays
        // white" because on_target can never be true out of the lethal
        // envelope.)
        if (info.gunsight_active && info.gunsight_has_target) {
            // Project the lead POINT from the EYE (parallax-correct, like the
            // bandit markers) — NOT the lead direction at infinity. The diamond
            // then sits ahead of the drawn bandit by the true lead. Fable
            // 2026-07-13: the old project_dir floated it 23-39 mrad high and
            // swung with bank ("pipper above the plane").
            const glm::dvec3 to_lp = info.gunsight_lead_point - pose.eye;
            const double lp_dist = glm::length(to_lp);
            const ScreenPoint lp =
                lp_dist > 1e-6 ? project_dir(to_lp / lp_dist, cam_forward,
                                             pose.up, fovy_rad, aspect)
                               : project_dir(info.gunsight_lead, cam_forward,
                                             pose.up, fovy_rad, aspect);
            if (lp.in_front) {
                const Vector2 lc = to_pxf(lp);  // float path (was int-cast;
                                                // it drew float lines anyway)
                const Color col =
                    info.gunsight_on_target
                        ? Color{240, 45, 45, 255}
                        // RED: aim on the lead point — fire
                        : Color{245, 245, 245, 230};  // WHITE: off the solution
                // Thicken + brighten the moment it goes hot so the RED snap is
                // unmistakable in a fast dogfight (a hair heavier than the
                // white).
                const float th = info.gunsight_on_target ? 2.5f : 2.0f;
                const float fx = lc.x;
                const float fy = lc.y;
                constexpr float d = 8.0f;  // diamond half-extent
                DrawLineEx({fx - d, fy}, {fx, fy - d}, th, col);
                DrawLineEx({fx, fy - d}, {fx + d, fy}, th, col);
                DrawLineEx({fx + d, fy}, {fx, fy + d}, th, col);
                DrawLineEx({fx, fy + d}, {fx - d, fy}, th, col);
            }
        }

        // FLAK SIGHT (spec §7.5): while manned, the solved lead pipper drawn
        // INSIDE the physical ring's view -- a small double circle, visually
        // distinct from the aircraft's diamond. WHITE ring = solution live;
        // it is the ANSWER the mils ring frames (the Flakvisier licence,
        // red-team P0-1). Plus the drum readout.
        if (info.flak_manned) {
            // The pipper's screen point doubles as the hit-marker anchor.
            bool have_fc = false;
            Vector2 fc{0.0f, 0.0f};
            if (info.flak_lead_valid) {
                const glm::dvec3 to_fp = info.flak_lead_point - pose.eye;
                const double fp_dist = glm::length(to_fp);
                if (fp_dist > 1e-6) {
                    const ScreenPoint fp =
                        project_dir(to_fp / fp_dist, cam_forward, pose.up,
                                    fovy_rad, aspect);
                    if (fp.in_front) {
                        fc = to_pxf(fp);
                        have_fc = true;
                        // ★★★ THE PIPPER IS THE ALLY BLUE, AND IT IS THIN
                        // (Chad, 2026-09-01: "a neon color coded ... the
                        // correct blue as neon ... the thickness of the
                        // pipper occludes the view of the enemy a little
                        // bit"). QUERIED, never picked: this is
                        // [teams] ally from render/team_color.h -- the exact
                        // 180 deg HSV complement of the slag orange, the one
                        // cross-layer palette the map, the liveries and the
                        // beacons all read. There is deliberately no second
                        // colour table (CLAUDE.md H1), so a bare RGB here
                        // would be the fork that rule forbids. Full value +
                        // saturation IS the neon.
                        const glm::dvec3 ab = render::team_colors().ally;
                        const auto ch8 = [](double v) {
                            return static_cast<unsigned char>(
                                std::clamp(v, 0.0, 1.0) * 255.0);
                        };
                        const Color fcol{ch8(ab.r), ch8(ab.g), ch8(ab.b), 235};
                        // The ring is a hairline and the centre pip lost
                        // 40% of its radius: what he is shooting at has to
                        // stay visible THROUGH the mark that frames it.
                        DrawCircleLinesV({fc.x, fc.y}, 7.0f, fcol);
                        DrawCircleV({fc.x, fc.y}, 1.1f, fcol);
                    }
                    // Env-gated pipper-projection probe (SEADS_FLAK_ZOOM_DBG
                    // — works in a REAL drive too, by design: RMB zoom is
                    // hard-false headless so only Chad's hands can exercise
                    // it). getenv cached once — never per frame in render/.
                    static const bool fzdbg =
                        std::getenv("SEADS_FLAK_ZOOM_DBG") != nullptr;
                    if (fzdbg)
                        std::fprintf(stderr,
                                     "[FZDBG3] in_front=%d ndc=(%.3f,%.3f) "
                                     "px=(%.1f,%.1f) ls=%.3f\n",
                                     static_cast<int>(fp.in_front), fp.x,
                                     fp.y, fc.x, fc.y, info.lens_shift_ndc);
                }
            }
            // HIT / KILL CONFIRMATION (Chad 2026-08-29: "add a flak hit
            // indication somewhere"): four diagonal ticks -- the classic hit
            // X -- at the pipper, or at screen centre when the solution is
            // gone (a kill drops the sticky target the SAME frame, so the
            // kill X must not depend on a pipper that just died with it).
            // Hit = YELLOW quick flash; kill = red, larger, slower. Alpha
            // rides the app-side decaying envelopes, which are bumped only
            // by the PLAYER's own counters (AI-gun sweeps book into the
            // sink and cannot flash this).
            // ★ Round 3 (Chad 2026-08-30 "not sure if I saw hit markers" +
            // red-team round 2): the hit X shipped 235-alpha WHITE sitting
            // directly on the 245,245,245 cream reticle for 0.25 s -- near
            // camouflage. Warm yellow separates it from BOTH the reticle
            // and the red kill X; slightly larger/longer to be catchable.
            if (info.flak_hitmark > 0.02 || info.flak_killmark > 0.02) {
                const float mx = have_fc ? fc.x : sw * 0.5f;
                const float my = have_fc ? fc.y : sh * 0.5f;
                const auto ticks = [&](float r0, float r1, float th,
                                       Color c) {
                    const float iv = 0.7071068f;
                    for (int q = 0; q < 4; ++q) {
                        const float sx = (q & 1) ? 1.0f : -1.0f;
                        const float sy = (q & 2) ? 1.0f : -1.0f;
                        DrawLineEx({mx + sx * r0 * iv, my + sy * r0 * iv},
                                   {mx + sx * r1 * iv, my + sy * r1 * iv},
                                   th, c);
                    }
                };
                if (info.flak_hitmark > 0.02)
                    ticks(10.0f, 20.0f, 2.5f,
                          Color{255, 216, 64,
                                static_cast<unsigned char>(
                                    245.0 * info.flak_hitmark)});
                if (info.flak_killmark > 0.02)
                    ticks(12.0f, 26.0f, 3.0f,
                          Color{255, 70, 45,
                                static_cast<unsigned char>(
                                    245.0 * info.flak_killmark)});
            }
            char fl[64];
            if (info.flak_reload_left_s > 0.0)
                std::snprintf(fl, sizeof fl, "FLAK  RELOADING %.1f",
                              info.flak_reload_left_s);
            else
                std::snprintf(fl, sizeof fl, "FLAK  %d RDS",
                              info.flak_rounds_left);
            const int fw = MeasureText(fl, 22);
            DrawText(fl, sw / 2 - fw / 2, sh - 170, 22,  // clear of the snow-info + flaps stack
                     info.flak_reload_left_s > 0.0
                         ? Color{250, 190, 90, 235}
                         : Color{220, 235, 240, 220});
        }

        // ★ STING RPAS HUD (the sting lane). Two states, the flak counter's
        // own idiom (snprintf + MeasureText-centred + state-swapped colour):
        //  SHOULDERED — a launcher crosshair dead centre + the launch prompt.
        //  FLYING — battery %, turns left, launches left; the armed destruct
        //  countdown replaces the line in warning orange.
        if (info.sting_shouldered) {
            const int cx = sw / 2, cy = sh / 2;
            const Color xc{140, 210, 255, 235};
            DrawLine(cx - 14, cy, cx - 4, cy, xc);
            DrawLine(cx + 4, cy, cx + 14, cy, xc);
            DrawLine(cx, cy - 14, cx, cy - 4, xc);
            DrawLine(cx, cy + 4, cx, cy + 14, xc);
            DrawCircleLines(cx, cy, 22.0f, Color{140, 210, 255, 120});
            char sl[64];
            std::snprintf(sl, sizeof sl, "STING x%d  --  LEAD AND CLICK",
                          info.sting_left);
            const int sw_ = MeasureText(sl, 22);
            DrawText(sl, sw / 2 - sw_ / 2, sh - 196, 22,
                     Color{140, 210, 255, 235});
        }
        if (info.sting_flying) {
            // ★ THE STING'S OWN AIM PAIR — the aeroplane's exact styles
            // (neon double-ring reticle on the carried aim, open-center red
            // crosshair on the drone's nose), because it flies the same
            // cascade and the §9.2 law is the same: the on-screen gap IS the
            // controller's error.
            const ScreenPoint sret = project_dir(
                info.sting_aim_dir, cam_forward, pose.up, fovy_rad, aspect);
            const glm::dvec3 snose =
                info.sting_draw.orientation * glm::dvec3{0.0, 0.0, -1.0};
            const ScreenPoint snos =
                project_dir(snose, cam_forward, pose.up, fovy_rad, aspect);
            if (snos.in_front) {
                const Vector2 nc = to_pxf(snos);
                constexpr Color kNoseRed{255, 40, 40, 235};
                constexpr float kIn = 3.0f, kOut = 9.0f;
                DrawLineEx({nc.x + kIn, nc.y}, {nc.x + kOut, nc.y}, 2.0f,
                           kNoseRed);
                DrawLineEx({nc.x - kIn, nc.y}, {nc.x - kOut, nc.y}, 2.0f,
                           kNoseRed);
                DrawLineEx({nc.x, nc.y + kIn}, {nc.x, nc.y + kOut}, 2.0f,
                           kNoseRed);
                DrawLineEx({nc.x, nc.y - kIn}, {nc.x, nc.y - kOut}, 2.0f,
                           kNoseRed);
            }
            if (sret.in_front) {
                const Vector2 rc = to_pxf(sret);
                DrawCircleLinesV(rc, 13.5f, kAimNeon);
                DrawCircleLinesV(rc, 14.5f, kAimNeon);
            }
            // ★ fly-5 HUD REWORK (Chad: "I found the current hud hard to
            // see / read"): one dark BACKED panel bottom-centre, big type,
            // three rows — the plane's AIR-plate idiom (box + outline)
            // scaled up for the whole stack so it reads at a glance.
            //  row 1  SPD / ALT
            //  row 2  BATT / LEFT (or the destruct countdown, in orange)
            //  row 3  the DRONE's OWN air plate (fly-5: the plane's plate
            //         reads the parked aeroplane — a drone at the bubble
            //         edge needs its own), gated on bubble_live like the
            //         plane's.
            char sa[64];
            std::snprintf(sa, sizeof sa, "SPD %d   ALT %d",
                          static_cast<int>(info.sting_speed_mps),
                          static_cast<int>(info.sting_alt_m));
            char sl[96];
            if (info.sting_warn_s >= 0.0)
                std::snprintf(sl, sizeof sl, "DESTRUCT %.1f",
                              info.sting_warn_s);
            else
                std::snprintf(sl, sizeof sl, "BATT %d%%   LEFT %d",
                              static_cast<int>(info.sting_battery01 * 100.0),
                              info.sting_left);
            char sair[24];
            const int apct =
                static_cast<int>(info.sting_air_frac * 100.0 + 0.5);
            std::snprintf(sair, sizeof sair,
                          info.sting_air_frac < 0.15 ? "RPAS THIN AIR"
                                                     : "RPAS AIR %d%%",
                          apct);
            constexpr int kFsBig = 30, kFsRow = 26;
            const int w1 = MeasureText(sa, kFsRow);
            const int w2 = MeasureText(sl, kFsBig);
            const int w3 = info.bubble_live ? MeasureText(sair, kFsRow) : 0;
            // fly-6 (Chad): "offset that gauge cluster to the left where it
            // dosent block my view of my rpas" — the chase cam frames the
            // drone low-centre, so the panel lives at the LEFT EDGE now,
            // rows left-aligned, same vertical band.
            const int pw = std::max(w1, std::max(w2, w3)) + 36;
            const int ph = 14 + kFsRow + 8 + kFsBig +
                           (info.bubble_live ? 8 + kFsRow : 0) + 14;
            const int px = 24;
            const int py = sh - 170 - ph;
            const int tx = px + 18;  // rows left-aligned inside the panel
            const Color panel{14, 22, 30, 190};
            const Color edge{100, 170, 220, 170};
            DrawRectangle(px, py, pw, ph, panel);
            DrawRectangleLines(px, py, pw, ph, edge);
            int ry = py + 14;
            DrawText(sa, tx, ry, kFsRow, Color{235, 245, 250, 250});
            ry += kFsRow + 8;
            DrawText(sl, tx, ry, kFsBig,
                     info.sting_warn_s >= 0.0
                         ? Color{255, 110, 60, 255}
                         : (info.sting_battery01 < 0.25
                                ? Color{255, 190, 90, 255}
                                : Color{150, 220, 255, 255}));
            if (info.bubble_live) {
                ry += kFsBig + 8;
                const Color acol =
                    info.sting_air_frac < 0.15
                        ? Color{255, 70, 55, 255}
                        : info.sting_air_frac < 0.6
                              ? Color{255, 190, 90, 255}
                              : Color{120, 220, 140, 250};
                DrawText(sair, tx, ry, kFsRow, acol);
            }
        }

        // BANDIT markers + PER-BANDIT distance (Chad 2026-07-13: "put distance
        // markers on ALL bandits — I should see how far away all of them are").
        // For EVERY in-front drone: a red "BANDIT" tag above and its slant
        // range below in small legible green. The old label faded to INVISIBLE
        // past 3 km — the opposite of what a chase needs — so the range now
        // NEVER fades (always legible, that's the whole point), and the BANDIT
        // tag persists far enough out (~9 km) to keep the fleet trackable while
        // you run one down. READ-ONLY on drone state; same lens as the pipper.
        if (!info.drones_draw.empty()) {
            // The BASE tag/range glyph height. RUNG E4.2 floors the ON-SCREEN
            // size at [plane_legibility] tag_min_px (0 or anything <= the base
            // leaves these two exactly as they were), so a far contact's tag
            // can never shrink below readable.
            constexpr int kLabelFontSizeBase = 12;
            constexpr int kDistFontSizeBase = 12;
            const int kLabelFontSize = render::tag_font_px(
                kLabelFontSizeBase, info.legibility.tag_min_px);
            const int kDistFontSize = render::tag_font_px(
                kDistFontSizeBase, info.legibility.tag_min_px);
            // RUNG E4.2: a dark halo behind the glyphs. Chad's report is that
            // the tags wash out "especially in the white scatter light" — but a
            // tag also has to survive dark ground, so the fix is CONTRAST on
            // both sides: a near-black ring under the coloured glyph reads on
            // white AND the bright glyph reads on the ring. tag_outline_px = 0
            // draws NOTHING extra and the text call below is the original one.
            const int kTagHaloPx =
                static_cast<int>(std::lround(info.legibility.tag_outline_px));
            const auto draw_text_haloed = [&](const char* s, int tx, int ty,
                                              int fs, Color c) {
                if (kTagHaloPx > 0) {
                    // Alpha follows the glyph so a fading tag fades WITH its
                    // halo (a halo outliving its text is a black smudge).
                    const Color halo{8, 10, 12, c.a};
                    for (int oy = -kTagHaloPx; oy <= kTagHaloPx; ++oy)
                        for (int ox = -kTagHaloPx; ox <= kTagHaloPx; ++ox)
                            if (ox != 0 || oy != 0)
                                DrawText(s, tx + ox, ty + oy, fs, halo);
                }
                DrawText(s, tx, ty, fs, c);
            };
            constexpr double kLabelFadeStart =
                4000.0;  // [m] begin BANDIT-tag fade
            constexpr double kLabelFadeEnd =
                9000.0;  // [m] BANDIT tag gone (range still shows)
            constexpr double kLabelLiftPx =
                14.0;  // [px] lift above screen point
            // 2026-07-26 GHOST FIX (Chad: "I have ghosts after I kill that keep
            // the bandit tag but not have a body"). This loop was a range-for
            // over drones_draw, so it had NO INDEX and therefore never
            // consulted drones_alive -- every killed wreck kept its red BANDIT
            // tag and range readout hanging in the sky with nothing drawn under
            // it (the body pass, the pipper, and the map all guard; only this
            // label pass and the threat caret above did not). Indexed now, same
            // guard as every other consumer. Empty drones_alive => all drawn,
            // so the off-conquest path is bit-identical.
            const int nlab = static_cast<int>(info.drones_draw.size());
            for (int li = 0; li < nlab; ++li) {
                if (li < static_cast<int>(info.drones_alive.size()) &&
                    info.drones_alive[li] == 0)
                    continue;
                const sim::SimState& drone = info.drones_draw[li];
                const glm::dvec3 to_drone = drone.position - pose.eye;
                const double dist = glm::length(to_drone);
                if (dist < 1e-6) continue;
                const ScreenPoint dp = project_dir(to_drone / dist, cam_forward,
                                                   pose.up, fovy_rad, aspect);
                if (!dp.in_front) continue;
                const Vector2 dc = to_pxf(dp);
                // Red BANDIT tag above (fades with range to tame far clutter).
                const double fade_t = (dist - kLabelFadeStart) /
                                      (kLabelFadeEnd - kLabelFadeStart);
                const double alpha_f =
                    std::max(0.0, std::min(1.0, 1.0 - fade_t));
                if (alpha_f > 0.0) {
                    // game-AI-R4 (Chad: "every plane acted like an enemy"):
                    // a PLAYER-FACTION maverick is an ALLY — cool cyan tag,
                    // never the red BANDIT. Keyed on the per-drone HOSTILITY
                    // flag (DroneState::friendly_side, the round tag's own
                    // single source — red-team P1-1: raw faction painted a
                    // furball-hostile teammate cyan). List empty
                    // (non-conquest) => everyone is a BANDIT, bit-identical.
                    const bool ally =
                        li < static_cast<int>(info.drones_friendly.size()) &&
                        info.drones_friendly[li] != 0;
                    // Center (~6 px/char at the 12 px base). E4.2: the
                    // half-width scales with the FONT FLOOR so a bigger tag
                    // stays centred; at the base size the ratio is exactly
                    // 1.0f, so this is the original expression.
                    const float tag_k = static_cast<float>(kLabelFontSize) /
                                        static_cast<float>(kLabelFontSizeBase);
                    const float lx = dc.x - (ally ? 12.0f : 21.0f) * tag_k;
                    // The lift scales with the font floor too, or a taller
                    // glyph would hang down over the plane it labels. tag_k is
                    // exactly 1.0f at the base size => the original number.
                    const float ly =
                        dc.y - static_cast<float>(kLabelLiftPx) * tag_k;
                    // E4.2 alpha floor (Fable adjudication): floors the fade
                    // INSIDE the band only -- the alpha_f > 0.0 gate above
                    // means the 9 km cutoff and the ghost fix still kill the
                    // tag outright; 0 = today's fade bit-identically.
                    const unsigned char al =
                        static_cast<unsigned char>(std::max(
                            200.0 * alpha_f,
                            std::min(200.0, info.legibility.tag_min_alpha)));
                    // S-mapteam: the tag wears the TEAM colour, from the one
                    // palette — ally BLUE, enemy SLAG ORANGE, identical to the
                    // plane's own livery and to its map icon.
                    const auto tcol = [&](const glm::dvec3& c) {
                        const auto q = [](double v) {
                            return static_cast<unsigned char>(std::lround(
                                std::min(1.0, std::max(0.0, v)) * 255.0));
                        };
                        return Color{q(c.x), q(c.y), q(c.z), al};
                    };
                    // RUNG E4.2: the BANDIT tag wears the DERIVED legibility
                    // tint (render::enemy_legibility_tint — the same one
                    // authority the 3D livery and the E4.3 glint bead read, no
                    // second literal). enemy_tint_shift = 0 returns
                    // team_colors().enemy bit-identically, so the tag is
                    // today's tag. Allies stay on the ruled complement blue.
                    draw_text_haloed(
                        ally ? "ALLY" : "BANDIT", static_cast<int>(lx),
                        static_cast<int>(ly), kLabelFontSize,
                        tcol(ally ? render::faction_color(
                                        li < static_cast<int>(
                                                 info.drones_faction.size())
                                            ? info.drones_faction[li]
                                            : 0)
                                  : render::enemy_legibility_tint(
                                        render::team_colors().enemy,
                                        info.legibility.enemy_tint_shift)));
                }
                // Slant range below EVERY bandit — always legible green fine
                // print (no distance fade: the far ones are exactly the ones
                // you need the range for). Auto-formats m -> km past 1 km.
                char dline[32];
                if (dist >= 1000.0)
                    std::snprintf(dline, sizeof dline, "%.1f km",
                                  dist / 1000.0);
                else
                    std::snprintf(dline, sizeof dline, "%.0f m", dist);
                const int tw = MeasureText(dline, kDistFontSize);
                const int dx = static_cast<int>(dc.x) - tw / 2;  // center
                const int dy = static_cast<int>(dc.y) + 12;      // below bandit
                draw_text_haloed(dline, dx, dy, kDistFontSize,
                                 Color{120, 240, 140, 235});  // easy-to-see
            }
        }
    }

    // Time-on-target + range/closure readout (S8-drone Stage 3 + Task E):
    // the tracking number. Only when the gunsight is active.
    // Shows: lifetime on-target %, live range [m], closure rate [m/s] (+ =
    // closing), and a live ON TARGET cue when nose is on the solution.
    if (info.gunsight_active && info.gunsight_has_target) {
        char tline[128];
        const char* cue = info.gunsight_on_target ? "* ON TARGET *" : "";
        std::snprintf(tline, sizeof tline,
                      "TOT %3.0f%%   RNG %5.0f m   CLO %+5.0f m/s   %s",
                      info.tot_frac * 100.0, info.tot_range, info.tot_closure,
                      cue);
        DrawText(tline, 12, 36, 18,
                 info.gunsight_on_target ? Color{90, 240, 130, 255}
                                         : Color{230, 220, 160, 220});
    }

    // Kill count + kill flash (kill-loop HUD).
    if (info.kill_count > 0 || info.ticks_since_kill < 240) {
        const bool flash = info.ticks_since_kill < 240;
        char kline[64];
        std::snprintf(kline, sizeof kline, "KILLS  %lld", info.kill_count);
        const Color kcol =
            flash ? Color{255, 80, 80, 255} : Color{230, 220, 160, 220};
        DrawText(kline, 12, 60, 18, kcol);
    }

    // Player HEALTH bar + DEATHS + "DOWNED" flash (bandit combat AI). Drawn
    // only in a combat session (hp_max > 0). A framed bar whose fill drops with
    // damage and shifts green -> amber -> red as HP falls, so return fire is
    // legible at a glance. All display-only (reads the combat snapshot; no
    // writes).
    if (info.player_hp_max > 0.0) {
        const int bx = 12, by = 88, bw = 180, bh = 14;
        const double frac =
            std::clamp(info.player_hp / info.player_hp_max, 0.0, 1.0);
        // Fill color: green when healthy, amber mid, red when low.
        Color fill = (frac > 0.5)    ? Color{90, 220, 110, 235}
                     : (frac > 0.25) ? Color{240, 200, 70, 235}
                                     : Color{240, 70, 60, 235};
        DrawRectangle(bx - 2, by - 2, bw + 4, bh + 4, Color{20, 24, 28, 180});
        DrawRectangle(bx, by, static_cast<int>(bw * frac), bh, fill);
        DrawRectangleLines(bx, by, bw, bh, Color{200, 210, 220, 200});
        char hline[48];
        std::snprintf(hline, sizeof hline, "HP %3.0f", info.player_hp);
        DrawText(hline, bx + bw + 8, by - 2, 16, Color{220, 225, 230, 220});

        // Component damage panel (damage_model_plan.md): a small labelled
        // mini-bar per component so the pilot can see WHAT is hurt (a dead
        // engine vs a broken wing fly very differently). green -> amber -> red
        // as health falls.
        const auto comp_bar = [&](int row, const char* label, double h) {
            const int cy = by + bh + 6 + row * 16;
            const int cbx = bx + 34, cbw = 90, cbh = 10;
            const double f = std::clamp(h, 0.0, 1.0);
            const Color fc = (f > 0.5)    ? Color{90, 210, 110, 230}
                             : (f > 0.25) ? Color{235, 195, 70, 230}
                                          : Color{235, 70, 60, 230};
            DrawText(label, bx, cy - 1, 12, Color{200, 210, 220, 210});
            DrawRectangle(cbx - 1, cy - 1, cbw + 2, cbh + 2,
                          Color{20, 24, 28, 170});
            DrawRectangle(cbx, cy, static_cast<int>(cbw * f), cbh, fc);
            DrawRectangleLines(cbx, cy, cbw, cbh, Color{160, 170, 180, 160});
        };
        comp_bar(0, "ENG", info.dmg_engine);
        comp_bar(1, "PLT", info.dmg_pilot);
        comp_bar(2, "LWG", info.dmg_wing_left);
        comp_bar(3, "RWG", info.dmg_wing_right);
        comp_bar(4, "STR", info.dmg_structure);

        if (info.death_count > 0) {
            char dline[32];
            std::snprintf(dline, sizeof dline, "DEATHS %lld", info.death_count);
            DrawText(dline, bx, by + bh + 6 + 5 * 16 + 2, 14,
                     Color{200, 160, 160, 200});
        }
        // "DOWNED — RESPAWNING" flash for ~2 s after a death (clock-free, tick-
        // driven, like the kill flash). Centered banner.
        if (info.ticks_since_death < 240) {
            const char* msg = "DOWNED  —  RESPAWNING";
            const int fs = 34;
            const int tw = MeasureText(msg, fs);
            DrawText(msg, (GetScreenWidth() - tw) / 2, GetScreenHeight() / 3,
                     fs, Color{255, 70, 70, 255});
        }
    }

    // CONQUEST overlay (placeholder-grade): a small top-right status stack
    // (planes pool, the two faction scores, the 4-pump status) + a big centered
    // VICTORY/DEFEAT banner on the outcome latch. Gated on conquest_active =>
    // nothing draws otherwise. Read-only.
    if (info.conquest_active) {
        const int sw = GetScreenWidth();
        char line[96];
        std::snprintf(line, sizeof line, "PLANES: %d",
                      info.conquest_planes_left);
        DrawText(line, sw - 240, 100, 20, Color{225, 235, 250, 235});
        std::snprintf(line, sizeof line, "VALLEY %d | SUDBURY %d",
                      info.conquest_score_valley, info.conquest_score_sudbury);
        DrawText(line, sw - 240, 126, 18, Color{190, 210, 240, 220});
        std::snprintf(line, sizeof line, "PUMPS %d/%d",
                      info.conquest_pumps_alive, info.conquest_pumps_total);
        DrawText(line, sw - 240, 148, 18, Color{200, 200, 170, 220});
        // ★★★ L6 (Chad R9): "no new respawns from either side" -- said beside
        // the pump count, which is the cause of it.
        if (info.conquest_respawn_locked)
            DrawText("RESPAWNS LOCKED", sw - 240 + MeasureText(line, 18) + 12,
                     148, 18, Color{240, 150, 140, 230});
        // Near-pump health readout (fly-3): within 2 km of an alive pump the
        // HUD shows its hp fraction, so a 15 s sustained-fire budget visibly
        // bites hit by hit instead of reading as an indestructible light.
        if (info.conquest_near_pump_frac >= 0.0) {
            std::snprintf(
                line, sizeof line, "PUMP %d%%",
                static_cast<int>(info.conquest_near_pump_frac * 100.0 + 0.5));
            DrawText(line, sw - 240, 170, 20, Color{240, 200, 120, 240});
        }

        // COMPETITIVE rung: flashing "PUMP UNDER ATTACK" warning while an enemy
        // raider is on-station over the player's surface pump (blink on its own
        // clock — additive, gated off when not under attack). Read-only.
        if (info.conquest_pump_under_attack &&
            std::fmod(GetTime(), 0.7) < 0.45) {
            const char* warn = "PUMP UNDER ATTACK";
            const int fs = 24;
            const int tw = MeasureText(warn, fs);
            DrawText(warn, (sw - tw) / 2, 60, fs, Color{240, 90, 80, 255});
        }

        // SUDDEN-DEATH MATCH CLOCK (Chad, 2026-07-26): once a faction loses its
        // last pump it is on a 10-minute clock -- win outright before the
        // buzzer or lose. The SAME clock means opposite things to the two
        // sides, so the label says which: on YOUR clock it is a deadline
        // (amber, urgent red under a minute); on THEIRS it is a countdown to
        // your win (green). Disarmed (faction < 0) => nothing drawn at all.
        // ★ L6: gated on the POOL, not the target -- a held clock is
        // targetless (countdown_faction < 0) and must still be drawn.
        if (info.conquest_countdown_armed && info.conquest_outcome == 0) {
            const bool mine =
                info.conquest_countdown_faction == info.conquest_player_faction;
            const double left = info.conquest_countdown_s > 0.0
                                    ? info.conquest_countdown_s
                                    : 0.0;
            const int mins = static_cast<int>(left / 60.0);
            const int secs = static_cast<int>(left) % 60;
            char clock[64];
            // ★ L6: with no target the clock belongs to nobody yet -- naming
            // a side there would be a lie in whichever direction it picked.
            const char* label = info.conquest_countdown_faction < 0
                                    ? "CLOCK HELD"
                                    : (mine ? "KILL THEM ALL"
                                            : "ENEMY ON THE CLOCK");
            std::snprintf(clock, sizeof(clock), "%s  %d:%02d", label, mins,
                          secs);
            // Under a minute the pilot's own clock flashes -- the one moment
            // this readout has to break through everything else on screen.
            // ★ L2: a HELD clock never flashes. The urgency is about seconds
            // draining away, and while a repaired pump is holding them they are
            // not draining -- a blinking frozen number would be a lie told
            // twice a second.
            const bool urgent =
                mine && left < 60.0 && !info.conquest_countdown_paused;
            const bool blink = !urgent || std::fmod(GetTime(), 0.6) < 0.4;
            if (blink) {
                const Color cc =
                    info.conquest_countdown_paused
                        ? Color{150, 200, 240, 255}
                        : (mine ? (urgent ? Color{240, 90, 80, 255}
                                          : Color{240, 200, 120, 255})
                                : Color{120, 240, 150, 255});
                const int fs = 26;
                const int tw = MeasureText(clock, fs);
                DrawText(clock, (sw - tw) / 2, 96, fs, cc);
                // ★ L2 (Chad, 2026-09-01): "fix the surface pumps to stop the
                // game clock." BESIDE the readout, not instead of it -- the
                // seconds still matter, because losing the pump again resumes
                // from exactly them.
                if (info.conquest_countdown_paused &&
                    info.conquest_countdown_faction >= 0) {
                    const char* held = "CLOCK HELD";
                    DrawText(held, (sw - tw) / 2 + tw + 14, 96 + 4, fs - 6,
                             Color{150, 200, 240, 255});
                }
            }
        }

        // ★★★ THE DEATHMATCH (Chad's 2026-08-30 ruling). Every pump on the map
        // is dead, so the clock is null and there are no bubbles anywhere --
        // it is now last plane standing, points breaking a tie. This is drawn
        // in the clock's OWN slot on purpose: he flew a match where he killed
        // the last pump with 50 s left and the clock simply kept running, so
        // the one thing the HUD must never do here is go quiet and leave him
        // guessing whether the timer still has his name on it.
        if (info.conquest_deathmatch && info.conquest_outcome == 0) {
            // VALLEY == 0, SUDBURY == 1 (world::Faction, 1:1 with CqFaction).
            const bool valley = info.conquest_player_faction == 0;
            const int mine = valley ? info.conquest_score_valley
                                    : info.conquest_score_sudbury;
            const int theirs = valley ? info.conquest_score_sudbury
                                      : info.conquest_score_valley;
            char dm[64];
            std::snprintf(dm, sizeof(dm), "DEATHMATCH  %d - %d", mine, theirs);
            const int fs = 26;
            const int tw = MeasureText(dm, fs);
            DrawText(dm, (sw - tw) / 2, 96, fs, Color{200, 170, 255, 255});
        }

        if (info.conquest_outcome != 0) {
            const bool win = info.conquest_outcome == 1;
            const char* banner = win ? "VICTORY" : "DEFEAT";
            const int fs = 96;
            const int tw = MeasureText(banner, fs);
            const Color bc =
                win ? Color{120, 240, 150, 255} : Color{240, 90, 80, 255};
            DrawText(banner, (sw - tw) / 2, GetScreenHeight() / 2 - fs, fs, bc);
        }
    }

    // MB HUD: the attitude dial (pitch needle + roll arc + AoA strip), the
    // energy cluster, and the status stack (telemetry + flaps/gear) — all
    // read-only, through the ONE shared readout `r` computed above (never a
    // re-derived AoA/G/bank — the flat-instrument rule).
    //
    // ★★ R1c THE MODE LEAK (Chad, drive report 2026-08-17). These three are
    // AIRCRAFT instruments and nothing else: the attitude dial is pitch/roll/
    // AoA, the energy cluster is SPD/ALT/G, and the status stack is
    // GEAR DOWN + the V/ALT/THR/G/AoA/BANK telemetry line. All three were
    // drawn unconditionally, so the whole aircraft strip sat on top of the
    // snowmachine while driving it -- the sled dash's own comment already
    // calls itself "a SUBTRACTION from the aircraft HUD", and this is the
    // subtraction. `info.sled_active` is app/main.cpp's `drive_mode &&
    // sled_seeded`, the exact predicate the dash uses.
    //
    // draw_flaps_indicator STAYS CALLED unconditionally on purpose: despite
    // its name it also owns the WINTER surface plate, the sled note, the sled
    // dash itself and the ESCAPE/AIR plates. The aircraft-only parts inside it
    // (the flaps gauge, the wheel-BRAKE tag, ENGINE OUT) carry the same gate
    // internally. Splitting that function is a HUD-layout rung, not this one.
    if (!info.sled_active) {
        draw_attitude_dial(r, info, GetScreenWidth(), GetScreenHeight());
        draw_energy_cluster(r, info, GetScreenHeight());
        draw_status_stack(r, state, info, GetScreenHeight());
    }
    draw_flaps_indicator(state, params, info, GetScreenWidth(),
                         GetScreenHeight());

    // Feature A: viz-mode label — small tag so Chad sees which mode is active
    // when cycling with N. Always shown (Mirror == baseline so it reads
    // clearly).
    {
        char vtag[32];
        std::snprintf(vtag, sizeof(vtag), "VIZ: %s", viz_name(info.plane_viz));
        const int vfs = 12;
        const int vtw = MeasureText(vtag, vfs);
        const int vsw = GetScreenWidth();
        const int vsh = GetScreenHeight();
        DrawText(vtag, vsw - vtw - 14, vsh - 36, vfs,
                 Color{170, 210, 245, 160});
    }
    // Feature A/B screen-space overlays anchored to the plane: the rainbow
    // light-ring (Halo mode, drawn 3D above) and the floating player tag. Both
    // project the plane's world position to screen via the eye-relative camera.
    {
        const int sw = GetScreenWidth();
        const int sh = GetScreenHeight();
        const glm::dvec3 fwd = glm::normalize(pose.target - pose.eye);
        // Project a world point to screen; in_front guards behind-camera
        // (GetWorldToScreenEx returns garbage for points behind the lens).
        const auto project = [&](const glm::dvec3& wpos,
                                 bool& in_front) -> Vector2 {
            in_front = glm::dot(glm::normalize(wpos - pose.eye), fwd) > 0.05;
            Vector2 s = GetWorldToScreenEx(rel(wpos, pose.eye), cam, sw, sh);
            // The 3D scene renders through an off-center lens shift (reticle
            // centering); GetWorldToScreenEx uses the plain projection, so
            // correct the y to match the rendered frame.
            s.y += static_cast<float>(info.lens_shift_ndc * 0.5 * sh);
            return s;
        };
        // Plane APPARENT half-size in pixels — shrinks with view distance AND
        // with a wider FOV, grows when zoomed in. The NAME TAG scales to THIS
        // so it tracks the plane's on-screen size instead of a fixed pixel size
        // (Chad: zoom out -> the tag shrinks).
        const double plane_dist = glm::length(state.position - pose.eye);
        const double fovy_rad = info.fovy_deg * PI / 180.0;
        const float plane_px =
            static_cast<float>((5.5 / std::max(1.0, plane_dist)) * (sh * 0.5) /
                               std::tan(fovy_rad * 0.5));

        // Floating player TAG above the plane — BOLD Arial, subtly WING-ARCHED
        // (each glyph rides a shallow dome), YELLOW fill / WHITE line / BLACK
        // outer outline, and translucent (bold but see-through).
        {
            static Font s_tag_font{};
            static bool s_tag_init = false;
            if (!s_tag_init) {
                s_tag_init = true;
                // Arial BOLD for a defined, chunky tag; fall back gracefully.
                s_tag_font =
                    LoadFontEx("C:/Windows/Fonts/arialbd.ttf", 64, nullptr, 0);
                if (s_tag_font.texture.id == 0)
                    s_tag_font = LoadFontEx("C:/Windows/Fonts/arial.ttf", 64,
                                            nullptr, 0);
                if (s_tag_font.texture.id == 0) s_tag_font = GetFontDefault();
                SetTextureFilter(s_tag_font.texture, TEXTURE_FILTER_BILINEAR);
            }
            const glm::dvec3 up = glm::normalize(state.position);
            bool tag_front = false;
            const Vector2 anchor =
                project(state.position + up * 7.0, tag_front);
            if (tag_front) {
                const char* txt = "MaNdALaRK";  // Chad's callsign
                // Font size SCALES with the plane's apparent size (smaller than
                // before, and shrinks as you zoom/fly out), floored so it stays
                // legible up close and never fully vanishes.
                const float fs =
                    std::max(9.0f, std::min(26.0f, plane_px * 0.55f));
                const float trk = 1.0f;
                const Vector2 full = MeasureTextEx(s_tag_font, txt, fs, trk);
                const float amp = fs * 0.25f;  // wing arch scales with size
                const float ob =
                    std::max(1.0f, fs * 0.09f);  // outline thickness
                const float x0 = anchor.x - full.x * 0.5f;
                const float y0 = anchor.y - full.y;
                // One glyph, layered translucent outline: black -> white ->
                // gold. Offsets scale with the font so the outline stays
                // proportional.
                const auto put_char = [&](const char* ch, float x, float y) {
                    const auto d = [&](float ox, float oy, Color c) {
                        DrawTextEx(s_tag_font, ch, Vector2{x + ox, y + oy}, fs,
                                   trk, c);
                    };
                    for (int k = 0; k < 8; ++k) {  // black outer ring (8 dirs)
                        const float ang = k * (PI / 4.0f);
                        d(std::cos(ang) * ob, std::sin(ang) * ob,
                          Color{0, 0, 0, 115});
                    }
                    const float wb = ob * 0.55f;  // white line (4 dirs)
                    d(-wb, 0, Color{255, 255, 255, 125});
                    d(wb, 0, Color{255, 255, 255, 125});
                    d(0, -wb, Color{255, 255, 255, 125});
                    d(0, wb, Color{255, 255, 255, 125});
                    // S-mapteam: Chad flies for the ALLIED side, so his own
                    // callsign wears the ally BLUE like every other friendly
                    // tag (was gold). He stays findable by SIZE/POSITION (it
                    // is the only arched, plane-scaled tag on screen), not by
                    // a hue nobody else has — the same ruling as his map
                    // marker, which is ally-coloured and findable by SHAPE.
                    // ABSOLUTE BY FACTION (2026-09-10): his own side's hue,
                    // which is the ally blue for the shipped Valley player and
                    // the slag orange when he flies for Central City -- the
                    // same colour his map arrow and his aeroplane wear.
                    const glm::dvec3 ac =
                        render::faction_color(info.conquest_player_faction);
                    const auto q8 = [](double v) {
                        return static_cast<unsigned char>(std::lround(
                            std::min(1.0, std::max(0.0, v)) * 255.0));
                    };
                    d(0, 0, Color{q8(ac.x), q8(ac.y), q8(ac.z), 170});
                };
                float x = x0;
                for (const char* p = txt; *p; ++p) {
                    const char cbuf[2] = {*p, '\0'};
                    const Vector2 cm = MeasureTextEx(s_tag_font, cbuf, fs, trk);
                    const float t =
                        full.x > 0 ? (x + cm.x * 0.5f - x0) / full.x : 0.5f;
                    const float s = 2.0f * t - 1.0f;
                    const float yoff = -amp * (1.0f - s * s);  // dome (wing)
                    put_char(cbuf, x, y0 + yoff);
                    x += cm.x + trk;
                }
            }
        }
    }
    // M-KEY FULL-SCREEN BUBBLE MAP (Chad's ask, 2026-07-25): a top-down aeqd
    // overlay of the two faction ellipses + the vacuum gap between them, the
    // player, the 4 pumps, the 2 tunnel mouths + route hint, and the
    // mavericks. Pure DISPLAY (render/bubble_map.h's math is raylib-free and
    // reads only baked world constants + the live FrameInfo fields the app
    // already forwards) — nothing here feeds back into mouse->aim/control.
    // info.map_open false (the default) draws NOTHING new: bit-identical to
    // every pre-map frame. Drawn LAST (over the 3D scene + every other HUD
    // element, before the bezel) — a true overlay, not a mode swap; the 3D
    // world keeps rendering underneath and flight input stays live.
    //
    // ★ L4: THE BODY MOVED, VERBATIM, to render/map_screen.cpp. It was 450
    // lines of a second complete renderer sitting inside this function; the
    // call below is where it used to sit, so the frame order is unchanged.
    if (info.map_open) draw_map_screen(state, params, info);
    draw_bezel(GetScreenWidth(), GetScreenHeight(), info.season,
               info.escape_sky, info.bubble_live);
    pmark("fx_hud_post");
    // ★ L3: the app's own last overlay (the spawn menu). See FrameInfo::overlay
    // -- null in every frame but the ones the menu is open.
    if (info.overlay != nullptr) info.overlay(info.overlay_ctx);
    EndDrawing();
    pmark("swap");
}

}  // namespace render
