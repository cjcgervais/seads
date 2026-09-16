#include "render/buildings.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cmath>

#include <glm/geometric.hpp>  // cross (hero placement basis)

#include "external/glad.h"  // glEnable / rlDisableBackfaceCulling parity
#include "raymath.h"        // MatrixTranslate
#include "render/building_asset.h"    // GisBuildingVertex/Batch + the .bin parser
#include "render/sudbury_gis.gen.h"
#include "render/sudbury_hero.gen.h"  // S5c rigid glTF hero placements
#include "world/snowhill_geo.gen.h"   // SF1 anchor (lock hash checked below)
#include "rlgl.h"

namespace render {

// The hero placement frame (sudbury_hero.gen.h) is derived from the SAME projection
// the building dirs are baked with; if the projection re-bakes and the hero header
// is not regenerated, this fails the build rather than floating the church.
static_assert(kSudburyHeroProjLockHash == kSudburyProjectionLockHash,
              "sudbury_hero.gen.h projection lock != sudbury_gis.gen.h — re-run "
              "offline_tool/sudbury_hero_place.py after a projection re-bake");
// SF1: the snow-mountain anchor rides the same rule — a projection re-bake that
// forgets offline_tool/sf1_snowhill_place.py fails the build, never floats the
// hill off its schoolyard.
static_assert(world::kSnowhillProjectionLockHash == kSudburyProjectionLockHash,
              "snowhill_geo.gen.h projection lock != sudbury_gis.gen.h — re-run "
              "offline_tool/sf1_snowhill_place.py after a projection re-bake");

namespace {

// VS: world-absolute vertex (baked at drape radius, |v|~15000). The FS needs the
// EYE-RELATIVE world position (so toEye/up are referenced to the eye, not the planet
// center — Fable-AFTER P0). matModel is the pure -eye translate raylib auto-sets on
// DrawMesh, so matModel*vertex == vertex - eye (the planet.cpp/rig.cpp convention);
// eye-relative also keeps the dFdx facet-normal derivative well-conditioned. Passes
// the per-building tone jitter (packed in texcoord.x).
const char* kBuildingVS = R"GLSL(#version 330
in vec3 vertexPosition;      // world-absolute (baked at drape radius)
in vec2 vertexTexCoord;      // .x = per-building tone jitter (bj); .y = wall arc-length (ws)
uniform mat4 mvp;
uniform mat4 matModel;       // raylib-set: the eye-relative transform (translate -eye)
out vec3 vPos;               // EYE-RELATIVE world position (== vertex - eye)
out float vBj;
out float vWs;               // wall-perimeter arc-length (night window-grid X)
void main() {
    vPos = (matModel * vec4(vertexPosition, 1.0)).xyz;  // vertex - eye
    vBj = vertexTexCoord.x;
    vWs = vertexTexCoord.y;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)GLSL";

// FS: FLAT-shaded mono massing. The facet normal is derived from screen-space
// derivatives of the eye-relative world position (no baked normals -> hard planes,
// the stereoscope look), flipped to face the eye so it is outward for the convex
// exterior (culling is OFF, so winding is not load-bearing). Roof-vs-wall value is
// keyed off dot(N, up) where up = the radial (planet-local) direction. Mono
// (r==g==b) so the S1 silver post tones it; the planes stay the only chroma.
//
// NIGHT WINDOW LIGHTS: when the sun is below the horizon, most houses show a
// scattering of lit windows on their WALLS (a world-anchored window grid so the
// pattern is stable as the camera flies). The one sanctioned non-mono accent: a
// gentle warm-white glow (low saturation, so the S1 post keeps it silver-warm, not
// a chroma pop). Adds the "town twinkling at night from the air" read.
const char* kBuildingFS = R"GLSL(#version 330
in vec3 vPos;                // eye-relative world position (worldPos - eye)
in float vBj;
in float vWs;                // wall-perimeter arc-length (night window-grid X coord)
uniform vec3 uSunDir;        // world light-travel dir (sun -> scene)
uniform vec3 uEye;           // world eye position (to reconstruct up = normalize(worldPos))
uniform float uWallVal;
uniform float uRoofVal;
uniform float uAmbient;
uniform float uDiffuse;
uniform vec3 uWinColor;      // window-light tint (warm-white; low sat -> stays silver)
uniform float uWinBright;    // window-light emissive strength (0 = off)
uniform float uWinLitFrac;   // fraction of a lit building's windows that are on
out vec4 finalColor;

// Dave Hoskins hashes (deterministic, no clock) — the same family the ribbon FS
// uses; the early fract() keeps them stable at the ~15 km world magnitudes here.
float hash11(float p){ p = fract(p*0.1031); p *= p+33.33; p *= p+p; return fract(p); }
float hash21(vec2 p){ vec3 p3=fract(vec3(p.xyx)*0.1031); p3+=dot(p3,p3.yzx+33.33);
                      return fract((p3.x+p3.y)*p3.z); }

void main() {
    vec3 dpx = dFdx(vPos);
    vec3 dpy = dFdy(vPos);
    vec3 N = cross(dpx, dpy);
    float nl2 = dot(N, N);
    if (nl2 < 1.0e-12) { finalColor = vec4(vec3(uWallVal * uAmbient), 1.0); return; }
    N *= inversesqrt(nl2);
    vec3 toEye = normalize(-vPos);           // eye - worldPos = -(worldPos - eye)
    if (dot(N, toEye) < 0.0) N = -N;         // outward for the convex exterior
    vec3 up = normalize(vPos + uEye);        // planet-local radial (worldPos)
    float roofness = smoothstep(0.35, 0.75, dot(N, up));
    float base = mix(uWallVal, uRoofVal, roofness) * vBj;
    // SNOW hero (vWs == -2 sentinel): the mesh carries its own snow whites in
    // vBj -- the town wall/roof values would render a snow mountain as a dark
    // roof. Sun shading still applies below: that relief IS how a white mound
    // reads against white ground.
    if (vWs < -1.5) base = vBj;
    float lit = uAmbient + uDiffuse * max(dot(N, -uSunDir), 0.0);
    vec3 col = vec3(clamp(base * lit, 0.0, 1.0));

    // --- night window lights (walls only) --------------------------------
    // HORIZONTAL window coord = the BAKED wall arc-length vWs (Fable P0: a per-fragment
    // dot(worldPos, cross(up,N)) is identically 0 since up==normalize(worldPos)). The
    // VERTICAL coord is the radial height |worldPos| (== dot(worldPos,up), which DOES
    // advance base->eave). vWs==0 on cap/roof/Superstack verts -> no windows there.
    float sunEl = dot(-uSunDir, up);                       // sun elevation over up
    float night = 1.0 - smoothstep(-0.14, 0.10, sunEl);    // 1 night .. 0 day (dusk band)
    float wallness = 1.0 - roofness;
    if (uWinBright > 0.0 && night > 0.003 && wallness > 0.5 && vWs >= 0.0) {
        float radius = length(vPos + uEye);                // radial height (== dot(wp,up))
        // ~6.5 m window bays (a few per wall, not a blanket) x ~3.2 m floors; the pane
        // is a small DISCRETE window inset in each bay (Chad: "5-7 windows a house,
        // not walls of windows"), not a lit panel.
        vec2 g = vec2(vWs / 6.5, radius / 3.2);
        vec2 cell = floor(g), f = fract(g);
        float pane = step(0.38, f.x) * step(f.x, 0.60) *
                     step(0.38, f.y) * step(f.y, 0.72);
        float litWin = step(1.0 - uWinLitFrac, hash21(cell + vBj * 17.0));
        float hasLights = step(0.12, hash11(vBj * 41.0));  // ~88% of buildings
        col += pane * litWin * hasLights * night * wallness * uWinColor * uWinBright;
    }
    finalColor = vec4(col, 1.0);
}
)GLSL";

// One draped mesh from a baked batch: world-absolute float verts at
// dir*(radius_at(dir) + h*height_scale), texcoord.x = per-building jitter. No
// normals (the FS derives a flat facet normal).
Mesh build_batch_mesh(const GisBuildingBatch& B, const GisBuildingVertex* verts,
                      const unsigned short* indices, const HeightField& hf,
                      float height_scale) {
    Mesh m{};
    m.vertexCount = B.vtx_count;
    m.triangleCount = B.idx_count / 3;
    m.vertices = static_cast<float*>(MemAlloc(sizeof(float) * 3 * B.vtx_count));
    m.texcoords =
        static_cast<float*>(MemAlloc(sizeof(float) * 2 * B.vtx_count));
    m.indices = static_cast<unsigned short*>(
        MemAlloc(sizeof(unsigned short) * B.idx_count));
    for (int i = 0; i < B.vtx_count; ++i) {
        const GisBuildingVertex& V = verts[B.vtx_off + i];
        const glm::dvec3 d(V.dir[0], V.dir[1], V.dir[2]);
        const double radius =
            hf.radius_at(d) + static_cast<double>(V.h) * height_scale;
        m.vertices[3 * i + 0] = static_cast<float>(d.x * radius);
        m.vertices[3 * i + 1] = static_cast<float>(d.y * radius);
        m.vertices[3 * i + 2] = static_cast<float>(d.z * radius);
        m.texcoords[2 * i + 0] = V.bj;
        m.texcoords[2 * i + 1] = V.ws;   // wall arc-length -> night window-grid X
    }
    for (int k = 0; k < B.idx_count; ++k)
        m.indices[k] = indices[B.idx_off + k];  // LOCAL 0-based
    UploadMesh(&m, false);
    return m;
}

// S5c: append rigid glTF hero landmarks (blender-hero-forge). Each .glb is loaded,
// placed by its baked orthonormal frame (sudbury_hero.gen.h) — world = right*x +
// up*y + fwd*z + pos0, glb-local axes (x=right, y=up, z=facade-fwd) — grounded at
// hf.radius_at(dir) minus an explicit sink (Fable-5 BEFORE-consult, SOUND-WITH-FIXES:
// orthonormal basis + raw-radius sink, rigid placement fine at 50 m/R=15 km). Pushed
// as a WORLD-ABSOLUTE mesh so it draws through the SAME mono building shader — no
// fork, guaranteed tone-consistent with the town. texcoord = (tone, -1): ws<0 => no
// night windows (the church stays dark stone). SEADS_NO_HERO A/B-bypasses the set.
// NOTE: a hero is REAL-METRIC — it deliberately does NOT scale by look.height_scale
// (the town's vertical-exaggeration dial); a sourced landmark keeps its true height.
// Keep forge glb exports < 65535 verts (raylib converts u32->u16 indices at load).
void append_hero_meshes(BuildingSurfaces& b, const HeightField& hf) {
    static const bool no_hero = std::getenv("SEADS_NO_HERO") != nullptr;
    if (no_hero) return;
    for (int hi = 0; hi < kSudburyHeroCount; ++hi) {
        const HeroPlacement& H = kSudburyHeroes[hi];
        char path[256];
        std::snprintf(path, sizeof path, "%s/%s", SEADS_ASSET_DIR, H.glb);
        if (!FileExists(path)) {
            TraceLog(LOG_WARNING, "HERO: '%s' missing — skipped", path);
            continue;
        }
        Model model = LoadModel(path);
        if (model.meshCount < 1 || model.meshes == nullptr) {
            TraceLog(LOG_WARNING, "HERO: '%s' empty glb — skipped", path);
            UnloadModel(model);
            continue;
        }
        const glm::dvec3 up(H.dir[0], H.dir[1], H.dir[2]);
        const glm::dvec3 east(H.east[0], H.east[1], H.east[2]);
        const glm::dvec3 north(H.north[0], H.north[1], H.north[2]);
        const double c = std::cos(H.heading_rad), s = std::sin(H.heading_rad);
        const glm::dvec3 fwd = c * north + s * east;   // facade forward (glb +Z)
        const glm::dvec3 right = glm::cross(up, fwd);  // glb +X (right-handed, Fable)
        // ground at the center, then shift BACK along -fwd (off the road, onto the
        // lot). setback is tangential (<< R), so the radial drop is negligible.
        const glm::dvec3 pos0 =
            up * (hf.radius_at(up) - static_cast<double>(H.sink_m)) -
            fwd * static_cast<double>(H.setback_m);
        int added = 0;
        for (int mi = 0; mi < model.meshCount; ++mi) {
            const Mesh& src = model.meshes[mi];
            if (src.vertices == nullptr || src.vertexCount < 3) continue;
            // Optional per-vertex COLOR_0 (grayscale) -> the mono shader's per-vertex
            // tone (bj), so a hero can be TWO-TONE (grey fieldstone walls / white spire
            // + trim on the church). No color attribute => uniform H.tone.
            const unsigned char* col = src.colors;  // RGBA8 or null
            Mesh m{};
            m.vertexCount = src.vertexCount;
            m.triangleCount = src.triangleCount;
            m.vertices = static_cast<float*>(
                MemAlloc(sizeof(float) * 3 * src.vertexCount));
            m.texcoords = static_cast<float*>(
                MemAlloc(sizeof(float) * 2 * src.vertexCount));
            for (int i = 0; i < src.vertexCount; ++i) {
                const glm::dvec3 lp(src.vertices[3 * i + 0],
                                    src.vertices[3 * i + 1],
                                    src.vertices[3 * i + 2]);
                const glm::dvec3 wp =
                    right * lp.x + up * lp.y + fwd * lp.z + pos0;
                m.vertices[3 * i + 0] = static_cast<float>(wp.x);
                m.vertices[3 * i + 1] = static_cast<float>(wp.y);
                m.vertices[3 * i + 2] = static_cast<float>(wp.z);
                const float shade =
                    col ? (static_cast<float>(col[4 * i]) / 255.0f) : 1.0f;
                m.texcoords[2 * i + 0] = shade * H.tone;  // bj (per-vertex mono tone)
                // ws sentinel: -1 = masonry hero (no night windows); -2 = SNOW
                // hero (SF1 fix: the FS bypasses wall/roof values -- a snow
                // mountain through the roof path drew as a dark mound and
                // Chad's drive could not find it).
                m.texcoords[2 * i + 1] = H.snow != 0 ? -2.0f : -1.0f;
            }
            if (src.indices != nullptr && src.triangleCount > 0) {
                const int nidx = src.triangleCount * 3;
                m.indices = static_cast<unsigned short*>(
                    MemAlloc(sizeof(unsigned short) * nidx));
                for (int k = 0; k < nidx; ++k) m.indices[k] = src.indices[k];
            }
            UploadMesh(&m, false);
            b.meshes.push_back(m);
            ++added;
        }
        UnloadModel(model);  // frees the raylib source; our copies are independent
        TraceLog(added > 0 ? LOG_INFO : LOG_WARNING,
                 "HERO: '%s' placed (%d mesh)", H.glb, added);
    }
}

}  // namespace

