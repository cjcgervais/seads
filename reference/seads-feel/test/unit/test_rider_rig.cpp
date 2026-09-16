// ★ THE SUDBURIAN JOINT-CATALOGUE VALIDATOR -- docs/SUDBURIAN_LADDER.md rung R0,
// §2.3a ("a validator test that FAILS LOUD if any declared joint is absent").
//
// This file is the direct retirement of DEFECT 15 ("no tests whatsoever":
// `impact render/sled_model.cpp` was 0 dependents, 0 test TUs, no GLB schema
// validator) and the proof half of DEFECT 13 ("silent partial rigs"). The
// catalogue in render/rider_rig.h is the guard; these are the teeth.
//
// It runs HEADLESSLY: seads_tests links NO raylib, and render/rider_rig.cpp is
// in seads_render_core precisely so it is reachable from here (§0.5). The GLB
// is parsed with cgltf, exactly as test/unit/test_asset_validator.cpp does --
// the CGLTF_IMPLEMENTATION lives THERE and is the only copy in this binary, so
// this TU includes the header declarations only.
//
// ★ THE SHIPPED ASSET IS NEVER MUTATED. The deliberate-missing-joint test works
// on an in-memory copy of the real node-name list with one entry deleted.

#include <algorithm>
#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <glm/glm.hpp>
#include <string>
#include <vector>

#include "render/rider_rig.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include "cgltf.h"  // implementation lives in test_asset_validator.cpp
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#ifdef NDEBUG
#error "SEADS gate requires an assert-live build (SPEC 6.1)"
#endif

namespace {

// One parsed view of assets/sled/indy650.glb: node names, parent names, local
// translations and world positions. Loaded once, shared by every case.
struct Glb {
    bool ok = false;
    std::vector<std::string> name;
    std::vector<std::string> parent_name;  // "" = a glTF root
    std::vector<glm::vec3> local_t;
    std::vector<glm::vec3> world_p;
    std::vector<char> has_mesh;
    std::vector<std::string> skin0_joint;  // joint names of skin 0, in order
    int skins = -1;
    int animations = -1;
};

const Glb& glb() {
    static Glb g = [] {
        Glb out;
        const std::string path =
            std::string(SEADS_ASSET_DIR) + "/sled/indy650.glb";
        cgltf_options opt{};
        cgltf_data* d = nullptr;
        if (cgltf_parse_file(&opt, path.c_str(), &d) != cgltf_result_success)
            return out;
        if (cgltf_load_buffers(&opt, d, path.c_str()) != cgltf_result_success) {
            cgltf_free(d);
            return out;
        }
        const cgltf_size n = d->nodes_count;
        out.name.assign(n, std::string());
        out.parent_name.assign(n, std::string());
        out.local_t.assign(n, glm::vec3(0.0f));
        out.world_p.assign(n, glm::vec3(0.0f));
        out.has_mesh.assign(n, 0);
        out.skins = static_cast<int>(d->skins_count);
        out.animations = static_cast<int>(d->animations_count);
        for (cgltf_size i = 0; i < n; ++i) {
            const cgltf_node& nd = d->nodes[i];
            out.name[i] = nd.name != nullptr ? nd.name : "";
            out.has_mesh[i] = nd.mesh != nullptr ? 1 : 0;
            if (nd.parent != nullptr && nd.parent->name != nullptr)
                out.parent_name[i] = nd.parent->name;
            if (nd.has_translation)
                out.local_t[i] = glm::vec3(nd.translation[0], nd.translation[1],
                                           nd.translation[2]);
            cgltf_float w[16];
            cgltf_node_transform_world(&nd, w);
            out.world_p[i] = glm::vec3(w[12], w[13], w[14]);
        }
        if (d->skins_count > 0) {
            const cgltf_skin& sk = d->skins[0];
            for (cgltf_size j = 0; j < sk.joints_count; ++j)
                out.skin0_joint.emplace_back(sk.joints[j]->name != nullptr
                                                 ? sk.joints[j]->name
                                                 : "");
        }
        cgltf_free(d);
        out.ok = true;
        return out;
    }();
    return g;
}

int index_of(const std::vector<std::string>& v, const std::string& s) {
    const auto it = std::find(v.begin(), v.end(), s);
    return it == v.end() ? -1 : static_cast<int>(it - v.begin());
}

}  // namespace

