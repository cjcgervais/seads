#include "render/ribbons.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

#include <glm/geometric.hpp>

#include "external/glad.h"  // glPolygonOffset / glEnable (glad decls; impl in raylib)
#include "raymath.h"        // MatrixTranslate
#include "render/ribbon_clip.h"  // T24 excavation clip (pure, test-pinned)
#include "render/ribbon_subdiv.h"  // ★ ROAD-REPAIR: the subdivided drape (pure)
#include "render/snow_light_glsl.h"  // ROAD-REPAIR: the shared sparkle
#include "render/sudbury_gis.gen.h"
#include "rlgl.h"

namespace render {

namespace {

// VS: world-absolute vertex position (draped at bake radius) -> clip via
// raylib's auto-set mvp (model = eye-relative translate). Passes (s, v)
// through. Eye- relative rendering keeps the float magnitudes small (the
// planet's double->float seam) — identical to the water pass.
const char* kRibbonVS = R"GLSL(#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;   // (arc-length s in metres, transverse v in [-1,1])
uniform mat4 mvp;
out vec2 vSV;
void main() {
    vSV = vertexTexCoord;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)GLSL";

// FS. ROADS: a WEATHERED asphalt bed (a global light fade + ~mottle_frac of the
// length in lighter mottled patches — "less perfect") with a light duty-cycle
// dashed centerline; mono (r==g==b) so the S1 post silvers it. TRAILS: a
// packed-snow body carved by fine groomer lines running the LENGTH of the trail
// (the "comb"/corduroy a drag pan leaves) — continuous, NOT dashed.
// ★ Those lines were TRANSVERSE until Chad's S2 fly ruled otherwise, while this
// very paragraph claimed they were longitudinal. Comment and code now agree,
// and the phase is metres ACROSS the ribbon (v * uHalfW). All procedural variation is a deterministic hash of (s, v) —
// render/ reads no clock (house law); the pattern is stable frame-to-frame.
const char* kRibbonFS = R"GLSL(#version 330
in vec2 vSV;              // (s metres, v transverse in [-1,1])
uniform vec3 uBed;        // road asphalt (mono)
uniform vec3 uLine;       // road centerline (mono)
uniform float uCenterFrac;
uniform float uDashM;
uniform float uGapM;
uniform float uTrail;     // 0 road / 1 snowmobile trail
uniform float uRoadMottle;     // strength of the lighter mottled patches [0,1]
uniform float uRoadMottleFrac; // fraction of the length that mottles (~0.25)
uniform float uRoadFade;       // global light fade on the asphalt [0,1]
uniform vec3 uTrailColor;      // trail body (subtle clay: light brown/grey/yellow)
uniform float uTrailMottle;    // subtle natural variation on the clay path [0,1]
uniform float uWinter;         // season snow amount [0,1] (0 = summer, identity)
uniform vec3 uTrailWinterColor;// groomed packed snow (darker+bluer than wild snow)
uniform float uCorduroy;       // groomer cross-line depth [0,1]
uniform float uCorduroyM;      // groomer line spacing ACROSS the trail (m)
uniform float uHalfW;          // THIS ribbon's half-width (m), recovered
                               // from its drawn vertices (INV-6)
out vec4 finalColor;

// Dave Hoskins hash (deterministic, no clock).
float hash11(float p){ p = fract(p*0.1031); p *= p+33.33; p *= p+p; return fract(p); }
float hash21(vec2 p){ vec3 p3=fract(vec3(p.xyx)*0.1031); p3+=dot(p3,p3.yzx+33.33);
                      return fract((p3.x+p3.y)*p3.z); }
float vnoise(float x){ float i=floor(x), f=fract(x); f=f*f*(3.0-2.0*f);
                       return mix(hash11(i), hash11(i+1.0), f); }

void main() {
    float s = vSV.x;
    float v = vSV.y;
    if (uTrail > 0.5) {
        // Subtle smooth CLAY path (light brown/grey/yellow) with a whisper of
        // natural mottle so it isn't a painted stripe. Winter groomer/corduroy
        // lines return later with the weather-dynamics pass — removed for now.
        float m = (vnoise(s*0.04) - 0.5 +
                   (hash21(floor(vec2(s*0.5, (v*0.5+0.5)*3.0))) - 0.5)) * uTrailMottle;
        vec3 body = uTrailColor * (1.0 + m);
        // ★ S1: under winter the trail is GROOMED SNOW, not a clay path.
        if (uWinter > 0.0) {
            // CORDUROY: the groomer's transverse ridges. This is what makes a
            // trail legible in a white world -- the trail is not brighter than
            // the snowpack around it (it is darker and bluer, being packed),
            // so TEXTURE carries the read, exactly as it does in real life.
            // Softened toward the edges so it does not alias into a moire at
            // grazing flight angles (the lesson the road's high-freq mottle
            // already taught: it flickered and was removed).
            // ★ RULED (Chad, S2 fly 2026-08-10): the groomer lines run PARALLEL
            // to the length of the trail, never across it. He is right, and the
            // code already agreed with him in prose while doing the opposite --
            // this FS's own docstring said "fine LONGITUDINAL groomer lines
            // running the whole length" while the shader modulated ARC LENGTH
            // (sin(s*...)), which draws them transverse. A drag pan leaves
            // ridges ALONG the direction of grooming; that is the corduroy
            // anyone who has ridden a groomed trail is looking for.
            //
            // So the phase is the TRANSVERSE position in metres: v is [-1,1]
            // across the ribbon, uHalfW is that ribbon's own half-width taken
            // from its DRAWN vertices (INV-6 -- never a look-constant, never a
            // second width), so v*uHalfW is metres off the centerline and lines
            // of constant phase run down the trail.
            float xm = v * uHalfW;
            float ph = xm * (6.28318530718 / max(uCorduroyM, 0.05));
            float cord = sin(ph);
            // ANALYTIC ANTI-ALIAS. Fine parallel lines viewed at a grazing
            // angle from a plane are a moire generator -- the road's high-freq
            // mottle already taught this tree that lesson and was removed for
            // it. fwidth(ph) is the phase change per pixel; once that
            // approaches pi the pattern is under-sampled, so fade it to flat
            // instead of letting it alias. Costs one derivative and means the
            // corduroy can be FINE up close (which is the point) without
            // shimmering at altitude.
            float w = fwidth(ph);
            cord *= 1.0 / (1.0 + (w / 3.14159265) * (w / 3.14159265));
            float edge = 1.0 - smoothstep(0.55, 1.0, abs(v));
            vec3 groomed = uTrailWinterColor *
                           (1.0 + m * 0.35 + cord * uCorduroy * edge);
            body = mix(body, groomed, clamp(uWinter, 0.0, 1.0));
        }
        finalColor = vec4(clamp(body, 0.0, 1.0), 1.0);
        return;
    }
    // ROAD: the asphalt stays DARK + CONTINUOUS — only a LIGHT fade + SMOOTH low-freq
    // mottling, never erasure (Chad: light fading/mottling, not full-erasure sections).
    // The high-freq per-texel mottle is GONE (it aliased -> flicker at grazing flight);
    // only the smooth ~28 m patch noise remains. Fades toward a DARK worn grey (0.30) so
    // even a peak patch can't lighten the road into the lit terrain (stays a road).
    float patch = smoothstep(1.0 - uRoadMottleFrac, 1.0, vnoise(s*0.035));  // smooth 0..1
    float lighten = clamp(uRoadFade*0.5 + patch*uRoadMottle, 0.0, 0.5);
    vec3 asphalt = mix(uBed, vec3(0.30), lighten);       // toward a DARK worn grey (mono)
    // light duty-cycle dashed centerline (Fable P1-3).
    float period = max(uDashM + uGapM, 1.0e-3);
    float lit = 1.0 - step(uDashM / period, fract(s / period));
    vec3 c = (abs(v) < uCenterFrac && lit > 0.5) ? uLine : asphalt;
    finalColor = vec4(c, 1.0);
}
)GLSL";

// One draped mesh from a baked path: world-absolute float verts at
// dir*(facet_radius_at(dir)+lift), texcoords (s, v). No normals (the FS is
// unlit — roads read by value + dash, not shading). facet_radius_at is the
// rendered terrain surface itself (R4d) — same field, the mesh's own
// interpolation, so the ribbon can neither float over a facet dip nor drown
// under a facet rise. `kept` is the T24 cut-clipped index list (LOCAL
// 0-based); a fully-clipped path (kept empty) builds nothing.
//
// ★ ROAD-REPAIR / ONAPING SINK. That drape is EXACT AT THE BAKED RUNGS and a
// straight CHORD between them, and the baked rungs are 52.67 m apart at the
// median (up to 79.91 m) — so over a concave grade break the deck bridges
// ABOVE the ground the machine is standing on and the body ends up inside the
// asphalt. `max_seg_m` (the [ribbons] dial) subdivides each drawn quad at
// build time so no chord exceeds it; 0.0 is the byte-identical identity. The
// geometry itself is render/ribbon_subdiv.h (pure, ctest-pinned) — this
// function is the UPLOAD, and it returns a LIST because a subdivided road path
// runs past raylib's unsigned-short index limit.
std::vector<Mesh> build_path_meshes(const GisRibbonPath& P,
                                    const HeightField& hf, float lift,
                                    int subdiv, int tiles, double max_seg_m,
                                    double max_tr_m,
                                    const std::vector<unsigned short>& kept,
                                    long* verts_out) {
    std::vector<Mesh> out;
    const std::vector<RibbonBatchCPU> batches = build_ribbon_batches(
        P, kept,
        [&hf, subdiv, tiles](const glm::dvec3& d) {
            return drawn_radius_at(hf, d, subdiv, tiles);  // the RENDERED
                                                           // surface
        },
        lift, hf.R, max_seg_m, max_tr_m);
    for (const RibbonBatchCPU& b : batches) {
        if (b.idx.empty()) continue;
        Mesh m{};
        m.vertexCount = static_cast<int>(b.pos.size() / 3);
        m.triangleCount = static_cast<int>(b.idx.size()) / 3;
        m.vertices =
            static_cast<float*>(MemAlloc(sizeof(float) * b.pos.size()));
        m.texcoords =
            static_cast<float*>(MemAlloc(sizeof(float) * b.uv.size()));
        m.indices = static_cast<unsigned short*>(
            MemAlloc(sizeof(unsigned short) * b.idx.size()));
        std::copy(b.pos.begin(), b.pos.end(), m.vertices);
        std::copy(b.uv.begin(), b.uv.end(), m.texcoords);
        std::copy(b.idx.begin(), b.idx.end(), m.indices);
        if (verts_out != nullptr) *verts_out += m.vertexCount;
        UploadMesh(&m, false);
        out.push_back(m);
    }
    return out;
}

}  // namespace

