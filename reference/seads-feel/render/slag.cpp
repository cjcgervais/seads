#include "render/slag.h"

#include <cmath>
#include <cstdio>
#include <glm/glm.hpp>
#include <string>
#include <vector>

#include "render/copper_cliff_geo.h"  // shared dump anchor / crest axis / pour-face aim
#include "render/team_color.h"  // kSlagOrange: the heatColor() hot stop, single-sourced
#include "rlgl.h"

namespace render {

namespace {

// Toe sink: the ridge/lava toes drop this far BELOW the base terrain so there
// is no seam where the face meets the ground (single source for the mesh +
// lava, Fable P2-2).
constexpr float kToeSink = 2.0f;

// Mono flat-shaded facet material (dark slag ridge + steel pot) — the same
// dFdx-facet pattern as the buildings/train, mono so the S1 post silvers it.
const char* kMonoVS = R"GLSL(#version 330
in vec3 vertexPosition;
uniform mat4 mvp;
uniform mat4 matModel;
out vec3 vPos;
void main() {
    vPos = (matModel * vec4(vertexPosition, 1.0)).xyz;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)GLSL";

const char* kMonoFS = R"GLSL(#version 330
in vec3 vPos;
uniform vec3 uSunDir;
uniform vec3 uColor;
uniform float uAmbient;
uniform float uDiffuse;
out vec4 finalColor;
void main() {
    vec3 N = cross(dFdx(vPos), dFdy(vPos));
    float nl2 = dot(N, N);
    if (nl2 < 1.0e-12) { finalColor = vec4(uColor * uAmbient, 1.0); return; }
    N *= inversesqrt(nl2);
    if (dot(N, normalize(-vPos)) < 0.0) N = -N;
    float lit = uAmbient + uDiffuse * max(dot(N, -uSunDir), 0.0);
    finalColor = vec4(clamp(uColor * lit, 0.0, 1.0), 1.0);
}
)GLSL";

// The LAVA cascade — the sanctioned WARM-CHROMA exception. Emissive
// hot->cooling- red->crust from a SINGLE-SOURCED heat() (Fable: no fork with
// any limb/scatter). SINGLE CLOCK (Fable CC3 P0): everything derives from the
// pour phase p; a front advances down the face (p_arr per downslope), then each
// parcel COOLS after the front passes, reaching crust by p->1 which equals the
// p=0 state (wrap-safe, no pop). OPAQUE (Fable P1: dst replaced -> the orange
// survives day AND night, gate- robust); NIGHT-BRIGHTEST via a sun-elevation
// boost. The high chroma (C=max-min large) passes the S1 split-tone sat gate
// unsilvered, like the aircraft.
const char* kLavaVS = R"GLSL(#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;      // .x = downslope fraction d (0 rim -> 1 base)
uniform mat4 mvp;
uniform mat4 matModel;
out vec3 vPos;
out float vD;
void main() {
    vPos = (matModel * vec4(vertexPosition, 1.0)).xyz;
    vD = vertexTexCoord.x;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)GLSL";

// NOTE: the lava FS body deliberately carries NO `#version` line and no literal
// for the molten `hot` stop. lava_fragment_source() below prepends the version
// directive plus a `#define SLAG_HOT vec3(...)` built from render::kSlagOrange,
// so the molten slag and the ENEMY TEAM COLOUR are the same number by
// construction (CLAUDE.md H1: single-source or fork). Chad's ruling 2026-08-09:
// "make the enemies the slag orange color we use across this codebase".
const char* kLavaFSBody = R"GLSL(
in vec3 vPos;
in float vD;
uniform float uPhase;        // pour phase [0,1)
uniform float uGlow;         // emissive gain
uniform float uNight;        // extra emissive when the sun is down (night_boost)
uniform vec3 uSunDir;        // world light-travel dir (night gate)
uniform vec3 uEye;           // world eye (to reconstruct up = normalize(worldPos))
out vec4 finalColor;
// SINGLE SOURCE of the molten->crust ramp (h in [0,1]: 1 hot, 0 crust).
vec3 heatColor(float h) {
    vec3 crust = vec3(0.05, 0.02, 0.02);
    vec3 red   = vec3(0.72, 0.10, 0.02);
    vec3 hot   = SLAG_HOT;   // injected from render::kSlagOrange
    return h < 0.5 ? mix(crust, red, h * 2.0) : mix(red, hot, (h - 0.5) * 2.0);
}
void main() {
    float p = uPhase;
    // SINGLE CLOCK, retimed for a SLOW decelerating OOZE (Chad 2026-07-14: the old
    // front raced the whole face in ~1 s). The front advances down a CONVEX map
    // pow(vD,1.6) -> fast off the lip, crawling at the toe (real slag thickens/cools
    // as it spreads), and now occupies phase [0.06,0.72] = 0.66 of the cycle (was
    // 0.35) so the descent takes ~2x longer for a fixed cadence. A brief fed plateau
    // then an exp cool to crust. Fable-BEFORE verified WRAP-SAFE: latest extinction
    // p = 0.78 + 0.055*ln(1/0.03) = 0.973 < 1, and every vD is discarded at p=0
    // (front not arrived) -> the face is FULLY dark on (0.973,1) u [0,0.06), no pop.
    float front = pow(vD, 1.6);
    float p_arr = 0.06 + front * 0.66;         // front: rim 0.06 -> base 0.72
    float p_tail = p_arr + 0.06;               // brief fed plateau after arrival
    if (p < p_arr) discard;                    // front not here yet -> the ridge shows
    float heat = exp(-max(0.0, p - p_tail) / 0.055);  // fed plateau, then cools
    if (heat < 0.03) discard;                 // cooled to crust -> the ridge shows
    vec3 up = normalize(vPos + uEye);
    float night = 1.0 - smoothstep(-0.14, 0.10, dot(-uSunDir, up));  // match the flown lamp dusk gate
    vec3 col = heatColor(heat) * uGlow * (1.0 + night * uNight);
    finalColor = vec4(col, 1.0);              // OPAQUE emissive (chroma survives)
}
)GLSL";