const world::BuildingColliders* building_colliders(double inflate_reserve_m,
                                                   double R_planet) {
    static world::BuildingColliders colliders;
    static bool tried = false;
    if (!tried) {
        tried = true;
        char path[512];
        std::snprintf(path, sizeof path, "%s/sudbury_buildings.bin",
                      SEADS_ASSET_DIR);
        int size = 0;
        unsigned char* data = LoadFileData(path, &size);
        if (data != nullptr && size > 0) {
            const BuildingAsset asset =
                parse_building_asset(data, static_cast<std::size_t>(size));
            UnloadFileData(data);
            if (!asset.ok) {
                TraceLog(LOG_WARNING,
                         "BUILDINGS: %s malformed — collision OFF", path);
            } else if (asset.lock_hash != kSudburyProjectionLockHash) {
                TraceLog(LOG_WARNING,
                         "BUILDINGS: %s lock mismatch — collision OFF", path);
            } else if (!asset.colliders.empty()) {
                std::vector<world::BuildingColliders::Prism> prisms;
                prisms.reserve(asset.colliders.size());
                for (const GisBuildingCollider& c : asset.colliders) {
                    prisms.push_back(
                        {glm::dvec3(c.center_dir[0], c.center_dir[1],
                                    c.center_dir[2]),
                         static_cast<double>(c.radius_m),
                         static_cast<double>(c.height_m)});
                }
                // 1024x512 uv cells (~92 m ground at the equator); u_offset 0
                // — the index only needs build/query consistency, and both
                // live inside world::BuildingColliders.
                colliders.build(std::move(prisms), 1024, 512, 0.0,
                                inflate_reserve_m, R_planet);
                TraceLog(LOG_INFO,
                         "BUILDINGS: %zu collision prisms indexed (R4f)",
                         colliders.prisms.size());
            }
        } else {
            TraceLog(LOG_WARNING, "BUILDINGS: %s missing — collision OFF",
                     path);
        }
    }
    return colliders.empty() ? nullptr : &colliders;
}

