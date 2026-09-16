// THE FLAK GUN, DRAWN -- see render/flak_model.h for the contract.
#include "render/flak_model.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "raylib.h"

// cgltf: the implementation is compiled inside raylib's rmodels.c (C
// linkage, non-static) -- declare only, never define CGLTF_IMPLEMENTATION
// here or the link gets duplicate symbols (the sled_model.cpp idiom).
extern "C" {
#include "external/cgltf.h"
}

namespace render {
namespace {

struct FNode {
    std::string name;
    glm::dvec3 t{0.0};
    glm::dquat r{1.0, 0.0, 0.0, 0.0};
    glm::dvec3 s{1.0};
    int parent = -1;
    glm::dmat4 world{1.0};
};

struct FPrim {
    Mesh mesh{};
    Color col{255, 255, 255, 255};
    int node = -1;
};

struct FlakModel {
    bool tried = false;
    bool ok = false;
    std::vector<FNode> nodes;
    std::vector<int> order;  // parents before children
    std::vector<FPrim> prims;
    int n_train = -1, n_cradle = -1;
    // the recoiling assembly (barrel+receiver, drum, grips -- what slides
    // on the real gun); sight + shoulder rest stay on the cradle
    int n_gunmesh = -1, n_drum = -1, n_grips = -1;
    // STAGE C drum swap: which way is OUTBOARD along the drum's bracket,
    // MEASURED off the loaded st_drum node rather than hardcoded (+1 in the
    // shipped GLB, where the drum is offset to the RIGHT). A re-export that
    // moves the drum to the other side moves the animation with it.
    double drum_out_sign = 1.0;
    Material mat{};
};

FlakModel g_flak;

int find_node(const FlakModel& fm, const char* name) {
    for (std::size_t i = 0; i < fm.nodes.size(); ++i)
        if (fm.nodes[i].name == name) return static_cast<int>(i);
    return -1;
}

// Read a float VEC3/scalar accessor into out (n * comps floats).
bool read_floats(const cgltf_accessor* a, int comps, std::vector<float>& out) {
    if (a == nullptr) return false;
    out.resize(a->count * static_cast<std::size_t>(comps));
    for (cgltf_size i = 0; i < a->count; ++i)
        if (cgltf_accessor_read_float(a, i, &out[i * comps], comps) == 0)
            return false;
    return true;
}

bool load_model() {
    FlakModel& fm = g_flak;
    char path[512];
    std::snprintf(path, sizeof path, "%s/flak/oerlikon_mk4.glb",
                  SEADS_ASSET_DIR);
    cgltf_options opt{};
    cgltf_data* data = nullptr;
    if (cgltf_parse_file(&opt, path, &data) != cgltf_result_success ||
        cgltf_load_buffers(&opt, data, path) != cgltf_result_success) {
        TraceLog(LOG_WARNING, "FLAK: %s missing/unreadable -- gun not drawn",
                 path);
        if (data != nullptr) cgltf_free(data);
        return false;
    }
    const std::size_t nn = data->nodes_count;
    fm.nodes.resize(nn);
    for (std::size_t i = 0; i < nn; ++i) {
        const cgltf_node& n = data->nodes[i];
        FNode& out = fm.nodes[i];
        out.name = n.name != nullptr ? n.name : "";
        if (n.has_translation)
            out.t = {n.translation[0], n.translation[1], n.translation[2]};
        if (n.has_rotation)
            out.r = glm::dquat(n.rotation[3], n.rotation[0], n.rotation[1],
                               n.rotation[2]);
        if (n.has_scale) out.s = {n.scale[0], n.scale[1], n.scale[2]};
        for (std::size_t c = 0; c < n.children_count; ++c)
            fm.nodes[n.children[c] - data->nodes].parent = static_cast<int>(i);
    }
    // parents-before-children (glTF array order is not topological)
    fm.order.reserve(nn);
    {
        std::vector<char> placed(nn, 0);
        bool grew = true;
        while (fm.order.size() < nn && grew) {
            grew = false;
            for (std::size_t i = 0; i < nn; ++i) {
                if (placed[i]) continue;
                const int p = fm.nodes[i].parent;
                if (p < 0 || placed[p]) {
                    fm.order.push_back(static_cast<int>(i));
                    placed[i] = 1;
                    grew = true;
                }
            }
        }
    }
    fm.n_train = find_node(fm, flak::kNodeTrain);
    fm.n_cradle = find_node(fm, flak::kNodeCradle);
    fm.n_gunmesh = find_node(fm, "flak_gun");
    fm.n_drum = find_node(fm, "flak_drum");
    fm.n_grips = find_node(fm, "flak_grips");
    {
        // st_drum carries the drum's authored offset from the centreline; its
        // sign IS the outboard direction. Fall back to +1 (the shipped side)
        // if the station is missing or dead on the centreline.
        const int sd = find_node(fm, "st_drum");
        if (sd >= 0 && fm.nodes[sd].t.x < 0.0) fm.drum_out_sign = -1.0;
    }
    if (fm.n_train < 0 || fm.n_cradle < 0) {
        TraceLog(LOG_WARNING, "FLAK: driven nodes missing in %s -- not drawn",
                 path);
        cgltf_free(data);
        fm.nodes.clear();
        return false;
    }
    // meshes: one upload per primitive, flat base_color via lround (the
    // sled_model rule -- truncation darkens every tone by up to 1/255)
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
            FPrim fp;
            fp.node = static_cast<int>(i);
            Mesh& m = fp.mesh;
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
                fp.col = Color{static_cast<unsigned char>(
                                   std::lround(c[0] * 255.0f)),
                               static_cast<unsigned char>(
                                   std::lround(c[1] * 255.0f)),
                               static_cast<unsigned char>(
                                   std::lround(c[2] * 255.0f)),
                               255};
            }
            UploadMesh(&m, false);
            fm.prims.push_back(fp);
        }
    }
    cgltf_free(data);
    fm.mat = LoadMaterialDefault();
    fm.ok = !fm.prims.empty();
    TraceLog(fm.ok ? LOG_INFO : LOG_WARNING,
             "FLAK: '%s' loaded (%d prims, %d nodes)", path,
             static_cast<int>(fm.prims.size()), static_cast<int>(nn));
    return fm.ok;
}