const world::LineNetwork& sudbury_linework(double R_planet, double u_offset) {
    // ★ WINTER_LAW §2.3: the linework PROMOTED out of render/, exactly as
    // HeightField was promoted out of render/sphere_param.h. Built here rather
    // than in app/main.cpp because this file is where the baked vertices are
    // already read -- one reader of kSudburyRibbonVerts, so the corridor the
    // sled drives can never be recovered from a different array than the one
    // the GPU draws.
    static world::LineNetwork net;
    static bool built = false;
    if (built) return net;
    built = true;
    net.u_offset = u_offset;
    std::vector<glm::dvec3> dirs;
    std::vector<float> arcs;
    for (std::size_t pi = 0; pi < kSudburyRibbonPathCount; ++pi) {
        const GisRibbonPath& P = kSudburyRibbonPaths[pi];
        if (P.vtx_count < 4) continue;
        if (P.kind == 3) continue;  // waterways are not corridors
        dirs.clear();
        arcs.clear();
        dirs.reserve(static_cast<std::size_t>(P.vtx_count));
        arcs.reserve(static_cast<std::size_t>(P.vtx_count));
        for (int i = 0; i < P.vtx_count; ++i) {
            const GisRibbonVertex& V = kSudburyRibbonVerts[P.vtx_off + i];
            dirs.emplace_back(V.dir[0], V.dir[1], V.dir[2]);
            arcs.push_back(V.s);
        }
        net.add_path(dirs.data(), arcs.data(), dirs.size(),
                     static_cast<world::LineKind>(P.kind), R_planet);
    }
    net.build_index();
    TraceLog(LOG_INFO,
             "WINTER S2 linework: %zu stations, %zu junctions from %zu baked "
             "paths",
             net.st.size(), net.junctions.size(),
             static_cast<std::size_t>(kSudburyRibbonPathCount));
    return net;
}