// Assemble the lava fragment source: version directive + a SLAG_HOT define
// carrying team_colors().enemy + the body above. This is the ONE place the
// molten colour becomes a number, and it is literally the same number the
// ENEMY FACTION wears everywhere else (map icons, aircraft livery, BANDIT
// tags) — config/world.toml [teams] enemy is the single source, pushed by
// app/main.cpp's set_team_colors() before any draw. %.6f keeps the GLSL
// literal exact for the shipped 2-decimal constants.
std::string lava_fragment_source() {
    const glm::dvec3 hot = team_colors().enemy;
    char def[128];
    std::snprintf(def, sizeof def,
                  "#version 330\n#define SLAG_HOT vec3(%.6f, %.6f, %.6f)\n",
                  hot.x, hot.y, hot.z);
    return std::string(def) + kLavaFSBody;
}

// Continuous crest-height helpers (Fable F3/F9: the ridge mesh, the lava, AND
// the track crest-drape ALL sample the SAME h(z)/y0(z) — a constant lift would
// fork). Height profile ALONG the crest (Chad 2026-07-13): a LONG gradual ramp
// up to the crest at each end (the grade the loaded train climbs), a FLAT crest
// in the middle (the track runs lengthwise + the pots pour), then a long
// shallow descent. az<=crest_flat_half = full crest; beyond it, smoothstep down
// to 0 at the ridge end over the long ramp run — so a slag train can actually
// climb onto the dump (not the old 90 m near-vertical taper).
float ridge_endtaper(const SlagRidge& g, float z) {
    const float az = std::fabs(z);
    if (az <= g.crest_flat_half) return 1.0f;
    const float ramp =
        g.half_len - g.crest_flat_half;  // the long ramp/descent run
    if (ramp < 1.0f) return 0.0f;
    float e = (g.half_len - az) / ramp;  // 1 at the flat edge -> 0 at the end
    e = e < 0.0f ? 0.0f : (e > 1.0f ? 1.0f : e);
    return e * e * (3.0f - 2.0f * e);  // smoothstep (gentle S-curve grade)
}
float ridge_noise(
    float z) {  // gentle continuous terrace/lift variation (no clock)
    return std::sin(z * 0.045f) * 0.10f + std::sin(z * 0.011f + 1.7f) * 0.06f;
}
// Flat local frame -> sphere+terrain base offset (Fable F3: z^2/2Ra curvature +
// the terrain radius drift over the 700 m ridge; else the ends float ~4 m).
double ridge_y0(const SlagRidge& g, float z) {
    const double zz = z;
    const glm::dvec3 dir = glm::normalize(g.anchor * std::cos(zz / g.Ra) +
                                          g.axis * std::sin(zz / g.Ra));
    return (g.hf->radius_at(dir) - g.Ra) - (zz * zz) / (2.0 * g.Ra);
}