Matrix to_ray_m(const glm::mat4& g) {
    // raylib Matrix is row-major fields m0..m15 with column-major storage
    // convention matching glm's memory layout transposed; MatrixTranspose of
    // a memcpy equals this direct field mapping.
    Matrix m;
    m.m0 = g[0][0]; m.m1 = g[0][1]; m.m2 = g[0][2]; m.m3 = g[0][3];
    m.m4 = g[1][0]; m.m5 = g[1][1]; m.m6 = g[1][2]; m.m7 = g[1][3];
    m.m8 = g[2][0]; m.m9 = g[2][1]; m.m10 = g[2][2]; m.m11 = g[2][3];
    m.m12 = g[3][0]; m.m13 = g[3][1]; m.m14 = g[3][2]; m.m15 = g[3][3];
    return m;
}

}  // namespace

bool flak_stations(flak::Stations& out) {
    FlakModel& fm = g_flak;
    if (!fm.tried) {
        fm.tried = true;
        load_model();
    }
    if (!fm.ok) return false;
    // rest-pose world transforms: compose the authored TRS once (no drive)
    for (const int i : fm.order) {
        FNode& nd = fm.nodes[i];
        const glm::dmat4 local = glm::translate(glm::dmat4(1.0), nd.t) *
                                 glm::mat4_cast(nd.r) *
                                 glm::scale(glm::dmat4(1.0), nd.s);
        nd.world = nd.parent >= 0 ? fm.nodes[nd.parent].world * local : local;
    }
    const auto at = [&](const char* n, glm::dvec3& v) {
        const int i = find_node(fm, n);
        if (i < 0) return false;
        v = glm::dvec3(fm.nodes[i].world[3]);
        return true;
    };
    bool ok = true;
    ok &= at("st_muzzle", out.muzzle);
    ok &= at("st_eye", out.eye);
    ok &= at("st_sight_rear", out.sight_rear);
    ok &= at("st_sight_front", out.sight_front);
    ok &= at("st_grip_l", out.grip_l);
    ok &= at("st_grip_r", out.grip_r);
    ok &= at("st_pad_l", out.pad_l);
    ok &= at("st_pad_r", out.pad_r);
    ok &= at("st_eject", out.eject);
    ok &= at("st_drum", out.drum);
    ok &= at("st_foot_l", out.foot_l);
    ok &= at("st_foot_r", out.foot_r);
    ok &= at("st_approach", out.approach);
    glm::dvec3 tr(0.0), tn(0.0);
    ok &= at(flak::kNodeCradle, tr);
    ok &= at(flak::kNodeTrain, tn);
    out.trunnion_h = tr.y;
    out.train_h = tn.y;
    return ok;
}