// --- the catalogue is internally coherent (no asset needed) ----------------

TEST_CASE("rider joint catalogue is topologically sorted and well formed",
          "[rider_rig]") {
    const auto& js = render::rider_joint_specs();
    REQUIRE(js.size() == static_cast<std::size_t>(render::kRiderJointCount));
    std::vector<std::string> seen;
    for (int j = 0; j < render::kRiderJointCount; ++j) {
        const render::RiderJointSpec& s = js[static_cast<std::size_t>(j)];
        INFO("joint " << j << " = " << s.name);
        REQUIRE(std::string(s.name).size() > 0);
        // raylib truncates bone names at char[32] (§1.2), so a >31-char name
        // silently collides with its neighbours. Pin the ceiling now.
        REQUIRE(std::string(s.name).size() < 32u);
        REQUIRE(index_of(seen, s.name) < 0);  // no duplicate names
        seen.emplace_back(s.name);
        // parent index strictly less than own => one forward composing pass
        REQUIRE(s.parent < j);
        if (s.parent >= 0)
            REQUIRE(std::string(js[static_cast<std::size_t>(s.parent)].name) ==
                    std::string(s.parent_name));
        else
            REQUIRE(std::string(s.parent_name) == "sudburian_rig");
        // declared segment length agrees with the declared offset
        REQUIRE(std::fabs(glm::length(s.rest_local_m) - s.segment_len_m) <
                1e-4f);
    }
    // exactly one root
    int roots = 0;
    for (const render::RiderJointSpec& s : js)
        if (s.parent < 0) ++roots;
    REQUIRE(roots == 1);
}

TEST_CASE("rider IK chains are declared consistently with the joint catalogue",
          "[rider_rig]") {
    const auto& cs = render::rider_chain_specs();
    REQUIRE(cs.size() == static_cast<std::size_t>(render::kRiderChainCount));
    const auto& js = render::rider_joint_specs();
    const auto& ms = render::machine_node_specs();
    for (const render::RiderChainSpec& c : cs) {
        INFO("chain " << c.debug_name);
        // a 2-bone chain: root -> mid -> tip, each the parent of the next
        for (int k = 0; k < 3; ++k) {
            REQUIRE(c.joint[static_cast<std::size_t>(k)] >= 0);
            REQUIRE(c.joint[static_cast<std::size_t>(k)] <
                    render::kRiderJointCount);
        }
        REQUIRE(js[static_cast<std::size_t>(c.joint[1])].parent == c.joint[0]);
        REQUIRE(js[static_cast<std::size_t>(c.joint[2])].parent == c.joint[1]);
        REQUIRE(c.socket >= 0);
        REQUIRE(c.socket < render::kMachineNodeCount);
        REQUIRE((c.model_side == 0 || c.model_side == 1));
        // model_side 0 == the "_L" nodes == model -X (§2.3a, the L/R trap):
        // the chain's side must agree with its socket's measured world-x sign.
        const int want = c.model_side == 0 ? -1 : +1;
        REQUIRE(ms[static_cast<std::size_t>(c.socket)].world_x_sign == want);
    }
    // all four chains distinct, and the arms/legs split 2/2
    int arms = 0;
    for (const render::RiderChainSpec& c : cs)
        if (c.is_arm) ++arms;
    REQUIRE(arms == 2);
}