// CC6 — build the SINGLE-SOURCE benched pour-face profile into the ridge. B
// benches = B steep risers + (B-1) flat-ish treads, ending at (1,1). run/drop
// are split by fixed shares so the MEAN slope stays 35deg (endpoints unchanged)
// while risers read as terraces. Riser slope =
// atan((drop_share/run_share)*tan35) ~= 47deg (< the 55deg cap, Fable) and is
// INDEPENDENT of B. B=1 -> the legacy single planar face.
void build_face_profile(SlagRidge& g, int benches) {
    const float run_share =
        0.55f;  // fraction of the total run in the (steep) risers
    const float drop_share = 0.85f;  // fraction of the total drop in the risers
    int B = benches < 1 ? 1 : (benches > 5 ? 5 : benches);
    if (B == 1) {
        g.face_nk = 2;
        g.face_xf[0] = 0.0f;
        g.face_xf[1] = 1.0f;
        g.face_yf[0] = 0.0f;
        g.face_yf[1] = 1.0f;
        return;
    }
    const float rr = run_share / B, tr = (1.0f - run_share) / (B - 1);
    const float rd = drop_share / B, td = (1.0f - drop_share) / (B - 1);
    int k = 0;
    float x = 0.0f, y = 0.0f;
    g.face_xf[k] = 0.0f;
    g.face_yf[k] = 0.0f;
    ++k;
    for (int i = 0; i < B; ++i) {
        x += rr;
        y += rd;
        g.face_xf[k] = x;
        g.face_yf[k] = y;
        ++k;  // riser
        if (i < B - 1) {
            x += tr;
            y += td;
            g.face_xf[k] = x;
            g.face_yf[k] = y;
            ++k;
        }  // tread
    }
    g.face_nk = k;  // = 2B
    g.face_xf[k - 1] = 1.0f;
    g.face_yf[k - 1] = 1.0f;  // pin the toe exactly
}

// Lobed truncated ridge: extrude a cross-section (back toe / back crest / front
// crest / front toe) along Z=axis. The FRONT (+X=face) side is the steep pour
// face at the angle of repose (front-toe run = face_run*h/height -> slope
// tan(angle) for ALL h, Fable-confirmed); the crest strip [-crest_hw,+crest_hw]
// at y0+h carries the track; the ends taper h->0 (no vertical end-wall).
// Culling off + eye-flip FS -> winding free.
Mesh build_ridge_mesh(const SlagLook& L, const SlagRidge& g) {
    (void)L;
    const int K =
        60;  // more stations for the longer ridge + the smooth ramp grade
    const int nk = g.face_nk;  // front-face knots (crest edge .. toe)
    const int vps =
        2 + nk;  // verts per station: back-toe, back-crest, front[0..nk-1]
    std::vector<float> v;
    std::vector<unsigned short> idx;
    auto push = [&](float x, float y, float z) {
        v.push_back(x);
        v.push_back(y);
        v.push_back(z);
    };
    for (int s = 0; s <= K; ++s) {
        const float z =
            -g.half_len + 2.0f * g.half_len * static_cast<float>(s) / K;
        const float h = ridge_crest_h(g, z);
        const float y0 = static_cast<float>(ridge_y0(g, z));
        const float frac = h / g.height;  // taper the runs with the height
        const float frun = g.face_run * frac;
        const float brun = g.back_run * frac;
        const float sink = kToeSink;  // sink toes into terrain (no gap)
        push(-(g.crest_hw + brun), y0 - sink, z);  // 0 back toe
        push(-g.crest_hw, y0 + h, z);              // 1 back crest
        // front face: crest edge (k=0) down the benched profile to the toe
        // (k=nk-1), sampling the SHARED knots (mesh vertical scale = h+sink,
        // Fable P1-2).
        for (int k = 0; k < nk; ++k)
            push(g.crest_hw + frun * g.face_xf[k],
                 (y0 + h) - (h + sink) * g.face_yf[k], z);
    }
    for (int s = 0; s < K; ++s) {
        const unsigned short a = static_cast<unsigned short>(s * vps);
        const unsigned short b = static_cast<unsigned short>((s + 1) * vps);
        for (int e = 0; e < vps - 1;
             ++e) {  // back face, crest top, then each bench segment
            const unsigned short a0 = a + e, a1 = a + e + 1;
            const unsigned short b0 = b + e, b1 = b + e + 1;
            idx.push_back(a0);
            idx.push_back(a1);
            idx.push_back(b1);
            idx.push_back(a0);
            idx.push_back(b1);
            idx.push_back(b0);
        }
    }
    Mesh m{};
    m.vertexCount = static_cast<int>(v.size() / 3);
    m.triangleCount = static_cast<int>(idx.size() / 3);
    m.vertices = static_cast<float*>(MemAlloc(sizeof(float) * v.size()));
    m.indices = static_cast<unsigned short*>(
        MemAlloc(sizeof(unsigned short) * idx.size()));
    for (std::size_t i = 0; i < v.size(); ++i) m.vertices[i] = v[i];
    for (std::size_t i = 0; i < idx.size(); ++i) m.indices[i] = idx[i];
    UploadMesh(&m, false);
    return m;
}

