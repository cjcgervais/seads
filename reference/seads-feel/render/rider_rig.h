#pragma once

#include <array>
#include <functional>
#include <glm/glm.hpp>
#include <string>
#include <vector>

// ★ THE SUDBURIAN JOINT CATALOGUE -- docs/SUDBURIAN_LADDER.md §2.2 / §2.3a.
//
// WHAT THIS RETIRES. Defect 13 ("silent partial rigs") and defect 15 ("no
// tests whatsoever"). Before this file, render/sled_model.cpp bound every rider
// joint and every machine socket with a bare `find_node(sm, "forearm_L")` that
// returns -1 when the name is absent, and the calling code then skipped the
// limb with NO warning: a typo, a Blender rename or a bad export silently
// produced a one-armed rider that nothing in the build or the gate could see.
//
// HOW. Exactly the aircraft pattern (render/rig.h `aircraft_node_specs()`): a
// fixed-size std::array indexed by an enum, so a MISSING ENTRY is a compile
// error, and the entries carry MEASURED geometry so the catalogue is a real
// contract rather than a name list. Binding returns an explicit result that
// NAMES every absent node; there is no way to consume it and not notice.
//
// PURE (§0.5 LAYERING): glm + std only, ZERO raylib. That is what puts it in
// seads_render_core and therefore inside seads_tests, which links no raylib --
// the whole reason the old binding code was untestable is that it lived in the
// raylib-only `seads` exe target. Mesh upload and draw stay there; the
// catalogue and the validator live here.
//
// ★★ THE SUDBURIAN IS IN THE FILE (2026-08-18). These are the UE5 names of §2.2
// (`upperarm_l`, `lowerarm_l`, `calf_l`, ...), MEASURED out of the spliced
// assets/sled/indy650.glb -- the 42-bone `sudburian_rig` skin that
// assets/character/sudburian_src/splice_sudburian.py put in place of the
// 19-joint legacy rider (docs/RIDER_AUTHORITY.md). The 23 joints below are the
// ones the runtime DRIVES or MEASURES (the four IK chains, the CG segments, the
// head, the two clavicles that now sit on the arm path, and -- R3-HANDS -- the
// two CONTROL bones); the remaining skin joints (twists, the off-side thumb and
// mitt-front, balls, scarf, head_fp_anchor) ride their parents and need no
// entry here.
//
// ★ R3-HANDS (Chad, 2026-08-24) ADDED THE TWO CONTROL BONES. `thumb_01_r` is
// the throttle thumb and `mittfront_01_l` the brake fingers -- §7.2 of
// docs/SUDBURIAN_LADDER.md names both by name. They are here rather than left
// to ride their parents because the runtime now DRIVES them, and this
// catalogue is what makes a missing bone a loud refusal instead of a silently
// dropped animation. Both were verified to carry skin weight before the entry
// was written: `thumb_01_r` 184 verts, `mittfront_01_l` 610.
//
// ★★ THE L/R CROSSOVER (§2.3a) IS NOW REAL. The Sudburian's `_l` is his
// ANATOMICAL LEFT and sits at model +X (`hand_l` x = +0.335); the machine's
// `_L` sockets are DO-NOT-RENAME and sit at model -X (`grip_socket_L` x =
// -0.315). So `hand_l` welds to `grip_socket_R` and `foot_l` to
// `board_socket_R`, and that pairing is written ONCE, in rider_chain_specs()
// (`model_side`), never re-derived from a name. `model_side` still means what
// it always meant -- 0 = model -X, 1 = model +X -- and the runtime's
// throttle side is still derived from the grip socket's x sign, so the RIGHT
// hand (`hand_r`, model -X) is the throttle by measurement, exactly as ruled.