// ★ DO NOT RENAME. §2.3a: these eleven are MACHINE-side, and renaming any of
// them silently breaks steering, suspension or the rider IK. This case exists
// so that a future rename is a red test with a name on it, not a mystery.
TEST_CASE("machine socket names and sides are pinned against rename",
          "[rider_rig]") {
    const auto& ms = render::machine_node_specs();
    REQUIRE(ms.size() == static_cast<std::size_t>(render::kMachineNodeCount));
    const char* kExpect[render::kMachineNodeCount] = {
        "CH_steer_pivot", "CH_susp_L",      "CH_susp_R",
        "CH_susp_T",      "CH_cam",         "ski_L",
        "ski_R",          "grip_socket_L",  "grip_socket_R",
        "board_socket_L", "board_socket_R"};
    const int kSign[render::kMachineNodeCount] = {0,  -1, +1, 0,  0, -1,
                                                  +1, -1, +1, -1, +1};
    for (int m = 0; m < render::kMachineNodeCount; ++m) {
        INFO("machine node " << m);
        REQUIRE(std::string(ms[static_cast<std::size_t>(m)].name) ==
                std::string(kExpect[m]));
        REQUIRE(ms[static_cast<std::size_t>(m)].world_x_sign == kSign[m]);
    }
}

// --- the catalogue matches the SHIPPED asset -------------------------------

TEST_CASE("shipped indy650 GLB carries every declared rider joint",
          "[rider_rig][asset]") {
    const Glb& g = glb();
    REQUIRE(g.ok);
    const render::RigBindResult r = render::bind_rider_rig(g.name);
    INFO("missing: " << render::describe_missing(r));
    REQUIRE(r.complete());
    REQUIRE(r.missing.empty());
    for (int j = 0; j < render::kRiderJointCount; ++j)
        REQUIRE(r.joint[static_cast<std::size_t>(j)] >= 0);
    for (int m = 0; m < render::kMachineNodeCount; ++m)
        REQUIRE(r.machine[static_cast<std::size_t>(m)] >= 0);
    // ★ THE SUDBURIAN (2026-08-18): the skin is the full 42-bone rig of
    // SUDBURIAN_LADDER §2.2 (34 body + 6 scarf + head_fp_anchor + root) and the
    // catalogue is the 21 joints the runtime DRIVES. Every catalogue joint must
    // be a SKIN joint -- so the catalogue describes the thing that actually
    // deforms the mesh, not some parallel set of nodes that happen to share
    // names -- and the skin must be the whole rig, not a def-bones-only export.
    // 42 -> 44, R2c-7s(e) 2026-08-20: Chad ruled the SHORT scarf tail moves
    // ("a little bit but more simply") -- patch_scarf.py adds scarf_s01/s02
    // under neck_01. The count still tells the riders apart (legacy = 19).
    REQUIRE(g.skin0_joint.size() == 44u);
    const auto& js = render::rider_joint_specs();
    for (int j = 0; j < render::kRiderJointCount; ++j) {
        INFO("catalogue joint " << js[static_cast<std::size_t>(j)].name);
        REQUIRE(index_of(g.skin0_joint, js[static_cast<std::size_t>(j)].name) >=
                0);
    }
}

TEST_CASE("shipped rider parentage matches the declared catalogue",
          "[rider_rig][asset]") {
    const Glb& g = glb();
    REQUIRE(g.ok);
    const auto& js = render::rider_joint_specs();
    for (int j = 0; j < render::kRiderJointCount; ++j) {
        const render::RiderJointSpec& s = js[static_cast<std::size_t>(j)];
        const int idx = index_of(g.name, s.name);
        INFO("joint " << s.name);
        REQUIRE(idx >= 0);
        REQUIRE(g.parent_name[static_cast<std::size_t>(idx)] ==
                std::string(s.parent_name));
    }
}

