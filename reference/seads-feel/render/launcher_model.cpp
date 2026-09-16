// THE GUNSTOCK LAUNCHER, DRAWN -- see render/launcher_model.h for the
// contract. Cloned from render/sting_model.cpp (itself the flak_model clone);
// the one addition is the station reader, which is flak_stations()'s.
#include "render/launcher_model.h"

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

struct LNode {
    std::string name;
    glm::dvec3 t{0.0};
    glm::dquat r{1.0, 0.0, 0.0, 0.0};
    glm::dvec3 s{1.0};
    int parent = -1;
    glm::dmat4 world{1.0};
};

struct LPrim {
    Mesh mesh{};
    Color col{255, 255, 255, 255};  // as authored in the GLB
    bool dark = false;              // keep `col` as-is, never team-tint it
    int node = -1;
};

struct LauncherModel {
    bool tried = false;
    bool ok = false;
    std::vector<LNode> nodes;
    std::vector<int> order;  // parents before children
    std::vector<LPrim> prims;
    Material mat{};
    sting::Stations st;  // NOMINAL until the GLB overwrites it
};

LauncherModel g_launcher;

bool read_floats(const cgltf_accessor* a, int comps, std::vector<float>& out) {
    if (a == nullptr) return false;
    out.resize(a->count * static_cast<std::size_t>(comps));
    for (cgltf_size i = 0; i < a->count; ++i)
        if (cgltf_accessor_read_float(a, i, &out[i * comps], comps) == 0)
            return false;
    return true;
}

// The livery rule sting_model.cpp documents, applied unchanged: ONLY a
// material whose name contains "body" wears the quantized ally tint. The
// stock, the optic, the carbon rail and the grip rubber keep their authored
// colours -- accents should read as material, not as paint.
bool is_dark_material(const cgltf_material* m) {
    if (m == nullptr || m->name == nullptr) return true;  // unnamed: keep
    std::string n = m->name;
    for (char& c : n) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return n.find("body") == std::string::npos;
}

int find_node(const LauncherModel& lm, const char* name) {
    for (std::size_t i = 0; i < lm.nodes.size(); ++i)
        if (lm.nodes[i].name == name) return static_cast<int>(i);
    return -1;
}

// Rest-pose world transforms. The launcher is RIGID: composed once at load,
// never per frame.
void compose_rest(LauncherModel& lm) {
    for (const int i : lm.order) {
        LNode& nd = lm.nodes[i];
        const glm::dmat4 local = glm::translate(glm::dmat4(1.0), nd.t) *
                                 glm::mat4_cast(nd.r) *
                                 glm::scale(glm::dmat4(1.0), nd.s);
        nd.world = nd.parent >= 0 ? lm.nodes[nd.parent].world * local : local;
    }
}

// FAIL SOFT, AND NAME WHAT IS MISSING. A launcher whose st_shoulder never
// exported would otherwise pose the buttplate at the model origin and weld
// the stock into the middle of his chest -- a silently wrong pose, which is
// worse than no pose at all.
bool read_stations(LauncherModel& lm) {
    const auto at = [&](const char* n, glm::dvec3& v) {
        const int i = find_node(lm, n);
        if (i < 0) {
            TraceLog(LOG_WARNING,
                     "LAUNCHER: station '%s' missing -- launcher not ready", n);
            return false;
        }
        v = glm::dvec3(lm.nodes[i].world[3]);
        return true;
    };
    sting::Stations st;
    bool ok = true;
    ok &= at("st_grip_r", st.grip_r);
    ok &= at("st_grip_l", st.grip_l);
    ok &= at("st_shoulder", st.shoulder);
    ok &= at("st_sight", st.sight);
    ok &= at("st_muzzle", st.muzzle);
    if (!ok) return false;
    st.from_glb = true;
    lm.st = st;
    return true;
}