namespace render {

// The joints of the Sudburian skin the runtime binds, in a topologically
// sorted order: every joint's parent index is strictly less than its own, so
// a caller can compose world transforms in one forward pass. Keep in lockstep
// with rider_joint_specs() in rider_rig.cpp.
//
// The enum symbols keep their R0 spelling (Forearm = `lowerarm_*`, Shin =
// `calf_*`) so the pure pose math in rider_pose.cpp reads unchanged; the L/R
// in a symbol now means the SUDBURIAN's own `_l`/`_r` = anatomical, model +X
// for `_l`.
enum RiderJoint {
    kRiderRoot = 0,   // parent is the non-joint armature node "sudburian_rig"
    kRiderPelvis = 1,
    kRiderSpine1 = 2,  // spine_01
    kRiderSpine2 = 3,  // spine_02
    kRiderSpine3 = 4,  // spine_03: both clavicles hang off this
    kRiderNeck = 5,    // neck_01
    kRiderHead = 6,    // the cam-follow node
    kRiderClavicleL = 7,   // NEW at R2: sits between spine_03 and the arm
    kRiderUpperarmL = 8,
    kRiderForearmL = 9,    // lowerarm_l
    kRiderHandL = 10,
    kRiderClavicleR = 11,
    kRiderUpperarmR = 12,
    kRiderForearmR = 13,   // lowerarm_r
    kRiderHandR = 14,
    kRiderThighL = 15,
    kRiderShinL = 16,      // calf_l
    kRiderFootL = 17,
    kRiderThighR = 18,
    kRiderShinR = 19,      // calf_r
    kRiderFootR = 20,
    // ★ R3-HANDS, the two CONTROL bones. Leaves: they drive no chain and carry
    // no CG segment, so they append here without moving any existing index --
    // and their parents (hand_r 14, hand_l 10) are still strictly less than
    // their own, which is the topological contract the forward pass relies on.
    kRiderThumbR = 21,      // thumb_01_r  -- the THROTTLE thumb
    kRiderMittFrontL = 22,  // mittfront_01_l -- the BRAKE fingers
    kRiderJointCount = 23,
};

// parent index sentinel: the joint's glTF parent is NOT itself a skin joint
// (true only of `root`, whose parent is the armature node "sudburian_rig").
inline constexpr int kRiderNoJointParent = -1;

// One declared joint. rest_local_m is the joint's glTF local TRANSLATION in the
// shipped file. For every joint below the clavicles/hips it is a pure +Y bone
// offset (Blender exports a child bone in its parent's bone frame), so its
// length IS the segment length; the exceptions are `root` (0), `pelvis` (the
// rig's stand-up along root's +Z), the clavicles and the thighs. Rotations are
// deliberately NOT pinned: the rest pose is the SEATED pose R2b signed and the
// IK identity, and it is authored art; the skeleton's TOPOLOGY and SEGMENT
// LENGTHS are the structural contract.
struct RiderJointSpec {
    const char* name = "";         // exact glTF node name (UE5, §2.2)
    int parent = kRiderNoJointParent;  // RiderJoint index of the parent joint
    const char* parent_name = "";  // glTF node name of the parent, joint or not
    glm::vec3 rest_local_m{0.0f};  // measured local translation, metres
    float segment_len_m = 0.0f;    // |rest_local_m|, metres
};

// The four 2-bone IK chains render/sled_model.cpp solves, and the machine
// socket each chain's TIP is pinned to. These are exactly the bindings that
// used to fail silently: a missing `forearm_L` dropped the whole left arm, a
// missing `board_socket_R` dropped the right leg, and the rider just stopped
// tracking the machine with no diagnostic anywhere.
enum RiderChain {
    kChainArmL = 0,
    kChainArmR = 1,
    kChainLegL = 2,
    kChainLegR = 3,
    kRiderChainCount = 4,
};

struct RiderChainSpec {
    const char* debug_name = "";
    std::array<int, 3> joint{{-1, -1, -1}};  // root / mid / tip RiderJoint
    bool is_arm = false;  // false = a leg chain (foot -> running board)
    int socket = -1;     // MachineNode the tip welds to (see below)
    // 0 = model -X (the machine's "_L" sockets), 1 = model +X ("_R" sockets).
    // ★ THE CROSSOVER LIVES HERE: the Sudburian's `_l` chains carry model_side
    // 1 and the `_r` chains model_side 0. Written once, measured, tested.
    int model_side = 0;
    // Rest-pose (tip world position - socket world position), metres, MEASURED
    // out of the shipped GLB. A live contract, not an ideal. On the Sudburian
    // the leg rows are the R2b SEATING (foot 3.8 mm above and 272 mm forward
    // of the board socket -- the sockets are frame bolts, not the sole's
    // spot); the arm rows are the FIST: `hand_*` is the wrist and the KNUCKLE
    // is on the bar (seat_sudburian.py), so the wrist reads 20 mm out, 51 mm
    // up, 83 mm aft of the grip socket = 98 mm, one fist. The handlebar
    // assembly itself was carried in from the live session the same day
    // (splice_bar.py) after the file's bar was found 100 mm stale.
    glm::vec3 rest_tip_minus_socket_m{0.0f};
};

// ★ THE DO-NOT-RENAME LIST (§2.3a). Machine-side, not rider-side. Renaming any
// of these silently breaks steering, suspension or the rider IK. Pinned here so
// a future rename is a red test and not a mystery bug report.
enum MachineNode {
    kMachSteerPivot = 0,
    kMachSuspL = 1,
    kMachSuspR = 2,
    kMachSuspT = 3,
    kMachCam = 4,
    kMachSkiL = 5,
    kMachSkiR = 6,
    kMachGripSocketL = 7,
    kMachGripSocketR = 8,
    kMachBoardSocketL = 9,
    kMachBoardSocketR = 10,
    kMachineNodeCount = 11,
};

struct MachineNodeSpec {
    const char* name = "";  // exact glTF node name -- DO NOT RENAME
    // Sign of the node's WORLD x in the shipped asset: -1 for the "_L" side
    // (which is model -X, the rider's anatomical RIGHT -- see the L/R trap
    // above), +1 for "_R", 0 for a centreline node. This is the check that
    // catches a HALF-done R2 rename, where the rider flips to anatomical
    // naming and the machine does not.
    int world_x_sign = 0;
};

// ★ THE EXPORT-PAYLOAD LIST. These are NOT joints and NOT sockets -- they are
// the mesh/accessory nodes that a WRONG EXPORT CHECKBOX silently deletes, and
// this list exists to catch a bad export, not a code regression. Do not "clean
// it up" later on the grounds that nothing in the C++ reads these names.
//
// THE TRAP, by name: indy650.glb was exported by hand with
// bpy.ops.export_scene.gltf. Run with `use_visible=True` instead of
// `use_renderable=True`, the operator DROPS every viewport-hidden object even
// though its hide_render is False. A GLB exported that way once shipped, and
// it was caught only by someone manually diffing node lists: it parses fine,
// it loads fine, and the rider is simply not in it. A joint-name check does
// NOT catch it, because the ARMATURE survives and only the skinned mesh and
// the accessories vanish. Since 2026-08-18 the rider is the single skinned
// `sudburian_proxy` (the R2b blockout; R2c dresses it) and it comes in through
// splice_sudburian.py, whose verify() would also refuse a file without it --
// belt and braces, both loud.
enum ExportPayloadNode {
    kPayloadSudburianProxy = 0,  // the skinned blockout, 1176 v (392 unique)
    kPayloadSnowFlap = 1,        // machine-side, same failure mode
    kPayloadPanFrontBar = 2,     // machine-side, same failure mode
    kExportPayloadCount = 3,
};

struct ExportPayloadSpec {
    const char* name = "";
    bool has_mesh = true;
};

// The catalogues. Fixed size, enum-indexed: a missing entry cannot compile.
const std::array<RiderJointSpec, kRiderJointCount>& rider_joint_specs();
const std::array<RiderChainSpec, kRiderChainCount>& rider_chain_specs();
const std::array<MachineNodeSpec, kMachineNodeCount>& machine_node_specs();
const std::array<ExportPayloadSpec, kExportPayloadCount>& export_payload_specs();

// Resolve a declared glTF node name to an index in some host scene graph.
// Returning a negative value means ABSENT. This is the only place a -1 is
// allowed to appear, and bind_rider_rig turns it into a named failure
// immediately, so it can never propagate to a caller that ignores it.
using NodeLookup = std::function<int(const char*)>;

// The explicit binding result. `missing` is the whole point: absence is a
// value you have to look at, not a -1 you can forget to test.
struct RigBindResult {
    std::array<int, kRiderJointCount> joint{};
    std::array<int, kMachineNodeCount> machine{};
    std::vector<std::string> missing;  // every absent declared node, in order

