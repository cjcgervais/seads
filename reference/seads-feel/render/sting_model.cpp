// THE STING, DRAWN -- see render/sting_model.h for the contract.
#include "render/sting_model.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include "raylib.h"
#include "rlgl.h"  // rlDisableDepthMask -- the prop-disc pass (draw.cpp:3502)

// cgltf: the implementation is compiled inside raylib's rmodels.c (C
// linkage, non-static) -- declare only, never define CGLTF_IMPLEMENTATION
// here or the link gets duplicate symbols (the flak_model.cpp idiom).
extern "C" {
#include "external/cgltf.h"
}

#include "render/team_color.h"
#include "render/team_kit.h"

namespace render {
namespace {

struct SNode {
    std::string name;
    // 0 = a rest node. +1 / -1 = a driven prop and which way it turns (the
    // quad convention: ul/lr one way, ur/ll the other). Resolved ONCE at load
    // from the node name so the per-frame draw does no string work.
    int spin_sign = 0;
    // Tip radius [model units] of a driven prop, MEASURED at load from that
    // prop's own vertices (max hypot(x, y) about its hub) rather than typed in
    // as 0.23. A re-export that changes the blade length moves the disc with
    // it; a constant would silently draw the old prop's disc on the new prop.
    double tip_r = 0.0;
    glm::dvec3 t{0.0};
    glm::dquat r{1.0, 0.0, 0.0, 0.0};
    glm::dvec3 s{1.0};
    int parent = -1;
    glm::dmat4 world{1.0};
};

struct SPrim {
    Mesh mesh{};
    Color col{255, 255, 255, 255};  // as authored in the GLB
    bool dark = false;              // keep `col` as-is, never team-tint it
    int node = -1;
};

struct StingModel {
    bool tried = false;
    bool ok = false;
    std::vector<SNode> nodes;
    std::vector<int> order;  // parents before children
    std::vector<SPrim> prims;
    Material mat{};
    // ★ THE BLUR DISC. ONE unit-radius mesh, built procedurally at load and
    // scaled per prop -- there is no texture pipeline here and none is needed.
    // DOUBLE-SIDED by construction (each segment emitted twice with opposite
    // winding) rather than by toggling backface culling around the pass: the
    // disc is seen from both sides as the drone rolls, and a state toggle is a
    // thing a later edit can forget to restore.
    Mesh disc{};
    bool disc_ok = false;
    Material disc_mat{};
};

StingModel g_sting;

// Read a float VEC3/scalar accessor into out (n * comps floats).
bool read_floats(const cgltf_accessor* a, int comps, std::vector<float>& out) {
    if (a == nullptr) return false;
    out.resize(a->count * static_cast<std::size_t>(comps));
    for (cgltf_size i = 0; i < a->count; ++i)
        if (cgltf_accessor_read_float(a, i, &out[i * comps], comps) == 0)
            return false;
    return true;
}

// Tint rule: ONLY the declared body slot ("body" in the material name --
// the asset ships sting_mat_body as the ally-tint target) wears the team
// blue; every other material (tan pods, silver tape, carbon, glass) keeps
// its authored colour so the material story survives the livery pass. A
// name-based allowlist, not a darkness heuristic: the first cut tinted by
// "not dark" and would have painted the tan pods blue.
bool is_dark_material(const cgltf_material* m) {
    if (m == nullptr || m->name == nullptr) return true;  // unnamed: keep as-is
    std::string n = m->name;
    for (char& c : n) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return n.find("body") == std::string::npos;
}

bool load_model() {
    StingModel& sm = g_sting;
    char path[512];
    std::snprintf(path, sizeof path, "%s/drone/sting.glb", SEADS_ASSET_DIR);
    cgltf_options opt{};
    cgltf_data* data = nullptr;
    if (cgltf_parse_file(&opt, path, &data) != cgltf_result_success ||
        cgltf_load_buffers(&opt, data, path) != cgltf_result_success) {
        TraceLog(LOG_WARNING, "STING: %s missing/unreadable -- primitive fallback",
                 path);
        if (data != nullptr) cgltf_free(data);
        return false;
    }
    const std::size_t nn = data->nodes_count;
    sm.nodes.resize(nn);
    for (std::size_t i = 0; i < nn; ++i) {
        const cgltf_node& n = data->nodes[i];
        SNode& out = sm.nodes[i];
        out.name = n.name != nullptr ? n.name : "";
        // ★ THE FOUR DRIVEN PROPS. Matched by the asset's own naming
        // (sting_prop_ul / _ur / _ll / _lr); anything else stays rest-posed,
        // so a renamed or missing prop degrades to a still model rather than
        // to a wrongly-spun one.
        if (out.name.rfind("sting_prop_", 0) == 0) {
            const std::string sfx = out.name.substr(11);
            if (sfx == "ul" || sfx == "lr") out.spin_sign = 1;
            else if (sfx == "ur" || sfx == "ll") out.spin_sign = -1;
        }
        if (n.has_translation)
            out.t = {n.translation[0], n.translation[1], n.translation[2]};
        if (n.has_rotation)
            out.r = glm::dquat(n.rotation[3], n.rotation[0], n.rotation[1],
                               n.rotation[2]);
        if (n.has_scale) out.s = {n.scale[0], n.scale[1], n.scale[2]};
        for (std::size_t c = 0; c < n.children_count; ++c)
            sm.nodes[n.children[c] - data->nodes].parent = static_cast<int>(i);
    }
    // parents-before-children (glTF array order is not topological)
    sm.order.reserve(nn);
    {
        std::vector<char> placed(nn, 0);
        bool grew = true;
        while (sm.order.size() < nn && grew) {
            grew = false;
            for (std::size_t i = 0; i < nn; ++i) {
                if (placed[i]) continue;
                const int p = sm.nodes[i].parent;
                if (p < 0 || placed[p]) {
                    sm.order.push_back(static_cast<int>(i));
                    placed[i] = 1;
                    grew = true;
                }
            }
        }
    }
    // meshes: one upload per primitive, flat base_color via lround (the
    // sled_model/flak_model rule -- truncation darkens every tone by up to
    // 1/255)
    for (std::size_t i = 0; i < nn; ++i) {
        const cgltf_node& n = data->nodes[i];
        if (n.mesh == nullptr) continue;
        for (cgltf_size pi = 0; pi < n.mesh->primitives_count; ++pi) {
            const cgltf_primitive& pr = n.mesh->primitives[pi];
            const cgltf_accessor *pos = nullptr, *nrm = nullptr;
            for (cgltf_size a = 0; a < pr.attributes_count; ++a) {
                if (pr.attributes[a].type == cgltf_attribute_type_position)
                    pos = pr.attributes[a].data;
                if (pr.attributes[a].type == cgltf_attribute_type_normal)
                    nrm = pr.attributes[a].data;
            }
            std::vector<float> vp, vn;
            if (!read_floats(pos, 3, vp)) continue;
            // The disc is the blade's own reach: measured here, off the very
            // vertices being uploaded, so the two cannot disagree.
            if (sm.nodes[i].spin_sign != 0) {
                double r2max = 0.0;
                for (std::size_t v = 0; v + 2 < vp.size(); v += 3) {
                    const double r2 = static_cast<double>(vp[v]) * vp[v] +
                                      static_cast<double>(vp[v + 1]) * vp[v + 1];
                    if (r2 > r2max) r2max = r2;
                }
                sm.nodes[i].tip_r =
                    std::max(sm.nodes[i].tip_r, std::sqrt(r2max));
            }
            const bool has_n = read_floats(nrm, 3, vn);
            SPrim sp;
            sp.node = static_cast<int>(i);
            sp.dark = is_dark_material(pr.material);
            Mesh& m = sp.mesh;
            m.vertexCount = static_cast<int>(pos->count);
            m.triangleCount =
                pr.indices != nullptr
                    ? static_cast<int>(pr.indices->count / 3)
                    : m.vertexCount / 3;
            m.vertices = static_cast<float*>(
                MemAlloc(sizeof(float) * 3 * static_cast<unsigned>(m.vertexCount)));
            std::memcpy(m.vertices, vp.data(), sizeof(float) * vp.size());
            if (has_n) {
                m.normals = static_cast<float*>(MemAlloc(
                    sizeof(float) * 3 * static_cast<unsigned>(m.vertexCount)));
                std::memcpy(m.normals, vn.data(), sizeof(float) * vn.size());
            }
            if (pr.indices != nullptr) {
                m.indices = static_cast<unsigned short*>(MemAlloc(
                    sizeof(unsigned short) * static_cast<unsigned>(pr.indices->count)));
                for (cgltf_size k = 0; k < pr.indices->count; ++k)
                    m.indices[k] = static_cast<unsigned short>(
                        cgltf_accessor_read_index(pr.indices, k));
            }
            if (pr.material != nullptr &&
                pr.material->has_pbr_metallic_roughness) {
                const float* c =
                    pr.material->pbr_metallic_roughness.base_color_factor;
                sp.col = Color{static_cast<unsigned char>(
                                   std::lround(c[0] * 255.0f)),
                               static_cast<unsigned char>(
                                   std::lround(c[1] * 255.0f)),
                               static_cast<unsigned char>(
                                   std::lround(c[2] * 255.0f)),
                               255};
            }
            UploadMesh(&m, false);
            sm.prims.push_back(sp);
        }
    }
    cgltf_free(data);
    // ★ THE UNIT BLUR DISC: a triangle fan in the XY plane (normal +Z, the
    // props' own spin axis), radius 1, emitted twice per segment with opposite
    // winding so it is visible from either side. Unindexed -- 3 verts per tri
    // is 32 * 2 * 3 = 192 vertices, and an index buffer for that is bookkeeping
    // without a saving.
    {
        constexpr int kSeg = 32;
        const int tris = kSeg * 2;
        Mesh& dm = sm.disc;
        dm.vertexCount = tris * 3;
        dm.triangleCount = tris;
        dm.vertices = static_cast<float*>(
            MemAlloc(sizeof(float) * 3 * static_cast<unsigned>(dm.vertexCount)));
        dm.normals = static_cast<float*>(
            MemAlloc(sizeof(float) * 3 * static_cast<unsigned>(dm.vertexCount)));
        int w = 0;
        const auto put = [&](float x, float y, float nz) {
            dm.vertices[w * 3 + 0] = x;
            dm.vertices[w * 3 + 1] = y;
            dm.vertices[w * 3 + 2] = 0.0f;
            dm.normals[w * 3 + 0] = 0.0f;
            dm.normals[w * 3 + 1] = 0.0f;
            dm.normals[w * 3 + 2] = nz;
            ++w;
        };
        for (int k = 0; k < kSeg; ++k) {
            const double a0 = 2.0 * 3.14159265358979323846 * k / kSeg;
            const double a1 = 2.0 * 3.14159265358979323846 * (k + 1) / kSeg;
            const float x0 = static_cast<float>(std::cos(a0));
            const float y0 = static_cast<float>(std::sin(a0));
            const float x1 = static_cast<float>(std::cos(a1));
            const float y1 = static_cast<float>(std::sin(a1));
            put(0.0f, 0.0f, 1.0f);  put(x0, y0, 1.0f);  put(x1, y1, 1.0f);
            put(0.0f, 0.0f, -1.0f); put(x1, y1, -1.0f); put(x0, y0, -1.0f);
        }
        UploadMesh(&dm, false);
        sm.disc_ok = true;
    }
    // Default-shader material, neutral dark, per-draw alpha on the diffuse
    // colour -- g_fleet.prop_mat's idiom verbatim (render/draw.cpp:976).
    sm.disc_mat = LoadMaterialDefault();
    // A little above the Bf 109's {40,44,50}: that disc is read against a
    // daylit sky, this one is often a small machine against a night treeline,
    // and a near-black disc on a black sky is a disc nobody can see.
    sm.disc_mat.maps[MATERIAL_MAP_DIFFUSE].color = Color{58, 64, 74, 255};
    sm.mat = LoadMaterialDefault();
    sm.ok = !sm.prims.empty();
    TraceLog(sm.ok ? LOG_INFO : LOG_WARNING,
             "STING: '%s' loaded (%d prims, %d nodes)", path,
             static_cast<int>(sm.prims.size()), static_cast<int>(nn));
    return sm.ok;
}

Matrix to_ray_m(const glm::mat4& g) {
    // raylib Matrix is row-major fields m0..m15 with column-major storage
    // convention matching glm's memory layout transposed; MatrixTranspose of
    // a memcpy equals this direct field mapping (the flak_model.cpp idiom).
    Matrix m;
    m.m0 = g[0][0]; m.m1 = g[0][1]; m.m2 = g[0][2]; m.m3 = g[0][3];
    m.m4 = g[1][0]; m.m5 = g[1][1]; m.m6 = g[1][2]; m.m7 = g[1][3];
    m.m8 = g[2][0]; m.m9 = g[2][1]; m.m10 = g[2][2]; m.m11 = g[2][3];
    m.m12 = g[3][0]; m.m13 = g[3][1]; m.m14 = g[3][2]; m.m15 = g[3][3];
    return m;
}

// Kill switch: SEADS_STING_MODEL=0 forces "not ready" (the flak_gunner_enabled
// idiom, render/flak_gunner.cpp:623-628 -- static-init getenv read, cached
// once per process).
bool env_enabled() {
    static const bool on = [] {
        const char* e = std::getenv("SEADS_STING_MODEL");
        return e == nullptr || std::strcmp(e, "0") != 0;
    }();
    return on;
}

}  // namespace

bool sting_model_ready() {
    if (!env_enabled()) return false;
    StingModel& sm = g_sting;
    if (!sm.tried) {
        sm.tried = true;
        load_model();
    }
    return sm.ok;
}

bool sting_model_draw(const glm::dvec3& pos, const glm::dmat3& orient,
                      const glm::dvec3& eye, float ambient, float spin,
                      float rate_rad_s) {
    if (!env_enabled()) return false;
    StingModel& sm = g_sting;
    if (!sm.tried) {
        sm.tried = true;
        load_model();
    }
    if (!sm.ok) return false;

    // World transforms. Every node is at its rest transform except the four
    // props, whose local gets the spin folded in about the model's glTF Z.
    // Composed as T * R_rest * R(spin) * S -- the spin is applied in the
    // node's OWN frame (after its rest rotation), which is the only ordering
    // that stays correct if a future export gives a prop a rest tilt. The
    // shipped asset has none, so today R_rest is identity and this is exactly
    // the T * R(spin) * S the ask names.
    const double sp = static_cast<double>(spin);
    const bool spinning = std::abs(sp) > 0.0;
    for (const int i : sm.order) {
        SNode& nd = sm.nodes[i];
        glm::dmat4 local = glm::translate(glm::dmat4(1.0), nd.t) *
                           glm::mat4_cast(nd.r);
        if (spinning && nd.spin_sign != 0) {
            local = local * glm::rotate(glm::dmat4(1.0),
                                        sp * static_cast<double>(nd.spin_sign),
                                        glm::dvec3(0.0, 0.0, 1.0));
        }
        local = local * glm::scale(glm::dmat4(1.0), nd.s);
        nd.world = nd.parent >= 0 ? sm.nodes[nd.parent].world * local : local;
    }

    // mount: model -> eye-relative world (double until the subtraction, the
    // R = 15 km float rule). Rotation columns are `orient`'s own columns
    // (right, up, forward) verbatim -- the caller already built them from
    // glm::mat3_cast(sting_draw.orientation), the same idiom flak_gun.h's
    // make_mount_frame composes from (up, fwd0).
    const glm::dvec3 rel = pos - eye;
    glm::dmat4 mount(1.0);
    mount[0] = glm::dvec4(orient[0], 0.0);
    mount[1] = glm::dvec4(orient[1], 0.0);
    mount[2] = glm::dvec4(orient[2], 0.0);
    mount[3] = glm::dvec4(rel, 1.0);

    // ★★★ HIS OWN SIDE'S HUE, ABSOLUTE (Chad 2026-09-10: "Sudbury always has
    // to be the orange team"). This RPAS is the PLAYER'S, so it wears the
    // colour of the side he chose at the spawn menu -- ally blue flying for
    // the Valley (unchanged, bit for bit, from what shipped) and the slag
    // orange flying for Central City. render::player_team() is the session's
    // one answer (render/team_kit.h); this is a per-frame draw, so it follows
    // the choice with no rebuild.
    const glm::dvec3 ab = render::faction_color(render::player_team());
    const auto q = [ambient](double v) {
        const double a = std::min(1.0, std::max(0.0, v)) *
                         std::min(1.0f, std::max(0.0f, ambient));
        return static_cast<unsigned char>(std::lround(a * 255.0));
    };
    const Color ally_tint{q(ab.x), q(ab.y), q(ab.z), 255};

    // ★ HOW FULL THE DISC IS. 0 below kBlurOnRadS (individual blades, no
    // disc -- the rail spin-up and a slow hover), ramping to kBlurMaxAlpha by
    // kBlurFullRadS. Smoothstep, not linear: a linear ramp makes the disc
    // appear with a visible edge the moment the throttle cracks off idle.
    // ★ ST-5 prop/audio round (Chad: "blades to appear to move even faster at
    // the top end"). The two rate ends still bracket the TRUE spin law exactly
    // (render/sting_audio.h's 40 -> 150 rad/s), and with the speed band's
    // floor now at 150 m/s the drone at the S key sits ON kBlurOnRadS -- so
    // the loiter draws bare blades with no disc at all, which is the "see the
    // blades seemingly spinning slower" half of the ruling, for free.
    // The CEILING is what moves: 0.45 -> 0.55, so the pinned disc is denser
    // and the top of the throttle reads hotter than cruise. Deliberately not
    // pushed further -- past ~0.6 the disc starts hiding the airframe behind
    // it, and he asked for the blades to stay apparent, not for a grey plate.
    constexpr double kBlurOnRadS = 40.0;    // = the flying idle/loiter rate
    constexpr double kBlurFullRadS = 150.0; // = pinned
    constexpr double kBlurMaxAlpha = 0.55;
    const double rr = static_cast<double>(rate_rad_s);
    double blur = (rr - kBlurOnRadS) / (kBlurFullRadS - kBlurOnRadS);
    blur = std::min(1.0, std::max(0.0, blur));
    // CONCAVE, not smoothstep. Smoothstep was the first cut and it was wrong
    // for this job: it spends the first third of the band near zero, so a
    // 60 rad/s drone -- a real cruising speed -- drew a 9/255 disc that was
    // invisible against a night sky, and the "visibly vary" Chad asked for did
    // not start until the top of the throttle. x^0.6 rises fast off the
    // threshold and still separates 60 / 90 / 150 clearly (0.36 / 0.62 / 1.0).
    blur = std::pow(blur, 0.6);

    for (const SPrim& sp : sm.prims) {
        Color c = sp.dark ? sp.col : ally_tint;
        // Chad: the blades must "still be apparent". So they are never faded
        // out -- they are TINTED toward the disc's own colour as it fills in,
        // to at most kBladeSink of the way. A real prop's blades do not
        // vanish, they stop being separable from the disc; and a colour lerp
        // does that without asking the opaque pass to alpha-blend (which the
        // depth-sorted body pass is not set up for).
        if (sm.nodes[sp.node].spin_sign != 0 && blur > 0.0) {
            constexpr double kBladeSink = 0.55;
            const double f = blur * kBladeSink;
            const auto lerp8 = [f](unsigned char a, unsigned char b) {
                return static_cast<unsigned char>(
                    std::lround(a + (static_cast<double>(b) - a) * f));
            };
            const Color d = sm.disc_mat.maps[MATERIAL_MAP_DIFFUSE].color;
            c = Color{lerp8(c.r, d.r), lerp8(c.g, d.g), lerp8(c.b, d.b), 255};
        }
        sm.mat.maps[MATERIAL_MAP_DIFFUSE].color = c;
        const glm::mat4 xf = glm::mat4(mount * sm.nodes[sp.node].world);
        DrawMesh(sp.mesh, sm.mat, to_ray_m(xf));
    }

    // ★ THE TRANSLUCENT PASS, AFTER every opaque prim of this model (Fable C6,
    // render/draw.cpp:3499): alpha on, depth-WRITE off, so four overlapping
    // discs and the fuselage behind them composite instead of punching each
    // other out. Wrapped here rather than at the call sites because the Sting
    // has two of those and a blend mode left open by one of them would leak
    // into whatever drew next.
    if (blur > 0.0 && sm.disc_ok) {
        Color& dc = sm.disc_mat.maps[MATERIAL_MAP_DIFFUSE].color;
        dc.a = static_cast<unsigned char>(
            std::lround(std::min(1.0, blur * kBlurMaxAlpha) * 255.0));
        BeginBlendMode(BLEND_ALPHA);
        rlDisableDepthMask();  // depth TEST stays on; WRITE off
        for (const SNode& nd : sm.nodes) {
            if (nd.spin_sign == 0 || !(nd.tip_r > 0.0)) continue;
            // The disc rides the node's world transform (hub position, and any
            // rest tilt the prop has) but NOT its spin: a disc of revolution is
            // invariant under its own rotation, so spinning it would be work
            // that cannot be seen. sm.nodes[].world already carries the spin,
            // and that is harmless for exactly that reason.
            const glm::dmat4 xf =
                mount * nd.world *
                glm::scale(glm::dmat4(1.0), glm::dvec3(nd.tip_r, nd.tip_r, 1.0));
            DrawMesh(sm.disc, sm.disc_mat, to_ray_m(glm::mat4(xf)));
        }
        rlEnableDepthMask();
        EndBlendMode();
    }
    return true;
}

}  // namespace render