// An open truncated-cone slag pot (the static CC4 tipping pot at the crest rim;
// CC5 hands the tipping to the TRAIN pots). Local base at y=0.
Mesh build_pot_mesh() {
    const int n = 12;
    const double kTwoPi = 6.28318530717958647692;
    const float rb = 1.5f, rt = 2.0f, h = 3.0f;
    std::vector<float> v;
    std::vector<unsigned short> idx;
    for (int i = 0; i < n; ++i) {
        const double a = kTwoPi * i / n;
        v.push_back(rb * static_cast<float>(std::cos(a)));
        v.push_back(0.0f);
        v.push_back(rb * static_cast<float>(std::sin(a)));
    }
    for (int i = 0; i < n; ++i) {
        const double a = kTwoPi * i / n;
        v.push_back(rt * static_cast<float>(std::cos(a)));
        v.push_back(h);
        v.push_back(rt * static_cast<float>(std::sin(a)));
    }
    const unsigned short ctr = static_cast<unsigned short>(v.size() / 3);
    v.push_back(0.0f);
    v.push_back(0.0f);
    v.push_back(0.0f);
    for (int i = 0; i < n; ++i) {
        const int j = (i + 1) % n;
        const unsigned short bi = i, bj = j, ti = n + i, tj = n + j;
        idx.push_back(bi);
        idx.push_back(bj);
        idx.push_back(tj);
        idx.push_back(bi);
        idx.push_back(tj);
        idx.push_back(ti);
        idx.push_back(ctr);
        idx.push_back(bj);
        idx.push_back(bi);
    }
    Mesh m{};
    m.vertexCount = static_cast<int>(v.size() / 3);
    m.triangleCount = static_cast<int>(idx.size() / 3);
    m.vertices = static_cast<float*>(MemAlloc(sizeof(float) * v.size()));
    m.indices = static_cast<unsigned short*>(
        MemAlloc(sizeof(unsigned short) * idx.size()));
    for (std::size_t i = 0; i < v.size(); ++i) m.vertices[i] = v[i];
    for (std::size_t i = 0; i < idx.size(); ++i) m.indices[i] = idx[i];
    UploadMesh(&m, false);
    return m;
}