BuildingSurfaces build_building_surfaces(const HeightField& hf,
                                         const BuildingLook& look) {
    BuildingSurfaces b;
    b.look = look;
    // Baked town massing now lives in the runtime binary asset (moved out of the
    // constexpr header so the whole-map fill scales). Missing / malformed / lock-
    // mismatched -> no town batches (inert), the SAME graceful path as a no-OSM
    // bake; the glTF heroes still load. Lock hash gates a stale bake (re-bake law).
    char path[512];
    std::snprintf(path, sizeof path, "%s/sudbury_buildings.bin", SEADS_ASSET_DIR);
    int size = 0;
    unsigned char* data = LoadFileData(path, &size);
    if (data != nullptr && size > 0) {
        const BuildingAsset asset =
            parse_building_asset(data, static_cast<std::size_t>(size));
        UnloadFileData(data);
        if (!asset.ok) {
            TraceLog(LOG_WARNING, "BUILDINGS: %s malformed — no town batches", path);
        } else if (asset.lock_hash != kSudburyProjectionLockHash) {
            TraceLog(LOG_WARNING,
                     "BUILDINGS: %s projection-lock mismatch — no town batches "
                     "(re-bake needed)", path);
        } else {
            for (const GisBuildingBatch& B : asset.batches) {
                if (B.vtx_count < 3 || B.idx_count < 3) continue;  // dummy batch
                b.meshes.push_back(build_batch_mesh(
                    B, asset.verts.data(), asset.indices.data(), hf, look.height_scale));
                b.culls.push_back(BatchCull{
                    glm::dvec3(B.center_dir[0], B.center_dir[1], B.center_dir[2]),
                    B.half_angle});  // Stage-2 tile bound (heroes keep pi -> never cull)
            }
            TraceLog(LOG_INFO, "BUILDINGS: %zu batches from %s",
                     asset.batches.size(), path);
        }
    } else {
        TraceLog(LOG_WARNING, "BUILDINGS: %s missing/empty — no town batches", path);
    }
    b.planet_R = hf.R;
    // horizon top-allowance for the cull, connected to the baked relief + the height
    // dial (Fable P2 — not a bare 800 m): town buildings <= ~80 m, *height_scale, over
    // the max terrain relief. Over-estimating is safe (draws more, never false-culls).
    b.cull_h_top = hf.relief_scale + 120.0 * static_cast<double>(look.height_scale);
    append_hero_meshes(b, hf);  // S5c rigid glTF heroes ride the same mono shader
    // heroes (glTF + any no-bound batches) draw always: pad culls to match meshes
    // with the never-cull default cone (Stage-2 cull is town-only).
    while (b.culls.size() < b.meshes.size()) b.culls.emplace_back();
    if (b.meshes.empty()) {
        TraceLog(LOG_INFO, "BUILDINGS: no batches — inert (no-OSM bake?)");
        return b;  // ok=false
    }
    b.shader = LoadShaderFromMemory(kBuildingVS, kBuildingFS);
    b.loc_wall = GetShaderLocation(b.shader, "uWallVal");
    b.loc_roof = GetShaderLocation(b.shader, "uRoofVal");
    b.loc_amb = GetShaderLocation(b.shader, "uAmbient");
    b.loc_diff = GetShaderLocation(b.shader, "uDiffuse");
    b.loc_sun = GetShaderLocation(b.shader, "uSunDir");
    b.loc_eye = GetShaderLocation(b.shader, "uEye");
    b.loc_wincol = GetShaderLocation(b.shader, "uWinColor");
    b.loc_winbright = GetShaderLocation(b.shader, "uWinBright");
    b.loc_winlit = GetShaderLocation(b.shader, "uWinLitFrac");
    b.mat = LoadMaterialDefault();
    b.mat.shader = b.shader;
    b.ok = true;
    TraceLog(LOG_INFO, "BUILDINGS: %zu draped batches", b.meshes.size());
    return b;
}