RibbonSurfaces build_ribbon_surfaces(const HeightField& hf,
                                     const RibbonLook& look, int subdiv,
                                     int tiles,
                                     const std::vector<CutDisk>& cuts) {
    RibbonSurfaces r;
    r.look = look;
    const double t0 = GetTime();
    for (std::size_t pi = 0; pi < kSudburyRibbonPathCount; ++pi) {
        const GisRibbonPath& P = kSudburyRibbonPaths[pi];
        if (P.vtx_count < 3 || P.idx_count < 3)
            continue;  // dummy (count 0) path
        if (P.kind == 3)
            continue;  // rivers are MIRROR water — drawn by
                       // render/river_surfaces
        // T24: clip the drape at the excavation cuts (the terrain's own rule).
        const std::vector<unsigned short> kept =
            ribbon_indices_outside_cuts(pi, cuts, hf.R);
        if (kept.empty()) continue;  // fully swallowed by a cut
        const std::vector<Mesh> built =
            build_path_meshes(P, hf, look.lift_m, subdiv, tiles,
                              look.max_seg_m, look.max_tr_m, kept,
                              &r.vert_count);
        if (built.empty()) continue;
        // ★ ROAD-REPAIR: a path can now be several meshes (the u16 index
        // limit), so kind and half-width are pushed PER MESH -- they are
        // per-mesh uniforms and the draw loop indexes them by mesh.
        for (const Mesh& m : built) {
            r.meshes.push_back(m);
            r.kinds.push_back(P.kind);
        }
        const std::size_t hw_from = r.half_w_m.size();
        // ★ INV-6: this batch's own half-width, measured off the vertices it is
        // about to rasterize -- the MEDIAN over its (L,R) pairs, immune to the
        // 2x miter inflation at corners and to the wilderness width taper at
        // the run ends (the same reason world/linework.h takes a median). The
        // corduroy spacing is metres-across, so it needs a width; taking one
        // from a look-constant here would be the parallel constant §2.4b bans.
        {
            std::vector<float> hw;
            hw.reserve(static_cast<std::size_t>(P.vtx_count) / 2);
            for (int k = 0; k + 1 < P.vtx_count; k += 2) {
                const GisRibbonVertex& A = kSudburyRibbonVerts[P.vtx_off + k];
                const GisRibbonVertex& B = kSudburyRibbonVerts[P.vtx_off + k + 1];
                const glm::dvec3 a(A.dir[0], A.dir[1], A.dir[2]);
                const glm::dvec3 b(B.dir[0], B.dir[1], B.dir[2]);
                hw.push_back(static_cast<float>(
                    hf.R * std::asin(std::min(1.0, 0.5 * glm::length(a - b)))));
            }
            if (hw.empty()) {
                r.half_w_m.push_back(2.6f);
            } else {
                std::nth_element(hw.begin(), hw.begin() + hw.size() / 2, hw.end());
                r.half_w_m.push_back(hw[hw.size() / 2]);
            }
            // The path's ONE recovered half-width, stamped on each of its
            // meshes: the width is a property of the baked PATH, and slicing
            // that path into u16-sized meshes must not turn it into several
            // different widths (the corduroy spacing would then step at an
            // invisible batch boundary).
            const float hw1 = r.half_w_m[hw_from];
            for (std::size_t k = 1; k < built.size(); ++k)
                r.half_w_m.push_back(hw1);
        }
    }
    if (r.meshes.empty()) {
        TraceLog(LOG_INFO,
                 "RIBBONS: no baked paths — inert (roads-only bake?)");
        return r;  // ok=false
    }
    r.shader = LoadShaderFromMemory(kRibbonVS, kRibbonFS);
    r.loc_bed = GetShaderLocation(r.shader, "uBed");
    r.loc_line = GetShaderLocation(r.shader, "uLine");
    r.loc_center = GetShaderLocation(r.shader, "uCenterFrac");
    r.loc_dash = GetShaderLocation(r.shader, "uDashM");
    r.loc_gap = GetShaderLocation(r.shader, "uGapM");
    r.loc_trail = GetShaderLocation(r.shader, "uTrail");
    r.loc_mottle = GetShaderLocation(r.shader, "uRoadMottle");
    r.loc_mottle_frac = GetShaderLocation(r.shader, "uRoadMottleFrac");
    r.loc_fade = GetShaderLocation(r.shader, "uRoadFade");
    r.loc_tcolor = GetShaderLocation(r.shader, "uTrailColor");
    r.loc_tmottle = GetShaderLocation(r.shader, "uTrailMottle");
    r.loc_winter = GetShaderLocation(r.shader, "uWinter");
    r.loc_twcolor = GetShaderLocation(r.shader, "uTrailWinterColor");
    r.loc_cord = GetShaderLocation(r.shader, "uCorduroy");
    r.loc_cordm = GetShaderLocation(r.shader, "uCorduroyM");
    r.loc_halfw = GetShaderLocation(r.shader, "uHalfW");
    r.mat = LoadMaterialDefault();
    r.mat.shader = r.shader;
    r.ok = true;
    TraceLog(LOG_INFO,
             "RIBBONS: %zu draped meshes, %ld verts, max_seg_m %.2f, "
             "max_tr_m %.2f, built in %.0f ms (roads + trails)",
             r.meshes.size(), r.vert_count, look.max_seg_m, look.max_tr_m,
             (GetTime() - t0) * 1000.0);
    return r;
}

