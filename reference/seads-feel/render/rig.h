#pragma once

#include <array>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>

#include "sim/state.h"  // sim::Inputs — the commanded surface deflection (rig-B)

// Fleet Rig — the render-only articulated aircraft (docs/fleet_rig_plan.md,
// /orchestrate rig-A). A flat, topologically-sorted scene hierarchy: one linear
// pass composes every node's world matrix (cache-friendly, no pointer chasing,
// no recursion). PURE: glm + std only, ZERO raylib — so it lives in
// seads_render_core and the gate pins it headlessly (the sphere_param pattern).
// render/draw.cpp (app-target-only) consumes this: it wraps each node's world
// matrix through to_ray() and calls DrawMesh.
//
// RENDER-ONLY (RA9 + SPEC §6): the rig reads sim state, never writes it, never
// feeds a control/sim tick. Cosmetic surface deflection + prop (rig-B) are a
// downstream function of already-computed state; nothing here is an input to a
// later physics tick.
//
// Body frame (SPEC §7): +X right, +Y up, -Z forward (nose = -Z). Rest poses and
// hinge axes below are all in this frame.

namespace render {

// Fixed node indices (parent index < own index, so updateRig is one forward
// pass). rig-D: the real Bf 109 F-4 airframe split into ~30 SEPARABLE damage
// components (Chad's fine-grained granularity call, 2026-07-09). Topologically
// ordered: every node's parent index is strictly less than its own. Fuselage is
// the root; the mesh for each node is authored in Blender (D.2) and ingested
// per node (D.3). Legacy 9-node names (kPropeller/kLandingGear) are RETIRED —
// the real rig splits the prop into spinner+blades and the gear into
// strut/wheel/door per side. Keep this enum in lockstep with
// aircraft_node_specs() in rig.cpp.
enum NodeIndex {
    kFuselage = 0,    // root monocoque
    kEngineCowl = 1,  // DB 601 cowl (child of fuselage)
    kSpinner = 2,     // prop spinner, nose tip (-Z)
    kPropBlades = 3,  // 3-blade VDM airscrew (child of spinner); render = disc
    kCanopy = 4,      // glass canopy (MaterialClass::Glass)
    kPilot = 5,       // seated pilot bust (MaterialClass::Matte)
    kLeftWing = 6,
    kLeftAileron = 7,  // child of kLeftWing
    kLeftFlap = 8,     // child of kLeftWing (static — no flap Input in v1)
    kLeftWingtip = 9,  // child of kLeftWing (rounded F-series tip)
    kRightWing = 10,
    kRightAileron = 11,   // child of kRightWing
    kRightFlap = 12,      // child of kRightWing
    kRightWingtip = 13,   // child of kRightWing
    kVertStab = 14,       // vertical stabiliser / fin
    kRudder = 15,         // child of kVertStab
    kLeftHorizStab = 16,  // left tailplane
    kLeftElevator = 17,   // child of kLeftHorizStab
    kRightHorizStab = 18,
    kRightElevator = 19,  // child of kRightHorizStab
    kLeftGearDoor =
        20,  // hinges on the WING BAY EDGE, parent fuselage (Fable P1-3)
    kLeftGearStrut = 21,  // main gear leg (child of fuselage)
    kLeftWheel = 22,      // child of kLeftGearStrut (retracts WITH the leg)
    kRightGearDoor = 23,
    kRightGearStrut = 24,
    kRightWheel = 25,    // child of kRightGearStrut
    kTailWheel = 26,     // retractable tailwheel (child of fuselage)
    kRadiator = 27,      // ventral/under-wing radiator bath
    kLeftExhaust = 28,   // exhaust stack bank (child of cowl)
    kRightExhaust = 29,  // child of cowl
    kNodeCount = 30,
};

// The render material a node is drawn with (rig-D D.3). Mirror = the pop-art
// monochrome-saturation mirror finish (the whole airframe). Glass = translucent
// canopy (drawn in the back-to-front translucent pass, Fable P1-4). Matte = a
// non-mirror diffuse (the pilot reads as a figure inside, not chrome).
enum class MaterialClass { Mirror, Glass, Matte };

// Which control Input (sim::Inputs) drives a node's cosmetic deflection
// (rig-B). The mapping is applied about the node's hinge_axis; the SIGN is
// DERIVED from the plant convention (+Input = +ω about the +body axis,
// step.cpp:91-97) and the real-aircraft surface sense, then pinned per axis
// (test_rig). None = a static node (wings, fuselage, flaps — no flap Input in
// v1). Gear = the unfold, driven by the ACTUAL slewed state.gear (not a
// commanded Input) — 0 tucks in-bay, 1 deploys. Wheel = ground-roll spin (R4
// solid ground): driven by an APP-ACCUMULATED roll angle (tick-derived, never a
// render clock — the prop's time-free discipline), rotating the main tyres
// about their lateral hinge while the airframe rolls.
enum class Driven { None, Pitch, Roll, Rudder, RollMirrored, Gear, Wheel };

// One node's authored spec (the "catalog" the asset validator checks in rig-A.3
// and rig-B's animation reads). box_dims is the local-space size of the
// placeholder GenMeshCube (rig-A.2) — shape lives in the MESH so every node's
// scl stays UNIFORM (Fable: a non-uniformly-scaled parent shears its rotating
// children; here all scl == 1, so the invariant holds trivially).
struct NodeSpec {
    int parent = -1;
    glm::vec3 rest_pos{0.0f};
    glm::quat rest_rot{1, 0, 0, 0};
    glm::vec3 box_dims{1.0f};  // component AABB (metre); the placeholder
                               // GenMeshCube size AND the validator units
                               // reference (the real GLB AABB must match).
    glm::vec3 hinge_axis{1, 0,
                         0};  // CANONICAL body-frame axis for deflection
                              // (rest_rot absorbs real sweep/dihedral, Fable
                              // P1-2). Meaningful only for driven surfaces.
    Driven driven = Driven::None;
    MaterialClass material = MaterialClass::Mirror;  // draw material (D.3)
    const char* mesh_key = "";  // assets/bf109/<mesh_key>.glb (D.2/D.3); ""
                                // = placeholder GenMeshCube until ingested.
};

struct SceneNode {
    glm::vec3 pos{0.0f};
    glm::quat rot{1, 0, 0, 0};
    glm::vec3 scl{1.0f};
    int parent = -1;        // index into the flat array, -1 = root
    glm::mat4 world{1.0f};  // cache, recomputed each frame by updateRig
    std::uint8_t flags =
        0;  // bit0 attached, bit1 destroyed, bit2 animating (rig-C)
    float hp = 1.0f;
};
using Rig = std::vector<SceneNode>;

// Local TRS. Order T * R * S: scale, then rotate, then translate (standard
// column-vector convention p_world = local * p_local).
inline glm::mat4 local(const SceneNode& n) {
    return glm::translate(glm::mat4(1.0f), n.pos) * glm::mat4_cast(n.rot) *
           glm::scale(glm::mat4(1.0f), n.scl);
}

// One linear pass: world[i] = parent.world * local[i] (parent-on-left, column
// vectors). Requires parent index < own index (topological order) — asserted at
// build. These are MODEL-space world matrices (root at origin); draw.cpp
// composes the eye-relative body-to-world transform on the outside, so the big
// ~km translation never enters a child node's local (Fable Claim 4 precision).
void updateRig(Rig& r);

// The load-bearing glm -> raylib Matrix bridge, PURE and testable (Fable Claim
// 1). glm::mat4 memory is column-major; raylib's Matrix struct stores fields
// ROW-major (m0,m4,m8,m12 = row 0), so a raw memcpy TRANSPOSES. This returns
// the 16 floats in raylib's FIELD-DECLARATION order (m0,m4,m8,m12,m1,m5,...),
// so draw.cpp aggregate-inits `Matrix{f[0],...,f[15]}` with no transpose
// reaching the GPU. Element identity: field for (row r, col c) == glm g[c][r].
std::array<float, 16> to_ray_fields(const glm::mat4& g);

// Build the placeholder aircraft rig (9 nodes, rest pose in body frame). Shapes
// live in NodeSpec.box_dims (uniform node scale). Returns the specs (the
// catalog rig-A.2 meshes and rig-A.3 validates) and the SceneNode array.
const std::array<NodeSpec, kNodeCount>& aircraft_node_specs();
Rig build_aircraft_rig();

// Cosmetic deflection magnitudes [radians] at full command (rig-B). FELT knobs
// from config/world.toml [fleet_rig] (degrees at the config edge). The
// DIRECTION signs are structural (in rig.cpp, tied to the mesh hinge); these
// are the positive magnitudes.
struct DeflectGains {
    float aileron_rad = 0.0f;   // roll surfaces at |roll| = 1
    float elevator_rad = 0.0f;  // elevator at |pitch| = 1
    float rudder_rad = 0.0f;    // rudder at |yaw| = 1
    float gear_deploy_rad =
        0.0f;  // gear swing at full extension (gear_ext = 1)
};

// Pose the rig's driven nodes (rig-B), then call updateRig. PURE and read-only:
//   - control surfaces from the COMMANDED Inputs `ctrl` (the true surface
//     position — in this plant Input IS the normalized deflection, causally
//     correct and lag-free, Fable C3): elevator<-ctrl.pitch, rudder<-ctrl.yaw,
//     R-aileron<-+ctrl.roll, L-aileron<--ctrl.roll (antisymmetric). Signs
//     folded into the per-Driven signal map below (ONE place).
//   - the gear node from the ACTUAL slewed extension `gear_ext` in [0,1]
//     (state.gear), 0 = tucked in-bay, 1 = deployed.
//   - the main tyres from `wheel_roll_rad` (R4): the APP-accumulated ground-
//     roll angle (tick-derived; render reads no clock). Defaulted 0 = the
//     rest pose bit-exactly, so every pre-wheel caller is a strict superset.
// Static nodes keep rest_rot. Never writes sim/control state (RA9); the caller
// passes ctrl by value and gear_ext by value.
void apply_deflection(Rig& r, const sim::Inputs& ctrl, float gear_ext,
                      const DeflectGains& g, float wheel_roll_rad = 0.0f);

// The mirror-finish shader (GLSL 330) sources. Kept in the PURE core (not
// draw.cpp) so the asset-validator ctest (rig-A.3) can pin their DECLARED
// uniforms against an allowlist HEADLESSLY (no GL context). Declared ⊇ active,
// so declared ⊆ allowlist is a stronger check than the GL active-uniform set —
// the strongest form of Fable's #1 fix, closing a stray clock/state uniform
// backdoor at the source. draw.cpp loads them via LoadShaderFromMemory.
//
// BOTH strings are explicit: raylib's DEFAULT vertex shader (GL33, rlgl.h)
// outputs only fragTexCoord/fragColor with uniform mvp — it does NOT supply the
// fragPosition/fragNormal the mirror FS reads. So the VS must be authored (the
// planet.cpp pattern); relying on the default VS silently drops the reflection
// (undefined FS inputs) or fails to link on a strict driver (Fable
// after-consult P0). DrawMesh auto-uploads mvp/matModel/matNormal by name.
const char* mirror_vs_source();
const char* mirror_fs_source();

}  // namespace render