TEST_CASE("shipped rider segment geometry matches the declared catalogue",
          "[rider_rig][asset]") {
    const Glb& g = glb();
    REQUIRE(g.ok);
    const auto& js = render::rider_joint_specs();
    std::array<glm::vec3, render::kRiderJointCount> measured{};
    for (int j = 0; j < render::kRiderJointCount; ++j) {
        const int idx = index_of(g.name, js[static_cast<std::size_t>(j)].name);
        REQUIRE(idx >= 0);
        measured[static_cast<std::size_t>(j)] =
            g.local_t[static_cast<std::size_t>(idx)];
    }
    // 0.1 mm: the catalogue is written to 6 dp of the float32 in the file, and
    // defect 1's inversion is 56 mm, so this is tight enough to catch a real
    // re-rig and loose enough not to trip on float printing.
    const std::vector<int> bad =
        render::validate_rider_rest_geometry(measured, 1e-4f);
    // ★ R0 FOLLOW-UP 1. This INFO used to sit INSIDE the `for` body, so Catch2
    // scoped it out again before the REQUIRE below ever ran and a real geometry
    // drift failed as a bare `false` with no joint named. Build one string in
    // the loop, scope it OUTSIDE, and the failure says which joint moved.
    std::string drifted;
    for (const int j : bad) {
        if (!drifted.empty()) drifted += ", ";
        drifted += js[static_cast<std::size_t>(j)].name;
    }
    INFO("drifted: " << drifted);
    REQUIRE(bad.empty());
}

TEST_CASE("declared machine sockets sit on their declared side of the machine",
          "[rider_rig][asset]") {
    const Glb& g = glb();
    REQUIRE(g.ok);
    const auto& ms = render::machine_node_specs();
    for (int m = 0; m < render::kMachineNodeCount; ++m) {
        const render::MachineNodeSpec& s = ms[static_cast<std::size_t>(m)];
        const int idx = index_of(g.name, s.name);
        INFO("machine node " << s.name);
        REQUIRE(idx >= 0);
        const float x = g.world_p[static_cast<std::size_t>(idx)].x;
        if (s.world_x_sign < 0)
            REQUIRE(x < -0.05f);
        else if (s.world_x_sign > 0)
            REQUIRE(x > 0.05f);
        else
            REQUIRE(std::fabs(x) < 1e-4f);
    }
}

TEST_CASE("rest tip-to-socket offsets match the shipped asset",
          "[rider_rig][asset]") {
    const Glb& g = glb();
    REQUIRE(g.ok);
    const auto& js = render::rider_joint_specs();
    const auto& ms = render::machine_node_specs();
    for (const render::RiderChainSpec& c : render::rider_chain_specs()) {
        INFO("chain " << c.debug_name);
        const int tip = index_of(
            g.name, js[static_cast<std::size_t>(c.joint[2])].name);
        const int sock =
            index_of(g.name, ms[static_cast<std::size_t>(c.socket)].name);
        REQUIRE(tip >= 0);
        REQUIRE(sock >= 0);
        const glm::vec3 d = g.world_p[static_cast<std::size_t>(tip)] -
                            g.world_p[static_cast<std::size_t>(sock)];
        // 1 mm. These are DEFECT 6 for the leg chains (boot 41.7 mm below and
        // 50.8 mm aft of the board socket); R1 drives them to zero and must
        // re-measure this table on purpose when it does.
        REQUIRE(std::fabs(d.x - c.rest_tip_minus_socket_m.x) < 1e-3f);
        REQUIRE(std::fabs(d.y - c.rest_tip_minus_socket_m.y) < 1e-3f);
        REQUIRE(std::fabs(d.z - c.rest_tip_minus_socket_m.z) < 1e-3f);
    }
}

// --- ★ THE WRONG-CHECKBOX GUARD --------------------------------------------
//
// THESE ASSERTIONS EXIST TO CATCH A WRONG EXPORT CHECKBOX, NOT A CODE
// REGRESSION. Do not delete them because "no C++ reads these names".
//
// assets/sled/indy650.glb is exported by hand with bpy.ops.export_scene.gltf.
// Run it with `use_visible=True` instead of `use_renderable=True` and the
// operator SILENTLY DROPS every viewport-hidden object even though its
// hide_render is False -- which is exactly the rider meshes, the accessories,
// the snow flap, the front pan bar and the two IK pole empties. A GLB exported
// that way SHIPPED ONCE and was caught only by manually diffing node lists:
// it parses, it loads, the armature is still there, and the man is simply gone.
//
// A joint-name check does NOT catch this, which is why these three cases are
// separate from every case above: the SKIN and the JOINTS survive a bad export
// and only the deformed meshes vanish.

