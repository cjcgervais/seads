#include "render/ribbons.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/geometric.hpp>

#include "external/glad.h"  // glPolygonOffset / glEnable (glad decls; impl in raylib)
#include "raymath.h"        // MatrixTranslate
#include "render/ribbon_clip.h"  // T24 excavation clip (pure, test-pinned)
#include "render/ribbon_junction.h"  // ★ ROAD-REPAIR F2: the junction cut (pure)
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
// ★ ROAD-REPAIR AA -- the centreline anti-alias width multiplier.
// 0.0 == the identity BY AN EXPLICIT BRANCH (the old hard ternary,
// verbatim); > 0 scales the screen-space footprint the stripe and the
// dash are box-filtered over. See docs/road_repair/onaping_flash_AA.md.
uniform float uLineAA;
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
    float u = s / period;
    float duty = uDashM / period;
    vec3 c;
    if (uLineAA > 0.0) {
        // ANALYTIC ANTI-ALIAS (ROAD-REPAIR rung AA). The stripe is a ~0.312 m
        // band (|v| < 0.06 of a ~2.6 m half-width) with a step() dash on top
        // and, until this rung, not one derivative anywhere -- while the trail
        // corduroy forty lines up had the fwidth fade all along.
        //
        // ⚠ CORRECTED BY THE RED-TEAM FOLD. This comment used to say the band
        // is sub-pixel "at 70 m and an 8 deg graze". IT IS NOT. At
        // kChaseFovyDeg = 60 and 1080p the scale at screen centre is
        // (H/2)/tan(fovy/2) = 935 px/rad, so 0.312 m crosses ONE pixel only at
        // ~292 m (~322 m on the looser H/fovy = 1031 px/rad convention); at
        // 70 m the stripe is ~4 px wide. And a graze foreshortens the road
        // ALONG its length -- it does not narrow the stripe's TRANSVERSE
        // width at all. The flashing stripe pixels the rig grades are
        // therefore the ones HUNDREDS of metres down the road, where the whole
        // deck is 1-2 px across; that is exactly what the per-pixel traces
        // showed (AA doc 3.2). The defect is real and the fix is the same one;
        // only the range at which it bites was misstated
        // (docs/road_repair/onaping_flash_F3.md 6.4).
        //
        // Both terms below are the EXACT box filter of a rectangular pulse over
        // the pixel's own footprint -- a difference of two clamped ramps, not a
        // smoothstep pair -- so they are energy-preserving by construction: once
        // the feature is narrower than a pixel the coverage falls off as
        // width/footprint and the stripe fades toward the asphalt mean instead
        // of saturating at half contrast. That single expression IS both the
        // "smoothstep edge" and the "contrast fade below 1 px" the rung asks
        // for. uLineAA scales the footprint (the strength dial).
        //
        // ⚠ THE FOOTPRINT IS THE L2 GRADIENT, NOT fwidth (red-team fold).
        // fwidth(x) is |dFdx| + |dFdy|, an L1 sum: for the SAME footprint it
        // reads 1x when the feature's gradient is axis-aligned in screen space
        // and up to sqrt(2)x when it is diagonal -- so an fwidth-driven fade
        // onset moves with the VIEW ORIENTATION. length(vec2(dFdx, dFdy)) is
        // the true gradient magnitude, the rate of change of the coordinate
        // per pixel in its steepest screen direction; it is invariant under
        // screen rotation, which is what "isotropic" means here, and it is the
        // exact box-filter width this coverage expression assumes.
        //
        // The derivatives here sit inside a branch on a UNIFORM, i.e.
        // dynamically uniform control flow -- the same rule the uTrail branch
        // above and the bank FS's lit path already keep.
        float wv = max(length(vec2(dFdx(v), dFdy(v))) * uLineAA, 1.0e-6);
        float cov = clamp((uCenterFrac - abs(v)) / wv + 0.5, 0.0, 1.0)
                  - clamp((-uCenterFrac - abs(v)) / wv + 0.5, 0.0, 1.0);
        // The dash, along the road: the pulse [0, duty) of the unit period,
        // plus its next copy, so a pixel straddling the wrap reads half.
        float wd = max(length(vec2(dFdx(u), dFdy(u))) * uLineAA, 1.0e-6);
        float p = fract(u);
        float dcov = clamp((duty - p) / wd + 0.5, 0.0, 1.0)
                   - clamp((0.0 - p) / wd + 0.5, 0.0, 1.0)
                   + clamp((1.0 + duty - p) / wd + 0.5, 0.0, 1.0)
                   - clamp((1.0 - p) / wd + 0.5, 0.0, 1.0);
        // Past half a period per pixel the dash is unresolvable in principle;
        // hand over to its own duty cycle (the mean) rather than to a ramp.
        dcov = mix(clamp(dcov, 0.0, 1.0), duty, smoothstep(0.25, 0.5, wd));
        c = mix(asphalt, uLine, clamp(cov * dcov, 0.0, 1.0));
    } else {
        // The identity, VERBATIM. `lit` lives HERE and only here: the AA arm
        // above computes its own analytic coverage and never reads it (the
        // red-team's dead-store finding -- it was declared above the branch
        // and used in one arm).
        float lit = 1.0 - step(duty, fract(u));
        c = (abs(v) < uCenterFrac && lit > 0.5) ? uLine : asphalt;
    }
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
                                    const std::vector<RibbonQuadTrim>* trim,
                                    long* verts_out) {
    std::vector<Mesh> out;
    const std::vector<RibbonBatchCPU> batches = build_ribbon_batches(
        P, kept,
        [&hf, subdiv, tiles](const glm::dvec3& d) {
            return drawn_radius_at(hf, d, subdiv, tiles);  // the RENDERED
                                                           // surface
        },
        lift, hf.R, max_seg_m, max_tr_m, trim);
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
    // ★ ROAD-REPAIR F2 -- THE JUNCTION CUT. The plan is built BEFORE any mesh
    // because a junction is a property of the whole network, not of one path:
    // it needs every path's T24-clipped index list at once to know which ways
    // end where. `junction_cut_m <= 0` returns an empty plan by an explicit
    // branch -- nothing is measured, nothing is allocated, and every
    // `trim_for()` below is nullptr, which is the pre-F2 drape vertex for
    // vertex.
    std::vector<std::vector<unsigned short> > kept_all(kSudburyRibbonPathCount);
    for (std::size_t pi = 0; pi < kSudburyRibbonPathCount; ++pi) {
        const GisRibbonPath& P = kSudburyRibbonPaths[pi];
        if (P.vtx_count < 3 || P.idx_count < 3) continue;
        if (P.kind == 3) continue;
        kept_all[pi] = ribbon_indices_outside_cuts(pi, cuts, hf.R);
    }
    const double tj0 = GetTime();
    const JunctionPlan plan =
        build_junction_plan(kept_all, hf.R, look.junction_cut_m,
                            look.max_seg_m, look.max_tr_m);
    const double junction_ms = (GetTime() - tj0) * 1000.0;
    for (std::size_t pi = 0; pi < kSudburyRibbonPathCount; ++pi) {
        const GisRibbonPath& P = kSudburyRibbonPaths[pi];
        if (P.vtx_count < 3 || P.idx_count < 3)
            continue;  // dummy (count 0) path
        if (P.kind == 3)
            continue;  // rivers are MIRROR water — drawn by
                       // render/river_surfaces
        // T24: clip the drape at the excavation cuts (the terrain's own rule).
        const std::vector<unsigned short>& kept = kept_all[pi];
        if (kept.empty()) continue;  // fully swallowed by a cut
        const std::vector<Mesh> built =
            build_path_meshes(P, hf, look.lift_m, subdiv, tiles,
                              look.max_seg_m, look.max_tr_m, kept,
                              plan.trim_for(pi), &r.vert_count);
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
    // ★ F2: THE CAPS. One polygon per cut node, draped by the SAME radius_fn +
    // lift the legs are, with its boundary ring taken verbatim from the trimmed
    // leg ends -- so the seam is a shared position, not a near miss. Pushed as
    // ordinary road meshes (kind 0): they go through the same shader, the same
    // polygon offset and the same draw loop, because a junction cap IS road.
    long cap_verts = 0;
    // ★ F2 SEAM INSTRUMENT. The claim "the cap shares the trimmed leg end's
    // vertices" is a code property (one `ribbon_rung_at`, one `place`), and a
    // code property is exactly the kind of claim that rots. So it is MEASURED
    // on the mesh that ships: every leg vertex within 40 m of a cut node goes
    // into a 2 m spatial hash, and every cap BOUNDARY vertex is then asked for
    // its distance to the nearest one. A seam that is shared reads 0.000 m; a
    // seam that has drifted reads the crack, in metres, before Chad does.
    double seam_max_gap_m = 0.0;
    long seam_checked = 0, seam_exact = 0;
    if (!plan.caps.empty()) {
        const double cell = 2.0;
        std::vector<glm::dvec3> nodes_xyz;
        nodes_xyz.reserve(plan.caps.size());
        for (const JunctionCapCPU& c : plan.caps)
            nodes_xyz.push_back(
                c.centre * (drawn_radius_at(hf, c.centre, subdiv, tiles) +
                            look.lift_m));
        std::unordered_map<long long, std::vector<int> > ngrid;
        auto key3 = [&](double x, double y, double z, double cs) {
            const long long i = static_cast<long long>(std::floor(x / cs));
            const long long j = static_cast<long long>(std::floor(y / cs));
            const long long k = static_cast<long long>(std::floor(z / cs));
            return (i * 73856093LL) ^ (j * 19349663LL) ^ (k * 83492791LL);
        };
        for (int i = 0; i < static_cast<int>(nodes_xyz.size()); ++i)
            ngrid[key3(nodes_xyz[i].x, nodes_xyz[i].y, nodes_xyz[i].z, 64.0)]
                .push_back(i);
        std::unordered_map<long long, std::vector<glm::vec3> > vgrid;
        for (const Mesh& m : r.meshes)
            for (int v = 0; v < m.vertexCount; ++v) {
                const glm::vec3 p(m.vertices[v * 3], m.vertices[v * 3 + 1],
                                  m.vertices[v * 3 + 2]);
                bool near = false;
                for (int dx = -1; dx <= 1 && !near; ++dx)
                    for (int dy = -1; dy <= 1 && !near; ++dy)
                        for (int dz = -1; dz <= 1 && !near; ++dz) {
                            std::unordered_map<
                                long long, std::vector<int> >::const_iterator
                                it = ngrid.find(key3(p.x + dx * 64.0,
                                                     p.y + dy * 64.0,
                                                     p.z + dz * 64.0, 64.0));
                            if (it == ngrid.end()) continue;
                            for (int ci : it->second)
                                if (glm::length(glm::dvec3(p) - nodes_xyz[ci]) <
                                    40.0) {
                                    near = true;
                                    break;
                                }
                        }
                if (near) vgrid[key3(p.x, p.y, p.z, cell)].push_back(p);
            }
        for (const JunctionCapCPU& c : plan.caps)
            for (std::size_t i = 0; i < c.ring.size(); ++i) {
                // ONLY the leg columns are a seam. The corner-span points
                // between two legs belong to no leg -- grading them would
                // measure the WIDTH of the intersection and call it a crack.
                if (i >= c.ring_on_leg.size() || !c.ring_on_leg[i]) continue;
                const double rr = drawn_radius_at(hf, c.ring[i], subdiv, tiles) +
                                  look.lift_m;
                const glm::vec3 p(static_cast<float>(c.ring[i].x * rr),
                                  static_cast<float>(c.ring[i].y * rr),
                                  static_cast<float>(c.ring[i].z * rr));
                double best = 1.0e30;
                for (int dx = -1; dx <= 1; ++dx)
                    for (int dy = -1; dy <= 1; ++dy)
                        for (int dz = -1; dz <= 1; ++dz) {
                            std::unordered_map<
                                long long,
                                std::vector<glm::vec3> >::const_iterator it =
                                vgrid.find(key3(p.x + dx * cell, p.y + dy * cell,
                                                p.z + dz * cell, cell));
                            if (it == vgrid.end()) continue;
                            for (const glm::vec3& q : it->second)
                                best = std::min(
                                    best, static_cast<double>(glm::length(q - p)));
                        }
                ++seam_checked;
                if (best <= 1.0e29) {
                    if (best == 0.0) ++seam_exact;
                    seam_max_gap_m = std::max(seam_max_gap_m, best);
                }
            }
    }
    double cap_sag_max_m = 0.0, cap_sag_valley_max_m = 0.0;
    long cap_sag_tris = 0, cap_sag_gt10 = 0, cap_sag_valley_tris = 0,
         cap_sag_valley_gt10 = 0;
    if (!plan.caps.empty()) {
        std::vector<std::vector<int> > vert_cap;
        const std::vector<RibbonBatchCPU> cb = build_junction_cap_batches(
            plan.caps,
            [&hf, subdiv, tiles](const glm::dvec3& d) {
                return drawn_radius_at(hf, d, subdiv, tiles);
            },
            look.lift_m, &vert_cap);
        // ★ F2 SAG RE-CHECK -- the STOP condition of this rung. Rung 8 exists
        // because a 53 m flat chord floated up to +7.224 m above the ground the
        // machine stands on ("I went into the road"), and a junction cap is by
        // nature a BIGGER flat polygon than a subdivided road quad -- exactly
        // the shape that re-opens that sink. So the cap's own triangles are
        // measured the way the SEADS_RIBBON_SAG ruler measures a chord: the
        // centroid and the three edge midpoints of every cap triangle against
        // drawn_radius_at + lift, the surface the machine drives.
        // ⚠ Valley is kPumpValleySurface (world/faction_bubbles.h), the same
        // anchor onaping_sink.md's 2.5 km slice uses.
        const glm::dvec3 valley(-0.9174656105010327, 0.3714782345902696,
                                0.14234034836849382);
        for (std::size_t bi = 0; bi < cb.size(); ++bi) {
            const RibbonBatchCPU& b = cb[bi];
            for (std::size_t k = 0; k + 2 < b.idx.size(); k += 3) {
                const unsigned short i0 = b.idx[k], i1 = b.idx[k + 1],
                                     i2 = b.idx[k + 2];
                const glm::dvec3 A(b.pos[i0 * 3], b.pos[i0 * 3 + 1],
                                   b.pos[i0 * 3 + 2]);
                const glm::dvec3 B(b.pos[i1 * 3], b.pos[i1 * 3 + 1],
                                   b.pos[i1 * 3 + 2]);
                const glm::dvec3 C(b.pos[i2 * 3], b.pos[i2 * 3 + 1],
                                   b.pos[i2 * 3 + 2]);
                const glm::dvec3 smp[4] = {(A + B + C) / 3.0, 0.5 * (A + B),
                                           0.5 * (B + C), 0.5 * (C + A)};
                double worst = 0.0;
                for (int q = 0; q < 4; ++q) {
                    const double rr = glm::length(smp[q]);
                    if (rr < 1.0) continue;
                    const glm::dvec3 d = smp[q] / rr;
                    worst = std::max(rr - (drawn_radius_at(hf, d, subdiv,
                                                           tiles) +
                                           look.lift_m),
                                     worst);
                }
                ++cap_sag_tris;
                cap_sag_max_m = std::max(cap_sag_max_m, worst);
                if (worst > 0.10) ++cap_sag_gt10;
                const int ci =
                    (bi < vert_cap.size() && i0 < vert_cap[bi].size())
                        ? vert_cap[bi][i0]
                        : -1;
                if (ci >= 0 &&
                    hf.R * std::acos(std::min(
                               1.0, std::max(-1.0,
                                             glm::dot(plan.caps[ci].centre,
                                                      valley)))) <= 2500.0) {
                    ++cap_sag_valley_tris;
                    cap_sag_valley_max_m =
                        std::max(cap_sag_valley_max_m, worst);
                    if (worst > 0.10) ++cap_sag_valley_gt10;
                }
            }
        }
        for (const RibbonBatchCPU& b : cb) {
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
            r.vert_count += m.vertexCount;
            cap_verts += m.vertexCount;
            UploadMesh(&m, false);
            r.meshes.push_back(m);
            r.kinds.push_back(0);      // a junction cap is ROAD
            r.half_w_m.push_back(6.0f);  // unused: the cap is |v| == 1, so the
                                         // corduroy/dash never reads it
        }
    }
    if (look.junction_cut_m > 0.0) {
        TraceLog(LOG_INFO,
                 "RIBBONS F2 junction cut: dial %.2f m, radius %.2f-%.2f m; "
                 "%d ways / %d endpoints -> %d nodes (deg1 %d, deg2 %d, "
                 "deg3+ %d)",
                 look.junction_cut_m, plan.cut_radius_min_m,
                 plan.cut_radius_max_m, plan.ways, plan.endpoints,
                 plan.nodes_total, plan.nodes_deg1, plan.nodes_deg2,
                 plan.nodes_deg3plus);
        TraceLog(LOG_INFO,
                 "RIBBONS F2 cut: %d nodes, %d legs (%d clamped by the 40%% "
                 "length rule), %d quads dropped; SKIPPED %d trail, %d short; "
                 "plan %.0f ms",
                 plan.nodes_cut, plan.legs_cut, plan.legs_clamped,
                 plan.quads_dropped, plan.nodes_skipped_trail,
                 plan.nodes_skipped_short, junction_ms);
        TraceLog(LOG_INFO,
                 "RIBBONS F2 caps: %zu caps, %ld verts, area %.0f m2 (%.1f %% "
                 "outside every leg corridor), ~%.0f m of deck removed",
                 plan.caps.size(), cap_verts, plan.cap_area_m2,
                 plan.cap_area_m2 > 0.0
                     ? 100.0 * plan.cap_area_uncovered_m2 / plan.cap_area_m2
                     : 0.0,
                 plan.road_len_removed_m);
        TraceLog(LOG_INFO,
                 "RIBBONS F2 SEAM: %ld cap boundary verts checked, %ld matched "
                 "a leg vertex EXACTLY, max gap %.6f m",
                 seam_checked, seam_exact, seam_max_gap_m);
        TraceLog(LOG_INFO,
                 "RIBBONS F2 CAP SAG (vs drawn_radius_at + lift, the surface "
                 "the machine drives): %ld tris, %ld over 0.10 m (%.2f %%), "
                 "worst +%.3f m; within 2.5 km of Valley %ld tris, %ld over "
                 "0.10 m, worst +%.3f m",
                 cap_sag_tris, cap_sag_gt10,
                 cap_sag_tris > 0 ? 100.0 * static_cast<double>(cap_sag_gt10) /
                                        static_cast<double>(cap_sag_tris)
                                  : 0.0,
                 cap_sag_max_m, cap_sag_valley_tris, cap_sag_valley_gt10,
                 cap_sag_valley_max_m);
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
    r.loc_line_aa = GetShaderLocation(r.shader, "uLineAA");
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
             "max_tr_m %.2f, over_bank_bias %.2f, built in %.0f ms "
             "(roads + trails)",
             r.meshes.size(), r.vert_count, look.max_seg_m, look.max_tr_m,
             look.over_bank_bias,
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
    // ★ ROAD-REPAIR E2 -- THE FLASH INSTRUMENT'S POSITIVE CONTROL, and
    // nothing else. The bank pass below uses the IDENTICAL (-2, -4), which is
    // exactly why bank-over-deck has no arbitration at all; a flash grader
    // that cannot MOVE this pair cannot prove it is reading depth arbitration
    // rather than some other per-frame difference. SEADS_FLASH_POSCTL scales
    // THIS pass's offset only, is read ONCE, and DEFAULTS TO 1.0 -- the
    // shipped numbers, bit-identical, off.
    static const float po_scale = [] {
        const char* e = std::getenv("SEADS_FLASH_POSCTL");
        if (e == nullptr || e[0] == '\0') return 1.0f;
        return static_cast<float>(std::atof(e));
    }();
    // ★ ROAD-REPAIR F3 -- THE OVER-BANK BIAS. [ribbons] over_bank_bias
    // scales THIS pass's pair by (1 + b) while draw_bank_strips below keeps
    // (-2, -4), which is the only relative depth bias the deck-versus-bank tie
    // has ever had. It COMPOSES with the E2 positive control: the deck draws
    // at po_scale * (1 + b). 0.0 == the identity BY BRANCH -- the else arm is
    // the shipped call, verbatim, not a multiply by 1.0.
    const double over_bank_bias = r.look.over_bank_bias;
    glEnable(GL_POLYGON_OFFSET_FILL);
    if (over_bank_bias > 0.0) {
        const float b = static_cast<float>(1.0 + over_bank_bias);
        glPolygonOffset(-2.0f * po_scale * b, -4.0f * po_scale * b);
    } else {
        glPolygonOffset(-2.0f * po_scale, -4.0f * po_scale);
    }
    rlDisableBackfaceCulling();  // a road on a side-slope reads from either
                                 // side

    const Matrix xf =
        MatrixTranslate(static_cast<float>(-eye.x), static_cast<float>(-eye.y),
                        static_cast<float>(-eye.z));
    const RibbonLook& L = r.look;
    // ★ ROAD-REPAIR F3 §6 -- THE ATTRIBUTION ARM. The ribbon pass draws
    // ROADS (kind 0/1) and snowmobile TRAILS (kind 2) as separate draped
    // strips in ONE pass with ONE polygon offset, so a trail crossing a road
    // is a deck-on-deck tie no per-pass offset can ever separate.
    // SEADS_NO_TRAILS=1 skips the kind-2 meshes so that share can be MEASURED
    // instead of argued. Read once; unarmed, not one branch moves.
    static const bool no_trails = std::getenv("SEADS_NO_TRAILS") != nullptr;
    for (std::size_t i = 0; i < r.meshes.size(); ++i) {
        if (no_trails && r.kinds[i] == 2) continue;
        const float uTrail =
            r.kinds[i] == 2 ? 1.0f : 0.0f;  // per-mesh kind selector
        SetShaderValue(r.shader, r.loc_trail, &uTrail, SHADER_UNIFORM_FLOAT);
        SetShaderValue(r.shader, r.loc_bed, &L.road_bed, SHADER_UNIFORM_VEC3);
        SetShaderValue(r.shader, r.loc_line, &L.road_line, SHADER_UNIFORM_VEC3);
        // ★ ROAD-REPAIR F3 §6 -- THE WHITE-LINE ARM (Chad, 2026-09-16, flying
        // Chelmsford: "there were a few flashing spots in the road, the white
        // line"). The dashed centreline is NOT a second coplanar pass and NOT
        // its own geometry: it is one ternary in the ribbon FS on the deck's
        // own interpolated v/s, so it shares the deck's single polygon offset
        // BY CONSTRUCTION and cannot z-fight its own deck. SEADS_NO_LINE=1
        // zeroes uCenterFrac, which paints the deck plain asphalt -- so the
        // pixels that stop flashing are the ones whose flash was the LINE's
        // contrast being re-decided against whatever else is drawn there.
        // Read once; unarmed, the shipped value verbatim.
        static const bool no_line = std::getenv("SEADS_NO_LINE") != nullptr;
        const float center_frac = no_line ? 0.0f : L.road_center_frac;
        SetShaderValue(r.shader, r.loc_center, &center_frac,
                       SHADER_UNIFORM_FLOAT);
        // ★ ROAD-REPAIR rung AA -- THE CENTRELINE ANTI-ALIAS, and its
        // kill, read HERE beside SEADS_NO_LINE so the arm that MEASURED the
        // line's share and the fix for it are one switch in one place.
        // ⚠ SEMANTICS, UNIFIED BY THE RED-TEAM FOLD: this env REPLACES the
        // config value, exactly like SEADS_OVER_BANK_BIAS does in
        // app/main.cpp -- the env value IS the dial value, `=0` is the kill,
        // and it is clamped to the loader's own [0, 4] so the seat can never
        // ask for a value the config table would reject. It used to MULTIPLY
        // the shipped 1.0, which read the same for the kill and for 2.0 but
        // silently differed the moment the shipped value moved off 1.0.
        // Read once.
        static const float line_aa_env = [] {
            const char* e = std::getenv("SEADS_LINE_AA");
            if (e == nullptr || e[0] == '\0') return -1.0f;  // unset
            const double v = std::atof(e);
            return static_cast<float>(v < 0.0 ? 0.0 : (v > 4.0 ? 4.0 : v));
        }();
        const float line_aa = line_aa_env >= 0.0f ? line_aa_env : L.line_aa;
        SetShaderValue(r.shader, r.loc_line_aa, &line_aa,
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
// ★ ROAD-REPAIR AA -- the speckle anti-alias width multiplier.
// 0.0 == the identity BY AN EXPLICIT BRANCH (the pre-AA fade arithmetic,
// verbatim); > 0 scales the grain footprint the Nyquist gate is taken on.
uniform float uSpeckleAA;
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
    // ★ ROAD-REPAIR rung AA -- THE GRAIN-PERIOD GATE. The fade above
    // is a SOFT rational roll-off; at the flash rig's 70 m / 6-11 deg graze it
    // still passes ~30-40 % of the BINARY speckle through, and that residue is
    // 13.5-16.4 pp of the road-mask flicker Chad reported -- about 86 % of it
    // (docs/road_repair/onaping_flash_F3.md 6.3).
    //
    // ⚠ THE GATE TAKES ITS OWN, ISOTROPIC FOOTPRINT (red-team fold). The `w`
    // above is fwidth(grain.x) + fwidth(grain.y) -- an L1 sum of two L1 sums,
    // which over-reads the true footprint by up to 2x per axis and up to ~4x
    // compounded, and by an amount that depends on how the bank happens to be
    // ORIENTED on screen. It is left EXACTLY as it was because it is the
    // pre-AA fade, i.e. the identity at dial 0, and the identity is verbatim.
    // The gate below instead uses the FROBENIUS NORM of the (grain <- pixel)
    // Jacobian, sqrt(|d(grain)/dx|^2 + |d(grain)/dy|^2). That is invariant
    // under a rotation of the screen axes (the Jacobian is right-multiplied by
    // the rotation and the Frobenius norm is unchanged), which is what makes
    // it isotropic, and it equals sqrt(s1^2 + s2^2) of the singular values --
    // so it never UNDER-reads the worst-case grain-space step per pixel, s1.
    // max(fwidth(grain.x), fwidth(grain.y)) was the other candidate and is
    // NOT isotropic: both terms are still L1 norms, so it carries the same
    // orientation-dependent up-to-sqrt(2) swing.
    //
    // wi is GRAINS PER PIXEL, so wi = 1 is the sampling limit: past it the field
    // carries no recoverable signal, only a hash of the eye position. Gate the
    // binary term off over wi in [0.5, 1.0] -- a grain period of 2 px down to
    // 1 px -- and `cover` lands on `dens`, the EXACT mean coverage: the same
    // energy-preserving destination the old fade aimed at, reached instead of
    // merely approached.
    //
    // ⚠ THE ONSET IS THE LOOK/FLICKER TRADE, AND IT IS MEASURED. `sp` is a
    // step() on a per-cell hash, so every cell BOUNDARY is a hard
    // discontinuity carrying the full uDark contrast and no edge filter exists
    // for a random cell field -- which means the oreo still shimmers WELL
    // below Nyquist. Swept at J2 (road-mask flash; 18.53 % unarmed, 4.78 %
    // with the speckle removed outright):
    //
    //     speckle_aa 1.0  gate 1-2 px period  ->  10.11 %   <-- SHIPPED
    //     speckle_aa 2.0  gate 2-4 px         ->   6.62 %
    //     speckle_aa 3.0  gate 3-6 px         ->   5.11 %   (at the floor)
    //
    // The shipped 1.0 is the value that leaves the LOOK alone: at 3.0 the near
    // A/B (8 m, 12 deg graze) erases the oreo crumb from ~7 m outward and the
    // mid-field banks read as plain white. Chad signed that crumb, and a dial
    // that buys 5 pp by deleting it is a design change wearing an AA costume,
    // so it is offered as a DIAL and not taken (AA doc 5 + 8). Raising this
    // past ~1.5 is HIS ruling to make, not the grader's.
    if (uSpeckleAA > 0.0) {
        vec2 gdx = dFdx(grain);
        vec2 gdy = dFdy(grain);
        float wi = sqrt(dot(gdx, gdx) + dot(gdy, gdy));
        fade *= 1.0 - smoothstep(0.5, 1.0, wi * uSpeckleAA);
    }
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
    // ★ ROAD-REPAIR F1 -- WHAT THE YIELD COST, said out loud on every armed
    // build. A bank that vanished planet-wide and a bank that yielded only at
    // the junctions look identical from the seat at ONE intersection; this
    // line is the difference between them, and it is the number the doc's
    // over-reach check quotes. bank_mesh.cpp cannot print it (pure TU, zero
    // raylib), so its caller does.
    if (p.deck_yield_m > 0.0) {
        const BankDeckYieldStat ys = bank_deck_yield_stat();
        TraceLog(LOG_INFO,
                 "ROAD-REPAIR F1 deck_yield_m %.2f: %lld of %lld bank stations "
                 "yielded (%.2f %%); deck penetration p50 %.2f p90 %.2f p99 "
                 "%.2f max %.2f m",
                 p.deck_yield_m, ys.capped, ys.stations,
                 ys.stations > 0 ? 100.0 * static_cast<double>(ys.capped) /
                                       static_cast<double>(ys.stations)
                                 : 0.0,
                 ys.pen_p50, ys.pen_p90, ys.pen_p99, ys.pen_max);
        // The sweep, so the dial can be re-chosen from any armed run without
        // a second build (see bank_deck_yield_fraction_over).
        const double sweep[14] = {0.05, 0.10, 0.15, 0.20, 0.25, 0.30, 0.40,
                                  0.50, 0.75, 1.00, 1.50, 2.00, 3.00, 5.00};
        for (const double t : sweep)
            TraceLog(LOG_INFO,
                     "ROAD-REPAIR F1 sweep: deck_yield_m %.2f would yield "
                     "%.2f %% of bank stations",
                     t, 100.0 * bank_deck_yield_fraction_over(t));
    }
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
    b.loc_speckle_aa = GetShaderLocation(b.shader, "uSpeckleAA");
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
    // ★ ROAD-REPAIR F3 §6 -- THE BANK-OFFSET ARM, the SEADS_FLASH_POSCTL
    // pattern exactly: read ONCE, DEFAULTS TO 1.0, scales THIS pass's pair and
    // nothing else. It exists because the bank carries a 6 m BURIAL SKIRT that
    // descends to skirt_bury_m BELOW the drawn terrain facet, and this
    // negative offset pulls that buried band back toward the eye while the
    // terrain pass has no offset at all. SEADS_BANK_POSCTL=0 lets the terrain
    // win wherever the skirt is buried, which is how that tie gets MEASURED
    // instead of argued. Instrument only -- unarmed, the shipped call.
    static const float bank_po_scale = [] {
        const char* e = std::getenv("SEADS_BANK_POSCTL");
        if (e == nullptr || e[0] == '\0') return 1.0f;
        return static_cast<float>(std::atof(e));
    }();
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-2.0f * bank_po_scale, -4.0f * bank_po_scale);
    rlDisableBackfaceCulling();
    const Matrix xf =
        MatrixTranslate(static_cast<float>(-eye.x), static_cast<float>(-eye.y),
                        static_cast<float>(-eye.z));
    SetShaderValue(b.shader, b.loc_base, &b.look.base, SHADER_UNIFORM_VEC3);
    SetShaderValue(b.shader, b.loc_density, &b.look.speckle_density,
                   SHADER_UNIFORM_FLOAT);
    // ★ ROAD-REPAIR F3 §6 -- THE GRAVEL-SPECKLE ARM. The oreo albedo is a
    // deterministic hash of ~22 cm grains in surface metres. At 70 m and a 6-11
    // degree graze one grain is far under one pixel, so which grain a pixel
    // samples is decided by where the eye is -- and 2 cm of eye travel
    // re-samples the whole field. SEADS_BANK_SPECKLE=0 zeroes the speckle
    // CONTRAST (uDark), leaving the same geometry, the same lighting and the
    // same sparkle, so the aliasing hypothesis can be MEASURED against the
    // depth one. Read once; unarmed, the shipped value verbatim.
    static const float speckle_dark_scale = [] {
        const char* e = std::getenv("SEADS_BANK_SPECKLE");
        if (e == nullptr || e[0] == '\0') return 1.0f;
        return static_cast<float>(std::atof(e));
    }();
    const float speckle_dark = b.look.speckle_dark * speckle_dark_scale;
    SetShaderValue(b.shader, b.loc_dark, &speckle_dark,
                   SHADER_UNIFORM_FLOAT);
    // ★ ROAD-REPAIR rung AA -- THE SPECKLE NYQUIST GATE, and its kill,
    // read HERE beside SEADS_BANK_SPECKLE so the arm that MEASURED the
    // speckle's 13.5-16.4 pp and the fix for it are one switch in one place.
    // ⚠ SEMANTICS, UNIFIED BY THE RED-TEAM FOLD: this env REPLACES the config
    // value, exactly like SEADS_OVER_BANK_BIAS does in app/main.cpp -- the env
    // value IS the dial value, `=0` is the kill, clamped to the loader's own
    // [0, 4]. It used to MULTIPLY the shipped 1.0. Read once.
    static const float speckle_aa_env = [] {
        const char* e = std::getenv("SEADS_SPECKLE_AA");
        if (e == nullptr || e[0] == '\0') return -1.0f;  // unset
        const double v = std::atof(e);
        return static_cast<float>(v < 0.0 ? 0.0 : (v > 4.0 ? 4.0 : v));
    }();
    const float speckle_aa =
        speckle_aa_env >= 0.0f ? speckle_aa_env : b.look.speckle_aa;
    SetShaderValue(b.shader, b.loc_speckle_aa, &speckle_aa,
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