void unload_ribbon_surfaces(RibbonSurfaces& r) {
    if (!r.ok) return;
    for (Mesh& m : r.meshes) UnloadMesh(m);
    r.meshes.clear();
    UnloadShader(r.shader);
    // LoadMaterialDefault's maps are raylib-owned; the shader is ours (unloaded
    // above) — do NOT UnloadMaterial (it would free the shared default
    // texture).
    r.ok = false;
}

void draw_ribbon_surfaces(RibbonSurfaces& r, const glm::dvec3& eye,
                          float winter) {
    if (!r.ok) return;
    static const bool no_ribbons = std::getenv("SEADS_NO_RIBBONS") != nullptr;
    if (no_ribbons) return;  // A/B bypass (smoke + Chad's fly), read once

    // Slope-scaled depth offset: pull the near-tangent ribbon toward the eye so
    // it wins the z-fight against the coincident terrain at grazing
    // (low-flight) angles — a thin draped strip needs a stronger factor than
    // the water's -1 (Fable P1-2). Paired with the small facet-clearance lift
    // baked into the vertices (R4d: the drape sits on the rendered facet, so
    // the lift is clearance, not a burial-tail guess).
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-2.0f, -4.0f);
    rlDisableBackfaceCulling();  // a road on a side-slope reads from either
                                 // side

    const Matrix xf =
        MatrixTranslate(static_cast<float>(-eye.x), static_cast<float>(-eye.y),
                        static_cast<float>(-eye.z));
    const RibbonLook& L = r.look;
    for (std::size_t i = 0; i < r.meshes.size(); ++i) {
        const float uTrail =
            r.kinds[i] == 2 ? 1.0f : 0.0f;  // per-mesh kind selector
        SetShaderValue(r.shader, r.loc_trail, &uTrail, SHADER_UNIFORM_FLOAT);
        SetShaderValue(r.shader, r.loc_bed, &L.road_bed, SHADER_UNIFORM_VEC3);
        SetShaderValue(r.shader, r.loc_line, &L.road_line, SHADER_UNIFORM_VEC3);
        SetShaderValue(r.shader, r.loc_center, &L.road_center_frac,
                       SHADER_UNIFORM_FLOAT);
        SetShaderValue(r.shader, r.loc_dash, &L.road_dash_m,
                       SHADER_UNIFORM_FLOAT);
        SetShaderValue(r.shader, r.loc_gap, &L.road_gap_m,
                       SHADER_UNIFORM_FLOAT);
        SetShaderValue(r.shader, r.loc_mottle, &L.road_mottle,
                       SHADER_UNIFORM_FLOAT);
        SetShaderValue(r.shader, r.loc_mottle_frac, &L.road_mottle_frac,
                       SHADER_UNIFORM_FLOAT);
        SetShaderValue(r.shader, r.loc_fade, &L.road_fade,
                       SHADER_UNIFORM_FLOAT);
        SetShaderValue(r.shader, r.loc_tcolor, &L.trail_color,
                       SHADER_UNIFORM_VEC3);
        SetShaderValue(r.shader, r.loc_winter, &winter, SHADER_UNIFORM_FLOAT);
        SetShaderValue(r.shader, r.loc_twcolor, &L.trail_winter_color,
                       SHADER_UNIFORM_VEC3);
        SetShaderValue(r.shader, r.loc_cord, &L.trail_corduroy,
                       SHADER_UNIFORM_FLOAT);
        SetShaderValue(r.shader, r.loc_cordm, &L.trail_corduroy_m,
                       SHADER_UNIFORM_FLOAT);
        // Per-mesh: the width of THIS ribbon, measured off its own verts.
        const float hw = i < r.half_w_m.size() ? r.half_w_m[i] : 2.6f;
        SetShaderValue(r.shader, r.loc_halfw, &hw, SHADER_UNIFORM_FLOAT);
        SetShaderValue(r.shader, r.loc_tmottle, &L.trail_mottle,
                       SHADER_UNIFORM_FLOAT);
        DrawMesh(r.meshes[i], r.mat, xf);
    }

    rlEnableBackfaceCulling();
    glPolygonOffset(0.0f, 0.0f);
    glDisable(GL_POLYGON_OFFSET_FILL);
}

}  // namespace render