    bool complete() const { return missing.empty(); }
};

// Bind the whole declared catalogue through `lookup`. Never throws, never
// asserts: it reports. Callers decide what a partial rig means for them -- but
// they cannot fail to be told.
RigBindResult bind_rider_rig(const NodeLookup& lookup);

// Convenience overload: bind against a flat list of node names (index = the
// position in the list). This is what the validator test drives -- it can build
// the real GLB's name list, DELETE one entry, and watch the bind fail loudly,
// without ever mutating the shipped asset on disk.
RigBindResult bind_rider_rig(const std::vector<std::string>& node_names);

// One-line, log-ready summary of what is absent. Empty string when complete.
std::string describe_missing(const RigBindResult& r);

// Which export-payload nodes are absent from `node_names`. Empty = the export
// carried the rider. Non-empty = someone ticked use_visible.
std::vector<std::string> missing_export_payload(
    const std::vector<std::string>& node_names);

// Geometry contract. `local_t_m[j]` is the measured local translation of joint
// j in the file under test; returns the RiderJoint indices whose translation
// differs from the catalogue by more than tol_m in any component. Empty =
// the skeleton the code was written against is the skeleton that shipped.
std::vector<int> validate_rider_rest_geometry(
    const std::array<glm::vec3, kRiderJointCount>& local_t_m, float tol_m);

}  // namespace render