TEST_CASE("shipped GLB is still RIGGED -- exactly one skin, no animations",
          "[rider_rig][asset][export]") {
    const Glb& g = glb();
    REQUIRE(g.ok);
    // One skin. A frozen / apply-modifiers / unskinned export would sail
    // straight through every name-presence check in this file, and the rider
    // would render as a statue welded to the rest pose.
    REQUIRE(g.skins == 1);
    REQUIRE(g.skin0_joint.size() == 44u);  // §2.2's 42 + scarf_s01/s02 (R2c-7s(e))
    // Zero animations is the contract (1 skin / 44 joints / 0 animations); the
    // pose is a pure function of kernel state (§0.1), so an animation
    // appearing here is a pipeline surprise worth stopping on.
    REQUIRE(g.animations == 0);
}

TEST_CASE("shipped GLB carries every rider mesh and accessory node",
          "[rider_rig][asset][export]") {
    const Glb& g = glb();
    REQUIRE(g.ok);
    const std::vector<std::string> missing =
        render::missing_export_payload(g.name);
    for (const std::string& m : missing) INFO("dropped by export: " << m);
    REQUIRE(missing.empty());
    // and the ones that are supposed to carry geometry actually do -- an empty
    // placeholder with the right name is still a missing rider.
    for (const render::ExportPayloadSpec& s : render::export_payload_specs()) {
        const int idx = index_of(g.name, s.name);
        INFO("payload node " << s.name);
        REQUIRE(idx >= 0);
        REQUIRE((g.has_mesh[static_cast<std::size_t>(idx)] != 0) == s.has_mesh);
    }
}

TEST_CASE("export-payload validator fails loud when the rider is dropped",
          "[rider_rig][export]") {
    // Simulate the exact bad export in memory -- the shipped file is never
    // touched. use_visible drops ALL of them at once, so that is what is
    // modelled here, but the single-node case is checked too.
    const Glb& g = glb();
    REQUIRE(g.ok);
    std::vector<std::string> names = g.name;  // COPY
    for (const render::ExportPayloadSpec& s : render::export_payload_specs()) {
        const int idx = index_of(names, s.name);
        REQUIRE(idx >= 0);
        names[static_cast<std::size_t>(idx)] = "";
    }
    const std::vector<std::string> gone = render::missing_export_payload(names);
    REQUIRE(gone.size() ==
            static_cast<std::size_t>(render::kExportPayloadCount));
    REQUIRE(std::find(gone.begin(), gone.end(),
                      std::string("sudburian_proxy")) != gone.end());
    // ... and the joint catalogue still binds clean, which is precisely why
    // this guard has to be its own check: the armature survives the bad export.
    REQUIRE(render::bind_rider_rig(names).complete());

    // one node at a time, each named
    for (const render::ExportPayloadSpec& s : render::export_payload_specs()) {
        std::vector<std::string> one = g.name;  // COPY
        one[static_cast<std::size_t>(index_of(one, s.name))] = "";
        const std::vector<std::string> m = render::missing_export_payload(one);
        INFO("dropped " << s.name);
        REQUIRE(m.size() == 1u);
        REQUIRE(m[0] == std::string(s.name));
    }
}

// --- ★ THE TEETH: the validator must FAIL LOUD on a missing joint -----------
//
// Defect 13 in one case. Before render/rider_rig.h this exact situation --
// one joint name absent from the GLB -- produced a -1 that render/sled_model.cpp
// dropped on the floor, and the limb just stopped working with no diagnostic.
// The shipped file is NOT touched: the removal happens on an in-memory copy of
// its node-name list.