bool load_model() {
    LauncherModel& lm = g_launcher;
    char path[512];
    std::snprintf(path, sizeof path, "%s/drone/launcher.glb", SEADS_ASSET_DIR);
    cgltf_options opt{};
    cgltf_data* data = nullptr;
    if (cgltf_parse_file(&opt, path, &data) != cgltf_result_success ||
        cgltf_load_buffers(&opt, data, path) != cgltf_result_success) {
        TraceLog(LOG_WARNING,
                 "LAUNCHER: %s missing/unreadable -- no launcher drawn", path);
        if (data != nullptr) cgltf_free(data);
        return false;
    }
    const std::size_t nn = data->nodes_count;
    lm.nodes.resize(nn);
    for (std::size_t i = 0; i < nn; ++i) {
        const cgltf_node& n = data->nodes[i];
        LNode& out = lm.nodes[i];
        out.name = n.name != nullptr ? n.name : "";
        if (n.has_translation)
            out.t = {n.translation[0], n.translation[1], n.translation[2]};
        if (n.has_rotation)
            out.r = glm::dquat(n.rotation[3], n.rotation[0], n.rotation[1],
                               n.rotation[2]);
        if (n.has_scale) out.s = {n.scale[0], n.scale[1], n.scale[2]};
        for (std::size_t c = 0; c < n.children_count; ++c)
            lm.nodes[n.children[c] - data->nodes].parent = static_cast<int>(i);
    }
    // parents-before-children (glTF array order is not topological)
    lm.order.reserve(nn);
    {
        std::vector<char> placed(nn, 0);
        bool grew = true;
        while (lm.order.size() < nn && grew) {
            grew = false;
            for (std::size_t i = 0; i < nn; ++i) {
                if (placed[i]) continue;
                const int p = lm.nodes[i].parent;
                if (p < 0 || placed[p]) {
                    lm.order.push_back(static_cast<int>(i));
                    placed[i] = 1;
                    grew = true;
                }
            }
        }
    }
    compose_rest(lm);
    const bool stations_ok = read_stations(lm);
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
            const bool has_n = read_floats(nrm, 3, vn);
            LPrim sp;
            sp.node = static_cast<int>(i);
            sp.dark = is_dark_material(pr.material);
            Mesh& m = sp.mesh;
            m.vertexCount = static_cast<int>(pos->count);
            m.triangleCount = pr.indices != nullptr
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
                sp.col = Color{
                    static_cast<unsigned char>(std::lround(c[0] * 255.0f)),
                    static_cast<unsigned char>(std::lround(c[1] * 255.0f)),
                    static_cast<unsigned char>(std::lround(c[2] * 255.0f)),
                    255};
            }
            UploadMesh(&m, false);
            lm.prims.push_back(sp);
        }
    }
    cgltf_free(data);
    lm.mat = LoadMaterialDefault();
    lm.ok = !lm.prims.empty() && stations_ok;
    TraceLog(lm.ok ? LOG_INFO : LOG_WARNING,
             "LAUNCHER: '%s' loaded (%d prims, %d nodes, stations %s)", path,
             static_cast<int>(lm.prims.size()), static_cast<int>(nn),
             stations_ok ? "ok" : "INCOMPLETE");
    if (lm.ok)
        TraceLog(LOG_INFO,
                 "LAUNCHER: st_muzzle (%.3f %.3f %.3f) st_shoulder "
                 "(%.3f %.3f %.3f) -- muzzle %.3f m ahead of the buttplate",
                 lm.st.muzzle.x, lm.st.muzzle.y, lm.st.muzzle.z,
                 lm.st.shoulder.x, lm.st.shoulder.y, lm.st.shoulder.z,
                 lm.st.shoulder.z - lm.st.muzzle.z);
    return lm.ok;
}

Matrix to_ray_m(const glm::mat4& g) {
    // raylib Matrix field mapping, the flak_model.cpp idiom.
    Matrix m;
    m.m0 = g[0][0]; m.m1 = g[0][1]; m.m2 = g[0][2]; m.m3 = g[0][3];
    m.m4 = g[1][0]; m.m5 = g[1][1]; m.m6 = g[1][2]; m.m7 = g[1][3];
    m.m8 = g[2][0]; m.m9 = g[2][1]; m.m10 = g[2][2]; m.m11 = g[2][3];
    m.m12 = g[3][0]; m.m13 = g[3][1]; m.m14 = g[3][2]; m.m15 = g[3][3];
    return m;
}

// Kill switches, cached once per process (the flak_gunner_enabled idiom).
// SEADS_STING_MODEL=0 takes the drone AND the launcher; SEADS_STING_LAUNCHER=0
// takes the launcher alone.
bool env_enabled() {
    static const bool on = [] {
        const char* a = std::getenv("SEADS_STING_MODEL");
        const char* b = std::getenv("SEADS_STING_LAUNCHER");
        return (a == nullptr || std::strcmp(a, "0") != 0) &&
               (b == nullptr || std::strcmp(b, "0") != 0);
    }();
    return on;
}

bool ensure_loaded() {
    if (!env_enabled()) return false;
    LauncherModel& lm = g_launcher;
    if (!lm.tried) {
        lm.tried = true;
        load_model();
    }
    return lm.ok;
}

}  // namespace

bool launcher_model_ready() { return ensure_loaded(); }

const sting::Stations& launcher_stations() {
    ensure_loaded();
    return g_launcher.st;
}

bool launcher_model_draw(const glm::dvec3& pos, const glm::dmat3& basis,
                         const glm::dvec3& eye, float ambient) {
    if (!ensure_loaded()) return false;
    LauncherModel& lm = g_launcher;

    // mount: model -> eye-relative world (double until the subtraction, the
    // R = 15 km float rule). Columns are `basis`'s own, verbatim.
    const glm::dvec3 rel = pos - eye;
    glm::dmat4 mount(1.0);
    mount[0] = glm::dvec4(basis[0], 0.0);
    mount[1] = glm::dvec4(basis[1], 0.0);
    mount[2] = glm::dvec4(basis[2], 0.0);
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

    for (const LPrim& sp : lm.prims) {
        lm.mat.maps[MATERIAL_MAP_DIFFUSE].color = sp.dark ? sp.col : ally_tint;
        const glm::mat4 xf = glm::mat4(mount * lm.nodes[sp.node].world);
        DrawMesh(sp.mesh, lm.mat, to_ray_m(xf));
    }
    return true;
}

}  // namespace render
