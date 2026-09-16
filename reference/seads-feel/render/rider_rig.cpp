#include "render/rider_rig.h"

#include <cmath>

namespace render {
namespace {

// Every number below was MEASURED out of assets/sled/indy650.glb on 2026-08-18,
// by reading the glTF JSON chunk directly (node.translation, in metres), the
// day the SUDBURIAN was spliced into it (assets/character/sudburian_src/
// splice_sudburian.py) in place of the 19-joint legacy rider. They are not
// design targets -- SUDBURIAN_LADDER §3's table is the target, and R2b is the
// rung that seated THIS blockout at those proportions and Chad signed it. This
// table's job is to say what the shipped file IS, so that any drift from it is
// loud.
//
// The facts, in numbers:
//   upperarm 0.344 / forearm 0.270 / hand 0.170 -> back-solve to H 1.849 m
//   against the ruled 1.850 (LADDER §3.0-TER); thigh 0.453 / calf 0.455.
//   `pelvis` is +0.979862 along root's local +Z: root is Blender's ground
//   bone with its -90 deg X rest, so its +Z is model UP -- the seated pelvis
//   sits 0.98 m above the armature origin. The clavicles hang 30 mm off the
//   spine at 161 mm up; the thighs are +-0.095 off the pelvis (§3: joint span
//   0.190, the STRUCK-defect-4 width, correct for 1.85 m).
const std::array<RiderJointSpec, kRiderJointCount>& specs_impl() {
    static const std::array<RiderJointSpec, kRiderJointCount> k = {{
        // name         parent               parent_name       rest_local_m
        {"root", kRiderNoJointParent, "sudburian_rig",
         glm::vec3(0.000000f, 0.000000f, 0.000000f), 0.000000f},
        {"pelvis", kRiderRoot, "root",
         glm::vec3(0.000000f, 0.000000f, 0.979862f), 0.979862f},
        {"spine_01", kRiderPelvis, "pelvis",
         glm::vec3(0.000000f, 0.095000f, 0.000000f), 0.095000f},
        {"spine_02", kRiderSpine1, "spine_01",
         glm::vec3(0.000000f, 0.146000f, 0.000000f), 0.146000f},
        {"spine_03", kRiderSpine2, "spine_02",
         glm::vec3(0.000000f, 0.146000f, 0.000000f), 0.146000f},
        {"neck_01", kRiderSpine3, "spine_03",
         glm::vec3(0.000000f, 0.146000f, 0.000000f), 0.146000f},
        {"head", kRiderNeck, "neck_01",
         glm::vec3(0.000000f, 0.096000f, 0.000000f), 0.096000f},
        // arms: clavicle 30 mm off the spine, glenohumeral span 0.420
        {"clavicle_l", kRiderSpine3, "spine_03",
         glm::vec3(-0.030000f, 0.161000f, 0.000000f), 0.163771f},
        {"upperarm_l", kRiderClavicleL, "clavicle_l",
         glm::vec3(0.000000f, 0.180624f, 0.000000f), 0.180624f},
        {"lowerarm_l", kRiderUpperarmL, "upperarm_l",
         glm::vec3(0.000000f, 0.344000f, 0.000000f), 0.344000f},
        {"hand_l", kRiderForearmL, "lowerarm_l",
         glm::vec3(0.000000f, 0.270000f, 0.000000f), 0.270000f},
        {"clavicle_r", kRiderSpine3, "spine_03",
         glm::vec3(0.030000f, 0.161000f, 0.000000f), 0.163771f},
        {"upperarm_r", kRiderClavicleR, "clavicle_r",
         glm::vec3(0.000000f, 0.180624f, 0.000000f), 0.180624f},
        {"lowerarm_r", kRiderUpperarmR, "upperarm_r",
         glm::vec3(0.000000f, 0.344000f, 0.000000f), 0.344000f},
        {"hand_r", kRiderForearmR, "lowerarm_r",
         glm::vec3(0.000000f, 0.270000f, 0.000000f), 0.270000f},
        // legs: hip offset +-0.095 => joint span 0.190 (§3, correct)
        {"thigh_l", kRiderPelvis, "pelvis",
         glm::vec3(-0.095000f, 0.000000f, 0.000000f), 0.095000f},
        {"calf_l", kRiderThighL, "thigh_l",
         glm::vec3(0.000000f, 0.453000f, 0.000000f), 0.453000f},
        {"foot_l", kRiderShinL, "calf_l",
         glm::vec3(0.000000f, 0.455000f, 0.000000f), 0.455000f},
        {"thigh_r", kRiderPelvis, "pelvis",
         glm::vec3(0.095000f, 0.000000f, 0.000000f), 0.095000f},
        {"calf_r", kRiderThighR, "thigh_r",
         glm::vec3(0.000000f, 0.453000f, 0.000000f), 0.453000f},
        {"foot_r", kRiderShinR, "calf_r",
         glm::vec3(0.000000f, 0.455000f, 0.000000f), 0.455000f},
        // ★ R3-HANDS. MEASURED out of the shipped assets/sled/indy650.glb
        // 2026-08-24, same as every row above -- not typed from the modelling
        // script. The thumb's offset is NOT a pure +Y bone offset because the
        // thumb is splayed off the fist; the mitt-front's is.
        {"thumb_01_r", kRiderHandR, "hand_r",
         glm::vec3(-0.069606f, 0.050000f, 0.007418f), 0.086023f},
        {"mittfront_01_l", kRiderHandL, "hand_l",
         glm::vec3(0.000000f, 0.100000f, 0.000000f), 0.100000f},
    }};
    return k;
}

}  // namespace

const std::array<RiderJointSpec, kRiderJointCount>& rider_joint_specs() {
    return specs_impl();
}

const std::array<RiderChainSpec, kRiderChainCount>& rider_chain_specs() {
    // ★ THE CROSSOVER, written once. `_l` = anatomical left = model +X =
    // the machine's `_R` socket (model_side 1); `_r` = model -X = `_L`
    // socket (model_side 0). rest_tip_minus_socket_m: measured world tip minus
    // world socket in the shipped file (2026-08-18, after splice_bar.py
    // carried the LIVE handlebar in). The arm rows are the FIST: hand_* is
    // the WRIST joint and seat_sudburian.py puts the KNUCKLE on the bar, so
    // the wrist sits 98 mm behind-and-above the grip socket -- 20 mm out,
    // 51 mm up, 83 mm aft. The leg rows are the R2b seating.
    // ★ 2026-08-18 evening (R2c-M2 hand tuning, Chad at the viewport): the
    // hands were slid along the bars independently -- left 36 mm outboard to
    // the new end flange, right 12.7 mm outboard and 6.35 mm aft -- so the
    // two arm rows are no longer mirrors of each other. Re-measured off the
    // shipped bytes; test_rider_pose "chain table" re-derives them.
    static const std::array<RiderChainSpec, kRiderChainCount> k = {{
        {"arm_l",
         {{kRiderUpperarmL, kRiderForearmL, kRiderHandL}},
         true,
         kMachGripSocketR,
         1,
         glm::vec3(0.053508f, 0.045236f, -0.094443f)},
        {"arm_r",
         {{kRiderUpperarmR, kRiderForearmR, kRiderHandR}},
         true,
         kMachGripSocketL,
         0,
         glm::vec3(-0.031599f, 0.049249f, -0.093675f)},
        {"leg_l",
         {{kRiderThighL, kRiderShinL, kRiderFootL}},
         false,
         kMachBoardSocketR,
         1,
         glm::vec3(0.000006f, 0.003784f, 0.272032f)},
        {"leg_r",
         {{kRiderThighR, kRiderShinR, kRiderFootR}},
         false,
         kMachBoardSocketL,
         0,
         glm::vec3(-0.000006f, 0.003784f, 0.272032f)},
    }};
    return k;
}

const std::array<MachineNodeSpec, kMachineNodeCount>& machine_node_specs() {
    // World-x signs measured off the shipped asset: CH_susp_L -0.4635,
    // ski_L -0.4635, grip_socket_L -0.3150, board_socket_L -0.3000, and the
    // three centreline nodes at exactly 0.
    static const std::array<MachineNodeSpec, kMachineNodeCount> k = {{
        {"CH_steer_pivot", 0},
        {"CH_susp_L", -1},
        {"CH_susp_R", +1},
        {"CH_susp_T", 0},
        {"CH_cam", 0},
        {"ski_L", -1},
        {"ski_R", +1},
        {"grip_socket_L", -1},
        {"grip_socket_R", +1},
        {"board_socket_L", -1},
        {"board_socket_R", +1},
    }};
    return k;
}

const std::array<ExportPayloadSpec, kExportPayloadCount>&
export_payload_specs() {
    // Verified present in the spliced GLB 2026-08-18. See the header for the
    // use_visible-vs-use_renderable trap this list exists to catch.
    static const std::array<ExportPayloadSpec, kExportPayloadCount> k = {{
        {"sudburian_proxy", true},
        {"snow_flap", true},
        {"pan_front_bar", true},
    }};
    return k;
}

std::vector<std::string> missing_export_payload(
    const std::vector<std::string>& node_names) {
    std::vector<std::string> missing;
    for (const ExportPayloadSpec& s : export_payload_specs()) {
        bool found = false;
        for (const std::string& n : node_names)
            if (n == s.name) {
                found = true;
                break;
            }
        if (!found) missing.emplace_back(s.name);
    }
    return missing;
}

RigBindResult bind_rider_rig(const NodeLookup& lookup) {
    RigBindResult r;
    r.joint.fill(-1);
    r.machine.fill(-1);
    if (!lookup) {  // a null callback is a missing rig, not a crash
        for (const RiderJointSpec& s : rider_joint_specs())
            r.missing.emplace_back(s.name);
        for (const MachineNodeSpec& s : machine_node_specs())
            r.missing.emplace_back(s.name);
        return r;
    }
    const std::array<RiderJointSpec, kRiderJointCount>& js = rider_joint_specs();
    for (int j = 0; j < kRiderJointCount; ++j) {
        const int idx = lookup(js[static_cast<std::size_t>(j)].name);
        r.joint[static_cast<std::size_t>(j)] = idx;
        if (idx < 0) r.missing.emplace_back(js[static_cast<std::size_t>(j)].name);
    }
    const std::array<MachineNodeSpec, kMachineNodeCount>& ms =
        machine_node_specs();
    for (int m = 0; m < kMachineNodeCount; ++m) {
        const int idx = lookup(ms[static_cast<std::size_t>(m)].name);
        r.machine[static_cast<std::size_t>(m)] = idx;
        if (idx < 0) r.missing.emplace_back(ms[static_cast<std::size_t>(m)].name);
    }
    return r;
}

RigBindResult bind_rider_rig(const std::vector<std::string>& node_names) {
    return bind_rider_rig([&node_names](const char* n) -> int {
        for (std::size_t i = 0; i < node_names.size(); ++i)
            if (node_names[i] == n) return static_cast<int>(i);
        return -1;
    });
}

std::string describe_missing(const RigBindResult& r) {
    if (r.missing.empty()) return std::string();
    std::string s;
    for (std::size_t i = 0; i < r.missing.size(); ++i) {
        if (i != 0) s += ", ";
        s += r.missing[i];
    }
    return s;
}

std::vector<int> validate_rider_rest_geometry(
    const std::array<glm::vec3, kRiderJointCount>& local_t_m, float tol_m) {
    std::vector<int> bad;
    const std::array<RiderJointSpec, kRiderJointCount>& js = rider_joint_specs();
    for (int j = 0; j < kRiderJointCount; ++j) {
        const glm::vec3& want = js[static_cast<std::size_t>(j)].rest_local_m;
        const glm::vec3& got = local_t_m[static_cast<std::size_t>(j)];
        if (std::fabs(want.x - got.x) > tol_m ||
            std::fabs(want.y - got.y) > tol_m ||
            std::fabs(want.z - got.z) > tol_m)
            bad.push_back(j);
    }
    return bad;
}

}  // namespace render