// ============================================================================
// SF2-BANKS -- THE OREO SNOWBANKS (WINTER_LAW 3.6c). Geometry comes from
// render/bank_mesh (pure, samples the REAL SnowpackField::depth_at, so the
// drawn bank and the driven bank are one field); this is the upload, the
// gravel-speckle shader, and a draw pass that reuses the ribbons' exact
// eye-relative xf / polygon-offset / cull-off block (red-team F7).
// ============================================================================

namespace render {

namespace {

const char* kBankVS = R"GLSL(#version 330
in vec3 vertexPosition;
in vec3 vertexNormal;     // ★ ROAD-REPAIR: the analytic strip normal
in vec2 vertexTexCoord;   // (arc-length s in metres, ring v: 0 road edge ..
                          //  1 outer bank edge, 1.0->1.25 the burial skirt)
uniform mat4 mvp;
out vec2 vSV;
out vec3 vN;
out vec3 vPos;            // world-ABSOLUTE (the drape's own space)
void main() {
    vSV = vertexTexCoord;
    vN = vertexNormal;
    vPos = vertexPosition;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)GLSL";

// The OREO FS. Base = snowbank white, slightly darker/bluer than wild pack
// (white-on-white is illegible -- the S1 trail lesson; banks read by value +
// TEXTURE). The gravel is a deterministic hash speckle of ~22 cm grains in
// (s, v*width) metres, densified toward the CREST band (the plow's dirtiest
// throw -- that is the oreo: white cream, dark crumb), with the corduroy's
// analytic anti-alias discipline (fwidth fade, never moire at altitude), and
// faded out across the burial skirt.
//
// ★ ROAD-REPAIR (Chad: "the sparkle drops out at every road edge"). The oreo
// above is an ALBEDO, and until now it was written straight to the framebuffer
// -- so a bank strip was a flat unlit band that did not know whether it was
// noon or midnight, while the snowfield two metres away was lit, moonlit and
// sparkling. That is the drop-out, and it was structural: the sparkle lived in
// the planet FS and nowhere else. So the albedo now runs through the SAME
// shape the planet's ground does --
//
//     lit = albedo * (uNightFill + uDayGain * ndl) + albedo * uMoonFill * ndlM
//           + albLum * uWinterNightGlow * nightAmt
//           + (moon/star sparkle + sun sparkle)
//
// -- with every dial single-sourced from the planet's own uniforms that frame
// (render::BankLight) and the sparkle lattice itself concatenated from
// render/snow_light_glsl, the string the planet FS uses. Two consumers, one
// lattice: a bank crest and the field beside it cannot glint differently.
// uLit == 0 restores the pre-repair flat pass verbatim (SEADS_BANK_LIT=0).
const char* kBankFSBody = R"GLSL(
in vec2 vSV;
in vec3 vN;
in vec3 vPos;
out vec4 finalColor;
uniform vec3 uBase;
uniform float uDensity;   // fraction of grains showing gravel [0,1]
uniform float uDark;      // fleck darkness bite [0,1]
uniform float uSmudge;    // extra density at the crest band
uniform float uCrestV;    // ring-normalized crest position
uniform float uWidthM;    // rise + fall, metres across the strip
uniform vec3 uEye;        // world-absolute eye (the eye-relative seam)
uniform vec3 uSunDir;     // light-travel, sun -> scene
uniform vec3 uMoonDir;
uniform float uMoonFill;
uniform float uDayGain;
uniform float uNightFill;
uniform vec3 uWinterNightGlow;
uniform float uSnowSparkle;
uniform float uSnowSparkleSharp;
uniform float uSunSparkle;
uniform float uSunSparkleSharp;
uniform float uLit;       // 0 = the pre-repair flat pass
float hash12(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}
void main() {
    vec2 grain = vec2(vSV.x, vSV.y * uWidthM) / 0.22;
    float h = hash12(floor(grain));
    float crest = exp(-pow((vSV.y - uCrestV) / 0.28, 2.0));
    float dens = clamp(uDensity * (1.0 + uSmudge * crest), 0.0, 0.95);
    float sp = step(1.0 - dens, h);
    // Analytic anti-alias, ENERGY-PRESERVING (Chad's drive: "too white from
    // up high"): once a grain is subpixel, fade toward the MEAN coverage
    // instead of toward clean white -- from altitude the bank keeps its
    // dirty-grey plow read instead of bleaching to a pure white band. Near
    // the eye the term resolves to binary grains (the oreo crumb).
    float w = fwidth(grain.x) + fwidth(grain.y);
    float fade = 1.0 / (1.0 + w * w * 0.25);
    float cover = mix(dens, sp, fade);
    // The skirt is plain snow diving under the terrain -- no gravel there.
    float skirt = smoothstep(1.0, 1.2, vSV.y);
    vec3 albedo = uBase * (1.0 - cover * uDark * (1.0 - skirt));

    // ---- the lit path (ROAD-REPAIR) -------------------------------------
    // Every derivative below is taken in UNIFORM control flow: there is no
    // branch in this shader, and uLit multiplies the RESULT rather than
    // guarding the computation, so fwidth() inside snow_sparkle_glint is
    // defined (the GLSL rule planet.cpp already keeps for the lake glint).
    vec3 fragRel = vPos - uEye;
    float dist = length(fragRel);
    vec3 viewDir = fragRel / max(dist, 1e-3);
    vec3 fragDir = normalize(vPos);
    vec3 shN = normalize(vN);
    vec3 toSun = -normalize(uSunDir);
    vec3 toMoon = -normalize(uMoonDir);
    float ndl = max(dot(shN, toSun), 0.0);
    float ndlMoon = max(dot(shN, toMoon), 0.0);
    // The SAME terminator the planet hoists: sun elevation at this fragment's
    // own place on the globe, not a camera-space guess.
    float sunElHere = dot(toSun, fragDir);
    float nightAmt = 1.0 - smoothstep(-0.10, 0.05, sunElHere);
    vec3 lit = albedo * (uNightFill + uDayGain * ndl);
    lit += albedo * uMoonFill * ndlMoon;
    // S1b: snow keeps a NEUTRAL lift after dark, scaled by albedo LUMINANCE
    // (per-channel would tint it cream -- the planet's own finding).
    float albLum = dot(albedo, vec3(0.299, 0.587, 0.114));
    lit += vec3(albLum) * uWinterNightGlow * nightAmt;

    vec3 spc = snow_sparkle_cell(fragDir);
    float spHost = snow_sparkle_host(spc);
    vec3 spN = snow_sparkle_facet(spc, shN);
    float spGlint = snow_sparkle_glint(spN, uMoonDir, viewDir,
                                       uSnowSparkleSharp, spHost, dist);
    // kStar: starlight ALWAYS glints (Chad, explicitly) -- the moon only adds
    // to it. Same constant, same gate shape as the planet's, minus its
    // winterSurf mask: a snowbank IS a winter surface, unconditionally.
    float kStar = 0.15;
    float spGate = nightAmt * (kStar + uMoonFill * ndlMoon);
    float snowSparkle = clamp(spGlint * spGate, 0.0, 1.0) * uSnowSparkle;
    float ssGlint = snow_sparkle_glint(spN, uSunDir, viewDir,
                                       uSunSparkleSharp, spHost, dist);
    // Day only, sun-facing. The planet's compaction suppression has no
    // meaning here (a plow windrow carries no track compaction channel), so
    // it is absent rather than passed a fake 0 -- and the two gates hand over
    // at the same terminator either way.
    float ssGate = (1.0 - nightAmt) * ndl;
    float sunSparkle = clamp(ssGlint * ssGate, 0.0, 1.0) * uSunSparkle;
    lit += vec3(snowSparkle + sunSparkle);

    finalColor = vec4(clamp(mix(albedo, lit, clamp(uLit, 0.0, 1.0)),
                            0.0, 1.0), 1.0);
}
)GLSL";

}  // namespace