void unload_building_surfaces(BuildingSurfaces& b) {
    if (!b.ok) return;
    for (Mesh& m : b.meshes) UnloadMesh(m);
    b.meshes.clear();
    UnloadShader(b.shader);
    // LoadMaterialDefault's maps are raylib-owned; only the shader is ours (freed
    // above) — do NOT UnloadMaterial (it would free the shared default texture).
    b.ok = false;
}

void draw_building_surfaces(BuildingSurfaces& b, const glm::vec3& sun_dir,
                            const glm::dvec3& eye) {
    if (!b.ok) return;
    static const bool no_buildings = std::getenv("SEADS_NO_BUILDINGS") != nullptr;
    if (no_buildings) return;  // A/B bypass (smoke + Chad's fly), read once

    // Culling OFF (the FS derives + eye-flips the facet normal, so winding is not
    // load-bearing; a prism viewed from outside is depth-ordered correctly). No
    // polygon offset: walls are vertical and the base ring is sunk into terrain, so
    // no coplanar z-fight with the terrain mesh.
    rlDisableBackfaceCulling();

    const Matrix xf =
        MatrixTranslate(static_cast<float>(-eye.x), static_cast<float>(-eye.y),
                        static_cast<float>(-eye.z));
    const glm::vec3 eye_f(static_cast<float>(eye.x), static_cast<float>(eye.y),
                          static_cast<float>(eye.z));
    const BuildingLook& L = b.look;
    SetShaderValue(b.shader, b.loc_wall, &L.wall_val, SHADER_UNIFORM_FLOAT);
    SetShaderValue(b.shader, b.loc_roof, &L.roof_val, SHADER_UNIFORM_FLOAT);
    SetShaderValue(b.shader, b.loc_amb, &L.ambient, SHADER_UNIFORM_FLOAT);
    SetShaderValue(b.shader, b.loc_diff, &L.diffuse, SHADER_UNIFORM_FLOAT);
    SetShaderValue(b.shader, b.loc_sun, &sun_dir, SHADER_UNIFORM_VEC3);
    SetShaderValue(b.shader, b.loc_eye, &eye_f, SHADER_UNIFORM_VEC3);
    SetShaderValue(b.shader, b.loc_wincol, &L.window_color, SHADER_UNIFORM_VEC3);
    SetShaderValue(b.shader, b.loc_winbright, &L.window_bright, SHADER_UNIFORM_FLOAT);
    SetShaderValue(b.shader, b.loc_winlit, &L.window_lit_frac, SHADER_UNIFORM_FLOAT);

    // Stage-2 horizon/distance cull: skip a batch whose spatial tile cone is entirely
    // beyond the reach (angular horizon + a range cap). Mirrors the tree chunk cull
    // (world/props visible_chunks). From low altitude the far hemisphere (~half the
    // map) is culled every frame; the far-hemisphere towns never touch the GPU.
    const double R = b.planet_R;
    const double len = glm::length(eye);
    const glm::dvec3 sub = len > 1.0 ? eye / len : glm::dvec3(0.0, 0.0, 1.0);
    double reach_ang = 3.15;  // R<=0 (unbuilt) -> draw all (no cull)
    if (R > 0.0) {
        const double alt = std::max(0.0, len - R);
        const double range_cap = 90000.0;  // defensive ceiling; the sphere horizon
                                           // (max ~28 km arc) ALWAYS binds first here
        const double horizon = R * std::acos(std::min(1.0, R / (R + alt))) +
                               R * std::sqrt(2.0 * b.cull_h_top / R);
        reach_ang = std::min(range_cap, horizon) / R;
    }
    int drawn = 0;
    for (std::size_t i = 0; i < b.meshes.size(); ++i) {
        const BatchCull& cb = b.culls[i];
        const double d = glm::dot(sub, cb.center_dir);
        const double ang = std::acos(d < -1.0 ? -1.0 : (d > 1.0 ? 1.0 : d));
        if (ang < reach_ang + cb.half_angle) {
            DrawMesh(b.meshes[i], b.mat, xf);
            ++drawn;
        }
    }
    static bool logged = false;  // one-shot: confirm the cull actually skips batches
    if (!logged) {
        logged = true;
        TraceLog(LOG_INFO, "BUILDINGS: cull drew %d/%zu batches (alt %.0f m)", drawn,
                 b.meshes.size(), std::max(0.0, len - R));
    }

    rlEnableBackfaceCulling();
}

}  // namespace render