bool flak_model_draw(const FlakDraw& g, const glm::dvec3& eye) {
    FlakModel& fm = g_flak;
    if (!fm.tried) {
        fm.tried = true;
        load_model();
    }
    if (!fm.ok) return false;

    // STAGE C -- THE DRUM SWAP. Closed form off the reload fraction; see
    // render/flak_gun.h "STAGE C" for the three phases and the bracket-sign
    // ruling. reload_frac == 0 => `swap` is the identity AND the guard below
    // means the drum node's `t` is never even touched: bit-identical rest.
    const flak::DrumSwap swap = flak::drum_swap(g.reload_frac);
    const bool swapping = g.reload_frac > 0.0;

    // per-node locals: authored TRS, with the two driven nodes' rotation
    // composed from the pose (authored rotations are identity in this GLB,
    // but compose anyway so a re-export with a baked tilt keeps working)
    for (const int i : fm.order) {
        FNode& nd = fm.nodes[i];
        glm::dquat r = nd.r;
        if (i == fm.n_train) r = r * flak::train_quat(g.train_rad);
        if (i == fm.n_cradle) r = r * flak::cradle_quat(g.elev_rad);
        glm::dvec3 t = nd.t;
        // recoil: the sliding assembly kicks AFT (-Z) in cradle space
        if (g.recoil_m != 0.0 &&
            (i == fm.n_gunmesh || i == fm.n_drum || i == fm.n_grips))
            t.z -= g.recoil_m;
        // The drum comes off its bracket sideways and falls away.
        if (swapping && i == fm.n_drum) {
            t.x += fm.drum_out_sign * swap.out_m;
            t.y -= swap.down_m;
        }
        const glm::dmat4 local = glm::translate(glm::dmat4(1.0), t) *
                                 glm::mat4_cast(r) *
                                 glm::scale(glm::dmat4(1.0), nd.s);
        nd.world = nd.parent >= 0 ? fm.nodes[nd.parent].world * local : local;
    }

    // mount: model -> eye-relative world (double until the subtraction, the
    // R = 15 km float rule). Rotation columns = (right0, up, fwd0): the
    // model_dir_to_world map from flak_gun.h as a matrix.
    const flak::MountFrame f = flak::make_mount_frame(g.pos, g.up, g.fwd0);
    const glm::dvec3 rel = f.pos - eye;
    glm::dmat4 mount(1.0);
    mount[0] = glm::dvec4(f.right0, 0.0);
    mount[1] = glm::dvec4(f.up, 0.0);
    mount[2] = glm::dvec4(f.fwd0, 0.0);
    mount[3] = glm::dvec4(rel, 1.0);

    for (const FPrim& fp : fm.prims) {
        // Mid-swap the gun carries NO drum at all -- the feed throat is bare,
        // which is the whole read of "he is reloading" from outside.
        if (swapping && !swap.visible && fp.node == fm.n_drum) continue;
        fm.mat.maps[MATERIAL_MAP_DIFFUSE].color = fp.col;
        const glm::mat4 xf =
            glm::mat4(mount * fm.nodes[fp.node].world);
        DrawMesh(fp.mesh, fm.mat, to_ray_m(xf));
    }
    return true;
}

}  // namespace render