BankSurfaces build_bank_surfaces(const HeightField& hf, int subdiv, int tiles,
                                 const world::SnowpackField& snow,
                                 const BankBuildParams& p, const BankLook& look,
                                 const std::vector<CutDisk>& cuts) {
    BankSurfaces b;
    b.look = look;
    const double t0 = GetTime();
    long verts = 0;
    std::vector<BankStripCPU> strips =
        build_bank_strips(hf, subdiv, tiles, snow, p, cuts, &verts);
    if (strips.empty()) {
        TraceLog(LOG_INFO, "BANKS: no plowed-road strips -- inert");
        return b;  // ok=false
    }
    // BATCH the (many small) strips into as few u16 meshes as fit: thousands
    // of short streets -> thousands of strips -> a draw-call catastrophe if
    // uploaded 1:1 (measured 12,818 meshes on the shipped bake). Disjoint
    // triangle lists concatenate freely with a base-vertex offset.
    std::vector<float> pos, uv, nrm;
    std::vector<unsigned short> idx;
    auto flush_mesh = [&]() {
        if (pos.empty()) return;
        Mesh m{};
        m.vertexCount = static_cast<int>(pos.size() / 3);
        m.triangleCount = static_cast<int>(idx.size() / 3);
        m.vertices = static_cast<float*>(MemAlloc(sizeof(float) * pos.size()));
        m.texcoords = static_cast<float*>(MemAlloc(sizeof(float) * uv.size()));
        // ★ ROAD-REPAIR: the strips carry normals now (they are lit).
        m.normals = static_cast<float*>(MemAlloc(sizeof(float) * nrm.size()));
        m.indices = static_cast<unsigned short*>(
            MemAlloc(sizeof(unsigned short) * idx.size()));
        std::copy(pos.begin(), pos.end(), m.vertices);
        std::copy(uv.begin(), uv.end(), m.texcoords);
        std::copy(nrm.begin(), nrm.end(), m.normals);
        std::copy(idx.begin(), idx.end(), m.indices);
        UploadMesh(&m, false);
        b.meshes.push_back(m);
        pos.clear();
        uv.clear();
        nrm.clear();
        idx.clear();
    };
    for (BankStripCPU& s : strips) {
        const std::size_t base = pos.size() / 3;
        if (base + s.pos.size() / 3 > 65535u) flush_mesh();
        const std::size_t base2 = pos.size() / 3;
        pos.insert(pos.end(), s.pos.begin(), s.pos.end());
        uv.insert(uv.end(), s.uv.begin(), s.uv.end());
        nrm.insert(nrm.end(), s.nrm.begin(), s.nrm.end());
        for (std::uint16_t i : s.idx)
            idx.push_back(static_cast<unsigned short>(base2 + i));
    }
    flush_mesh();
    // The crest position for the smudge band: the ring whose composed profile
    // is tallest, normalized -- asked of the SAME knot list the geometry used.
    std::size_t rings_for_log = 0;
    {
        const std::vector<double> rings =
            bank_ring_offsets(snow.p, p.chord_tol_m, p.max_rings);
        rings_for_log = rings.size();
        const double end = snow.p.bank_rise_m + snow.p.bank_fall_m;
        b.width_m = static_cast<float>(end);
        double best_e = snow.p.bank_rise_m, best_h = -1.0;
        for (double e : rings) {
            const double feather =
                e < snow.p.corridor_edge_m
                    ? e / std::max(1e-6, snow.p.corridor_edge_m)
                    : 1.0;
            const double h = feather + snow.bank_profile(e);
            if (h > best_h) {
                best_h = h;
                best_e = e;
            }
        }
        b.crest_v = static_cast<float>(best_e / std::max(1e-6, end));
    }
    // ★ ROAD-REPAIR: the FS is the shared sparkle lattice + the oreo body, in
    // that order (the body calls snow_sparkle_*), so the two consumers of
    // kSnowSparkleGLSL compile the identical text.
    const std::string bank_fs =
        std::string("#version 330\n") + kSnowSparkleGLSL + kBankFSBody;
    b.shader = LoadShaderFromMemory(kBankVS, bank_fs.c_str());
    b.loc_base = GetShaderLocation(b.shader, "uBase");
    b.loc_density = GetShaderLocation(b.shader, "uDensity");
    b.loc_dark = GetShaderLocation(b.shader, "uDark");
    b.loc_smudge = GetShaderLocation(b.shader, "uSmudge");
    b.loc_crest = GetShaderLocation(b.shader, "uCrestV");
    b.loc_width = GetShaderLocation(b.shader, "uWidthM");
    b.loc_eye = GetShaderLocation(b.shader, "uEye");
    b.loc_sun = GetShaderLocation(b.shader, "uSunDir");
    b.loc_moon = GetShaderLocation(b.shader, "uMoonDir");
    b.loc_moon_fill = GetShaderLocation(b.shader, "uMoonFill");
    b.loc_day_gain = GetShaderLocation(b.shader, "uDayGain");
    b.loc_night_fill = GetShaderLocation(b.shader, "uNightFill");
    b.loc_night_glow = GetShaderLocation(b.shader, "uWinterNightGlow");
    b.loc_snow_sparkle = GetShaderLocation(b.shader, "uSnowSparkle");
    b.loc_snow_sparkle_sharp = GetShaderLocation(b.shader, "uSnowSparkleSharp");
    b.loc_sun_sparkle = GetShaderLocation(b.shader, "uSunSparkle");
    b.loc_sun_sparkle_sharp = GetShaderLocation(b.shader, "uSunSparkleSharp");
    b.loc_lit = GetShaderLocation(b.shader, "uLit");
    b.mat = LoadMaterialDefault();
    b.mat.shader = b.shader;
    b.ok = true;
    const BankStripContinuity cont = bank_strip_continuity(
        strips, static_cast<int>(rings_for_log) + std::max(1, p.skirt_rings));
    TraceLog(LOG_INFO,
             "BANKS: %zu strip meshes, %ld verts, built in %.0f ms (oreo, lit) "
             "| rings %d+%d | jag p99 %.3f max %.3f m, ends p99 %.3f max %.3f m",
             b.meshes.size(), verts, (GetTime() - t0) * 1000.0,
             static_cast<int>(rings_for_log), std::max(1, p.skirt_rings),
             cont.step_p99, cont.step_max, cont.end_amp_p99, cont.end_amp_max);
    return b;
}