// Molten rivers down the +X pour face of the RIDGE (Fable F9: each vertex
// samples the SAME h(z)/y0(z) at its own drifting z, so the source sits on the
// real crest and the toe lands at the real base). Rivers are spread along the
// crest near the dump point, each narrowing at the source and fanning/widening
// + meandering downslope, with a LEADING CENTER vertex (round advancing front).
// texcoord.x = vD (0 rim -> 1 toe) the single-clock shader keys on; culling is
// off for the lava draw so winding is free.
Mesh build_lava_mesh(const SlagLook& L, const SlagRidge& g) {
    std::vector<float> v;
    std::vector<float> tc;
    std::vector<unsigned short> idx;
    // steps a multiple of the profile segments so the uniform d-samples LAND on
    // the bench knots (Fable P1-1: else a quad chords across a knot and buries
    // the lava). sub >= 4 AND >= 16/segments keeps the legacy planar face (B=1)
    // at its old 16 steps so the meander/nose don't go angular (Fable P2-1).
    const int seg = g.face_nk - 1;
    const int sub = (16 / seg) > 4 ? (16 / seg) : 4;
    const int steps = seg * sub;
    const float spacing =
        14.0f;  // z between thread BASE offsets along the crest (m)
    const float braid_amp =
        12.0f;  // lateral weave amplitude (m) -> threads CROSS = braid
    const float braid_freq =
        1.5f;                // weave cycles down the face (per-thread jittered)
    const float fan = 8.0f;  // extra downslope z spread (alluvial widening)
    const float pulse_freq =
        2.0f;  // width-pulse cycles -> pinch (split) / swell (merge)
    const float toe = 0.82f;  // downslope frac where the nose rounds to a point
    const float kTwoPi = 6.28318530717958647692f;
    // clamp the `rivers` fly-dial (Fable-AFTER P2, config-relative-bounds
    // lesson): the strip indices are unsigned short, so an absurd rivers count
    // would wrap `start`.
    const int rivers = L.rivers < 1 ? 1 : (L.rivers > 64 ? 64 : L.rivers);
    // BRAIDED river (Chad 2026-07-14): each thread WEAVES laterally
    // (braid_amp*sin), and adjacent threads share overlapping lateral ranges so
    // they CROSS -> an anastomosing braid, not parallel strips. Where the width
    // pulse PINCHES, a dark bar (bare crust) shows between threads (split);
    // where it swells, threads overlap (merge). Every vertex samples the SHARED
    // single-source face profile at its own drifting zc so all threads stay
    // glued to the terraced pour face (no float). texcoord.x = vD unchanged
    // (the single-clock shader is geometry-agnostic).
    for (int rv = 0; rv < rivers; ++rv) {
        const float z0 = (rv - 0.5f * (rivers - 1)) * spacing;
        // fixed deterministic per-thread hash (no clock) -> freq jitter breaks
        // the lockstep so threads split/merge instead of weaving as one wave
        // (Fable P2).
        float hs = std::sin(rv * 12.9898f) * 43758.5453f;
        hs = hs - std::floor(hs);  // fract -> [0,1)
        const float bfreq = braid_freq * (1.0f + 0.30f * hs);
        const float wphase = rv * 2.4f;  // per-thread weave phase
        const float pphase = rv * 1.7f;  // per-thread width-pulse phase
        const float fan_dir =
            (rv % 2 == 0) ? 1.0f : -1.0f;  // alternate spread direction
        const unsigned short start = static_cast<unsigned short>(v.size() / 3);
        for (int s = 0; s <= steps; ++s) {
            const float d = static_cast<float>(s) / steps;
            const float zc = z0 +
                             braid_amp * std::sin(kTwoPi * bfreq * d + wphase) +
                             fan_dir * fan * d;
            const float h = ridge_crest_h(g, zc);
            const float y0 = static_cast<float>(ridge_y0(g, zc));
            const float frun = g.face_run * (h / g.height);
            float xf, yf;
            face_profile_at(g, d, xf,
                            yf);  // SHARED benched profile (matches the mesh)
            const float cx = g.crest_hw + frun * xf;
            // per-thread lift eps*rv (Fable P1): threads coincide at braid
            // crossings -> coplanar z-fight flicker; a few cm of separation
            // kills it (imperceptible).
            const float cy = (y0 + h) - (h + kToeSink) * yf + L.lift_m +
                             static_cast<float>(rv) * 0.02f;
            // width PULSES down the face: pinches to a thread (split/bar), then
            // swells (merge). Min factor 0.10 keeps hw > 0 (no inverted tris,
            // Fable). Widens downslope (alluvial).
            float hw =
                L.river_halfwidth_m * (0.30f + 1.10f * d) *
                (0.55f + 0.45f * std::sin(kTwoPi * pulse_freq * d + pphase));
            if (d > toe) {
                const float u = (d - toe) / (1.0f - toe);
                hw *=
                    std::sqrt(std::max(0.0f, 1.0f - u * u));  // semicircle nose
            }
            // the center LEADS the sides (front bulges forward + overtakes
            // itself).
            const float vd_c = std::max(0.0f, d - 0.06f * d);
            v.push_back(cx);  // left (+z)
            v.push_back(cy);
            v.push_back(zc + hw);
            tc.push_back(d);
            tc.push_back(0.0f);
            v.push_back(cx);  // center (leading)
            v.push_back(cy);
            v.push_back(zc);
            tc.push_back(vd_c);
            tc.push_back(0.5f);
            v.push_back(cx);  // right (-z)
            v.push_back(cy);
            v.push_back(zc - hw);
            tc.push_back(d);
            tc.push_back(1.0f);
        }
        for (int s = 0; s < steps; ++s) {
            const unsigned short l0 =
                start + static_cast<unsigned short>(s * 3);
            const unsigned short c0 = l0 + 1, r0 = l0 + 2;
            const unsigned short l1 = l0 + 3, c1 = l0 + 4, r1 = l0 + 5;
            idx.push_back(l0);  // left sub-quad
            idx.push_back(c0);
            idx.push_back(c1);
            idx.push_back(l0);
            idx.push_back(c1);
            idx.push_back(l1);
            idx.push_back(c0);  // right sub-quad
            idx.push_back(r0);
            idx.push_back(r1);
            idx.push_back(c0);
            idx.push_back(r1);
            idx.push_back(c1);
        }
    }
    Mesh m{};
    m.vertexCount = static_cast<int>(v.size() / 3);
    m.triangleCount = static_cast<int>(idx.size() / 3);
    m.vertices = static_cast<float*>(MemAlloc(sizeof(float) * v.size()));
    m.texcoords = static_cast<float*>(MemAlloc(sizeof(float) * tc.size()));
    m.indices = static_cast<unsigned short*>(
        MemAlloc(sizeof(unsigned short) * idx.size()));
    for (std::size_t i = 0; i < v.size(); ++i) m.vertices[i] = v[i];
    for (std::size_t i = 0; i < tc.size(); ++i) m.texcoords[i] = tc[i];
    for (std::size_t i = 0; i < idx.size(); ++i) m.indices[i] = idx[i];
    UploadMesh(&m, false);
    return m;
}

}  // namespace

