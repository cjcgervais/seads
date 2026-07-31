#include "render/rig.h"

#include <glm/gtc/type_ptr.hpp>

namespace render {

void updateRig(Rig& r) {
    for (std::size_t i = 0; i < r.size(); ++i)
        r[i].world = (r[i].parent < 0) ? local(r[i])
                                       : r[r[i].parent].world * local(r[i]);
}

std::array<float, 16> to_ray_fields(const glm::mat4& g) {
    // raylib Matrix field order: m0,m4,m8,m12, m1,m5,m9,m13, m2,m6,m10,m14,
    // m3,m7,m11,m15 (row-major by name). Field for (row r, col c) is glm
    // g[c][r] (glm is column-major: g[col][row]). Translation lands in
    // m12/m13/m14 = indices 3/7/11.
    return {
        g[0][0], g[1][0], g[2][0], g[3][0],  // m0  m4  m8  m12  (row 0)
        g[0][1], g[1][1], g[2][1], g[3][1],  // m1  m5  m9  m13  (row 1)
        g[0][2], g[1][2], g[2][2], g[3][2],  // m2  m6  m10 m14  (row 2)
        g[0][3], g[1][3], g[2][3], g[3][3],  // m3  m7  m11 m15  (row 3)
    };
}

// Placeholder rest pose (metre-scale, body frame). Structural art like the
// current DrawCube dims (draw.cpp) — the FELT knobs (mirror/colors/gains) move
// to config in rig-A.2/rig-B, not these. All scl == 1 (uniform): shape lives in
// box_dims (the GenMeshCube size), so the leaf-only-uniform-scale invariant
// holds trivially.
const std::array<NodeSpec, kNodeCount>& aircraft_node_specs() {
    static const std::array<NodeSpec, kNodeCount> specs = [] {
        std::array<NodeSpec, kNodeCount> s{};
        using M = MaterialClass;
        const glm::quat I{1, 0, 0, 0};
        const glm::vec3 X{1, 0, 0}, Y{0, 1, 0}, Z{0, 0, 1};
        const float d2r = 3.14159265358979f / 180.0f;
        auto Rx = [&](float deg) { return glm::angleAxis(deg * d2r, X); };
        auto Ry = [&](float deg) { return glm::angleAxis(deg * d2r, Y); };
        auto Rz = [&](float deg) { return glm::angleAxis(deg * d2r, Z); };
        // Wing control-surface tilt (rig-D round 2, Fable 2026-07-10): the wing
        // dihedral+sweep baked into rest_rot so the CANONICAL +X hinge lies on
        // the wing hinge line and the surface sits flush in its inset bay
        // across the whole span. R_world = rest_rot ∘ angleAxis(θ,+X) still
        // deflects cleanly. Left/right are TRUE mirrors (Rz∓·Ry±), like the
        // gear splay pair.
        const glm::quat surfL = Rz(-2.395f) * Ry(2.239f);
        const glm::quat surfR = Rz(2.395f) * Ry(-2.239f);
        // rig-D: the Bf 109 F-4 to published dims (span 9.925 m, length 8.85 m,
        // height 2.60 m, ~3.0 m VDM airscrew). Body frame +X right/+Y up/-Z
        // nose; origin ~ wing quarter-chord (CG). Positions are metre-scale;
        // child positions are PARENT-local. box_dims = the component AABB
        // (placeholder cube + validator units ruler). Hinge axes are CANONICAL
        // (rest_rot absorbs real sweep — none needed here, the surfaces sit
        // square). meshes land in assets/bf109/<key>.glb (D.2); "" until then =
        // GenMeshCube.

        // --- Fuselage group -------------------------------------------------
        // box_dims RECONCILED to the exported GLB AABBs (rig-D D.2/D.1b) — the
        // real meshes, not the placeholder estimates. Verified per GLB.
        s[kFuselage] = {-1, {0, 0, 0.9f}, I,         {1.06f, 1.31f, 7.6f},
                        X,  Driven::None, M::Mirror, "fuselage"};
        // engine_cowl box y 1.08->1.12 (cowl MG Beule bulges welded, rig-D
        // guns); spinner box z 0.7->0.77 (hub 20mm muzzle collar proud of the
        // tip).
        s[kEngineCowl] = {
            kFuselage, {0, 0.05f, -2.5f}, I,         {1.06f, 1.12f, 1.6f},
            X,         Driven::None,      M::Mirror, "engine_cowl"};
        s[kSpinner] = {
            kFuselage,    {0, 0, -3.35f}, I,        {0.55f, 0.55f, 0.77f}, X,
            Driven::None, M::Mirror,      "spinner"};  // nose (-Z marker)
        s[kPropBlades] = {
            kSpinner, {0, 0, -0.1f}, I,         {3.0f, 3.0f, 0.11f},
            Z,        Driven::None,  M::Mirror, "prop_blades"};  // render=disc
        s[kCanopy] = {
            kFuselage, {0, 0.75f, -0.6f}, I,        {0.69f, 0.53f, 1.9f},
            X,         Driven::None,      M::Glass, "canopy"};
        s[kPilot] = {
            kFuselage, {0, 0.55f, -0.3f}, I,        {0.48f, 0.66f, 0.46f},
            X,         Driven::None,      M::Matte, "pilot"};

        // --- Left wing group ------------------------------------------------
        // wing box y 0.42->0.58 / z 2.05->2.40 (underwing 20mm gondola pod +
        // forward barrel welded, rig-D guns leg).
        s[kLeftWing] = {
            kFuselage, {-2.55f, -0.25f, 0.2f}, I,         {4.6f, 0.58f, 2.40f},
            X,         Driven::None,           M::Mirror, "wing_l"};
        s[kLeftAileron] = {kLeftWing, {-1.60f, 0.179f, 0.629f},
                           surfL,     {1.24f, 0.13f, 0.44f},
                           X,         Driven::RollMirrored,
                           M::Mirror, "aileron_l"};
        s[kLeftFlap] = {kLeftWing, {-0.05f, 0.114f, 0.568f},
                        surfL,     {1.64f, 0.16f, 0.56f},
                        X,         Driven::None,
                        M::Mirror, "flap_l"};  // static in v1
        s[kLeftWingtip] = {
            kLeftWing, {-2.3f, 0.20f, -0.1f}, I,         {0.40f, 0.17f, 1.05f},
            X,         Driven::None,          M::Mirror, "wingtip_l"};

        // --- Right wing group -----------------------------------------------
        s[kRightWing] = {
            kFuselage, {2.55f, -0.25f, 0.2f}, I,         {4.6f, 0.58f, 2.40f},
            X,         Driven::None,          M::Mirror, "wing_r"};
        s[kRightAileron] = {kRightWing, {1.60f, 0.179f, 0.629f},
                            surfR,      {1.24f, 0.13f, 0.44f},
                            X,          Driven::Roll,
                            M::Mirror,  "aileron_r"};
        s[kRightFlap] = {kRightWing, {0.05f, 0.114f, 0.568f},
                         surfR,      {1.64f, 0.16f, 0.56f},
                         X,          Driven::None,
                         M::Mirror,  "flap_r"};
        s[kRightWingtip] = {
            kRightWing, {2.3f, 0.20f, -0.1f}, I,         {0.40f, 0.17f, 1.05f},
            X,          Driven::None,         M::Mirror, "wingtip_r"};

        // --- Empennage ------------------------------------------------------
        // Empennage moved fwd (z 4.6->4.05 / 4.5->3.95) so the fin+hstab seat
        // ON the fuselage tail cone instead of cantilevering off it; fin raised
        // (y 0.85->1.00) + taller; hstab chord slimmed; elevator tips 45-raked.
        // (Chad 2026-07-10 tail refit.) box_dims = measured GLB AABBs.
        s[kVertStab] = {
            kFuselage, {0, 1.00f, 4.05f}, I,         {0.14f, 1.60f, 1.125f},
            X,         Driven::None,      M::Mirror, "vert_stab"};
        s[kRudder] = {kVertStab, {0, 0.0f, 0.75f},
                      I,         {0.11f, 1.31f, 0.605f},
                      Y,         Driven::Rudder,
                      M::Mirror, "rudder"};  // hinge +Y (yaw)
        s[kLeftHorizStab] = {
            kFuselage, {-1.15f, 0.35f, 3.95f}, I,         {2.0f, 0.10f, 1.005f},
            X,         Driven::None,           M::Mirror, "hstab_l"};
        s[kLeftElevator] = {kLeftHorizStab,
                            {0, 0, 0.75f},
                            I,
                            {2.0f, 0.12f, 0.586f},
                            X,
                            Driven::Pitch,
                            M::Mirror,
                            "elevator_l"};
        s[kRightHorizStab] = {
            kFuselage, {1.15f, 0.35f, 3.95f}, I,         {2.0f, 0.10f, 1.005f},
            X,         Driven::None,          M::Mirror, "hstab_r"};
        s[kRightElevator] = {kRightHorizStab,
                             {0, 0, 0.75f},
                             I,
                             {2.0f, 0.12f, 0.586f},
                             X,
                             Driven::Pitch,
                             M::Mirror,
                             "elevator_r"};

        // --- Landing gear (rest_pos = DEPLOYED/down; gear_ext=0 retracts) ----
        // RUGGED ARCADE STANCE (rig-D D.3b, Fable consult 2026-07-10): the
        // mains sit on a wide, planted, splayed+raked tripod for rough Sudbury
        // fields / frozen lakes. The splay/rake live in rest_rot (NOT a tilted
        // hinge — hinge stays the canonical ±Z retract axis, Fable P1-2);
        // R_world = rest_rot ∘ Rz(θ). Because splay is a Z-rotation it COMMUTES
        // with the retract (same axis) — it just biases the sweep endpoint, so
        // the tuck stays inside the wing box.
        //   splay σ = 12°  : bottom of each main kicks OUTWARD (wide track ~2.1
        //   m) rake  r =  5°  : strut bottom forward of the pivot (nose-over
        //   margin)
        // P0-1 (Fable): the RIGHT main/door retract MIRRORED, so their hinge is
        // −Z (about +Z, the shared (ext−1)·gain angle would sweep the right leg
        // across the belly). Left = +Z. Wheels are static children at the strut
        // bottom; each carries a counter-rotation (Rx(−5°)·Rz(±9°)) so the tyre
        // stands ~3° camber on the ground, not the full 12° strut splay (a
        // canted tyre reads "broken"; Fable P1-6). Doors are fuselage-parented
        // Driven:: Gear fairings given the SAME splay/rake + mirrored hinge as
        // their strut so they track it in deploy AND retract without the D.3b
        // enum reorder.
        const glm::vec3 Zneg{0, 0, -1};
        const glm::quat strutL =
            Rz(-12.0f) * Rx(5.0f);  // left: splay out + rake
        const glm::quat strutR = Rz(12.0f) * Rx(5.0f);  // right: mirror splay
        const glm::quat wheelL = Rx(-5.0f) * Rz(9.0f);  // counter to ~3° camber
        const glm::quat wheelR = Rx(-5.0f) * Rz(-9.0f);
        s[kLeftGearDoor] = {
            kFuselage, {-0.7f, -0.55f, -1.3f}, strutL,    {0.06f, 0.6f, 1.0f},
            Z,         Driven::Gear,           M::Mirror, "gear_door_l"};
        s[kLeftGearStrut] = {
            kFuselage, {-0.85f, -0.7f, -1.4f}, strutL,    {0.2f, 1.08f, 0.18f},
            Z,         Driven::Gear,           M::Mirror, "gear_strut_l"};
        s[kLeftWheel] = {
            kLeftGearStrut, {0, -1.05f, 0}, wheelL,   {0.25f, 0.65f, 0.65f}, X,
            Driven::Wheel,  M::Matte,       "wheel_l"};
        s[kRightGearDoor] = {
            kFuselage, {0.7f, -0.55f, -1.3f}, strutR,    {0.06f, 0.6f, 1.0f},
            Zneg,      Driven::Gear,          M::Mirror, "gear_door_r"};
        s[kRightGearStrut] = {
            kFuselage, {0.85f, -0.7f, -1.4f}, strutR,    {0.2f, 1.08f, 0.18f},
            Zneg,      Driven::Gear,          M::Mirror, "gear_strut_r"};
        s[kRightWheel] = {
            kRightGearStrut, {0, -1.05f, 0}, wheelR,   {0.25f, 0.65f, 0.65f}, X,
            Driven::Wheel,   M::Matte,       "wheel_r"};
        s[kTailWheel] = {
            kFuselage, {0, -0.35f, 4.7f}, I,        {0.12f, 0.45f, 0.25f},
            X,         Driven::Gear,      M::Matte, "tail_wheel"};  // hinge +X

        // --- Furniture ------------------------------------------------------
        // GEAR-BAY fairing (Chad 2026-07-10): repurposed from the ventral
        // radiator bath — moved fwd to the gear (z -1.35, over the strut top
        // pivot y -0.7) so the mains deploy OUT OF it. Slim (only slightly
        // wider than the fuselage) with chamfered/smoothed edges — a moulded
        // fairing, not a slab.
        s[kRadiator] = {
            kFuselage, {0, -0.55f, -1.35f}, I,         {1.3f, 0.26f, 1.15f},
            X,         Driven::None,        M::Mirror, "radiator"};
        s[kLeftExhaust] = {kEngineCowl, {-0.55f, -0.05f, 0.2f},
                           I,           {0.27f, 0.2f, 0.62f},
                           X,           Driven::None,
                           M::Mirror,   "exhaust_l"};
        s[kRightExhaust] = {
            kEngineCowl, {0.55f, -0.05f, 0.2f}, I,         {0.27f, 0.2f, 0.62f},
            X,           Driven::None,          M::Mirror, "exhaust_r"};
        return s;
    }();
    return specs;
}

// The mirror-finish VERTEX shader (GLSL 330). Authored explicitly (the
// planet.cpp kVS pattern) because raylib's default VS does NOT emit
// fragPosition/fragNormal (Fable after-consult P0). fragPosition is the
// EYE-RELATIVE world position (matModel is the eye-relative body*node transform
// draw.cpp bakes, so matModel*vertex == vertex - eye, camera at origin);
// fragNormal is the world normal via matNormal (inverse-transpose, robust to
// scale). DrawMesh auto-uploads mvp/matModel/matNormal by name.
const char* mirror_vs_source() {
    return R"(#version 330
in vec3 vertexPosition;
in vec3 vertexNormal;
uniform mat4 mvp;
uniform mat4 matModel;   // eye-relative body*node transform (auto-set)
uniform mat4 matNormal;  // inverse-transpose of matModel (auto-set)
out vec3 fragPosition;   // eye-relative world position (camera at origin)
out vec3 fragNormal;     // world normal
void main() {
    fragPosition = (matModel * vec4(vertexPosition, 1.0)).xyz;
    fragNormal = normalize((matNormal * vec4(vertexNormal, 0.0)).xyz);
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)";
}

// The mirror-finish FRAGMENT shader (GLSL 330). There is NO clock/time uniform
// — the finish is a pure function of geometry + sun + config (seam: no clock in
// render/). Fable C2: N is renormalized in the FS (interpolated normals shrink;
// a non-unit N makes reflect() wrong, not dim). The DECLARED uniform set here
// is pinned against an allowlist by the rig-A.3 asset validator.
const char* mirror_fs_source() {
    return R"(#version 330
in vec3 fragPosition;
in vec3 fragNormal;
uniform samplerCube env;       // grayscale planet albedo cubemap (read-only)
uniform vec3  u_planeColor;    // this plane's saturated chroma (the only color)
uniform vec3  u_sunDir;        // world light-travel dir (sun -> scene)
uniform float u_fresnelPower;  // rim exponent (higher = tighter edge blaze)
uniform float u_reflectivity;  // env-reflection strength over the body [0,1]
out vec4 finalColor;
void main() {
    vec3 V = normalize(fragPosition);          // eye -> fragment
    vec3 N = normalize(fragNormal);            // renormalize (interp shrinks it)
    vec3 R = reflect(V, N);                    // world reflection direction
    vec3 envCol = texture(env, R).rgb;         // grayscale world reflection
    // Fresnel rim: ~0 head-on, ~1 at grazing so the silhouette edge blazes.
    float fres = pow(1.0 - max(dot(-V, N), 0.0), u_fresnelPower);
    // Lambert shade of the plane's own chroma. The 0.35/0.65 split is FORM, not
    // a felt knob (cf. the planet FS bare 0.38); the mirror knobs are config.
    float ndl = max(dot(N, -normalize(u_sunDir)), 0.0);
    vec3 body = u_planeColor * (0.35 + 0.65 * ndl);
    // Grayscale reflection over the body; the Fresnel rim pushes back to pure
    // chroma so the edge is the color accent (the only saturation in-world).
    vec3 col = mix(body, envCol, u_reflectivity * (1.0 - fres));
    col = mix(col, u_planeColor, fres);
    // Tight sun glint (Blinn-Phong, high exponent).
    vec3 H = normalize(-normalize(u_sunDir) - V);
    col += vec3(pow(max(dot(N, H), 0.0), 64.0)) * ndl;
    finalColor = vec4(col, 1.0);
}
)";
}