void unload_bank_surfaces(BankSurfaces& b) {
    if (!b.ok) return;
    for (Mesh& m : b.meshes) UnloadMesh(m);
    b.meshes.clear();
    UnloadShader(b.shader);
    b.ok = false;
}

void draw_bank_surfaces(BankSurfaces& b, const glm::dvec3& eye,
                        const BankLight& light) {
    if (!b.ok) return;
    static const bool no_banks = std::getenv("SEADS_NO_BANKS") != nullptr;
    if (no_banks) return;  // A/B bypass, the ribbon pattern
    // ★ ROAD-REPAIR A/B: SEADS_BANK_LIT=0 draws the pre-repair FLAT oreo
    // (uLit == 0 makes the mix() return the albedo verbatim), so the lit pass
    // can be compared against the thing it replaced in one process.
    static const bool lit_off = [] {
        const char* e = std::getenv("SEADS_BANK_LIT");
        return e != nullptr && std::atoi(e) == 0;
    }();

    // The ribbons' exact block (red-team F7): polygon offset wins the z-fight
    // against the coincident terrain at grazing angles; eye-relative xf keeps
    // world-absolute floats from jittering at speed; cull off because a bank
    // on a side-slope reads from either side.
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-2.0f, -4.0f);
    rlDisableBackfaceCulling();
    const Matrix xf =
        MatrixTranslate(static_cast<float>(-eye.x), static_cast<float>(-eye.y),
                        static_cast<float>(-eye.z));
    SetShaderValue(b.shader, b.loc_base, &b.look.base, SHADER_UNIFORM_VEC3);
    SetShaderValue(b.shader, b.loc_density, &b.look.speckle_density,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(b.shader, b.loc_dark, &b.look.speckle_dark,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(b.shader, b.loc_smudge, &b.look.crest_smudge,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(b.shader, b.loc_crest, &b.crest_v, SHADER_UNIFORM_FLOAT);
    SetShaderValue(b.shader, b.loc_width, &b.width_m, SHADER_UNIFORM_FLOAT);
    // ★ ROAD-REPAIR: the light, single-sourced from the planet pass's own
    // values this frame (render/draw.cpp fills BankLight from the same
    // SnowParams / SunParams / moon fill draw_planet is handed).
    const glm::vec3 eye_f(eye);
    SetShaderValue(b.shader, b.loc_eye, &eye_f, SHADER_UNIFORM_VEC3);
    SetShaderValue(b.shader, b.loc_sun, &light.sun_dir, SHADER_UNIFORM_VEC3);
    SetShaderValue(b.shader, b.loc_moon, &light.moon_dir, SHADER_UNIFORM_VEC3);
    SetShaderValue(b.shader, b.loc_moon_fill, &light.moon_fill,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(b.shader, b.loc_day_gain, &light.day_gain,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(b.shader, b.loc_night_fill, &light.night_fill,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(b.shader, b.loc_night_glow, &light.night_glow,
                   SHADER_UNIFORM_VEC3);
    SetShaderValue(b.shader, b.loc_snow_sparkle, &light.snow_sparkle,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(b.shader, b.loc_snow_sparkle_sharp,
                   &light.snow_sparkle_sharp, SHADER_UNIFORM_FLOAT);
    SetShaderValue(b.shader, b.loc_sun_sparkle, &light.sun_sparkle,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(b.shader, b.loc_sun_sparkle_sharp, &light.sun_sparkle_sharp,
                   SHADER_UNIFORM_FLOAT);
    const float lit_arm = lit_off ? 0.0f : 1.0f;
    SetShaderValue(b.shader, b.loc_lit, &lit_arm, SHADER_UNIFORM_FLOAT);
    for (Mesh& m : b.meshes) DrawMesh(m, b.mat, xf);
    rlEnableBackfaceCulling();
    glPolygonOffset(0.0f, 0.0f);
    glDisable(GL_POLYGON_OFFSET_FILL);
}

}  // namespace render