// ---- the shared ridge frame (public: train.cpp reads it for the crest drape)
// ----

SlagRidge make_slag_ridge(const HeightField& hf, const SlagLook& look) {
    SlagRidge g;
    g.hf = &hf;
    g.anchor = glm::normalize(kCcDumpDir);
    g.up = g.anchor;
    g.Ra = hf.radius_at(g.anchor);
    g.anchor_pos = g.anchor * g.Ra;
    // axis = the crest tangent = the spur's dump-leg through-direction
    // (Gram-Schmidt onto the tangent plane) so the crest runs ALONG the track
    // (NE-SW here).
    glm::dvec3 gv = kCcAxisReturn - kCcAxisApproach;
    gv = gv - glm::dot(gv, g.up) * g.up;
    if (glm::length(gv) < 1e-9)
        gv = glm::cross(g.up, glm::dvec3(0, 1, 0));  // degeneracy guard
    g.axis = glm::normalize(gv);
    // face = horizontal pour dir ⟂ crest, aimed NW (Chad: the pour faces NW
    // toward the open view/highway; kCcFaceAim is a point NW of the dump).
    glm::dvec3 f = glm::normalize(glm::cross(g.up, g.axis));
    if (glm::dot(f, kCcFaceAim - g.anchor) < 0.0) f = -f;
    g.face = f;
    g.side =
        glm::normalize(glm::cross(g.face, g.up));  // = ±axis; the trunnion line
    g.height = look.mound_height_m;
    g.half_len = 0.5f * look.ridge_length_m;
    g.crest_hw = 0.5f * look.crest_width_m;
    // FLAT crest = the middle ~22% each side of the length; the rest is the
    // long climb ramp + shallow descent (Chad: the train can't climb a steep
    // hill). Grade at length 1000/height 45 ≈ 6.6° — a readable gradual
    // embankment.
    g.crest_flat_half = 0.22f * g.half_len;
    const float kDeg2Rad = 3.14159265358979f / 180.0f;
    g.face_run = look.mound_height_m / std::tan(look.face_angle_deg * kDeg2Rad);
    g.back_run = look.mound_height_m * 2.4f;  // gentler back slope
    build_face_profile(
        g, look.benches);  // CC6: the shared benched pour-face profile
    g.ok = true;
    return g;
}

float ridge_crest_h(const SlagRidge& g, float z) {
    return g.height * ridge_endtaper(g, z) * (1.0f + 0.12f * ridge_noise(z));
}

// (xf,yf) at profile parameter t=[0,1]: piecewise-linear over the knots at t_k
// = k/(nk-1). The mesh + lava sample THIS; the drape inverts it
// (face_drop_at_run).
void face_profile_at(const SlagRidge& g, float t, float& xf, float& yf) {
    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    const float s = t * static_cast<float>(g.face_nk - 1);
    int k = static_cast<int>(s);
    if (k >= g.face_nk - 1) k = g.face_nk - 2;
    const float f = s - static_cast<float>(k);
    xf = g.face_xf[k] + (g.face_xf[k + 1] - g.face_xf[k]) * f;
    yf = g.face_yf[k] + (g.face_yf[k + 1] - g.face_yf[k]) * f;
}