Rig build_aircraft_rig() {
    const auto& specs = aircraft_node_specs();
    Rig r(kNodeCount);
    for (int i = 0; i < kNodeCount; ++i) {
        r[i].parent = specs[i].parent;
        r[i].pos = specs[i].rest_pos;
        r[i].rot = specs[i].rest_rot;
        r[i].scl = glm::vec3(1.0f);  // uniform: shape is in box_dims
    }
    updateRig(r);
    return r;
}

// The per-Driven signal: the commanded Input component with the DIRECTION SIGN
// folded in (ONE place — Fable P1 antisymmetry). Derived from the plant
// (step.cpp:91-97: +Input => +ω about the +body axis) and the real-aircraft
// surface sense; pinned per axis in test_rig:
//   +pitch = pitch-up (+ωx) => elevator TE up. About the +X hinge, +θ swings
//   the
//     aft (+Z) trailing edge toward −Y (down) = pitch-DOWN, so pitch-up needs
//     −θ.
//   +yaw = yaw-left (+ωy) => rudder TE left (−X). About +Y, +θ swings +Z toward
//     +X (right), so yaw-left needs −θ.
//   +roll = roll-left (+ωz) => right aileron TE down (+θ about +X), left
//   aileron
//     TE up (−θ): antisymmetric via the ±roll signal.
// gear_ext in [0,1] drives the unfold off the DEPLOYED rest pose: (ext−1)·gain,
// so ext=1 => 0 (rest/down), ext=0 => −gain (retracted up).
void apply_deflection(Rig& r, const sim::Inputs& ctrl, float gear_ext,
                      const DeflectGains& g, float wheel_roll_rad) {
    const auto& specs = aircraft_node_specs();
    for (int i = 0; i < kNodeCount; ++i) {
        const NodeSpec& s = specs[i];
        float angle = 0.0f;
        switch (s.driven) {
            case Driven::Pitch:
                angle = -ctrl.pitch * g.elevator_rad;
                break;
            case Driven::Rudder:
                angle = -ctrl.yaw * g.rudder_rad;
                break;
            case Driven::Roll:
                angle = ctrl.roll * g.aileron_rad;
                break;
            case Driven::RollMirrored:
                angle = -ctrl.roll * g.aileron_rad;
                break;
            case Driven::Gear:
                angle = (glm::clamp(gear_ext, 0.0f, 1.0f) - 1.0f) *
                        g.gear_deploy_rad;
                break;
            case Driven::Wheel:
                // Ground roll (R4): forward motion (-Z) spins the tyre top
                // toward the nose = NEGATIVE rotation about the +X hinge
                // (right-hand rule: +theta about +X carries +Y toward +Z, the
                // tail). The caller accumulates the angle POSITIVE for
                // forward roll.
                angle = -wheel_roll_rad;
                break;
            case Driven::None:
            default:
                break;
        }
        r[i].rot = (s.driven == Driven::None)
                       ? s.rest_rot
                       : s.rest_rot * glm::angleAxis(angle, s.hinge_axis);
    }
    updateRig(r);
}

}  // namespace render
