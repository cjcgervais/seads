// THE SUDBURIAN ON THE GUN -- see render/flak_gunner.h for the contract.
#include "render/flak_gunner.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include "raylib.h"

#include "render/flak_pose.h"
#include "render/scarf_drape.h"
#include "render/trail_chain.h"

// cgltf: implementation lives in raylib's rmodels.c (the flak_model.cpp /
// sled_model.cpp idiom -- declare only).
extern "C" {
#include "external/cgltf.h"
}

namespace render {
namespace {

struct GNode {
    std::string name;
    glm::dvec3 t{0.0};
    glm::dquat r{1.0, 0.0, 0.0, 0.0};
    glm::dvec3 s{1.0};
    int parent = -1;
    glm::dmat4 rest{1.0};   // rest world (model frame)
    glm::dmat4 posed{1.0};  // posed world (gun model frame)
    bool has_pose = false;
};

struct GPrim {
    Mesh mesh{};
    Color col{200, 200, 200, 255};
    // ★ SCARF-DRAPE: material `sudburian_scarf_blue` -- the scarf's own prim.
    // The drawn-back scan must see the COAT, not the scarf (the sled bind's
    // rule: with the scarf included, the bunched wrap IS the farthest "back
    // surface" and the keep-out pushes the chain off its own fabric).
    bool is_scarf = false;
    std::vector<float> base_pos, base_nrm;
    std::vector<int> jidx;   // 4 per vert, into the SKIN JOINT list
    std::vector<float> jw;   // 4 per vert
};

struct GunnerModel {
    bool tried = false;
    bool ok = false;
    std::vector<GNode> nodes;
    std::vector<int> order;          // parents before children
    std::vector<int> joints;         // node index per skin joint
    std::vector<glm::dmat4> ibm;     // inverse bind, per skin joint
    std::vector<GPrim> prims;
    flak::GunnerAnthro anthro;       // MEASURED off the rig rest joints
    Material mat{};
    // ===== SCARF-DRAPE (2026-09-03): the solved scarf at the gun ==========
    // Same solver, same banded drawn-back planes, same pod boxes as the sled
    // rider (render/sled_model.cpp's scarf block is the reference wiring; the
    // constants are shared through render/scarf_drape.h).
    struct SVert {
        int prim = -1, vert = -1, ring = -1;
    };
    std::vector<SVert> back_verts;   // ring = torso band (equal-width by t)
    int back_n_rings = 0;
    std::vector<SVert> pod_verts;    // ring = long-chain bone index
    int scarf_node_id[kScarfSegments] = {-1, -1, -1, -1, -1, -1};
    int scarf_s_node_id[kScarfShortSegs] = {-1, -1};
    bool scarf_ok = false, scarf_s_ok = false, scarf_primed = false;
    TrailChainParams scarf_par{}, scarf_s_par{};
    TrailChainState scarf_st{}, scarf_s_st{};
    float scarf_worst = 1.0e9f;      // instrument latch
};

GunnerModel g_gunner;

int find_node(const GunnerModel& gm, const char* name) {
    for (std::size_t i = 0; i < gm.nodes.size(); ++i)
        if (gm.nodes[i].name == name) return static_cast<int>(i);
    return -1;
}

glm::dvec3 rest_pos(const GunnerModel& gm, int i) {
    return glm::dvec3(gm.nodes[i].rest[3]);
}

bool read_floats(const cgltf_accessor* a, int comps, std::vector<float>& out) {
    if (a == nullptr) return false;
    out.resize(a->count * static_cast<std::size_t>(comps));
    for (cgltf_size i = 0; i < a->count; ++i)
        if (cgltf_accessor_read_float(a, i, &out[i * comps], comps) == 0)
            return false;
    return true;
}


// ★★★ SCARF-DRAPE: measure everything the scarf solve needs off THIS load of
// the GLB, at rest -- the chain nodes, the segment lengths, the pods' own
// boxes in each bone's local frame (= the probe station frame, because
// trail_chain_frames writes bone k's world as [X,Y,Z | p[k]] and the pods are
// rigid one-joint skins), and the drawn-back band capture. This mirrors
// render/sled_model.cpp's bind (the reference wiring); the ruled constants
// come from render/scarf_drape.h so neither reader retypes the other.
void wire_scarf(GunnerModel& gm) {
    gm.scarf_ok = gm.scarf_s_ok = false;
    gm.back_n_rings = 0;
    char nm[16];
    for (int k = 0; k < kScarfSegments; ++k) {
        std::snprintf(nm, sizeof nm, "scarf_%02d", k + 1);
        gm.scarf_node_id[k] = find_node(gm, nm);
        if (gm.scarf_node_id[k] < 0) {
            TraceLog(LOG_WARNING,
                     "FLAK GUNNER: scarf bone %s missing -- scarf rides rigid",
                     nm);
            return;
        }
    }
    for (int k = 0; k < kScarfShortSegs; ++k) {
        std::snprintf(nm, sizeof nm, "scarf_s%02d", k + 1);
        gm.scarf_s_node_id[k] = find_node(gm, nm);
    }
    const int nkn = find_node(gm, "neck_01");
    const int pvn = find_node(gm, "pelvis");
    if (nkn < 0 || pvn < 0) return;
    // segment length: measured mean bone spacing x the pod-run ruling.
    {
        double sum = 0.0;
        for (int k = 1; k < kScarfSegments; ++k)
            sum += glm::length(gm.nodes[gm.scarf_node_id[k]].t);
        gm.scarf_par.segments = kScarfSegments;
        gm.scarf_par.seg_len_m = static_cast<float>(
            sum / double(kScarfSegments - 1) * kScarfRunSegFrac);
        gm.scarf_par.back_keepout_m = kScarfBackMarginM;
        gm.scarf_par.back_keepout_min_m = 0.002f;
    }
    // rest frame refs for the band capture
    const glm::dvec3 neck_r = rest_pos(gm, nkn);
    const glm::dvec3 pelvis_r = rest_pos(gm, pvn);
    const glm::dvec3 anchor_r = rest_pos(gm, gm.scarf_node_id[0]);
    const glm::dvec3 torso_r = neck_r - pelvis_r;
    const double torso_len = glm::length(torso_r);
    if (torso_len < 1.0e-4) return;
    const glm::dvec3 axis2 = torso_r / torso_len;
    glm::dvec3 b2 = anchor_r - neck_r;
    b2 -= axis2 * glm::dot(b2, axis2);
    if (glm::length(b2) < 1.0e-4) return;
    const glm::dvec3 back_n2 = glm::normalize(b2);
    const glm::dvec3 lat2 = glm::cross(axis2, back_n2);
    std::vector<glm::dmat4> jm_rest(gm.joints.size());
    for (std::size_t j = 0; j < gm.joints.size(); ++j)
        jm_rest[j] = gm.nodes[gm.joints[j]].rest * gm.ibm[j];
    // the drawn-back capture: same filters as the sled bind (t -0.05..1.05,
    // >= 0.09 m behind the neck, |lateral| <= 0.26), equal-width bands by t --
    // banding by t VALUE, never by tessellation gaps (the costume lesson).
    struct Cand {
        double t;
        int prim, vert;
    };
    std::vector<Cand> cand;
    for (std::size_t pi = 0; pi < gm.prims.size(); ++pi) {
        const GPrim& gp = gm.prims[pi];
        if (gp.is_scarf) continue;
        const int nv = gp.mesh.vertexCount;
        for (int v = 0; v < nv; ++v) {
            const glm::dvec3 pw(
                jm_rest[gp.jidx[4 * v]] *
                glm::dvec4(gp.base_pos[3 * v], gp.base_pos[3 * v + 1],
                           gp.base_pos[3 * v + 2], 1.0));
            const double t = glm::dot(pw - pelvis_r, axis2) / torso_len;
            if (t < -0.05 || t > 1.05) continue;
            if (glm::dot(pw - neck_r, back_n2) < 0.09) continue;
            if (std::abs(glm::dot(pw - neck_r, lat2)) > 0.26) continue;
            cand.push_back({t, static_cast<int>(pi), v});
        }
    }
    std::sort(cand.begin(), cand.end(),
              [](const Cand& a, const Cand& b) { return a.t < b.t; });
    gm.back_verts.clear();
    const int kRings = kTrailChainMaxBackPlanes + 1;
    if (!cand.empty() && cand.back().t - cand.front().t > 1.0e-4) {
        const double t_lo = cand.front().t;
        const double t_span = cand.back().t - t_lo;
        for (const Cand& c : cand) {
            int ring = static_cast<int>((c.t - t_lo) / t_span * kRings);
            if (ring >= kRings) ring = kRings - 1;
            if (ring < 0) ring = 0;
            gm.back_verts.push_back({c.prim, c.vert, ring});
        }
        gm.back_n_rings = kRings;
    }
    // the pods' own boxes, per bone, in bone-local space (L = ibm * v).
    glm::dvec3 lo[kScarfSegments + kScarfShortSegs];
    glm::dvec3 hi[kScarfSegments + kScarfShortSegs];
    bool any[kScarfSegments + kScarfShortSegs] = {};
    for (int k = 0; k < kScarfSegments + kScarfShortSegs; ++k) {
        lo[k] = glm::dvec3(1.0e9);
        hi[k] = glm::dvec3(-1.0e9);
    }
    gm.pod_verts.clear();
    for (std::size_t pi = 0; pi < gm.prims.size(); ++pi) {
        const GPrim& gp = gm.prims[pi];
        if (!gp.is_scarf) continue;
        const int nv = gp.mesh.vertexCount;
        for (int v = 0; v < nv; ++v) {
            // the MAX-WEIGHT joint, never slot 0 blind (glTF orders the four
            // slots arbitrarily, and this box is a constraint now).
            int jj = gp.jidx[4 * v];
            {
                float bw = gp.jw[4 * v];
                for (int q = 1; q < 4; ++q)
                    if (gp.jw[4 * v + q] > bw) {
                        bw = gp.jw[4 * v + q];
                        jj = gp.jidx[4 * v + q];
                    }
            }
            const int nd = gm.joints[jj];
            int bone = -1;
            for (int k = 0; k < kScarfSegments; ++k)
                if (nd == gm.scarf_node_id[k]) bone = k;
            for (int k = 0; k < kScarfShortSegs; ++k)
                if (nd == gm.scarf_s_node_id[k]) bone = kScarfSegments + k;
            if (bone < 0) continue;  // the wrap/knot: rigid to the neck
            const glm::dvec3 L(
                gm.ibm[jj] *
                glm::dvec4(gp.base_pos[3 * v], gp.base_pos[3 * v + 1],
                           gp.base_pos[3 * v + 2], 1.0));
            lo[bone] = glm::min(lo[bone], L);
            hi[bone] = glm::max(hi[bone], L);
            any[bone] = true;
            if (bone < kScarfSegments)
                gm.pod_verts.push_back({static_cast<int>(pi), v, bone});
        }
    }
    int wired = 0;
    for (int k = 0; k < kScarfSegments; ++k) {
        if (!any[k]) continue;
        TrailChainProbe& pb = gm.scarf_par.probe[k][0];
        pb.u_m = static_cast<float>(0.5 * (lo[k].x + hi[k].x));
        pb.v_m = static_cast<float>(0.5 * (lo[k].y + hi[k].y));
        pb.w_m = static_cast<float>(0.5 * (lo[k].z + hi[k].z));
        pb.hx_m = static_cast<float>(0.5 * (hi[k].x - lo[k].x));
        pb.hy_m = static_cast<float>(0.5 * (hi[k].y - lo[k].y));
        pb.hz_m = static_cast<float>(0.5 * (hi[k].z - lo[k].z));
        ++wired;
    }
    if (wired > 0) {
        gm.scarf_par.n_probe_stations = kScarfSegments;
    } else {
        // No pod boxes (scarf material renamed / pods unskinned): the
        // centreline-only fallback must keep the tube-half compensation the
        // sled's no-probe path keeps, or the fabric's inner half sits inside
        // the coat by construction (red-team finding 7).
        gm.scarf_par.back_keepout_m = kScarfBackMarginM + kScarfTubeHalfM;
        gm.scarf_par.back_keepout_min_m = kScarfTubeHalfM;
    }
    gm.scarf_ok = true;
    // the short tail: its own params (never a copy carrying the long table).
    if (gm.scarf_s_node_id[0] >= 0 && gm.scarf_s_node_id[1] >= 0) {
        gm.scarf_s_par = gm.scarf_par;
        gm.scarf_s_par.segments = kScarfShortSegs;
        gm.scarf_s_par.seg_len_m = static_cast<float>(
            glm::length(gm.nodes[gm.scarf_s_node_id[1]].t) *
            kScarfRunSegFrac);
        gm.scarf_s_par.damping = kScarfShortDamping;
        gm.scarf_s_par.n_probe_stations = 0;
        for (int k = 0; k <= kTrailChainMaxSegments; ++k)
            for (int s2 = 0; s2 < 2; ++s2)
                gm.scarf_s_par.probe[k][s2] = TrailChainProbe{};
        int wired_s = 0;
        for (int k = 0; k < kScarfShortSegs; ++k)
            if (any[kScarfSegments + k]) {
                TrailChainProbe& pb = gm.scarf_s_par.probe[k][0];
                const int b2i = kScarfSegments + k;
                pb.u_m = static_cast<float>(0.5 * (lo[b2i].x + hi[b2i].x));
                pb.v_m = static_cast<float>(0.5 * (lo[b2i].y + hi[b2i].y));
                pb.w_m = static_cast<float>(0.5 * (lo[b2i].z + hi[b2i].z));
                pb.hx_m = static_cast<float>(0.5 * (hi[b2i].x - lo[b2i].x));
                pb.hy_m = static_cast<float>(0.5 * (hi[b2i].y - lo[b2i].y));
                pb.hz_m = static_cast<float>(0.5 * (hi[b2i].z - lo[b2i].z));
                ++wired_s;
            }
        if (wired_s > 0) gm.scarf_s_par.n_probe_stations = kScarfShortSegs;
        gm.scarf_s_ok = true;
    }
    TraceLog(LOG_INFO,
             "FLAK GUNNER: scarf solve wired -- %d back verts in %d rings, "
             "%d pod boxes, seg_len %.4f m",
             static_cast<int>(gm.back_verts.size()), gm.back_n_rings, wired,
             static_cast<double>(gm.scarf_par.seg_len_m));
}

bool load_model() {
    GunnerModel& gm = g_gunner;
    char path[512];
    std::snprintf(path, sizeof path, "%s/sled/indy650.glb", SEADS_ASSET_DIR);
    cgltf_options opt{};
    cgltf_data* data = nullptr;
    if (cgltf_parse_file(&opt, path, &data) != cgltf_result_success ||
        cgltf_load_buffers(&opt, data, path) != cgltf_result_success) {
        TraceLog(LOG_WARNING,
                 "FLAK GUNNER: %s missing/unreadable -- gunner not drawn",
                 path);
        if (data != nullptr) cgltf_free(data);
        return false;
    }
    if (data->skins_count == 0) {
        TraceLog(LOG_WARNING, "FLAK GUNNER: no skin in %s", path);
        cgltf_free(data);
        return false;
    }
    const std::size_t nn = data->nodes_count;
    gm.nodes.resize(nn);
    for (std::size_t i = 0; i < nn; ++i) {
        const cgltf_node& n = data->nodes[i];
        GNode& out = gm.nodes[i];
        out.name = n.name != nullptr ? n.name : "";
        if (n.has_translation)
            out.t = {n.translation[0], n.translation[1], n.translation[2]};
        if (n.has_rotation)
            out.r = glm::dquat(n.rotation[3], n.rotation[0], n.rotation[1],
                               n.rotation[2]);
        if (n.has_scale) out.s = {n.scale[0], n.scale[1], n.scale[2]};
        for (std::size_t c = 0; c < n.children_count; ++c)
            gm.nodes[n.children[c] - data->nodes].parent = static_cast<int>(i);
    }
    gm.order.reserve(nn);
    {
        std::vector<char> placed(nn, 0);
        bool grew = true;
        while (gm.order.size() < nn && grew) {
            grew = false;
            for (std::size_t i = 0; i < nn; ++i) {
                if (placed[i]) continue;
                const int p = gm.nodes[i].parent;
                if (p < 0 || placed[p]) {
                    gm.order.push_back(static_cast<int>(i));
                    placed[i] = 1;
                    grew = true;
                }
            }
        }
    }
    for (const int i : gm.order) {
        GNode& nd = gm.nodes[i];
        const glm::dmat4 local = glm::translate(glm::dmat4(1.0), nd.t) *
                                 glm::mat4_cast(nd.r) *
                                 glm::scale(glm::dmat4(1.0), nd.s);
        nd.rest = nd.parent >= 0 ? gm.nodes[nd.parent].rest * local : local;
    }
    // --- the skin: joint list + inverse binds
    const cgltf_skin& sk = data->skins[0];
    gm.joints.resize(sk.joints_count);
    gm.ibm.resize(sk.joints_count);
    for (cgltf_size j = 0; j < sk.joints_count; ++j) {
        gm.joints[j] = static_cast<int>(sk.joints[j] - data->nodes);
        float m16[16];
        if (sk.inverse_bind_matrices == nullptr ||
            cgltf_accessor_read_float(sk.inverse_bind_matrices, j, m16, 16) ==
                0) {
            TraceLog(LOG_WARNING, "FLAK GUNNER: bad inverse binds in %s",
                     path);
            cgltf_free(data);
            return false;
        }
        glm::mat4 f;
        std::memcpy(&f[0][0], m16, sizeof m16);
        gm.ibm[j] = glm::dmat4(f);
    }
    // --- prims: ONLY the man. Skinned meshes on `sudburian_proxy` and the
    // PRISTINE helmet node; the dent variants are a sled crash nicety this
    // rung does not carry (named compromise, handoff).
    for (std::size_t i = 0; i < nn; ++i) {
        const cgltf_node& n = data->nodes[i];
        if (n.mesh == nullptr || n.skin == nullptr) continue;
        const std::string nm = n.name != nullptr ? n.name : "";
        // ★ Sudburian head (2026-09-04): the head patch's parts ride along so
        // the gunner is the same man as the rider — this allowlist is the one
        // place a new rider mesh is easy to miss (it is NOT automatic here).
        const bool head_part = nm == "sudburian_head_skin" ||
                               nm == "sudburian_teeth" ||
                               nm == "sudburian_scalp" ||
                               nm == "sudburian_goatee" ||
                               nm == "sudburian_mullet" ||
                               nm == "sudburian_glasses" ||
                               nm == "sudburian_lens_L" ||
                               nm == "sudburian_lens_R";
        if (nm != "sudburian_proxy" && nm != "helmet_sudburian" && !head_part)
            continue;
        for (cgltf_size pi = 0; pi < n.mesh->primitives_count; ++pi) {
            const cgltf_primitive& pr = n.mesh->primitives[pi];
            const cgltf_accessor *pos = nullptr, *nrm = nullptr,
                                 *jts = nullptr, *wts = nullptr;
            for (cgltf_size a = 0; a < pr.attributes_count; ++a) {
                switch (pr.attributes[a].type) {
                    case cgltf_attribute_type_position:
                        pos = pr.attributes[a].data;
                        break;
                    case cgltf_attribute_type_normal:
                        nrm = pr.attributes[a].data;
                        break;
                    case cgltf_attribute_type_joints:
                        jts = pr.attributes[a].data;
                        break;
                    case cgltf_attribute_type_weights:
                        wts = pr.attributes[a].data;
                        break;
                    default:
                        break;
                }
            }
            GPrim gp;
            if (!read_floats(pos, 3, gp.base_pos)) continue;
            const bool has_n = read_floats(nrm, 3, gp.base_nrm);
            if (jts == nullptr || wts == nullptr) continue;
            if (!read_floats(wts, 4, gp.jw)) continue;
            gp.jidx.resize(jts->count * 4);
            {
                cgltf_uint u4[4];
                for (cgltf_size v = 0; v < jts->count; ++v) {
                    if (cgltf_accessor_read_uint(jts, v, u4, 4) == 0) break;
                    for (int k = 0; k < 4; ++k)
                        gp.jidx[v * 4 + k] = static_cast<int>(u4[k]);
                }
            }
            Mesh& m = gp.mesh;
            m.vertexCount = static_cast<int>(pos->count);
            m.triangleCount = pr.indices != nullptr
                                  ? static_cast<int>(pr.indices->count / 3)
                                  : m.vertexCount / 3;
            m.vertices = static_cast<float*>(MemAlloc(
                sizeof(float) * 3 * static_cast<unsigned>(m.vertexCount)));
            std::memcpy(m.vertices, gp.base_pos.data(),
                        sizeof(float) * gp.base_pos.size());
            if (has_n) {
                m.normals = static_cast<float*>(MemAlloc(
                    sizeof(float) * 3 * static_cast<unsigned>(m.vertexCount)));
                std::memcpy(m.normals, gp.base_nrm.data(),
                            sizeof(float) * gp.base_nrm.size());
            }
            if (pr.indices != nullptr) {
                m.indices = static_cast<unsigned short*>(
                    MemAlloc(sizeof(unsigned short) *
                             static_cast<unsigned>(pr.indices->count)));
                for (cgltf_size k = 0; k < pr.indices->count; ++k)
                    m.indices[k] = static_cast<unsigned short>(
                        cgltf_accessor_read_index(pr.indices, k));
            }
            if (pr.material != nullptr) {
                if (pr.material->name != nullptr &&
                    std::strcmp(pr.material->name, "sudburian_scarf_blue") ==
                        0)
                    gp.is_scarf = true;
                if (pr.material->has_pbr_metallic_roughness) {
                    const float* c =
                        pr.material->pbr_metallic_roughness.base_color_factor;
                    gp.col = Color{
                        static_cast<unsigned char>(std::lround(c[0] * 255.0f)),
                        static_cast<unsigned char>(std::lround(c[1] * 255.0f)),
                        static_cast<unsigned char>(std::lround(c[2] * 255.0f)),
                        255};
                }
            }
            UploadMesh(&m, true);  // dynamic: CPU-skinned every draw
            gm.prims.push_back(std::move(gp));
        }
    }
    cgltf_free(data);
    // --- anthropometry, MEASURED off the rig rest joints (one-number rule)
    {
        const auto jp = [&](const char* n, glm::dvec3& v) {
            const int i = find_node(gm, n);
            if (i < 0) return false;
            v = rest_pos(gm, i);
            return true;
        };
        glm::dvec3 pelvis, ua_l, ua_r, la_l, hand_l, th_l, ca_l, ft_l, nk, hd;
        bool ok = true;
        ok &= jp("pelvis", pelvis);
        ok &= jp("upperarm_l", ua_l);
        ok &= jp("upperarm_r", ua_r);
        ok &= jp("lowerarm_l", la_l);
        ok &= jp("hand_l", hand_l);
        ok &= jp("thigh_l", th_l);
        ok &= jp("calf_l", ca_l);
        ok &= jp("foot_l", ft_l);
        ok &= jp("neck_01", nk);
        ok &= jp("head", hd);
        if (!ok) {
            TraceLog(LOG_WARNING, "FLAK GUNNER: rig joints missing in %s",
                     path);
            return false;
        }
        flak::GunnerAnthro& a = gm.anthro;
        a.torso_m = glm::length(0.5 * (ua_l + ua_r) - pelvis);
        a.thigh_m = glm::length(ca_l - th_l);
        a.calf_m = glm::length(ft_l - ca_l);
        a.uarm_m = glm::length(la_l - ua_l);
        a.farm_m = glm::length(hand_l - la_l);
        a.hip_half_m = std::abs(th_l.x - pelvis.x);
        // P1-10: the shoulders are solved now, so the rig's own shoulder
        // width places them (half the span between the upperarm joints).
        a.shoulder_half_m = 0.5 * glm::length(ua_l - ua_r);
        // Which side this rig calls LEFT, as a sign on model +X. The gun's
        // station suffixes use the opposite convention -- see
        // GunnerAnthro::left_sign; the pose pairs hand to pad by SIDE.
        a.left_sign = ua_l.x >= ua_r.x ? 1.0 : -1.0;
        a.neck_m = glm::length(hd - nk);
        // ★ FACE REACH, MEASURED (the one-number rule): how far the DRAWN
        // head extends forward of the head joint along the face axis -- the
        // helmet front. That point, not the joint, is what ends up in the
        // breech if the crane over-reaches, so the pose aims by it. Taken
        // over exactly the vertices this rig binds to the head bone, at rest
        // (base_pos IS the model-space bind position, since jm = posed * ibm
        // is the identity when posed == rest).
        {
            const int hi = find_node(gm, "head");
            if (hi >= 0) {
                const glm::dvec3 hp = rest_pos(gm, hi);
                const glm::dvec3 skull = hp - nk;
                glm::dvec3 face(0.0, 0.0, 1.0);
                const double sl2 = glm::dot(skull, skull);
                if (sl2 > 1e-12) {
                    const glm::dvec3 u = skull / std::sqrt(sl2);
                    const glm::dvec3 f = face - u * glm::dot(face, u);
                    if (glm::dot(f, f) > 1e-12) face = glm::normalize(f);
                }
                // ★ THROUGH THE BIND MATRICES, never off base_pos: this
                // rig does NOT author the head geometry in the head's rest
                // frame (the helmet's raw vertices sit ~0.10 m behind the
                // joint and the inverse-bind carries them onto the skull),
                // so a raw read measures a shape that is never drawn. This
                // is the SAME product the skin pass computes at rest,
                // jm = rest * ibm, so the number is the DRAWN helmet.
                std::vector<glm::dmat4> jrest(gm.joints.size());
                for (std::size_t j = 0; j < gm.joints.size(); ++j)
                    jrest[j] = gm.nodes[gm.joints[j]].rest * gm.ibm[j];
                double best = 0.0;
                for (const GPrim& gp : gm.prims) {
                    const std::size_t nv = gp.jw.size() / 4;
                    for (std::size_t v = 0; v < nv; ++v) {
                        int bj = -1;
                        float bw = 0.0f;
                        for (int k = 0; k < 4; ++k)
                            if (gp.jw[4 * v + k] > bw) {
                                bw = gp.jw[4 * v + k];
                                bj = gp.jidx[4 * v + k];
                            }
                        if (bj < 0 || bw <= 0.0f) continue;
                        if (bj >= static_cast<int>(gm.joints.size()) ||
                            gm.joints[bj] != hi)
                            continue;
                        const glm::dvec4 bp(gp.base_pos[3 * v],
                                            gp.base_pos[3 * v + 1],
                                            gp.base_pos[3 * v + 2], 1.0);
                        const glm::dvec3 pv(jrest[bj] * bp);
                        const double d = glm::dot(pv - hp, face);
                        if (d > best) best = d;
                    }
                }
                // A rig with no head-bound geometry (or a degenerate read)
                // keeps the declared default rather than collapsing to 0.
                if (best > 0.02) a.face_fwd_m = best;
                TraceLog(LOG_INFO,
                         "FLAK GUNNER: face_fwd_m = %.4f m (raw %.4f)",
                         a.face_fwd_m, best);
            }
        }
        // ankle_up_m keeps its declared default (NAMED COMPROMISE): the
        // sole is mesh, not a joint, and the rig's rest pose is SEATED with
        // the boots pitched on the running boards -- a sole-to-ankle height
        // measured there would be taken in the wrong pose (the
        // posed-not-rest law) and lie by the boot's rest pitch. If the
        // boot reads sunk or floating on the deck, this dial is the
        // suspect.
    }
    wire_scarf(gm);
    gm.mat = LoadMaterialDefault();
    gm.ok = !gm.prims.empty();
    TraceLog(gm.ok ? LOG_INFO : LOG_WARNING,
             "FLAK GUNNER: '%s' loaded (%d prims, %d joints)", path,
             static_cast<int>(gm.prims.size()),
             static_cast<int>(gm.joints.size()));
    return gm.ok;
}

// Shortest-arc rotation carrying unit-ish a onto unit-ish b.
glm::dquat arc_quat(glm::dvec3 a, glm::dvec3 b) {
    const double la = glm::length(a), lb = glm::length(b);
    if (la < 1e-12 || lb < 1e-12) return glm::dquat(1.0, 0.0, 0.0, 0.0);
    a /= la;
    b /= lb;
    const double d = glm::dot(a, b);
    if (d > 1.0 - 1e-12) return glm::dquat(1.0, 0.0, 0.0, 0.0);
    if (d < -1.0 + 1e-12) {
        // opposite: rotate pi about any axis orthogonal to a
        glm::dvec3 ax = glm::cross(a, glm::dvec3(1.0, 0.0, 0.0));
        if (glm::dot(ax, ax) < 1e-12)
            ax = glm::cross(a, glm::dvec3(0.0, 1.0, 0.0));
        return glm::angleAxis(flak::kPi, glm::normalize(ax));
    }
    const glm::dvec3 ax = glm::cross(a, b);
    return glm::normalize(glm::dquat(1.0 + d, ax.x, ax.y, ax.z));
}

// Pose one joint: solved position + shortest-arc(rest bone dir -> posed
// bone dir) composed onto the rest orientation.
void pose_joint(GunnerModel& gm, int node, const glm::dvec3& pos,
                const glm::dvec3& rest_aim, const glm::dvec3& posed_aim) {
    GNode& nd = gm.nodes[node];
    const glm::dmat3 rot =
        glm::mat3_cast(arc_quat(rest_aim, posed_aim)) * glm::dmat3(nd.rest);
    nd.posed = glm::dmat4(rot);
    nd.posed[3] = glm::dvec4(pos, 1.0);
    nd.has_pose = true;
}

Matrix to_ray_m(const glm::mat4& g) {
    Matrix m;
    m.m0 = g[0][0]; m.m1 = g[0][1]; m.m2 = g[0][2]; m.m3 = g[0][3];
    m.m4 = g[1][0]; m.m5 = g[1][1]; m.m6 = g[1][2]; m.m7 = g[1][3];
    m.m8 = g[2][0]; m.m9 = g[2][1]; m.m10 = g[2][2]; m.m11 = g[2][3];
    m.m12 = g[3][0]; m.m13 = g[3][1]; m.m14 = g[3][2]; m.m15 = g[3][3];
    return m;
}

}  // namespace

bool flak_gunner_enabled() {
    static const bool on = [] {
        const char* e = std::getenv("SEADS_FLAK_GUNNER");
        return e == nullptr || std::strcmp(e, "0") != 0;
    }();
    return on;
}

bool flak_gunner_ready() {
    if (!flak_gunner_enabled()) return false;
    GunnerModel& gm = g_gunner;
    if (!gm.tried) {
        gm.tried = true;
        load_model();
    }
    return gm.ok;
}

bool flak_gunner_draw(const FlakDraw& g, const glm::dvec3& eye, float alpha) {
    if (!(alpha > 0.0f)) return true;  // fully faded out: nothing to draw
    GunnerModel& gm = g_gunner;
    if (!gm.tried) {
        gm.tried = true;
        load_model();
    }
    if (!gm.ok) return false;
    flak::Stations st;
    if (!flak_stations(st)) return false;

    const flak::GunnerJoints gj =
        flak::gunner_solve(st, g.elev_rad, gm.anthro);

    // --- solved joints: name -> (position, aim point). The aim is the next
    // joint down the chain; leaves continue their parent's direction.
    // aim_name != nullptr: the bone points at that joint (rest dir taken
    // BETWEEN THE NAMED NODES at rest, posed dir between the solved
    // positions). aim_name == nullptr: a LEAF -- it continues its parent's
    // bone, so the posed aim extrapolates and the rest dir comes off the
    // node's own parent.
    struct Solved {
        const char* name;
        glm::dvec3 pos, aim;
        const char* aim_name;
    };
    const Solved table[] = {
        {"pelvis", gj.pelvis, gj.spine_01, "spine_01"},
        {"spine_01", gj.spine_01, gj.spine_02, "spine_02"},
        {"spine_02", gj.spine_02, gj.spine_03, "spine_03"},
        {"spine_03", gj.spine_03, gj.neck, "neck_01"},
        {"neck_01", gj.neck, gj.head, "head"},
        // "head" is NOT in this table -- its aim is the FACE, not the next
        // joint down the chain. See THE GAZE below.
        {"clavicle_l", gj.clavicle_l, gj.shoulder_l, "upperarm_l"},
        {"clavicle_r", gj.clavicle_r, gj.shoulder_r, "upperarm_r"},
        {"upperarm_l", gj.shoulder_l, gj.elbow_l, "lowerarm_l"},
        {"upperarm_r", gj.shoulder_r, gj.elbow_r, "lowerarm_r"},
        {"lowerarm_l", gj.elbow_l, gj.hand_l, "hand_l"},
        {"lowerarm_r", gj.elbow_r, gj.hand_r, "hand_r"},
        {"hand_l", gj.hand_l, gj.hand_l + (gj.hand_l - gj.elbow_l), nullptr},
        {"hand_r", gj.hand_r, gj.hand_r + (gj.hand_r - gj.elbow_r), nullptr},
        {"thigh_l", gj.hip_l, gj.knee_l, "calf_l"},
        {"thigh_r", gj.hip_r, gj.knee_r, "calf_r"},
        {"calf_l", gj.knee_l, gj.ankle_l, "foot_l"},
        {"calf_r", gj.knee_r, gj.ankle_r, "foot_r"},
        {"foot_l", gj.ankle_l, gj.ball_l, "ball_l"},
        {"foot_r", gj.ankle_r, gj.ball_r, "ball_r"},
        {"ball_l", gj.ball_l, gj.ball_l + (gj.ball_l - gj.ankle_l), nullptr},
        {"ball_r", gj.ball_r, gj.ball_r + (gj.ball_r - gj.ankle_r), nullptr},
    };
    for (GNode& nd : gm.nodes) nd.has_pose = false;
    for (const Solved& sv : table) {
        const int i = find_node(gm, sv.name);
        if (i < 0) continue;
        glm::dvec3 rest_aim(0.0);
        if (sv.aim_name != nullptr) {
            const int k = find_node(gm, sv.aim_name);
            if (k >= 0) rest_aim = rest_pos(gm, k) - rest_pos(gm, i);
        }
        if (glm::dot(rest_aim, rest_aim) < 1e-12) {
            // leaf: continue the parent's rest bone
            const int p = gm.nodes[i].parent;
            if (p >= 0) rest_aim = rest_pos(gm, i) - rest_pos(gm, p);
        }
        pose_joint(gm, i, sv.pos, rest_aim, sv.aim - sv.pos);
    }
    // ★★★ THE GAZE (Chad, 2026-08-31: "the sudburian should be looking
    // through the pipper, currently he looks at the ground"). Every other
    // bone here is aimed at the next joint down the chain, and for the head
    // -- a LEAF -- that meant aiming the SKULL AXIS (neck -> head) along the
    // crane direction toward st_eye. But the crane runs forward of and
    // barely above the shoulders, so aligning the top of his skull with it
    // laid the whole head over face-down: he stared at the deck at every
    // elevation. What has to point at the target is the FACE. So the head
    // takes the shortest arc carrying its REST face direction onto the SIGHT
    // AXIS (gj.look_dir, the rear-peep -> front-bead line carried through the
    // elevation) -- he looks down his own sight, through the ring and the
    // bead, at every elevation, and the arc is a pure pitch so no roll is
    // introduced. The head's POSITION is untouched: the crane onto st_eye is
    // P1-9's cheek-to-the-rest, and the helmet-clearance leg grades that
    // joint.
    {
        const int i = find_node(gm, "head");
        const int nk = find_node(gm, "neck_01");
        if (i >= 0 && nk >= 0) {
            // The rest face, MEASURED off the rig rather than declared: the
            // model frame's own forward (+Z -- the same convention the knee
            // splay and the spine bow already use) taken orthogonal to the
            // rig's rest skull axis, so a head modelled with any rest tilt
            // still yields a face square to its own skull.
            const glm::dvec3 skull =
                rest_pos(gm, i) - rest_pos(gm, nk);
            const glm::dvec3 fwd(0.0, 0.0, 1.0);
            glm::dvec3 rest_face = fwd;
            const double sl2 = glm::dot(skull, skull);
            if (sl2 > 1e-12) {
                const glm::dvec3 u = skull / std::sqrt(sl2);
                rest_face = fwd - u * glm::dot(fwd, u);
            }
            if (glm::dot(rest_face, rest_face) > 1e-12)
                pose_joint(gm, i, gj.head, rest_face, gj.look_dir);
            else  // skull axis parallel to forward: fall back to the chain
                pose_joint(gm, i, gj.head, skull, gj.head - gj.neck);
        }
    }
    // ★★★ THE SCARF HANGS OFF THE TORSO, NOT OFF THE GAZE (Chad, 2026-09-01:
    // "need to drop the scarf so that it runs along his body its still offset
    // a bit"). The rig parents both scarf chains -- scarf_01..06 and the short
    // scarf_s01..02 -- to `neck_01`, and this drawer AIMS neck_01 at the head.
    // So every degree the head cranes toward the sight swung the scarf with
    // it. Two changes, both minimal: the chain ROOT is carried by `spine_03`
    // (his chest, which is where a scarf actually hangs from), and it is
    // rotated so the AUTHORED TAIL DIRECTION points DOWN -- gravity, in the
    // gun model frame, which the mount maps to local down. The children rigid-
    // follow as before, so the scarf keeps every curve the art gave it; only
    // its hang changes.
    {
        const int sp = find_node(gm, "spine_03");
        if (sp >= 0 && gm.nodes[sp].has_pose) {
            const glm::dmat4 carry =
                gm.nodes[sp].posed * glm::inverse(gm.nodes[sp].rest);
            const glm::dvec3 down(0.0, -1.0, 0.0);
            // root, tip -- the tip gives the AUTHORED direction of the whole
            // tail, which is the thing being pointed at the ground.
            const char* chains[2][2] = {{"scarf_01", "scarf_06"},
                                        {"scarf_s01", "scarf_s02"}};
            for (const auto& ch : chains) {
                const int i = find_node(gm, ch[0]);
                const int t = find_node(gm, ch[1]);
                if (i < 0 || t < 0) continue;
                const glm::dvec3 pos(carry * glm::dvec4(rest_pos(gm, i), 1.0));
                pose_joint(gm, i, pos, rest_pos(gm, t) - rest_pos(gm, i), down);
            }
        }
    }
    // root rigid-follows the pelvis (it is the pelvis's PARENT in the rig,
    // so it cannot ride the ordinary parent-follow pass)
    {
        const int r = find_node(gm, "root");
        const int p = find_node(gm, "pelvis");
        if (r >= 0 && p >= 0 && gm.nodes[p].has_pose) {
            gm.nodes[r].posed = gm.nodes[p].posed *
                                glm::inverse(gm.nodes[p].rest) *
                                gm.nodes[r].rest;
            gm.nodes[r].has_pose = true;
        }
    }
    // everything else (twists, scarf chain, thumbs, mitt fronts, the FP
    // anchor) rigid-follows its posed parent, parents before children
    for (const int i : gm.order) {
        GNode& nd = gm.nodes[i];
        if (nd.has_pose) continue;
        const int p = nd.parent;
        if (p >= 0 && gm.nodes[p].has_pose) {
            nd.posed = gm.nodes[p].posed * glm::inverse(gm.nodes[p].rest) *
                       nd.rest;
            nd.has_pose = true;
        }
    }

    // ★★★ SCARF-DRAPE (2026-09-03): THE SCARF AT THE GUN IS SOLVED, NOT
    // RE-AIMED. Chad: "when he is standing at the flak gun and manning it,
    // part of the scarf distal to the knots go into the coat." The carry
    // block above places the ROOT; until now the whole authored tail rode it
    // RIGID, and rigid authored fabric knows nothing about the posed coat
    // under it. So: the same trailing-chain solve the sled rider uses -- same
    // solver, same banded drawn-back planes skinned off the POSED coat every
    // frame, same pod boxes -- in the gun model frame, where down is -Y (the
    // mount maps it to local down, exactly as the old rigid aim assumed).
    // Fixed sub-steps per draw, no clock read (house law): 240 on the first
    // frame so he is never seen with the chain mid-fall, 4 thereafter.
    if (gm.scarf_ok && gm.back_n_rings >= 2) {
        const int nkn = find_node(gm, "neck_01");
        const int pvn = find_node(gm, "pelvis");
        const int hdn = find_node(gm, "head");
        const bool frames_ok = nkn >= 0 && pvn >= 0 &&
                               gm.nodes[nkn].has_pose &&
                               gm.nodes[pvn].has_pose &&
                               gm.nodes[gm.scarf_node_id[0]].has_pose;
        TrailChainInput in;
        if (frames_ok) {
            const glm::vec3 down(0.0f, -1.0f, 0.0f);
            in.gravity_dir = down;
            const glm::dmat4& rw = gm.nodes[gm.scarf_node_id[0]].posed;
            in.anchor = glm::vec3(glm::dvec3(rw[3]));
            const glm::vec3 wx = glm::vec3(glm::dvec3(rw[0]));
            in.width_axis = glm::length(wx) > 1.0e-4f
                                ? glm::normalize(wx)
                                : glm::vec3(1.0f, 0.0f, 0.0f);
            const glm::vec3 neck_p(glm::dvec3(gm.nodes[nkn].posed[3]));
            const glm::vec3 pelvis_p(glm::dvec3(gm.nodes[pvn].posed[3]));
            in.back_origin = neck_p;
            in.back_normal = glm::vec3(0.0f);
            const glm::vec3 torso = neck_p - pelvis_p;
            if (glm::length(torso) > 1.0e-4f) {
                const glm::vec3 axv = glm::normalize(torso);
                glm::vec3 b = in.anchor - neck_p;
                b -= axv * glm::dot(b, axv);
                if (glm::length(b) > 1.0e-4f)
                    in.back_normal = glm::normalize(b);
            }
            // helmet sphere, up the posed head bone's own +Y (the joint is at
            // the base of the skull, the helmet is not).
            if (hdn >= 0 && gm.nodes[hdn].has_pose) {
                const glm::dmat4& hw = gm.nodes[hdn].posed;
                const glm::vec3 hy = glm::vec3(glm::dvec3(hw[1]));
                in.head_center =
                    glm::vec3(glm::dvec3(hw[3])) +
                    (glm::length(hy) > 1.0e-4f ? glm::normalize(hy)
                                               : glm::vec3(0.0f, 1.0f, 0.0f)) *
                        kScarfHeadCentreUpM;
            } else {
                in.head_center =
                    in.anchor + glm::vec3(0.0f, 1000.0f, 0.0f);  // inert
            }
            // ---- the banded drawn-back planes, re-skinned off the POSED
            // coat (the sled block's per-frame construction, adapted: ring
            // centroids give the normals, the origins are slid out to the
            // rearmost drawn vert in the scarf's own lateral strip).
            if (glm::length(in.back_normal) > 0.5f) {
                glm::vec3 cent[kTrailChainMaxBackPlanes + 1];
                int cnt[kTrailChainMaxBackPlanes + 1] = {};
                for (int r = 0; r <= kTrailChainMaxBackPlanes; ++r)
                    cent[r] = glm::vec3(0.0f);
                for (const GunnerModel::SVert& bv : gm.back_verts) {
                    const GPrim& gp = gm.prims[bv.prim];
                    const int jj = gp.jidx[4 * bv.vert];
                    const glm::vec3 pw(glm::dvec3(
                        gm.nodes[gm.joints[jj]].posed * gm.ibm[jj] *
                        glm::dvec4(gp.base_pos[3 * bv.vert],
                                   gp.base_pos[3 * bv.vert + 1],
                                   gp.base_pos[3 * bv.vert + 2], 1.0)));
                    cent[bv.ring] += pw;
                    ++cnt[bv.ring];
                }
                int nr = 0;
                for (int r = 0; r < gm.back_n_rings; ++r)
                    if (cnt[r] > 0) cent[nr++] = cent[r] / float(cnt[r]);
                if (nr >= 2) {
                    in.torso_origin = pelvis_p;
                    in.torso_axis = glm::normalize(neck_p - pelvis_p);
                    in.n_back_planes = nr - 1;
                    for (int b = 0; b + 1 < nr; ++b) {
                        const glm::vec3 e =
                            glm::normalize(cent[b + 1] - cent[b]);
                        glm::vec3 n =
                            in.back_normal - e * glm::dot(in.back_normal, e);
                        const float ln2 = glm::length(n);
                        n = ln2 > 1.0e-4f ? n / ln2 : in.back_normal;
                        in.bp_origin[b] = 0.5f * (cent[b] + cent[b + 1]);
                        in.bp_normal[b] = n;
                        in.bp_t[b] = glm::dot(cent[b] - in.torso_origin,
                                              in.torso_axis);
                    }
                    const glm::vec3 lat_b =
                        glm::cross(in.torso_axis, in.back_normal);
                    float push[kTrailChainMaxBackPlanes] = {};
                    for (const GunnerModel::SVert& bv : gm.back_verts) {
                        const GPrim& gp = gm.prims[bv.prim];
                        const int jj = gp.jidx[4 * bv.vert];
                        const glm::vec3 pw(glm::dvec3(
                            gm.nodes[gm.joints[jj]].posed * gm.ibm[jj] *
                            glm::dvec4(gp.base_pos[3 * bv.vert],
                                       gp.base_pos[3 * bv.vert + 1],
                                       gp.base_pos[3 * bv.vert + 2], 1.0)));
                        if (std::fabs(glm::dot(pw - in.back_origin, lat_b)) >
                            kScarfBackStripHalfM)
                            continue;
                        for (int b = 0; b + 1 < nr; ++b) {
                            if (bv.ring != b && bv.ring != b + 1) continue;
                            const float o = glm::dot(pw - in.bp_origin[b],
                                                     in.bp_normal[b]);
                            if (o > push[b]) push[b] = o;
                        }
                    }
                    for (int b = 0; b + 1 < nr; ++b)
                        in.bp_origin[b] += in.bp_normal[b] * push[b];
                    // ---- the anchor stand-off, pod-aware, multi-band,
                    // SHARED (trail_chain_anchor_need -- see its banner):
                    // bone 0's pod hangs off the PINNED root the constraint
                    // loop never grades.
                    {
                        const float ta = glm::dot(in.anchor - in.torso_origin,
                                                  in.torso_axis);
                        int ba = 0;
                        while (ba + 1 < in.n_back_planes &&
                               ta > in.bp_t[ba + 1])
                            ++ba;
                        const float s_anch = glm::dot(
                            in.anchor - in.bp_origin[ba], in.bp_normal[ba]);
                        const float keep_a = trail_chain_anchor_need(
                            gm.scarf_st, gm.scarf_par, in);
                        if (s_anch < keep_a)
                            in.anchor +=
                                in.bp_normal[ba] * (keep_a - s_anch);
                    }
                    in.bp_t[nr - 1] = glm::dot(cent[nr - 1] - in.torso_origin,
                                               in.torso_axis);
                }
            }
            if (in.n_back_planes > 0) {
                TrailChainInput in_s = in;
                if (gm.scarf_s_ok &&
                    gm.nodes[gm.scarf_s_node_id[0]].has_pose) {
                    in_s.anchor = glm::vec3(glm::dvec3(
                        gm.nodes[gm.scarf_s_node_id[0]].posed[3]));
                    // The short tail's bone-0 pod hangs off ITS pinned root
                    // (red-team finding 2): its own pod, its own band, its
                    // own stand-off -- never the long chain's.
                    const float ta_s = glm::dot(
                        in_s.anchor - in_s.torso_origin, in_s.torso_axis);
                    int ba_s = 0;
                    while (ba_s + 1 < in_s.n_back_planes &&
                           ta_s > in_s.bp_t[ba_s + 1])
                        ++ba_s;
                    const float s_anch_s =
                        glm::dot(in_s.anchor - in_s.bp_origin[ba_s],
                                 in_s.bp_normal[ba_s]);
                    const float keep_s = trail_chain_anchor_need(
                        gm.scarf_s_st, gm.scarf_s_par, in_s);
                    if (s_anch_s < keep_s)
                        in_s.anchor +=
                            in_s.bp_normal[ba_s] * (keep_s - s_anch_s);
                }
                int steps = 4;
                if (!gm.scarf_primed) {
                    trail_chain_reset(gm.scarf_st, gm.scarf_par, in.anchor,
                                      down);
                    if (gm.scarf_s_ok)
                        trail_chain_reset(gm.scarf_s_st, gm.scarf_s_par,
                                          in_s.anchor, down);
                    gm.scarf_primed = true;
                    steps = 240;  // settle before he is ever seen
                }
                for (int i = 0; i < steps; ++i) {
                    trail_chain_step(gm.scarf_st, gm.scarf_par, in);
                    if (gm.scarf_s_ok)
                        trail_chain_step(gm.scarf_s_st, gm.scarf_s_par, in_s);
                }
                glm::mat4 fr[kScarfSegments];
                trail_chain_frames(gm.scarf_st, in.width_axis, fr);
                for (int k = 0; k < kScarfSegments; ++k) {
                    GNode& nd = gm.nodes[gm.scarf_node_id[k]];
                    nd.posed = glm::dmat4(fr[k]);
                    nd.has_pose = true;
                }
                if (gm.scarf_s_ok) {
                    glm::mat4 frs[kScarfShortSegs];
                    trail_chain_frames(gm.scarf_s_st, in.width_axis, frs);
                    for (int k = 0; k < kScarfShortSegs; ++k) {
                        GNode& nd = gm.nodes[gm.scarf_s_node_id[k]];
                        nd.posed = glm::dmat4(frs[k]);
                        nd.has_pose = true;
                    }
                }
                // ---- the instrument (SEADS_FLAK_SCARF_DEBUG): the drawn
                // pods against the coat's banded surface, each vert in its
                // own band -- surf_vtx's twin. Negative = fabric inside.
                static const bool fs_dbg =
                    std::getenv("SEADS_FLAK_SCARF_DEBUG") != nullptr;
                if (fs_dbg) {
                    float worst = 1.0e9f;
                    int wb = -1;
                    for (const GunnerModel::SVert& sv : gm.pod_verts) {
                        const GPrim& gp = gm.prims[sv.prim];
                        const int jj = gp.jidx[4 * sv.vert];
                        const glm::vec3 pw(glm::dvec3(
                            gm.nodes[gm.joints[jj]].posed * gm.ibm[jj] *
                            glm::dvec4(gp.base_pos[3 * sv.vert],
                                       gp.base_pos[3 * sv.vert + 1],
                                       gp.base_pos[3 * sv.vert + 2], 1.0)));
                        const float tt = glm::dot(pw - in.torso_origin,
                                                  in.torso_axis);
                        int b = 0;
                        while (b + 1 < in.n_back_planes &&
                               tt > in.bp_t[b + 1])
                            ++b;
                        const float c = glm::dot(pw - in.bp_origin[b],
                                                 in.bp_normal[b]);
                        if (c < worst) {
                            worst = c;
                            wb = sv.ring;
                        }
                    }
                    static int tick = 0;
                    if (++tick % 30 == 0)
                        TraceLog(LOG_INFO,
                                 "FLAK SCARF: pod clearance now %.4f m "
                                 "(bone scarf_%02d)",
                                 static_cast<double>(worst), wb + 1);
                    if (worst < gm.scarf_worst - 0.001f) {
                        gm.scarf_worst = worst;
                        TraceLog(LOG_INFO,
                                 "FLAK SCARF: pod WORST YET %.4f m (NEGATIVE "
                                 "= drawn fabric INSIDE the coat) at bone "
                                 "scarf_%02d",
                                 static_cast<double>(worst), wb + 1);
                    }
                }
            }
        }
    }

    // --- joint matrices (gun model frame) and CPU skin
    std::vector<glm::dmat4> jm(gm.joints.size());
    for (std::size_t j = 0; j < gm.joints.size(); ++j) {
        const GNode& nd = gm.nodes[gm.joints[j]];
        jm[j] = (nd.has_pose ? nd.posed : nd.rest) * gm.ibm[j];
    }
    for (GPrim& gp : gm.prims) {
        Mesh& m = gp.mesh;
        for (int v = 0; v < m.vertexCount; ++v) {
            glm::dmat4 mtx(0.0);
            for (int k = 0; k < 4; ++k) {
                const float w = gp.jw[4 * v + k];
                if (w > 0.0f) mtx += jm[gp.jidx[4 * v + k]] * double(w);
            }
            const glm::dvec4 p =
                mtx * glm::dvec4(gp.base_pos[3 * v], gp.base_pos[3 * v + 1],
                                 gp.base_pos[3 * v + 2], 1.0);
            m.vertices[3 * v] = static_cast<float>(p.x);
            m.vertices[3 * v + 1] = static_cast<float>(p.y);
            m.vertices[3 * v + 2] = static_cast<float>(p.z);
            if (m.normals != nullptr && !gp.base_nrm.empty()) {
                const glm::dvec3 nrm = glm::normalize(
                    glm::dmat3(mtx) *
                    glm::dvec3(gp.base_nrm[3 * v], gp.base_nrm[3 * v + 1],
                               gp.base_nrm[3 * v + 2]));
                m.normals[3 * v] = static_cast<float>(nrm.x);
                m.normals[3 * v + 1] = static_cast<float>(nrm.y);
                m.normals[3 * v + 2] = static_cast<float>(nrm.z);
            }
        }
        UpdateMeshBuffer(m, 0, m.vertices,
                         m.vertexCount * 3 * static_cast<int>(sizeof(float)),
                         0);
        if (m.normals != nullptr)
            UpdateMeshBuffer(m, 2, m.normals,
                             m.vertexCount * 3 *
                                 static_cast<int>(sizeof(float)),
                             0);
    }

    // --- gun model frame -> eye-relative world: the man is a TRAIN rider,
    // so ONE rigid map = mount * train (the train_station_world map as a
    // matrix; double until the subtraction, the R = 15 km float rule).
    const flak::MountFrame f = flak::make_mount_frame(g.pos, g.up, g.fwd0);
    const glm::dvec3 rel = f.pos - eye;
    glm::dmat4 mount(1.0);
    mount[0] = glm::dvec4(f.right0, 0.0);
    mount[1] = glm::dvec4(f.up, 0.0);
    mount[2] = glm::dvec4(f.fwd0, 0.0);
    mount[3] = glm::dvec4(rel, 1.0);
    const glm::mat4 man2world = glm::mat4(
        mount * glm::mat4_cast(flak::train_quat(g.train_rad)));

    // The pullout fade rides as material alpha (raylib's default 3D pass
    // alpha-blends), so he cannot pop into existence inside the lens.
    const unsigned char al = static_cast<unsigned char>(
        std::lround(255.0f * std::clamp(alpha, 0.0f, 1.0f)));
    if (al == 0) return true;
    for (const GPrim& gp : gm.prims) {
        Color c = gp.col;
        c.a = al;
        gm.mat.maps[MATERIAL_MAP_DIFFUSE].color = c;
        DrawMesh(gp.mesh, gm.mat, to_ray_m(man2world));
    }
    return true;
}

}  // namespace render