// Drop fraction at horizontal run-fraction u=[0,1] (inverts face_xf, which is
// STRICTLY increasing) — the drape's single source, identical shape to the
// mesh/lava face.
float face_drop_at_run(const SlagRidge& g, float u) {
    u = u < 0.0f ? 0.0f : (u > 1.0f ? 1.0f : u);
    for (int k = 0; k < g.face_nk - 1; ++k) {
        if (u <= g.face_xf[k + 1]) {
            const float seg = g.face_xf[k + 1] - g.face_xf[k];
            const float f = seg > 1e-6f ? (u - g.face_xf[k]) / seg : 0.0f;
            return g.face_yf[k] + (g.face_yf[k + 1] - g.face_yf[k]) * f;
        }
    }
    return 1.0f;
}

double ridge_lift_at(const SlagRidge& g, const glm::dvec3& d) {
    if (!g.ok) return 0.0;
    const glm::dvec3 dn = glm::normalize(d);
    const glm::dvec3 delta =
        dn - g.anchor;  // small tangent offset (nearby dirs)
    const double z = glm::dot(delta, g.axis) * g.Ra;  // metres along the crest
    const double x =
        glm::dot(delta, g.face) * g.Ra;  // metres across (front = +face)
    if (std::fabs(z) >= g.half_len) return 0.0;
    const double h = ridge_crest_h(g, static_cast<float>(z));
    if (x >= -g.crest_hw && x <= g.crest_hw)
        return h;  // on the flat crest -> full
    // Fable-AFTER P1: the runs taper with h EXACTLY as the mesh draws them
    // (build_ridge_mesh frun/brun = run*h/height) — a flat run here forks the
    // crest and floats the rails ~11 m off the flank where the ridge tapers
    // (bites CC5).
    const double frac = h / g.height;
    if (x > g.crest_hw) {  // down the front (pour) face
        const double run = g.face_run * frac;
        if (run < 1e-3) return 0.0;
        const double u = (x - g.crest_hw) / run;
        if (u >= 1.0) return 0.0;
        // benched face — the THIRD consumer of the shared profile (Fable P0-1:
        // a linear h*(1-u) here forks the terraced mesh and floats/buries rails
        // on the flank). Vertical scale = h (the crest-relative lift), NOT
        // h+sink.
        return h * (1.0 - face_drop_at_run(g, static_cast<float>(u)));
    }
    const double run = g.back_run * frac;  // down the back
    if (run < 1e-3) return 0.0;
    const double u = (-x - g.crest_hw) / run;
    return u >= 1.0 ? 0.0 : h * (1.0 - u);
}

SlagRenderer build_slag_renderer(const HeightField& hf, const SlagLook& look) {
    SlagRenderer r;
    r.look = look;
    r.ridge = make_slag_ridge(hf, look);

    r.mound = build_ridge_mesh(look, r.ridge);
    r.pot = build_pot_mesh();
    r.lava = build_lava_mesh(look, r.ridge);

    r.mono_sh = LoadShaderFromMemory(kMonoVS, kMonoFS);
    r.m_sun = GetShaderLocation(r.mono_sh, "uSunDir");
    r.m_color = GetShaderLocation(r.mono_sh, "uColor");
    r.m_amb = GetShaderLocation(r.mono_sh, "uAmbient");
    r.m_diff = GetShaderLocation(r.mono_sh, "uDiffuse");
    r.mono_mat = LoadMaterialDefault();
    r.mono_mat.shader = r.mono_sh;

    const std::string lava_fs = lava_fragment_source();
    r.lava_sh = LoadShaderFromMemory(kLavaVS, lava_fs.c_str());
    r.l_phase = GetShaderLocation(r.lava_sh, "uPhase");
    r.l_glow = GetShaderLocation(r.lava_sh, "uGlow");
    r.l_night = GetShaderLocation(r.lava_sh, "uNight");
    r.l_sun = GetShaderLocation(r.lava_sh, "uSunDir");
    r.l_eye = GetShaderLocation(r.lava_sh, "uEye");
    r.lava_mat = LoadMaterialDefault();
    r.lava_mat.shader = r.lava_sh;

    r.ok = true;
    TraceLog(LOG_INFO, "SLAG: Copper Cliff slag ridge %.0f m, pour (%d rivers)",
             look.ridge_length_m, look.rivers);
    return r;
}

void unload_slag_renderer(SlagRenderer& r) {
    if (!r.ok) return;
    UnloadMesh(r.mound);
    UnloadMesh(r.pot);
    UnloadMesh(r.lava);
    UnloadShader(r.mono_sh);
    UnloadShader(r.lava_sh);
    r.ok = false;
}

