#include "render/draw.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "raylib.h"
#include "raymath.h"  // MatrixFrustum (off-center lens-shift projection)
#include "render/orient_cues.h"  // S-cues (comfort): ghost horizon + bank arc
#include "render/planet.h"
#include "render/readout.h"
#include "render/rig.h"  // Fleet Rig (rig-D port): Bf 109 F-4 node table + law
#include "rlgl.h"
#include "sim/world.h"

namespace render {

namespace {

// The seam: re-base to the camera eye in double, THEN cast (see draw.h).
Vector3 rel(const glm::dvec3& world, const glm::dvec3& eye) {
    const glm::dvec3 r = world - eye;
    return Vector3{static_cast<float>(r.x), static_cast<float>(r.y),
                   static_cast<float>(r.z)};
}

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
// crash surface is still the perfect sphere at R (SPEC §6).
//   relief_scale = metres at full-white DEM (Everest-class peaks read on a
//   15 km toy planet); subdiv = cubesphere verts/edge/face.
void draw_planet(const sim::AircraftParams& params, const glm::dvec3& eye) {
    // Geometry knobs come from config/world.toml (g_planet_cfg, set once at
    // startup); u_offset still puts the steep equatorial relief under the +X
    // spawn sub-point for a legible opening view (until Sudbury replaces it,
    // P1).
    static Planet planet =
        load_planet(SEADS_ASSET_DIR, params.R, g_planet_cfg.relief_scale,
                    g_planet_cfg.subdiv, g_planet_cfg.u_offset,
                    g_planet_cfg.dem_blur_radius);
    // Fixed world-space sun (directional: no eye rebasing). RAKING the spawn
    // sub-point (+X) at a low angle so terrain casts shadow and relief reads;
    // sun is the light-TRAVEL direction, a point is lit when its normal faces
    // -sun (-sun ~ (0.45,0.75,0.48): ~27° elevation over the +X sub-point).
    static const glm::vec3 sun =
        glm::normalize(glm::vec3{-0.45f, -0.75f, -0.48f});

    if (planet.ok) {
        draw_planet_mesh(planet, eye, sun);
        return;
    }
    // Fallback: assets or GPU features unavailable — keep the sim flyable with
    // the original untextured sphere (no floating wire shell).
    const Vector3 center = rel(glm::dvec3{0.0}, eye);
    DrawSphereEx(center, static_cast<float>(params.R), 64, 96,
                 Color{72, 108, 58, 255});
}

// --- Fleet Rig (ported from seads-tunnel render/rig.{h,cpp} + the rig-D
// D.3 ingestion in that tree's draw.cpp, 2026-07-23, PLANE-MODEL-ONLY) -----
// The node table, rest transforms, and cosmetic control-surface deflection
// law are PURE (render/rig.h/.cpp, glm + sim/state.h only — no tunnel-only
// systems to trim there). This block is the raylib ingestion: load the 30
// Bf 109 F-4 GLBs once, then per-frame pose + draw them through the shared
// mirror/glass/matte material passes.
//
// TRIM vs the source tree: seads-tunnel's planet build produces a real
// world-albedo CUBEMAP that the mirror shader samples for env reflections
// (render/planet.h there: Planet::cubemap). seads-feel's planet (this
// tree's render/planet.h) is a flat equirectangular-textured cubesphere —
// no cubemap exists to reuse, and building one is a planet-build change,
// out of scope for a render-only plane port. ensure_fleet() instead builds
// a tiny flat NEUTRAL-GRAY stub cubemap (1x1 px x6 faces) once, so the
// UNMODIFIED mirror shader (rig.cpp) still compiles and samples cleanly;
// the mirror reads as a flat lit sheen (sun lambert + Fresnel rim in the
// plane's own chroma) rather than the real planet reflected in the
// fuselage. Cosmetic trim only — the geometry/rig/deflection law is
// byte-identical to the source tree.
struct FleetDrawParams {
    glm::vec3 sun_dir{0.0f};       // world light-travel dir (sun -> scene)
    float reflectivity = 0.45f;    // env-reflection strength [0,1]
    float fresnel_power = 3.0f;    // Fresnel rim exponent (>0)
    glm::vec3 player_color{1.0f};  // hero plane chroma
    glm::vec3 bandit_color{1.0f};  // bandit plane chroma
    DeflectGains gains{};
    float prop_disc_alpha = 0.30f;
    float prop_idle_alpha = 0.06f;
};

// The FIXED world-space sun used for the mirror shading — the SAME literal
// draw_planet() uses (kept as a separate constant, not shared, to keep this
// port a minimal diff off draw_planet's existing static local).
constexpr glm::vec3 kFleetSunRaw{-0.45f, -0.75f, -0.48f};
// rig-B deflection magnitudes are config DEGREES -> radians at this render
// boundary (angles are radians internally elsewhere in the codebase).
constexpr float kFleetD2R = static_cast<float>(3.14159265358979323846 / 180.0);

struct FleetRig {
    bool ok = false;
    bool tried = false;
    Mesh meshes[kNodeCount] = {};
    // The loaded GLB Models are kept alive for the program lifetime —
    // meshes[i] aliases models[i].meshes[0], so UnloadModel would free the
    // GPU buffers we draw from.
    Model models[kNodeCount] = {};
    bool model_loaded[kNodeCount] = {};
    Shader shader = {};
    Material mat = {};        // Mirror: pop-art monochrome-saturation finish
    Material glass_mat = {};  // Glass: translucent canopy
    Material matte_mat = {};  // Matte: non-mirror diffuse (pilot, tyres)
    Material prop_mat = {};   // translucent prop blur-disc material
    Texture env_cubemap = {};  // the flat neutral-gray stub (see block note)
    int loc_color = -1, loc_sun = -1, loc_fresnel = -1, loc_refl = -1;
    Rig rig;
};
FleetRig g_fleet;

// The material a node draws with, routed by its authored MaterialClass.
Material& fleet_material(MaterialClass cls) {
    switch (cls) {
        case MaterialClass::Glass:
            return g_fleet.glass_mat;
        case MaterialClass::Matte:
            return g_fleet.matte_mat;
        case MaterialClass::Mirror:
        default:
            return g_fleet.mat;
    }
}

void ensure_fleet() {
    if (g_fleet.tried) return;
    g_fleet.tried = true;
    g_fleet.rig = build_aircraft_rig();
    const auto& specs = aircraft_node_specs();
    // Load the real Bf 109 F-4 mesh per node from assets/bf109/. Each GLB
    // was exported with the SEADS frame preserved (nose on -Z, identity node
    // transform), so model.meshes[0] is already in the node's local frame.
    // A missing/empty GLB falls back to the placeholder GenMeshCube for
    // THAT node only, so a single bad asset can't blank the aircraft.
    for (int i = 0; i < kNodeCount; ++i) {
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
            const glm::vec3 d = specs[i].box_dims;  // shape in mesh (scl==1)
            g_fleet.meshes[i] = GenMeshCube(d.x, d.y, d.z);
            TraceLog(LOG_WARNING,
                     "FLEET: node %d ('%s') GLB missing; cube fallback", i,
                     key);
        }
    }
    g_fleet.shader = LoadShaderFromMemory(mirror_vs_source(), mirror_fs_source());
    g_fleet.loc_color = GetShaderLocation(g_fleet.shader, "u_planeColor");
    g_fleet.loc_sun = GetShaderLocation(g_fleet.shader, "u_sunDir");
    g_fleet.loc_fresnel = GetShaderLocation(g_fleet.shader, "u_fresnelPower");
    g_fleet.loc_refl = GetShaderLocation(g_fleet.shader, "u_reflectivity");
    // A compile failure leaves raylib's default program (our uniforms
    // absent): loc_color == -1 => fall back to DrawCube.
    if (g_fleet.shader.id == 0 || g_fleet.loc_color < 0) {
        TraceLog(LOG_WARNING,
                 "FLEET: mirror shader unavailable; DrawCube fallback");
        return;
    }
    g_fleet.shader.locs[SHADER_LOC_MAP_CUBEMAP] =
        GetShaderLocation(g_fleet.shader, "env");
    g_fleet.mat = LoadMaterialDefault();
    g_fleet.mat.shader = g_fleet.shader;
    // The neutral-gray stub env cubemap (see the block note above): 1x1 px,
    // RGBA8, six faces of the identical mid-gray texel, built once.
    {
        const unsigned char px[4] = {130, 130, 130, 255};
        unsigned char data[6 * 4];
        for (int f = 0; f < 6; ++f)
            std::memcpy(data + f * 4, px, 4);
        const unsigned int id = rlLoadTextureCubemap(
            data, 1, RL_PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, 1);
        g_fleet.env_cubemap =
            Texture{id, 1, 1, 1, RL_PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    }
    // Matte (default shader): the pilot bust + rubber tyres read as form,
    // not chrome — a mid-dark neutral so they sit INSIDE the mirror airframe.
    g_fleet.matte_mat = LoadMaterialDefault();
    g_fleet.matte_mat.maps[MATERIAL_MAP_DIFFUSE].color = Color{38, 40, 44, 255};
    // Glass (default shader, translucent): the canopy — a cool tint at low
    // alpha, drawn in the back-to-front translucent pass so the pilot shows
    // through.
    g_fleet.glass_mat = LoadMaterialDefault();
    g_fleet.glass_mat.maps[MATERIAL_MAP_DIFFUSE].color =
        Color{150, 175, 195, 90};
    // Prop disc: default-shader material, neutral gray; per-draw alpha is
    // set on its diffuse color (throttle-scaled). Symmetric disc, no spin.
    g_fleet.prop_mat = LoadMaterialDefault();
    g_fleet.prop_mat.maps[MATERIAL_MAP_DIFFUSE].color = Color{40, 44, 50, 255};
    g_fleet.ok = true;
    TraceLog(LOG_INFO, "FLEET: Bf 109 rig built (%d nodes, real meshes)",
             kNodeCount);
}

// glm::mat4 -> raylib Matrix. to_ray_fields returns raylib FIELD-DECLARATION
// order; designated initializers pin field<-value so a raylib struct
// reorder can't silently transpose the matrix.
Matrix to_ray(const glm::mat4& g) {
    const std::array<float, 16> f = to_ray_fields(g);
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

// The landing-gear group: struts + wheels + doors + tailwheel. Hidden
// together when the gear is retracted (state.gear <= eps). Doors parent to
// the fuselage (bay-edge hinge), so membership is by index, not subtree.
bool is_gear_node(int i) {
    switch (i) {
        case kLeftGearDoor:
        case kLeftGearStrut:
        case kLeftWheel:
        case kRightGearDoor:
        case kRightGearStrut:
        case kRightWheel:
        case kTailWheel:
            return true;
        default:
            return false;
    }
}

// Body-frame primitives (SPEC §7: +X right, +Y up, -Z forward), drawn under
// the aircraft's model matrix. `enemy` swaps to the saturated bandit livery
// (S8-drone). `deflect` are the COMMANDED control Inputs that pose the
// ailerons/elevator/rudder (render-only, RA9); the gear unfolds from
// state.gear. The PROP is NOT drawn here — see draw_prop, a second
// translucent pass after ALL opaque bodies. Falls back to the flat DrawCube
// aircraft if the mirror shader/rig is unavailable, so the sim stays flyable.
void draw_aircraft(const sim::SimState& state, const glm::dvec3& eye,
                   const FleetDrawParams& fp, const sim::Inputs& deflect = {},
                   bool enemy = false, double scale = 1.0,
                   float wheel_roll_rad = 0.0f) {
    ensure_fleet();
    const Vector3 p = rel(state.position, eye);

    if (!g_fleet.ok) {
        // Fallback: the flat DrawCube aircraft — keeps the sim flyable with
        // no floating geometry if the mirror shader/GLBs are unavailable.
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
    // precision-safe (the big ~km translation is done in rel(), in double,
    // before the cast).
    const glm::mat4 body_to_world =
        glm::translate(glm::mat4(1.0f), glm::vec3(p.x, p.y, p.z)) *
        glm::mat4_cast(glm::quat(state.orientation)) *
        glm::scale(glm::mat4(1.0f), glm::vec3(static_cast<float>(scale)));

    // Per-plane uniforms. env is the flat stub cubemap built in ensure_fleet
    // (see the block note) — read-only, sampled once per plane.
    g_fleet.mat.maps[MATERIAL_MAP_CUBEMAP].texture = g_fleet.env_cubemap;
    const glm::vec3 col = enemy ? fp.bandit_color : fp.player_color;
    const float c3[3] = {col.x, col.y, col.z};
    const float s3[3] = {fp.sun_dir.x, fp.sun_dir.y, fp.sun_dir.z};
    SetShaderValue(g_fleet.shader, g_fleet.loc_color, c3, SHADER_UNIFORM_VEC3);
    SetShaderValue(g_fleet.shader, g_fleet.loc_sun, s3, SHADER_UNIFORM_VEC3);
    SetShaderValue(g_fleet.shader, g_fleet.loc_fresnel, &fp.fresnel_power,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(g_fleet.shader, g_fleet.loc_refl, &fp.reflectivity,
                   SHADER_UNIFORM_FLOAT);

    // Pose the driven surfaces from the commanded Inputs + the actual gear
    // extension, then recompute the node world matrices. The shared rig is
    // fully re-posed per call (player, then each drone), so single-threaded
    // reuse is safe — no leakage between planes.
    apply_deflection(g_fleet.rig, deflect, static_cast<float>(state.gear),
                     fp.gains, wheel_roll_rad);

    const auto& specs = aircraft_node_specs();
    for (int i = 0; i < kNodeCount; ++i) {
        // The prop BLADES draw as a translucent disc in draw_prop's 2nd pass;
        // the solid spinner still draws here.
        if (i == kPropBlades) continue;
        // The GLASS canopy is translucent — deferred to the back-to-front
        // pass (draw_prop) so the mirror body behind it composites correctly.
        if (specs[i].material == MaterialClass::Glass) continue;
        // The whole gear group is drawn only while extended (state.gear
        // slews 0->1); at 0 it is tucked in-bay and hidden.
        if (is_gear_node(i) && state.gear <= 1e-3) continue;
        const glm::mat4 m = body_to_world * g_fleet.rig[i].world;
        DrawMesh(g_fleet.meshes[i], fleet_material(specs[i].material),
                 to_ray(m));
    }
}

// The per-plane TRANSLUCENT surfaces — the propeller blur disc AND the glass
// canopy — drawn in a SEPARATE pass AFTER all opaque bodies (a depth-write-
// off surface would otherwise be overwritten by a farther plane's later
// opaque body). Alpha blending on, depth-WRITE off (caller wraps the whole
// pass in BeginBlendMode/rlDisableDepthMask). The prop is a throttle-scaled
// opacity (time-free: opacity, not a spinning phase).
void draw_prop(const sim::SimState& state, const glm::dvec3& eye,
               const FleetDrawParams& fp, double scale = 1.0) {
    if (!g_fleet.ok) return;  // fallback aircraft has no prop
    const Vector3 p = rel(state.position, eye);
    const glm::mat4 body_to_world =
        glm::translate(glm::mat4(1.0f), glm::vec3(p.x, p.y, p.z)) *
        glm::mat4_cast(glm::quat(state.orientation)) *
        glm::scale(glm::mat4(1.0f), glm::vec3(static_cast<float>(scale)));
    // Glass canopy first (it sits behind the prop disc from most angles; the
    // depth-write-off pass makes intra-plane order cosmetic, front-lit
    // anyway).
    const glm::mat4 gm = body_to_world * g_fleet.rig[kCanopy].world;
    DrawMesh(g_fleet.meshes[kCanopy], fleet_material(MaterialClass::Glass),
             to_ray(gm));
    // Opacity fills in with throttle (idle floor .. full-throttle disc).
    const float t = static_cast<float>(glm::clamp(state.throttle, 0.0, 1.0));
    const float a =
        fp.prop_idle_alpha + (fp.prop_disc_alpha - fp.prop_idle_alpha) * t;
    Color& c = g_fleet.prop_mat.maps[MATERIAL_MAP_DIFFUSE].color;
    c.a = static_cast<unsigned char>(glm::clamp(a, 0.0f, 1.0f) * 255.0f);
    const glm::mat4 m = body_to_world * g_fleet.rig[kPropBlades].world;
    DrawMesh(g_fleet.meshes[kPropBlades], g_fleet.prop_mat, to_ray(m));
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
    DrawEllipse(static_cast<int>(c.x), static_cast<int>(c.y),
                rx + outline_px, ry + outline_px, BLACK);
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
    const double splash_t =
        (now_s - g_splash.splash_start_s) / kSplashDuration;
    const bool splashing = splash_t >= 0.0 && splash_t < 1.0;

    // TREMBLE (display-only wobble; deterministic in now_s, per-axis phase
    // offset so it reads as a shake, not a diagonal slide) — only in the
    // fully-scared band.
    Vector2 a = anchor;
    if (fear01 > 0.7f) {
        a.x += static_cast<float>(std::sin(now_s * 40.0)) * fear01 * 2.0f;
        a.y += static_cast<float>(std::cos(now_s * 40.0 + 1.3)) * fear01 *
              2.0f;
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
            const double ring_t =
                std::clamp(splash_t - i * 0.12, 0.0, 1.0);
            if (ring_t <= 0.0) continue;
            const float radius =
                (14.0f + static_cast<float>(ring_t) * 46.0f) * scale;
            const unsigned char al = static_cast<unsigned char>(
                (1.0 - ring_t) * 180.0);
            DrawCircleLinesV(a, radius, Color{60, 255, 120, al});
        }
    }

    constexpr Color kBody{57, 255, 60, 255};    // bright neon-green body
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
        DrawCircleV(Vector2{eye_l.x - 0.8f * s, eye_l.y - 0.8f * s},
                   0.7f * s, kWhite);  // glints
        DrawCircleV(Vector2{eye_r.x - 0.8f * s, eye_r.y - 0.8f * s},
                   0.7f * s, kWhite);
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
            Rectangle band{head_c.x, head_c.y + 2.0f * s, 13.0f * s,
                          3.2f * s};
            Vector2 origin{6.5f * s, 1.6f * s};
            DrawRectanglePro(band, origin, 12.0f, BLACK);
        } else {
            // SCARED: glasses FLUNG — a tiny pair offset up and away,
            // tilted, as if knocked off.
            Rectangle band{head_c.x - 10.0f * s, head_c.y - 16.0f * s,
                          9.0f * s, 2.4f * s};
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
                Vector2{dp.x - 1.6f * s, dp.y},
                Vector2{dp.x + 1.6f * s, dp.y},
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
    const float dlen = std::sqrt(dir_unit.x * dir_unit.x +
                                 dir_unit.y * dir_unit.y);
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
void draw_ghost_sweat(Vector2 head_c, float head_r, float fear01,
                      double now_s, float s) {
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
            const float px = a.x + (i % 2 == 0 ? -1.0f : 1.0f) *
                              (3.0f + 10.0f * along) * s;
            const float py = hem_y + along * 22.0f * s * tail_amt;
            const unsigned char al = static_cast<unsigned char>(
                (1.0f - along) * 140.0f * tail_amt);
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
void draw_aim_ghost(Vector2 anchor, Vector2 dir_unit, float fear01,
                    double now_s, float scale = 1.0f) {
    fear01 = std::clamp(fear01, 0.0f, 1.0f);

    // Splash (panic) trigger/re-arm — identical mechanism to draw_aim_mouse,
    // shared g_splash (only one skin is ever live at a time, kAimBuddySkin).
    if (fear01 >= kSplashFear && g_splash.armed) {
        g_splash.splash_start_s = now_s;
        g_splash.armed = false;
    }
    if (fear01 < kSplashRearm) g_splash.armed = true;
    g_splash.last_fear = fear01;
    const double splash_t =
        (now_s - g_splash.splash_start_s) / kSplashDuration;
    const bool splashing = splash_t >= 0.0 && splash_t < 1.0;

    // RELIEF trigger/re-arm (Chad: "also relief at safety") — a separate
    // hysteretic latch keyed off a HIGH-WATER read of fear01 so it only
    // fires after a real scare, not a brief blip through the low band.
    g_splash.fear_high_water = std::max(g_splash.fear_high_water, fear01);
    if (fear01 < kReliefFear && g_splash.fear_high_water > kReliefHighWater &&
        g_splash.relief_armed) {
        g_splash.relief_start_s = now_s;
        g_splash.relief_armed = false;
        g_splash.fear_high_water = 0.0f;
    }
    if (fear01 > kReliefRearm) g_splash.relief_armed = true;
    const double relief_t =
        (now_s - g_splash.relief_start_s) / kReliefDuration;
    const bool relieving = !splashing && relief_t >= 0.0 && relief_t < 1.0;

    // TREMBLE — same shake as the mouse, only in the fully-scared band, and
    // only OUTSIDE panic (the panic form has its own, stronger tremble).
    Vector2 a = anchor;
    if (!splashing && fear01 > 0.7f) {
        a.x += static_cast<float>(std::sin(now_s * 40.0)) * fear01 * 2.0f;
        a.y += static_cast<float>(std::cos(now_s * 40.0 + 1.3)) * fear01 *
              2.0f;
    }

    float s = scale;
    if (relieving) {
        // A slow exhale: squash to ~0.92x then recover over the beat.
        const float rt = static_cast<float>(std::clamp(relief_t, 0.0, 1.0));
        const float squash =
            1.0f - 0.08f * std::sin(PI * std::min(rt / 0.4f, 1.0f));
        s *= squash;
    }

    if (splashing) {
        draw_ghost_panic(a, static_cast<float>(std::clamp(splash_t, 0.0, 1.0)),
                         now_s, s);
    } else {
        // Body alpha PULSES at ~6 Hz in the fully-scared band ("flickering
        // with fright"); a steady MORE TRANSLUCENT 85 otherwise (was 140 —
        // Chad: "I need it more translucent").
        unsigned char body_alpha = 85;
        if (fear01 > 0.7f) {
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
        if (fear01 >= 0.3f && fear01 < 0.7f) {
            wave_speed = 6.0f;
            wave_amp = 1.5f;
        } else if (fear01 >= 0.7f) {
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
        // silhouette).
        const Vector2 arm_l{a.x - body_half_w - 1.0f * s, head_c.y + 6.0f * s};
        const Vector2 arm_r{a.x + body_half_w + 1.0f * s, head_c.y + 6.0f * s};
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
            DrawCircleV(Vector2{eye_l.x, eye_l.y + look_down},
                       eye_r_px * 0.5f, BLACK);
            DrawCircleV(Vector2{eye_r.x, eye_r.y + look_down},
                       eye_r_px * 0.5f, BLACK);

            DrawLineEx(Vector2{eye_l.x - 3.2f * s, eye_l.y - eye_r_px - 1.4f * s},
                      Vector2{eye_l.x + 1.4f * s, eye_l.y - eye_r_px - 3.0f * s},
                      1.3f * s, BLACK);
            DrawLineEx(Vector2{eye_r.x + 3.2f * s, eye_r.y - eye_r_px - 1.4f * s},
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
            DrawCircleV(Vector2{head_c.x, head_c.y + 5.5f * s}, mouth_r,
                       BLACK);
            draw_ghost_sweat(head_c, head_r, fear01, now_s, s);
        }
    }

    // Pointer tick — identical in role to the mouse's (direction legible
    // without rotating the art). Uses the ORIGINAL anchor `a` and `scale`
    // (not the panic/relief-perturbed `s`) so it stays a stable, legible
    // direction cue through every state.
    const Color kTickRim{57, 255, 60, 200};
    const float dlen = std::sqrt(dir_unit.x * dir_unit.x +
                                 dir_unit.y * dir_unit.y);
    if (dlen > 1e-6f) {
        const Vector2 ud{dir_unit.x / dlen, dir_unit.y / dlen};
        const Vector2 p0{a.x + ud.x * 18.0f * scale, a.y + ud.y * 18.0f * scale};
        const Vector2 p1{a.x + ud.x * 27.0f * scale, a.y + ud.y * 27.0f * scale};
        DrawLineEx(p0, p1, 2.2f * scale, kTickRim);
    }
}

// The skin-dispatching entry point every call site uses. `kAimBuddySkin`
// picks the shipped skin; every branch stays compilable so a future
// economy-item picker can switch on a player selection instead.
void draw_aim_buddy(Vector2 anchor, Vector2 dir_unit, float fear01,
                    double now_s, float scale = 1.0f) {
    switch (kAimBuddySkin) {
        case BuddySkin::kMouse:
            draw_aim_mouse(anchor, dir_unit, fear01, now_s, scale);
            return;
        case BuddySkin::kGhost:
            draw_aim_ghost(anchor, dir_unit, fear01, now_s, scale);
            return;
    }
}

}  // namespace aim_buddy

}  // namespace

void set_planet_build_params(const PlanetBuildParams& p) { g_planet_cfg = p; }

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
void draw_bezel(int sw, int sh) {
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

    // Title along the bottom edge (clear of the top-left flight HUD).
    DrawText("SEADS  ·  SPHERICAL EARTH  ·  R=15 km", m + 10, sh - m - 18, 14,
             Color{170, 210, 245, 190});
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
                       const sim::AircraftParams& params, const FrameInfo& info,
                       int sh) {
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

    // FLAPS + GEAR lines (always shown — the "better indication" ask).
    const double flap_target = info.flap_mode == 0 ? 0.0
                               : info.flap_mode == 1
                                   ? static_cast<double>(params.flap_combat)
                                   : 1.0;
    std::snprintf(line, sizeof line, "FLAPS %s",
                  flap_mode_label(info.flap_mode));
    DrawText(line, x, sh - 112, 18, Color{225, 235, 250, 235});
    bar(sh - 112, state.flap, flap_target);
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

void draw_frame(const sim::SimState& state, const sim::AircraftParams& params,
                const CameraPose& pose, const FrameInfo& info) {
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
    ClearBackground(Color{18, 24, 44, 255});

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
    draw_planet(params, pose.eye);
    // Fleet Rig (rig-D port): the real Bf 109 F-4 mesh, posed by the
    // commanded Inputs (info.player_inputs / info.drone_inputs[i] — a
    // missing/short drone_inputs draws that drone at rest, the documented
    // rig-B fallback). rig_* fields are unset in this tree (no
    // config/world.toml [fleet_rig] loader here — cosmetic, not a physics
    // dial), so FleetDrawParams below carries FrameInfo's built-in plausible
    // defaults, matching the tunnel tree's own "unset => a plausible mirror"
    // contract.
    const glm::vec3 fleet_sun = glm::normalize(kFleetSunRaw);
    const DeflectGains gains{
        static_cast<float>(info.rig_aileron_deg) * kFleetD2R,
        static_cast<float>(info.rig_elevator_deg) * kFleetD2R,
        static_cast<float>(info.rig_rudder_deg) * kFleetD2R,
        static_cast<float>(info.rig_gear_deploy_deg) * kFleetD2R};
    const FleetDrawParams fp{fleet_sun,
                             static_cast<float>(info.rig_reflectivity),
                             static_cast<float>(info.rig_fresnel_power),
                             info.rig_player_color,
                             info.rig_bandit_color,
                             gains,
                             static_cast<float>(info.rig_prop_disc_alpha),
                             static_cast<float>(info.rig_prop_idle_alpha)};
    // Opaque pass: player + every drone, each posed by its OWN commanded
    // Inputs. Gear reads each plane's state.gear.
    draw_aircraft(state, pose.eye, fp, info.player_inputs, /*enemy=*/false,
                 /*scale=*/1.0, static_cast<float>(info.player_wheel_roll_rad));
    for (std::size_t i = 0; i < info.drones_draw.size(); ++i) {
        const sim::Inputs di =
            i < info.drone_inputs.size() ? info.drone_inputs[i] : sim::Inputs{};
        draw_aircraft(info.drones_draw[i], pose.eye, fp, di, /*enemy=*/true,
                     info.drone_scale);
    }
    // MB-7c (iii): wingtip vortex trails — fading world-space streamers off
    // the tips near stall AoA / high G. Read-only cosmetic overlay; points
    // are re-based to the eye at draw time (the double->float seam).
    if (info.vortices != nullptr) {
        const auto draw_trail = [&](const std::vector<VortexPoint>& tr) {
            for (std::size_t i = 1; i < tr.size(); ++i) {
                const VortexPoint& a = tr[i - 1];
                const VortexPoint& b = tr[i];
                // Break across respawn/teleport gaps.
                if (glm::length(b.pos - a.pos) > 30.0) continue;
                const double fade = b.strength * (1.0 - b.age / kVortexLife);
                if (fade <= 0.0) continue;
                const unsigned char al =
                    static_cast<unsigned char>(200.0 * fade);
                DrawLine3D(rel(a.pos, pose.eye), rel(b.pos, pose.eye),
                           Color{225, 240, 255, al});
            }
        };
        draw_trail(info.vortices->left);
        draw_trail(info.vortices->right);
    }
    // Fleet Rig translucent pass: ALL blur discs + glass canopies AFTER ALL
    // opaque bodies, alpha-blended with depth-WRITE off so a farther body
    // can't overwrite a nearer disc/canopy and overlapping ones composite.
    BeginBlendMode(BLEND_ALPHA);
    rlDisableDepthMask();
    draw_prop(state, pose.eye, fp);
    for (const sim::SimState& d : info.drones_draw)
        draw_prop(d, pose.eye, fp, info.drone_scale);
    rlEnableDepthMask();
    EndBlendMode();
    EndMode3D();

    // Correct-frame HUD (SPEC §12): the readouts come through the ONE shared
    // render::flight_readout — velocity-relative AoA, local_up-aware bank/G.
    // The telemetry line itself lives in the bottom-left status stack now
    // (Chad: the top strip was unreadable); only the help line stays up top.
    const FlightReadout r = flight_readout(state, params);
    DrawText(info.raw_mode
                 ? "RAW: S/W pitch  A/D roll  Q/E yaw  Shift/Ctrl throttle  "
                   "RMB stick   [F1] instructor"
                 : "MOUSE aim   S/W A/D Q/E override   SPACE freelook   "
                   "Shift/Ctrl throttle   [F1] raw",
             12, 12, 16, Color{200, 200, 200, 180});
    // BUILD STAMP (Chad 2026-07-23, the "am I flying the right kernel?"
    // ambiguity — never again): the TREE identity, drawn always. Every
    // worktree's build says which kernel it is; a fly report without this
    // stamp visible is a report about an unknown kernel.
    DrawText("KERNEL v5 [seads-feel]", GetScreenWidth() - 232,
             GetScreenHeight() - 26, 16, Color{255, 190, 60, 200});

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
        if (nos.in_front) {
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
        if (ret.in_front) {
            const Vector2 rc = to_pxf(ret);
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
        const float fear01 = static_cast<float>(aim_buddy::smoothstep01(
            aim_buddy::kMouseFearStart, aim_buddy::kMouseFearCommit,
            fear_elev));
        const double now_s = GetTime();

        // v5 ON-SCREEN scared mouse (Chad's fear read is valuable on-screen
        // too, not just as the off-screen direction marker): when the aim is
        // ON screen AND fear01 > 0.3, dock a SMALL (~22 px) scared mouse just
        // outside the reticle ring, bottom-right of it. Below 0.3 on-screen,
        // no mouse at all — the "cool mouse" ONLY ever appears as the
        // off-screen direction marker (policy: Chad framed the mouse as the
        // off-screen marker; the docked variant is purely the fear cue).
        if (ret.in_front && fear01 > 0.3f) {
            const Vector2 rc = to_pxf(ret);
            const Vector2 dock{rc.x + 26.0f, rc.y + 20.0f};
            const Vector2 to_ret{rc.x - dock.x, rc.y - dock.y};  // tiny tick
                                                                 // back at
                                                                 // the ring
            aim_buddy::draw_aim_buddy(dock, to_ret, fear01, now_s,
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
            const bool off_screen =
                !ret.in_front || std::abs(ret.x) > 1.0 ||
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
                    aim_buddy::draw_aim_buddy(anchor, dir_unit, fear01,
                                                now_s);
                }
            }
        }

        // Lead-angle pipper (S8-drone): the meter's snapshot (computed once per
        // sim tick in app::tick) says WHERE to aim for a hit on the engaged
        // bandit; here we only PROJECT that world direction and draw it. Green
        // when the nose is on the solution and in range, amber otherwise.
        if (info.gunsight_active && info.gunsight_has_target) {
            const ScreenPoint lp = project_dir(info.gunsight_lead, cam_forward,
                                               pose.up, fovy_rad, aspect);
            if (lp.in_front) {
                const Vector2 lc = to_pxf(lp);  // float path (was int-cast;
                                                // it drew float lines anyway)
                const Color col = info.gunsight_on_target
                                      ? Color{80, 240, 120, 255}
                                      : Color{240, 180, 60, 220};
                const float fx = lc.x;
                const float fy = lc.y;
                constexpr float d = 8.0f;  // diamond half-extent
                DrawLineEx({fx - d, fy}, {fx, fy - d}, 2.0f, col);
                DrawLineEx({fx, fy - d}, {fx + d, fy}, 2.0f, col);
                DrawLineEx({fx + d, fy}, {fx, fy + d}, 2.0f, col);
                DrawLineEx({fx, fy + d}, {fx - d, fy}, 2.0f, col);
            }
        }
    }

    // Time-on-target readout (S8-drone Stage 3): the tracking number. Only when
    // the gunsight is active; shows the lifetime on-target %, the engaged
    // range, and a live ON TARGET cue when the nose is on the solution.
    if (info.gunsight_active && info.gunsight_has_target) {
        char tline[96];
        std::snprintf(tline, sizeof tline, "TOT %3.0f%%   RNG %5.0f m   %s",
                      info.tot_frac * 100.0, info.tot_range,
                      info.gunsight_on_target ? "* ON TARGET *" : "");
        DrawText(tline, 12, 36, 18,
                 info.gunsight_on_target ? Color{90, 240, 130, 255}
                                         : Color{230, 220, 160, 220});
    }

    // MB HUD: the attitude dial (pitch needle + roll arc + AoA strip), the
    // energy cluster, and the status stack (telemetry + flaps/gear) — all
    // read-only, through the ONE shared readout `r` computed above (never a
    // re-derived AoA/G/bank — the flat-instrument rule).
    draw_attitude_dial(r, info, GetScreenWidth(), GetScreenHeight());
    draw_energy_cluster(r, info, GetScreenHeight());
    draw_status_stack(r, state, params, info, GetScreenHeight());

    draw_bezel(GetScreenWidth(), GetScreenHeight());
    EndDrawing();
}

}  // namespace render