TEST_CASE("validator fails loud when a declared rider joint is missing",
          "[rider_rig]") {
    const Glb& g = glb();
    REQUIRE(g.ok);
    REQUIRE(render::bind_rider_rig(g.name).complete());  // baseline is clean

    const auto& js = render::rider_joint_specs();
    for (int j = 0; j < render::kRiderJointCount; ++j) {
        const std::string victim = js[static_cast<std::size_t>(j)].name;
        std::vector<std::string> names = g.name;  // COPY -- asset untouched
        const int idx = index_of(names, victim);
        REQUIRE(idx >= 0);
        names[static_cast<std::size_t>(idx)] = "";  // the joint is gone

        const render::RigBindResult r = render::bind_rider_rig(names);
        INFO("removed joint " << victim);
        REQUIRE_FALSE(r.complete());                       // absence is seen
        REQUIRE(r.missing.size() == 1u);                   // and only this one
        REQUIRE(r.missing[0] == victim);                   // and it is NAMED
        REQUIRE(r.joint[static_cast<std::size_t>(j)] < 0);
        // the failure carries a human-readable message, which is what the
        // runtime logs: silence is exactly the defect being retired.
        REQUIRE(render::describe_missing(r) == victim);
    }
}

TEST_CASE("validator fails loud when a declared machine socket is missing",
          "[rider_rig]") {
    const Glb& g = glb();
    REQUIRE(g.ok);
    const auto& ms = render::machine_node_specs();
    for (int m = 0; m < render::kMachineNodeCount; ++m) {
        const std::string victim = ms[static_cast<std::size_t>(m)].name;
        std::vector<std::string> names = g.name;  // COPY -- asset untouched
        const int idx = index_of(names, victim);
        REQUIRE(idx >= 0);
        names[static_cast<std::size_t>(idx)] = "";

        const render::RigBindResult r = render::bind_rider_rig(names);
        INFO("removed machine node " << victim);
        REQUIRE_FALSE(r.complete());
        REQUIRE(r.missing.size() == 1u);
        REQUIRE(r.missing[0] == victim);
        REQUIRE(r.machine[static_cast<std::size_t>(m)] < 0);
    }
}

TEST_CASE("an empty rig reports every declared node as missing", "[rider_rig]") {
    const render::RigBindResult r =
        render::bind_rider_rig(std::vector<std::string>{});
    REQUIRE_FALSE(r.complete());
    REQUIRE(r.missing.size() ==
            static_cast<std::size_t>(render::kRiderJointCount) +
                static_cast<std::size_t>(render::kMachineNodeCount));
    REQUIRE(render::describe_missing(r).find("upperarm_l") !=
            std::string::npos);
    // a null lookup is a missing rig, not a crash
    const render::RigBindResult n = render::bind_rider_rig(render::NodeLookup{});
    REQUIRE_FALSE(n.complete());
    REQUIRE(n.missing.size() == r.missing.size());
}

TEST_CASE("geometry validator fails loud when a segment length drifts",
          "[rider_rig]") {
    const auto& js = render::rider_joint_specs();
    std::array<glm::vec3, render::kRiderJointCount> t{};
    for (int j = 0; j < render::kRiderJointCount; ++j)
        t[static_cast<std::size_t>(j)] =
            js[static_cast<std::size_t>(j)].rest_local_m;
    REQUIRE(render::validate_rider_rest_geometry(t, 1e-4f).empty());
    // a 1 cm re-rig of the left forearm -- far below anything you would notice
    // in a still, far above the tolerance
    t[static_cast<std::size_t>(render::kRiderForearmL)].y += 0.01f;
    const std::vector<int> bad =
        render::validate_rider_rest_geometry(t, 1e-4f);
    REQUIRE(bad.size() == 1u);
    REQUIRE(bad[0] == render::kRiderForearmL);
}
