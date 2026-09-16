#include "render/train.h"

#include <cmath>
#include <cstdio>
#include <vector>

#include <glm/glm.hpp>

#include "render/copper_cliff_geo.h"  // kCcSpur — the shared CLOSED slag spur
#include "render/slag.h"  // SlagRidge + ridge_lift_at (the rails ride the crest, F3)
#include "rlgl.h"

namespace render {

namespace {

// The CLOSED spur (smelter -> dump -> return) is the SHARED kCcSpur (render/
// copper_cliff_geo.h) — unit sphere dirs from the LOCKED aeqd projection at the real
// Copper Cliff smelter/dump lon-lat. No OSM railway fetch exists, so it is authored.
// CC5: when a ridge is supplied, two crest points are inserted either side of the dump
// (kCcSpur[3]) so the dump leg runs ATOP the ridge crest.
const glm::dvec3* const kSpur = kCcSpur;
constexpr int kSpurN = 6;

// Chaikin corner-cut on a CLOSED ring (Fable CC2 P1: the raw polyline is C0-not-C1 at
// every vertex; smoothing rounds it). Each pass replaces every point with two (1/4, 3/4)
// points per edge; dirs renormalized.
std::vector<glm::dvec3> chaikin_closed(const std::vector<glm::dvec3>& p, int passes) {
    std::vector<glm::dvec3> cur = p;
    for (int it = 0; it < passes; ++it) {
        const std::size_t n = cur.size();
        std::vector<glm::dvec3> next;
        next.reserve(n * 2);
        for (std::size_t i = 0; i < n; ++i) {
            const glm::dvec3& a = cur[i];
            const glm::dvec3& b = cur[(i + 1) % n];
            next.push_back(glm::normalize(0.75 * a + 0.25 * b));
            next.push_back(glm::normalize(0.25 * a + 0.75 * b));
        }
        cur.swap(next);
    }
    return cur;
}

// Append a box (center c, half-extents h). Culling is off + the FS flips the facet
// normal to face the eye, so winding is free.
void add_box(std::vector<float>& v, std::vector<unsigned short>& idx, glm::vec3 c,
             glm::vec3 h) {
    const unsigned short base = static_cast<unsigned short>(v.size() / 3);
    for (int b = 0; b < 8; ++b) {
        v.push_back(c.x + ((b & 1) ? h.x : -h.x));
        v.push_back(c.y + ((b & 2) ? h.y : -h.y));
        v.push_back(c.z + ((b & 4) ? h.z : -h.z));
    }
    static const int face[6][4] = {{0, 2, 6, 4}, {1, 5, 7, 3}, {0, 4, 5, 1},
                                   {2, 3, 7, 6}, {0, 1, 3, 2}, {4, 6, 7, 5}};
    for (auto& f : face) {
        idx.push_back(base + f[0]);
        idx.push_back(base + f[1]);
        idx.push_back(base + f[2]);
        idx.push_back(base + f[0]);
        idx.push_back(base + f[2]);
        idx.push_back(base + f[3]);
    }
}

// Append a ring of n verts at height y, radius r, center (cx,_,cz). Returns the base idx.
unsigned short add_ring(std::vector<float>& v, float cx, float cz, float y, float r,
                        int n) {
    const unsigned short base = static_cast<unsigned short>(v.size() / 3);
    const double kTwoPi = 6.28318530717958647692;
    for (int i = 0; i < n; ++i) {
        const double a = kTwoPi * i / n;
        v.push_back(cx + r * static_cast<float>(std::cos(a)));
        v.push_back(y);
        v.push_back(cz + r * static_cast<float>(std::sin(a)));
    }
    return base;
}

// Bridge two n-vert rings (a then b) with a quad band.
void bridge_rings(std::vector<unsigned short>& idx, unsigned short a, unsigned short b,
                  int n) {
    for (int i = 0; i < n; ++i) {
        const int j = (i + 1) % n;
        idx.push_back(a + i);
        idx.push_back(a + j);
        idx.push_back(b + j);
        idx.push_back(a + i);
        idx.push_back(b + j);
        idx.push_back(b + i);
    }
}

// Central-difference tangent stencil (~a car length) — SHARED by the rail bake and the
// car draw so the two can't split (Fable Stage-3 P2-b).
constexpr double kTangentEps = 4.0;

Mesh make_mesh(std::vector<float>& v, std::vector<unsigned short>& idx) {
    if (v.size() / 3 > 65535)  // ushort index ceiling (Fable Stage-3 P2-a tripwire)
        TraceLog(LOG_ERROR, "TRAIN: mesh %zu verts > 65535 -> ushort index overflow",
                 v.size() / 3);
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

// The electric trolley LOCO (steeplecab): a low chassis, a tall center-peaked cab with
// short end hoods (double-ended, no turning), and a roof PANTOGRAPH (mast + contact bar
// reaching to the trolley wire). Local +X fwd, +Y up from the rail, +Z lateral.
Mesh build_loco() {
    std::vector<float> v;
    std::vector<unsigned short> idx;
    add_box(v, idx, {0.0f, 0.9f, 0.0f}, {5.6f, 0.9f, 1.55f});   // chassis (11 m)
    add_box(v, idx, {0.0f, 3.0f, 0.0f}, {2.1f, 1.3f, 1.35f});   // center cab (the peak)
    add_box(v, idx, {3.7f, 2.0f, 0.0f}, {1.7f, 0.55f, 1.3f});   // front hood
    add_box(v, idx, {-3.7f, 2.0f, 0.0f}, {1.7f, 0.55f, 1.3f});  // rear hood
    add_box(v, idx, {0.0f, 5.1f, 0.0f}, {0.12f, 0.85f, 0.12f}); // pantograph mast
    add_box(v, idx, {0.0f, 6.0f, 0.0f}, {0.10f, 0.06f, 1.05f}); // contact bar (lateral)
    return make_mesh(v, idx);
}

// The static part of a slag-pot CAR: a flat 4-wheel deck + two trunnion cradles (one per
// pot, at local X = +-kPotX) each a pair of side posts holding the pot's trunnion pins.
constexpr float kPotX = 2.2f;         // fore/aft seat of the two pots on the deck
constexpr float kTrunnionY = 2.05f;   // trunnion (pot pivot) height above the rail
Mesh build_potcar() {
    std::vector<float> v;
    std::vector<unsigned short> idx;
    add_box(v, idx, {0.0f, 0.5f, 0.0f}, {3.7f, 0.32f, 1.5f});    // flatcar deck
    add_box(v, idx, {2.4f, 0.05f, 0.0f}, {0.9f, 0.35f, 1.5f});   // front truck
    add_box(v, idx, {-2.4f, 0.05f, 0.0f}, {0.9f, 0.35f, 1.5f});  // rear truck
    for (float xoff : {kPotX, -kPotX}) {                          // cradle posts (both pots)
        add_box(v, idx, {xoff, 1.35f, 1.25f}, {0.28f, 0.72f, 0.2f});
        add_box(v, idx, {xoff, 1.35f, -1.25f}, {0.28f, 0.72f, 0.2f});
    }
    return make_mesh(v, idx);
}

// A giant cast-steel slag POT, centered at its TRUNNION (y=0 = the pivot; the cup hangs
// below and the rim rises above) so the tip rotation is a clean rigid rotation of the
// local frame (Fable F6). A thick-walled tapered cup, WIDER AT THE RIM, with a thick rim
// annulus and a hollow cavity. Culling off + eye-flip FS -> winding free.
Mesh build_slag_pot() {
    const int n = 14;
    std::vector<float> v;
    std::vector<unsigned short> idx;
    const float ybot = -1.45f, yrim = 1.15f;         // cup bottom / rim (trunnion at y=0)
    const float rbot = 1.05f, rout = 1.55f;          // outer: narrow base -> wide rim
    const float rin = 1.28f, ribot = -0.85f;         // inner cavity: rim inner r / floor y
    const float rinb = 0.7f;
    const unsigned short obot = add_ring(v, 0, 0, ybot, rbot, n);   // outer bottom
    const unsigned short otop = add_ring(v, 0, 0, yrim, rout, n);   // outer rim
    const unsigned short itop = add_ring(v, 0, 0, yrim, rin, n);    // inner rim
    const unsigned short ibot = add_ring(v, 0, 0, ribot, rinb, n);  // inner floor
    bridge_rings(idx, obot, otop, n);   // outer wall
    bridge_rings(idx, otop, itop, n);   // thick rim annulus (top face)
    bridge_rings(idx, itop, ibot, n);   // inner wall (cavity)
    const unsigned short ob = static_cast<unsigned short>(v.size() / 3);  // outer base ctr
    v.push_back(0.0f);
    v.push_back(ybot);
    v.push_back(0.0f);
    const unsigned short ic = static_cast<unsigned short>(v.size() / 3);  // cavity floor ctr
    v.push_back(0.0f);
    v.push_back(ribot);
    v.push_back(0.0f);
    for (int i = 0; i < n; ++i) {
        const int j = (i + 1) % n;
        idx.push_back(ob);
        idx.push_back(obot + i);
        idx.push_back(obot + j);  // outer bottom cap
        idx.push_back(ic);
        idx.push_back(ibot + j);
        idx.push_back(ibot + i);  // cavity floor
    }
    return make_mesh(v, idx);
}

const char* kTrainVS = R"GLSL(#version 330
in vec3 vertexPosition;      // LOCAL car space (+X fwd, +Y up, +Z right)
uniform mat4 mvp;            // raylib-set: proj*view*matModel
uniform mat4 matModel;       // the car pose (eye-relative): cols fwd|up|right|(pt-eye)
out vec3 vPos;               // eye-relative world position
void main() {
    vPos = (matModel * vec4(vertexPosition, 1.0)).xyz;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)GLSL";

// Mono flat-shaded steel (the buildings' facet-normal-from-dFdx pattern). Mono (r==g==b)
// so the S1 silver post tones it (the CC3 hot slag is the one chroma exception).
const char* kTrainFS = R"GLSL(#version 330
in vec3 vPos;                // eye-relative world position
uniform vec3 uSunDir;        // world light-travel dir (sun -> scene)
uniform vec3 uColor;
uniform float uAmbient;
uniform float uDiffuse;
out vec4 finalColor;
void main() {
    vec3 N = cross(dFdx(vPos), dFdy(vPos));
    float nl2 = dot(N, N);
    if (nl2 < 1.0e-12) { finalColor = vec4(uColor * uAmbient, 1.0); return; }
    N *= inversesqrt(nl2);
    if (dot(N, normalize(-vPos)) < 0.0) N = -N;       // outward toward the eye
    float lit = uAmbient + uDiffuse * max(dot(N, -uSunDir), 0.0);
    finalColor = vec4(clamp(uColor * lit, 0.0, 1.0), 1.0);
}
)GLSL";

// World point at draped arc-length s (wrapped into [0,L)) + its unit dir. `ridge`
// (nullable) lifts the rail head onto the CC4 slag ridge crest (ridge_lift_at returns 0
// off the ridge, so only the crest cars rise). Used by the moving cars AND the static
// rails so the two can NEVER fork (Fable Stage-2 Q5: rails drape via the SAME point_at).
glm::dvec3 point_at(const TrainRenderer& r, double s, glm::dvec3& dir_out,
                    const SlagRidge* ridge) {
    const std::size_t m = r.pts.size();
    s = std::fmod(s, r.L);
    if (s < 0.0) s += r.L;   // Fable CC2 P0: truncated fmod is negative for trailing cars
    if (s >= r.L) s = 0.0;   // Fable-AFTER P1: fmod(-tiny)+L can round to EXACTLY L -> OOB
    std::size_t i = 0;
    while (i + 1 < r.cum.size() && r.cum[i + 1] <= s) ++i;
    const double seg = r.cum[i + 1] - r.cum[i];
    const double t = seg > 1e-9 ? (s - r.cum[i]) / seg : 0.0;
    const glm::dvec3& a = r.pts[i];
    const glm::dvec3& b = r.pts[(i + 1) % m];
    dir_out = glm::normalize(a + (b - a) * t);  // Fable CC2 P1: renormalize the lerp
    const double crest = ridge ? ridge_lift_at(*ridge, dir_out) : 0.0;
    return dir_out * (r.hf->radius_at(dir_out) + r.look.lift_m + crest);
}

// CC6 (Chad: "a track to move on") — a STATIC rail mesh: two draped rail ribbons at
// standard gauge + cross-ties, sampled every ~4 m along the SAME draped spur as the
// cars (point_at, so the rails climb the ridge crest too). Continuous ribbon strips
// (Fable: not per-segment boxes, which gap/overlap on curves). Baked in ONE local
// anchor frame (origin-relative), drawn eye-relative like the ridge; mono steel FS.
Mesh build_rails(const TrainRenderer& r, const SlagRidge* ridge,
                 const glm::dvec3& origin) {
    std::vector<float> v;
    std::vector<unsigned short> idx;
    const double gauge_h = 0.7175;   // half of standard gauge 1.435 m
    const double rail_up = 0.28;     // rail head above the drape
    const double rail_hw = 0.13;     // rail ribbon half-width
    const double tie_up = 0.14, tie_over = 0.20, tie_half_len = 0.95;
    const double step = 4.0, tie_step = 6.0, eps = kTangentEps;
    auto frame = [&](double s, glm::dvec3& up, glm::dvec3& right) {
        glm::dvec3 dir, d2;
        const glm::dvec3 p = point_at(r, s, dir, ridge);
        const glm::dvec3 pf = point_at(r, s + eps, d2, ridge);
        const glm::dvec3 pb = point_at(r, s - eps, d2, ridge);
        up = dir;
        glm::dvec3 fwd = pf - pb;
        fwd = fwd - glm::dot(fwd, up) * up;
        fwd = glm::length(fwd) < 1e-9 ? glm::dvec3(1, 0, 0) : glm::normalize(fwd);
        right = glm::normalize(glm::cross(fwd, up));
        return p;
    };
    const int N = std::max(2, static_cast<int>(std::ceil(r.L / step)));
    auto push = [&](const glm::dvec3& w) {
        v.push_back(static_cast<float>(w.x - origin.x));
        v.push_back(static_cast<float>(w.y - origin.y));
        v.push_back(static_cast<float>(w.z - origin.z));
    };
    for (int rail = 0; rail < 2; ++rail) {                 // two continuous rail ribbons
        const double side = rail == 0 ? 1.0 : -1.0;
        const unsigned short base = static_cast<unsigned short>(v.size() / 3);
        for (int i = 0; i <= N; ++i) {
            glm::dvec3 up, right;
            const glm::dvec3 p = frame(r.L * i / N, up, right);
            const glm::dvec3 c = p + up * rail_up + right * (side * gauge_h);
            push(c - right * rail_hw);
            push(c + right * rail_hw);
        }
        for (int i = 0; i < N; ++i) {
            const unsigned short a0 = base + static_cast<unsigned short>(i * 2);
            const unsigned short a1 = a0 + 1, b0 = a0 + 2, b1 = a0 + 3;
            idx.push_back(a0); idx.push_back(a1); idx.push_back(b1);
            idx.push_back(a0); idx.push_back(b1); idx.push_back(b0);
        }
    }
    const int NT = std::max(1, static_cast<int>(std::floor(r.L / tie_step)));
    for (int i = 0; i < NT; ++i) {                          // cross-ties (flat quads)
        glm::dvec3 up, right;
        const glm::dvec3 p = frame(r.L * i / NT, up, right);
        const glm::dvec3 fwd = glm::normalize(glm::cross(up, right));  // along the track
        const glm::dvec3 ctr = p + up * tie_up;
        const double w = gauge_h + tie_over;
        const unsigned short base = static_cast<unsigned short>(v.size() / 3);
        push(ctr - right * w - fwd * tie_half_len);
        push(ctr + right * w - fwd * tie_half_len);
        push(ctr + right * w + fwd * tie_half_len);
        push(ctr - right * w + fwd * tie_half_len);
        idx.push_back(base); idx.push_back(base + 1); idx.push_back(base + 2);
        idx.push_back(base); idx.push_back(base + 2); idx.push_back(base + 3);
    }
    return make_mesh(v, idx);
}

}  // namespace

TrainRenderer build_train_renderer(const HeightField& hf, const TrainLook& look,
                                   const SlagRidge* ridge) {
    TrainRenderer r;
    r.look = look;
    r.hf = &hf;

    // Build the (optionally rerouted) control polyline. CC5 F2: insert two crest points
    // ANGULARLY either side of the dump so the dump leg runs atop the ridge crest. The
    // -axis point precedes kSpur[3] (axis = the travel direction), and P-/kSpur3/P+ are
    // collinear so Chaikin keeps the crest segment straight through the dump.
    std::vector<glm::dvec3> raw(kSpur, kSpur + kSpurN);
    if (ridge && ridge->ok) {
        // Crest points near the ridge ENDS (98% of the half-length) so the dump leg
        // spans the WHOLE ridge: the track enters at the ramp bottom (crest h~0), climbs
        // the long grade to the flat crest, and descends the far ramp (Chad 2026-07-13).
        const double delta = 0.98 * ridge->half_len / ridge->Ra;  // angular, not metres (F2)
        const glm::dvec3 a = ridge->anchor, ax = ridge->axis;
        const glm::dvec3 pm = glm::normalize(a * std::cos(delta) - ax * std::sin(delta));
        const glm::dvec3 pp = glm::normalize(a * std::cos(delta) + ax * std::sin(delta));
        raw = {kSpur[0], kSpur[1], kSpur[2], pm, kSpur[3], pp, kSpur[4], kSpur[5]};
        r.coupled = true;
        r.dump_face = ridge->face;
    }
    r.pts = chaikin_closed(raw, 2);
    const std::size_t m = r.pts.size();
    r.cum.assign(m + 1, 0.0);
    for (std::size_t i = 0; i < m; ++i) {
        const glm::dvec3& a = r.pts[i];
        const glm::dvec3& b = r.pts[(i + 1) % m];
        const double ang = std::acos(glm::clamp(glm::dot(a, b), -1.0, 1.0));
        const double rad = 0.5 * (hf.radius_at(a) + hf.radius_at(b));
        r.cum[i + 1] = r.cum[i] + ang * rad;
    }
    r.L = r.cum[m];

    // F7: the dump arc-length = the point on the smoothed spur closest to the dump dir
    // (Chaikin does not pass through the control point). Fable-AFTER P1: a bare
    // vertex-argmax snaps ~30 m off the true midpoint (61 m vertex spacing) AND is an fp
    // TIE on the collinear crest -> the pots would pour at the EDGE of the lava, not over
    // it. So argmax the nearest vertex, then REFINE by projecting the anchor onto its two
    // adjacent segments and taking the closer.
    if (r.coupled) {
        double best = -2.0;
        std::size_t bi = 0;
        for (std::size_t i = 0; i < m; ++i) {
            const double dp = glm::dot(r.pts[i], ridge->anchor);
            if (dp > best) {
                best = dp;
                bi = i;
            }
        }
        double bd2 = 1e300;
        for (std::size_t k = 0; k < 2; ++k) {  // segments (bi-1 -> bi) and (bi -> bi+1)
            const std::size_t i = (bi + m - 1 + k) % m;
            const glm::dvec3& a = r.pts[i];
            const glm::dvec3& b = r.pts[(i + 1) % m];
            const glm::dvec3 ab = b - a;
            const double den = glm::dot(ab, ab);
            const double t =
                den > 1e-18 ? glm::clamp(glm::dot(ridge->anchor - a, ab) / den, 0.0, 1.0)
                            : 0.0;
            const glm::dvec3 d = (a + ab * t) - ridge->anchor;
            const double d2 = glm::dot(d, d);
            if (d2 < bd2) {
                bd2 = d2;
                r.s_dump = r.cum[i] + t * (r.cum[i + 1] - r.cum[i]);
            }
        }
    }

    // Fable P1 (CC7): the pour-gate window is [0.85, pots+0.85]*car_gap of head travel
    // past the dump; if it exceeds the loop length L it wraps onto itself -> ghost/
    // always-on pours. Huge margin at the defaults (~319 m vs L~3951 m) but car_gap_m is
    // a fly-dial, so warn if a crank pushes the window past L.
    const double pour_window = (static_cast<double>(r.look.pots) + 0.85) * r.look.car_gap_m;
    if (pour_window >= r.L)
        TraceLog(LOG_WARNING,
                 "TRAIN: pour window %.0f m >= spur %.0f m (car_gap_m too large) -> "
                 "ghost pours; lower [train] car_gap_m or pots",
                 pour_window, r.L);

    // CC6 static rails: baked relative to the ridge dump anchor (or the smelter end)
    // so the mesh coords stay small; drawn eye-relative. Uses the SAME draped point_at
    // as the cars, so the rails ride the ridge crest identically (no fork).
    r.rails_origin = (ridge && ridge->ok)
                         ? ridge->anchor_pos
                         : glm::normalize(r.pts[0]) * hf.radius_at(r.pts[0]);
    r.rails = build_rails(r, ridge, r.rails_origin);

    // CC6: prefer the reference-true glTF loco + pot (blender-hero-forge, exported in
    // the SEADS frame: +X fwd, +Y up, +Z right, export_yup=False -> no axis convert).
    // A missing/empty glb falls back to the procedural mesh so the train never blanks.
    auto load_or_build = [](const char* rel, Model& mdl, bool& loaded,
                            Mesh (*fallback)()) -> Mesh {
        char path[256];
        std::snprintf(path, sizeof path, "%s/coppercliff/%s", SEADS_ASSET_DIR, rel);
        if (FileExists(path)) {
            Model m = LoadModel(path);
            if (m.meshCount >= 1 && m.meshes != nullptr &&
                m.meshes[0].vertexCount > 0) {  // Fable P2-b: reject an empty primitive
                if (m.meshCount > 1)             // Fable P2-a: only meshes[0] is drawn
                    TraceLog(LOG_WARNING, "TRAIN: '%s' has %d prims; only [0] drawn",
                             rel, m.meshCount);
                mdl = m;
                loaded = true;
                return m.meshes[0];
            }
            UnloadModel(m);  // empty/degenerate glb
        }
        TraceLog(LOG_WARNING, "TRAIN: '%s' missing/empty; procedural fallback", rel);
        return fallback();
    };
    r.loco = load_or_build("loco.glb", r.loco_model, r.loco_loaded, build_loco);
    r.potcar = build_potcar();
    r.pot = load_or_build("slag_pot.glb", r.pot_model, r.pot_loaded, build_slag_pot);

    r.shader = LoadShaderFromMemory(kTrainVS, kTrainFS);
    r.loc_sun = GetShaderLocation(r.shader, "uSunDir");
    r.loc_color = GetShaderLocation(r.shader, "uColor");
    r.loc_amb = GetShaderLocation(r.shader, "uAmbient");
    r.loc_diff = GetShaderLocation(r.shader, "uDiffuse");
    r.mat = LoadMaterialDefault();
    r.mat.shader = r.shader;
    r.ok = true;
    TraceLog(LOG_INFO, "TRAIN: slag spur %.0f m, loco + %d pot-cars%s", r.L, look.pots,
             r.coupled ? " (crest-routed, tipping)" : "");
    return r;
}

void unload_train_renderer(TrainRenderer& r) {
    if (!r.ok) return;
    if (r.loco_loaded) UnloadModel(r.loco_model);  // frees meshes[0] (== r.loco)
    else UnloadMesh(r.loco);
    UnloadMesh(r.potcar);
    if (r.pot_loaded) UnloadModel(r.pot_model);
    else UnloadMesh(r.pot);
    UnloadMesh(r.rails);
    UnloadShader(r.shader);
    r.ok = false;
}

namespace {
// Signed shortest arc distance a into [-L/2, L/2) (Fable F11: floored mod + recenter).
double wrapdist(double a, double L) {
    double d = std::fmod(a, L);
    if (d < 0.0) d += L;
    if (d > 0.5 * L) d -= L;
    return d;
}

// C1 tip bump (Fable F5): peak 1 at x=0, exactly 0 with zero SLOPE at |x|>=1.
double tip_bump(double x) {
    const double a = 1.0 - x * x;
    return a <= 0.0 ? 0.0 : a * a;
}

Matrix model_of(const glm::dvec3& X, const glm::dvec3& Y, const glm::dvec3& Z,
                const glm::dvec3& P) {
    return Matrix{static_cast<float>(X.x), static_cast<float>(Y.x),
                  static_cast<float>(Z.x), static_cast<float>(P.x),
                  static_cast<float>(X.y), static_cast<float>(Y.y),
                  static_cast<float>(Z.y), static_cast<float>(P.y),
                  static_cast<float>(X.z), static_cast<float>(Y.z),
                  static_cast<float>(Z.z), static_cast<float>(P.z),
                  0.0f, 0.0f, 0.0f, 1.0f};
}
}  // namespace

void draw_train_renderer(TrainRenderer& r, double phase, const glm::vec3& sun_dir,
                         const glm::dvec3& eye, const SlagRidge* ridge) {
    if (!r.ok || r.L < 1.0) return;
    SetShaderValue(r.shader, r.loc_sun, &sun_dir, SHADER_UNIFORM_VEC3);
    SetShaderValue(r.shader, r.loc_color, &r.look.color, SHADER_UNIFORM_VEC3);
    SetShaderValue(r.shader, r.loc_amb, &r.look.ambient, SHADER_UNIFORM_FLOAT);
    SetShaderValue(r.shader, r.loc_diff, &r.look.diffuse, SHADER_UNIFORM_FLOAT);

    const double s_head = phase * r.L;
    const double gap = r.look.car_gap_m;
    const double eps = kTangentEps;  // tangent stencil ~ a car length (Fable CC2 P1)
    const double thmax = 2.10;  // ~120 deg tip past horizontal to pour
    const bool tip = r.coupled && ridge != nullptr;
    rlDisableBackfaceCulling();  // FS flips the facet normal to face the eye

    // static rails (CC6): a pure translation of the baked local mesh (origin - eye).
    const Matrix Mrails = model_of({1, 0, 0}, {0, 1, 0}, {0, 0, 1}, r.rails_origin - eye);
    DrawMesh(r.rails, r.mat, Mrails);
    for (int c = 0; c <= r.look.pots; ++c) {
        const double s = s_head - static_cast<double>(c) * gap;
        glm::dvec3 dir;
        const glm::dvec3 p = point_at(r, s, dir, ridge);
        glm::dvec3 d2;
        const glm::dvec3 pf = point_at(r, s + eps, d2, ridge);
        const glm::dvec3 pb = point_at(r, s - eps, d2, ridge);
        const glm::dvec3 up = dir;  // radial
        glm::dvec3 fwd = pf - pb;
        fwd = fwd - glm::dot(fwd, up) * up;  // Gram-Schmidt onto the tangent plane
        if (glm::length(fwd) < 1e-6) continue;
        fwd = glm::normalize(fwd);
        const glm::dvec3 right = glm::normalize(glm::cross(fwd, up));
        const glm::dvec3 P = p - eye;  // eye-relative
        const Matrix M = model_of(fwd, up, right, P);
        if (c == 0) {
            DrawMesh(r.loco, r.mat, M);  // the electric trolley loco
            continue;
        }
        DrawMesh(r.potcar, r.mat, M);  // static deck + cradles

        // Tip each of the two pots about the car's FWD trunnion axis toward the pour
        // face as it crosses the dump zone (Fable F6 rigid rotation; F5 C1 bump).
        double th = 0.0;
        double s_sign = 1.0;
        if (tip) {
            const double d = wrapdist(s - r.s_dump, r.L);
            th = thmax * tip_bump(d / r.look.tip_span_m);
            s_sign = glm::dot(right, r.dump_face) < 0.0 ? -1.0 : 1.0;  // toward +face
        }
        const glm::dvec3 tside = s_sign * right;
        const glm::dvec3 pot_up = std::cos(th) * up + std::sin(th) * tside;
        const glm::dvec3 pot_rt = -std::sin(th) * up + std::cos(th) * tside;
        for (float xoff : {kPotX, -kPotX}) {
            const glm::dvec3 trunnion = p + up * static_cast<double>(kTrunnionY) +
                                        fwd * static_cast<double>(xoff);
            const Matrix Mp = model_of(fwd, pot_up, s_sign * pot_rt, trunnion - eye);
            DrawMesh(r.pot, r.mat, Mp);
        }
    }
    rlEnableBackfaceCulling();
}

double train_dump_phase(const TrainRenderer& r, double train_phase) {
    if (!r.ok || !r.coupled || r.L < 1.0) return -1.0;  // sentinel -> caller falls back
    const double gap = r.look.car_gap_m;
    const double w = wrapdist(train_phase * r.L - r.s_dump, r.L);  // head past the dump
    const int pots = r.look.pots;
    // Fable F1: pour ONLY while the string is over the dump (else ~424 ghost pours/lap);
    // frac(w/gap+0.15) cycles once per arriving pot, =0 at both window edges (no seam).
    if (w >= 0.85 * gap && w <= (static_cast<double>(pots) + 0.85) * gap) {
        const double q = w / gap + 0.15;
        return q - std::floor(q);
    }
    return 0.0;  // FS-dark between passes
}

}  // namespace render