namespace {
// Model matrix (raylib layout: m0,m4,m8,m12 = row 0) with columns X|Y|Z|P.
Matrix model_of(const glm::dvec3& X, const glm::dvec3& Y, const glm::dvec3& Z,
                const glm::dvec3& P) {
    return Matrix{static_cast<float>(X.x),
                  static_cast<float>(Y.x),
                  static_cast<float>(Z.x),
                  static_cast<float>(P.x),
                  static_cast<float>(X.y),
                  static_cast<float>(Y.y),
                  static_cast<float>(Z.y),
                  static_cast<float>(P.y),
                  static_cast<float>(X.z),
                  static_cast<float>(Y.z),
                  static_cast<float>(Z.z),
                  static_cast<float>(P.z),
                  0.0f,
                  0.0f,
                  0.0f,
                  1.0f};
}
}  // namespace

void draw_slag_renderer(SlagRenderer& r, double phase, const glm::vec3& sun_dir,
                        const glm::dvec3& eye, bool draw_static_pot) {
    if (!r.ok) return;
    const SlagRidge& g = r.ridge;
    const glm::dvec3 P = g.anchor_pos - eye;  // eye-relative ridge base origin

    // --- ridge (opaque mono dark); local X=face, Y=up, Z=axis ---
    SetShaderValue(r.mono_sh, r.m_sun, &sun_dir, SHADER_UNIFORM_VEC3);
    SetShaderValue(r.mono_sh, r.m_color, &r.look.mound_color,
                   SHADER_UNIFORM_VEC3);
    SetShaderValue(r.mono_sh, r.m_amb, &r.look.ambient, SHADER_UNIFORM_FLOAT);
    SetShaderValue(r.mono_sh, r.m_diff, &r.look.diffuse, SHADER_UNIFORM_FLOAT);
    rlDisableBackfaceCulling();
    const Matrix Mframe = model_of(g.face, g.up, g.axis, P);
    DrawMesh(r.mound, r.mono_mat, Mframe);

    // --- static tipping pot at the crest dump point (CC4 interim; when the CC5
    // train
    //     feeds the pour its pots supply the tipping and this is dropped). Tips
    //     about the crest axis (g.side) toward +face (over the edge). ---
    if (draw_static_pot) {
        const double thmax = 1.95;  // tips past horizontal to pour
        const double th =
            glm::smoothstep(0.0, 0.15, phase) * thmax *
            (1.0 - glm::smoothstep(0.60, 0.75, phase));  // up then back
        const glm::dvec3 pot_up = std::cos(th) * g.up + std::sin(th) * g.face;
        const glm::dvec3 pot_x = std::cos(th) * g.face - std::sin(th) * g.up;
        const double h0 = ridge_crest_h(g, 0.0f);
        const double y00 = ridge_y0(g, 0.0f);
        const glm::dvec3 rim = g.anchor_pos + g.up * (y00 + h0) +
                               g.face * (static_cast<double>(g.crest_hw) * 0.9);
        const Matrix Mpot = model_of(pot_x, pot_up, g.side, rim - eye);
        const glm::vec3 pot_col{0.42f, 0.42f, 0.44f};  // steel, still mono
        SetShaderValue(r.mono_sh, r.m_color, &pot_col, SHADER_UNIFORM_VEC3);
        DrawMesh(r.pot, r.mono_mat, Mpot);
    }

    // --- lava cascade (opaque WARM-CHROMA emissive; single-clock front) ---
    const float ph = static_cast<float>(phase);
    SetShaderValue(r.lava_sh, r.l_phase, &ph, SHADER_UNIFORM_FLOAT);
    SetShaderValue(r.lava_sh, r.l_glow, &r.look.glow, SHADER_UNIFORM_FLOAT);
    SetShaderValue(r.lava_sh, r.l_night, &r.look.night_boost,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(r.lava_sh, r.l_sun, &sun_dir, SHADER_UNIFORM_VEC3);
    const glm::vec3 eyef{static_cast<float>(eye.x), static_cast<float>(eye.y),
                         static_cast<float>(eye.z)};
    SetShaderValue(r.lava_sh, r.l_eye, &eyef, SHADER_UNIFORM_VEC3);
    DrawMesh(r.lava, r.lava_mat, Mframe);
    rlEnableBackfaceCulling();
}

}  // namespace render
