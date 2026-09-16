// ★ WINTER S3-ART GLTF WIRING -- see sled_model.h for the contract.
//
// COORDINATE LAW (measured off the GLB itself, 2026-08-12 -- do not trust
// doc prose over the file): model forward = +Z (hood/windshield/headlight
// at +Z, taillight/snow-flap at -Z), up = +Y, ground plane y=0, and the
// RIDER'S LEFT = +X (a +Z-facing body with +Y up has left = up x fwd = +X).
// The "_L"-suffixed nodes sit at MODEL -X: the .blend naming is front-view
// viewer-relative, i.e. "_L" parts are on the rider's RIGHT. Kernel patch 0
// (ski L = the rider's left) therefore binds to the CH_susp node at +X --
// the binding below resolves by NODE POSITION, never by name, so it stays
// honest even if the naming is ever fixed in the .blend.
//
// The mount maps model->body with R_y(180deg): model +Z (fwd) -> body -Z
// (game fwd), model +X (rider left) -> body -X (game left). Pure rotation,
// no mirror -- left stays left.

#include "render/sled_model.h"

#include <algorithm>  // std::clamp -- the helmet gloss
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>  // R3-HANDS: the shell-split's union-find
#include <map>         // R3-HANDS: weld-by-position
#include <string>
#include <vector>

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>

#include "raylib.h"
#include "rlgl.h"        // additive/depth-mask state for the headlight beam
#include "render/rider_pose.h"  // the PURE pose math (defects 6-11, 14)
#include "render/rider_rig.h"  // the DECLARED joint catalogue (defect 13/15)
#include "render/rig.h"  // to_ray_fields (glm -> raylib Matrix bridge)
#include "render/body_chain.h"  // ★ R4a: the MEASURED body
#include "render/body_blend.h"  // ★ R4a rung 2: the drawn legs
#include "render/body_drive.h"  // ★ R4a rung 2b: the chain's drive POLICY
#include "render/flak_pose.h"   // ★ G2c: GunnerAnthro::ankle_up_m -- the one
                                // measured boot-sole->ankle number (0.10 m)
#include "render/rider_load.h"  // ★ R4a: the rod model = THE STAGE SELECTOR
#include "sim/walker.h"  // ★ R4c §7.3 stages 4-7: the man, once he is off
#include "sim/gait.h"    // ★★★ GAIT LADDER G1: his feet, off the same walker
#include "render/scarf_drape.h"
#include "render/trail_chain.h"  // ★ SCARF_SPEC §3: the PURE trailing chain
#include "render/team_color.h"  // kSlagOrange: the one warm stop, single-sourced
#include "render/team_kit.h"    // player_kit(): the side's four colours
#include "render/sting_deploy.h"  // ★★★ ST-5: the un-hunch's PURE arithmetic

// cgltf: the implementation is compiled inside raylib's rmodels.c (C
// linkage, non-static) -- declare only, never define CGLTF_IMPLEMENTATION
// here or the link gets duplicate symbols.
extern "C" {
#include "external/cgltf.h"
}

namespace render {
namespace {

Matrix to_ray_m(const glm::mat4& g) {
    const std::array<float, 16> f = to_ray_fields(g);
    Matrix m;
    std::memcpy(&m, f.data(), sizeof m);
    return m;
}

struct SNode {
    std::string name;
    int parent = -1;
    glm::vec3 t{0.0f};
    glm::quat r{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 s{1.0f};
    // per-frame pose scratch
    glm::mat4 local{1.0f};  // CHANNEL-MODIFIED local, built each frame --
                            // every later pass must compose from THIS, never
                            // from the rest TRS (the drive-3 head/ski bug)
    glm::mat4 world{1.0f};
    bool world_override = false;  // IK wrote `world` directly this frame
};

struct SPrim {
    Mesh mesh{};
    Color col{200, 200, 200, 255};
    int node = -1;
    bool skinned = false;
    // Emissive GAIN, not a flag. The scene target is RGBA16F and post.cpp's
    // halation keys off a luma threshold, so a gain above 1 survives into the
    // bloom instead of clipping -- which is the whole arcade lever for the
    // lamp. 0 = shade normally.
    float emit = 0.0f;
    Color emit_col{255, 255, 255, 255};  // what the emissive burns AT
    // ★ THE HELMET'S SHINE (docs/HELMET_SPEC.md 3). Chad: "get it polished to a
    // shine." 0 = the shading this machine has always had, bit for bit.
    // Gated on the MATERIAL NAME, not on roughness: 24 of the 34 sled materials
    // carry roughness < 0.5, so a roughness gate would re-light the whole hood.
    float gloss = 0.0f;
    // The scarf's own skinned prim (material `sudburian_scarf_blue`): the
    // back-surface scan at bind must see the SUIT, not the scarf -- with the
    // scarf included, the bunched wrap IS the farthest "back surface" (it
    // measured 0.169 vs the suit's 0.075) and the keep-out pushes the chain
    // off its own wrap.
    bool is_scarf = false;
    // ★★★ THE HELMET'S TEAM BAND (material `helmet_orange`, Chad 2026-09-10:
    // "helmet color blue for central city"). Flagged the same way the scarf is
    // -- on the MATERIAL, never the node -- so a zone rename in the asset
    // cannot quietly take the band off the team. The other eight helmet zones
    // (silver, white, black, the two black stripes) are NOT team-coloured and
    // are untouched on both sides.
    bool is_helmet_band = false;
    // Set when SEADS_SCARF_COLOR supplied this prim's colour by hand. The A/B
    // override is the LAST word: the team kit must not paint over the thing
    // Chad is looking at while he compares two scarves.
    bool col_locked = false;
    // ★ MULTIDENT (Chad 2026-08-20: crash-damage helmets, U cycles): -1 =
    // always drawn; 0 = the pristine helmet (drawn only at dent state 0);
    // 1..4 = the baked dent variants (drawn only at their state).
    int dent = -1;
    // ★★ R3-HANDS. A per-PRIMITIVE animation, composed inside the node's own
    // transform at draw. It exists because each control is ONE node holding a
    // static housing AND the part that actually moves, so a node-level
    // rotation would swing the perch and the throttle body along with the
    // lever. Identity on every other prim in the file.
    //
    // WHICH prim moves is read off the geometry, never an index: on the brake
    // it is the `indy_red` blade (Chad's own words -- "the fingers in front of
    // the RED brake lever"); on the throttle red is the KILL CAP ("throttle is
    // not red"), so its lever is peeled out of the rubber housing as a
    // separate connected SHELL instead.
    bool is_lever = false;
    glm::mat4 anim{1.0f};
    glm::vec3 pivot{0.0f};  // node-space point the lever turns about
    glm::vec3 axis{0.0f};   // node-space turn axis
    // CPU-side rest data for skinned prims (model space, pre-skin)
    std::vector<float> base_pos, base_nrm;
    std::vector<unsigned short> jidx;  // 4 per vert (skin joint indices)
    std::vector<float> jw;             // 4 per vert
    // ★ Sudburian head: the mullet's engine-side wind (Chad ruled NO new
    // bones, 2026-09-04 — the Blender 7-bone rig is the motion SPEC only).
    // Per-vert (fade, column-amplitude): fade is 0 where the curtain is
    // pinned under the helmet and 1 at the free bottom; the middle column
    // rides ~1.6x for the helmet's rear opening. Empty on every other prim.
    bool is_mullet = false;
    std::vector<glm::vec2> wind;
};

// ★★★ R3-WS. ONE TICK of the fore-aft weight-shift ladder, resolved before the
// pose pass runs. Every member is a pure function of (SledRig, the solved root
// shift, the rest captures) -- no statics, no accumulator (SPEC 2).
//
// ★ `active` IS THE 0-OFF LAW MADE STRUCTURAL. It is false for every
// `rider_fwd_m >= 0`, and every use site below is inside `if (ws.active)`, so
// the neutral / forward-lean / pure-stand poses cannot be reached by any code
// this rung added. Bit-identity is a property of the control flow, not of a
// constant that happens to be zero.
struct WsState {
    bool active = false;
    float u = 0.0f;          // the ladder coordinate
    float s = 0.0f;          // stand fraction
    float aft_m = 0.0f;      // the pelvis's model -Z retreat, reach-capped
    float aft_cap_m = 0.0f;  // the largest retreat the ARMS allow (SPEC 4)
    float dy_m = 0.0f;       // the pelvis's ladder-driven vertical delta
    float theta_r = 0.0f;    // the reach hinge, forward positive
    float rho = 0.0f;        // the arm-ratio target this cell aimed at
    float ratio = 0.0f;      // ... and the worst ratio it achieved
    float ratio_head = 0.0f; // the SAME cell with the ladder off (no-regression)
    bool shortened = false;  // SPEC 4 fired: the travel was cut for the arms
    float w_kneel = 0.0f, w_stand = 0.0f, w_deck = 0.0f;
    // ★★★ R3-WS(c). The foot's own schedule (0 = the rest weld, 1 = the
    // measured heel limit at the back of the runner) and the neck counter the
    // trunk cap leaves the head. Both are 0 for every rider_fwd_m >= 0.
    float foot_slide = 0.0f;
    float neck_rad = 0.0f;
    // ★ R3-WS(b). The trunk-flexion gate is NO LONGER the foot blend. The feet
    // still unweld over kFootBlendHiU (that is the defect this rung exists to
    // kill and it wants to happen early); the FLEXION is re-timed with the
    // stand so the baked C_s(u) cannot dip. render/rider_pose.h carries the
    // measurement.
    float w_reach = 0.0f;
    bool kneel_ok = false;
    // ★ THE KNEEL IS CARRIED HIP-RELATIVE, NOT IN WORLD. SPEC 3c.4 gates
    // |solved knee - K| <= 1 mm, and that holds only if (hip, K, F) is an
    // EXACT triangle at pose time. The ladder resolves against a closed-form
    // stand reference while the R1c Newton then nudges the root a few mm, so a
    // world-space K is a few mm stale -- measured as a 321 mm knee error,
    // because a 2-bone IK amplifies an inconsistent triangle. Storing the
    // OFFSETS makes the triangle exact by construction at whatever the live
    // hip turns out to be; how far that leaves the knee off the seat is then a
    // reported number (SEADS_SLED_WS_DEBUG) instead of a broken pose.
    glm::vec3 knee_rel[2]{}, foot_rel[2]{};  // from the LIVE hip, per side
    glm::vec3 knee[2]{}, foot[2]{};          // the constructed world targets
};
// ===================== THE SCARF (docs/SCARF_SPEC.md) =======================
// Ladder §5: "six-segment bone chain scarf_01..06, skinned, driven entirely
// render-side, carrying no authored keyframes ... it is the only element on
// screen that SHOWS velocity". §7.6: the same solver becomes SUPERMAN in R4.
// The solver is render/trail_chain.* (pure). Everything here is the wiring.
//
// The ribbon: 7 stations x 2 verts x 2 sides = 28 verts, 24 triangles, both
// windings so it reads from either face without touching global cull state.
constexpr int kScarfStations = kScarfSegments + 1;
// ★ R2c-7s(d), CHAD'S THIRD DRIVE (2026-08-20): the procedural ribbons are
// RETIRED. The scarf is now AUTHORED GEOMETRY inside the GLB itself
// (assets/character/sudburian_src/scarf_geom.py, applied to the live Blender
// session and patched into indy650.glb by patch_scarf.py): a two-loop neck
// wrap bunched at the back, a knot at the FRONT, a short tail over the LEFT
// shoulder (rigid to neck_01) and a long fluffy tail whose six pods are
// rigid-skinned to scarf_01..06 -- so this file's whole remaining job is to
// DRIVE THOSE SIX BONES with the trailing chain. The "3 tails" read of the
// second drive was the "+" cross strips; they are gone with the ribbons.
//
// The authored pods are SEG_RUN = 0.048 m long; the bones in the file stay at
// their authored 0.080 m spacing, so the run-time chain steps
// kScarfRunSegFrac of the measured spacing and the pods meet exactly.
// SAME constant as scarf_geom.py SEG_FRAC -- one ruling, two readers.
// 6 x 0.048 = 0.288 m of hang from the collar = halfway down the 0.53 m back.
// kScarfRunSegFrac: now render/scarf_drape.h (the flak gunner reads it too).
// ★ FOURTH DRIVE "the scarf rests INSIDE their back": the chain is the tube's
// CENTERLINE, and the old keep-out held it at surface + 0.012 while the
// authored tube is 0.027 m half-thick -- the inner half of the tube lay
// inside the suit. The keep-out (and the anchor stand-off with it: the
// effective-plane rule clamps the plane at the anchor's own depth, so a
// deeper anchor would neuter a deeper plane) now stands the centerline off
// by margin + tube half-thickness. The bridge geometry overshoots the anchor
// to cover the root gap this opens.
// kScarfTubeHalfM: now render/scarf_drape.h.
// The SHORT tail's two bones (scarf_s01/s02, R2c-7s(e)): a 2-link chain,
// heavily damped -- Chad: "the shorter one can move a little bit but more
// simply". Missing bones = the short tail rides rigid (older GLB), warned.
// kScarfShortSegs / kScarfShortDamping: now render/scarf_drape.h.
// ★ THE BACK STAND-OFF IS MEASURED AT BIND, NOT TYPED (red-team P1-1). The
// knot bone `scarf_01` is authored 7 mm INSIDE the suit's back surface
// (MEASURED: the skinned rest proxy's back, within the torso band, lies
// 0.0747 m behind the §3b torso plane; the knot sits 0.0677 behind it), so a
// typed 0.06 keep-out let the ribbon z-fight along the back and emerge from
// the shoulder blades. At bind the rest-skinned proxy is scanned and the
// keep-out becomes (surface + this margin); the solver's anchor is stood off
// the bone by the difference so the root sits ON the back, and the six bones
// are written at the solver's positions (an authored cloth follows the
// surface-anchored scarf, which is what a cloth would do).
// kScarfBackMarginM: now render/scarf_drape.h.
constexpr float kScarfTorsoHalfWidthM = 0.15f;  // the band scanned: |lateral| below this
// ★ The lateral strip the plane's SURFACE offset is taken over (2026-08-24).
// Narrow on purpose: the scarf lies along the spine, and the rearmost point of
// a wide band is a shoulder or an elbow, not the surface under the ribbon.
// The scarf's own tail is TAIL_W 0.066 wide, so half of it plus a little.
// kScarfBackStripHalfM: now render/scarf_drape.h.
// A stall must never buy a 600-step frame. The cap DROPS time; it never
// stretches dt (SCARF_SPEC §2) -- a stretched dt would change the settle.
constexpr int kScarfMaxSubsteps = 16;
// The helmet keep-out sphere sits this far up the head bone's own +Y from the
// head joint -- the joint is at the base of the skull, the helmet is not.
// kScarfHeadCentreUpM: now render/scarf_drape.h.
// ★ EIGHTH DRIVE FLUTTER (Chad, 2026-08-20 late): "at the top speed ... it
// should not go flat like a board but it should still flap just with a higher
// frequency / less amplitude but the flapping should also vary some."  This
// REVERSES the first-build ruling (d) that dropped forced flutter -- his call.
// Built as a WIND-DIRECTION wobble on the SOLVER'S INPUT (the pure chain and
// its 12 gated cases are untouched; the resonance-cliff lesson: drive the
// input, never inject energy into the constraint loop):
//   f(v) = St * v / hang   (flag flapping, Strouhal-like: RISES with speed;
//                           8 m/s -> ~3.9 Hz, 20 m/s -> ~9.8 Hz, capped)
//   A(v) = A0 * vref / (v + vref)  (FALLS with speed, never 0:
//                           8 m/s -> ~7.6 deg, 20 m/s -> ~4.7 deg)
// Two incommensurate sines (second at the golden ratio of the first) plus a
// slow amplitude envelope = "vary some": the pattern never repeats exactly.
// Phase accumulates from TICK time only (render reads no clock; determinism
// law) -- the same tick sequence replays the same flutter, bit-exact.
// SEADS_SCARF_FLUTTER=0 kills it; "amp,freq" scales both dials live.
constexpr float kScarfFlutterStrouhal = 0.20f;   // flag-flap St over the hang
constexpr float kScarfFlutterHangM = 0.408f;     // 6 x 0.068 (the ruled hang)
constexpr float kScarfFlutterAmpRad = 0.22f;     // amplitude scale (A0)
constexpr float kScarfFlutterVRefMps = 12.0f;    // A halves by ~vref
constexpr float kScarfFlutterVMinMps = 1.5f;     // idle: bumps give the life
constexpr float kScarfFlutterFMaxHz = 12.0f;     // cap (60 Hz frames sample it)
constexpr float kScarfFlutterEnvFrac = 0.31f;    // envelope freq = 0.31 f
constexpr float kScarfFlutterGold = 0.618034f;   // the second sine's ratio
// The chain solver is a hard low-pass (settle ~2.6 s): anchor-level forcing
// at 4..10 Hz is crushed to fractions of a degree (MEASURED: +/-0.35 deg at
// 8 m/s from a 7.6 deg input wobble). So the VISIBLE flap is a traveling
// wave applied to a COPY of the solved chain right before the bone frames
// are built -- zero feedback into the solver (no energy, no resonance
// cliff), displacement grows toward the free end like a flag:
//   d(k) = A_tip(v) * env * s^1.5 * [sin(ph1 - kWave*s) e_flap
//                                    + 0.55 sin(ph2 - kWave*s + 1.7) e_lat]
constexpr float kScarfFlutterTipM = 0.14f;       // tip amplitude scale
// (0.045 measured INVISIBLE: 27 changed px between 20 m/s frames 0.2 s
//  apart. 0.14 -> tip ~84 mm at 8 m/s, ~52 mm at 20: a real flap.)
constexpr float kScarfFlutterWaveRad = 5.0f;     // ~0.8 wavelengths per tail

struct SledModel {
    bool tried = false, ok = false;
    std::vector<SNode> nodes;
    std::vector<int> order;  // parents-before-children traversal
    std::vector<SPrim> prims;
    std::vector<int> skin_joints;         // node index per skin joint
    std::vector<glm::mat4> skin_ibm;      // inverse bind, per joint
    std::vector<glm::mat4> rest_world;    // zero-channel pose, captured once
    // channel nodes (resolved by name at load; -1 = absent)
    int n_steer = -1, n_susp[3] = {-1, -1, -1}, n_ski[2] = {-1, -1};
    int n_root = -1, n_pelvis = -1, n_head = -1, n_neck = -1;
    // ★ G2g "stand straight when he walks": the authored rest is the SEATED
    // R2b pose, spine curled over bars that are not there on foot. The
    // hunch is MEASURED at load (rest pelvis->neck line vs model up) and
    // its removal pre-baked as one per-spine-joint quat in each joint's
    // parent rest frame; pose_pass slerps it in only while he walks.
    //
    // ★★★ G2h (Chad, after flying G2g: "torso needs to straighten back a
    // little and head is now too tilted back it needs to be straight up and
    // down while walking"). TWO measured findings, both fixed here:
    //
    //  (a) THE BACK. G2g measured ONE angle -- the pelvis->neck CHORD versus
    //      model up -- and put a third of it into each spine joint. A chord
    //      says nothing about CURVATURE: three segments can average vertical
    //      while every one of them is bent, and the authored seated spine is
    //      exactly a C-curve. G2h measures EACH SEGMENT instead (the joint's
    //      own rest +Y column, i.e. the direction to its child) and stands
    //      that segment upright, accumulating down the chain so each joint
    //      corrects what the ones below it left. Same channel, same owner,
    //      same gate -- a strictly better measurement of the same quantity.
    //      (When every segment is vertical the chord is too, so this is a
    //      superset of G2g's law, never a contradiction of it.)
    //
    //  (b) THE HEAD, AND WHY IT WENT BACKWARDS. Straightening the spine
    //      rotates EVERYTHING above spine_03 -- the neck and the head ride it
    //      rigidly -- so the seated head, which was authored looking level
    //      over hunched shoulders, got pitched BACK by the whole removal
    //      angle. That is not a bug in the removal, it is a missing term: the
    //      head needs its own. `gait_head_q[0]` stands the neck_01->head
    //      segment upright (in spine_03's rest frame) and `gait_head_q[1]`
    //      stands the HEAD's own axis upright (in neck_01's rest frame), both
    //      measured against the ALREADY-ACCUMULATED spine correction so the
    //      composition lands plumb rather than merely level-ish.
    //
    // ⚠ ONE OWNER PER JOINT PER TERM, AND G3 WILL COMPOSE WITH THIS. G3's
    // counter-yaw wants spine_01..03 too; it must MULTIPLY onto this channel
    // at the same site (this is the posture term, that is the swing term),
    // never replace it, and never introduce a second straightening.
    // ⚠ The head pair is derived from the FULL-strength spine correction, so
    // `SEADS_GAIT_STRAIGHT=0` (an A/B of the back alone) leaves the head
    // over-corrected by exactly the spine angle. That is an A/B artefact of
    // deliberately breaking the chain, not a defect of the shipped default.
    glm::quat gait_straight_q[3]{glm::quat(1, 0, 0, 0), glm::quat(1, 0, 0, 0),
                                 glm::quat(1, 0, 0, 0)};
    // [0] = neck_01 (in spine_03's rest frame), [1] = head (in neck_01's)
    glm::quat gait_head_q[2]{glm::quat(1, 0, 0, 0), glm::quat(1, 0, 0, 0)};
    int n_grip[2] = {-1, -1}, n_board[2] = {-1, -1};
    // IK chains: [side][joint] -- side 0 = model -X ("_L" nodes), 1 = +X
    int arm[2][3] = {{-1, -1, -1}, {-1, -1, -1}};  // upperarm/forearm/hand
    int leg[2][3] = {{-1, -1, -1}, {-1, -1, -1}};  // thigh/shin/foot
    // rest-pose IK captures: bend-plane normal + twist offsets + hand welds
    glm::vec3 arm_bend_n[2]{}, leg_bend_n[2]{};
    glm::mat3 arm_tw[2][2]{}, leg_tw[2][2]{};  // [side][upper=0/lower=1]
    glm::mat4 hand_off[2]{};                   // grip_socket -> hand rest
    // ★ R2c-5, THE CONTROL-INPUT POSE. Which side is the THROTTLE hand, and the
    // two per-side signs that mean "elbow DOWN" and "wrist UP" on THIS asset.
    // All three are DERIVED FROM THE SHIPPED GEOMETRY at load (see
    // capture_rest_ik) rather than retyped: the sides do not mirror, and the
    // Blender session paid three times for assuming they did.
    int arm_throttle_side = -1;   // the model -X side = the rider's RIGHT hand
    float arm_swing_down[2]{};    // sign of the pole rotation that drops the elbow
    float arm_wrist_up[2]{};      // sign of the bar roll that raises the wrist
    // ★★ R3-HANDS (Chad, 2026-08-24). The two CONTROL bones and the levers
    // they reach for. See render/rider_pose.h for his words, and
    // capture_rest_ik for why the brake elbow's old 2.1 mm rise needed no new
    // dial once the bar roll stopped eating it.
    int n_thumb_r = -1;       // thumb_01_r, the throttle thumb
    int n_mittfront_l = -1;   // mittfront_01_l, the brake fingers
    int n_throttle_block = -1;
    int n_brake_lever = -1;
    CurlAim thumb_aim{};      // measured at load against throttle_block
    CurlAim mitt_aim{};       // measured at load against brake_lever
    // ★ DEFECT 7 (SUDBURIAN_LADDER §1.1). The leg's counterpart of hand_off,
    // and it did not exist before R1a: the boot target was `rest_world[foot]`,
    // a CONSTANT, so the boots tracked nothing while the hip leaned away and
    // the leg IK saturated at dmax 0.8181 m. Now board_socket -> foot rest,
    // carrying the defect-6 re-seat (render::kBootReseatM).
    glm::mat4 foot_off[2]{};                   // board_socket -> foot rest
    float arm_len[2][2]{}, leg_len[2][2]{};    // [side][seg]
    // ★ §D. The declared-joint -> node index map (so the pure rider_cg can be
    // fed by RiderJoint index) and the reduced hinge model captured at load.
    int rj[kRiderJointCount]{};
    RiderHingeModel hinge{};
    // ★★★ ST-5 THE UN-HUNCH (Chad, 2026-09-05: "the sudburian seems hunched
    // over unnecessarily"). Three numbers, all MEASURED off the rest skeleton
    // at load and never typed:
    //   sit_spine[k]     the spine_01/02/03 node indices, taken from the
    //                    DECLARED catalogue (`rj`), never a bare find_node;
    //   sit_parent_q[k]  each one's PARENT's rest world rotation, normalised
    //                    -- the frame a model-+X rotation has to be
    //                    conjugated into to still be a model-+X rotation
    //                    (the same law q_hinge obeys through `root_q`);
    //   sit_hunch_rad    the rest trunk tilt itself: the forward lean of the
    //                    pelvis->neck axis in the model's YZ plane. This is
    //                    the hunch the asset ships with, so the un-hunch is a
    //                    FRACTION OF A MEASUREMENT and a re-export that
    //                    changes the rider's posture moves it by itself.
    // All three are inert while every hand-grip weight is 0.
    // ★★★ ST-5 THE SWEEP extends the captured chain from three rungs to
    // FIVE -- spine_01/02/03, neck_01, head -- because the twist goes all the
    // way up and the head LEADS it. The first three entries are exactly the
    // three the un-hunch has always used (same nodes, same parent
    // rotations); the two new ones are additive and a missing neck/head
    // leaves them -1, which makes the twist stop at the chest instead of
    // making the whole rig draw wrong.
    int sit_spine[render::sting::kTwistCount] = {-1, -1, -1, -1, -1};
    glm::quat sit_parent_q[render::sting::kTwistCount]{};
    float sit_hunch_rad = 0.0f;
    // ★ THE SCARF (docs/SCARF_SPEC.md §4). Six existing rig bones driven by the
    // pure trailing-chain solver, plus a procedural ribbon drawn off the same
    // frames so the thing is VISIBLE before an authored cloth exists. The bone
    // driving is the point: the day a skinned scarf is spliced in, it rides
    // these world matrices with NO code change (spec §0, THE ONE DEPARTURE).
    int scarf_node[kScarfSegments] = {-1, -1, -1, -1, -1, -1};
    // (n_neck lives with the other channel nodes above -- the scarf lane and the
    // R3-WS lane each added it, at different lines, so git merged BOTH with no
    // conflict. One member, one declaration.)
    glm::mat4 scarf_anchor_local{1.0f};  // scarf_01's rest TRS in the neck frame
    float scarf_seg_len = 0.080f;        // MEASURED mean |scarf_0k.t| for k=2..6
    float scarf_back_surface = 0.0f;     // MEASURED at bind: suit back behind the torso plane
    float scarf_anchor_standoff = 0.0f;  // bone -> solver anchor, along back_normal
    TrailChainState scarf{};
    // ★ R4a THE NON-INERTIAL FRAME (Chad ruled 2026-08-27: the scarf gets it).
    // One step of velocity/omega history, and the epoch it was taken under.
    TrailFrameTracker scarf_frame{};
    long scarf_epoch = -1;
    TrailChainParams scarf_par{};
    int scarf_s_node[kScarfShortSegs] = {-1, -1};
    glm::mat4 scarf_s_anchor_local{1.0f};  // scarf_s01's rest TRS in the neck
    float scarf_s_seg_len = 0.080f;
    TrailChainState scarf_s{};
    TrailChainParams scarf_s_par{};
    bool scarf_s_ok = false;
    // ★ SCARF-DRAPE: the short tail's pod boxes, measured at bind alongside
    // the long chain's but PARKED here -- scarf_s_par is built later by
    // copying scarf_par, and the long chain's probe table must not survive
    // that copy.
    TrailChainProbe scarf_s_probe[kScarfShortSegs]{};
    bool scarf_s_have_probe[kScarfShortSegs] = {};
    // ===== ★★★ R4a THE ARMING RUNG =====================================
    // The rod model's REST bake (render/rider_load.h) and the two references
    // the stage weight is normalised by -- both MEASURED off this asset, never
    // typed. See capture_rider_load().
    RiderLoadModel rl{};
    double rl_seat_ref = 0.0;
    double rl_board_ref = 0.0;
    bool rl_ok = false;
    bool rl_tried = false;
    // The body chain's OWN frame-field history. It is a separate tracker from
    // the scarf's on purpose: the two re-prime on different events (the body
    // re-primes every time it ARMS, the scarf only on an epoch bump), and one
    // shared tracker would hand whichever re-primed a stale sample.
    TrailFrameTracker body_frame{};
    long body_epoch = -1;
    TrailChainState body{};
    TrailChainParams body_par{};
    bool body_ok = false;
    // [0,1]. 0 = stage 0, seated; 1 = stage 3, nothing below carries.
    float stage_arm = 0.0f;
    // The last solved loads, for the debug print and for nothing else.
    RiderLoad rl_last{};
    // The SAME solve with the acceleration zeroed -- the live reference the
    // stage weight is normalised by. See the banner at its call site.
    RiderLoad rl_ref{};
    float body_dbg_accel = 0.0f;  // |a_body|, for the SEADS_BODY_CHAIN=2 line
    // ★ THE ARMING MEMORY'S STATE. It lives here because it must persist
    // across frames; the POLICY that advances it is render/body_drive.*, where
    // a test can reach it -- the rung-2b lesson, that a stage machine living
    // in this TU is a stage machine no gate can execute. 0 = fully recovered,
    // which is also the shipped default.
    BodyBuckMemory body_buck;
    // ★★★ R4c -- THE DEPARTED MAN, AND RENDER NO LONGER OWNS HIM. He is
    // `sim::WalkerState`, stepped by the kernel and handed here on the rig;
    // what lives in this struct is only what DRAWING him costs.
    // `flight_off_model` is the model-frame vector from where he WOULD be if he
    // were still holding on to where he now is -- exactly zero on the frame he
    // lets go, so nothing jumps at the transition.
    glm::vec3 flight_off_model{0.0f};
    // ★★★ R4c: HIS OWN FRAME, IN MODEL SPACE. Stage 2 left him welded to the
    // machine's ATTITUDE while he flew -- a translation and nothing more -- and
    // wrote that down as a debt. A walk cannot carry that debt: a man whose feet
    // step along the machine's forward while he travels along his own is broken
    // on sight. So the departed man gets an orientation: HIS up (the sphere's,
    // under HIS feet, not the machine's) and HIS forward (the heading the kernel
    // walks him along).
    glm::vec3 flight_up_model{0.0f, 1.0f, 0.0f};
    glm::vec3 flight_fwd_model{0.0f, 0.0f, 1.0f};
    // What the drawn gait spends, straight off the kernel's own walker: the
    // phase it advanced BY DISTANCE, the stride the snow allowed, and how much
    // snow the swinging foot has to clear.
    float gait_phase = 0.0f;
    float gait_stride_m = 0.0f;
    float gait_lift_m = 0.0f;
    float gait_stance_frac = 0.5f;
    float gait_depth_frac = 0.0f;   // [0,1], how deep the snow he is in is
    float gait_arm_swing = 0.35f;   // radians of fore/aft hand travel
    bool gait_on = false;  // he is on his feet (or his hands and knees)
    // ★★★ GAIT LADDER G1: his two feet's WORLD-space `sim::GaitState` targets,
    // converted into MODEL space the SAME way `flight_off_model` is (§4's own
    // instruction) -- once here, in `sled_model_draw`, where `pos`/`basis` are
    // in scope, so `pose_pass`'s legs section only ever CONSUMES a model-space
    // point and runs no gait math of its own (L-PURE). Index convention
    // matches `sim::GaitFoot`: 0 = left, 1 = right -- the crossover onto
    // `sm.leg[s]`'s `model_side` happens where it is consumed, not here.
    //
    // ⚠ THE NPC SEAM, NAMED (red-team, G1b): `sim::GaitState` itself is
    // instance-clean (L-NPC) -- N walkers, N copies, no shared state. This
    // CACHE is not: `SledModel` lives in the file-static singleton `g_sled`
    // (see `sled_model_draw` below), so a second drawn walker would need its
    // own `SledModel`/cache, not a second write into these two floats. Not
    // this rung's job (G1 is the hero rider only) -- flagged here for
    // whichever rung actually draws the millwright-tier crowd (G8's own
    // plan entry names the seam as `combat/reinforce.h:28`).
    glm::vec3 gait_foot_model[2]{glm::vec3(0.0f), glm::vec3(0.0f)};
    float gait_foot_pitch_rad[2] = {0.0f, 0.0f};
    bool gait_foot_ok[2] = {false, false};
    // ★★★ GAIT LADDER G2: THE PELVIS LIVES. Straight off `sim::GaitState`
    // (`rig.gait`), cached here for the identical reason the two floats
    // above are -- `pose_pass`'s pelvis channel only ever CONSUMES these
    // (L-PURE), never re-derives them. `gait_pelvis_drop_m` is already in
    // METRES of world/model scale (no basis rotation needed, unlike the foot
    // targets: a drop along the model's own `up` axis at the pelvis IS the
    // law, in either frame). `gait_sway_left_m` and `gait_roll_rad` are
    // published along `left = cross(up, fwd)` in SIM's world space; the
    // consumer builds its OWN `left` the same way off `flight_up_model` /
    // `flight_fwd_model` (a proper rotation carries a cross product's sense
    // with it -- see sim/gait.h's sign-convention banner), so no crossover
    // wire needs remembering here the way the per-foot index does.
    float gait_pelvis_drop_m = 0.0f;
    float gait_bob_m = 0.0f;
    float gait_sway_left_m = 0.0f;
    float gait_roll_rad = 0.0f;
    // ★★★ PRONE. Chad drove stage 2 and said he "fall[s] off backward, staying
    // in a sitting position" -- and lying in the snow in a sitting position is
    // the same defect one stage later. His ruling names the pose: "get up from
    // being prone or supine to crawl". This is the angle between standing and
    // face-down, in radians about his own right axis; pi/2 is not a chosen
    // number, it is what LYING DOWN means -- his body axis horizontal instead
    // of vertical.
    float flight_pitch_rad = 0.0f;
    // ★ He is under the snow: skip the rider prims entirely. `skinned` IS the
    // rider -- every machine prim is unskinned -- so this takes the man, his
    // suit and his scarf and leaves the machine alone.
    bool hide_rider = false;
    // ★★★ HIS ORIENTATION, BUILT ONCE AND SPENT TWICE. The root spends it to
    // stand him up in his own frame; the leg IK spends it on the BEND NORMAL,
    // and that second consumer is not optional -- the pole vector that decides
    // which way a knee folds is a MODEL-space direction, so a man who has
    // turned 90 degrees would bend his knees SIDEWAYS with an unrotated one.
    // Two call sites, one quaternion: a second construction is how they start
    // disagreeing about which way he is facing.
    glm::quat flight_q{1.0f, 0.0f, 0.0f, 0.0f};
    double stage_arm_lp = 0.0;
    float stage_arm_raw = 0.0f;
    bool stage_arm_seeded = false;
    // R4a rung 2 -- LADDER 7.3 stage 2's own signal, filtered with the same
    // discipline stage 3's is: seeded on the first frame (never ramped up from
    // a zero it never had) and RESET ON EPOCH, so a respawn cannot carry the
    // dead life's boot release into the fresh one.
    double board_rel_lp = 0.0;
    float board_release = 0.0f;
    float board_release_raw = 0.0f;
    bool board_rel_seeded = false;
    unsigned board_rel_epoch = 0u;
    // The deepest face-allowance the seated pose needed. Instrument, not a
    // dial: if this is ever ~0 on a seated man the ruling is not in the build.
    float body_dbg_allow_m = 0.0f;
    float body_dbg_fd = 0.0f, body_dbg_v = 0.0f, body_dbg_w = 0.0f;
    int body_dbg_ticks = 0;
    // ★ SIM time, not wall time: total ticks x dt. The =2 log had no
    // clock at all, so two episodes 40 s apart were indistinguishable
    // from two frames apart when reading the file back.
    long long body_dbg_tick_total = 0;
    // The pin bake (capture_body_pin): each station as (segment, t) on the
    // rider's own joint polyline, plus the rest-pose residual so a station
    // that does not actually lie on the polyline says so out loud.
    int body_pin_seg[kBodyChainStations]{};
    float body_pin_t[kBodyChainStations]{};
    // The station's REST offset from its polyline point, in the segment's own
    // frame. Projection alone left one station 80 mm off the polyline (the
    // shoulder -- the chain cuts a corner the joints do not), and 80 mm of
    // "residual" is 80 mm of POP the first constraint pass takes out the
    // instant the chain arms. Carrying the offset makes the pin EXACT by
    // construction instead of approximate, so there is nothing to pop.
    glm::vec3 body_pin_off[kBodyChainStations]{};
    float body_pin_res[kBodyChainStations]{};
    bool body_pin_ok = false;
    // flutter phase accumulators (TICK time; wrapped each step -- see the
    // kScarfFlutter* banner). Main sine / golden-ratio sine / slow envelope.
    float scarf_flut_ph1 = 0.0f;
    float scarf_flut_ph2 = 0.0f;
    float scarf_flut_phe = 0.0f;
    // ★ R2c-7s(g), consult P0-1: the DRAWN BACK as the resting surface. At
    // bind, the suit's back-quad vertices are found on the rest-skinned mesh
    // (faces, not a lateral filter that misses them -- the coarse suit has
    // ZERO central back verts, the old scan measured one shoulder vert) and
    // clustered into rings along the torso; per frame those few verts are
    // re-skinned (rigid, one joint each) and become the banded keep-out
    // planes the chain rests on.
    struct BackVert {
        int prim = -1;
        int vert = -1;
        int ring = -1;
    };
    std::vector<BackVert> back_verts;
    int back_n_rings = 0;
    // ★ SCARF-DRAPE slab (2026-09-04): TORSO-side verts (max-weight joint on
    // the torso chain, sides included), ring-tagged with the SAME t banding
    // as back_verts -- per frame they give each band's coat half-width, the
    // number that bounds the keep-out laterally (bp_lat_half's banner in
    // trail_chain.h).
    std::vector<BackVert> side_verts;
    int torso_nodes[5] = {-1, -1, -1, -1, -1};
    // ★★★ THE NUMBER THAT CAN FAIL ON A SINK (2026-08-27, Chad: "it sinks in
    // at the coat just below the knots a bit").
    //
    // `back_clr` graded the chain against the single chord plane and stayed
    // positive through five rounds of "its still sinking"; `surf_clr` replaced
    // it with the chain against the BANDED planes -- and `surf_clr` CANNOT
    // FAIL ON THIS DEFECT EITHER, because it still grades the CHAIN. The
    // constraint pass guarantees the chain sits at or beyond
    // back_keepout_min_m, so surf_clr reads a healthy +0.015 while the DRAWN
    // FABRIC is visibly inside the coat: the chain is the ribbon's CENTRELINE
    // and the complaint is about its SURFACE.
    //
    // The third time is the vertices themselves. These are the scarf's own
    // drawn verts, captured at bind and restricted to the SOLVER-DRIVEN bones
    // (scarf_01..06 and the short tail) -- the wrap and knot are rigid to
    // neck_01, authored geometry that no keep-out constant can move, and
    // mixing them in would drown the signal in a permanent offset.
    std::vector<BackVert> scarf_surf_verts;  // .ring unused: the band is LIVE
    // Worst surf_vtx seen this session, so a DRIVE prints a handful of lines
    // instead of five a frame. A log nobody can read is not an instrument.
    float scarf_surf_worst = 1.0e9f;
    bool scarf_ok = false;
    bool scarf_primed = false;
    // ★ THE SUDBURIAN (2026-08-18). The legacy rider's `root` hung off an
    // UNROTATED armature node and had an identity rest rotation, so "model
    // frame" and "root's parent frame" were the same frame and pose_pass could
    // add a model-frame lean straight into root.t and pre-multiply a model-X
    // hinge onto the pelvis. The Sudburian's armature node carries the R2b
    // 180 deg seating yaw and its `root` is Blender's ground bone (-90 deg X
    // rest), so those two frames are NOT the model frame any more. Both are
    // captured here off the rest pose -- DERIVED, not assumed -- and every
    // model-frame demand is expressed through them. On an identity rig this
    // reduces bit-for-bit to the old arithmetic.
    glm::mat3 root_parent_R{1.0f};   // rest world rotation of root's PARENT
    glm::mat3 root_R{1.0f};          // rest world rotation of `root` itself
    glm::quat root_q{1.0f, 0.0f, 0.0f, 0.0f};  // the same, as a quaternion
    // ★★★ R3-WS. Everything the fore-aft weight-shift ladder captures at load.
    // All of it is DERIVED from the rest pose or from the baked seat profile in
    // render/rider_pose.h -- nothing here is typed.
    glm::vec3 ws_pelvis_rest{0.0f};   // rest pelvis world position
    glm::vec3 ws_shoulder_off[2]{};   // rest (upperarm - pelvis), per model side
    glm::vec3 ws_foot_rest[2]{};      // rest ankle world position, per side
    float ws_arm_dmax[2]{};           // the arm IK's own reach limit, per side
    float ws_rho0 = 1.0f;             // the rest arm ratio (worst side)
    float ws_sit_h = 0.0f;            // rest pelvis height above the seat
    // ★ R3-WS(c) / A9. The neck counter's MEASURED sign and the frame its
    // rotation is conjugated into (neck_01's parent's rest world rotation) --
    // the same q_hinge pattern the pelvis hinge already uses.
    float ws_neck_up_sign = 0.0f;
    glm::quat ws_neck_parent_q{1.0f, 0.0f, 0.0f, 0.0f};
    float ws_kneel_h = 0.0f;          // the SOLVED kneeling hip height
    // ★ R3-WS(d) / D4(a). The DECK-side anchor of the blended knee bound: the
    // knee's rest |x|, per side, measured off rest_world like everything else
    // here. The kneel side of the blend is the constructed K's own |x|.
    float ws_deck_knee_x[2]{};
    float ws_curve[kWsBakeS][kWsBakeU]{};   // C_s(u) fore-aft, baked at load
    float ws_curve_y[kWsBakeS][kWsBakeU]{}; // ... and its VERTICAL twin
    bool ws_baked = false;
    bool ws_monotone_ok = false;
    float ws_slope_min = 0.0f;  // R3-WS(b): min dC/du over every baked slice
    // R3-WS(b): the worst joint world delta between adjacent bake cells, the
    // numerator of the continuity-in-`a` gate.
    float ws_cell_dj[kWsBakeS][kWsBakeU]{};
    WsState ws_last{};  // the state the LAST solve actually posed with
    Shader shader{};
    Material mat{};
    int loc_color = -1, loc_sun = -1, loc_emit = -1, loc_emit_col = -1;
    int loc_gloss = -1;
    // the headlight beam: its own hull, shader and uniforms
    Mesh beam_mesh{};
    Shader beam_shader{};
    Material beam_mat{};
    bool beam_ok = false;
    int lb_apex = -1, lb_dir = -1, lb_r0 = -1, lb_spread = -1, lb_range = -1;
    int lb_intensity = -1, lb_tint = -1, lb_cross = -1;
    // NODE-LOCAL centre of the lens glass. The GLB's headlight_lens NODE
    // origin is the sled root (0,0,-0.17) -- the front-bottom of the pan --
    // while the glass itself sits up on the hood. Aim the beam from the BULB,
    // not from the node origin, or the shaft pours out from under the machine.
    glm::vec3 lens_center{0.0f};
    // The steering tie rods are a CLOSED LINKAGE the node tree cannot express:
    // each rod is parented to the bellcrank (inner ball joint), but its outer
    // ball rides the spindle, which steers on its own kingpin at a DIFFERENT
    // ratio (bar sweep 32 deg vs ski 24 deg). Left to the hierarchy alone the
    // outer end sweeps free in the air the moment the bars turn. pose_pass
    // closes the loop every frame: pin the inner ball, aim the rod at
    // tierod_end (which already poses with the ski), stretch to seat the ball.
    struct TieRodLink {
        int rod = -1, end = -1;         // tie_rod_X node / tierod_end_X node
        glm::vec3 li{0.0f}, lo{0.0f};   // rod-local inner / outer ball at rest
    };
    TieRodLink tie_link[2];
};

SledModel g_sled;

// -- shading: one small sun-lit shader; emissive parts (lights, gauges)
// draw at full colour so the machine reads at night in a permanently
// winter world.
const char* kVS = R"GLSL(
#version 330
in vec3 vertexPosition;
in vec3 vertexNormal;
in vec4 vertexColor;
uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;
out vec3 fN;
out vec3 fP;   // eye-relative position (the eye IS the origin here)
out vec4 fC;
void main() {
    fN = normalize(vec3(matNormal * vec4(vertexNormal, 0.0)));
    fP = vec3(matModel * vec4(vertexPosition, 1.0));
    fC = vertexColor;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)GLSL";
const char* kFS = R"GLSL(
#version 330
in vec3 fN;
in vec3 fP;
in vec4 fC;
out vec4 finalColor;
uniform vec3 uColor;
uniform vec3 uSunDir;   // direction of travel of sunlight (sun -> scene)
uniform float uEmit;     // emissive GAIN: 0 = shade normally, >1 = overdrive
uniform vec3 uEmitColor; // what the emissive burns at (may differ from uColor)
uniform float uGloss;    // 0 = matte (and BIT-IDENTICAL to the pre-helmet shader)
void main() {
    // half-Lambert wrap: a hero machine must never fall to silhouette
    // black on its shadow side -- the whole point of this rung is that
    // Chad can SEE it (snow bounce justifies the floor physically).
    // DRIVE-2 ("the back end is all so black I can't distinguish
    // anything"): pure-black albedo destroys ALL shading -- lift the
    // value floor to charcoal, and add a view-keyed rim so edges separate
    // panel from panel even inside the dark mass.
    vec3 N = normalize(fN);
    // ★ Sudburian head (2026-09-04): the face's windburn/lip grade rides in
    // COLOR_0. Prims without a colour VBO get the constant attribute (1,1,1,1)
    // from the driver, and x*1.0 is exact in IEEE — every existing primitive
    // shades bit for bit as before.
    vec3 base = max(uColor * fC.rgb, vec3(0.085));
    float d = dot(N, -uSunDir) * 0.5 + 0.5;
    vec3 V = normalize(-fP);   // eye-relative render: the eye is at 0
    float rim = pow(1.0 - abs(dot(N, V)), 3.0);
    vec3 lit = base * (0.24 + 0.86 * d * d) + vec3(0.14) * rim;
    // ★ THE HELMET HIGHLIGHT (docs/HELMET_SPEC.md 3). Blinn-Phong, eye-relative:
    // fP is already eye-relative so V is the view direction, and uSunDir is the
    // sunlight's direction of TRAVEL, so the light vector is -uSunDir. The
    // exponent rides the gloss so the visor (roughness 0.05 -> gloss 0.9) gets a
    // tight hot spot and the painted shell (0.15 -> 0.7) a broader one.
    // At uGloss == 0 this term is EXACTLY 0.0 and every other primitive shades
    // bit for bit as before (SEADS_HELMET_GLOSS=0 forces it, to measure that).
    float spec = uGloss * 0.55 *
                 pow(max(dot(N, normalize(-uSunDir + V)), 0.0),
                     mix(16.0, 96.0, uGloss));
    lit += vec3(spec);
    // uEmit is a GAIN. clamp() only picks the emissive OVER the lit term; the
    // unclamped multiply is what carries values past 1.0 into the RGBA16F
    // target, where post.cpp's halation turns them into a real bloom instead
    // of a clipped white patch.
    finalColor = vec4(mix(lit, uEmitColor * uEmit, clamp(uEmit, 0.0, 1.0)), 1.0);
}
)GLSL";

// ===================== THE HEADLIGHT BEAM ===================================
// Chad: "the light should shoot a 1 million candle beam straight ahead".
//
// The hull is NOT the thing you shade. It is a conservative bounding volume
// that exists so the rasterizer hands us fragments; the brightness is a volume
// integral evaluated per fragment from the VIEW RAY's perpendicular distance to
// the beam AXIS. That one choice is what removes every classic artefact at
// once: brightness reaches zero exactly AT the silhouette by construction, so
// there is no cone-shaped edge to hide, and it degenerates correctly when you
// look straight down the beam (distance -> 0 -> full brightness) where the
// usual grazing-angle trick goes black exactly when it should be brightest.
//
// The hull is built 1.25x wider than the shader's radius, so rasterization
// edges, MSAA and derivative wobble all land in a region that is already black.
constexpr float kBeamR0 = 0.09f;      // lens radius (m) -- a frustum, not a cone
constexpr float kBeamSpread = 0.123f;  // tan(7 deg) half-angle
constexpr float kBeamRange = 45.0f;    // m
constexpr float kBeamHullPad = 1.25f;
constexpr float kBeamSink = 0.15f;  // start the hull behind the lens plane, so
                                    // the bodywork hides the join
constexpr int kBeamSeg = 24;
// A headlight is aimed DOWN at the road, not level. Level, the shaft rises
// past the horizon and sits exactly where the rider is trying to look --
// Chad, after the first drive: "that light blinded me so bad the whole time".
constexpr float kBeamDownDeg = 5.0f;

const char* kBeamVS = R"GLSL(#version 330
in vec3 vertexPosition;
uniform mat4 mvp;
uniform mat4 matModel;
out vec3 vFragPos;      // eye-relative world position (the eye IS the origin)
void main() {
    vFragPos = (matModel * vec4(vertexPosition, 1.0)).xyz;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)GLSL";

const char* kBeamFS = R"GLSL(#version 330
in vec3 vFragPos;
out vec4 finalColor;
uniform vec3 uApex;       // eye-relative apex, at the lens plane
uniform vec3 uDir;        // unit forward
uniform float uR0;
uniform float uSpread;
uniform float uRange;
uniform float uIntensity; // HDR master (night-gated on the CPU side)
uniform vec3 uTint;
uniform float uCross;     // 0.5 eye outside the hull, 1.0 inside

float hash21(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

void main() {
    vec3 P = vFragPos;
    vec3 V = normalize(P);              // eye at the origin

    float t = max(dot(P - uApex, uDir), 0.0);

    // perpendicular distance from the VIEW RAY to the beam AXIS
    vec3 n = cross(V, uDir);
    float nl = length(n);
    float d = (nl > 1e-5) ? abs(dot(uApex, n)) / nl
                          : length(uApex - dot(uApex, uDir) * uDir);

    float r = uR0 + t * uSpread;        // SHADER radius, not the hull radius
    float u = clamp(d / r, 0.0, 1.0);

    // radial: exactly zero at the rim, with zero slope going into it
    float u2 = u * u;
    float w = 1.0 - u2;
    float radial = w * w * (0.30 + 0.70 * exp(-3.5 * u2));

    // axial: net 1/t, NOT inverse-square. Flux in the cone is constant and the
    // cross-section grows as t^2, but the chord the eye ray travels through the
    // cone grows as t -- screen radiance is the product. Inverse-square kills
    // the beam by 4 m and it reads as a stub.
    float axial = (3.0 / (3.0 + t))                       // 1/t, softened
                * exp(-t / (0.42 * uRange));              // Beer-Lambert tail
    axial *= smoothstep(0.0, 0.55, t);                    // no hot dot at the lens
    axial *= 1.0 - smoothstep(0.78 * uRange, uRange, t);  // black before the cap

    // LOOKING DOWN THE BARREL. The chase camera sits behind the machine on the
    // beam's own axis, so without this the brightest part of the volume covers
    // the middle of the screen and the rider is staring into it. Physically
    // this is the scattering phase: from behind the lamp you see the weak
    // BACK-scatter lobe, not the strong forward one you'd see facing it.
    float align = clamp(dot(V, uDir), 0.0, 1.0);
    float phase = mix(1.0, 0.16, smoothstep(0.45, 0.95, align));

    // ...and never render dense volume right in the eye. Fades the near field
    // so the beam cannot wash the frame when the camera is close behind.
    float near_fade = smoothstep(2.0, 9.0, length(P));

    float I = uIntensity * radial * axial * uCross * phase * near_fade;
    I *= 1.0 + 0.06 * (hash21(gl_FragCoord.xy) - 0.5);    // kills tail banding
    I = clamp(I, 0.0, 6.0);   // above this you only feed fireflies to the bloom

    // whiten the core; let halation supply the colour in the surround
    vec3 c = mix(uTint, vec3(1.0), clamp(I * 0.22, 0.0, 1.0));
    finalColor = vec4(c * I, 1.0);   // BLEND_ADDITIVE is (SRC_ALPHA, ONE)
}
)GLSL";

// The hull: a truncated cone, 24 segments, near ring sunk behind the lens,
// closed by a far cap so a view ray always crosses it exactly twice.
Mesh build_beam_hull() {
    const float z0 = -kBeamSink, z1 = kBeamRange;
    const float r0 = kBeamHullPad * (kBeamR0 + 0.0f * kBeamSpread);
    const float r1 = kBeamHullPad * (kBeamR0 + kBeamRange * kBeamSpread);
    const int vc = kBeamSeg * 2 + kBeamSeg + 1;   // side rings + cap fan
    const int tc = kBeamSeg * 2 + kBeamSeg;
    Mesh m{};
    m.vertexCount = vc;
    m.triangleCount = tc;
    m.vertices = static_cast<float*>(std::calloc(vc * 3, sizeof(float)));
    m.normals = static_cast<float*>(std::calloc(vc * 3, sizeof(float)));
    m.indices =
        static_cast<unsigned short*>(std::calloc(tc * 3, sizeof(unsigned short)));
    auto put = [&](int i, float x, float y, float z) {
        m.vertices[3 * i] = x; m.vertices[3 * i + 1] = y; m.vertices[3 * i + 2] = z;
        m.normals[3 * i + 2] = 1.0f;
    };
    for (int k = 0; k < kBeamSeg; ++k) {
        const float a = 6.2831853f * k / kBeamSeg;
        put(k, r0 * std::cos(a), r0 * std::sin(a), z0);
        put(kBeamSeg + k, r1 * std::cos(a), r1 * std::sin(a), z1);
        put(2 * kBeamSeg + k, r1 * std::cos(a), r1 * std::sin(a), z1);
    }
    put(3 * kBeamSeg, 0.0f, 0.0f, z1);   // cap centre
    int f = 0;
    auto tri = [&](int a, int b, int c) {
        m.indices[f++] = static_cast<unsigned short>(a);
        m.indices[f++] = static_cast<unsigned short>(b);
        m.indices[f++] = static_cast<unsigned short>(c);
    };
    for (int k = 0; k < kBeamSeg; ++k) {
        const int j = (k + 1) % kBeamSeg;
        tri(k, kBeamSeg + k, kBeamSeg + j);
        tri(k, kBeamSeg + j, j);
        tri(2 * kBeamSeg + k, 3 * kBeamSeg, 2 * kBeamSeg + j);
    }
    UploadMesh(&m, false);
    return m;
}


// kSlagOrange as a raylib Color. Single-sourced from render/team_color.h, so
// the gauges, the lava's hot stop and the enemy faction stay the SAME orange --
// Chad: "I want the gauges to be the common to this codebase (thematic) slag
// orange". Never retype the literal here.
Color slag_color() {
    return Color{static_cast<unsigned char>(kSlagOrange.x * 255.0),
                 static_cast<unsigned char>(kSlagOrange.y * 255.0),
                 static_cast<unsigned char>(kSlagOrange.z * 255.0), 255};
}

glm::mat4 compose(const glm::vec3& t, const glm::quat& r, const glm::vec3& s) {
    glm::mat4 m = glm::mat4_cast(r);
    m[0] *= s.x;
    m[1] *= s.y;
    m[2] *= s.z;
    m[3] = glm::vec4(t, 1.0f);
    return m;
}

// Bone basis: +Y from `from` toward `to` (the glTF/Blender bone axis),
// bend-plane normal `n` fixing the roll.
glm::mat3 bone_basis(const glm::vec3& from, const glm::vec3& to,
                     const glm::vec3& n) {
    const glm::vec3 y = glm::normalize(to - from);
    glm::vec3 x = glm::cross(y, n);
    const float xl = glm::length(x);
    x = xl > 1e-5f ? x / xl : glm::vec3(1.0f, 0.0f, 0.0f);
    const glm::vec3 z = glm::cross(x, y);
    return glm::mat3(x, y, z);
}

// ★★★ R3-HANDS: SPLIT THE THROTTLE LEVER OFF ITS HOUSING.
//
// `indy_red` on the throttle is the KILL CAP, not the lever -- Chad, plainly:
// "throttle is not red", and thumb_01_r sits 121 mm away from that tab. The
// real thumb lever was authored inside the SAME `indy_rubber` primitive as the
// box it hinges on, so neither material nor node can separate them.
//
// They are separate SHELLS though, and that is measurable. Welding by position
// and unioning across triangles finds FOUR components in that primitive, and
// the lever is unambiguous on four independent counts: it is the only thin
// plate (98.4 x 54.8 x 10.1 mm against the box's 36 x 59 x 54), it carries 264
// of the 350 verts, it reaches furthest OUTBOARD toward the grip, and it is
// 70 mm from the thumb where every other shell is 122-130 mm. The rule below
// is the outboard reach, because that one is a pure geometric fact about which
// part the hand is meant to touch.
//
// Moves the lever's triangles into `out`, leaves the rest in `m`, both
// re-indexed compactly. Returns false and touches nothing when the primitive
// holds a single shell -- so it is inert on every other mesh in the file.
bool split_outboard_shell(Mesh& m, Mesh& out) {
    if (m.vertices == nullptr || m.indices == nullptr || m.vertexCount <= 0)
        return false;
    const int vc = m.vertexCount;
    const int ic = m.triangleCount * 3;
    std::map<std::array<int, 3>, int> key;
    std::vector<int> rep(static_cast<std::size_t>(vc), 0);
    for (int v = 0; v < vc; ++v) {
        const std::array<int, 3> k{
            static_cast<int>(std::lround(m.vertices[3 * v + 0] * 1e6f)),
            static_cast<int>(std::lround(m.vertices[3 * v + 1] * 1e6f)),
            static_cast<int>(std::lround(m.vertices[3 * v + 2] * 1e6f))};
        auto it = key.find(k);
        if (it == key.end())
            it = key.emplace(k, static_cast<int>(key.size())).first;
        rep[static_cast<std::size_t>(v)] = it->second;
    }
    const int nu = static_cast<int>(key.size());
    std::vector<int> uf(static_cast<std::size_t>(nu));
    for (int i = 0; i < nu; ++i) uf[static_cast<std::size_t>(i)] = i;
    std::function<int(int)> find = [&](int a) {
        while (uf[static_cast<std::size_t>(a)] != a) {
            uf[static_cast<std::size_t>(a)] =
                uf[static_cast<std::size_t>(uf[static_cast<std::size_t>(a)])];
            a = uf[static_cast<std::size_t>(a)];
        }
        return a;
    };
    for (int t = 0; t + 2 < ic; t += 3) {
        const int a = rep[m.indices[t]], b = rep[m.indices[t + 1]],
                  c = rep[m.indices[t + 2]];
        for (const auto& pr : {std::make_pair(a, b), std::make_pair(b, c)}) {
            const int ra = find(pr.first), rb = find(pr.second);
            if (ra != rb) uf[static_cast<std::size_t>(ra)] = rb;
        }
    }
    std::map<int, float> reach;
    for (int v = 0; v < vc; ++v) {
        const int r = find(rep[static_cast<std::size_t>(v)]);
        const float ax = std::fabs(m.vertices[3 * v + 0]);
        auto it = reach.find(r);
        if (it == reach.end() || ax > it->second) reach[r] = ax;
    }
    if (reach.size() < 2) return false;  // one shell: nothing to split
    int lever = reach.begin()->first;
    for (const auto& kv : reach)
        if (kv.second > reach[lever]) lever = kv.first;

    auto build = [&](bool want_lever, Mesh& dst) {
        std::vector<int> remap(static_cast<std::size_t>(vc), -1);
        std::vector<float> pos, nrm;
        std::vector<unsigned short> idx;
        for (int t = 0; t + 2 < ic; t += 3) {
            if ((find(rep[m.indices[t]]) == lever) != want_lever) continue;
            for (int k = 0; k < 3; ++k) {
                const int v = m.indices[t + k];
                if (remap[static_cast<std::size_t>(v)] < 0) {
                    remap[static_cast<std::size_t>(v)] =
                        static_cast<int>(pos.size() / 3);
                    for (int j = 0; j < 3; ++j) {
                        pos.push_back(m.vertices[3 * v + j]);
                        nrm.push_back(
                            m.normals != nullptr ? m.normals[3 * v + j] : 0.0f);
                    }
                }
                idx.push_back(static_cast<unsigned short>(
                    remap[static_cast<std::size_t>(v)]));
            }
        }
        dst = Mesh{};
        dst.vertexCount = static_cast<int>(pos.size() / 3);
        dst.triangleCount = static_cast<int>(idx.size() / 3);
        if (dst.vertexCount == 0 || dst.triangleCount == 0) return false;
        dst.vertices =
            static_cast<float*>(std::calloc(pos.size(), sizeof(float)));
        dst.normals =
            static_cast<float*>(std::calloc(nrm.size(), sizeof(float)));
        dst.indices = static_cast<unsigned short*>(
            std::calloc(idx.size(), sizeof(unsigned short)));
        std::copy(pos.begin(), pos.end(), dst.vertices);
        std::copy(nrm.begin(), nrm.end(), dst.normals);
        std::copy(idx.begin(), idx.end(), dst.indices);
        return true;
    };
    Mesh rest{};
    if (!build(true, out) || !build(false, rest)) return false;
    std::free(m.vertices);
    std::free(m.normals);
    std::free(m.indices);
    m = rest;
    return true;
}

int find_node(const SledModel& sm, const char* name) {
    for (std::size_t i = 0; i < sm.nodes.size(); ++i)
        if (sm.nodes[i].name == name) return static_cast<int>(i);
    return -1;
}

void capture_rest_ik(SledModel& sm);

// ★★ R3-HANDS. The WORLD centre of a node's own geometry, not its origin.
//
// THE TRAP THIS EXISTS FOR, and it was caught by measuring rather than by
// reading: `throttle_block` and `brake_lever` BOTH have their node origin at
// the machine's centreline, (0, 0.700, 0.297) -- the same point. Their actual
// hardware lives in MESH space, the throttle at x ~ -0.19 and the brake at
// x ~ +0.30, i.e. on opposite sides. Aiming a control bone at the node origin
// therefore aims both hands at one fictional point 0.31-0.37 m away, and the
// resulting "measurement" would have been a true reading of a question nobody
// asked. Against the real geometry the targets sit 0.12 m and 0.073 m from
// their bones, on their own sides.
//
// Vertices are node-local here (the node transform is applied at draw), so the
// AABB is unioned in that space and transformed once at the end. Returns false
// when the node carries no primitives, which the caller reads as "do not aim".
bool node_geom_centre_world(const SledModel& sm, int node, glm::vec3& out);
void pose_pass(SledModel& sm, const SledRig& rig, float sag0, float hinge_theta,
               float root_fwd, float root_up, const WsState& ws);
std::array<glm::vec3, kRiderJointCount> posed_joint_pos(const SledModel& sm);
void capture_weight_shift(SledModel& sm);   // ★★★ R3-WS
void bake_weight_shift(SledModel& sm);      // ★★★ R3-WS
void ws_sweep_report(SledModel& sm);        // ★★★ R3-WS evidence
void pose_and_solve_lean(SledModel& sm, const SledRig& rig, float sag0);
float ws_declared_cg_y(const SledModel& sm, float a_m, const SledRig& rig);
glm::vec3 wpos(const glm::mat4& m);

// ★ G2h. The shortest rotation carrying unit `from` onto unit `to`. Both
// degenerate cases are NAMED rather than left to produce a silent NaN:
// already-aligned is the identity, and exactly-opposed has no shortest arc at
// all, so the axis there is an arbitrary perpendicular -- stated here so a
// reader knows it was decided and not missed. (Neither case can arise for the
// posture measurements below, where the seated lean is a few degrees off
// vertical, but a helper that NaNs on a re-export is a trap for the next rung.)
inline glm::quat q_from_to(const glm::vec3& from, const glm::vec3& to) {
    const glm::vec3 a = glm::normalize(from);
    const glm::vec3 b = glm::normalize(to);
    const float d = std::clamp(glm::dot(a, b), -1.0f, 1.0f);
    if (d > 1.0f - 1.0e-7f) return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 axis = glm::cross(a, b);
    if (glm::length(axis) < 1.0e-6f)
        axis = glm::cross(a, std::fabs(a.x) < 0.9f ? glm::vec3(1.0f, 0.0f, 0.0f)
                                                   : glm::vec3(0.0f, 1.0f, 0.0f));
    return glm::angleAxis(std::acos(d), glm::normalize(axis));
}

// ★ G2h. A node's rest world ROTATION with any scale normalised out -- the
// same guard `root_q`'s capture states ("normalising columns is what makes
// that a fact and not a hope"), written once so the four posture captures
// below cannot each forget it separately.
inline glm::quat rest_rot_q(const std::vector<glm::mat4>& rest, int node) {
    if (node < 0) return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::mat3 m(rest[static_cast<std::size_t>(node)]);
    for (int c = 0; c < 3; ++c) {
        const float L = glm::length(m[c]);
        if (L > 1.0e-8f) m[c] /= L;
    }
    return glm::quat_cast(m);
}

// ★ §D. Capture the reduced hinge model, exactly the way hand_off / foot_off are
// captured: from the rest pose, once, at load. The CG response to a unit root
// translation is MEASURED here rather than assumed to be 1.0, which is the whole
// finding of §D (the pinned hand and boot chains carry 42.2 % of the rider's
// mass and do not translate with the root, so the true response is 0.77-0.87,
// not 1.0).
//
// ★ R1c: it is now a 2x2 JACOBIAN, captured by THREE poses (rest + one probe
// per axis) instead of two. The off-diagonal terms are real -- a root rise
// changes how far forward the pinned arms and legs fold, and a root translation
// changes their height -- and carrying them is what lets two Newton steps close
// BOTH axes instead of one.
// ★★★ ST-5 THE UN-HUNCH, MEASURED. Reads `rest_world` and nothing else, so
// it can run the moment the rest pose exists and before any capture that
// poses. Every quantity it stores is described at its declaration in
// SledModel; what is worth saying HERE is why the tilt is taken pelvis->neck
// and not pelvis->spine_03: spine_03 is mid-back, and its axis reads 28.6 deg
// on this asset while the trunk a viewer calls "hunched" -- hips to collar --
// reads the ~33 deg the ST-5 plan quotes. The thing Chad looked at is the
// whole trunk, so the whole trunk is what gets measured.
//
// ⚠ A degenerate or missing chain leaves sit_hunch_rad at 0, which makes the
// entire rung the identity rather than a wrong pose: this is polish welded
// into the r4a lane's file, and it must never be the reason a rider draws
// broken.
void capture_sit_up(SledModel& sm) {
    sm.sit_hunch_rad = 0.0f;
    // ★★★ ST-5 THE SWEEP: five rungs, not three. The first three are the
    // un-hunch's spine; neck_01 and head carry the top 35 % of the twist.
    // A MISSING NECK OR HEAD IS NOT FATAL HERE -- it leaves that rung at -1
    // and the twist simply stops below it -- whereas a missing SPINE rung
    // still aborts the whole capture, because the un-hunch's three shares are
    // a spread of ONE rotation and two thirds of it is not a pose.
    const int joint[render::sting::kTwistCount] = {
        kRiderSpine1, kRiderSpine2, kRiderSpine3, kRiderNeck, kRiderHead};
    for (int k = 0; k < render::sting::kTwistCount; ++k) {
        sm.sit_spine[k] = -1;
        sm.sit_parent_q[k] = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        const int n = sm.rj[joint[k]];
        if (n < 0 || static_cast<std::size_t>(n) >= sm.nodes.size()) {
            if (k < 3) return;
            continue;
        }
        const int p = sm.nodes[n].parent;
        // The parent's rest world rotation, scale stripped -- "normalising
        // columns is what makes that a fact and not a hope", the same guard
        // the root/flight conjugations pay for.
        glm::mat3 pr = p >= 0 ? glm::mat3(sm.rest_world[p]) : glm::mat3(1.0f);
        bool ok = true;
        for (int c = 0; c < 3; ++c) {
            const float L = glm::length(pr[c]);
            if (L <= 1.0e-8f) {
                ok = false;
                break;
            }
            pr[c] /= L;
        }
        if (!ok) {
            if (k < 3) return;  // the un-hunch's spread cannot survive a hole
            continue;           // ...but the twist can stop at the chest
        }
        sm.sit_spine[k] = n;
        sm.sit_parent_q[k] = glm::quat_cast(pr);
    }
    if (sm.n_pelvis < 0 || sm.n_neck < 0) return;
    // THE TRUNK AXIS, in MODEL space. +Z is the nose (the coordinate law this
    // file steers by), so a trunk leaning toward the bars has a positive z
    // component and atan2(z, y) is the forward tilt off model up.
    const glm::vec3 trunk = wpos(sm.rest_world[sm.n_neck]) -
                            wpos(sm.rest_world[sm.n_pelvis]);
    if (glm::length(glm::vec2(trunk.y, trunk.z)) < 1.0e-4f) return;
    sm.sit_hunch_rad = std::atan2(trunk.z, trunk.y);
    TraceLog(LOG_INFO,
             "SLED: ST-5 rest trunk hunch %.2f deg (pelvis->neck_01); the "
             "shouldered pose takes out %.0f %% of it",
             static_cast<double>(sm.sit_hunch_rad * 57.2957795f),
             static_cast<double>(render::sting::kSitUpFrac * 100.0f));
}

void capture_hinge(SledModel& sm) {
    const SledRig zero{};  // all channels 0; with sag0 = 0 the susp delta is 0
    const WsState off;     // ★ R3-WS: the hinge model is a LADDER-FREE capture
    pose_pass(sm, zero, 0.0f, 0.0f, 0.0f, 0.0f, off);
    const std::array<glm::vec3, kRiderJointCount> rest = posed_joint_pos(sm);
    const glm::vec3 cg0 = rider_cg(rest);
    const float probe = 0.10f;  // the probe distance, in metres
    pose_pass(sm, zero, 0.0f, 0.0f, probe, 0.0f, off);
    const glm::vec3 cgz = rider_cg(posed_joint_pos(sm));
    pose_pass(sm, zero, 0.0f, 0.0f, 0.0f, probe, off);
    const glm::vec3 cgy = rider_cg(posed_joint_pos(sm));
    sm.hinge = capture_hinge_model(rest, (cgz.z - cg0.z) / probe);
    sm.hinge.k_zy = (cgy.z - cg0.z) / probe;
    sm.hinge.k_yz = (cgz.y - cg0.y) / probe;
    sm.hinge.k_yy = (cgy.y - cg0.y) / probe;
    // A rig whose vertical response is degenerate would make the 2x2 solve
    // meaningless; fall back to 1.0 exactly the way capture_hinge_model guards
    // k_hat, so the solve stays total.
    if (!(std::fabs(sm.hinge.k_yy) > 1e-3f)) sm.hinge.k_yy = 1.0f;
    TraceLog(LOG_INFO,
             "SLED: rider hinge model a %.6f b %.6f J[%.6f %.6f; %.6f %.6f] "
             "(§D + R1c)",
             static_cast<double>(sm.hinge.a), static_cast<double>(sm.hinge.b),
             static_cast<double>(sm.hinge.k_hat),
             static_cast<double>(sm.hinge.k_zy),
             static_cast<double>(sm.hinge.k_yz),
             static_cast<double>(sm.hinge.k_yy));
}

bool load_model() {
    SledModel& sm = g_sled;
    char path[512];
    std::snprintf(path, sizeof path, "%s/sled/indy650.glb", SEADS_ASSET_DIR);
    cgltf_options opt{};
    cgltf_data* data = nullptr;
    if (cgltf_parse_file(&opt, path, &data) != cgltf_result_success ||
        cgltf_load_buffers(&opt, data, path) != cgltf_result_success) {
        TraceLog(LOG_WARNING, "SLED: %s missing/unreadable; placeholder", path);
        if (data != nullptr) cgltf_free(data);
        return false;
    }

    // -- nodes (keep the full hierarchy; raylib's LoadModel flattens it,
    // which is exactly what this rung cannot afford)
    const std::size_t nn = data->nodes_count;
    sm.nodes.resize(nn);
    for (std::size_t i = 0; i < nn; ++i) {
        const cgltf_node& n = data->nodes[i];
        SNode& out = sm.nodes[i];
        out.name = n.name != nullptr ? n.name : "";
        if (n.has_translation)
            out.t = {n.translation[0], n.translation[1], n.translation[2]};
        if (n.has_rotation)
            out.r = glm::quat(n.rotation[3], n.rotation[0], n.rotation[1],
                              n.rotation[2]);
        if (n.has_scale) out.s = {n.scale[0], n.scale[1], n.scale[2]};
        for (std::size_t c = 0; c < n.children_count; ++c)
            sm.nodes[n.children[c] - data->nodes].parent = static_cast<int>(i);
    }
    // parents-before-children order (glTF does not guarantee array order)
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

    // -- skin (the rider): joint node indices + inverse bind matrices
    if (data->skins_count > 0) {
        const cgltf_skin& sk = data->skins[0];
        sm.skin_joints.resize(sk.joints_count);
        sm.skin_ibm.resize(sk.joints_count, glm::mat4(1.0f));
        for (std::size_t jj = 0; jj < sk.joints_count; ++jj) {
            sm.skin_joints[jj] =
                static_cast<int>(sk.joints[jj] - data->nodes);
            if (sk.inverse_bind_matrices != nullptr) {
                float m16[16];
                cgltf_accessor_read_float(sk.inverse_bind_matrices, jj, m16,
                                          16);
                std::memcpy(&sm.skin_ibm[jj], m16, sizeof m16);
            }
        }
    }

    // -- primitives -> raylib meshes (staging_snow, a Blender lighting prop,
    // is skipped: it is not part of the machine)
    for (std::size_t i = 0; i < nn; ++i) {
        const cgltf_node& n = data->nodes[i];
        if (n.mesh == nullptr || sm.nodes[i].name == "staging_snow") continue;
        const bool skinned = n.skin != nullptr;
        for (std::size_t pi = 0; pi < n.mesh->primitives_count; ++pi) {
            const cgltf_primitive& pr = n.mesh->primitives[pi];
            const cgltf_accessor* pos = nullptr;
            const cgltf_accessor* nrm = nullptr;
            const cgltf_accessor* jts = nullptr;
            const cgltf_accessor* wts = nullptr;
            const cgltf_accessor* col0 = nullptr;
            for (std::size_t a = 0; a < pr.attributes_count; ++a) {
                const cgltf_attribute& at = pr.attributes[a];
                if (at.type == cgltf_attribute_type_position) pos = at.data;
                if (at.type == cgltf_attribute_type_normal) nrm = at.data;
                if (at.type == cgltf_attribute_type_joints) jts = at.data;
                if (at.type == cgltf_attribute_type_weights) wts = at.data;
                if (at.type == cgltf_attribute_type_color) col0 = at.data;
            }
            if (pos == nullptr || nrm == nullptr || pr.indices == nullptr)
                continue;
            const int vc = static_cast<int>(pos->count);
            const int ic = static_cast<int>(pr.indices->count);
            SPrim sp;
            sp.node = static_cast<int>(i);
            sp.skinned = skinned;
            Mesh& m = sp.mesh;
            m.vertexCount = vc;
            m.triangleCount = ic / 3;
            m.vertices = static_cast<float*>(
                std::calloc(static_cast<std::size_t>(vc) * 3, sizeof(float)));
            m.normals = static_cast<float*>(
                std::calloc(static_cast<std::size_t>(vc) * 3, sizeof(float)));
            m.indices = static_cast<unsigned short*>(std::calloc(
                static_cast<std::size_t>(ic), sizeof(unsigned short)));
            for (int v = 0; v < vc; ++v) {
                cgltf_accessor_read_float(pos, v, m.vertices + 3 * v, 3);
                cgltf_accessor_read_float(nrm, v, m.normals + 3 * v, 3);
            }
            // ★ Sudburian head: COLOR_0 carries the face's baked grade
            // (windburn flush / rose lips / shade). Only the head patch's
            // prims have it; everything else keeps a null colour VBO and the
            // shader's constant white attribute.
            if (col0 != nullptr) {
                m.colors = static_cast<unsigned char*>(std::calloc(
                    static_cast<std::size_t>(vc) * 4, sizeof(unsigned char)));
                for (int v = 0; v < vc; ++v) {
                    float c4[4] = {1.0f, 1.0f, 1.0f, 1.0f};
                    cgltf_accessor_read_float(col0, v, c4, 4);
                    for (int k = 0; k < 4; ++k)
                        m.colors[4 * v + k] = static_cast<unsigned char>(
                            std::lround(std::clamp(c4[k], 0.0f, 1.0f) *
                                        255.0f));
                }
            }
            for (int k = 0; k < ic; ++k)
                m.indices[k] = static_cast<unsigned short>(
                    cgltf_accessor_read_index(pr.indices, k));
            if (skinned && jts != nullptr && wts != nullptr) {
                sp.base_pos.assign(m.vertices, m.vertices + 3 * vc);
                sp.base_nrm.assign(m.normals, m.normals + 3 * vc);
                sp.jidx.resize(static_cast<std::size_t>(vc) * 4);
                sp.jw.resize(static_cast<std::size_t>(vc) * 4);
                for (int v = 0; v < vc; ++v) {
                    cgltf_uint j4[4] = {0, 0, 0, 0};
                    cgltf_accessor_read_uint(jts, v, j4, 4);
                    for (int k = 0; k < 4; ++k)
                        sp.jidx[4 * v + k] =
                            static_cast<unsigned short>(j4[k]);
                    cgltf_accessor_read_float(wts, v, &sp.jw[4 * v], 4);
                }
            }
            // ★ THE MULLET'S WIND WEIGHTS (Sudburian head, 2026-09-04).
            // Fade and column amplitude are geometric properties of the BIND
            // curtain, derived here once: pinned for the top ~30% (under the
            // helmet, where the sculpt's solidify fades to zero thickness),
            // free by ~60% down; the middle column rides 1.6x because the
            // helmet's rear opening frees it (the Blender rig's spec).
            if (skinned && sm.nodes[i].name == "sudburian_mullet") {
                sp.is_mullet = true;
                float y_min = 1.0e9f, y_max = -1.0e9f;
                for (int v = 0; v < vc; ++v) {
                    y_min = std::min(y_min, sp.base_pos[3 * v + 1]);
                    y_max = std::max(y_max, sp.base_pos[3 * v + 1]);
                }
                const float span = std::max(y_max - y_min, 1.0e-4f);
                sp.wind.resize(static_cast<std::size_t>(vc));
                for (int v = 0; v < vc; ++v) {
                    const float yn =
                        (y_max - sp.base_pos[3 * v + 1]) / span;  // 0 top
                    const float fade =
                        std::clamp((yn - 0.30f) / 0.30f, 0.0f, 1.0f);
                    const float colf = std::clamp(
                        1.0f - std::fabs(sp.base_pos[3 * v]) / 0.055f, 0.0f,
                        1.0f);
                    sp.wind[static_cast<std::size_t>(v)] =
                        glm::vec2(fade, 1.0f + 0.6f * colf);
                }
            }
            if (pr.material != nullptr &&
                pr.material->has_pbr_metallic_roughness) {
                const cgltf_float* c =
                    pr.material->pbr_metallic_roughness.base_color_factor;
                // ROUND, never truncate: 0.12 * 255 = 30.6 used to land on 30,
                // so the helmet's slag orange came out #FF8C1E while the lava
                // shader's GPU rounding of the same kSlagOrange gives #FF8C1F
                // (Chad, 2026-08-19: "the exact right code"). Every material
                // moves by at most one LSB toward its true byte.
                sp.col = Color{static_cast<unsigned char>(std::lround(c[0] * 255.0f)),
                               static_cast<unsigned char>(std::lround(c[1] * 255.0f)),
                               static_cast<unsigned char>(std::lround(c[2] * 255.0f)), 255};
                // ★ THE ONLY GLOSSY THINGS ON THE MACHINE ARE THE HELMET'S
                // (docs/HELMET_SPEC.md 3, red-team R4). Gate on the MATERIAL
                // NAME: 24 of the 34 sled materials carry roughness < 0.5, so a
                // roughness gate alone would put a highlight on the whole hood.
                // SEADS_HELMET_GLOSS=0 forces every gloss to 0 -- that is how
                // the "bit-identical everywhere else" claim gets MEASURED
                // instead of asserted (H7).
                static const bool gloss_off =
                    []() {
                        const char* e = std::getenv("SEADS_HELMET_GLOSS");
                        return e != nullptr && e[0] == '0';
                    }();
                const char* mname = pr.material->name;
                if (!gloss_off && mname != nullptr &&
                    std::strncmp(mname, "helmet_", 7) == 0) {
                    const float rough =
                        pr.material->pbr_metallic_roughness.roughness_factor;
                    sp.gloss = std::clamp((0.5f - rough) / 0.5f, 0.0f, 1.0f);
                }
                // ★ R3-HANDS: the MOVING half of the BRAKE, keyed on the
                // material for the same reason -- `indy_red` is its blade and
                // `indy_rubber` the perch it hinges on. ONLY the brake: red on
                // the throttle is the kill cap (Chad: "throttle is not red"),
                // and flagging it there animated the wrong part.
                if (mname != nullptr && std::strcmp(mname, "indy_red") == 0 &&
                    sm.nodes[i].name == "brake_lever")
                    sp.is_lever = true;
                // The scarf's live colour A/B (SEADS_SCARF_COLOR="r,g,b"),
                // keyed on the MATERIAL name so it survives any node rename.
                if (mname != nullptr &&
                    std::strcmp(mname, "sudburian_scarf_blue") == 0) {
                    sp.is_scarf = true;
                    static const char* col_env =
                        std::getenv("SEADS_SCARF_COLOR");
                    int r = 0, g = 0, b = 0;
                    if (col_env != nullptr &&
                        std::sscanf(col_env, "%d,%d,%d", &r, &g, &b) == 3) {
                        sp.col = Color{static_cast<unsigned char>(r),
                                       static_cast<unsigned char>(g),
                                       static_cast<unsigned char>(b), 255};
                        sp.col_locked = true;  // the A/B beats the team kit
                    }
                }
                // ★ The helmet's team band. The GLB already carries
                // kSlagOrange here (test_rider_pose H8 pins the asset), so the
                // VALLEY kit repaints it with the identical bytes and the
                // CENTRAL CITY kit turns it blue.
                if (mname != nullptr &&
                    std::strcmp(mname, "helmet_orange") == 0)
                    sp.is_helmet_band = true;
            }
            const std::string& nm = sm.nodes[i].name;
            if (nm == "helmet_sudburian") sp.dent = 0;
            else if (nm.rfind("helmet_dent_", 0) == 0)
                sp.dent = nm.back() - '0';
            // -- THE EMISSIVE TABLE.
            // The two gauge entries here used to read "gauge_face_L/R", names
            // that have never existed in the asset -- the objects are
            // indy650_-prefixed -- so the dials have never once lit. Both
            // spellings are accepted now so a future rename cannot silently
            // put them out again.
            const bool gauge_face = nm == "indy650_gauge_face_L" ||
                                    nm == "indy650_gauge_face_R" ||
                                    nm == "gauge_face_L" || nm == "gauge_face_R";
            if (nm == "headlight_lens") {
                // ARCADE OVERDRIVE. Chad: "The lamp on the machine needs to be
                // massively over powered this is an arcade." Far above the
                // halation threshold on purpose, so the lamp reads as a hot
                // source with a bloom around it rather than a white rectangle.
                sp.emit = 9.0f;
                sp.emit_col = Color{255, 246, 214, 255};  // warm tungsten
            } else if (nm == "taillight") {
                sp.emit = 2.6f;
                sp.emit_col = Color{255, 40, 24, 255};
            } else if (gauge_face) {
                // The dials, in the codebase's one warm hue. Just over the
                // halation threshold: enough to sit up off the dark cockpit
                // and carry a faint glow at night, not enough to flare and
                // wash out the cockpit around it.
                sp.emit = 1.45f;
                sp.emit_col = slag_color();
            } else if (nm == "indy650_gauge_glass_L" ||
                       nm == "indy650_gauge_glass_R") {
                // The glass sits IN FRONT of the dial and this shader is
                // opaque, so the glass is the disc you actually see -- light
                // the dial only and the lens hides it. Slightly under the face
                // so the two never read as one flat chip.
                sp.emit = 1.25f;
                sp.emit_col = slag_color();
            } else if (nm.rfind("indy650_ind_lens_", 0) == 0) {
                // The warning lamps keep their own signal colours -- an amber
                // dash and a red OIL light are different information.
                sp.emit = 1.30f;
                sp.emit_col = sp.col;
            }
            // ★★ R3-HANDS: peel the throttle LEVER out of the housing's own
            // primitive so it can hinge on its own. Only this one primitive is
            // ever split; everything else takes the single-shell early-out.
            Mesh lever_mesh{};
            const bool split_lever =
                sm.nodes[i].name == "throttle_block" && !skinned &&
                pr.material != nullptr && pr.material->name != nullptr &&
                std::strcmp(pr.material->name, "indy_rubber") == 0 &&
                split_outboard_shell(m, lever_mesh);
            UploadMesh(&m, skinned);  // dynamic buffers for CPU skinning
            if (split_lever) {
                SPrim lp = sp;         // same material / emissive / colour
                lp.mesh = lever_mesh;  // ...but only the lever's triangles
                lp.is_lever = true;
                lp.base_pos.clear();
                lp.base_nrm.clear();
                lp.jidx.clear();
                lp.jw.clear();
                UploadMesh(&lp.mesh, false);
                TraceLog(LOG_INFO,
                         "SLED: R3-HANDS -- throttle lever split off its "
                         "housing (%d verts / %d tris)",
                         lp.mesh.vertexCount, lp.mesh.triangleCount);
                sm.prims.push_back(std::move(lp));
            }
            sm.prims.push_back(std::move(sp));
        }
    }

    // ★ THE SCARF'S COLOUR (Chad's 2026-08-20 ruling) now lives IN THE GLB:
    // patch_scarf.py writes material `sudburian_scarf_blue` with
    // kComplementBlue PARSED from render/team_color.h (independently verified
    // against the executable invariant in test_team_color.cpp). The prim
    // capture above reads it back like any other material; the
    // SEADS_SCARF_COLOR="r,g,b" A/B override is applied there, by material
    // name.
    cgltf_free(data);

    // -- channel nodes, bound through the DECLARED CATALOGUE (render/rider_rig.h,
    // SUDBURIAN_LADDER §2.3a). This block used to be two dozen bare find_node()
    // calls whose -1 returns were never checked: a missing `forearm_L` skipped
    // the whole left arm with no warning anywhere (defect 13). Now every
    // declared node is bound in one pass and absence is a NAMED, LOUD failure,
    // exactly like the CH_susp_L/R check it replaces.
    const RigBindResult bind = bind_rider_rig([&sm](const char* n) {
        return find_node(sm, n);
    });
    if (!bind.complete()) {
        TraceLog(LOG_ERROR,
                 "SLED: declared rig nodes MISSING in GLB (%d): %s -- the rider "
                 "would be silently crippled, so the hero model is refused",
                 static_cast<int>(bind.missing.size()),
                 describe_missing(bind).c_str());
        return false;
    }
    // Suspension binds by POSITION (coordinate law above): kernel 0 (rider's
    // LEFT ski) = the CH_susp node at model +X. The catalogue supplies the two
    // indices; the POSITION test that orders them is unchanged.
    const int csl = bind.machine[kMachSuspL];
    const int csr = bind.machine[kMachSuspR];
    const bool l_is_posx = sm.nodes[csl].t.x > sm.nodes[csr].t.x;
    sm.n_susp[0] = l_is_posx ? csl : csr;  // kernel L  -> +X node
    sm.n_susp[1] = l_is_posx ? csr : csl;  // kernel R  -> -X node
    sm.n_susp[2] = bind.machine[kMachSuspT];
    sm.n_ski[0] = bind.machine[kMachSkiL];
    sm.n_ski[1] = bind.machine[kMachSkiR];
    sm.n_steer = bind.machine[kMachSteerPivot];
    sm.n_root = bind.joint[kRiderRoot];
    sm.n_pelvis = bind.joint[kRiderPelvis];
    sm.n_head = bind.joint[kRiderHead];
    sm.n_neck = bind.joint[kRiderNeck];   // ★ R3-WS(c), the head-up counter
    for (int j = 0; j < kRiderJointCount; ++j)
        sm.rj[j] = bind.joint[static_cast<std::size_t>(j)];
    // side 0 = model -X ("_L" nodes), side 1 = +X -- the chain catalogue's own
    // model_side, so the mapping is declared rather than re-typed here.
    for (const RiderChainSpec& c : rider_chain_specs()) {
        int* dst = c.is_arm ? sm.arm[c.model_side] : sm.leg[c.model_side];
        for (int k = 0; k < 3; ++k)
            dst[k] = bind.joint[static_cast<std::size_t>(c.joint[k])];
        (c.is_arm ? sm.n_grip : sm.n_board)[c.model_side] =
            bind.machine[static_cast<std::size_t>(c.socket)];
    }

    // -- rest pose (all channels zero) captured once: IK bend planes, twist
    // offsets and the hand<->grip weld transforms are all defined off it.
    sm.rest_world.resize(nn, glm::mat4(1.0f));
    for (const int i : sm.order) {
        const SNode& nd = sm.nodes[i];
        const glm::mat4 local = compose(nd.t, nd.r, nd.s);
        sm.rest_world[i] =
            nd.parent < 0 ? local : sm.rest_world[nd.parent] * local;
    }
    // ★ the two rest frames pose_pass expresses model-frame demands through
    if (sm.n_root >= 0) {
        const int rp = sm.nodes[sm.n_root].parent;
        sm.root_parent_R =
            rp >= 0 ? glm::mat3(sm.rest_world[rp]) : glm::mat3(1.0f);
        sm.root_R = glm::mat3(sm.rest_world[sm.n_root]);
        // strip any scale before quaternion conversion (the file has none;
        // normalising columns is what makes that a fact and not a hope)
        glm::mat3 rr = sm.root_R;
        for (int c = 0; c < 3; ++c) rr[c] = glm::normalize(rr[c]);
        sm.root_q = glm::quat_cast(rr);
    }
    // ★★★ G2h: THE POSTURE CAPTURE -- the straight back AND the plumb head,
    // measured once off the rest pose itself, five segments in one accumulating
    // sweep. (G2g's version measured the pelvis->neck CHORD and split it in
    // three; see `gait_straight_q`'s banner for why a chord cannot see the
    // curvature it is the average of, and why the head then leaned back.)
    //
    // THE ONE PIECE OF ALGEBRA THIS RESTS ON, written out so nobody has to
    // re-derive it at the next rung. `pose_pass` inserts these as plain LOCAL
    // pre-rotations, `local = c_k * rest_local`, and `c_k` is conjugated into
    // joint k's PARENT's REST frame. Compose that up the hierarchy:
    //     R_k_now = Qacc * parent_rest_R * c_k * rest_local
    //             = Qacc * parent_rest_R * conj(qpar) * C_k * qpar * rest_local
    //             = Qacc * C_k * parent_rest_R * rest_local
    //             = Qacc * C_k * R_k_rest
    // where `Qacc` is everything the joints BELOW k already contributed and
    // `C_k` is the same rotation read in MODEL space. So each segment's live
    // direction is `Qacc * C_k * dir_rest[k]`, and standing it upright means
    //     C_k = q_from_to(dir_rest[k], conj(Qacc) * up)
    // -- the `conj(Qacc)` is the whole reason this is a sweep and not five
    // independent measurements. `Qacc` then picks up `C_k` and moves on.
    //
    // The five segments, each read as the joint's OWN rest +Y column (every
    // rig row in render/rider_rig.cpp offsets its child along local +Y, so
    // that column IS the direction to the child -- and for `head`, which has
    // no child joint, it is the skull axis, which is exactly what "straight up
    // and down" is asking about):
    //     spine_01 -> spine_02 -> spine_03 -> neck_01 -> head -> (skull axis)
    {
        const glm::vec3 upm(0.0f, 1.0f, 0.0f);
        const int seg[5] = {sm.rj[kRiderSpine1], sm.rj[kRiderSpine2],
                            sm.rj[kRiderSpine3], sm.n_neck, sm.n_head};
        glm::quat qacc(1.0f, 0.0f, 0.0f, 0.0f);
        float deg[5] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
        for (int k = 0; k < 5; ++k) {
            if (seg[k] < 0) continue;
            const glm::quat rest_q = rest_rot_q(sm.rest_world, seg[k]);
            const glm::vec3 dir_rest = glm::normalize(rest_q * upm);
            const glm::quat c_model =
                q_from_to(dir_rest, glm::conjugate(qacc) * upm);
            deg[k] = 2.0f * std::acos(std::clamp(c_model.w, -1.0f, 1.0f)) *
                     57.29578f;
            const glm::quat qpar = rest_rot_q(sm.rest_world,
                                              sm.nodes[seg[k]].parent);
            const glm::quat c_local =
                glm::conjugate(qpar) * c_model * qpar;
            if (k < 3)
                sm.gait_straight_q[k] = c_local;
            else
                sm.gait_head_q[k - 3] = c_local;
            qacc = qacc * c_model;
        }
        // The whole posture in one row, because the next reader of this rung
        // will want the numbers and not the derivation: how far each of the
        // five segments was off vertical in the AUTHORED SEATED rest, which is
        // exactly how much this term stands each of them up.
        TraceLog(LOG_INFO,
                 "SLED: G2h posture -- upright corrections deg: spine_01 %.2f "
                 "spine_02 %.2f spine_03 %.2f neck_01 %.2f head %.2f",
                 static_cast<double>(deg[0]), static_cast<double>(deg[1]),
                 static_cast<double>(deg[2]), static_cast<double>(deg[3]),
                 static_cast<double>(deg[4]));
    }
    capture_sit_up(sm);  // ★★★ ST-5: rest-only, so it may precede every pose
    capture_rest_ik(sm);
    capture_hinge(sm);  // ★ §D, and it must follow capture_rest_ik
    capture_weight_shift(sm);  // ★★★ R3-WS, and it must follow both
    bake_weight_shift(sm);     // ... and the bake must follow the captures
    ws_sweep_report(sm);       // SEADS_SLED_WS_DEBUG only; no-op otherwise

    // ★ THE SCARF BIND (SCARF_SPEC §4.1). MEASURED in the shipped GLB: the six
    // bones scarf_01..06 ARE skin joints 8..13 of the 42-joint `sudburian_rig`
    // -- they exist, and ZERO vertices weight to them, so today they drive a
    // procedural ribbon and tomorrow they drive an authored cloth unchanged.
    // A missing scarf DISABLES the scarf and nothing else: unlike a missing
    // arm, it is not a crippled rider, and the machine must still draw.
    {
        // neck / head / pelvis come from the DECLARED catalogue (they were
        // bound and loudly validated above) -- never a second bare find_node.
        sm.n_neck = sm.rj[kRiderNeck];
        std::string missing;
        auto need = [&missing](int idx, const char* nm) {
            if (idx < 0) {
                if (!missing.empty()) missing += ", ";
                missing += nm;
            }
        };
        need(sm.n_neck, "neck_01");
        need(sm.n_head, "head");
        need(sm.n_pelvis, "pelvis");
        for (int k = 0; k < kScarfSegments; ++k) {
            char nm[16];
            std::snprintf(nm, sizeof nm, "scarf_%02d", k + 1);
            sm.scarf_node[k] = find_node(sm, nm);
            need(sm.scarf_node[k], nm);
        }
        // ONE warning, naming every absent bone (§4.1).
        if (!missing.empty())
            TraceLog(LOG_WARNING,
                     "SCARF: bones MISSING in the GLB (%s) -- the scarf is OFF; "
                     "the machine still draws", missing.c_str());
        sm.scarf_ok = missing.empty();
        if (sm.scarf_ok) {
            // seg_len is MEASURED off the file (the mean local |t| of
            // scarf_02..06 = 0.080000 m), never the retyped literal.
            float sum = 0.0f;
            for (int k = 1; k < kScarfSegments; ++k)
                sum += glm::length(sm.nodes[sm.scarf_node[k]].t);
            sm.scarf_seg_len = sum / static_cast<float>(kScarfSegments - 1) *
                               kScarfRunSegFrac;  // pods are authored SEG_RUN
            sm.scarf_par.segments = kScarfSegments;
            sm.scarf_par.seg_len_m = sm.scarf_seg_len;
            // The knot's rest TRS IN THE NECK'S FRAME. The anchor position, the
            // rest chain direction and the ribbon's width axis all come from
            // this one matrix, so nothing about the scarf is written down here.
            const SNode& s0 = sm.nodes[sm.scarf_node[0]];
            sm.scarf_anchor_local = compose(s0.t, s0.r, s0.s);
            // PRIME AT BIND along the rest chain, so a 0-tick first frame draws
            // a resting scarf and never an unprimed one (red-team P2-4).
            const glm::mat4& aw = sm.rest_world[sm.scarf_node[0]];
            const glm::vec3 dir_rest =
                glm::normalize(glm::vec3(aw * glm::vec4(0, 1, 0, 0)));
            trail_chain_reset(sm.scarf, sm.scarf_par, glm::vec3(aw[3]),
                              dir_rest);
            // ★ MEASURE THE SUIT'S BACK (red-team P1-1). Skin the proxy's rest
            // vertices ONCE with the rest joint matrices (rest_world * ibm --
            // the same product the per-frame skinning uses, so this is the
            // surface the player sees, not the mesh node's raw space, which
            // carries the R2b seating yaw and reads 0.44 m off), and take the
            // farthest vertex behind the rest torso plane inside the torso
            // band. The rest torso plane is §3b evaluated on rest_world.
            {
                const glm::vec3 neck_r(sm.rest_world[sm.n_neck][3]);
                const glm::vec3 pelvis_r(sm.rest_world[sm.n_pelvis][3]);
                const glm::vec3 anchor_r(aw[3]);
                const glm::vec3 torso_r = neck_r - pelvis_r;
                const float torso_len = glm::length(torso_r);
                float s_max = -1.0e9f;
                float s_anchor = 0.0f;
                if (torso_len > 1.0e-4f) {
                    const glm::vec3 axis = torso_r / torso_len;
                    glm::vec3 b = anchor_r - neck_r;
                    b -= axis * glm::dot(b, axis);
                    if (glm::length(b) > 1.0e-4f) {
                        const glm::vec3 back_n = glm::normalize(b);
                        const glm::vec3 lat = glm::cross(axis, back_n);
                        s_anchor = glm::dot(anchor_r - neck_r, back_n);
                        std::vector<glm::mat4> jm_rest(sm.skin_joints.size());
                        for (std::size_t j = 0; j < sm.skin_joints.size(); ++j)
                            jm_rest[j] = sm.rest_world[sm.skin_joints[j]] *
                                         sm.skin_ibm[j];
                        for (const SPrim& sp : sm.prims) {
                            if (!sp.skinned || sp.is_scarf) continue;
                            const int vc = sp.mesh.vertexCount;
                            for (int v = 0; v < vc; ++v) {
                                glm::mat4 mtx(0.0f);
                                for (int k = 0; k < 4; ++k) {
                                    const float w = sp.jw[4 * v + k];
                                    if (w > 0.0f)
                                        mtx += jm_rest[sp.jidx[4 * v + k]] * w;
                                }
                                const glm::vec3 pw(
                                    mtx * glm::vec4(sp.base_pos[3 * v],
                                                    sp.base_pos[3 * v + 1],
                                                    sp.base_pos[3 * v + 2],
                                                    1.0f));
                                const float t =
                                    glm::dot(pw - pelvis_r, axis) / torso_len;
                                if (t < -0.05f || t > 0.98f) continue;
                                if (std::fabs(glm::dot(pw - neck_r, lat)) >
                                    kScarfTorsoHalfWidthM)
                                    continue;
                                const float sb = glm::dot(pw - neck_r, back_n);
                                if (sb > s_max) s_max = sb;
                            }
                        }
                    }
                }
                // ---- ring capture for the banded planes (P0-1). The
                // back-face verts sit at |lateral| 0.215..0.221 -- the old
                // 0.15 filter rejected every one of them.
                if (torso_len > 1.0e-4f) {
                    const glm::vec3 axis2 = torso_r / torso_len;
                    glm::vec3 b2 = anchor_r - neck_r;
                    b2 -= axis2 * glm::dot(b2, axis2);
                    if (glm::length(b2) > 1.0e-4f) {
                        const glm::vec3 back_n2 = glm::normalize(b2);
                        const glm::vec3 lat2 = glm::cross(axis2, back_n2);
                        std::vector<glm::mat4> jm_rest(sm.skin_joints.size());
                        for (std::size_t jj2 = 0; jj2 < sm.skin_joints.size();
                             ++jj2)
                            jm_rest[jj2] = sm.rest_world[sm.skin_joints[jj2]] *
                                           sm.skin_ibm[jj2];
                        struct Cand {
                            float t;
                            int prim, vert;
                        };
                        std::vector<Cand> cand;
                        for (std::size_t pi = 0; pi < sm.prims.size(); ++pi) {
                            const SPrim& sp = sm.prims[pi];
                            if (!sp.skinned || sp.is_scarf) continue;
                            for (int v = 0; v < sp.mesh.vertexCount; ++v) {
                                const glm::mat4& mj =
                                    jm_rest[sp.jidx[4 * v]];
                                const glm::vec3 pw(
                                    mj * glm::vec4(sp.base_pos[3 * v],
                                                   sp.base_pos[3 * v + 1],
                                                   sp.base_pos[3 * v + 2],
                                                   1.0f));
                                const float t =
                                    glm::dot(pw - pelvis_r, axis2) /
                                    torso_len;
                                if (t < -0.05f || t > 1.05f) continue;
                                const float sb =
                                    glm::dot(pw - neck_r, back_n2);
                                if (sb < 0.09f) continue;
                                if (std::fabs(glm::dot(pw - neck_r, lat2)) >
                                    0.26f)
                                    continue;
                                cand.push_back(
                                    {t, static_cast<int>(pi), v});
                            }
                        }
                        std::sort(cand.begin(), cand.end(),
                                  [](const Cand& a, const Cand& b) {
                                      return a.t < b.t;
                                  });
                        sm.back_verts.clear();
                        // ★★★ BAND BY t VALUE, NOT BY GAPS BETWEEN CANDIDATES.
                        //
                        // The original banding opened a new ring only where
                        // consecutive sorted candidates were more than 0.05
                        // apart in t. That is a property of how DENSELY the
                        // back happens to be tessellated, not of the back, and
                        // it silently collapsed the moment the costume was
                        // exported into the GLB (2026-08-24). MEASURED, same
                        // binary, the two files A/B:
                        //
                        //   no costume    328 verts ->  7 rings  keep-out 0.027
                        //                 back_clr +0.095  (rests ON the back)
                        //   with costume 5675 verts ->  1 ring   keep-out 0.259
                        //                 back_clr -0.070  (70 mm INSIDE it)
                        //
                        // One ring means n_back_planes never gets set, so the
                        // solver falls back to the SINGLE chord plane -- the
                        // very fit the consult rejected for a bent torso -- and
                        // the fallback keep-out is s_max, the FARTHEST vertex
                        // in the band, which with a coat in the file is the
                        // skirt at 0.2322 m. A 0.2592 keep-out is one the chain
                        // cannot satisfy, so it sat 70 mm inside the coat.
                        // That is Chad's "in the game the scarf sinks down".
                        //
                        // Equal-width bands over the observed t range depend on
                        // the torso, not the tessellation, so the same body
                        // gives the same planes whether it is wearing 328 verts
                        // or 5675.
                        // ★ THE EMPTY GUARD IS LOAD-BEARING. The banding this
                        // replaced walked `cand` and left ring = -1 on an empty
                        // list, so back_n_rings came out 0 and the caller took
                        // the single-plane path. front()/back() on an empty
                        // vector is UB -- a scan that finds nothing (a filter
                        // change, a rider with no back-facing verts in band)
                        // would read garbage instead of disabling.
                        const int kRings = kTrailChainMaxBackPlanes + 1;
                        const float t_lo = cand.empty() ? 0.0f : cand.front().t;
                        const float t_hi = cand.empty() ? 0.0f : cand.back().t;
                        const float t_span = t_hi - t_lo;
                        if (!cand.empty() && t_span > 1.0e-4f) {
                            for (const Cand& c : cand) {
                                int ring = static_cast<int>(
                                    (c.t - t_lo) / t_span * float(kRings));
                                if (ring >= kRings) ring = kRings - 1;
                                if (ring < 0) ring = 0;
                                sm.back_verts.push_back(
                                    {c.prim, c.vert, ring});
                            }
                            sm.back_n_rings = kRings;
                        } else {
                            sm.back_n_rings = 0;
                        }
                        TraceLog(LOG_INFO,
                                 "SCARF: drawn-back capture -- %d verts in %d "
                                 "rings (need >= 2 for banded planes)",
                                 static_cast<int>(sm.back_verts.size()),
                                 sm.back_n_rings);
                        // ★ SCARF-DRAPE slab: the TORSO-side capture. Wider
                        // laterally (0.45) and reaching around the sides
                        // (sb >= -0.02), but joint-filtered to the torso
                        // chain so a forward-reaching ARM never inflates a
                        // band's width. Ring by the SAME t banding.
                        sm.side_verts.clear();
                        {
                            const char* tn[5] = {"pelvis", "spine_01",
                                                 "spine_02", "spine_03",
                                                 "neck_01"};
                            for (int q = 0; q < 5; ++q)
                                sm.torso_nodes[q] = find_node(sm, tn[q]);
                        }
                        if (sm.back_n_rings >= 2) {
                            for (std::size_t pi = 0; pi < sm.prims.size();
                                 ++pi) {
                                const SPrim& sp = sm.prims[pi];
                                if (!sp.skinned || sp.is_scarf) continue;
                                for (int v = 0; v < sp.mesh.vertexCount;
                                     ++v) {
                                    std::size_t jj = sp.jidx[4 * v];
                                    float bw = sp.jw[4 * v];
                                    for (int q = 1; q < 4; ++q)
                                        if (sp.jw[4 * v + q] > bw) {
                                            bw = sp.jw[4 * v + q];
                                            jj = sp.jidx[4 * v + q];
                                        }
                                    const int nd3 = sm.skin_joints[jj];
                                    bool torso_v = false;
                                    for (int q = 0; q < 5; ++q)
                                        if (nd3 == sm.torso_nodes[q])
                                            torso_v = true;
                                    if (!torso_v) continue;
                                    const glm::mat4& mj =
                                        jm_rest[sp.jidx[4 * v]];
                                    const glm::vec3 pw(
                                        mj *
                                        glm::vec4(sp.base_pos[3 * v],
                                                  sp.base_pos[3 * v + 1],
                                                  sp.base_pos[3 * v + 2],
                                                  1.0f));
                                    const float t =
                                        glm::dot(pw - pelvis_r, axis2) /
                                        torso_len;
                                    if (t < -0.05f || t > 1.05f) continue;
                                    // the FULL torso round: chest included,
                                    // for the slab's front face.
                                    if (std::fabs(glm::dot(pw - neck_r,
                                                           lat2)) > 0.45f)
                                        continue;
                                    int ring = static_cast<int>(
                                        (t - t_lo) / t_span * float(kRings));
                                    if (ring >= kRings) ring = kRings - 1;
                                    if (ring < 0) ring = 0;
                                    sm.side_verts.push_back(
                                        {static_cast<int>(pi), v, ring});
                                }
                            }
                            TraceLog(LOG_INFO,
                                     "SCARF: torso-side capture -- %d verts "
                                     "(per-band slab widths)",
                                     static_cast<int>(sm.side_verts.size()));
                        }
                        // ★ AND THE SCARF'S OWN DRAWN VERTS, for surf_vtx.
                        // Captured unconditionally so the bind path does not
                        // depend on an env var (an env-dependent bind is a
                        // fork waiting to happen); only the MEASUREMENT is
                        // gated on SEADS_SCARF_DEBUG.
                        sm.scarf_surf_verts.clear();
                        for (std::size_t pi = 0; pi < sm.prims.size(); ++pi) {
                            const SPrim& sp = sm.prims[pi];
                            if (!sp.skinned || !sp.is_scarf) continue;
                            for (int v = 0; v < sp.mesh.vertexCount; ++v) {
                                const int nd =
                                    sm.skin_joints[sp.jidx[4 * v]];
                                bool driven = false;
                                for (int k = 0; k < kScarfSegments; ++k)
                                    if (nd == sm.scarf_node[k]) driven = true;
                                for (int k = 0; k < kScarfShortSegs; ++k)
                                    if (nd == sm.scarf_s_node[k])
                                        driven = true;
                                // ★ 2026-08-27, CHAD: "right at knots". The
                                // first cut EXCLUDED the wrap and knot on the
                                // reasoning that they are rigid to neck_01 and
                                // no keep-out constant can move them -- which
                                // is true, and which made the instrument BLIND
                                // TO THE EXACT REGION HE NAMED. Capture both
                                // and report them SEPARATELY: .ring 0 = the
                                // solver-driven chain (a keep-out can fix it),
                                // .ring 1 = the authored wrap/knot (only the
                                // asset can). Which of the two is negative
                                // decides which lane the fix lives in.
                                sm.scarf_surf_verts.push_back(
                                    {static_cast<int>(pi), v, driven ? 0 : 1});
                            }
                        }
                        TraceLog(LOG_INFO,
                                 "SCARF: drawn-SCARF capture -- %d solver-"
                                 "driven verts (surf_vtx grades THESE)",
                                 static_cast<int>(
                                     sm.scarf_surf_verts.size()));
                        // ★ SCARF-DRAPE (2026-09-03): THE PODS' OWN BOXES,
                        // measured in each bone's LOCAL frame -- which IS the
                        // probe station frame, because trail_chain_frames
                        // writes bone k's world as [X,Y,Z | p[k]] and the pods
                        // are rigid one-joint skins: drawn vert = p[k] +
                        // X*L.x + Y*L.y + Z*L.z with L = ibm * v, a CONSTANT.
                        // Chad, 2026-09-03: "as he leans forward / back his
                        // scarf goes into his coat" -- surf_vtx measured the
                        // fabric -0.064 m inside at full forward lean while
                        // the centreline stayed legal. The centreline was the
                        // wrong thing to grade; these boxes are the drawn
                        // thing, and trail_chain now holds THEM off the
                        // banded planes.
                        {
                            glm::vec3 lo[kScarfSegments + kScarfShortSegs];
                            glm::vec3 hi[kScarfSegments + kScarfShortSegs];
                            bool any[kScarfSegments + kScarfShortSegs] = {};
                            for (int k = 0;
                                 k < kScarfSegments + kScarfShortSegs; ++k) {
                                lo[k] = glm::vec3(1.0e9f);
                                hi[k] = glm::vec3(-1.0e9f);
                            }
                            for (const SledModel::BackVert& sv :
                                 sm.scarf_surf_verts) {
                                if (sv.ring != 0) continue;  // pods only
                                const SPrim& sp2 = sm.prims[sv.prim];
                                // ★ The MAX-WEIGHT joint, never slot 0 blind
                                // (red-team finding 6): glTF orders the
                                // 4-joint slots arbitrarily, and this box is
                                // now a CONSTRAINT, not an instrument.
                                std::size_t jv = sp2.jidx[4 * sv.vert];
                                {
                                    float bw = sp2.jw[4 * sv.vert];
                                    for (int q = 1; q < 4; ++q)
                                        if (sp2.jw[4 * sv.vert + q] > bw) {
                                            bw = sp2.jw[4 * sv.vert + q];
                                            jv = sp2.jidx[4 * sv.vert + q];
                                        }
                                }
                                const int nd2 = sm.skin_joints[jv];
                                int bone = -1;
                                for (int k = 0; k < kScarfSegments; ++k)
                                    if (nd2 == sm.scarf_node[k]) bone = k;
                                for (int k = 0; k < kScarfShortSegs; ++k)
                                    if (nd2 == sm.scarf_s_node[k])
                                        bone = kScarfSegments + k;
                                if (bone < 0) continue;
                                const glm::vec3 L(
                                    sm.skin_ibm[jv] *
                                    glm::vec4(sp2.base_pos[3 * sv.vert],
                                              sp2.base_pos[3 * sv.vert + 1],
                                              sp2.base_pos[3 * sv.vert + 2],
                                              1.0f));
                                lo[bone] = glm::min(lo[bone], L);
                                hi[bone] = glm::max(hi[bone], L);
                                any[bone] = true;
                            }
                            int wired = 0;
                            for (int k = 0; k < kScarfSegments; ++k) {
                                if (!any[k]) continue;
                                TrailChainProbe& pb = sm.scarf_par.probe[k][0];
                                pb.u_m = 0.5f * (lo[k].x + hi[k].x);
                                pb.v_m = 0.5f * (lo[k].y + hi[k].y);
                                pb.w_m = 0.5f * (lo[k].z + hi[k].z);
                                pb.hx_m = 0.5f * (hi[k].x - lo[k].x);
                                pb.hy_m = 0.5f * (hi[k].y - lo[k].y);
                                pb.hz_m = 0.5f * (hi[k].z - lo[k].z);
                                ++wired;
                            }
                            if (wired > 0)
                                sm.scarf_par.n_probe_stations = kScarfSegments;
                            // The short tail's own two pods, parked here until
                            // its params are built below (they are copied FROM
                            // scarf_par, so the long chain's table must be
                            // scrubbed out of the copy first).
                            for (int k = 0; k < kScarfShortSegs; ++k) {
                                const int b2 = kScarfSegments + k;
                                if (!any[b2]) continue;
                                TrailChainProbe& pb = sm.scarf_s_probe[k];
                                pb.u_m = 0.5f * (lo[b2].x + hi[b2].x);
                                pb.v_m = 0.5f * (lo[b2].y + hi[b2].y);
                                pb.w_m = 0.5f * (lo[b2].z + hi[b2].z);
                                pb.hx_m = 0.5f * (hi[b2].x - lo[b2].x);
                                pb.hy_m = 0.5f * (hi[b2].y - lo[b2].y);
                                pb.hz_m = 0.5f * (hi[b2].z - lo[b2].z);
                                sm.scarf_s_have_probe[k] = true;
                            }
                            TraceLog(LOG_INFO,
                                     "SCARF: pod boxes wired -- %d of %d long-"
                                     "chain stations (drawn fabric now graded "
                                     "against the coat, not the centreline)",
                                     wired, kScarfSegments);
                        }
                    }
                }
                if (s_max > -1.0e8f) {
                    sm.scarf_back_surface = s_max;
                    // With the banded planes the keep-out is measured OFF THE
                    // DRAWN SURFACE, and the roots are AUTHORED 30 mm off the
                    // back (R2c-7s(g)) -- no anchor stand-off.
                    // ★ SCARF-DRAPE: with pod boxes wired the box carries the
                    // fabric's own thickness, so the keep-out is the MARGIN
                    // alone -- margin + tube-half on top of a box that already
                    // spans the tube would rest the fabric 15 mm prouder than
                    // the art intends.
                    sm.scarf_par.back_keepout_m =
                        sm.back_n_rings >= 2
                            ? (sm.scarf_par.n_probe_stations > 0
                                   ? kScarfBackMarginM
                                   : kScarfBackMarginM + kScarfTubeHalfM)
                            : s_max + kScarfBackMarginM + kScarfTubeHalfM;
                    // ★ The floor the anchor-pin may relax the keep-out to.
                    // The chain is the ribbon's CENTRELINE, so a pin that
                    // relaxes to 0 puts the fabric's inner half inside the
                    // coat -- it grazes instead of resting on it. Its own
                    // half-thickness is the honest floor.
                    sm.scarf_par.back_keepout_min_m =
                        sm.scarf_par.n_probe_stations > 0 ? 0.002f
                                                          : kScarfTubeHalfM;
                    sm.scarf_anchor_standoff = 0.0f;
                } else {
                    TraceLog(LOG_WARNING,
                             "SCARF: no skinned torso vertices to measure the "
                             "back -- keep-out stays the typed default");
                }
            }
            // -- the short tail's two bones (optional: an older GLB has
            // none and the short tail simply rides rigid with the neck).
            {
                std::string smissing;
                for (int k = 0; k < kScarfShortSegs; ++k) {
                    char nm2[16];
                    std::snprintf(nm2, sizeof nm2, "scarf_s%02d", k + 1);
                    sm.scarf_s_node[k] = find_node(sm, nm2);
                    if (sm.scarf_s_node[k] < 0) smissing += std::string(nm2) + " ";
                }
                sm.scarf_s_ok = smissing.empty();
                if (sm.scarf_s_ok) {
                    const SNode& ss0 = sm.nodes[sm.scarf_s_node[0]];
                    sm.scarf_s_anchor_local = compose(ss0.t, ss0.r, ss0.s);
                    // the SAME run fraction as the long end: 2 x 0.068 =
                    // 0.136 m = exactly 1/3 of the long end's 0.408 (sixth
                    // ruling), with the same authored 0.080 bone spacing.
                    sm.scarf_s_seg_len =
                        glm::length(sm.nodes[sm.scarf_s_node[1]].t) *
                        kScarfRunSegFrac;
                    sm.scarf_s_par = sm.scarf_par;
                    sm.scarf_s_par.segments = kScarfShortSegs;
                    sm.scarf_s_par.seg_len_m = sm.scarf_s_seg_len;
                    sm.scarf_s_par.damping = kScarfShortDamping;
                    // ★ SCARF-DRAPE: the copy above carried the LONG chain's
                    // pod boxes -- six stations of the wrong geometry on a
                    // two-link chain. Scrub them and install the short tail's
                    // own, measured in the same bind pass.
                    sm.scarf_s_par.n_probe_stations = 0;
                    for (int k = 0; k <= kTrailChainMaxSegments; ++k)
                        for (int s2 = 0; s2 < 2; ++s2)
                            sm.scarf_s_par.probe[k][s2] = TrailChainProbe{};
                    {
                        int wired_s = 0;
                        for (int k = 0; k < kScarfShortSegs; ++k)
                            if (sm.scarf_s_have_probe[k]) {
                                sm.scarf_s_par.probe[k][0] =
                                    sm.scarf_s_probe[k];
                                ++wired_s;
                            }
                        if (wired_s > 0)
                            sm.scarf_s_par.n_probe_stations = kScarfShortSegs;
                    }
                    const glm::mat4& saw = sm.rest_world[sm.scarf_s_node[0]];
                    trail_chain_reset(
                        sm.scarf_s, sm.scarf_s_par, glm::vec3(saw[3]),
                        glm::normalize(glm::vec3(saw * glm::vec4(0, 1, 0, 0))));
                } else {
                    TraceLog(LOG_WARNING,
                             "SCARF: short-tail bones missing (%s) -- the "
                             "short tail rides rigid", smissing.c_str());
                }
            }
            sm.scarf_primed = true;
            TraceLog(LOG_INFO,
                     "SCARF: %d bones wired, run seg_len %.4f m, "
                     "back surface %.4f m behind the torso plane, keep-out "
                     "%.4f, anchor stand-off %.4f",
                     kScarfSegments, static_cast<double>(sm.scarf_seg_len),
                     static_cast<double>(sm.scarf_back_surface),
                     static_cast<double>(sm.scarf_par.back_keepout_m),
                     static_cast<double>(sm.scarf_anchor_standoff));
        }
    }

    sm.shader = LoadShaderFromMemory(kVS, kFS);
    sm.loc_color = GetShaderLocation(sm.shader, "uColor");
    sm.loc_sun = GetShaderLocation(sm.shader, "uSunDir");
    sm.loc_emit = GetShaderLocation(sm.shader, "uEmit");
    sm.loc_emit_col = GetShaderLocation(sm.shader, "uEmitColor");
    sm.loc_gloss = GetShaderLocation(sm.shader, "uGloss");

    sm.beam_shader = LoadShaderFromMemory(kBeamVS, kBeamFS);
    sm.beam_mesh = build_beam_hull();
    sm.beam_mat = LoadMaterialDefault();
    sm.beam_mat.shader = sm.beam_shader;
    // raylib multiplies by colDiffuse -- leave it WHITE or the beam is tinted
    sm.beam_mat.maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
    sm.lb_apex = GetShaderLocation(sm.beam_shader, "uApex");
    sm.lb_dir = GetShaderLocation(sm.beam_shader, "uDir");
    sm.lb_r0 = GetShaderLocation(sm.beam_shader, "uR0");
    sm.lb_spread = GetShaderLocation(sm.beam_shader, "uSpread");
    sm.lb_range = GetShaderLocation(sm.beam_shader, "uRange");
    sm.lb_intensity = GetShaderLocation(sm.beam_shader, "uIntensity");
    sm.lb_tint = GetShaderLocation(sm.beam_shader, "uTint");
    sm.lb_cross = GetShaderLocation(sm.beam_shader, "uCross");
    sm.beam_ok = sm.lb_apex >= 0 && find_node(sm, "headlight_lens") >= 0;
    if (sm.beam_ok) {
        // Measure the glass, don't trust the node origin (see lens_center).
        const int nlens = find_node(sm, "headlight_lens");
        glm::vec3 lo(1e9f), hi(-1e9f);
        for (const SPrim& sp : sm.prims) {
            if (sp.node != nlens || sp.mesh.vertices == nullptr) continue;
            for (int vi = 0; vi < sp.mesh.vertexCount; ++vi) {
                const glm::vec3 p(sp.mesh.vertices[vi * 3 + 0],
                                  sp.mesh.vertices[vi * 3 + 1],
                                  sp.mesh.vertices[vi * 3 + 2]);
                lo = glm::min(lo, p);
                hi = glm::max(hi, p);
            }
        }
        if (lo.x <= hi.x) sm.lens_center = (lo + hi) * 0.5f;
    }
    // ---- the tie-rod linkage anchors (see TieRodLink). The lug is measured
    // off the bellcrank's own mesh (its x-extreme vertex cluster per side);
    // the outer ball is the tierod_end node, already a child of the ski.
    {
        const int nbc = find_node(sm, "bellcrank");
        const Mesh* bc_mesh = nullptr;
        if (nbc >= 0)
            for (const SPrim& sp : sm.prims)
                if (sp.node == nbc && sp.mesh.vertices != nullptr) {
                    bc_mesh = &sp.mesh;
                    break;
                }
        for (int s = 0; s < 2; ++s) {
            const char* rod_nm = s == 0 ? "tie_rod_L" : "tie_rod_R";
            const char* end_nm = s == 0 ? "tierod_end_L" : "tierod_end_R";
            const int nrod = find_node(sm, rod_nm);
            const int nend = find_node(sm, end_nm);
            if (nrod < 0 || nend < 0 || nbc < 0 || bc_mesh == nullptr) continue;
            float ex = s == 0 ? 1e9f : -1e9f;
            for (int v = 0; v < bc_mesh->vertexCount; ++v) {
                const float x = bc_mesh->vertices[v * 3];
                ex = s == 0 ? std::min(ex, x) : std::max(ex, x);
            }
            glm::vec3 lug(0.0f);
            int nl = 0;
            for (int v = 0; v < bc_mesh->vertexCount; ++v)
                if (std::fabs(bc_mesh->vertices[v * 3] - ex) < 0.006f) {
                    lug += glm::vec3(bc_mesh->vertices[v * 3],
                                     bc_mesh->vertices[v * 3 + 1],
                                     bc_mesh->vertices[v * 3 + 2]);
                    ++nl;
                }
            if (nl == 0) continue;
            lug /= static_cast<float>(nl);
            const glm::mat4 rod_inv = glm::inverse(sm.rest_world[nrod]);
            auto& tl = sm.tie_link[s];
            tl.rod = nrod;
            tl.end = nend;
            tl.li = glm::vec3(rod_inv * sm.rest_world[nbc] *
                              glm::vec4(lug, 1.0f));
            tl.lo = glm::vec3(rod_inv *
                              glm::vec4(glm::vec3(sm.rest_world[nend][3]), 1.0f));
        }
    }
    sm.mat = LoadMaterialDefault();
    sm.mat.shader = sm.shader;
    TraceLog(LOG_INFO, "SLED: hero GLB wired (%d nodes, %d prims)",
             static_cast<int>(sm.nodes.size()),
             static_cast<int>(sm.prims.size()));
    return true;
}

glm::vec3 wpos(const glm::mat4& m) { return glm::vec3(m[3]); }

// Rest-pose captures for one 2-bone chain: segment lengths, the bend-plane
// normal that reproduces the AUTHORED elbow/knee direction, and per-bone
// twist offsets so that at zero channels the IK reproduces the rest pose
// EXACTLY (the seated pose Chad approved is the identity of this solver).
void capture_chain(const SledModel& sm, const int chain[3],
                   const glm::vec3& target0, glm::vec3* bend_n,
                   glm::mat3 tw[2], float len[2]) {
    const glm::vec3 s0 = wpos(sm.rest_world[chain[0]]);
    const glm::vec3 e0 = wpos(sm.rest_world[chain[1]]);
    len[0] = glm::length(sm.nodes[chain[1]].t);
    len[1] = glm::length(sm.nodes[chain[2]].t);
    // ★ DEFECT 14. The bend plane is now composed by the PURE, TESTED
    // render::rest_bend_normal (render/rider_pose.h): same primary branch
    // (n = st x (e0-s0), pointing toward the authored elbow/knee), but the
    // near-extended fallback is derived from the chain's own parent frame
    // instead of the world-space constant (0,-1,0.1) that could not mirror
    // between sides and could flip through the limb. On the shipped asset the
    // fallback is unreachable -- measured |st x (e0-s0)| is 0.109817 on both
    // arms and 0.123604 on both legs (★ R1c: re-taken post-§C; the leg figure
    // used to read 0.1155 here), against a 1e-4 threshold -- so this is a
    // robustness change, not a visual one.
    const glm::vec3 n = rest_bend_normal(s0, e0, target0,
                                         glm::mat3(sm.rest_world[chain[0]]));
    *bend_n = n;
    for (int k = 0; k < 2; ++k) {
        const glm::vec3 a = k == 0 ? s0 : e0;
        const glm::vec3 b = k == 0 ? e0 : target0;
        const glm::mat3 basis = bone_basis(a, b, n);
        tw[k] = glm::transpose(basis) * glm::mat3(sm.rest_world[chain[k]]);
    }
}

// ★ R2c-5. THE BAR AXIS, TAKEN FROM THE MACHINE. seat_sudburian.py builds it as
// `grip_socket_R - grip_socket_L` and the runtime must agree, so it is
// side1(model +X) - side0(model -X) -- pointing model +X, the rider's LEFT.
// It is read from whatever pose the transforms are in, which is the whole
// point: at rest for the sign capture, LIVE (steered) every frame after that.
glm::vec3 bar_axis(const glm::mat4& grip_side0, const glm::mat4& grip_side1) {
    return wpos(grip_side1) - wpos(grip_side0);
}

bool node_geom_centre_world(const SledModel& sm, int node, glm::vec3& out) {
    if (node < 0) return false;
    glm::vec3 lo(1e30f), hi(-1e30f);
    bool any = false;
    for (const SPrim& sp : sm.prims) {
        if (sp.node != node || sp.mesh.vertices == nullptr) continue;
        for (int v = 0; v < sp.mesh.vertexCount; ++v) {
            const glm::vec3 p(sp.mesh.vertices[3 * v + 0],
                              sp.mesh.vertices[3 * v + 1],
                              sp.mesh.vertices[3 * v + 2]);
            lo = glm::min(lo, p);
            hi = glm::max(hi, p);
            any = true;
        }
    }
    if (!any) return false;
    out = wpos(sm.rest_world[node] *
               glm::translate(glm::mat4(1.0f), (lo + hi) * 0.5f));
    return true;
}

void capture_rest_ik(SledModel& sm) {
    for (int s = 0; s < 2; ++s) {
        if (sm.arm[s][0] >= 0 && sm.n_grip[s] >= 0) {
            // arm target = the HAND's rest position (grips carry an offset)
            capture_chain(sm, sm.arm[s], wpos(sm.rest_world[sm.arm[s][2]]),
                          &sm.arm_bend_n[s], sm.arm_tw[s], sm.arm_len[s]);
            sm.hand_off[s] =
                socket_weld(sm.rest_world[sm.n_grip[s]],
                            sm.rest_world[sm.arm[s][2]], glm::vec3(0.0f));
        }
        // ★ DEFECTS 7 + 6, and they are ONE mechanism, not two. The leg copies
        // the arm's proven weld (socket_weld, render/rider_pose.h) so the boot
        // rides board_socket_L/R instead of a load-time constant; the defect-6
        // re-seat is a correction baked INTO that captured weld, so the sole
        // sits on the running-board deck rather than 66.8 mm through it.
        //
        // ★ THE CROSSOVER IS REAL NOW (§2.3a, 2026-08-18): the Sudburian's
        // `foot_l` (anatomical left, model +X) welds to `board_socket_R`. It is
        // written ONCE in rider_chain_specs() (`model_side`), and this loop
        // indexes by model side, so nothing here knows a name. kBootReseatM is
        // ZERO for this rider: the R2b seating IS the rest.
        if (sm.leg[s][0] >= 0 && sm.n_board[s] >= 0) {
            sm.foot_off[s] = socket_weld(sm.rest_world[sm.n_board[s]],
                                         sm.rest_world[sm.leg[s][2]],
                                         kBootReseatM);
            // Capture against the RE-SEATED target, not the authored one, so
            // the solver's twist offsets reproduce the pose the leg will
            // actually hold at rest and the knee keeps its authored side.
            capture_chain(sm, sm.leg[s],
                          wpos(sm.rest_world[sm.n_board[s]] * sm.foot_off[s]),
                          &sm.leg_bend_n[s], sm.leg_tw[s], sm.leg_len[s]);
        }
    }
    // ★ R2c-5. THE CONTROL-INPUT POSE'S THREE DERIVED FACTS, all read off the
    // rest pose, none of them retyped from the Blender script.
    //
    // (a) WHICH SIDE IS THE THROTTLE. Chad ruled the throttle is the RIGHT
    //     hand. The rider's right is model -X (the Sudburian's `hand_r` sits
    //     at x = -0.335, welded to the machine's `grip_socket_L`), so the
    //     throttle side is the side whose GRIP SOCKET sits at negative x --
    //     tested, the same way the suspension pair is ordered by position
    //     rather than by name. A rename cannot silently swap it and a mirrored
    //     re-export cannot either. The 2026-08-18 splice, which renamed every
    //     rider joint and crossed the L/R pairing, changed nothing here.
    if (sm.n_grip[0] >= 0 && sm.n_grip[1] >= 0) {
        sm.arm_throttle_side =
            wpos(sm.rest_world[sm.n_grip[0]]).x <
                    wpos(sm.rest_world[sm.n_grip[1]]).x
                ? 0
                : 1;
        const glm::vec3 bar = bar_axis(sm.rest_world[sm.n_grip[0]],
                                       sm.rest_world[sm.n_grip[1]]);
        for (int s = 0; s < 2; ++s) {
            if (sm.arm[s][0] < 0) continue;
            const glm::vec3 sh = wpos(sm.rest_world[sm.arm[s][0]]);
            const glm::vec3 el = wpos(sm.rest_world[sm.arm[s][1]]);
            const glm::vec3 wr = wpos(sm.rest_world[sm.arm[s][2]]);
            // (b) the pole sign that drops the elbow, and (c) the bar-roll sign
            //     that raises the wrist. Measured per side; they do NOT mirror.
            sm.arm_swing_down[s] = swing_down_sign(sh, el, wr - sh);
            sm.arm_wrist_up[s] =
                wrist_up_sign(wr, wpos(sm.rest_world[sm.n_grip[s]]), bar);
        }
        TraceLog(LOG_INFO,
                 "SLED: control pose -- throttle side %d (grip x %+.3f), "
                 "elbow-down signs %+.0f/%+.0f, wrist-up signs %+.0f/%+.0f",
                 sm.arm_throttle_side,
                 wpos(sm.rest_world[sm.n_grip[sm.arm_throttle_side]]).x,
                 sm.arm_swing_down[0], sm.arm_swing_down[1],
                 sm.arm_wrist_up[0], sm.arm_wrist_up[1]);

        // ★★ R3-HANDS. Each control bone measures its curl against the lever
        // it actually reaches for -- axis and direction derived here, never
        // typed, so a re-export that moves a lever moves the animation with it.
        sm.n_thumb_r = sm.rj[kRiderThumbR];
        sm.n_mittfront_l = sm.rj[kRiderMittFrontL];
        sm.n_throttle_block = find_node(sm, "throttle_block");
        sm.n_brake_lever = find_node(sm, "brake_lever");
        // ★ AGAINST THE LEVER'S GEOMETRY, NEVER ITS ORIGIN -- both origins are
        // the same centreline point. See node_geom_centre_world.
        glm::vec3 t_thr(0.0f), t_brk(0.0f);
        if (sm.n_thumb_r >= 0 &&
            node_geom_centre_world(sm, sm.n_throttle_block, t_thr))
            sm.thumb_aim = curl_aim(sm.rest_world[sm.n_thumb_r], t_thr);
        // ★★★ THE BRAKE FINGERS DO NOT AIM AT THE LEVER. Chad, 2026-08-24, on
        // seeing the first build: "fingers should be squeezing in, (swinging
        // their tips aft towards the rider themselves, the fulcrums being the
        // bases of the fingers themselves, the tips are supposed to swing
        // inward aft) currently they are lifting up to the sky".
        //
        // Aiming at the lever's centroid is EXACTLY what lifted them. The
        // lever assembly sits UP and FORWARD of the mitt's base (+20 mm y,
        // +49 mm z from it), so "toward the lever" is up-and-forward -- the
        // opposite of a squeeze. A finger closing on a lever does not travel
        // to the lever's centroid at all: it FLEXES about its own base and
        // carries its tip AFT, toward the palm and the rider. So that is what
        // it aims at, and "aft" is MEASURED off the machine -- grip socket to
        // the rider's own pelvis, levelled -- rather than typed as "model -Z",
        // so a re-export that flips the model flips this with it.
        //
        // ★ The thumb keeps the lever target: Chad signed it ("thumb for
        // throttle looks good"), and a thumb really does press toward the
        // paddle it sits under. The two controls are different motions and
        // only one of them is a reach.
        // ★★★ THE BRAKE FINGERS DO NOT AIM AT THE LEVER. Chad, 2026-08-24,
        // on seeing the first build: "fingers should be squeezing in ...
        // currently they are lifting up to the sky".
        //
        // Aiming at the lever's centroid is what lifted them: the lever sits
        // UP and FORWARD of the mitt's base, so "toward the lever" is
        // up-and-forward, the opposite of a squeeze. The fingers aim toward
        // the RIDER instead, and that direction is measured off the machine
        // rather than typed, so a re-export that flips the model flips it too.
        //
        // ★ The thumb keeps the lever target: Chad signed it ("thumb for
        // throttle looks good"), and a thumb really does press toward the
        // paddle it sits under.
        const int bs = sm.arm_throttle_side == 0 ? 1 : 0;  // the brake side
        if (sm.n_mittfront_l >= 0 && sm.n_pelvis >= 0 && sm.n_grip[bs] >= 0) {
            glm::vec3 aft = wpos(sm.rest_world[sm.n_pelvis]) -
                            wpos(sm.rest_world[sm.n_grip[bs]]);
            aft.y = 0.0f;  // a squeeze travels level, not down into the deck
            const float l = glm::length(aft);
            if (l > 1e-6f) {
                t_brk = wpos(sm.rest_world[sm.n_mittfront_l]) + aft / l * 0.2f;
                sm.mitt_aim = curl_aim(sm.rest_world[sm.n_mittfront_l], t_brk);
            }
        }

        // ★★ R3-HANDS: THE TWO LEVERS THEMSELVES. Nothing in this file drove
        // either one, so both were DEAD while Chad squeezed them -- and the
        // brake blade moving IS the mechanism he described ("they will press
        // on the brake lever and in turn, squeeze it into the handlebar").
        //
        // Each turns about a pin at its INBOARD end -- the end that mounts --
        // sitting in the AFT-FACE plane (min z, model +Z being forward), which
        // is Chad's own R2c-M2 ruling for this hardware. Inboard is derived
        // from the geometry's own sign, so the two sides need no separate line.
        for (SPrim& sp : sm.prims) {
            const bool brk = sp.node == sm.n_brake_lever;
            const bool thr = sp.node == sm.n_throttle_block;
            if (!sp.is_lever || (!brk && !thr)) continue;
            glm::vec3 lo(1e30f), hi(-1e30f);
            for (int v = 0; v < sp.mesh.vertexCount; ++v) {
                const glm::vec3 p(sp.mesh.vertices[3 * v + 0],
                                  sp.mesh.vertices[3 * v + 1],
                                  sp.mesh.vertices[3 * v + 2]);
                lo = glm::min(lo, p);
                hi = glm::max(hi, p);
            }
            const float inb = std::fabs(lo.x) < std::fabs(hi.x) ? lo.x : hi.x;
            sp.pivot = glm::vec3(inb, (lo.y + hi.y) * 0.5f, lo.z);
            sp.axis = glm::vec3(0.0f, 1.0f, 0.0f);  // the vertical pin
            TraceLog(LOG_INFO,
                     "SLED: R3-HANDS -- %s lever pivot (%+.4f %+.4f %+.4f) "
                     "arm %.1f mm",
                     brk ? "BRAKE" : "THROTTLE", sp.pivot.x, sp.pivot.y,
                     sp.pivot.z, (hi.x - lo.x) * 1000.0f);
        }

        TraceLog(LOG_INFO,
                 "SLED: R3-HANDS -- thumb aim axis %d sign %+.0f at lever "
                 "(%+.3f %+.3f %+.3f) d %.3f m; mitt aim axis %d sign %+.0f "
                 "at lever (%+.3f %+.3f %+.3f) d %.3f m",
                 sm.thumb_aim.axis, static_cast<double>(sm.thumb_aim.sign),
                 t_thr.x, t_thr.y, t_thr.z,
                 sm.n_thumb_r >= 0
                     ? glm::length(t_thr - wpos(sm.rest_world[sm.n_thumb_r]))
                     : -1.0f,
                 sm.mitt_aim.axis, static_cast<double>(sm.mitt_aim.sign),
                 t_brk.x, t_brk.y, t_brk.z,
                 sm.n_mittfront_l >= 0
                     ? glm::length(t_brk -
                                   wpos(sm.rest_world[sm.n_mittfront_l]))
                     : -1.0f);
    }
}

// 2-bone IK: write world matrices for the chain's upper+lower bones so the
// lower bone's TIP lands on `target`; roll from the captured rest twist.
void solve_chain(SledModel& sm, const int chain[3], const glm::vec3& target,
                 const glm::vec3& bend_n, const glm::mat3 tw[2],
                 const float len[2]) {
    const glm::vec3 s = wpos(sm.nodes[chain[0]].world);
    glm::vec3 st = target - s;
    float d = glm::length(st);
    const float dmin = std::fabs(len[0] - len[1]) + 1e-3f;
    const float dmax = len[0] + len[1] - 1e-3f;
    d = d < dmin ? dmin : (d > dmax ? dmax : d);
    const glm::vec3 dir = glm::normalize(st);
    const float a =
        (len[0] * len[0] + d * d - len[1] * len[1]) / (2.0f * d);
    const float h2 = len[0] * len[0] - a * a;
    const float h = h2 > 0.0f ? std::sqrt(h2) : 0.0f;
    // bend direction: perpendicular to the chain inside the rest bend plane
    glm::vec3 bend = glm::cross(bend_n, dir);
    const float bl = glm::length(bend);
    bend = bl > 1e-5f ? bend / bl : glm::vec3(0.0f, 0.0f, 1.0f);
    // keep the elbow/knee on its authored side of the chain
    const glm::vec3 e = s + dir * a + bend * h;
    const glm::vec3 tgt = s + dir * d;
    for (int k = 0; k < 2; ++k) {
        const glm::vec3 fa = k == 0 ? s : e;
        const glm::vec3 fb = k == 0 ? e : tgt;
        const glm::mat3 rot = bone_basis(fa, fb, bend_n) * tw[k];
        glm::mat4 w{rot};
        w[3] = glm::vec4(fa, 1.0f);
        sm.nodes[chain[k]].world = w;
        sm.nodes[chain[k]].world_override = true;
    }
}

// ★ §D. ONE PER-FRAME POSE PASS: channel locals, world composition, the four
// 2-bone IK chains and the settle sweep. It was inline in sled_model_draw until
// R1a attempt 2; the hinge solve has to evaluate the posed CG, so the pass had
// to become callable more than once per frame. Everything in it is unchanged
// except that fore-aft lean arrives as `root_fwd` (solved) instead of
// `rig.lean_fwd_m` (raw), and `hinge_theta` rotates the pelvis.
//
// It writes ONLY into the per-frame scratch (`local`, `world`, `world_override`)
// and reads only the rest TRS plus its arguments, so calling it N times is
// idempotent in N -- there is no accumulator here and §0.1 is untouched.
//
// ★ R1c: the VERTICAL channel now arrives the same way as the fore-aft one, as
// `root_up` (solved) instead of `rig.lean_up_m` (raw). That single change is
// what turns the pelvis into a solved degree of freedom: raising `root` lifts
// the thigh roots with the torso while the boots stay welded to
// `board_socket_L/R`, so the knees FOLD and the man comes up off the seat
// instead of hinging about it. The LATERAL channel is deliberately still raw --
// out of scope for R1c, and its lie is reported, not fixed.
// ★★★ ST-5 THE HAND HOOK'S ONE TRANSFORM: ABSOLUTE WORLD -> the rig's MODEL
// space, for the draw in progress. SledRig::hand_grip names its targets in
// absolute world -- the space render/draw.cpp solves the launcher in, and the
// space RiderBack publishes back into -- while pose_pass works entirely in
// model space, so exactly one conversion is needed and this is where it is
// kept. Written from the SAME (pos, basis, cg_h, sag0) `mount` is built from
// and BEFORE the pose runs, so it is never a solve stale.
//
// ⚠ It is READ only on a frame whose rig carries a non-zero grip weight. The
// reference and weight-shift PROBE passes build their own all-zero SledRigs,
// so no probe can reach it, and neither can the load-time capture passes that
// run before any draw has set it.
glm::dmat4 g_w2m{1.0};

void pose_pass(SledModel& sm, const SledRig& rig, float sag0, float hinge_theta,
               float root_fwd, float root_up, const WsState& ws) {
    // steer: +1 = LEFT (kernel). Positive rotation about model +Y takes the
    // nose (+Z) toward +X = the rider's left. Bars swing on the raked
    // steering-head empty (its rest rotation IS the rake, so a local +Y spin
    // is a spin about the post); skis swing on their own origins (kingpins).
    // ★ DEFECT 10, the ski-angle lie. The skis now swing to the KERNEL's own
    // steer_max_rad (0.42 rad = 24.06 deg) instead of a retyped 25 deg, which
    // over-rotated the drawn ski against the simulated one by 3.9 %. The bar
    // sweep stays 32 deg and is marked UNSOURCED at its definition -- it has no
    // kernel owner and inventing one would be a fork, not a fix.
    const glm::quat q_steer =
        glm::angleAxis(rig.steer * kBarSweepRad, glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::quat q_ski = glm::angleAxis(rig.steer * ski_steer_max_rad(),
                                           glm::vec3(0.0f, 1.0f, 0.0f));
    // ★ §D. The pelvis hinge, forward positive, about the model +X axis through
    // the pelvis origin. PRE-multiplied, not post-: `root`'s rest rotation is
    // the identity with unit scale (measured off the shipped file), so the
    // pelvis's PARENT frame IS the model frame and a pre-multiply rotates about
    // model +X. Post-multiplying would rotate about the pelvis's own local axis,
    // which is tilted 11.3 deg, and the hinge would not be a hip hinge.
    // ★ ... and, since the Sudburian, conjugated into the pelvis's PARENT
    // frame (root's rest world rotation), so it is still a rotation about
    // MODEL +X through the pelvis origin whatever the rig's rest frames are.
    // On the legacy rig root_q was the identity and this was the bare quat.
    //
    // ★★★ R3-WS. The REACH hinge rides the SAME axis. It is a second, solved
    // trunk flexion whose job is to keep the hands on the bars while the pelvis
    // retreats -- and because it is the same rotation about model +X through
    // the pelvis, the "back bent over, weight low and rearward" crouch Chad
    // asked for is EMERGENT (SPEC 3a): there is no separate crouch dial.
    // `ws.theta_r` is exactly 0 for every rider_fwd_m >= 0.
    const glm::quat q_hinge_model = glm::angleAxis(
        hinge_theta + (ws.active ? ws.theta_r : 0.0f),
        glm::vec3(1.0f, 0.0f, 0.0f));
    const glm::quat q_hinge =
        glm::conjugate(sm.root_q) * q_hinge_model * sm.root_q;
    // rider lean: kernel +LEFT/+up = model +X/+Y (coordinate law). Fore-aft and
    // (R1c) vertical are no longer read raw off the rig -- see §D and §E;
    // `root_fwd` / `root_up` are the SOLVED residual translation and the rest of
    // each demand is carried by q_hinge. Lateral is still raw (out of scope).
    // ★★★ R3-WS: the SEAT SLIDE rides in on the same model-frame demand
    // vector -- the butt goes back along z and follows the measured seat top
    // along y. Both terms are identically 0 when the ladder is inactive.
    // ★★★ ST-5 THE UN-HUNCH'S ONE INPUT, and it is not a new one. `sit` is
    // the larger of the two hand-grip weights the ST-5 hook already carries,
    // so the trunk comes up on exactly the schedule the hands leave the bars
    // on (right at t 0.25-0.50 of the eased blend, left at 0.55-0.80) and
    // goes back down on exactly the frame the launcher is cut. NO NEW STATE,
    // no second timer to disagree with the first, and -- because every
    // capture and probe pass in this file builds an all-zero SledRig -- it is
    // provably 0 everywhere except a frame that is actually drawing a man
    // holding a launcher.
    const float sit = std::fmax(
        0.0f, std::fmin(1.0f, std::fmax(rig.hand_grip[0].weight,
                                        rig.hand_grip[1].weight)));
    const glm::vec3 lean{rig.lean_lat_m,
                         root_up + (ws.active ? ws.dy_m : 0.0f),
                         root_fwd - (ws.active ? ws.aft_m : 0.0f)};

    for (const int i : sm.order) {
        SNode& nd = sm.nodes[i];
        nd.world_override = false;
        glm::vec3 t = nd.t;
        glm::quat r = nd.r;
        for (int k = 0; k < 3; ++k)
            if (i == sm.n_susp[k])
                t.y += rig.susp_m[k] - sag0;  // compression pulls the
                                              // unsprung parts UP toward the
                                              // chassis; droop is emergent
        if (i == sm.n_ski[0] || i == sm.n_ski[1]) r = r * q_ski;
        if (i == sm.n_steer) r = r * q_steer;
        if (i == sm.n_root) {
            // ★ §A / F3. The bump-soak crouch is SPLIT: kAbsorbRootFrac of it
            // lands here, where it takes the thigh roots down with the torso so
            // the boots (welded rigid to board_socket by D6/D7) fold the knees.
            // The remainder stays at the pelvis. Applying all of it at the
            // pelvis is F3 -- the torso telescoping into frozen hips, which is
            // the one defect Chad saw by eye.
            //
            // Both demands are MODEL-frame vectors; root.t lives in root's
            // PARENT frame, so they go through the captured rest rotation.
            glm::vec3 demand_model =
                lean - glm::vec3(0.0f, absorb_root_drop_m(rig.absorb), 0.0f);
            // ★★★ R4a §7.3 STAGE 4: AND IF HE HAS LET GO, THE WHOLE MAN GOES
            // WITH IT. `flight_off_model` is a MODEL-frame vector like its two
            // neighbours here, so the departure rides the same channel the lean
            // and the crouch already do -- there is no second way to move him.
            //
            // ⚠ GATED ON THE RIG, NOT ON `sm`, AND THAT IS LOAD-BEARING: this
            // function is also run with PROBE rigs for the reference solves
            // (`pose_and_solve_lean`), and those default `grip_attached` true.
            // A reference pose solved with the man already thrown off would be
            // a reference for a machine nobody is riding.
            if (!rig.grip_attached) demand_model += sm.flight_off_model;
            t += glm::transpose(sm.root_parent_R) * demand_model;
            // ★★★ R4c: AND HE STANDS IN HIS OWN FRAME, NOT THE MACHINE'S.
            // In the rest pose his up IS model +Y and his forward IS model +Z,
            // so a rotation that carries those two onto HIS up and HIS heading
            // is exactly the orientation he should be drawn at -- composed on
            // top of the rest pose rather than replacing it, so every hinge,
            // crouch and lean below still means what it meant.
            //
            // ⚠ IT ROTATES HIM ABOUT THE ROOT, WHICH IS THE GROUND BONE AT HIS
            // FEET, not about his hips -- so he pivots the way a man standing
            // does. And it is conjugated by the ROOT'S PARENT's rest rotation,
            // not by `root_q`: a node's local rotation lives in its parent's
            // frame (the pelvis uses `root_q` because ITS parent is the root).
            if (!rig.grip_attached) {
                // ⚠ CONJUGATED BY THE ROOT'S PARENT'S rest rotation, not by
                // `root_q`: a node's local rotation lives in its PARENT's
                // frame. (The pelvis below uses `root_q` because ITS parent is
                // the root -- the two are different frames and swapping them
                // is a bug that looks like a mystery twist.)
                // ⚠ NORMALISED FIRST, like `root_q` at load. Its counterpart
                // 760 lines up says "normalising columns is what makes that a
                // fact and not a hope" and then does it; this call was fed the
                // raw matrix, so any scale on the root's parent would have
                // produced a silently wrong rotation rather than a failure.
                // Same guard, both places. Red-team, 2026-09-01.
                glm::mat3 par_n = sm.root_parent_R;
                for (int c = 0; c < 3; ++c) {
                    const float L = glm::length(par_n[c]);
                    if (L > 1.0e-8f) par_n[c] /= L;
                }
                const glm::quat q_par = glm::quat_cast(par_n);
                r = (glm::conjugate(q_par) * sm.flight_q * q_par) * r;
            }
        }
        // ★★★ G2g "STAND STRAIGHT WHEN HE WALKS" (Chad: "he is still
        // hunching over his back like he is on the sled"), G2h "torso needs
        // to straighten back a little and head is now too tilted back it
        // needs to be straight up and down while walking". The measured
        // upright corrections (load-time, see gait_straight_q's banner)
        // slerp into the three spine joints, the neck and the head ONLY
        // while he walks -- same sacred gate as the pelvis terms below
        // (`gait_on && !grip_attached`, the signed riding pose cannot move)
        // -- and they EASE IN with the same get-up pitch the whole body
        // already uses, so a man rising from his knees uncurls instead of
        // snapping upright.
        //
        // ⚠ G3, READ THIS BEFORE YOU TOUCH spine_01..03: this is the POSTURE
        // owner of the spine channel. Counter-yaw is a DIFFERENT term on the
        // same joints and it composes HERE, multiplied onto `r` beside this
        // one (posture first, swing on top) -- one owner per joint per term,
        // never a second straightening and never a replacement.
        if (sm.gait_on && !rig.grip_attached) {
            // The get-up ease, shared by both terms below so the back and the
            // head uncurl together rather than at two different rates.
            const float rise_ease = std::clamp(
                1.0f - sm.flight_pitch_rad / 0.96f, 0.0f, 1.0f);
            if (i == sm.rj[kRiderSpine1] || i == sm.rj[kRiderSpine2] ||
                i == sm.rj[kRiderSpine3]) {
                static const float straight_scale = [] {
                    const char* e = std::getenv("SEADS_GAIT_STRAIGHT");
                    return e != nullptr ? static_cast<float>(std::atof(e))
                                        : 1.0f;
                }();
                const float amt = straight_scale * rise_ease;
                if (amt > 1.0e-4f) {
                    const int k = (i == sm.rj[kRiderSpine1]) ? 0
                                  : (i == sm.rj[kRiderSpine2]) ? 1
                                                               : 2;
                    r = glm::slerp(glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                                   sm.gait_straight_q[k], amt) *
                        r;
                }
            }
            // ★★★ G2h, THE HEAD. A slice of the ladder's G7 (head
            // stabilisation) taken early because G2g's own fix created the
            // need for it: the spine correction rotates the neck and head
            // rigidly with spine_03, so without this the head pitches BACK by
            // the entire straightening angle. Two joints, measured against
            // the accumulated spine correction (see the capture): the neck
            // segment stands up, then the skull axis stands up on top of it.
            // ⚠ PRE-multiplied, so the head's own cam yaw/pitch channel a few
            // lines below (which POST-multiplies, i.e. turns the head in its
            // OWN frame) still means "look where he is looking" -- his gaze
            // rides the plumb head instead of fighting it.
            if (i == sm.n_neck || i == sm.n_head) {
                static const float head_scale = [] {
                    const char* e = std::getenv("SEADS_GAIT_HEAD");
                    return e != nullptr ? static_cast<float>(std::atof(e))
                                        : 1.0f;
                }();
                const float amt = head_scale * rise_ease;
                if (amt > 1.0e-4f) {
                    const int k = (i == sm.n_neck) ? 0 : 1;
                    r = glm::slerp(glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                                   sm.gait_head_q[k], amt) *
                        r;
                }
            }
        }
        if (i == sm.n_pelvis) {
            // ★ DEFECT 8: `absorb` is no longer hard-coded 0. It is composed by
            // render::rider_absorb() from SledState::susp_x + susp_v (see
            // render/rider_pose.h) and handed in on SledRig, so this crouch is
            // now a real terrain-reaction channel. pelvis.t lives in root's
            // frame, hence the same conjugation as the hinge.
            t += glm::transpose(sm.root_R) *
                 glm::vec3(0.0f, -absorb_pelvis_drop_m(rig.absorb), 0.0f);

            // ★★★ GAIT LADDER G2 -- THE PELVIS LIVES (plan §4 G2). Gated on
            // `sm.gait_on && !rig.grip_attached` -- the equivalent of the
            // `welded` flag, not yet declared this early in the function --
            // so the SIGNED riding pose cannot move a hair for these terms;
            // that gate is sacred.
            //
            // ★★★ WHY REST-FRAME AXES, NOT `flight_up_model` /
            // `flight_fwd_model`: this term is inserted the SAME way the
            // hinge just above is -- pre-multiplied into `pelvis.local` via
            // `transpose(root_R) * v` (translation) or
            // `conjugate(root_q) * q * root_q` (rotation) -- and that
            // machinery composes UNDER `root.world`, which already carries
            // `flight_q` whenever the root channel is active (the
            // `!rig.grip_attached` branch a few dozen lines up). Working the
            // composition through (root.world = flight_q * root_q; a term
            // inserted this way collapses to an ambient effect of exactly
            // `flight_q * term_rest` for a translation, or
            // `flight_q * term_rest * conjugate(flight_q)` for a rotation --
            // i.e. `flight_q` maps a REST-frame axis onto the man's actual
            // one, automatically, for free) shows `flight_q` is applied
            // ONCE by the hierarchy itself. Building the term out of the
            // ALREADY-CURRENT `flight_up_model` / `flight_fwd_model` would
            // apply `flight_q` a SECOND time -- the exact "rotates by his
            // heading twice" bug this file's own foot-pitch-axis banner
            // (a few hundred lines down) already paid for once. So this
            // term is built on the REST-frame axes instead -- model +Y is
            // rest up, +Z is rest forward, +X is rest left
            // (`cross(rest_up, rest_fwd)`) -- and the hierarchy's own
            // `flight_q` carries them to the man's ACTUAL up / forward /
            // left for free, exactly the way the absorb term two lines up
            // already relies on (it is only ever live while seated, where
            // `flight_q` is the identity, so this cancellation was never
            // visible until gait needed it too).
            if (sm.gait_on && !rig.grip_attached) {
                // ★ L-ONE-DIAL: bob and sway each get their OWN env scale
                // for Chad's A/B, default 1.0 -- the pelvis drop itself is
                // NOT a look dial (it is the physical fix for the reach
                // clamp) and is deliberately not exposed here.
                static const float bob_scale = [] {
                    const char* e = std::getenv("SEADS_GAIT_BOB");
                    return e != nullptr ? static_cast<float>(std::atof(e))
                                        : 1.0f;
                }();
                static const float sway_scale = [] {
                    const char* e = std::getenv("SEADS_GAIT_SWAY");
                    return e != nullptr ? static_cast<float>(std::atof(e))
                                        : 1.0f;
                }();
                // Vertical: the drop LOWERS (a minus along rest-up), the
                // bob rides on top of it (+ is a rise, matching
                // sim/gait.h's own sign convention). Lateral: the sway,
                // along rest-left (model +X).
                const float vertical_m =
                    sm.gait_bob_m * bob_scale - sm.gait_pelvis_drop_m;
                const float lateral_m = sm.gait_sway_left_m * sway_scale;
                t += glm::transpose(sm.root_R) *
                     glm::vec3(lateral_m, vertical_m, 0.0f);
                // Trendelenburg: a rotation about rest-forward (model +Z),
                // carried to the man's ACTUAL forward the same way the
                // hinge's model +X is carried to the pelvis's actual flex
                // axis, immediately below.
                const glm::quat q_roll_model = glm::angleAxis(
                    sm.gait_roll_rad, glm::vec3(0.0f, 0.0f, 1.0f));
                const glm::quat q_roll =
                    glm::conjugate(sm.root_q) * q_roll_model * sm.root_q;
                r = q_roll * r;
            }

            r = q_hinge * r;  // ★ §D, and PRE-multiplied on purpose
            // the DRIVE-2 stand hinge that used to be here is defect 9, deleted
        }
        // ★★★ ST-5 THE UN-HUNCH -- "he should sit up to aim".
        //
        // THE TRUNK, AND ONLY THE TRUNK. The hunch could be taken out at the
        // pelvis in one line, and that would be wrong twice over: the pelvis
        // carries the thigh roots, so rotating it would swing his legs out
        // from under the tunnel, and it is the joint the R3-WS ladder and the
        // §D hinge both solve through -- their math is not this rung's to
        // touch. Rotating the SPINE chain instead pivots the trunk about the
        // hips with the pelvis, the seat contact and both legs untouched,
        // which is the anatomy of a seated man straightening anyway.
        //
        // ⚠ CONJUGATED INTO EACH JOINT'S PARENT REST FRAME, so the angle
        // means model +X at every rung of the chain (a node's local rotation
        // lives in its PARENT's frame -- the lesson root_q vs the root's
        // parent paid for 200 lines up). And it is PRE-multiplied, like the
        // hinge: the joint's own rest rotation still means what it meant, and
        // the whole chain above -- neck, head, both clavicles, both shoulders
        // -- rides it rigidly because that is what a hierarchy is for.
        //
        // ⚠ ORDER IS LOAD-BEARING AND IT IS ALREADY RIGHT. This runs inside
        // the node loop, so every world matrix above the spine is rebuilt
        // BEFORE the arm IK below reads `sm.nodes[sm.arm[s][0]].world` for
        // the shoulder. The arms therefore solve to the launcher FROM the
        // lifted shoulders and follow the trunk for free; nothing has to be
        // told the order, it is the order.
        //
        // ⚠ AND THE NECK MOVES, WHICH IS THE POINT AND NOT A BUG. The ST-5
        // launcher anchors on `sled_model_rider_back`'s neck, published AFTER
        // this pose, so the launcher chases the lifted shoulder by one frame.
        // At 60 Hz over a 3 s draw that is a lag of a millimetre or two on a
        // pose that is itself easing -- it reads as the stock settling into
        // the shoulder. Fighting it would mean a second, predictive copy of
        // this rotation in draw.cpp, i.e. two opinions about one pose.
        if (sit > 0.0f && sm.sit_hunch_rad != 0.0f) {
            for (int k = 0; k < 3; ++k) {
                if (i != sm.sit_spine[k]) continue;
                const float a =
                    render::sting::sit_up_rad(sm.sit_hunch_rad, sit, k);
                if (a != 0.0f) {
                    const glm::quat q_sit =
                        glm::angleAxis(a, glm::vec3(1.0f, 0.0f, 0.0f));
                    r = (glm::conjugate(sm.sit_parent_q[k]) * q_sit *
                         sm.sit_parent_q[k]) *
                        r;
                }
                break;
            }
        }
        // ★★★ ST-5 THE SWEEP -- "they shall twist head and torso in addition
        // to the arms" (Chad, 2026-09-05).
        //
        // The arms already followed: their IK targets are the launcher's own
        // grips, so swinging the aim dragged the hands with it. What did NOT
        // follow was everything the arms hang off -- he tracked a target 90
        // degrees off the nose with his shoulders still square to the
        // windshield and his elbows wrenched across a frozen chest. This is
        // the trunk catching up.
        //
        // ⚠ ABOUT MODEL +Y (up), AND AFTER THE SIT-UP AT EACH JOINT. It is
        // pre-multiplied like the sit-up above, so at every rung the twist is
        // composed OUTSIDE the straightening -- he sits up, then turns -- and
        // both still mean model axes because both are conjugated into the
        // SAME captured parent rest rotation. The head takes a share of the
        // ELEVATION too, about model +X: he looks up the sight line.
        //
        // ⚠ SCALED BY `sit`, WHICH IS THE HAND-GRIP WEIGHT AND NOTHING NEW.
        // Stowed, the whole block is multiplied by zero; the stance-loss CUT
        // unwinds him in the same one frame it puts his hands back on the
        // bars, because there is no second piece of state here to catch up.
        //
        // ⚠ AND IT IS INSIDE THE NODE LOOP, WHICH IS WHAT MAKES THE ARMS
        // FREE. Every world matrix from spine_01 up is rebuilt here, BEFORE
        // the arm IK below reads `sm.nodes[sm.arm[s][0]].world` for the
        // shoulder -- so the arm chain roots off the TWISTED torso and the
        // elbows solve to a launcher his chest is already facing.
        if (sit > 0.0f &&
            (rig.sting_aim_az != 0.0f || rig.sting_aim_el != 0.0f)) {
            for (int k = 0; k < render::sting::kTwistCount; ++k) {
                if (i != sm.sit_spine[k]) continue;
                const float ty =
                    render::sting::twist_rad(rig.sting_aim_az, sit, k);
                const float tx =
                    k == render::sting::kTwHead
                        ? render::sting::head_el_rad(rig.sting_aim_el, sit)
                        : 0.0f;
                if (ty != 0.0f || tx != 0.0f) {
                    const glm::quat q_tw =
                        glm::angleAxis(ty, glm::vec3(0.0f, 1.0f, 0.0f)) *
                        glm::angleAxis(tx, glm::vec3(1.0f, 0.0f, 0.0f));
                    r = (glm::conjugate(sm.sit_parent_q[k]) * q_tw *
                         sm.sit_parent_q[k]) *
                        r;
                }
                break;
            }
        }
        if (i == sm.n_neck && ws.active && ws.neck_rad > 0.0f) {
            // ★★★ R3-WS(c) / SPEC 11.2.4. "his head comes up". The trunk
            // pitches ws.theta_r forward about model +X through the pelvis and
            // the neck rides it rigidly, so the counter is the same rotation
            // the other way, capped at kNeckCounterMaxRad -- expressed in the
            // neck's PARENT frame by the pure TU, exactly the way q_hinge is
            // conjugated into root's. The head's own cam channels are applied
            // on the CHILD node below, so they compose after this and are
            // untouched. Identically the identity for every rider_fwd_m >= 0.
            r = ws_neck_counter(ws.neck_rad, sm.ws_neck_up_sign,
                                sm.ws_neck_parent_q) *
                r;
        }
        if (i == sm.n_head) {
            // ★ DEFECT 11: the bare +-1.4 / +-0.9 clamps are named (and their
            // provenance stated) at render::kHeadYawMaxRad / kHeadPitchMaxRad.
            // Values unchanged.
            // ★★★ ST-5 THE SWEEP: A MAN LOOKS THROUGH ONE THING AT A TIME.
            // This channel is "head follows the chase camera" (§9d, camera
            // owns look). While he is on the sight it is the WRONG master --
            // the orbit is where the PLAYER is standing to watch, and his
            // eyes belong on the aim, which the twist block above already
            // put them on. So the camera's claim on his neck fades out on
            // exactly the schedule the launcher comes up, and the two never
            // stack into a doubled 160 degree neck. `sit` is 0 whenever the
            // launcher is stowed, so this is the identity for every frame
            // drawn before this rung and every frame with it away.
            const float cam_w = 1.0f - sit;
            const float yaw =
                std::fmax(-kHeadYawMaxRad,
                          std::fmin(kHeadYawMaxRad, rig.cam_yaw * cam_w));
            const float pit = std::fmax(
                -kHeadPitchMaxRad,
                std::fmin(kHeadPitchMaxRad, rig.cam_pitch * cam_w));
            r = r * glm::angleAxis(yaw, glm::vec3(0.0f, 1.0f, 0.0f)) *
                glm::angleAxis(pit, glm::vec3(1.0f, 0.0f, 0.0f));
        }
        nd.local = compose(t, r, nd.s);
        nd.world =
            nd.parent < 0 ? nd.local : sm.nodes[nd.parent].world * nd.local;
    }

    // ---- IK: hands welded to the (steering) grips, boots planted on the
    // boards -- the constraints the .blend solved live, re-solved here.
    // ★ R2c-5. The bar axis for the wrist roll, taken LIVE off the two grip
    // sockets -- so it steers with the bars, exactly as seat_sudburian.py takes
    // it live off grip_socket_R/L. A rest-space axis would be 58 deg off.
    const glm::vec3 bar_w =
        sm.n_grip[0] >= 0 && sm.n_grip[1] >= 0
            ? bar_axis(sm.nodes[sm.n_grip[0]].world, sm.nodes[sm.n_grip[1]].world)
            : glm::vec3(0.0f);
    // ★★★ R4a §7.3 STAGE 4 -- AND THIS IS THE LINE THAT ACTUALLY LETS GO.
    // Everything else in this rung moves a man; these two welds are what was
    // HOLDING him. The hands are IK-solved onto `grip_socket_L/R` and the boots
    // onto `board_socket_L/R` -- the constraints the .blend solved live -- and
    // while they stand, no translation of the root can take him off the
    // machine: the arms would simply stretch back to the bars.
    //
    // ⚠ THE ORDER IS §7.3's AND IT IS ALREADY RIGHT: the boots leave at STAGE 2
    // (`body_blend`, the leg release the chain drives) and these welds are
    // stage 4. What this flag does is make stage 4 possible at all.
    const bool welded = rig.grip_attached;
    for (int s = 0; s < 2; ++s) {
        // ★★★ R4c-2: HE IS NOT HOLDING HANDLEBARS ANY MORE. Chad: "Sudburian
        // should spread arms for balance." Until now the arm IK was gated on
        // `welded` and NOTHING wrote the arms while he walked -- so he waded
        // through the snow still gripping a bar that is thirty metres away, in
        // the seated R2c-5 pose. It was the loudest remaining tell.
        //
        // ⚠ STRICTLY ADDITIVE: this branch is entered ONLY when he is off the
        // machine, so it cannot move one frame of the signed riding pose.
        // ★★★ GAIT LADDER G2j -- THE PUMP-REPAIR WORK POSE. Chad: "make him
        // look like he is working with his hands in front... a holding still
        // out front with his left hand and a torquing of a wrench in another,
        // then a hammer fist with the right as the settling of the machine
        // blow."
        //
        // ⚠ ONE OWNER PER LIMB PER FRAME. The walking-arm branch below is
        // gated OFF while this one runs (`!work_now` on its condition), so the
        // two never both solve the same chain -- the second solve would simply
        // win and the first would be dead code that looked alive.
        // ⚠ AND NO `continue` AT THE END OF THIS BLOCK. G1e's bug (this file's
        // own banner, a few dozen lines down) was exactly that: a `continue`
        // in this per-side loop silently skipped the LEG block for that side.
        // The legs must keep solving -- he is STANDING at the pump, and a man
        // whose legs stopped being posed sinks into the rig's rest pose.
        // ⚠ THE BLOW OUTLIVES `work_on` BY DESIGN: `FinishRepair` is the
        // transition OUT of Repairing, so gating the hammer on `work_on`
        // alone would be a hammer nobody ever sees swing (sim/gait.h).
        const bool work_now =
            !welded && (rig.work_on || rig.work_blow_on);
        if (work_now && sm.arm[s][0] >= 0) {
            const glm::vec3 sh = wpos(sm.nodes[sm.arm[s][0]].world);
            const glm::vec3 up_m = sm.flight_up_model;
            const glm::vec3 fwd_m = sm.flight_fwd_model;
            const glm::vec3 rt_m = glm::cross(up_m, fwd_m);
            const float reach = sm.arm_len[s][0] + sm.arm_len[s][1];
            // ★ L-MEASURE, AND THE SIDE IS NOT GUESSED. `rt_m` is
            // `cross(up, fwd)`, which in this rig's rest frame points to his
            // LEFT (model +X is rest-left; see the pelvis sway term's own
            // banner). And `model_side 0` is the anatomical RIGHT (`_r`
            // bones) -- the crossover is measured and stated in
            // `render/rider_rig.h` and again in `sim/gait.h`. The walking
            // branch below already encodes exactly this with its
            // `out = (s == 0 ? -rt_m : rt_m)`, so this block reuses that
            // derivation rather than typing a second, silently different one.
            const bool anat_left = (s == 1);
            const glm::vec3 out = (s == 0 ? -rt_m : rt_m);
            glm::vec3 hand_t;
            if (anat_left) {
                // THE BRACE. Extended out front and slightly down, palm on the
                // machine, and NEARLY still: the only motion is a few
                // millimetres of give at the wrench's own rate, because an arm
                // holding against a torque is loaded, not frozen. Kill that
                // term and the pose reads as a mannequin; make it large and he
                // is helping to turn the wrench, which is not what a brace is.
                const float give =
                    0.010f * static_cast<float>(sim::repair_wrench_stroke(
                                 static_cast<double>(rig.work_phase), 0.62));
                // ★ AND IT IS DELIBERATELY NOT THE MIRROR OF THE WRENCH ARM
                // (L-MEASURE's "sides never mirror", applied to a POSE rather
                // than to a sign): the brace is LONG and reaches ACROSS the
                // midline onto the machine, the wrench arm is SHORT and folded
                // at the elbow over the fastener. Two arms doing the same
                // thing at opposite x is the read this pose exists to avoid.
                const glm::vec3 dir = glm::normalize(
                    fwd_m * 0.92f - up_m * 0.30f - out * 0.10f);
                hand_t = sh + dir * (0.88f * reach + give);
            } else if (rig.work_blow_on) {
                // THE SETTLING BLOW. `+1` is the fist at the top of the wind
                // up, `-1` is it driven down into the machine; the sim owns
                // the shape (`repair_blow_swing`) and this is the arc it is
                // spent on. The hand stays out FRONT throughout -- he is
                // hitting the pump, not the sky and not his own boot.
                const float bs = rig.work_blow;
                const glm::vec3 dir =
                    glm::normalize(fwd_m * 0.58f + up_m * (0.66f * bs) +
                                   out * 0.10f);
                hand_t = sh + dir * (0.82f * reach);
            } else {
                // THE WRENCH. The hand travels a short arc across the front of
                // the machine -- up-and-in on the loaded pull, back down on the
                // reset -- with the elbow travelling with it because the chain
                // solves from the shoulder. Small on purpose: a ratchet has
                // maybe 20 degrees of swing before you reset it, and a big arc
                // reads as stirring a pot.
                const float k = static_cast<float>(sim::repair_wrench_stroke(
                    static_cast<double>(rig.work_phase), 0.62));
                const glm::vec3 dir = glm::normalize(
                    fwd_m * 0.72f - up_m * (0.52f - 0.22f * k) +
                    out * (0.20f + 0.12f * k));
                hand_t = sh + dir * (0.66f * reach);
            }
            solve_chain(sm, sm.arm[s], hand_t, sm.flight_q * sm.arm_bend_n[s],
                        sm.arm_tw[s], sm.arm_len[s]);
        }
        if (!welded && !work_now && sm.gait_on && sm.arm[s][0] >= 0) {
            const glm::vec3 sh = wpos(sm.nodes[sm.arm[s][0]].world);
            const glm::vec3 up_m = sm.flight_up_model;
            const glm::vec3 fwd_m = sm.flight_fwd_model;
            const glm::vec3 rt_m = glm::cross(up_m, fwd_m);
            const float reach = sm.arm_len[s][0] + sm.arm_len[s][1];
            // ★ THE SPREAD IS THE SNOW'S. A man on a packed trail swings his
            // arms by his sides; a man wading chest-deep holds them OUT, and
            // the deeper it is the wider they go. `depth_frac` is the kernel's
            // own continuous number, so there is no threshold and hardpack
            // spreads exactly zero.
            const float dfrac = sm.gait_depth_frac;
            const float spread = 0.25f + 0.55f * dfrac;  // radians out
            // ★ AND THE SWING IS ANTI-PHASE WITH THE SAME-SIDE LEG -- the right
            // arm is forward when the right leg is back. That is measured human
            // coordination, and it is the half that makes a walk read as one
            // body rather than four limbs. It COLLAPSES as the spread grows:
            // below a wading pace the arms stop swinging and start balancing,
            // which is what stops a trudge looking like a fast walk slowed
            // down.
            const double ph =
                static_cast<double>(sm.gait_phase) + (s == 0 ? 0.5 : 0.0);
            const float swing = static_cast<float>(std::sin(ph * 6.2831853)) *
                                sm.gait_arm_swing * (1.0f - 0.8f * dfrac);
            const glm::vec3 out = (s == 0 ? -rt_m : rt_m);
            // Hand hangs at 0.86 of reach -- a slightly bent elbow, and never
            // at full extension where a two-bone solver goes singular.
            const glm::vec3 dir =
                glm::normalize(-up_m * std::cos(spread) +
                               out * std::sin(spread) + fwd_m * swing);
            const glm::vec3 hand_t = sh + dir * (0.86f * reach);
            solve_chain(sm, sm.arm[s], hand_t, sm.flight_q * sm.arm_bend_n[s],
                        sm.arm_tw[s], sm.arm_len[s]);
            // ★★★ G1e -- NO `continue` HERE, AND THAT IS THE WHOLE BUG CHAD
            // SAW ("walking with hands"). Arms and legs share this one
            // per-side loop: the `continue` R4c-3 shipped here skipped the
            // WALKING-LEG block below on every afoot frame, for both sides
            // -- the arms swung and the legs never solved, on main, since
            // adef92479, gated-but-never-flown. Everything between here and
            // the leg block is `welded`-gated, so falling through while
            // afoot touches nothing else.
        }
        if (welded && sm.arm[s][0] >= 0 && sm.n_grip[s] >= 0) {
            glm::mat4 hand_w = sm.nodes[sm.n_grip[s]].world * sm.hand_off[s];
            glm::vec3 bend_n = sm.arm_bend_n[s];
            // ★★★ ST-5 THE HAND HOOK -- "grasp the stock of the launcher".
            //
            // ⚠ WHICH HAND IS WHICH IS MEASURED, NOT NAMED. SledRig indexes
            // its two grips ANATOMICALLY ([0] left, [1] right) because that is
            // what the caller knows; this loop indexes by MODEL side, and on
            // this asset the rider's right is model -X. The bridge is
            // `arm_throttle_side`, which capture_rest_ik derives from the
            // shipped geometry (the grip socket at negative x) precisely so a
            // rename or a mirrored re-export cannot swap it -- the same lesson
            // flak_pose's `left_sign` paid for. With no derived side there is
            // no ruling to apply and both hands stay on the bars.
            //
            // THE PAIRING IS THE GUNSTOCK'S, and it is not a side question:
            // both grips sit on the launcher's CENTRELINE, one behind the
            // other, so there is no left grip and right grip to match up.
            // What decides it is the TRIGGER: the rear pistol grip (st_grip_r)
            // is the firing hand, which Chad's rider fires with his right, and
            // the forward grip (st_grip_l) is the support hand. draw.cpp fills
            // hand_grip[1] from st_grip_r and hand_grip[0] from st_grip_l.
            const int anat = sm.arm_throttle_side < 0
                                 ? -1
                                 : (s == sm.arm_throttle_side ? 1 : 0);
            const float gw =
                anat < 0 ? 0.0f
                         : std::fmax(0.0f, std::fmin(1.0f,
                                                     rig.hand_grip[anat].weight));
            // ★ R2c-5, THE CONTROL-INPUT POSE. `throttle` drives the RIGHT arm
            // (elbow down, hand up), `brake` the LEFT (elbow up, hand down and
            // over the bar); the sides are derived, the signs are measured, and
            // the magnitudes are Chad's one 20 deg. At zero input every term
            // below is exactly zero, so the signed rest visual is untouched.
            // arm_throttle_side < 0 means the sockets never bound (the load
            // refuses such a GLB loudly, so this is belt-and-braces): with no
            // derived side there is no ruling to apply, and BOTH arms must
            // then sit still rather than both answering the brake.
            const bool has_ctl = sm.arm_throttle_side >= 0;
            const bool is_throttle = s == sm.arm_throttle_side;
            // ★★★ ST-5: A HAND THAT HAS LET GO IS NOT WORKING THE LEVER.
            // The control pose fades out with exactly the weight that fades
            // the weld across, so the throttle wrist-roll cannot go on
            // twisting a bar he released. `gw` is 0 whenever the launcher is
            // stowed, and this line is then the multiply by one it was.
            const float amt = (!has_ctl ? 0.0f
                                        : (is_throttle ? rig.throttle
                                                       : rig.brake)) *
                              (1.0f - gw);
            const float dir_s = is_throttle ? 1.0f : -1.0f;  // brake is UP/DOWN
            // ★★ R3-HANDS. THE DIFFERENTIAL, and it is the instrument this
            // rung exists because nobody had. The rider's whole body rides the
            // suspension, so a raw elbow HEIGHT moves ~40 mm frame to frame
            // with zero control input -- which is why five prints of an
            // absolute position could never have shown that the brake elbow
            // rises 2 mm. Solve the SAME frame once WITHOUT the control term
            // and keep the elbow/wrist, so the debug line below reports a
            // delta that can actually be wrong.
            static const bool ctl_debug =
                std::getenv("SEADS_SLED_CTL_DEBUG") != nullptr;
            glm::vec3 el0(0.0f), wr0(0.0f);
            if (ctl_debug) {
                solve_chain(sm, sm.arm[s], wpos(hand_w), bend_n, sm.arm_tw[s],
                            sm.arm_len[s]);
                el0 = wpos(sm.nodes[sm.arm[s][1]].world);
                wr0 = wpos(hand_w);
            }
            if (amt > 0.0f) {
                // ★ THE BAR ROLL IS THROTTLE-ONLY NOW (Chad, 2026-08-24).
                // Rolling the whole hand about the bar IS twisting the grip --
                // "it looks like it twists the handle backwards" -- and on a
                // real machine the fingers close on the lever while the hand
                // holds the bar still. The brake's squeeze moved to
                // `mittfront_01_l` below. The throttle side keeps its roll
                // untouched: he did not name it, and its +29.1 mm is signed.
                if (is_throttle) {
                    hand_w = roll_about_axis(
                        hand_w, wpos(sm.nodes[sm.n_grip[s]].world), bar_w,
                        dir_s * sm.arm_wrist_up[s] * kControlWristRad * amt);
                }
                // then the SHOULDER swing, about the chain direction the moved
                // wrist defines -- the pole angle, which is what carries the
                // elbow down or up without touching the contact or the flex.
                // ★ BOTH SIDES KEEP CHAD'S ONE 20 DEG, UNCHANGED. The brake
                // elbow's old 2.1 mm was never the angle's fault -- the bar
                // roll above was dragging the very chain this rotates about.
                // Measured with the roll gone: +29.6 mm. See capture_rest_ik.
                bend_n = swing_bend_normal(
                    bend_n, wpos(hand_w) - wpos(sm.nodes[sm.arm[s][0]].world),
                    dir_s * sm.arm_swing_down[s] * kControlElbowSwingRad * amt);
            }
            // ★★★ ST-5 THE SUBSTITUTION ITSELF, and it is four lines because
            // it is not a new solver: the ONE thing that changes is the point
            // this weld welds to. Everything after it -- the two-bone IK, the
            // twist offsets, the world override that makes the contact exact
            // -- is the code that has always run, so the elbow bends its way
            // out to the launcher on its own and nothing has to be told how.
            //
            // ORIENTATION FIRST, THEN THE CONTACT. The hand keeps the WRAP it
            // holds a handlebar with (the rest weld `hand_off` captured that,
            // and it is the only measurement of this rider's grip anybody
            // has); what changes is the cylinder it wraps. So swing the whole
            // hand by the shortest arc that carries the LIVE bar axis onto the
            // launcher grip's axis, scaled by the weight -- at 0 it is the
            // identity, at 1 his knuckles lie across the launcher exactly as
            // they lay across the bar, which points them along the fire axis
            // because the grip axis is square to it.
            //
            // ⚠ THE SIGN OF `lat` IS NOT TRUSTED. An axis has two ends and the
            // caller's is the launcher's, not this rider's; taking the wrong
            // one rotates his hand a half turn, palm out. So pick the end his
            // knuckles ALREADY point at, measured against the bar he is
            // holding this frame -- the same "derive it, never type it"
            // discipline as arm_throttle_side above.
            if (gw > 0.0f) {
                glm::vec3 tgt_m =
                    glm::vec3(g_w2m * glm::dvec4(rig.hand_grip[anat].pos, 1.0));
                // ★★★ THE REACH CLAMP, AND IT IS NOT OPTIONAL. The weld ends
                // in a world_override -- the hand node is PLACED at the
                // target while solve_chain can only straighten the arm to
                // upperarm+forearm. A target past that length tears wrist
                // from forearm, and on a single-skinned-prim rider the tear
                // drags the sweater and torso with it (the "collapsed over"
                // defect: a 1.05 m target on a 0.61 m arm). Whatever the
                // caller asks for, the chain is never asked to reach past
                // itself; a clamped grip reads as a hand resting short of
                // the grip, which is a pose, not a wound.
                {
                    const glm::vec3 sh_m = wpos(sm.nodes[sm.arm[s][0]].world);
                    const float reach =
                        sm.arm_len[s][0] + sm.arm_len[s][1] - 1e-3f;
                    const glm::vec3 dv = tgt_m - sh_m;
                    const float dl = glm::length(dv);
                    if (dl > reach) tgt_m = sh_m + dv * (reach / dl);
                }
                glm::vec3 lat_m =
                    glm::vec3(g_w2m * glm::dvec4(rig.hand_grip[anat].lat, 0.0));
                const float ll = glm::length(lat_m);
                const float bl = glm::length(bar_w);
                if (ll > 1.0e-6f && bl > 1.0e-6f) {
                    lat_m /= ll;
                    const glm::vec3 bax = bar_w / bl;
                    if (glm::dot(lat_m, bax) < 0.0f) lat_m = -lat_m;
                    const glm::vec3 ax = glm::cross(bax, lat_m);
                    const float axl = glm::length(ax);
                    if (axl > 1.0e-6f)
                        hand_w = roll_about_axis(
                            hand_w, wpos(hand_w), ax,
                            std::atan2(axl, glm::dot(bax, lat_m)) * gw);
                }
                // ...and only now the contact moves. A plain mix, so the CUT
                // (deploy -> 0 in one frame) puts the hand back on the bar in
                // one frame too: there is no smoothing in this weld to fight
                // it, and no state that could disagree about where the hand
                // was going.
                const glm::vec3 mixed = glm::mix(wpos(hand_w), tgt_m, gw);
                hand_w[3] = glm::vec4(mixed, 1.0f);
            }
            solve_chain(sm, sm.arm[s], wpos(hand_w), bend_n, sm.arm_tw[s],
                        sm.arm_len[s]);
            sm.nodes[sm.arm[s][2]].world = hand_w;
            sm.nodes[sm.arm[s][2]].world_override = true;
            // ★ R2c-5 EVIDENCE, and it is here because NO ctest runs this TU:
            // SEADS_SLED_CTL_DEBUG=1 prints what the control pose actually
            // produced, so the rung is judged on measured millimetres and not
            // on eyeballing a 20 deg swing in a night screenshot. Off by
            // default, one getenv, read once (hoisted above, next to the
            // baseline solve that makes the delta possible).
            // ★★ R3-HANDS PRINTS THE DELTA, not the position. `d_elbow` and
            // `d_wrist` are this frame WITH the control minus this frame
            // WITHOUT it, so "+y = the elbow rose" is a fact and not a guess
            // about which part of the number the suspension contributed.
            if (ctl_debug) {
                const glm::vec3 el = wpos(sm.nodes[sm.arm[s][1]].world);
                const glm::vec3 wr = wpos(hand_w);
                const glm::vec3 de = el - el0, dw = wr - wr0;
                TraceLog(LOG_INFO,
                         "SLED CTL: side %d (%s) amt %.2f  d_elbow %+.1f %+.1f "
                         "%+.1f mm  d_wrist %+.1f %+.1f %+.1f mm",
                         s, is_throttle ? "throttle" : "brake",
                         static_cast<double>(amt), de.x * 1000.0f,
                         de.y * 1000.0f, de.z * 1000.0f, dw.x * 1000.0f,
                         dw.y * 1000.0f, dw.z * 1000.0f);
            }
        }
        // ★★★ R4c §7.3 STAGE 7 -- THE WALK. The boot is no longer welded to a
        // running board, so something has to say where it goes, and this is
        // the ONLY place in the file that does. The curve itself is
        // `sim::walker_foot_offset` -- in the library, executed by ctest --
        // and what happens here is gather / call / solve, exactly like its
        // neighbour above.
        //
        // ★ THE TWO LEGS ARE HALF A CYCLE APART and nothing else distinguishes
        // them: one phase, one curve, one stride. A second authored leg is how
        // a gait starts disagreeing with itself.
        //
        // ★ EVERY OFFSET IS IN HIS OWN FRAME (`flight_fwd_model` /
        // `flight_up_model`), so his feet step along the direction the KERNEL
        // is walking him -- the one thing a procedural gait must never get
        // wrong, and the reason the root carries his orientation above.
        if (!welded && sm.gait_on && sm.leg[s][0] >= 0 && sm.n_root >= 0) {
            // Where his foot sits under him at rest, carried into his frame:
            // the STANCE WIDTH is the machine's own (he is the same man), and
            // its height is left as the rig authored it -- SEADS_WALK_FOOTDROP
            // lowers it if his boots read high on the snow. ⚠ THAT DROP IS THE
            // ONE NUMBER IN THIS RUNG I COULD NOT MEASURE WITHOUT THE GAME: the
            // rest pose has his boots on a running board, not on the ground,
            // and how far that is above the snow is a fact about the .blend.
            // It is a dial with a default of 0 and a printed measurement, not
            // a constant somebody chose.
            // ★★★ THE TARGET HANGS FROM THE HIP, NOT FROM HIS SEATED POSE, AND
            // THIS IS THE FIX FOR "legs are not to be animated like a wheel".
            //
            // The first version built the foot target off `rest_world[foot] -
            // rest_world[root]` -- his offset IN THE SEATED RIDING POSE. That
            // put his boots where they sit ON THE RUNNING BOARDS: some 0.2 m
            // ABOVE the ground bone and roughly 0.55 m apart, straddling a
            // machine that is no longer there. Add the swing lift on top and
            // the target ends up only ~0.3 m below the hip against a 0.9 m leg
            // chain -- so the IK had to fold the leg almost double and swing
            // the thigh right round to reach it. That is Chad's wheel, and his
            // straddle, and the IK clamp, all from one wrong origin.
            //
            // ★ SO IT IS BUILT FROM TWO THINGS THE RIG ITSELF MEASURES: the
            // HIP's own posed position (so the stance width is his hip width,
            // not the machine's), and the leg chain's own length (so the foot
            // hangs where a leg can actually put it and the solver never
            // clamps). Chad's ruling in his own words: "lift their thighs to
            // step forwards, not to rotate 360 degrees at the hip" -- a target
            // a leg can reach IS a thigh lift; a target it cannot is a wheel.
            const glm::vec3 hip_now = wpos(sm.nodes[sm.leg[s][0]].world);
            const glm::vec3 up_m = sm.flight_up_model;
            const glm::vec3 fwd_m = sm.flight_fwd_model;
            const glm::vec3 rt_m = glm::cross(up_m, fwd_m);
            // ★ 0.92 OF THE CHAIN, not 1.0: a standing leg is very slightly
            // bent, and a target at the full reach is exactly where a two-bone
            // solver goes singular. It is also the headroom the swing lift is
            // spent out of, which is why the lift can never drive the solver
            // into its clamp.
            // The chain is two segments (thigh, calf) -- their SUM is what a
            // leg can reach.
            const float leg_reach = sm.leg_len[s][0] + sm.leg_len[s][1];
            // ⚠ SEADS_WALK_FOOTDROP survives as an ADDITIVE trim on either
            // path below, but its default is now 0: the sampled ground
            // (GAIT LADDER G1) replaces the unmeasured offset it used to
            // carry. See render/sled_model.h SledRig::gait's banner.
            static const float foot_drop = [] {
                const char* e = std::getenv("SEADS_WALK_FOOTDROP");
                return e != nullptr ? static_cast<float>(std::atof(e)) : 0.0f;
            }();

            // ★★★ GAIT LADDER G1: THE CROSSOVER. `sim::GaitFoot`'s own
            // contract (sim/gait.h) is 0 = left, 1 = right; `sm.leg[s]`'s
            // `model_side` runs the OTHER way (render/rider_rig.h's measured
            // crossover: `sm.leg[0]` is the anatomical RIGHT leg). This is
            // the one place that has to cross the wire back.
            const int gait_idx = (s == 0) ? 1 : 0;
            const bool have_gait =
                rig.gait != nullptr && sm.gait_foot_ok[gait_idx];

            // ★★★ KNOWN, MEASURED, DEFERRED TO G2 (verify round G1c; see
            // docs/PLAN_20260904_gait_ladder.md §6): late stance rides the
            // reach clamp -- hip-to-ground ~0.835 m plus the horizontal
            // trail from a world-fixed pin exceeds 0.98*leg_reach on an
            // ordinary stride, because a rigid-height hip genuinely cannot
            // span it. A HUMAN can't either: the pelvis drops at contact.
            // That is G2's pelvis law ("a leg over-reach lowers the pelvis,
            // never straightens the knee"), and NO render-side rewrite of
            // the target can substitute for it -- G1b tried scaling the
            // pin-to-hip horizontal by 0.35 and that re-derived the target
            // off the MOVING hip, i.e. skate at 0.65x body speed by
            // construction (the verify round's P1). So the target passes
            // through UNTOUCHED; the clamp below stays as the safety net
            // (its brief late-stance engagement is the accepted, measured
            // G1 debt), and SEADS_GAIT_DEBUG=1 prints the clamp duty cycle
            // once a second so G2 tunes against a number, not a feeling.
            static const bool gait_debug = [] {
                const char* e = std::getenv("SEADS_GAIT_DEBUG");
                return e != nullptr && std::atoi(e) != 0;
            }();

            glm::vec3 foot_t;
            if (have_gait) {
                // ★ THE TARGET IS THE SAMPLED GROUND, not the hip-hang law
                // below -- `sm.gait_foot_model` was converted into THIS
                // frame (model space, off the posed root) once, in
                // `sled_model_draw`, and this consumer runs no gait math of
                // its own (L-PURE).
                // ★ RED-TEAM FIX (G1b, P3): the footdrop trim is applied
                // BEFORE the reach clamp now, not after -- adding it past the
                // clamp could itself push a target back out of reach.
                // ★★★ G1e: THE CHAIN ENDS AT THE ANKLE, NOT THE SOLE. The
                // sim's targets are ground points (sole level); the 2-bone
                // chain the solver drives ends at the FOOT BONE. A standing
                // man's ankle sits (pelvis rest height - leg chain) above
                // his sole -- both rig-measured, nothing typed -- and
                // without this the ground is beyond the chain's reach on
                // EVERY frame (the 100% clamp duty the smoke rig measured).
                // ★ G2c (Chad: "feet go below the road"): the height is the
                // MEASURED boot-sole->ankle number the flak gunner solve
                // already owns (GunnerAnthro::ankle_up_m, 0.10 m) -- the
                // earlier derivation (pelvis rest - chain = 0.072) was a
                // straight-leg estimate 2.8 cm short, and the boots sank by
                // exactly that on the plowed road. One number, one owner.
                const float ankle_h =
                    static_cast<float>(flak::GunnerAnthro{}.ankle_up_m);
                const glm::vec3 target_m = sm.gait_foot_model[gait_idx] +
                                           up_m * (ankle_h + foot_drop);
                const glm::vec3 delta = target_m - hip_now;
                const float dist = glm::length(delta);
                // ★ L-MEASURE: reach clamped <= 0.98*chain -- a clamped
                // `solve_chain` is a rigid strut, never handed an
                // unreachable target. The late-stance engagement of this
                // clamp is the measured G1 debt the banner above records.
                const float max_reach = 0.98f * leg_reach;
                const bool clamped = dist > max_reach && dist > 1.0e-6f;
                foot_t = clamped ? hip_now + delta * (max_reach / dist)
                                 : target_m;
                if (gait_debug) {
                    // Duty cycle of the clamp, plus the fore-aft SWEEP of the
                    // target relative to the hip -- if the legs ever read
                    // frozen again, this line says whether the TARGETS moved
                    // (sim + conversion healthy, blame the solve) or did not
                    // (blame the frames upstream).
                    static int hits = 0, total = 0;
                    static float sweep_min = 1.0e9f, sweep_max = -1.0e9f;
                    static double window_start = GetTime();
                    ++total;
                    if (clamped) ++hits;
                    const float along = glm::dot(delta, fwd_m);
                    sweep_min = std::min(sweep_min, along);
                    sweep_max = std::max(sweep_max, along);
                    const double now = GetTime();
                    if (now - window_start >= 1.0) {
                        TraceLog(LOG_INFO,
                                 "GAIT clamp duty: %.1f%% (%d/%d), target "
                                 "sweep fwd [%.2f, %.2f] m, last %.2f s",
                                 total > 0 ? 100.0 * hits / total : 0.0, hits,
                                 total, static_cast<double>(sweep_min),
                                 static_cast<double>(sweep_max),
                                 now - window_start);
                        hits = 0;
                        total = 0;
                        sweep_min = 1.0e9f;
                        sweep_max = -1.0e9f;
                        window_start = now;
                    }
                }
            } else {
                if (gait_debug) {
                    static int fb_n = 0;
                    if ((++fb_n % 120) == 0)
                        std::fprintf(stderr,
                                     "GAIT legs FALLBACK: gait=%d ok=%d\n",
                                     rig.gait != nullptr ? 1 : 0,
                                     sm.gait_foot_ok[gait_idx] ? 1 : 0);
                }
                // ★ THE PRE-G1 LAW, BIT-IDENTICAL: `rig.gait == nullptr` (no
                // gait wired up) or the foot has not been seeded yet (its
                // very first tick) falls back here exactly as it always did.
                const float stand_drop = 0.92f * leg_reach;
                double f_off = 0.0, u_off = 0.0;
                sim::walker_foot_offset(
                    static_cast<double>(sm.gait_phase) + (s == 0 ? 0.0 : 0.5),
                    static_cast<double>(sm.gait_stride_m),
                    static_cast<double>(sm.gait_lift_m),
                    static_cast<double>(sm.gait_stance_frac), &f_off, &u_off);
                // ★ AND THE LIFT IS SPENT UPWARD FROM THE FOOT, i.e. it
                // SHORTENS the drop -- a raised foot is a folded leg, never a
                // longer one.
                const float drop =
                    stand_drop + foot_drop - static_cast<float>(u_off);
                foot_t = hip_now - up_m * drop + fwd_m * static_cast<float>(f_off);
            }
            // ★ THE POLE VECTOR TURNS WITH HIM. `leg_bend_n` is the MODEL-space
            // direction the knee folds toward; leaving it unrotated bends the
            // knees of a man walking east as though he were still facing the
            // machine's nose. Same quaternion the root just used.
            solve_chain(sm, sm.leg[s], foot_t, sm.flight_q * sm.leg_bend_n[s],
                        sm.leg_tw[s], sm.leg_len[s]);
            if (have_gait) {
                // ★★★ RED-TEAM FIX (G1b, P2): THE AXIS WAS BACKWARDS.
                // `sim::gait.cpp`'s own convention is "+pitch_rad = toe-up"
                // (heel_r < toe_r). Rodrigues, small-angle: rotating a vector
                // `v` by `theta` about axis `A` moves it by `theta*(A x v)`.
                // Take `v = fwd_m` (the direction toward the toe) and
                // `A = rt_m = up_m x fwd_m`: by the BAC-CAB identity,
                //     (up x fwd) x fwd = fwd*(up.fwd) - up*(fwd.fwd) = -up
                // (fwd, up orthonormal: up.fwd = 0, fwd.fwd = 1). So a
                // POSITIVE angle about `rt_m` moves the toe TOWARD `-up_m`,
                // i.e. TOE-DOWN -- backwards from the sim's own sign.
                // Negating the axis flips the sign of the cross product:
                //     (fwd x up) x fwd = up*(fwd.fwd) - fwd*(up.fwd) = up
                // so `-rt_m` (equivalently `fwd_m x up_m`) moves the toe
                // TOWARD `up_m` for a positive angle -- toe-up, matching
                // `sim/gait.cpp` exactly. ONE axis correctly serves BOTH
                // feet: this derivation never used `side_sign` -- the
                // sagittal (fwd/up) plane a toe pitches in is the same
                // plane for the left foot and the right foot.
                // ★ FRAME (verify round G1c): `rt_m` is built from
                // `flight_up_model`/`flight_fwd_model`, which are ALREADY the
                // man's CURRENT directions in model space -- the same space
                // the node `world` matrices live in. Do NOT multiply by
                // `flight_q` here: `flight_q` maps REST axes onto those
                // current directions, so applying it to the already-current
                // `rt_m` rotates by his heading twice (at 90 deg off rest
                // heading the "pitch" becomes a foot ROLL). Contrast the
                // pole vector above, `flight_q * leg_bend_n[s]`, which IS a
                // rest-captured vector and needs exactly that mapping.
                const glm::vec3 side_axis_m =
                    glm::length(rt_m) > 1.0e-9f
                        ? glm::normalize(-rt_m)
                        : glm::vec3(1.0f, 0.0f, 0.0f);
                const glm::quat pitch_q = glm::angleAxis(
                    sm.gait_foot_pitch_rad[gait_idx], side_axis_m);
                // ★★★ G2b (Chad's drive: "feet are not attached, stuck
                // leading out front"): the first cut read the foot's CURRENT
                // world here -- but at this point in the pass that is the
                // STALE pre-IK channel pose (the settle sweep that would
                // re-attach it to the solved calf runs AFTER the chains, and
                // `world_override` makes it skip this node). So the boot
                // froze wherever the channel pose had left it while the leg
                // walked behind it. The foot's world is COMPOSED FROM THE
                // SOLVED CALF instead -- `calf.world * foot.local` puts the
                // ankle exactly on the IK target by solve_chain's own
                // contract -- and the pitch is applied about that.
                const int n_calf = sm.leg[s][1];
                const int n_foot = sm.leg[s][2];
                // ★★★ G2c -- THE ANKLE IS A HINGE (Chad: "walking on tippy
                // toes; need an ankle bone to hinge the feet"). POSITION
                // comes from the solved calf (G2b: `calf.world *
                // foot.local`, the ankle lands on the IK target by
                // solve_chain's contract). ORIENTATION does NOT: a foot that
                // inherits the calf's rotation points its toe down whenever
                // the knee flexes or the leg trails -- ballet. The boot's
                // world orientation is set ABSOLUTELY: the REST foot
                // orientation (boot flat, authored) carried onto his current
                // frame by `flight_q` (the rest->current mapping -- this IS
                // a rest-captured quantity, so unlike the G1c pitch axis it
                // NEEDS flight_q; see that banner), then the gait pitch on
                // top. The calf may do what it likes above it -- that
                // freedom is the hinge. G4 adds the heel-strike/toe-off
                // CURVE through the same `gait_foot_pitch_rad` channel.
                const glm::vec3 ankle_pos =
                    wpos(sm.nodes[n_calf].world * sm.nodes[n_foot].local);
                const glm::mat3 foot_flat =
                    glm::mat3_cast(sm.flight_q) *
                    glm::mat3(sm.rest_world[n_foot]);
                glm::mat4 foot_w =
                    glm::mat4(glm::mat3_cast(pitch_q) * foot_flat);
                foot_w[3] = glm::vec4(ankle_pos, 1.0f);
                sm.nodes[n_foot].world = foot_w;
                sm.nodes[n_foot].world_override = true;
            }
            continue;
        }
        if (welded && sm.leg[s][0] >= 0 && sm.n_board[s] >= 0) {
            // ★ DEFECTS 7 + 6. The boot target is the BOARD SOCKET's live
            // world transform times the weld captured at load -- exactly the
            // line above it for the hand -- not `rest_world[foot]`, which was a
            // load-time constant that tracked nothing.
            glm::mat4 foot_w = sm.nodes[sm.n_board[s]].world * sm.foot_off[s];
            glm::vec3 foot_t = wpos(foot_w);
            glm::vec3 bend_n = sm.leg_bend_n[s];
            // ★★★ R3-WS. THE DEFECT THIS RUNG EXISTS TO KILL: the boot above
            // was welded RIGID to the board socket, so sliding the butt back
            // (or standing) straightened the legs against a fixed foot and
            // dragged the arms off the bars. Chad: "the feet move front to
            // back on the running boards". They do now.
            //
            // Two blends, both C1 and both exactly 0 at u = 0:
            //   w_deck  -- the rigid weld -> the parametric deck target
            //   w_kneel -- the deck -> the KNEEL construction on the seat
            if (ws.active) {
                // ★★★ R3-WS(c). THE FEET NO LONGER TRACK THE HIPS. Chad drove
                // v1 and said the feet "only go back a little" -- because
                // kFootTrack tied them 1:1 to a 0.20-0.32 m pelvis retreat.
                // They now run their own schedule the full length of the
                // runner (1.0054 m at zero stand), which is what lets the
                // pelvis retreat SHRINK and the trunk stay up.
                foot_t = glm::mix(
                    foot_t,
                    deck_foot_target(sm.ws_foot_rest[s], ws.foot_slide),
                    ws.w_deck);
                // ★ R4a THE LEG THAT SHOVES. When the self-right is pushing, the
                // leg on the far side from his brace STRAIGHTENS into the
                // machine -- the visible half of "standing automatically helps
                // push you over righted". ADDITIVE and exactly zero when
                // rig.right_push is 0, so the signed R3 pose is untouched in
                // all ordinary riding: this cannot move a pose Chad approved.
                //
                // s: 0 = LEFT, 1 = RIGHT. right_push > 0 means bracing LEFT, so
                // the RIGHT foot drives. The foot goes DOWN and OUTBOARD, which
                // is what extending a leg against a machine on its side looks
                // like -- the knee opens and the boot reaches for the deck.
                if (rig.right_push != 0.0f) {
                    const float side_sign = (s == 0) ? -1.0f : 1.0f;
                    const float drive =
                        std::max(0.0f, rig.right_push * side_sign);
                    // 0.16 m of reach: the measured seated-to-standing lateral
                    // box is lean_lat_stand_m 0.35, and this is roughly half of
                    // it -- a real shove, still inside the rig's solved range
                    // so the IK does not saturate (dmax 0.8181 m).
                    foot_t.y -= 0.16f * drive;
                    foot_t.x += 0.10f * drive * side_sign;
                }
                if (ws.kneel_ok && ws.w_kneel > 0.0f) {
                    // ★ CONSTRUCTED AGAINST THE **LIVE** HIP, here, in the
                    // pose pass. ladder_state resolves against a closed-form
                    // stand reference and the R1c Newton then nudges the root;
                    // carrying the construction forward from there left the
                    // knee a measured 41.2 mm off the seat it is supposed to
                    // be resting on. Rebuilding it on the hip the pose
                    // actually has makes BOTH gates exact: the triangle is
                    // consistent (|solved knee - K| ~ 0) and the contacts are
                    // on the measured surface (|K - seat| ~ 0). The captured
                    // knee_rel/foot_rel remain the fallback for the case the
                    // sphere misses -- loud, not silent.
                    const glm::vec3 hip_live =
                        wpos(sm.nodes[sm.leg[s][0]].world);
                    const KneelPose kp = kneel_construct(
                        hip_live, sm.leg_len[s][0], sm.leg_len[s][1]);
                    const glm::vec3 knee_w =
                        kp.ok ? kp.knee : hip_live + ws.knee_rel[s];
                    const glm::vec3 foot_w2 =
                        kp.ok ? kp.foot : hip_live + ws.foot_rel[s];
                    // ★★★ R3-WS(d) / D4(b). NOT a straight lerp: the linear path
                    // sweeps the boot THROUGH the seat prism for about a third
                    // of the band (measured 0.10 m below the surface at
                    // mid-band). ws_kneel_foot_mix lifts the vertical on a
                    // faster schedule so the boot clears the seat first and
                    // then travels inboard and aft above it. Both endpoints
                    // are unchanged.
                    foot_t = ws_kneel_foot_mix(foot_t, foot_w2, ws.w_kneel);
                    // ★ SPEC 6: the kneel bends the knee in a DIFFERENT plane,
                    // so the bend normal is re-derived from the constructed
                    // (hip, knee, ankle) triangle by the SAME pure function
                    // that captured the rest one -- never a typed sign. With
                    // the triangle consistent by construction, the analytic IK
                    // reproduces the knee (gated at 1 mm in test_rider_pose).
                    const glm::vec3 kn = rest_bend_normal(
                        hip_live, knee_w, foot_w2,
                        glm::mat3(sm.nodes[sm.leg[s][0]].world));
                    // ★★★ R3-WS(d). NOT a mix(). The rest and kneel bend planes
                    // are near-ANTIPARALLEL on this rig (the rest ankle is
                    // 0.687 m ahead of the hip, the kneeling one 0.070 m
                    // behind it), and a lerp between antiparallel normals
                    // passes through zero -- which is what tore the legs into
                    // flat shards the first time the kneel was rendered. The
                    // pole is ROTATED about the chain instead; see
                    // render/rider_pose.h.
                    bend_n = ws_blend_bend_normal(sm.leg_bend_n[s], kn,
                                                  foot_t - hip_live,
                                                  ws.w_kneel);
                }
            }
            solve_chain(sm, sm.leg[s], foot_t, bend_n, sm.leg_tw[s],
                        sm.leg_len[s]);
            // The pin keeps the machine-carried boot orientation (0-OFF), and
            // in the kneel blends toward the orientation the SHIN gives it, so
            // the boot does not stay sole-down while the shin points aft.
            if (ws.active) {
                if (ws.w_kneel > 0.0f) {
                    const glm::mat4 shin_w = sm.nodes[sm.leg[s][1]].world *
                                             sm.nodes[sm.leg[s][2]].local;
                    const glm::quat qa = glm::quat_cast(glm::mat3(foot_w));
                    glm::quat qb = glm::quat_cast(glm::mat3(shin_w));
                    // ★ R3-WS(d). THE SHORT WAY ROUND, EXPLICITLY. A quaternion
                    // and its negation are the same rotation, and this pair is
                    // roughly a half-turn apart once the shin folds back --
                    // slerp without the sign fix takes the LONG arc, so the
                    // boot spins right through the seat on the way. Same class
                    // of bug as the bend-normal lerp above, one line to the
                    // left of it.
                    if (glm::dot(qa, qb) < 0.0f) qb = -qb;
                    foot_w = glm::mat4(
                        glm::mat3_cast(glm::slerp(qa, qb, ws.w_kneel)));
                }
                foot_w[3] = glm::vec4(foot_t, 1.0f);
            }
            // Pin the foot and flag the override, mirroring the hand, so the
            // settle pass below does not recompose it off the shin and undo the
            // plant. The boots are part of the ONE skinned `sudburian_proxy`,
            // so they follow foot_l/r through the joint matrices, not through
            // a parent node transform.
            sm.nodes[sm.leg[s][2]].world = foot_w;
            sm.nodes[sm.leg[s][2]].world_override = true;
        }
    }
    // settle descendants of IK-overridden bones (mitts under hands, feet
    // under shins): one parents-first pass recomputing every non-overridden
    // world -- idempotent for untouched subtrees, correct for the rest.
    // ★★ R3-HANDS -- THE TWO CONTROL BONES (Chad, 2026-08-24). "the thumb
    // should be animated for the throttle" and "the fingers in front of the
    // red brake lever should squeeze in". Both are LEAVES hanging off a hand
    // the arm IK has just overridden, so they are written as a channel-
    // modified `local` HERE and composed by the pass below -- which is exactly
    // what that pass exists to do, and why this needs no override of its own.
    //
    // The AXIS and the DIRECTION were measured at load against the lever each
    // bone reaches for (throttle_block / brake_lever); only the MAGNITUDE is
    // typed, and it is Chad's own ruled 20 deg rather than a new invention.
    // At zero input curl_rotation returns exactly the identity, so the signed
    // rest pose is reproduced bit-for-bit and neither bone can perturb it.
    {
        const struct {
            int node;
            const CurlAim& aim;
            float rad;
            float amt;
        } ctl[2] = {{sm.n_thumb_r, sm.thumb_aim, kControlThumbRad, rig.throttle},
                    {sm.n_mittfront_l, sm.mitt_aim, kControlFingerRad,
                     rig.brake}};
        // ★★ THE LEVERS MOVE. Each red lever turns about its own measured pin
        // by its own kernel input, so the brake blade squeezes back into the
        // bar and the thumb paddle hinges under the thumb. Zero input rebuilds
        // the identity EXACTLY (glm::rotate by 0 is the identity), so the rest
        // machine is untouched -- the same 0-OFF law the rider pose runs on.
        // ★★ THE LEVERS MOVE. Each turns about its own measured pin by its
        // own kernel input, so the brake blade squeezes back into the bar and
        // the throttle lever pushes forward onto it.
        //
        // ★ ONE SIGN SERVES BOTH, and that is derived rather than lucky. About
        // the vertical pin a point at offset dx travels dz = -dx*t, so a
        // POSITIVE turn carries the brake blade (which extends outboard at +x)
        // AFT into the bar, and the throttle lever (outboard at -x) FORWARD
        // onto it. Opposite sides and opposite intents cancel exactly.
        //
        // Zero input rebuilds the identity EXACTLY (glm::rotate by 0), so the
        // rest machine is untouched -- the same 0-OFF law the rider pose runs on.
        for (SPrim& sp : sm.prims) {
            if (!sp.is_lever) continue;
            const bool brk = sp.node == sm.n_brake_lever;
            const bool thr = sp.node == sm.n_throttle_block;
            if (!brk && !thr) continue;
            const float amt = brk ? rig.brake : rig.throttle;
            const float rad = brk ? kBrakeLeverRad : kThrottleLeverRad;
            sp.anim = glm::translate(glm::mat4(1.0f), sp.pivot) *
                      glm::rotate(glm::mat4(1.0f), rad * amt, sp.axis) *
                      glm::translate(glm::mat4(1.0f), -sp.pivot);
        }

        static const bool ctl_dbg2 =
            std::getenv("SEADS_SLED_CTL_DEBUG") != nullptr;
        for (const auto& c : ctl) {
            if (c.node < 0 || c.aim.axis < 0) continue;
            SNode& nd = sm.nodes[c.node];
            const glm::mat4 was = nd.local;
            const glm::quat q = curl_rotation(c.aim, c.rad, c.amt);
            nd.local = compose(nd.t, nd.r * q, nd.s);
            // The same differential discipline as the arms: a probe point one
            // decimetre along the bone's OWN +Y, with the curl minus without,
            // so "the thumb moved" is millimetres and not a hopeful screenshot.
            // ★ IN MODEL SPACE. The first version differenced `nd.local`
            // alone, which is the BONE'S PARENT's frame -- an arbitrarily
            // rotated one -- and it read "+34 mm forward" for fingers Chad
            // could see lifting skyward. A delta is only a direction if you
            // say which frame it is in. +y is up, +z is FORWARD (the rider's
            // pelvis is aft of the bars), so a squeeze must read -z.
            if (ctl_dbg2) {
                const glm::vec4 p(0.0f, 0.1f, 0.0f, 1.0f);
                const glm::mat4 pw =
                    nd.parent < 0 ? glm::mat4(1.0f) : sm.nodes[nd.parent].world;
                const glm::vec3 d = glm::vec3(pw * (nd.local * p) - pw * (was * p));
                TraceLog(LOG_INFO,
                         "SLED CTL: %s amt %.2f  axis %d sign %+.0f  d_tip "
                         "%+.1f %+.1f %+.1f mm",
                         nd.name.c_str(), static_cast<double>(c.amt),
                         c.aim.axis, static_cast<double>(c.aim.sign),
                         d.x * 1000.0f, d.y * 1000.0f, d.z * 1000.0f);
            }
        }
    }

    // ★★ DRIVE-3 BUG FIX (Chad: "the skis don't turn when I turn my arms",
    // "the head stays still at all times"): this pass used to compose from
    // the REST TRS, silently reverting every pass-1 channel override --
    // steer on the skis, the torso lean, the head -- for every node except
    // the IK-overridden arm/leg bones (which is why ONLY the arms worked).
    // It must compose from the CHANNEL-MODIFIED locals pass 1 stored.
    for (const int i : sm.order) {
        SNode& nd = sm.nodes[i];
        if (nd.parent < 0 || nd.world_override) continue;
        nd.world = sm.nodes[nd.parent].world * nd.local;
    }

    // ★ THE TIE-ROD LINK SOLVE (TieRodLink). The hierarchy has just swept each
    // rod with the bellcrank (32 deg bar sweep) while the spindle it must meet
    // steered on its own kingpin (24 deg) -- so the outer ball is now hanging
    // in the air. Pin the inner ball where it already sits (it is ON the
    // bellcrank), rotate the rod about it onto tierod_end, and stretch along
    // its own axis so the ball SEATS whatever the two ratios did to the span.
    // world_override is load-bearing: the later foot-blend and scarf composes
    // re-derive every non-overridden node from `local` and would silently
    // undo this.
    for (const auto& tl : sm.tie_link) {
        if (tl.rod < 0) continue;
        SNode& nd = sm.nodes[tl.rod];
        const glm::vec3 p_in(nd.world * glm::vec4(tl.li, 1.0f));
        const glm::vec3 cur(nd.world * glm::vec4(tl.lo, 1.0f));
        const glm::vec3 tgt = wpos(sm.nodes[tl.end].world);
        const glm::vec3 a = cur - p_in, b = tgt - p_in;
        const float la = glm::length(a), lb = glm::length(b);
        if (la < 1e-6f || lb < 1e-6f) continue;
        const glm::vec3 ah = a / la, bh = b / lb;
        const float d = glm::dot(ah, bh);
        glm::quat q;
        if (d < -0.999999f) {
            q = glm::angleAxis(3.14159265f,
                               glm::normalize(glm::vec3(-ah.y, ah.x, 0.0f)));
        } else {
            const glm::vec3 c = glm::cross(ah, bh);
            q = glm::normalize(glm::quat(1.0f + d, c.x, c.y, c.z));
        }
        const float s = lb / la;
        glm::mat4 sax(1.0f);
        for (int ci = 0; ci < 3; ++ci)
            for (int ri = 0; ri < 3; ++ri)
                sax[ci][ri] =
                    (ci == ri ? 1.0f : 0.0f) + (s - 1.0f) * ah[ri] * ah[ci];
        const glm::mat4 fix = glm::translate(glm::mat4(1.0f), p_in) *
                              glm::mat4_cast(q) * sax *
                              glm::translate(glm::mat4(1.0f), -p_in);
        nd.world = fix * nd.world;
        nd.world_override = true;
    }
}

// The 19 posed joint world positions, by RiderJoint index -- the argument the
// pure render::rider_cg takes.
std::array<glm::vec3, kRiderJointCount> posed_joint_pos(const SledModel& sm) {
    std::array<glm::vec3, kRiderJointCount> p{};
    for (int j = 0; j < kRiderJointCount; ++j)
        p[static_cast<std::size_t>(j)] = wpos(sm.nodes[sm.rj[j]].world);
    return p;
}

// ★★★ R4a THE ARMING RUNG -- THE PIN, and why the chain is NEVER "primed at
// bind".
//
// render/body_chain.h §1.2 states the rule this code exists to satisfy: PRIME
// THE BODY CHAIN WHEN IT ARMS, IN THE POSE IT ARMS IN -- never seated at bind,
// because the bind pose is ILLEGAL against the seat keep-out ON PURPOSE (he is
// sitting, his backside is inside the seat solid) and the least-penetration
// exit would take his pelvis out through the nearest face.
//
// ★ AND THE OBVIOUS READING OF THAT RULE IS A TRAP. "Prime it at the instant
// it arms" primes it SEATED -- because the moment the stage weight first
// leaves zero he is still on the machine; the weight only reaches 1 in free
// fall. A one-shot prime at the arming edge is a prime in exactly the pose the
// rule forbids.
//
// So the chain is not primed at an edge at all. WHILE THE STAGE WEIGHT IS
// ZERO THE CHAIN IS PINNED TO THE DRAWN MAN, every frame, and it is RELEASED
// when the stage arms. Then:
//   - "the pose it arms in" is exact and automatic, with no edge to detect
//     and no threshold to choose;
//   - the keep-out only starts biting as the chain starts to move, which is
//     when he is already leaving the seat -- the illegal bind overlap is never
//     integrated;
//   - and it is REVERSIBLE, which SUDBURIAN_LADDER §7.3 requires of stages
//     0-3: the weight falls back to zero and the chain re-pins to the man.
//
// THE PIN ITSELF is a bake, not a guess. Each of the 17 measured stations is
// projected onto the rest-pose POLYLINE of the rider's own joints (lateral
// pairs collapsed to their mean, the same collapse the rider-load model makes)
// and stored as (segment, t). At runtime the SAME (segment, t) is evaluated on
// the POSED polyline. A station therefore follows the drawn man through every
// pose the R3 ladder can strike, and nothing here is a second opinion about
// where his knee is.
struct BodyPinPath {
    // The polyline the stations are pinned to, rest and posed. 12 nodes:
    // grips, hand, forearm, upperarm, spine_03/02/01, pelvis, thigh, calf,
    // foot, and ONE virtual node past the foot so the toe station -- which
    // lies beyond the last joint -- has a segment to sit on instead of being
    // clamped onto the ankle (which would shorten the chain by a boot and let
    // the distance pass yank it straight on the first released step).
    static constexpr int kNodes = 12;
};

int body_pin_nodes_rest(const SledModel& sm, glm::vec3* out) {
    if (sm.n_grip[0] < 0 || sm.n_grip[1] < 0) return 0;
    auto j = [&](int a) { return wpos(sm.rest_world[sm.rj[a]]); };
    auto mid = [&](int a, int b) { return 0.5f * (j(a) + j(b)); };
    out[0] = 0.5f * (wpos(sm.rest_world[sm.n_grip[0]]) +
                     wpos(sm.rest_world[sm.n_grip[1]]));
    out[1] = mid(kRiderHandL, kRiderHandR);
    out[2] = mid(kRiderForearmL, kRiderForearmR);
    out[3] = mid(kRiderUpperarmL, kRiderUpperarmR);
    out[4] = j(kRiderSpine3);
    out[5] = j(kRiderSpine2);
    out[6] = j(kRiderSpine1);
    out[7] = j(kRiderPelvis);
    out[8] = mid(kRiderThighL, kRiderThighR);
    out[9] = mid(kRiderShinL, kRiderShinR);
    out[10] = mid(kRiderFootL, kRiderFootR);
    const glm::vec3 last = out[10] - out[9];
    const float ll = glm::length(last);
    out[11] = out[10] + (ll > 1.0e-5f ? last / ll : glm::vec3(0, 0, 1)) * 0.30f;
    return BodyPinPath::kNodes;
}

int body_pin_nodes_posed(const SledModel& sm, glm::vec3* out) {
    if (sm.n_grip[0] < 0 || sm.n_grip[1] < 0) return 0;
    auto j = [&](int a) { return wpos(sm.nodes[sm.rj[a]].world); };
    auto mid = [&](int a, int b) { return 0.5f * (j(a) + j(b)); };
    out[0] = 0.5f * (wpos(sm.nodes[sm.n_grip[0]].world) +
                     wpos(sm.nodes[sm.n_grip[1]].world));
    out[1] = mid(kRiderHandL, kRiderHandR);
    out[2] = mid(kRiderForearmL, kRiderForearmR);
    out[3] = mid(kRiderUpperarmL, kRiderUpperarmR);
    out[4] = j(kRiderSpine3);
    out[5] = j(kRiderSpine2);
    out[6] = j(kRiderSpine1);
    out[7] = j(kRiderPelvis);
    out[8] = mid(kRiderThighL, kRiderThighR);
    out[9] = mid(kRiderShinL, kRiderShinR);
    out[10] = mid(kRiderFootL, kRiderFootR);
    const glm::vec3 last = out[10] - out[9];
    const float ll = glm::length(last);
    out[11] = out[10] + (ll > 1.0e-5f ? last / ll : glm::vec3(0, 0, 1)) * 0.30f;
    return BodyPinPath::kNodes;
}

// The segment's own frame, built IDENTICALLY at bake and at runtime from the
// SAME polyline (rest there, posed here) so an offset expressed in it means the
// same thing in both. e = along the segment; the other two are Gram-Schmidt'd
// off the model's lateral axis, with a fallback for the degenerate case where a
// segment runs laterally (no station's does, which is why the fallback is a
// guard and not a branch anyone relies on).
void body_pin_frame(const glm::vec3& a, const glm::vec3& c, glm::vec3* e,
                    glm::vec3* f, glm::vec3* g) {
    const glm::vec3 d = c - a;
    const float l = glm::length(d);
    *e = l > 1.0e-6f ? d / l : glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 ref(1.0f, 0.0f, 0.0f);
    if (std::fabs(glm::dot(*e, ref)) > 0.99f) ref = glm::vec3(0.0f, 0.0f, 1.0f);
    glm::vec3 ff = ref - *e * glm::dot(ref, *e);
    const float fl = glm::length(ff);
    *f = fl > 1.0e-6f ? ff / fl : glm::vec3(0.0f, 0.0f, 1.0f);
    *g = glm::cross(*e, *f);
}

// Bake: for every station, the nearest point on the REST polyline.
void capture_body_pin(SledModel& sm) {
    sm.body_pin_ok = false;
    glm::vec3 rest[BodyPinPath::kNodes];
    if (body_pin_nodes_rest(sm, rest) != BodyPinPath::kNodes) return;
    const std::array<BodyChainStation, kBodyChainStations>& st =
        body_chain_stations();
    for (int i = 0; i < kBodyChainStations; ++i) {
        const glm::vec3 q = st[static_cast<std::size_t>(i)].rest_model;
        float best = 1.0e9f;
        int bseg = 0;
        float bt = 0.0f;
        for (int s = 0; s + 1 < BodyPinPath::kNodes; ++s) {
            const glm::vec3 e = rest[s + 1] - rest[s];
            const float ee = glm::dot(e, e);
            float t = ee > 1.0e-9f ? glm::dot(q - rest[s], e) / ee : 0.0f;
            t = std::min(1.0f, std::max(0.0f, t));
            const float dd = glm::length(q - (rest[s] + e * t));
            if (dd < best) {
                best = dd;
                bseg = s;
                bt = t;
            }
        }
        sm.body_pin_seg[i] = bseg;
        sm.body_pin_t[i] = bt;
        sm.body_pin_res[i] = best;
        const glm::vec3 base =
            rest[bseg] + (rest[bseg + 1] - rest[bseg]) * bt;
        glm::vec3 e, f, g;
        body_pin_frame(rest[bseg], rest[bseg + 1], &e, &f, &g);
        const glm::vec3 r = q - base;
        sm.body_pin_off[i] =
            glm::vec3(glm::dot(r, e), glm::dot(r, f), glm::dot(r, g));
    }
    sm.body_pin_ok = true;
}

// Evaluate the pin on the POSED skeleton: where each station's particle should
// sit if the chain is still riding the drawn man.
void body_pin_eval(const SledModel& sm, glm::vec3* out) {
    glm::vec3 posed[BodyPinPath::kNodes];
    if (body_pin_nodes_posed(sm, posed) != BodyPinPath::kNodes) return;
    for (int i = 0; i < kBodyChainStations; ++i) {
        const int s = sm.body_pin_seg[i];
        glm::vec3 e, f, g;
        body_pin_frame(posed[s], posed[s + 1], &e, &f, &g);
        const glm::vec3& o = sm.body_pin_off[i];
        out[i] = posed[s] + (posed[s + 1] - posed[s]) * sm.body_pin_t[i] +
                 e * o.x + f * o.y + g * o.z;
    }
}

// ★★★ R4a THE ARMING RUNG -- THE STAGE SELECTOR, RENDER-SIDE.
//
// docs/SESSION_HANDOFF_20260827_r4a_bodychain.md §7 item 1. The rod model is
// render/rider_load.* and is SHARED with tools/sled_probe.cpp (one model, one
// set of published numbers). What this function bakes is its REST half -- the
// masses, the pitch inertia and the three contact stations -- off `rest_world`,
// exactly as the probe bakes it off the GLB's own rest node transforms. Same
// stations, same map, so the game and the instrument describe the same rider.
//
// ⚠ IT MUST RUN OFF `rest_world`, NEVER OFF THE POSED NODES. The probe's model
// is a REST-pose bake and the lean `d` is what carries every pose change into
// the solve; baking a posed rider here would double-count the lean and there
// would be nothing red to say so.
//
// ★ THE TWO REFERENCES ARE MEASURED, NOT TYPED. seat_ref / board_ref are this
// model's OWN at-rest split -- level gravity, no acceleration, no lean. THE LAW
// (paid for seven times in this repo): a constant that describes the shipped
// table stops describing it the moment the table moves. So there is no
// constant: re-export the rider and both references follow him.
void capture_rider_load(SledModel& sm, double rider_mass_kg, double mass_kg,
                        double cg_height_m) {
    sm.rl_ok = false;
    if (sm.n_pelvis < 0 || sm.n_grip[0] < 0 || sm.n_grip[1] < 0) return;
    if (!(rider_mass_kg > 0.0) || !(mass_kg > 0.0)) return;
    std::array<glm::vec3, kRiderJointCount> jp{};
    for (int j = 0; j < kRiderJointCount; ++j) {
        if (sm.rj[j] < 0) return;
        jp[static_cast<std::size_t>(j)] = wpos(sm.rest_world[sm.rj[j]]);
    }
    const double drop = cg_height_m - static_cast<double>(kSagDefaultM);
    RiderLoadModel& M = sm.rl;
    M = RiderLoadModel{};
    M.m_r = rider_mass_kg;
    M.rider_frac = rider_mass_kg / mass_kg;
    M.cg_body = rider_load_to_body(glm::dvec3(rider_cg(jp)), drop);
    // I_pitch about body +X through the rider CG, parallel-axis over the SAME
    // segment table the probe uses. I_own is a SLENDER ROD -- a LOWER bound.
    for (const RiderSegment& s : rider_segments()) {
        const glm::dvec3 a = rider_load_to_body(
            glm::dvec3(jp[static_cast<std::size_t>(s.prox)]), drop);
        const glm::dvec3 b = rider_load_to_body(
            glm::dvec3(jp[static_cast<std::size_t>(s.dist)]), drop);
        const double m = static_cast<double>(s.mass_frac) * M.m_r;
        const glm::dvec3 c = a + static_cast<double>(s.cg_frac) * (b - a);
        const glm::dvec3 dd = c - M.cg_body;
        const glm::dvec3 seg = b - a;
        const double L = glm::length(seg);
        const glm::dvec3 u = L > 1e-9 ? seg / L : glm::dvec3(0.0);
        const double base = m * L * L / 12.0;
        M.I_pitch += base * (1.0 - u.x * u.x) +
                     m * (dd.y * dd.y + dd.z * dd.z);
    }
    // The three stations, lateral pairs collapsed to their mean.
    M.grip_body = rider_load_to_body(
        0.5 * (glm::dvec3(wpos(sm.rest_world[sm.n_grip[0]])) +
               glm::dvec3(wpos(sm.rest_world[sm.n_grip[1]]))),
        drop);
    M.boot_body = rider_load_to_body(
        0.5 * (glm::dvec3(jp[kRiderFootL]) + glm::dvec3(jp[kRiderFootR])),
        drop);
    M.seat_body = rider_load_to_body(glm::dvec3(jp[kRiderPelvis]), drop);

    const RiderLoad rest = rider_load_solve(
        M, glm::dvec3(0.0), glm::dvec3(0.0), glm::dvec3(0.0),
        glm::dvec3(0.0, -kRiderLoadG, 0.0), glm::dvec3(0.0));
    sm.rl_seat_ref = rest.seat_frac;
    sm.rl_board_ref = rest.board_frac;
    // A rider who is not resting on both supports at rest has no ladder to
    // come off, and normalising by ~0 would arm the chain on the first frame.
    sm.rl_ok = rest.rcase == kRiderSeated && sm.rl_seat_ref > 1.0e-3 &&
               sm.rl_board_ref > 1.0e-3;
    if (!sm.rl_ok) {
        TraceLog(LOG_WARNING,
                 "R4a: rider load model REFUSED (case %s, seat %.4f board "
                 "%.4f) -- the body chain stays OFF",
                 rider_case_name(rest.rcase), sm.rl_seat_ref,
                 sm.rl_board_ref);
        return;
    }
    TraceLog(LOG_INFO,
             "R4a: rider load model baked -- m_r %.1f kg, I_pitch %.3f kg m2, "
             "rest split seat %.4f / board %.4f",
             M.m_r, M.I_pitch, sm.rl_seat_ref, sm.rl_board_ref);
}

// ★ THE POSE-SPRING STIFFNESS, and it is CHAD'S DIAL, read once.
//
// SEADS_BODY_POSE_HZ=<hz> overrides it live; 0 turns the spring OFF and off is
// the bare flailing chain he rejected, kept as the A/B arm rather than deleted.
//
// ★ THE PULL-OUT LADDER, MEASURED (test_body_chain.cpp case 8): how far a
// 40 m/s^2 jolt held for 0.1 s drags the TOE off the pose, on the real 16-link
// measured body --
//
//     0 Hz  1.703 m   <- the bare chain. THE FIREHOSE HE REJECTED.
//     1 Hz  0.285 m   <- SHIPPED: a readable trail, the body still his
//     2 Hz  0.085 m
//     4 Hz  0.026 m
//     8 Hz  0.002 m   <- rigid; the legs may as well be welded
//
// 1 Hz is the rung that best matches his own words -- "he keeps a recognisable
// body and the legs trail behind it" -- and it is a MEASURED rung, not a value
// interpolated between two. It is still PROVISIONAL: he drives it, and the
// whole ladder is one env var away.
//
// ⚠ IT IS NOT MEASURED OFF HIM, only off the jolt. Nobody self-passes a feel
// dial (CLAUDE.md: one dial at a time, Chad flies each).
float body_pose_hz() {
    static const float hz = [] {
        const char* e = std::getenv("SEADS_BODY_POSE_HZ");
        return e != nullptr ? static_cast<float>(std::atof(e)) : 1.0f;
    }();
    return hz;
}

// ★★★ THE ARMING WEIGHT -- SUDBURIAN_LADDER §7.3 stages 0-3, and it is a
// CONTINUOUS, REVERSIBLE function of the ruled selector with NO THRESHOLD IN
// IT AT ALL.
//
//     u_seat  = 1 - min(1, seat_load_frac  / seat_ref)     stage 1 UNWEIGHTED
//     u_board = 1 - min(1, board_load_frac / board_ref)    stage 2 BOARDS FREE
//     arm     = u_seat * u_board                           stage 3 SUPERMAN
//
// Chad's ruling (R4A_THROW_RULING §1, §3 item 2): "seat_load_frac /
// board_load_frac are promoted from diagnostics to the stage-1/2/3 selector",
// and stages 0-3 are "continuous and reversible". The product is 1 exactly at
// CASE F -- nothing below carries, which IS "the body extends behind the
// anchor" -- and 0 whenever either support still carries its resting share.
// Nothing in between is a dial anybody chose.
//
// ★ MEASURED over the tape corpus (`seads_sled_probe stagesel <tape>`):
//   - 0.00 % occupancy at EVERY level on every replayable tape, settle
//     excluded. The rod model never leaves CASE S on Chad's driving, because
//     the corpus has essentially no airtime (griphold's own PEAK context reads
//     air_s = 0.000). So this cannot false-arm on anything he has driven --
//     which is exactly SUDBURIAN_LADDER §7.9's "release must be rare".
//   - ⚠ AND THEREFORE THE CORPUS CANNOT PROVE IT FIRES. The liveness is proved
//     synthetically instead, on the one axis that unweights a rider: sweeping
//     a_body.y from a hard landing to FREE FALL gives arm 0.00 / 0.04 / 0.16 /
//     0.36 / 0.64 / 1.00 at 0 / -0.2 / -0.4 / -0.6 / -0.8 / -1.0 g. The probe
//     prints that ladder before it touches a tape, and test_rider_load pins it.
//     A table of zeros is what a DEAD arm and an unexercised one both look
//     like; they are told apart here, deliberately.
float rider_stage_arm_here(const SledModel& sm, const RiderLoad& L) {
    if (!sm.rl_ok) return 0.0f;
    return rider_stage_arm(L.seat_frac, L.board_frac, sm.rl_seat_ref,
                           sm.rl_board_ref);
}

// ★★★ R3-WS. The rest captures the ladder needs, all DERIVED off `rest_world`
// (so a re-export moves them and the asset tests notice) plus the seat profile
// baked in render/rider_pose.h. Must run AFTER capture_rest_ik.
void capture_weight_shift(SledModel& sm) {
    if (sm.n_pelvis < 0) return;
    sm.ws_pelvis_rest = wpos(sm.rest_world[sm.n_pelvis]);
    sm.ws_sit_h = sm.ws_pelvis_rest.y - seat_top_y(sm.ws_pelvis_rest.z);
    sm.ws_rho0 = 0.0f;
    for (int s = 0; s < 2; ++s) {
        if (sm.arm[s][0] >= 0) {
            sm.ws_shoulder_off[s] =
                wpos(sm.rest_world[sm.arm[s][0]]) - sm.ws_pelvis_rest;
            sm.ws_arm_dmax[s] = sm.arm_len[s][0] + sm.arm_len[s][1] - 1e-3f;
            const float d = glm::length(wpos(sm.rest_world[sm.arm[s][2]]) -
                                        wpos(sm.rest_world[sm.arm[s][0]]));
            if (sm.ws_arm_dmax[s] > 1e-6f)
                sm.ws_rho0 = std::fmax(sm.ws_rho0, d / sm.ws_arm_dmax[s]);
        }
        if (sm.leg[s][0] >= 0) {
            sm.ws_foot_rest[s] = wpos(sm.rest_world[sm.leg[s][2]]);
            sm.ws_deck_knee_x[s] =
                std::fabs(wpos(sm.rest_world[sm.leg[s][1]]).x);
        }
    }
    // ★★★ R3-WS(c) / A9. THE NECK COUNTER'S SIGN IS MEASURED, NEVER TYPED --
    // the standing rule this asset has already broken three times. The probe
    // is the pure TU's (neck_up_sign): rotate the rest head about model +X and
    // read which way it goes. The frame the counter is conjugated into is
    // neck_01's PARENT's rest world rotation, so the delivered rotation is
    // about MODEL +X whatever the rig's rest frames turn out to be.
    if (sm.n_neck >= 0 && sm.n_head >= 0) {
        sm.ws_neck_up_sign = neck_up_sign(wpos(sm.rest_world[sm.n_neck]),
                                          wpos(sm.rest_world[sm.n_head]));
        const int np = sm.nodes[sm.n_neck].parent;
        glm::mat3 nr = np >= 0 ? glm::mat3(sm.rest_world[np]) : glm::mat3(1.0f);
        for (int c = 0; c < 3; ++c) nr[c] = glm::normalize(nr[c]);
        sm.ws_neck_parent_q = glm::quat_cast(nr);
    }

    // The KNEELING hip height, solved at the ladder's own rear station from
    // the anatomical knee-flexion limit -- see render/rider_pose.h for why
    // that is the number that separates "sitting back toward the heels" from
    // "upright on his knees", and why neither is a taste dial.
    const float z_rear = sm.ws_pelvis_rest.z - kLadderPelvisAftM;
    const float thigh = sm.leg[0][0] >= 0 ? sm.leg_len[0][0] : 0.453f;
    const float shin = sm.leg[0][0] >= 0 ? sm.leg_len[0][1] : 0.455f;
    sm.ws_kneel_h = kneel_sit_height(z_rear, sm.ws_pelvis_rest.x + 0.095f,
                                     thigh, shin);
    TraceLog(LOG_INFO,
             "SLED: R3-WS captures -- pelvis rest z %+.4f y %+.4f, sit_h %.4f, "
             "kneel_h %.4f (rear z %+.4f), rho0 %.4f, arm dmax %.4f/%.4f, "
             "neck up sign %+.0f",
             static_cast<double>(sm.ws_pelvis_rest.z),
             static_cast<double>(sm.ws_pelvis_rest.y),
             static_cast<double>(sm.ws_sit_h),
             static_cast<double>(sm.ws_kneel_h),
             static_cast<double>(z_rear), static_cast<double>(sm.ws_rho0),
             static_cast<double>(sm.ws_arm_dmax[0]),
             static_cast<double>(sm.ws_arm_dmax[1]),
             static_cast<double>(sm.ws_neck_up_sign));
    // ★★★ R3-WS(d) / SPEC 13.2.1 + 13.4 D3. THE SHARD GUARD, MEASURED AND
    // PRINTED at every load: what the anatomical limit asked for, what the
    // floor raised it to, and the flexion + chord the SOLVED hip height
    // actually delivers on the shipped seat. The rung's own law is that the
    // guard is judged by the artefact -- this row is what tells the reader
    // which number was in charge.
    {
        const glm::vec3 hip(sm.ws_pelvis_rest.x + 0.095f,
                            seat_top_y(z_rear) + sm.ws_kneel_h, z_rear);
        const KneelPose kp = kneel_construct(hip, thigh, shin);
        const float chord = kp.ok ? glm::length(kp.foot - hip) : 0.0f;
        TraceLog(LOG_INFO,
                 "SLED: R3-WS(d) SHARD GUARD -- anatomical chord %.4f (flex "
                 "%.1f deg), floor %.4f, SOLVED chord %.4f -> flexion %.1f "
                 "deg; kneel hip y %+.4f, knee (%+.4f %+.4f) ankle (%+.4f "
                 "%+.4f), knee above seat %.1f mm",
                 static_cast<double>(kneel_chord_m(thigh, shin)),
                 static_cast<double>(kKneeFlexMaxRad * 57.29578f),
                 static_cast<double>(kKneelChordFloorM),
                 static_cast<double>(chord),
                 static_cast<double>(kneel_flex_rad(thigh, shin, chord) *
                                     57.29578f),
                 static_cast<double>(hip.y), static_cast<double>(kp.knee.z),
                 static_cast<double>(kp.knee.y), static_cast<double>(kp.foot.z),
                 static_cast<double>(kp.foot.y),
                 static_cast<double>((kp.knee.y - seat_top_y(kp.knee.z)) *
                                     1000.0f));
    }
}

// ★★★ R3-WS. Resolve one tick of the ladder. Pure in (the rest captures, ONE
// SledRig, the already-posed machine nodes) -- the grip sockets it reads depend
// only on `steer`, never on the rider.
//
// ★★ AND IT DELIBERATELY DOES NOT SEE THE NEWTON'S ROOT SHIFT. An earlier
// version fed it the live `d`, which closed a feedback loop -- the reach solve
// moved the trunk, the trunk moved the CG, the CG moved `d`, `d` moved the
// reach solve -- and the fixed-Jacobian Newton stopped contracting: measured
// CG_y residual 83.1 mm and a worst arm ratio of 1.6720 at an aft demand of
// only 0.043 m, both artefacts of the loop rather than of the pose. The ladder
// is therefore resolved ONCE per solve against a CLOSED-FORM stand reference
// (`lean_seed` at zero fore-aft demand), exactly the way R1c resolves its own
// hinge angle once from lean_fwd_m and then holds it while the Newton runs.
WsState ladder_state(const SledModel& sm, const SledRig& rig,
                     float u_override, float k_slo = kKneelSLo,
                     float k_shi = kKneelSHi) {
    WsState ws;
    const float a_m = std::fmax(0.0f, -rig.lean_fwd_m);
    // ★ THE A/B ARM. SEADS_SLED_WS_OFF=1 forces the whole ladder inactive, so
    // the same sweep can measure HEAD's numbers on this binary instead of
    // asking a reader to trust a number taken on another build. Off by
    // default, one getenv, read once.
    static const bool ws_off = std::getenv("SEADS_SLED_WS_OFF") != nullptr;
    if (ws_off) return ws;
    // ★ 0-OFF. One branch, and everything downstream is inside it.
    if (!(a_m > 0.0f) && u_override < 0.0f) return ws;
    if (sm.n_pelvis < 0) return ws;
    ws.active = true;
    const float rise = stand_rise_m();
    ws.s = rise > 1e-6f
               ? std::fmax(0.0f, std::fmin(1.0f, rig.lean_up_m / rise))
               : 0.0f;
    ws.u = u_override >= 0.0f
               ? u_override
               : (sm.ws_baked ? ws_ladder_u(sm.ws_curve, a_m, ws.s) : 0.0f);
    // ★ R3-WS(d) / D1. The kneel's stand band is threaded through as an
    // argument -- defaulted to the shipped constants -- so the load-time band
    // sweep below can MEASURE candidates on the real pose path instead of
    // being rebuilt against a remembered number, exactly as R3-WS(c) swept the
    // foot and flexion bands.
    const WeightShiftWeights w = ws_weights(ws.u, ws.s, k_slo, k_shi);
    // ★★★★ R3-WS(e). kKneelRuntimeGain is **0** -- CHAD'S RULING 2026-08-21:
    // "not make kneeling for longitudinal movement". The kneel LEAVES the
    // fore-aft ladder, which now ends at the deep seated rear crouch, and is
    // RESERVED for the R4-SIDE "P" key (side hang). Nothing below is deleted:
    // the whole kneel branch, its chord floor, its trunk cap, its s-band and
    // the antiparallel bend-normal fix stay wired and gated so R4-SIDE
    // inherits them. See render/rider_pose.h at kKneelRuntimeGain and
    // docs/SUDBURIAN_LADDER.md "R4-SIDE". The line is unchanged; only the
    // constant moved -- as it did (the other way) in R3-WS(d).
    ws.w_kneel = w.kneel * kKneelRuntimeGain;
    ws.w_stand = w.stand;
    ws.w_deck = std::fmin(
        1.0f, std::fmax(0.0f, ws.u / kFootBlendHiU));  // linear ramp, C0 at 0
    {   // C1 smoothstep, same band
        const float t = ws.w_deck;
        ws.w_deck = t * t * (3.0f - 2.0f * t);
    }
    ws.w_reach = ws_reach_gate(ws.u, ws.s);
    // ★★★ R3-WS(c). The FULL runner slide, on its own schedule -- NOT the
    // pelvis's, which is the whole of Chad's first complaint.
    ws.foot_slide = ws_foot_slide(ws.u, ws.s);

    // The pelvis with NO ladder retreat yet: rest, plus the CLOSED-FORM stand
    // reference, the lateral channel and the absorb crouch. The pelvis origin
    // is moved only by translations, so this is exact and needs no pose pass --
    // and being closed form is what keeps the ladder out of the Newton's loop.
    const RiderLeanShift ref = lean_seed(sm.hinge, 0.0f, 0.0f, rig.lean_up_m);
    const glm::vec3 p0 = sm.ws_pelvis_rest +
                         glm::vec3(rig.lean_lat_m,
                                   ref.up - kAbsorbDropM * rig.absorb,
                                   ref.fwd);

    // ★ SPEC 4, THE HARD LADDER LIMIT, and the whole station solve lives in
    // the PURE TU (render::ws_solve_station) because this file has no test.
    // Here we only supply the live geometry.
    ReachArm arm[2];
    for (int s = 0; s < 2; ++s) {
        if (sm.arm[s][0] < 0 || sm.n_grip[s] < 0) continue;
        arm[s].shoulder_off = sm.ws_shoulder_off[s];
        arm[s].hand = wpos(sm.nodes[sm.n_grip[s]].world * sm.hand_off[s]);
        arm[s].dmax = sm.ws_arm_dmax[s];
    }
    // ★ THE NO-REGRESSION CEILING. The ladder may never leave an arm worse off
    // than the SAME cell leaves it with the ladder off, and must sit under
    // kArmRatioLimit wherever that ladder-free pose already does. It is
    // measured right here -- the un-retreated pelvis at zero flexion -- so the
    // bound is a measurement, not a remembered number from another build.
    ws.ratio_head = reach_pair_ratio(p0, arm[0], arm[1], 0.0f);
    const float ceiling = std::fmax(kArmRatioLimit, ws.ratio_head);

    // ★ THE ACTIVATION GATE, AND THE MEASUREMENT THAT PUT IT THERE. rho0 is the
    // REST arm ratio, but at full stand the arms are ALREADY past it (the
    // ladder-free ratio reaches 1.4292 with steer and lateral live) -- so an
    // ungated reach solve fires a large theta the instant a_m leaves 0, which
    // measured as a step: the crouch drops CG_y, the vertical solve lifts the
    // root, the board-pinned legs stretch, and the LEG ratio jumped
    // 1.1246 -> 1.3133 at a_m = 0.009 m. Gating theta on the same band the
    // foot blend uses makes the whole ladder ramp in together, and keeps 0-OFF
    // structural rather than numerical.
    ws.rho = ws_rho(ws.u, ws.s, sm.ws_rho0);
    // ★★★ R3-WS(c). Two changes, both in the pure TU: the retreat is the
    // A1 eye-dial (kSeatRetreatAftM) instead of the 0.320 kneel anchor, and
    // the standing branch carries SPEC 11.2.3's squat drop -- "his butt comes
    // down a little as he bends his knees some".
    const WsStation st = ws_solve_station(
        p0, sm.ws_pelvis_rest.z, 1.0f - ws.w_stand,
        ws.w_kneel * (sm.ws_kneel_h - sm.ws_sit_h) +
            ws_squat_drop(ws.u, ws.w_stand),
        arm[0], arm[1], ws_want_aft(ws.u, ws.w_kneel), ws.rho, ws.w_reach,
        ceiling,
        // ★ R3-WS(d) / SPEC 13.2.2: the trunk ceiling BLENDS to the kneel's
        // own 55 deg by w_kneel. At w_kneel = 0 this is exactly
        // kHeadUpTrunkMaxRad, so every seated and standing cell -- and all of
        // 0-OFF -- is bit-identical to R3-WS(c).
        ws_trunk_cap(ws.w_kneel));
    ws.aft_m = st.aft_m;
    ws.dy_m = st.dy_m;
    ws.theta_r = st.theta_rad;
    ws.ratio = st.ratio;
    ws.shortened = st.shortened;
    // ★ SPEC 11.2.4: the head-up counter follows whatever the capped trunk
    // actually took, so at theta_r = 0 it is exactly 0 (the 0-OFF law).
    ws.neck_rad = ws.theta_r;
    // The geometric cap the ROOT translation must also respect: the reach
    // limit measured at the station we ended up at.
    ws.aft_cap_m = std::fmax(kSeatRetreatAftM, ws_want_aft(1.0f, ws.w_kneel));
    for (int s = 0; s < 2; ++s) {
        if (sm.arm[s][0] < 0 || sm.n_grip[s] < 0) continue;
        ws.aft_cap_m = std::fmin(
            ws.aft_cap_m,
            reach_max_aft(p0 + glm::vec3(0.0f, ws.dy_m, 0.0f),
                          sm.ws_shoulder_off[s], arm[s].hand,
                          kArmRatioLimit * sm.ws_arm_dmax[s]));
    }
    ws.aft_cap_m = std::fmax(ws.aft_cap_m, ws.aft_m);
    const glm::vec3 pel = p0 + glm::vec3(0.0f, ws.dy_m, -ws.aft_m);

    // The kneel construction, per side, in the leg's own x plane (SPEC 3c).
    if (ws.w_kneel > 0.0f && sm.leg[0][0] >= 0) {
        ws.kneel_ok = true;
        for (int s = 0; s < 2; ++s) {
            const float hx = wpos(sm.nodes[sm.leg[s][0]].world).x;
            const KneelPose k = kneel_construct(glm::vec3(hx, pel.y, pel.z),
                                                sm.leg_len[s][0],
                                                sm.leg_len[s][1]);
            if (!k.ok) {   // SPEC 3c.2: fail LOUD, never clamp silently
                ws.kneel_ok = false;
                TraceLog(LOG_WARNING,
                         "SLED R3-WS: kneel sphere missed the seat at u %.3f "
                         "(hip %+.4f %+.4f %+.4f) -- kneel branch disabled",
                         static_cast<double>(ws.u), static_cast<double>(hx),
                         static_cast<double>(pel.y),
                         static_cast<double>(pel.z));
                break;
            }
            ws.knee[s] = k.knee;
            ws.foot[s] = k.foot;
            const glm::vec3 hip(hx, pel.y, pel.z);
            ws.knee_rel[s] = k.knee - hip;
            ws.foot_rel[s] = k.foot - hip;
        }
    }
    return ws;
}

// ★★★ R3-WS(d) / SPEC 13.4 D1 -- CONTINUITY IN **s**, THE AXIS THE PLAYER
// OWNS WITH A KEY.
//
// Every continuity gate this rung shipped steps in `a` at FIXED stand, and the
// kneel's kill band lives entirely on the OTHER axis: w_kneel dies across
// s in [kKneelSLo, kKneelSHi] while the kernel's own lean slew moves `s` by up
// to stand_slew_ds_per_tick() in ONE 60 Hz tick. So the whole kneel could
// unwind in two or three frames on the player's stand key and not one green
// gate would have seen it.
//
// The measurement is the same quantity SPEC 7.5 bounds -- the worst joint
// WORLD delta the pose takes for one runtime step of an input -- taken on the
// real pose path at the u values where the kneel is live. Fixed u, because
// that is what D1 asks for and because it isolates the s axis: the demand `a`
// the player is holding does not change while he stands up.
struct WsSlewProbe {
    float worst = 0.0f;
    float at_s = 0.0f;
    float at_u = 0.0f;
};

WsSlewProbe ws_continuity_in_s(SledModel& sm, float k_slo, float k_shi) {
    WsSlewProbe out;
    const float ds = stand_slew_ds_per_tick();
    // Sample the s axis eight times finer than the tick, so the worst tick is
    // found wherever it starts, not only on a lattice of tick boundaries.
    const int steps = 8 * 64;
    for (const float u : {0.8f, 1.0f}) {
        for (int i = 0; i <= steps; ++i) {
            const float s0 = static_cast<float>(i) / static_cast<float>(steps);
            const float s1 = std::fmin(1.0f, s0 + ds);
            if (!(s1 > s0)) continue;
            std::array<glm::vec3, kRiderJointCount> jp[2];
            for (int k = 0; k < 2; ++k) {
                SledRig probe{};
                probe.lean_fwd_m = -0.001f;   // any negative: u is overridden
                probe.lean_up_m = stand_rise_m() * (k == 0 ? s0 : s1);
                const RiderLeanShift ref =
                    lean_seed(sm.hinge, 0.0f, 0.0f, probe.lean_up_m);
                const WsState ws =
                    ladder_state(sm, probe, u, k_slo, k_shi);
                pose_pass(sm, probe, 0.0f, 0.0f, ref.fwd, ref.up, ws);
                jp[k] = posed_joint_pos(sm);
            }
            float dj = 0.0f;
            for (int j = 0; j < kRiderJointCount; ++j)
                dj = std::fmax(dj, glm::length(jp[1][std::size_t(j)] -
                                               jp[0][std::size_t(j)]));
            if (dj > out.worst) {
                out.worst = dj;
                out.at_s = s0;
                out.at_u = u;
            }
        }
    }
    return out;
}

// ★★★ R3-WS. BAKE C_s(u): how much CG the ladder's SHAPE carries aft, measured
// by running the REAL pose pass (no second forward-kinematics model, nothing
// that can drift from the shipped path -- SPEC 5). Runs once, at load.
void bake_weight_shift(SledModel& sm) {
    if (sm.n_pelvis < 0) return;
    SledRig probe{};
    probe.lean_fwd_m = -0.001f;  // any negative value: u is overridden below
    const WsState off;
    sm.ws_monotone_ok = true;
    float slice_slope[kWsBakeS]{};
    float slice_cont[kWsBakeS]{};
    int slice_cont_at[kWsBakeS]{};
    for (int si = 0; si < kWsBakeS; ++si) {
        probe.lean_up_m =
            stand_rise_m() * static_cast<float>(si) / (kWsBakeS - 1);
        // ★ THE BAKE MUST STAND WHERE THE RUNTIME STANDS. `ladder_state` places
        // the pelvis against the CLOSED-FORM stand reference, so the bake has
        // to pose against the SAME reference or the two curves describe two
        // different riders -- measured as a 40 mm CG_y residual and a 321 mm
        // knee error when the bake was taken at zero root shift.
        const RiderLeanShift ref =
            lean_seed(sm.hinge, 0.0f, 0.0f, probe.lean_up_m);
        pose_pass(sm, probe, 0.0f, 0.0f, ref.fwd, ref.up, off);
        const glm::vec3 base_cg = rider_cg(posed_joint_pos(sm));
        std::array<glm::vec3, kRiderJointCount> prev_jp{};
        for (int ui = 0; ui < kWsBakeU; ++ui) {
            const float u = static_cast<float>(ui) / (kWsBakeU - 1);
            const WsState ws = ladder_state(sm, probe, u);
            pose_pass(sm, probe, 0.0f, 0.0f, ref.fwd, ref.up, ws);
            const std::array<glm::vec3, kRiderJointCount> jp =
                posed_joint_pos(sm);
            const glm::vec3 cg = rider_cg(jp);
            sm.ws_curve[si][ui] = base_cg.z - cg.z;
            sm.ws_curve_y[si][ui] = cg.y - base_cg.y;
            // R3-WS(b). The joint travel this bake step costs, EVERY joint
            // (feet and shins included -- they are where the weld releases).
            // Divided by the CG the same step earns, it is the pose delta per
            // metre of aft demand, which is the thing SPEC 7.5 bounds.
            float dj = 0.0f;
            if (ui > 0)
                for (int j = 0; j < kRiderJointCount; ++j)
                    dj = std::fmax(dj, glm::length(jp[std::size_t(j)] -
                                                   prev_jp[std::size_t(j)]));
            sm.ws_cell_dj[si][ui] = dj;
            static const bool cell_debug =
                std::getenv("SEADS_WS_CELLS") != nullptr;
            if (cell_debug)
                TraceLog(LOG_INFO,
                         "WS CELL s=%.2f u=%.4f C %.4f dj %.4f aft %.4f dy "
                         "%+.4f theta %.1f slide %.4f",
                         static_cast<double>(si) / (kWsBakeS - 1),
                         static_cast<double>(u),
                         static_cast<double>(sm.ws_curve[si][ui]),
                         static_cast<double>(dj),
                         static_cast<double>(ws.aft_m),
                         static_cast<double>(ws.dy_m),
                         static_cast<double>(ws.theta_r * 57.29578f),
                         static_cast<double>(ws.foot_slide));
            prev_jp = jp;
        }
        // ★★★ R3-WS(b). THE CURVE-SHAPE GATE, AND IT IS HARD NOW.
        //
        // This used to be a LOG_WARNING, and R3-WS shipped with it firing: the
        // s = 1 slice dipped to -0.0195 m, the inverse's envelope turned that
        // dip into a flat spot, and a flat spot in C is an INFINITE jump in u.
        // The pose popped on stand entry and every gate in the rung missed it,
        // because every one of them stepped in `u` -- the coordinate the jump
        // lives underneath. A warning nobody reads is not a gate.
        //
        // So: the slope floor is enforced, at load, on the SHIPPED bake, and a
        // violation stops the program. It cannot be "measured and reported"
        // -- what it produces is a pose that snaps under the player's hands,
        // and shipping that quietly is the failure mode this project keeps
        // paying for. The bound is SPEC 7.5's own 0.06 m of joint travel per
        // demand step (kWsContinuityBoundM), not a taste dial -- and it is
        // measured on the posed skeleton, not on a slope proxy: see
        // render/rider_pose.h for why the proxy failed in both directions.
        const float slope = ws_min_slope(sm.ws_curve[si], kWsBakeU);
        slice_slope[si] = slope;
        sm.ws_slope_min = si == 0 ? slope : std::fmin(sm.ws_slope_min, slope);
        if (!ws_monotone(sm.ws_curve[si], kWsBakeU)) sm.ws_monotone_ok = false;
        // The continuity budget, in the RUNTIME coordinate, cell by cell.
        const float step =
            static_cast<float>(sim::SledParams{}.lean_aft_max_m) /
            static_cast<float>(kWsDemandCells);
        for (int ui = 1; ui < kWsBakeU; ++ui) {
            const float dc = sm.ws_curve[si][ui] - sm.ws_curve[si][ui - 1];
            const float cont =
                dc > 0.0f ? sm.ws_cell_dj[si][ui] * step / dc : 1e9f;
            if (cont > slice_cont[si]) {
                slice_cont[si] = cont;
                slice_cont_at[si] = ui;
            }
        }
    }
    {
        char row[640];
        int n = std::snprintf(
            row, sizeof row,
            "SLED R3-WS BAKE SHAPE (stand gate hi(1) %.2f): per stand slice, "
            "min dC/du per 1/32 step and worst pose delta per %.4f m of aft "
            "demand (SPEC 7.5 bound %.3f m):",
            static_cast<double>(kReachGateStandHiU),
            static_cast<double>(sim::SledParams{}.lean_aft_max_m) /
                static_cast<double>(kWsDemandCells),
            static_cast<double>(kWsContinuityBoundM));
        for (int si = 0; si < kWsBakeS; ++si)
            n += std::snprintf(
                row + n, sizeof row - std::size_t(n), "  s=%.2f %.5f/%.4f@%d",
                static_cast<double>(si) / (kWsBakeS - 1),
                static_cast<double>(slice_slope[si]),
                static_cast<double>(slice_cont[si]), slice_cont_at[si]);
        TraceLog(LOG_INFO, "%s", row);
    }
    sm.ws_baked = true;
    {   // ★★★ R3-WS(c) / A2. C PER STAND SLICE, ALWAYS, NOT JUST THE TWO ENDS.
        // The coming aft-clamp rung sizes lean_aft_max_m against these five
        // numbers and SPEC 9.1's old table (0.3277 / 0.320 / 0.2251) is stale
        // the moment this bake changes -- so the bake prints the whole row and
        // the spec is re-filed from it, in the same commit.
        char row[512];
        int n = std::snprintf(row, sizeof row,
                              "SLED R3-WS(c) BAKE C(1) per stand slice:");
        for (int si = 0; si < kWsBakeS; ++si)
            n += std::snprintf(row + n, sizeof row - std::size_t(n),
                               "  s=%.2f %.4f",
                               static_cast<double>(si) / (kWsBakeS - 1),
                               static_cast<double>(sm.ws_curve[si][kWsBakeU - 1]));
        n += std::snprintf(row + n, sizeof row - std::size_t(n),
                           "   (kernel lean_aft_max_m %.3f)",
                           static_cast<double>(sim::SledParams{}.lean_aft_max_m));
        TraceLog(LOG_INFO, "%s", row);
    }
    TraceLog(LOG_INFO,
             "SLED: R3-WS baked ladder CG travel C(1) = %.4f (s 0) .. %.4f "
             "(s 1) m, monotone %d, min dC/du %.5f m per 1/32 step; kernel "
             "lean_aft_max_m %.3f",
             static_cast<double>(sm.ws_curve[0][kWsBakeU - 1]),
             static_cast<double>(sm.ws_curve[kWsBakeS - 1][kWsBakeU - 1]),
             sm.ws_monotone_ok ? 1 : 0, static_cast<double>(sm.ws_slope_min),
             static_cast<double>(sim::SledParams{}.lean_aft_max_m));
    if (std::getenv("SEADS_SLED_WS_DEBUG") != nullptr) {
        for (int si = 0; si < kWsBakeS; ++si) {
            char row[768];
            int n = std::snprintf(row, sizeof row, "WS C[s=%.2f]:",
                                  static_cast<double>(si) / (kWsBakeS - 1));
            for (int ui = 0; ui < kWsBakeU; ++ui)
                n += std::snprintf(row + n, sizeof row - std::size_t(n),
                                   " %.4f",
                                   static_cast<double>(sm.ws_curve[si][ui]));
            TraceLog(LOG_INFO, "%s", row);
            n = std::snprintf(row, sizeof row, "WS Cy[s=%.2f]:",
                              static_cast<double>(si) / (kWsBakeS - 1));
            for (int ui = 0; ui < kWsBakeU; ui += 2)
                n += std::snprintf(row + n, sizeof row - std::size_t(n),
                                   " %+.4f",
                                   static_cast<double>(sm.ws_curve_y[si][ui]));
            TraceLog(LOG_INFO, "%s", row);
        }
    }
    for (int si = 0; si < kWsBakeS; ++si) {
        if (!(slice_slope[si] > 0.0f))
            TraceLog(LOG_FATAL,
                     "SLED R3-WS: the baked ladder curve C_s(u) is FLAT OR "
                     "FALLING at stand slice %d (min dC/du %.5f m per 1/32 "
                     "step). The inverse turns that into a POSE JUMP under the "
                     "player's hands -- the stand-entry pop R3-WS(b) exists to "
                     "kill. Re-time the flexion ramp (ws_reach_gate); do NOT "
                     "soften this gate.",
                     si, static_cast<double>(slice_slope[si]));
        if (!(slice_cont[si] <= kWsContinuityBoundM))
            TraceLog(LOG_FATAL,
                     "SLED R3-WS: the ladder pose is DISCONTINUOUS in the "
                     "runtime coordinate at stand slice %d -- %.4f m of joint "
                     "travel for one %.4f m step of aft demand (u index %d), "
                     "against SPEC 7.5's %.3f m. Re-time the flexion ramp "
                     "(ws_reach_gate); do NOT soften this gate.",
                     si, static_cast<double>(slice_cont[si]),
                     static_cast<double>(sim::SledParams{}.lean_aft_max_m) /
                         static_cast<double>(kWsDemandCells),
                     slice_cont_at[si],
                     static_cast<double>(kWsContinuityBoundM));
    }

    // ★★★ R3-WS(d) / D1. THE CONTINUITY-IN-s GATE, AND THE SWEEP THAT SIZED
    // THE BAND. This is the gate the kneel needed and did not have: the kill
    // band is crossed by the STAND KEY, not by the aft demand, and no gate in
    // this rung stepped that axis. Same 0.06 m bound as SPEC 7.5 -- the bound
    // is never the lever, the band is.
    {
        const float ds = stand_slew_ds_per_tick();
        static const bool band_sweep =
            std::getenv("SEADS_SLED_WS_BAND") != nullptr;
        if (band_sweep) {
            // The candidates, measured on the real pose path rather than
            // predicted. hi is swept with lo pinned at the shipped 0.20: the
            // kneel must still be gone by any real stand, so what is free is
            // how LONG it takes to die, not when it starts.
            char row[640];
            int n = std::snprintf(row, sizeof row,
                                  "SLED R3-WS(d) BAND SWEEP (ds/tick %.4f, "
                                  "bound %.3f): worst joint delta per tick of "
                                  "stand slew, by kKneelSHi:",
                                  static_cast<double>(ds),
                                  static_cast<double>(kWsContinuityBoundM));
            // The FLOOR first: the same measurement with the kneel branch
            // suppressed entirely (a band that is already dead at s = 0). That
            // is R3-WS(c)'s own number for a tick of stand slew, and it is how
            // much of the budget the kneel actually has to spend.
            {
                const WsSlewProbe fl = ws_continuity_in_s(sm, -1.0f, 0.0f);
                n += std::snprintf(row + n, sizeof row - std::size_t(n),
                                   "  KNEEL-OFF floor %.4f@s%.2f;",
                                   static_cast<double>(fl.worst),
                                   static_cast<double>(fl.at_s));
            }
            for (const float hi : {0.40f, 0.60f, 0.70f, 0.80f, 1.00f}) {
                const WsSlewProbe pr = ws_continuity_in_s(sm, kKneelSLo, hi);
                n += std::snprintf(row + n, sizeof row - std::size_t(n),
                                   "  [%.2f,%.2f]:%.4f@s%.2f",
                                   static_cast<double>(kKneelSLo),
                                   static_cast<double>(hi),
                                   static_cast<double>(pr.worst),
                                   static_cast<double>(pr.at_s));
            }
            for (const float lo : {0.00f, 0.10f}) {
                const WsSlewProbe pr = ws_continuity_in_s(sm, lo, 1.0f);
                n += std::snprintf(row + n, sizeof row - std::size_t(n),
                                   "  [%.2f,1.00]:%.4f@s%.2f",
                                   static_cast<double>(lo),
                                   static_cast<double>(pr.worst),
                                   static_cast<double>(pr.at_s));
            }
            TraceLog(LOG_INFO, "%s", row);
        }
        // The FLOOR is measured on this same binary, every load: the ladder
        // with the kneel branch suppressed, i.e. R3-WS(c)'s own number. It is
        // 0.0574 m on the shipped asset -- 96 % of SPEC 7.5's budget spent
        // before the kneel exists (OPEN-R3WS-STANDSLEW). So what this gate
        // bounds is the KNEEL'S OWN CONTRIBUTION, the same no-regression form
        // A6 gave the legs and SPEC 12.3 gave the knees, and the absolute
        // number is printed rather than hidden.
        const WsSlewProbe fl = ws_continuity_in_s(sm, -1.0f, 0.0f);
        const WsSlewProbe pr = ws_continuity_in_s(sm, kKneelSLo, kKneelSHi);
        const float add = pr.worst - fl.worst;
        TraceLog(LOG_INFO,
                 "SLED R3-WS(d) CONTINUITY IN s: worst joint delta %.4f m per "
                 "%.4f of stand slew (one 60 Hz tick) at s %.2f u %.2f, band "
                 "[%.2f, %.2f]; KNEEL-FREE floor %.4f at s %.2f "
                 "(OPEN-R3WS-STANDSLEW, R3-WS(c)'s own, SPEC 7.5 bound "
                 "%.3f); the KNEEL ADDS %+.4f (gate %.3f)",
                 static_cast<double>(pr.worst), static_cast<double>(ds),
                 static_cast<double>(pr.at_s), static_cast<double>(pr.at_u),
                 static_cast<double>(kKneelSLo), static_cast<double>(kKneelSHi),
                 static_cast<double>(fl.worst), static_cast<double>(fl.at_s),
                 static_cast<double>(kWsContinuityBoundM),
                 static_cast<double>(add),
                 static_cast<double>(kWsKneelSlewAddM));
        if (!(add <= kWsKneelSlewAddM))
            TraceLog(LOG_FATAL,
                     "SLED R3-WS(d): the KNEEL adds %.4f m of joint travel to "
                     "one %.4f tick of stand slew (worst %.4f at s %.2f u "
                     "%.2f, kneel-free floor %.4f), against the %.3f m "
                     "no-regression allowance. That is the kill-band pop, on "
                     "the player's own stand key. WIDEN kKneelSHi; do NOT "
                     "soften this gate.",
                     static_cast<double>(add), static_cast<double>(ds),
                     static_cast<double>(pr.worst),
                     static_cast<double>(pr.at_s), static_cast<double>(pr.at_u),
                     static_cast<double>(fl.worst),
                     static_cast<double>(kWsKneelSlewAddM));
    }
}

// ★★★ R3-WS. The ladder's own baked CG_y contribution at this tick, read off
// the same (u, s) cell the fore-aft inverse picked. Bilinear over the s axis,
// linear over the u axis -- the same interpolation ws_ladder_u uses, so the two
// curves can never be read at different points of the ladder.
float ws_declared_cg_y(const SledModel& sm, float a_m, const SledRig& rig) {
    const float rise = stand_rise_m();
    const float s = rise > 1e-6f
                        ? std::fmax(0.0f, std::fmin(1.0f, rig.lean_up_m / rise))
                        : 0.0f;
    const float u = ws_ladder_u(sm.ws_curve, a_m, s);
    const float ts = s * static_cast<float>(kWsBakeS - 1);
    int si = static_cast<int>(ts);
    if (si > kWsBakeS - 2) si = kWsBakeS - 2;
    const float fs = ts - static_cast<float>(si);
    const float tu = u * static_cast<float>(kWsBakeU - 1);
    int ui = static_cast<int>(tu);
    if (ui > kWsBakeU - 2) ui = kWsBakeU - 2;
    const float fu = tu - static_cast<float>(ui);
    const float a = sm.ws_curve_y[si][ui] +
                    fu * (sm.ws_curve_y[si][ui + 1] - sm.ws_curve_y[si][ui]);
    const float b =
        sm.ws_curve_y[si + 1][ui] +
        fu * (sm.ws_curve_y[si + 1][ui + 1] - sm.ws_curve_y[si + 1][ui]);
    return a + fs * (b - a);
}

// ★ §D. THE SOLVE. Poses the rider so that its mass-weighted CG displacement
// along model +Z equals the kernel's `lean_fwd_m` EXACTLY (measured max residual
// 0.499 mm over the reachable set), spending the anatomical hip hinge first and
// carrying the remainder as a root translation.
//
// Cost: 2 + kHingeNewtonSteps pose passes. The final pass leaves the model posed
// and is the one every downstream consumer sees; the skinning pass, which is two
// orders of magnitude more arithmetic, still runs exactly once.
void pose_and_solve_lean(SledModel& sm, const SledRig& rig, float sag0) {
    // 1. the TARGET, now on BOTH axes (R1c). sim/sled.cpp's
    //    `cg_off = (rider_mass_kg/mass)*(-lat, up, -fwd)` displaces the WHOLE
    //    rider mass, so the honesty condition is the same on every axis: the
    //    reference is the CG of the SAME (lat, absorb, steer) pose with NO
    //    fore-aft and NO vertical input at all, plus the kernel metres.
    const WsState ws_off;  // the reference pose is ALWAYS ladder-free
    pose_pass(sm, rig, sag0, 0.0f, 0.0f, 0.0f, ws_off);
    const glm::vec3 base = rider_cg(posed_joint_pos(sm));
    const float target_z = base.z + rig.lean_fwd_m;
    // ★★★ R3-WS -- THE ONE PLACE THIS RUNG TOUCHES THE R1c HONESTY LAW, AND IT
    // IS DECLARED RATHER THAN SILENT.
    //
    // The ladder's vertical is a HARD CONSTRAINT: the butt is ON the seat and
    // the knees are ON the seat. Its shape also moves CG_y -- the rear crouch
    // drops it, folding the legs onto the seat raises it -- and R1c's law would
    // have the root solve UNDO that, which measured out at up to +162 mm of
    // root rise: the man floating a hand's width above the seat he is sitting
    // on, which is a broken visual and the opposite of Chad's "weight low".
    //
    // So the ladder's OWN vertical CG contribution is BAKED (ws_curve_y, the
    // same load-time pass that bakes the fore-aft curve, on the same real pose
    // path) and ADDED TO THE TARGET. The Newton then has nothing to fight, d.up
    // converges to the pure-stand value, and the CG_y difference the ladder
    // introduces is a STATED, MEASURED number (printed by SEADS_SLED_WS_DEBUG)
    // rather than a solver artefact. FORE-AFT honesty is untouched: CG_z is
    // still driven to lean_fwd_m exactly, which is the axis the kernel's aft
    // weight shift actually uses.
    //
    // It is identically 0 for rider_fwd_m >= 0, so the signed R1a/R1c path is
    // bit-identical.
    const float ws_decl_y =
        (sm.ws_baked && rig.lean_fwd_m < 0.0f)
            ? ws_declared_cg_y(sm, std::fmax(0.0f, -rig.lean_fwd_m), rig)
            : 0.0f;
    const float target_y = base.y + rig.lean_up_m + ws_decl_y;

    // 2. the hinge angle, closed form on the captured reduced model, clamped to
    //    the anatomical limits. It is chosen on the fore-aft demand alone --
    //    the POSTURE choice, not the honesty mechanism.
    const float theta = hinge_theta_for(sm.hinge, rig.lean_fwd_m);

    // 3. the residual translation on both axes: one seed, then
    //    kHingeNewtonSteps fixed-Jacobian Newton steps on the TRUE posed CG.
    //    Fixed count, no tolerance loop, no accumulator.
    //
    //    ★ R1c, AND THIS IS THE WHOLE RUNG: because `hinge_cg_dy` is negative
    //    for every forward theta, holding CG_y at `lean_up_m` FORCES `d.up`
    //    positive as soon as the torso hinges. That is the rise. It is not
    //    authored and it is not a second animation channel -- it is what the
    //    vertical honesty constraint demands once the hip flexes, driven by
    //    `lean_fwd_m` through the hinge geometry, and it composes with a real
    //    stand automatically because both live in the ONE target above.
    RiderLeanShift d =
        lean_seed(sm.hinge, theta, rig.lean_fwd_m, rig.lean_up_m);
    // ★★★ R3-WS. The ladder is spent FIRST and the root translation carries
    // whatever it did not -- exactly the hinge-first pattern kHingeDemandShare
    // already ships, one level up. Because the baked C_s(u) is inverted on the
    // aft demand, the residual is ~0 while the demand is inside the ladder's
    // own travel, so the pelvis stays ON the seat instead of being translated
    // off the back of the machine.
    const WsState ws = ladder_state(sm, rig, -1.0f);
    for (int it = 0; it < kHingeNewtonSteps; ++it) {
        pose_pass(sm, rig, sag0, theta, d.fwd, d.up, ws);
        const glm::vec3 cg = rider_cg(posed_joint_pos(sm));
        d = lean_newton_step(sm.hinge, d, cg.z, cg.y, target_z, target_y);
        // ★ SPEC 4 AGAIN, ON THE RESIDUAL. The Newton would happily translate
        // the root past the point where the hands leave the bars. It may not:
        // arm integrity wins, and the CG error that leaves is REPORTED (see
        // SEADS_SLED_WS_DEBUG) rather than paid for with a torn wrist. Aft
        // only -- the signed forward-lean path is untouched.
        // ★ SEADS_SLED_WS_NOCLAMP=1 removes this clamp so the trade can be
        // MEASURED rather than argued: with it off, CG_z is honest to ~1 mm and
        // the arm ratio returns to HEAD's tearing values in the stand+steer
        // corner. Both numbers are in the R3-WS report; the ruling is Chad's.
        static const bool no_clamp =
            std::getenv("SEADS_SLED_WS_NOCLAMP") != nullptr;
        if (ws.active && !no_clamp) {
            const float floor_fwd = ws.aft_m - ws.aft_cap_m;
            if (d.fwd < floor_fwd) d.fwd = floor_fwd;
        }
    }
    pose_pass(sm, rig, sag0, theta, d.fwd, d.up, ws);
    sm.ws_last = ws;
    static const bool ws_debug = std::getenv("SEADS_SLED_WS_DEBUG") != nullptr;
    if (ws_debug && ws.active) {
        const glm::vec3 cg = rider_cg(posed_joint_pos(sm));
        TraceLog(LOG_INFO,
                 "SLED WS: a %.3f s %.2f steer %+.2f -> u %.4f aft %.4f "
                 "(cap %.4f) dy %+.4f slide %.3f theta_r %.1f deg (trunk %.1f "
                 "from vertical) neck %.1f rho %.4f ratio %.4f | "
                 "d %.4f/%.4f resid z %+.2f mm y %+.2f mm",
                 static_cast<double>(-rig.lean_fwd_m),
                 static_cast<double>(ws.s), static_cast<double>(rig.steer),
                 static_cast<double>(ws.u), static_cast<double>(ws.aft_m),
                 static_cast<double>(ws.aft_cap_m),
                 static_cast<double>(ws.dy_m),
                 static_cast<double>(ws.foot_slide),
                 static_cast<double>(ws.theta_r * 57.29578f),
                 static_cast<double>((ws.theta_r + kRestTrunkTiltRad) *
                                     57.29578f),
                 static_cast<double>(std::fmin(ws.neck_rad,
                                               kNeckCounterMaxRad) *
                                     57.29578f),
                 static_cast<double>(ws.rho), static_cast<double>(ws.ratio),
                 static_cast<double>(d.fwd), static_cast<double>(d.up),
                 static_cast<double>((cg.z - target_z) * 1000.0f),
                 static_cast<double>((cg.y - target_y) * 1000.0f));
    }
}

// ★★★ R3-WS EVIDENCE. render/sled_model.cpp is in the raylib-only `seads`
// target and has ZERO test coverage by construction (the pure formulas are all
// in render/rider_pose.cpp and ARE tested). So the numbers that can only be
// measured on the REAL pose path are printed here, off SEADS_SLED_WS_DEBUG,
// exactly the way R2c-5 printed its elbow and wrist. Off by default, one
// getenv, read once, and it runs at load so a screenshot session carries it.
void ws_sweep_report(SledModel& sm) {
    if (std::getenv("SEADS_SLED_WS_DEBUG") == nullptr) return;
    if (sm.n_pelvis < 0) return;
    // The sweep must cover the SAME aft demand with the ladder on and off, or
    // the A/B (SEADS_SLED_WS_OFF) compares two different ranges. Take the
    // larger of the ladder's own travel and the kernel's own clamp.
    const float a_full =
        std::fmax(sm.ws_curve[0][kWsBakeU - 1],
                  static_cast<float>(sim::SledParams{}.lean_aft_max_m));
    struct Worst {
        float v = 0.0f;
        float a = 0.0f, up = 0.0f, st = 0.0f, lat = 0.0f;
        int side = 0;
        void take(float x, float aa, float uu, float ss, float ll, int sd) {
            if (x > v) { v = x; a = aa; up = uu; st = ss; lat = ll; side = sd; }
        }
    };
    Worst arm_head, arm_ladder, leg_head, leg_ladder, regress;
    // ★★★ R3-WS(b). CONTINUITY IS NOW MEASURED ON EVERY STAND SLICE, NOT JUST
    // s = 0. The stand-entry pop lived at s = 1 and this measurement was
    // blind to it: it ran only inside `si == 0 && st == 0 && la == 0`, so the
    // one slice whose baked curve dipped was the one slice never watched. The
    // sweep already steps in the RUNTIME coordinate `a`, which is the other
    // half of what was needed.
    float worst_cont = 0.0f, worst_cont_x = 0.0f, worst_cont_foot = 0.0f;
    int worst_cont_s = 0, worst_cont_foot_s = 0;
    float worst_cont_foot_x = 0.0f;
    float slice_cont[5] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    float worst_knee = 0.0f, worst_seat = 0.0f, max_kneel_w = 0.0f;
    // ★★★ R3-WS(d) / D4(a). NO CELL UNGATED. The R3-WS(c) kneel numbers fired
    // only at w_kneel > 0.99, which left the whole blend region unwatched --
    // and D4 measured that the interesting failure (the foot through the seat)
    // lives exactly there. These are taken on EVERY active cell against the
    // BLENDED reference mix(deck-branch knee, K, w_kneel), which is the
    // identity at w = 0 and the old |solved knee - K| gate at w = 1.
    float worst_blend_knee = 0.0f, worst_blend_w = 0.0f;
    float worst_heel_lift = 0.0f;
    // D4(b): how far the blended BOOT gets inside the seat prism, if at all.
    // Positive = penetration depth below the seat surface at that station.
    float worst_seat_pen = 0.0f, worst_pen_w = 0.0f;
    // D6: the realized knee flexion, at rest and at the worst STEER cell --
    // SPEC-4 shortening moves the hip at steer, and the kneel keeps its solved
    // h, so the chord opens and the flexion is not the one h was solved for.
    float kneel_flex_rest = 0.0f, kneel_flex_worst_steer = 0.0f;
    float kneel_chord_min = 9.0f, kneel_chord_min_steer = 0.0f;
    float kneel_trunk_max = 0.0f;
    // ★★★ R3-WS(c) EVIDENCE (A10 + A7). The head-up cap had NO gate in v1 --
    // it is the headline of this amendment, so it is measured here and pinned
    // in test_rider_pose. Same for the crest (A1), the heel (A5), the knee
    // clearance and the hip->ankle chord (A7), and the foot travel (A4).
    float max_theta = 0.0f, max_theta_a = 0.0f, max_theta_s = 0.0f;
    float max_seat_rise = 0.0f;      // seated dy above the rest pan (crest)
    float min_heel_z = 9.0f;         // the most aft heel the sweep reaches
    float foot_travel[5] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    float min_chord = 9.0f, max_knee_x = 0.0f, min_knee_x = 9.0f;
    std::array<glm::vec3, kRiderJointCount> prev[5]{};
    for (int ui = 0; ui <= kWsDemandCells; ++ui) {
        const float a_m = a_full * static_cast<float>(ui) /
                          static_cast<float>(kWsDemandCells);
        for (int si = 0; si < 5; ++si) {
            for (int st = -1; st <= 1; ++st) {
                for (int la = -1; la <= 1; ++la) {
                    SledRig r{};
                    r.lean_fwd_m = -a_m;
                    r.lean_up_m = stand_rise_m() * static_cast<float>(si) / 4.0f;
                    r.steer = static_cast<float>(st);
                    r.lean_lat_m = 0.15f * static_cast<float>(la);
                    pose_and_solve_lean(sm, r, 0.0f);
                    const std::array<glm::vec3, kRiderJointCount> jp =
                        posed_joint_pos(sm);
                    const WsState& ws = sm.ws_last;
                    for (int sd = 0; sd < 2; ++sd) {
                        if (sm.arm[sd][0] < 0) continue;
                        const glm::vec3 h = wpos(sm.nodes[sm.n_grip[sd]].world *
                                                 sm.hand_off[sd]);
                        const float ar =
                            glm::length(wpos(sm.nodes[sm.arm[sd][0]].world) - h) /
                            sm.ws_arm_dmax[sd];
                        const float lr =
                            glm::length(wpos(sm.nodes[sm.leg[sd][0]].world) -
                                        wpos(sm.nodes[sm.leg[sd][2]].world)) /
                            (sm.leg_len[sd][0] + sm.leg_len[sd][1] - 1e-3f);
                        Worst& wa = ui == 0 ? arm_head : arm_ladder;
                        Worst& wl = ui == 0 ? leg_head : leg_ladder;
                        wa.take(ar, a_m, r.lean_up_m, r.steer, r.lean_lat_m, sd);
                        wl.take(lr, a_m, r.lean_up_m, r.steer, r.lean_lat_m, sd);
                        if (ui > 0 && ws.active)
                            regress.take(ar - ws.ratio_head, a_m, r.lean_up_m,
                                         r.steer, r.lean_lat_m, sd);
                    }
                    if (ws.active) {
                        if (ws.theta_r > max_theta) {
                            max_theta = ws.theta_r;
                            max_theta_a = a_m;
                            max_theta_s = r.lean_up_m;
                        }
                        // SPEC 13.2.4: the crest gate is about the BUTT ON
                        // THE PAN, so it is a SEATED, NON-KNEEL measurement.
                        // The kneel deliberately lifts the hip 0.245 m onto
                        // the rear rise -- reading that as a crest violation
                        // would be gating the kneel with the seated rule.
                        if (si == 0 && st == 0 && la == 0 &&
                            ws.w_kneel < 1e-4f)
                            max_seat_rise = std::fmax(max_seat_rise, ws.dy_m);
                        for (int sd = 0; sd < 2; ++sd) {
                            if (sm.leg[sd][0] < 0) continue;
                            const glm::vec3 ft =
                                wpos(sm.nodes[sm.leg[sd][2]].world);
                            const glm::vec3 hp =
                                wpos(sm.nodes[sm.leg[sd][0]].world);
                            const glm::vec3 kn =
                                wpos(sm.nodes[sm.leg[sd][1]].world);
                            // R3-WS(d). THE DECK GATES ARE FOR THE DECK.
                            // A5 heel limit, A4 travel and A7 knee band all
                            // describe a boot STANDING ON THE RUNNING BOARD.
                            // The kneel puts it on the SEAT, inboard and
                            // 0.35 m higher, so reading those cells here would
                            // report the kneel as a deck violation -- the same
                            // category error SPEC 13.2.4 flags for the
                            // knee-clearance gate. Kneel cells get their own
                            // numbers, below.
                            if (ws.w_kneel < 1e-4f) {
                                min_heel_z = std::fmin(min_heel_z,
                                                       ft.z - kBootHeelBackM);
                                if (st == 0 && la == 0)
                                    foot_travel[si] = std::fmax(
                                        foot_travel[si],
                                        sm.ws_foot_rest[sd].z - ft.z);
                                max_knee_x =
                                    std::fmax(max_knee_x, std::fabs(kn.x));
                                min_knee_x =
                                    std::fmin(min_knee_x, std::fabs(kn.x));
                            }
                            // The CHORD floor is absolute and applies
                            // everywhere -- it is the shard guard, and a
                            // shard does not care which branch tore it.
                            min_chord =
                                std::fmin(min_chord, glm::length(hp - ft));
                        }
                    }
                    if (ws.active && ws.kneel_ok && ws.w_kneel > 1e-4f) {
                        max_kneel_w = std::fmax(max_kneel_w, ws.w_kneel);
                        kneel_trunk_max =
                            std::fmax(kneel_trunk_max, ws.theta_r);
                        for (int sd = 0; sd < 2; ++sd) {
                            const glm::vec3 hipw =
                                wpos(sm.nodes[sm.leg[sd][0]].world);
                            const glm::vec3 knw =
                                wpos(sm.nodes[sm.leg[sd][1]].world);
                            const glm::vec3 ftw =
                                wpos(sm.nodes[sm.leg[sd][2]].world);
                            const KneelPose kp = kneel_construct(
                                hipw, sm.leg_len[sd][0], sm.leg_len[sd][1]);
                            const glm::vec3 kw =
                                kp.ok ? kp.knee : hipw + ws.knee_rel[sd];
                            // The KNEEL-FORM gate, at full kneel only: the
                            // knee is meant to be ON the pan, which is the
                            // OPPOSITE sense of the deck branch's clearance.
                            if (ws.w_kneel > 0.99f) {
                                worst_knee = std::fmax(
                                    worst_knee, glm::length(knw - kw));
                                worst_seat = std::fmax(
                                    worst_seat,
                                    std::fabs(kw.y - (seat_top_y(kw.z) +
                                                      kKneePadRM)));
                                // D6: the flexion the pose ACTUALLY folded to.
                                const float ch = glm::length(hipw - ftw);
                                const float fl = kneel_flex_rad(
                                    sm.leg_len[sd][0], sm.leg_len[sd][1], ch);
                                if (st == 0 && la == 0)
                                    kneel_flex_rest =
                                        std::fmax(kneel_flex_rest, fl);
                                if (fl > kneel_flex_worst_steer) {
                                    kneel_flex_worst_steer = fl;
                                    kneel_chord_min_steer =
                                        static_cast<float>(st);
                                }
                                kneel_chord_min =
                                    std::fmin(kneel_chord_min, ch);
                            }
                            // D4(a): EVERY ACTIVE CELL. What is measured
                            // across the whole blend is the hip->ankle CHORD
                            // -- the shard quantity -- and the HEEL LIFT the
                            // guard had to spend to hold it. The literal
                            // blended-POSITION form of D4(a) is gated in
                            // test_rider_pose.cpp, where both branch poses
                            // cost nothing to compute; this file has no test
                            // TU, so what it does here is MEASURE and print.
                            const float ch_b = glm::length(hipw - ftw);
                            if (worst_blend_knee == 0.0f ||
                                ch_b < worst_blend_knee) {
                                worst_blend_knee = ch_b;
                                worst_blend_w = ws.w_kneel;
                            }
                            // The heel lift is only meaningful where the
                            // kneel is the pose. Down in the blend the
                            // construction is being evaluated at a hip that
                            // is still on the DECK, where the seat solve is
                            // degenerate by design and its output is
                            // multiplied by a near-zero weight.
                            if (ws.w_kneel > 0.99f)
                                worst_heel_lift =
                                    std::fmax(worst_heel_lift,
                                              kp.ok ? kp.heel_lift_m : 0.0f);
                            // D4(b): DOES THE ROUTED BOOT CLEAR THE SEAT?
                            // The seat is a SOLID, not a half-space: it spans
                            // |x| <= kSeatXHalfM, z in [kSeatZRearM,
                            // kSeatZFrontM] and y in [kSeatYBottomM,
                            // seat_top_y(z)]. A boot on the running board is
                            // 128 mm BELOW that solid, which is where the feet
                            // have always been -- so penetration is the depth
                            // of the sole inside the box, and it is zero
                            // unless the boot is genuinely in it. Reported as
                            // the smallest escape distance, so a boot barely
                            // inside reads small and one buried reads deep.
                            const float sole = ftw.y - kAnkleAboveSoleM;
                            if (std::fabs(ftw.x) <= kSeatXHalfM &&
                                ftw.z >= kSeatZRearM && ftw.z <= kSeatZFrontM &&
                                sole > kSeatYBottomM &&
                                sole < seat_top_y(ftw.z)) {
                                const float pen =
                                    std::fmin(seat_top_y(ftw.z) - sole,
                                              sole - kSeatYBottomM);
                                if (pen > worst_seat_pen) {
                                    worst_seat_pen = pen;
                                    worst_pen_w = ws.w_kneel;
                                }
                            }
                        }
                    }
                    if (st == 0 && la == 0) {
                        if (ui > 0) {
                            float mx = 0.0f;
                            for (int j = 0; j < kRiderJointCount; ++j) {
                                const float dj =
                                    glm::length(jp[std::size_t(j)] -
                                                prev[si][std::size_t(j)]);
                                const bool foot =
                                    j == kRiderFootL || j == kRiderFootR ||
                                    j == kRiderShinL || j == kRiderShinR;
                                if (foot) {
                                    if (dj > worst_cont_foot) {
                                        worst_cont_foot = dj;
                                        worst_cont_foot_s = si;
                                        worst_cont_foot_x = a_m;
                                    }
                                } else {
                                    mx = std::fmax(mx, dj);
                                }
                            }
                            slice_cont[si] = std::fmax(slice_cont[si], mx);
                            if (mx > worst_cont) {
                                worst_cont = mx;
                                worst_cont_x = a_m;
                                worst_cont_s = si;
                            }
                        }
                        prev[si] = jp;
                    }
                }
            }
        }
    }
    TraceLog(LOG_INFO,
             "SLED WS SWEEP arm: LADDER cells (a>0) worst %.4f at a %.3f up "
             "%.2f steer %+.0f lat %+.2f side %d | HEAD cells (a=0, FROZEN by "
             "0-OFF) worst %.4f at up %.2f steer %+.0f lat %+.2f",
             static_cast<double>(arm_ladder.v),
             static_cast<double>(arm_ladder.a),
             static_cast<double>(arm_ladder.up),
             static_cast<double>(arm_ladder.st),
             static_cast<double>(arm_ladder.lat), arm_ladder.side,
             static_cast<double>(arm_head.v), static_cast<double>(arm_head.up),
             static_cast<double>(arm_head.st), static_cast<double>(arm_head.lat));
    TraceLog(LOG_INFO,
             "SLED WS SWEEP no-regression: worst (ladder ratio - the SAME "
             "cell's ladder-free ratio) %+.4f at a %.3f up %.2f steer %+.0f "
             "lat %+.2f side %d",
             static_cast<double>(regress.v), static_cast<double>(regress.a),
             static_cast<double>(regress.up), static_cast<double>(regress.st),
             static_cast<double>(regress.lat), regress.side);
    TraceLog(LOG_INFO,
             "SLED WS SWEEP leg: LADDER worst %.4f at a %.3f up %.2f steer %+.0f "
             "lat %+.2f | HEAD worst %.4f at up %.2f steer %+.0f lat %+.2f",
             static_cast<double>(leg_ladder.v),
             static_cast<double>(leg_ladder.a),
             static_cast<double>(leg_ladder.up),
             static_cast<double>(leg_ladder.st),
             static_cast<double>(leg_ladder.lat),
             static_cast<double>(leg_head.v), static_cast<double>(leg_head.up),
             static_cast<double>(leg_head.st), static_cast<double>(leg_head.lat));
    TraceLog(LOG_INFO,
             "SLED WS SWEEP kneel: |solved knee - K| %.4f mm, |K - seat| %.4f "
             "mm, w_kneel reached %.3f",
             static_cast<double>(worst_knee * 1000.0f),
             static_cast<double>(worst_seat * 1000.0f),
             static_cast<double>(max_kneel_w));
    // ★★★ R3-WS(d). THE FOUR NUMBERS THE KNEEL-ON RUNG IS JUDGED BY.
    TraceLog(LOG_INFO,
             "SLED WS(d) SWEEP kneel: trunk theta_r %.2f deg (kneel cap %.2f, "
             "head-up cap %.2f) -> %.2f deg FROM VERTICAL; realized flexion "
             "rest %.1f deg, worst-steer %.1f deg (steer %+.0f, limit %.1f); "
             "min hip->ankle chord at full kneel %.4f m (floor %.3f)",
             static_cast<double>(kneel_trunk_max * 57.29578f),
             static_cast<double>(kKneelTrunkMaxRad * 57.29578f),
             static_cast<double>(kHeadUpTrunkMaxRad * 57.29578f),
             static_cast<double>((kneel_trunk_max + kRestTrunkTiltRad) *
                                 57.29578f),
             static_cast<double>(kneel_flex_rest * 57.29578f),
             static_cast<double>(kneel_flex_worst_steer * 57.29578f),
             static_cast<double>(kneel_chord_min_steer),
             static_cast<double>(kKneeFlexMaxRad * 57.29578f),
             static_cast<double>(kneel_chord_min),
             static_cast<double>(kKneelChordFloorM));
    TraceLog(LOG_INFO,
             "SLED WS(d) SWEEP blend (D4): MIN hip->ankle chord over the WHOLE "
             "blend %.4f m at w_kneel %.3f (shard chord 0.197, floor %.3f); "
             "worst heel lift the guard spent %.4f m; worst BOOT penetration "
             "of the seat prism %.4f m at w_kneel %.3f (<= 0 = clear)",
             static_cast<double>(worst_blend_knee),
             static_cast<double>(worst_blend_w),
             static_cast<double>(kKneelChordFloorM),
             static_cast<double>(worst_heel_lift),
             static_cast<double>(worst_seat_pen),
             static_cast<double>(worst_pen_w));
    {
        // ★★★ R3-WS(d) / SPEC 13.4 D3. THE LEVER, PRE-COMPUTED AND **OFFERED**,
        // NOT APPLIED. Chad rejected a 79 deg trunk once already (seated, v1),
        // and the guarded kneel lands there again -- so the alternative he
        // would have to rule on is measured HERE, on the shipped reach solve,
        // rather than guessed at in a report. Moving the KNEEL's own station
        // forward (kLadderPelvisAftM the ANCHOR does not move) buys trunk
        // angle and costs ladder CG; these three rows are that trade.
        char row[640];
        int n = std::snprintf(row, sizeof row,
                              "SLED WS(d) D3 LEVER (kneel station -> trunk, "
                              "OFFERED not applied):");
        ReachArm arm[2];
        for (int sd = 0; sd < 2; ++sd) {
            if (sm.arm[sd][0] < 0 || sm.n_grip[sd] < 0) continue;
            arm[sd].shoulder_off = sm.ws_shoulder_off[sd];
            arm[sd].hand = wpos(sm.rest_world[sm.n_grip[sd]] * sm.hand_off[sd]);
            arm[sd].dmax = sm.ws_arm_dmax[sd];
        }
        for (const float station : {0.320f, 0.280f, 0.240f}) {
            const float z_rear = sm.ws_pelvis_rest.z - station;
            const float h = kneel_sit_height(z_rear,
                                             sm.ws_pelvis_rest.x + 0.095f,
                                             sm.leg_len[0][0], sm.leg_len[0][1]);
            const WsStation st = ws_solve_station(
                sm.ws_pelvis_rest, sm.ws_pelvis_rest.z, 1.0f,
                h - sm.ws_sit_h, arm[0], arm[1], station, kArmRatioLimit, 1.0f,
                kArmRatioLimit, kKneelTrunkMaxRad);
            n += std::snprintf(row + n, sizeof row - std::size_t(n),
                               "  %.3f m -> theta %.1f deg (%.1f from "
                               "vertical), aft taken %.3f, hip lift %.3f;",
                               static_cast<double>(station),
                               static_cast<double>(st.theta_rad * 57.29578f),
                               static_cast<double>((st.theta_rad +
                                                    kRestTrunkTiltRad) *
                                                   57.29578f),
                               static_cast<double>(st.aft_m),
                               static_cast<double>(h - sm.ws_sit_h));
        }
        TraceLog(LOG_INFO, "%s", row);
    }
    TraceLog(LOG_INFO,
             "SLED WS SWEEP continuity IN a (step %.4f m, EVERY stand slice, "
             "bound 0.06): worst %.4f at a %.3f s %.2f; shin/foot %.4f at a "
             "%.3f s %.2f; by slice %.4f %.4f %.4f %.4f %.4f",
             static_cast<double>(a_full /
                                 static_cast<float>(kWsDemandCells)),
             static_cast<double>(worst_cont), static_cast<double>(worst_cont_x),
             static_cast<double>(worst_cont_s) / 4.0,
             static_cast<double>(worst_cont_foot),
             static_cast<double>(worst_cont_foot_x),
             static_cast<double>(worst_cont_foot_s) / 4.0,
             static_cast<double>(slice_cont[0]),
             static_cast<double>(slice_cont[1]),
             static_cast<double>(slice_cont[2]),
             static_cast<double>(slice_cont[3]),
             static_cast<double>(slice_cont[4]));
    TraceLog(LOG_INFO,
             "SLED WS(c) SWEEP head-up: worst theta_r %.2f deg (cap %.2f) at a "
             "%.3f up %.2f -> trunk %.2f deg from vertical; neck counter %.2f "
             "deg (cap %.2f)",
             static_cast<double>(max_theta * 57.29578f),
             static_cast<double>(kHeadUpTrunkMaxRad * 57.29578f),
             static_cast<double>(max_theta_a), static_cast<double>(max_theta_s),
             static_cast<double>((max_theta + kRestTrunkTiltRad) * 57.29578f),
             static_cast<double>(std::fmin(max_theta, kNeckCounterMaxRad) *
                                 57.29578f),
             static_cast<double>(kNeckCounterMaxRad * 57.29578f));
    TraceLog(LOG_INFO,
             "SLED WS(c) SWEEP foot: travel by stand slice %.4f %.4f %.4f "
             "%.4f %.4f m (rear limit %.4f, full slide %.4f); most aft HEEL z "
             "%+.4f vs deck rear %+.4f; seated seat rise %.1f mm (crest gate "
             "10 mm)",
             static_cast<double>(foot_travel[0]),
             static_cast<double>(foot_travel[1]),
             static_cast<double>(foot_travel[2]),
             static_cast<double>(foot_travel[3]),
             static_cast<double>(foot_travel[4]),
             static_cast<double>(kFootZRearLimitM),
             static_cast<double>(sm.ws_foot_rest[0].z - kFootZRearLimitM),
             static_cast<double>(min_heel_z),
             static_cast<double>(kDeckZRearM),
             static_cast<double>(max_seat_rise * 1000.0f));
    TraceLog(LOG_INFO,
             "SLED WS(c) SWEEP legs (A7): min hip->ankle chord %.4f m (floor "
             "0.25), knee |x| in [%.4f, %.4f] (seat half 0.2060 + pad %.4f = "
             "%.4f, deck outer 0.3750)",
             static_cast<double>(min_chord), static_cast<double>(min_knee_x),
             static_cast<double>(max_knee_x), static_cast<double>(kKneePadRM),
             static_cast<double>(0.2060f + kKneePadRM));
    float worst_z = 0.0f, worst_y = 0.0f, worst_decl = 0.0f;
    float ship_z = 0.0f, ship_y = 0.0f;
    const float a_ship = static_cast<float>(sim::SledParams{}.lean_aft_max_m);
    for (int ui = 0; ui <= 32; ++ui) {
        const float a_m = a_full * static_cast<float>(ui) / 32.0f;
        for (int si = 0; si < 5; ++si) {
            SledRig r{};
            r.lean_fwd_m = -a_m;
            r.lean_up_m = stand_rise_m() * static_cast<float>(si) / 4.0f;
            const WsState off;
            pose_pass(sm, r, 0.0f, 0.0f, 0.0f, 0.0f, off);
            const glm::vec3 base = rider_cg(posed_joint_pos(sm));
            const float decl = a_m > 0.0f ? ws_declared_cg_y(sm, a_m, r) : 0.0f;
            pose_and_solve_lean(sm, r, 0.0f);
            const glm::vec3 cg = rider_cg(posed_joint_pos(sm));
            worst_z = std::fmax(worst_z,
                                std::fabs(cg.z - (base.z + r.lean_fwd_m)));
            worst_y = std::fmax(
                worst_y, std::fabs(cg.y - (base.y + r.lean_up_m + decl)));
            worst_decl = std::fmax(worst_decl, std::fabs(decl));
            if (a_m <= a_ship) {
                ship_z = std::fmax(ship_z,
                                   std::fabs(cg.z - (base.z + r.lean_fwd_m)));
                ship_y = std::fmax(
                    ship_y, std::fabs(cg.y - (base.y + r.lean_up_m + decl)));
            }
        }
    }
    TraceLog(LOG_INFO,
             "SLED WS SWEEP cg: over the SHIPPING range (a <= lean_aft_max_m "
             "%.2f) solver residual |CG_z| %.3f mm |CG_y| %.3f mm; over the "
             "whole swept range |CG_z| %.3f mm |CG_y| %.3f mm; the ladder's "
             "DECLARED CG_y departure from the R1c law is at most %.1f mm "
             "(stated, not solved away)",
             static_cast<double>(a_ship),
             static_cast<double>(ship_z * 1000.0f),
             static_cast<double>(ship_y * 1000.0f),
             static_cast<double>(worst_z * 1000.0f),
             static_cast<double>(worst_y * 1000.0f),
             static_cast<double>(worst_decl * 1000.0f));
}

}  // namespace

// ---- MULTIDENT state (cosmetic crash damage; Chad's 'U' ruling) ----------
static int g_helmet_dent = 0;   // 0 = pristine, 1..4 = baked dent variants
// ★ Chad 2026-08-25: the back HUD must be "pasted to his white/grey sweater".
// Published from the SAME posed frame the scarf's §3b torso plane derives --
// no second source of truth for where his back is. Written every frame the
// hero rig poses, and INVALIDATED first so a frame that fails to pose cannot
// leave the HUD stuck on the last good back.
RiderBack g_rider_back;
const RiderBack& sled_model_rider_back() { return g_rider_back; }
// The sweater frame in MODEL space, stashed where the scarf derives it and
// mounted to world once `mount` exists (see sled_model_draw).
bool g_rb_have = false;
glm::vec3 g_rb_pelvis_m{0.0f}, g_rb_neck_m{0.0f};
glm::vec3 g_rb_normal_m{0.0f}, g_rb_width_m{0.0f};
float g_rb_surface = 0.0f;

// ---- F-POSE: while the player MANS THE FLAK GUN the man is AT THE GUN
// (render/flak_gunner.cpp draws him there), so the parked sled must not
// show a second one. Every rider prim is skinned and nothing else on the
// sled is, so `skinned` IS the rider mask. app/main.cpp sets this each
// frame from the manned state; SEADS_FLAK_GUNNER=0 keeps it false forever
// (bit-identical off-arm).
static bool g_rider_hidden = false;
void sled_rider_hide_set(bool hidden) { g_rider_hidden = hidden; }
void sled_helmet_dent_set(int degree) {
    g_helmet_dent = degree < 0 ? 0 : degree > 4 ? 4 : degree;
}
int sled_helmet_dent_get() { return g_helmet_dent; }

bool sled_model_draw(const glm::dvec3& pos, const glm::dmat3& basis,
                     const glm::dvec3& eye, const glm::vec3& sun_dir,
                     double cg_h, const SledRig& rig) {
    SledModel& sm = g_sled;
    g_rb_have = false;          // a frame that fails to pose publishes nothing
    g_rider_back.valid = false;
    // ★ Y cycles the multident: off -> dent 1..4 -> off. It USED to be U, to
    // match the Blender preview binding -- and Chad ruled it off that key on
    // 2026-09-01, "move the cosmetics off the key, it's in game now": U is the
    // PUMP-FIX key in the game-loop lane, and one press beside a dead pump
    // would otherwise both start the fix and dent his helmet.
    //
    // ★ THE TRADE, STATED: what is lost is the correspondence with Blender's
    // own preview key, which is why U was chosen in the first place. What is
    // gained is that a gameplay key beats a preview convenience -- and this
    // cycle is now only a preview at all, because FALLS drive the dent
    // themselves (sled_helmet_dent_set, R4c-3). Mirrored deliberately from the
    // game-loop lane's edit so the two branches carry the same hunk.
    if (IsKeyPressed(KEY_Y)) g_helmet_dent = (g_helmet_dent + 1) % 5;
    if (!sm.tried) {
        sm.tried = true;
        sm.ok = load_model();
    }
    if (!sm.ok) return false;

    // Static visual sag: the .blend is authored ON the ground at static
    // load, so a susp channel equal to the sag draws the authored stance;
    // deviation from it is what moves the parts. Kernel susp_x rest hang is
    // 0.21 m with 0.26 travel; ~0.10 m static compression reads right and
    // SEADS_SLED_SAG0 overrides it for A/B without a rebuild.
    // ★ DEFECT 11: the 0.10f literal is now the named render::kSagDefaultM.
    // Value UNCHANGED -- it is a VISUAL stance dial with no kernel owner (the
    // kernel's own susp_rest_m is 0.21 with 0.26 travel, and the measured
    // static susp_x on a real drive tape is ~0.01 m, so this number is neither
    // of those and must not be quietly single-sourced to one of them).
    // ★ R5 row 9: read through the ONE accessor (render/rider_pose.h
    // sled_sag0_m) — the shadow proxies share the same drop, so the env
    // override moves the mesh and its shadow together, never one of them.
    static const float sag0 = sled_sag0_m();

    // The machine's own axes, once for the whole function: the GLB faces the
    // other way down -Z, so model and body differ by a half turn about Y.
    const glm::mat3 body_from_model_b = glm::mat3(glm::rotate(
        glm::mat4(1.0f), 3.14159265f, glm::vec3(0.0f, 1.0f, 0.0f)));
    const glm::mat3 model_from_body_b = glm::transpose(body_from_model_b);

    // ---- ★★★ R4c §7.3 STAGES 4-7 -- WHERE THE MAN IS, ONCE HE IS OFF ----
    //
    // The KERNEL decides all of it now (`sim/walker.{h,cpp}`): whether he let
    // go, where his free body flew, how deep the snow was, how long it took him
    // to get up, and how fast he can move through it. This block does not
    // decide one of those things. It converts ONE world position into the
    // model-frame vector the pose pass spends, and that is the whole job.
    //
    // ★ THE REFERENCE POINT IS THE MACHINE'S CG PLUS HIS LEAN, and it is a
    // DELTA that is spent, not an absolute position: `flight_off_model` is
    // "where he is" minus "where he would be if he were still holding on", so
    // whatever constant offset separates the drawn root from the kernel CG
    // cancels exactly, and the offset is identically ZERO on the frame he lets
    // go. Nothing jumps at the transition, and nothing here needs to know where
    // the .blend put his hips.
    //
    // ⚠ AND IT TRACKS HIS LEAN ON PURPOSE. The kernel zeroes the lean COMMANDS
    // the moment he is off (sim/sled.cpp §7.4) so his stored displacement slews
    // home over `lean_tau_s`. If the reference did not follow that decay, the
    // decay would drag the departed man sideways with it -- a body moved by an
    // animation of a rider who is not there.
    if (rig.walker != nullptr && rig.walker->mode != sim::WalkerMode::Riding) {
        const glm::dvec3 lean_b(-rig.lean_lat_m, rig.lean_up_m, -rig.lean_fwd_m);
        const glm::dvec3 ref_w = pos + basis * lean_b;
        const glm::dvec3 d_b =
            glm::transpose(basis) * (rig.walker->pos - ref_w);
        sm.flight_off_model = model_from_body_b * glm::vec3(d_b);
        // ★★★ G2e (Chad: "dismount after being capsized -- the sudburian
        // walks floating 5 feet above the snow"): the delta law above is a
        // CONTINUITY law -- zero on the frame he lets go -- but it leaves
        // the drawn man displaced from `walker.pos` by the ROOT'S REST SEAT
        // OFFSET *rotated by the machine's current basis*. Upright, that
        // constant is benign; CAPSIZED, it points the seat offset at the
        // sky and the walking man floats by it. The EXACT placement --
        // root lands on `walker.pos`, machine attitude irrelevant -- is
        //   M*(walker.pos - pos) - rest_root_pos
        // and it is blended in BY MODE, not by distance (the first cut
        // eased on distance-from-release and kept the old error alive
        // within 2 m of the machine -- which is exactly where a man who
        // just tipped his sled WALKS; Chad float-walked 4 feet up beside
        // the capsized machine, twice). The kernel's own ladder is the
        // stateless clock: through Riding/Falling the CONTINUITY delta
        // stands untouched (u = 0, the no-pop release law bit-exact);
        // through Buried the correction ramps in on `t_mode_s` while he is
        // UNDER THE SNOW where no blend can be seen; every later grounded
        // mode (Down / crawls / Afoot -- the ladder is one-way) holds
        // u = 1: the drawn man stands ON `walker.pos`, whatever attitude
        // the machine capsized at. If a shallow landing ever skips Buried,
        // the 0->1 step lands on the impact frame, inside the crash chaos.
        if (sm.n_root >= 0) {
            // ★★★ G2g (Chad: "I still walk a little up in the air"):
            // `walker.pos` is NOT ground -- the kernel pins it at `drive_r
            // + lie_clearance_m` (0.564 m, sim/walker.h), a drawn-origin
            // convention, and the exact anchor must land the ROOT (the
            // ground bone at his feet) on the GROUND UNDER him, not on the
            // convention. This same 0.564 m was ALSO the stubborn clamp
            // duty: the body rode that much high while the foot targets
            // sat on the real ground, a permanent vertical over-reach no
            // pelvis drop could close. One subtraction fixes both, and the
            // number is READ from the kernel's own param, never retyped.
            const glm::dvec3 up_w_anchor =
                glm::normalize(rig.walker->pos);
            const glm::dvec3 ground_w =
                rig.walker->pos -
                up_w_anchor * sim::WalkerParams{}.lie_clearance_m;
            const glm::vec3 exact_off =
                glm::vec3(model_from_body_b *
                          glm::vec3(glm::transpose(basis) *
                                    (ground_w - pos))) -
                glm::vec3(sm.rest_world[sm.n_root][3]);
            float u_exact = 1.0f;
            switch (rig.walker->mode) {
                case sim::WalkerMode::Riding:
                case sim::WalkerMode::Falling:
                    u_exact = 0.0f;
                    break;
                case sim::WalkerMode::Buried:
                    u_exact = std::clamp(
                        static_cast<float>(rig.walker->t_mode_s / 0.5), 0.0f,
                        1.0f);
                    break;
                default:
                    u_exact = 1.0f;
                    break;
            }
            sm.flight_off_model =
                sm.flight_off_model +
                (exact_off - sm.flight_off_model) * u_exact;
        }

        // ★★★ AND HIS OWN FRAME. Stage 2 left him welded to the machine's
        // ATTITUDE while he flew -- a translation and nothing more -- and wrote
        // that down as a debt. A WALK cannot carry that debt: a man whose feet
        // step along the machine's forward while he travels along his own is
        // broken on sight. Two world directions, rotated into model space
        // through the same pair every other term here uses:
        //   UP      = the sphere's outward at HIS position. Not the machine's
        //             +Y -- the machine can be on its roof; he is on the snow.
        //   FORWARD = the heading the KERNEL walks him along, so his feet and
        //             his travel cannot disagree by construction.
        const double hr = glm::length(rig.walker->pos);
        if (hr > 0.0) {
            const glm::dvec3 up_w = rig.walker->pos / hr;
            // ★★★ R4c: AND HE ACTUALLY GOES INTO IT. Chad drove the burial:
            // "im buried in the deep snow" -- the MODE was right and there was
            // nothing to see, because the kernel pins him at
            // `drive_r + lie_clearance_m` and never lowered him. `Buried` drew
            // as a man lying flat, HOVERING half a metre over the snow.
            //
            // He sinks by the snow he is in plus a little, scaled by the
            // kernel's published `submerge` -- so shallow snow structurally
            // cannot swallow him, and the ramp is a shape over a stage the
            // depth already bought rather than a second clock.
            if (rig.walker->submerge > 0.0) {
                sm.flight_off_model +=
                    model_from_body_b *
                    glm::vec3(glm::transpose(basis) *
                              (-up_w * (rig.walker->submerge *
                                        (rig.walker->depth_m + 0.30))));
            }
            // ★★★ GAIT LADDER G2i -- THE SHIFT JUMP, and it rides the SAME
            // world-up channel the burial sink one branch up rides (the sink
            // takes him DOWN by the snow he is in; the hop takes him UP by the
            // height the kernel-side ballistic says he has reached). Render
            // integrates nothing and decides nothing: `sim::step_hop` owns the
            // arc, `sim::step_gait` lifts the feet by the same number, and
            // this is the change of basis that spends it. Identically zero
            // whenever he is standing on the snow.
            if (rig.hop_height_m > 0.0f) {
                sm.flight_off_model +=
                    model_from_body_b *
                    glm::vec3(glm::transpose(basis) *
                              (up_w * static_cast<double>(rig.hop_height_m)));
            }
            sm.flight_up_model = glm::normalize(
                model_from_body_b * glm::vec3(glm::transpose(basis) * up_w));
            glm::dvec3 h_w = rig.walker->heading;
            if (glm::length(h_w) < 0.5)
                h_w = basis * glm::dvec3(0.0, 0.0, -1.0);
            glm::vec3 f_m = glm::normalize(
                model_from_body_b * glm::vec3(glm::transpose(basis) * h_w));
            // Orthogonalise against up, then renormalise: a heading with any
            // radial component left in it would TILT him, and tilt is a thing
            // the snow decides, not a rounding error.
            f_m -= sm.flight_up_model * glm::dot(f_m, sm.flight_up_model);
            if (glm::length(f_m) > 1.0e-6)
                sm.flight_fwd_model = glm::normalize(f_m);
        }

        // The gait, straight off the kernel -- render authors none of it.
        // ★ THE LIFT IS THE SNOW ITSELF: "large stepping with snow coming off"
        // is a foot clearing what it is standing in, so the swing arc's height
        // IS the depth he is in, plus a boot's worth on hardpack where the
        // depth is zero and a foot that never left the ground would drag.
        sm.gait_phase = static_cast<float>(rig.walker->gait_phase);
        sm.gait_stride_m = static_cast<float>(rig.walker->stride_m);
        // ★ THE LIFT IS CAPPED BY WHAT THE LEG CAN REACH, and the cap is the
        // kernel's, not a second opinion: `0.08 + depth` asked for 0.85 m of
        // foot rise in deep snow against a 0.908 m leg chain, which drove the
        // IK into its own clamp -- and a clamped two-bone chain is a rigid
        // strut, i.e. the literal "robot".
        sm.gait_lift_m = static_cast<float>(
            sim::walker_lift(sim::WalkerParams{}, rig.walker->depth_m));
        sm.gait_stance_frac = static_cast<float>(sim::walker_stance_frac(
            sim::WalkerParams{}, glm::length(rig.walker->vel)));
        sm.gait_depth_frac = static_cast<float>(
            sim::walker_depth_frac(sim::WalkerParams{}, rig.walker->depth_m));
        // Arm swing grows with how fast he is ACTUALLY going, not with what the
        // snow would allow -- otherwise he swings at full jog amplitude while
        // still accelerating from a standstill.
        sm.gait_arm_swing = static_cast<float>(
            0.10 + 0.06 * std::min(4.0, glm::length(rig.walker->vel)));
        // ★★★ GAIT LADDER G1: his two feet's WORLD targets, converted into
        // MODEL space the SAME way `flight_off_model` is, three lines above
        // this whole block -- `ref_w` and `model_from_body_b` are the
        // identical pair, so a foot target and the root it stands under can
        // never disagree about which frame they are in. This is the ONLY
        // place gait state is touched outside `sim/gait.cpp`: what happens
        // here is a change of basis, never a decision (L-PURE).
        if (rig.gait != nullptr && sm.n_root >= 0) {
            // ★★★ G1d (Chad's fly: "arms move legs do not"): a world point
            // does NOT map into node space as `M*(P - ref_w)` alone. The
            // ROOT is drawn at `rest_world[n_root] + flight_off_model`
            // (see the n_root channel in pose_pass) -- the whole skeleton
            // is ANCHORED at the root's REST position plus the delta. The
            // first cut dropped that rest anchor, so every foot target
            // carried a constant ~1 m bias off the man; the reach clamp
            // then held both legs at a near-constant direction, and the
            // legs read as rigid struts translating with the body while
            // the arms (walker-channel path) kept swinging. The mapping
            // that agrees with the root, exactly:
            //   model(P) = rest_root_pos + flight_off_model
            //            + M*(P - walker.pos)
            // (the delta is taken off HIS kernel position, in doubles,
            // so it stays small on the 15 km sphere). The decaying
            // lean/absorb root offsets are deliberately not mirrored here:
            // they move the MAN, never the ground he stands on.
            const glm::vec3 root_rest_pos =
                glm::vec3(sm.rest_world[sm.n_root][3]);
            static const bool conv_debug =
                std::getenv("SEADS_GAIT_DEBUG") != nullptr;
            static int conv_n = 0;
            if (conv_debug && (++conv_n % 60) == 0) {
                std::fprintf(stderr,
                             "GAIT conv: v0=%d v1=%d gait_on=%d grip=%d "
                             "t0=(%.2f %.2f %.2f)\n",
                             rig.gait->foot[0].valid ? 1 : 0,
                             rig.gait->foot[1].valid ? 1 : 0,
                             sm.gait_on ? 1 : 0, rig.grip_attached ? 1 : 0,
                             sm.gait_foot_model[0].x, sm.gait_foot_model[0].y,
                             sm.gait_foot_model[0].z);
            }
            // ★ G2g: the delta reference is the WORLD POINT THE ROOT IS
            // ANCHORED TO -- since the anchor law above grounds the root at
            // `walker.pos - up*lie_clearance`, the feet must be expressed
            // off the SAME point or they inherit the 0.564 m convention
            // shift and sink exactly that far under the surface. One
            // reference, both bodies.
            // ★★★ G2i-e (red-team P1): ... and when he JUMPS, the anchor
            // rises WITH him. The hop block above adds `up*hop_height` to
            // `flight_off_model`, i.e. the drawn root is at
            // `walker.pos - up*lie_clearance + up*h` -- so the anchor must be
            // that same point or every gait target draws `h` too high on top
            // of the `h` the sim already spent lifting it (`step_gait`'s
            // `target_w + up*height`): the foot landed 2h up against a hip
            // at h, the tuck read one hop-height deeper than its own dials,
            // and every AirTuck/AirLand number was tuned through the error.
            // Identically the old anchor whenever hop_height is 0.
            const glm::dvec3 gait_anchor_w =
                rig.walker->pos +
                glm::normalize(rig.walker->pos) *
                    (static_cast<double>(rig.hop_height_m) -
                     sim::WalkerParams{}.lie_clearance_m);
            for (int gi = 0; gi < 2; ++gi) {
                const sim::GaitFoot& gf = rig.gait->foot[gi];
                sm.gait_foot_ok[gi] = gf.valid;
                if (!gf.valid) continue;
                const glm::dvec3 d_gb =
                    glm::transpose(basis) * (gf.target_w - gait_anchor_w);
                sm.gait_foot_model[gi] = root_rest_pos + sm.flight_off_model +
                                         model_from_body_b * glm::vec3(d_gb);
                sm.gait_foot_pitch_rad[gi] = static_cast<float>(gf.pitch_rad);
            }
        } else {
            sm.gait_foot_ok[0] = sm.gait_foot_ok[1] = false;
        }
        // ★★★ GAIT LADDER G2: THE PELVIS TERMS, STRAIGHT OFF THE KERNEL --
        // no basis conversion needed, unlike the foot targets three lines
        // up. `pelvis_drop_m` and `bob_m` are magnitudes along an UP axis
        // (the model's own, applied where `pose_pass` builds the pelvis
        // channel) and `sway_m` / `roll_rad` are magnitudes along
        // `left = cross(up, fwd)`; a proper rotation (which is all
        // `model_from_body_b` / `basis` ever compose) carries a cross
        // product's SENSE with it (`sim/gait.h`'s own sign-convention
        // banner), so copying the scalar straight across is exact, not an
        // approximation that happens to work.
        if (rig.gait != nullptr) {
            sm.gait_pelvis_drop_m = static_cast<float>(rig.gait->pelvis_drop_m);
            sm.gait_bob_m = static_cast<float>(rig.gait->bob_m);
            sm.gait_sway_left_m = static_cast<float>(rig.gait->sway_m);
            sm.gait_roll_rad = static_cast<float>(rig.gait->roll_rad);
        } else {
            sm.gait_pelvis_drop_m = sm.gait_bob_m = sm.gait_sway_left_m =
                sm.gait_roll_rad = 0.0f;
        }
        // ★★★ A BELLY CRAWL DOES NOT RUN THE WALK CYCLE. Chad: "legs are not
        // to be animated like a wheel" -- and a man pitched 90 degrees
        // face-down with a walking gait underneath him is the purest form of
        // that. On his belly the legs TRAIL; on his knees they shuffle; only
        // on his feet does he step.
        sm.gait_on = rig.walker->mode == sim::WalkerMode::Afoot ||
                     rig.walker->mode == sim::WalkerMode::CrawlKnees;
        // ★ AND ONCE HE IS ALL THE WAY IN, HE IS NOT DRAWN AT ALL. The sink
        // above does the work and the snow patch occludes him for free (it is
        // opaque depth-tested geometry centred on the eye, and the camera is on
        // him) -- but a boot through a thin patch or a mesh seam would break
        // the one beat this whole stage exists for, so past the point where he
        // is under, the rider prims are skipped outright. It is only ever
        // reached with a burst of snow in the air in front of it.
        sm.hide_rider = rig.walker->submerge > 0.92f;
        // ★★★ AND WHETHER HE IS LYING IN IT. Down and Crawling are both
        // face-down (§7.5's "prone or supine to crawl"); getting to his feet
        // eases it out over the first moments of Afoot rather than snapping
        // him upright, because a discontinuous pose change IS the defect-8 pop
        // §7.8 exists to retire. The ease is a LOOK and it is the only number
        // in this block that is not measured.
        {
            // ★ THE PITCH LADDER IS CHAD'S SEQUENCE, IN DEGREES: flat on his
            // belly (90), up on hands and knees (55 -- torso inclined, not
            // horizontal), then upright. Each stage is a fixed attitude and the
            // LAST one eases, so nothing snaps at the moment he stands.
            float want = 0.0f;
            switch (rig.walker->mode) {
                case sim::WalkerMode::Buried:
                case sim::WalkerMode::Down:
                case sim::WalkerMode::CrawlProne:
                    want = 1.5707964f;  // flat: pi/2 is what LYING DOWN means
                    break;
                case sim::WalkerMode::CrawlKnees:
                    want = 0.96f;  // ~55 deg, up on his hands
                    break;
                case sim::WalkerMode::Afoot:
                    want = 0.96f * std::max(0.0f,
                                            1.0f - static_cast<float>(
                                                       rig.walker->t_mode_s) /
                                                       0.6f);
                    break;
                default:
                    want = 0.0f;
                    break;
            }
            sm.flight_pitch_rad = want;
        }
        // Built HERE, where his up, his forward and his pitch are all known.
        // In the rest pose his up IS model +Y and his forward IS model +Z, so a
        // rotation carrying those two onto HIS up and HIS heading is exactly
        // the orientation he should be drawn at -- composed on top of the rest
        // pose, so every hinge, crouch and lean still means what it meant. The
        // prone pitch rides on the RIGHT, i.e. about model +X, which this same
        // rotation maps onto HIS right: a pitch about his own shoulders
        // whatever way he is facing, and exactly the identity at pitch 0.
        {
            const glm::vec3 u = sm.flight_up_model;
            const glm::vec3 fz = sm.flight_fwd_model;
            const glm::vec3 rx = glm::cross(u, fz);
            sm.flight_q = glm::quat_cast(glm::mat3(rx, u, fz)) *
                          glm::angleAxis(-sm.flight_pitch_rad,
                                         glm::vec3(1.0f, 0.0f, 0.0f));
        }
    } else {
        sm.flight_off_model = glm::vec3(0.0f);
        sm.gait_on = false;
        sm.hide_rider = false;
        sm.gait_foot_ok[0] = sm.gait_foot_ok[1] = false;
        sm.gait_pelvis_drop_m = sm.gait_bob_m = sm.gait_sway_left_m =
            sm.gait_roll_rad = 0.0f;
    }

    // ★★★ ST-5 THE HAND HOOK: the one absolute-world -> model transform the
    // weld needs (see g_w2m). Built from the same four numbers as `mount`
    // further down -- `mount`'s eye-relative translation cancels against the
    // `+ eye` that lands it in absolute world, so this IS that matrix, and its
    // inverse is what turns a launcher grip into a hand target. Computed
    // unconditionally rather than under the weight: four multiplies, against a
    // conditional that would be a second thing to keep in step with `mount`.
    {
        const glm::dmat4 m2w =
            glm::translate(glm::dmat4(1.0), pos) * glm::dmat4(basis) *
            glm::translate(glm::dmat4(1.0),
                           glm::dvec3(0.0, -(cg_h - static_cast<double>(sag0)),
                                      0.0)) *
            glm::rotate(glm::dmat4(1.0), 3.14159265358979323846,
                        glm::dvec3(0.0, 1.0, 0.0));
        g_w2m = glm::inverse(m2w);
    }

    pose_and_solve_lean(sm, rig, sag0);

    // ---- ★★★ R4a: THE STAGE MACHINE, AND THE BODY CHAIN IT ARMS -----------
    // docs/SESSION_HANDOFF_20260827_r4a_bodychain.md §7, items 1-3. Placed
    // HERE: after every pose pass (so the pin below reads the DRAWN man, not a
    // half-posed one) and before the scarf, whose anchor is the neck and would
    // otherwise be a frame stale the day the body drives the spine.
    //
    // NOTHING THE GAME DRAWS CHANGES IN THIS RUNG. The chain is solved and the
    // stage is computed every frame; the drawn rider is NOT yet blended onto
    // the chain frames (that is the next rung, and it is the half that needs
    // Chad's eye). What ships live is the mechanism plus a DEBUG DRAW of it --
    // SEADS_BODY_CHAIN=1 -- so the chain and its DRAWN-SURFACE boxes can be
    // seen resting on, or floating over, the machine.
    {
        // ★ THE SAME PAIR THE STAGE-4 BLOCK ABOVE ALREADY BUILT, hoisted so
        // there is one definition of "which way the model faces" in this
        // function instead of two that can drift.
        // ---- the bake, once, and only when the kernel has supplied a mass
        // budget to bake it from.
        if (!sm.rl_tried && rig.rider_mass_kg > 0.0 && rig.mass_kg > 0.0) {
            sm.rl_tried = true;
            capture_rider_load(sm, rig.rider_mass_kg, rig.mass_kg,
                               rig.cg_height_m);
            capture_body_pin(sm);
            if (sm.rl_ok && sm.body_pin_ok) {
                // The asset half comes from body_chain_fill; everything below
                // is FEEL and is the caller's by that function's own contract.
                body_chain_fill(sm.body_par);
                sm.body_par.iterations = 4;
                // ⚠ MY DERIVATION, NOT A MEASUREMENT OFF HIM (handoff §5.4):
                // rho*Cd*A/(2m) for an 87.5 kg body. The scarf's 0.153 is a
                // RIBBON and would stream a man to 54 deg of lift at 3 m/s.
                sm.body_par.drag_k_per_m = 0.005f;
                // The scarf's, "until a drive says otherwise" -- and the drive
                // that says otherwise has not happened yet.
                sm.body_par.damping = 0.930f;
                // The head sphere and the back planes DISABLE for superman:
                // the chain IS the body, there is nothing to stay out of.
                sm.body_par.head_keepout_r_m = 0.0f;
                sm.body_par.back_keepout_m = 0.0f;
                // ★ Chad's dial, still 0, and it now means what it says: a
                // clearance held off the DRAWN surface, on top of the limb's
                // own measured box.
                sm.body_par.seat_keepout_m = 0.0f;
                sm.body_ok = true;
                TraceLog(LOG_INFO,
                         "R4a: body chain armed-capable -- %d links, %.6f m, "
                         "worst station %.4f m off the joint polyline "
                         "(carried as an offset, so the pin is exact)",
                         sm.body_par.segments,
                         static_cast<double>(body_chain_total_len()),
                         static_cast<double>(*std::max_element(
                             sm.body_pin_res,
                             sm.body_pin_res + kBodyChainStations)));
            }
        }
        if (sm.body_ok) {
            sm.body_par.dt_s = rig.dt_s;
            // ---- THE FRAME FIELD, in BODY axes, because that is the frame
            // the rider-load model works in. ⚠ AND IT CARRIES THE TRANSPORT
            // TERM THE SCARF'S DOES NOT.
            //
            // trail_frame_field differences the COMPONENTS of a vector in a
            // ROTATING frame. The true inertial acceleration is
            //     a = d(v_body)/dt + omega x v_body
            // and the second term is exactly what a component-wise backward
            // difference throws away. For the scarf that omission is inside a
            // look Chad has already driven and signed (2026-08-27), so it is
            // NOT corrected here -- changing it would move a signed feel on a
            // rung that was not asked to. It is a one-dial drive question and
            // it is written down rather than fixed silently.
            //
            // ★ MEASURED, `seads_sled_probe stagesel <tape>`: with the
            // transport term in, the render derivation tracks the kernel's own
            // per-substep a_body to |d seat_frac| p50 0.0002 and max 0.0235
            // over the replayable corpus -- i.e. the game's own selector and
            // the instrument's agree to under 2.4 % of a load fraction, on
            // Chad's real driving, with no debug sink anywhere near the
            // kernel.
            const bool ff_live = sm.body_frame.primed;
            // ★ RE-PRIME ON AN EXTERNAL WRITE, for the same reason the scarf
            // does: the mount seed and KEY_R autoright teleport the machine
            // and differencing across one reads hundreds of g.
            if (sm.body_epoch != rig.epoch) {
                sm.body_epoch = rig.epoch;
                sm.body_frame = TrailFrameTracker{};
                // the filter re-primes with the chain: a respawn must not carry
                // the dead life's stage weight into the fresh one.
                sm.stage_arm_seeded = false;
            }
            const float frame_dt = rig.dt_s * static_cast<float>(rig.ticks);
            const TrailFrameField bf = trail_frame_field(
                sm.body_frame, rig.vel_body_mps, rig.omega_body_rps, frame_dt);
            glm::vec3 a_body(0.0f), al_body(0.0f);
            if (ff_live) {
                a_body = bf.accel_mps2 +
                         glm::cross(rig.omega_body_rps, rig.vel_body_mps);
                al_body = bf.alpha_rps2;
            }
            // Gravity, on a sphere: down is -normalize(position), recomputed
            // this frame, never a fixed axis (CLAUDE.md sphere invariants).
            const glm::vec3 down_w =
                glm::vec3(-glm::normalize(glm::dvec3(pos)));
            const glm::vec3 g_body_v =
                glm::mat3(glm::transpose(basis)) * down_w *
                static_cast<float>(kRiderLoadG);
            // The lean, in the kernel's own body axes -- the same
            // (-lat, up, -fwd) the probe reads off SledState.
            const glm::dvec3 d_lean(-rig.lean_lat_m, rig.lean_up_m,
                                    -rig.lean_fwd_m);
            sm.rl_last = rider_load_solve(
                sm.rl, glm::dvec3(a_body), glm::dvec3(rig.omega_body_rps),
                glm::dvec3(al_body), glm::dvec3(g_body_v), d_lean);
            // ★★★ THE REFERENCE IS LIVE, AND CHAD'S FIRST DRIVE IS WHY.
            //
            // It used to be the model's frozen AT-REST UPRIGHT split, and that
            // armed the chain on ROLL ALONE. Measured (`stagesel`'s roll
            // ladder, no acceleration anywhere in it): 45 deg of roll reads
            // arm 0.086, 60 deg reads 0.250, 90 deg reads a full 1.000. Chad
            // drove it and saw exactly that -- "it goes to orange ... even if
            // tipped over".
            //
            // The cause is the model's OWN documented blindness, which I wrote
            // into render/rider_load.h's header and then built on top of
            // anyway: the supports are FRICTIONLESS and carry only along body
            // +Y, so tipping swings that axis off vertical and the loads fall
            // as cos(roll). The model is right -- a frictionless seat cannot
            // hold a man sideways -- and it is the wrong question. A man lying
            // on his tipped-over machine is not supermanning; by Chad's own
            // ruling (R4A_THROW_RULING §5.6) that is the STAND self-right
            // branch, a different limb of the ladder entirely.
            //
            // So the reference is the SAME solve at the SAME attitude and the
            // SAME lean with the ACCELERATION SET TO ZERO. The weight then
            // measures what it was always meant to -- how much support the
            // machine took away by ACCELERATING -- and not which way down
            // happens to point. Rolled 60 deg at rest: reference 0.3344, live
            // 0.3344, arm 0. Rolled 60 deg in free fall: reference 0.3344,
            // live 0, arm 1. A reference with no support left to lose cannot
            // arm at all (rider_stage_arm returns 0 on a degenerate one).
            //
            // ★ AND IT ADDS NO CONSTANT. The frozen split is still baked, but
            // only as the load-time sanity check it always was.
            const RiderLoad rl_ref = rider_load_solve(
                sm.rl, glm::dvec3(0.0), glm::dvec3(0.0), glm::dvec3(0.0),
                glm::dvec3(g_body_v), d_lean);
            sm.rl_ref = rl_ref;
            sm.body_dbg_accel = glm::length(a_body);
            sm.body_dbg_fd = glm::length(bf.accel_mps2);
            sm.body_dbg_v = glm::length(rig.vel_body_mps);
            sm.body_dbg_w = glm::length(rig.omega_body_rps);
            sm.body_dbg_ticks = rig.ticks;
            sm.body_dbg_tick_total += rig.ticks;
            // ★★★ CHAD'S CONTACT RULING, 2026-08-28. `free_frac` is air_s
            // normalised by the KERNEL'S OWN contact window
            // (SledComfort::rolled_grace_s) -- not a number chosen here. On
            // the ground it is 0 and the chain cannot arm however far he is
            // leaned; off the ground it reaches 1 and he is free at any
            // attitude, including inverted. The whole argument, the two
            // drives it was measured on and the table it reproduces are in
            // render/rider_load.h.
            const double air_free = rider_air_free_frac(
                static_cast<double>(rig.air_s),
                static_cast<double>(rig.air_grace_s));
            sm.stage_arm = rider_stage_arm_free(sm.rl_last.seat_frac,
                                                sm.rl_last.board_frac,
                                                rl_ref.seat_frac,
                                                rl_ref.board_frac, air_free);
            if (!sm.rl_ok) sm.stage_arm = 0.0f;
            // ★★★ AND IT IS LOW-PASSED, BECAUSE THE SPEC ALREADY RULED THAT --
            // AND I SHIPPED THE RAW VALUE ANYWAY.
            //
            // docs/R4A_PHASE0_MODEL.md §5.2, about this very instrument:
            // "grip_capacity_n is compared against grip_load_lp, NEVER against
            // grip_load_n" -- because an instantaneous sample of this model is
            // an artefact of a sampling rate that already differs by 2x between
            // the game and the gate, and "a constant that describes the shipped
            // table stops describing it the moment the table moves."
            //
            // The stage weight is the same instrument wearing a different hat,
            // and the raw value made every suspension impact a momentary
            // superman. `rider_load_lp_step` was MOVED, TESTED (case 5) and
            // then not used -- the moved-consumer trap, on my own rung.
            //
            // The house filter exactly: tau 0.1 s, and stepped on the SUBSTEP
            // (here the sim tick), never on the frame -- so it is frame-rate
            // independent like everything else the chain reads.
            {
                const double raw = static_cast<double>(sm.stage_arm);
                if (!sm.stage_arm_seeded) {
                    sm.stage_arm_lp = raw;
                    sm.stage_arm_seeded = true;
                }
                for (int s = 0; s < rig.ticks; ++s)
                    sm.stage_arm_lp = rider_load_lp_step(
                        sm.stage_arm_lp, raw, static_cast<double>(rig.dt_s));
                sm.stage_arm_raw = sm.stage_arm;
                sm.stage_arm = static_cast<float>(sm.stage_arm_lp);
            }
            // SEADS_BODY_ARM=x forces the stage weight for an A/B that does
            // not need a big enough jump to reach. Unset = the measured
            // selector, which is the shipped path.
            static const float arm_force = [] {
                const char* e = std::getenv("SEADS_BODY_ARM");
                return e != nullptr ? static_cast<float>(std::atof(e)) : -1.0f;
            }();
            if (arm_force >= 0.0f) sm.stage_arm = std::min(1.0f, arm_force);

            // ---- THE PIN / RELEASE (see capture_body_pin's banner).
            glm::vec3 pin[kBodyChainStations];
            for (int i = 0; i < kBodyChainStations; ++i) pin[i] = glm::vec3(0.0f);
            body_pin_eval(sm, pin);
            const glm::vec3 anchor = pin[0];
            // ---- THE DRIVE POLICY LIVES IN THE LIBRARY (render/body_drive.h).
            // Pin-or-release, the spring's target, and the stiffness law are
            // the STAGE MACHINE, not wiring -- and they used to sit right here,
            // in a TU no test links. Six green gate cases coexisted with a
            // drive Chad described as "the legs were going all over
            // erratically" precisely because nothing could execute them. What
            // stays here is what genuinely cannot be gated: reading the rig.
            TrailChainInput bin;
            bin.anchor = anchor;
            bin.wind_mps = -(model_from_body_b * rig.vel_body_mps);
            bin.gravity_dir = glm::normalize(
                model_from_body_b *
                (glm::mat3(glm::transpose(basis)) * down_w));
            bin.width_axis = glm::vec3(1.0f, 0.0f, 0.0f);
            bin.frame_accel_mps2 = model_from_body_b * a_body;
            bin.frame_omega_rps = model_from_body_b * rig.omega_body_rps;
            bin.frame_alpha_rps2 = model_from_body_b * al_body;
            // frame_origin stays 0: body_from_model is a PURE ROTATION so
            // the two origins coincide, and the body origin IS the system
            // CG (sim/sled.h). A lever arm anchored anywhere else is
            // silently zero and looks exactly like a dead term.
            bin.frame_origin = glm::vec3(0.0f);
            // ★ THE SEAT SOLID -- "his legs will hit the seat" (Chad,
            // 2026-08-25). The measured centreline profile render/
            // rider_pose.cpp already carries; this file knows no asset.
            bin.n_seat_samples = kSeatStations;
            bin.seat_z_rear = kSeatZRearM;
            bin.seat_z_front = kSeatZFrontM;
            bin.seat_x_half = kSeatXHalfM;
            bin.seat_y_bottom = kSeatYBottomM;
            const std::array<float, kSeatStations>& prof = seat_profile_y();
            for (int k = 0; k < kSeatStations; ++k)
                bin.seat_top_y[k] = prof[static_cast<std::size_t>(k)];
            int bsteps = rig.ticks;
            if (bsteps > kScarfMaxSubsteps) bsteps = kScarfMaxSubsteps;
            if (bsteps < 0) bsteps = 0;

            BodyDriveIn di;
            di.pin = pin;
            di.n = sm.body_par.segments;
            di.arm = sm.stage_arm;
            di.pose_hz_base = body_pose_hz();
            di.chain_in = bin;
            di.substeps = bsteps;
            di.dt_s = rig.dt_s;
            // ★★★ THE ARMING MEMORY (Chad's ruling 2026-08-29, "the buck has
            // to be remembered, harder the buck the longer the superman").
            // Policy and state live in render/body_drive.*; this is the wiring
            // and the wiring only.
            //
            // ★ IT WENT IN BY §7.7's PROVEN PATTERN, WHICH IS NOW COMPLETE:
            // shipped at gain 0 first (divisor exactly 1.0f, stiffness
            // expression BIT-IDENTICAL -- the same "all 0-OFF bit-identical"
            // proof assist_hull_frac, roll_stiff_vgain and release_floor_frac
            // each went in behind), THEN dialled as its own change once Chad
            // had driven it.  SEADS_BUCK_GAIN=0 still restores that arm
            // bit-identically, which is the A/B and the kill switch.
            // The dial is CALIBRATED, not guessed: his "maybe 10% of jumps"
            // read off his own 44 distinct tapes (tools/sled_probe.cpp,
            // `superman`), then confirmed on the stick.
            // ★★★ DRIVEN AND SIGNED BY CHAD, 2026-08-30: "yea great job, he
            // dosent let go of the bars yet but his legs fly sometimes, just
            // the right amount of novelty suprise."  So 0.02 / 3.0 is now the
            // DEFAULT, not an opt-in -- his drive is the only thing that could
            // license that, and it did.  §7.7's pattern ran to completion:
            // shipped OFF and proven bit-identical first, dialled after the
            // drive as its own change.
            //
            // ⚠ THE PAIR IS SIGNED, NOT THE GAIN ALONE. decay 3.0 is not a
            // free companion to gain 0.02 -- it is set by the POST-LANDING
            // RECOVERY bound (p90 1.03 s over his 44 drives). The arm with the
            // best separation, 0.02/1.0, leaves him limp for 20 s after the
            // worst landing in the corpus. Move one without re-measuring the
            // other and the thing he signed is gone.
            static const float buck_gain = [] {
                const char* e = std::getenv("SEADS_BUCK_GAIN");
                return e != nullptr ? static_cast<float>(std::atof(e)) : 0.02f;
            }();
            static const float buck_decay = [] {
                const char* e = std::getenv("SEADS_BUCK_DECAY");
                return e != nullptr ? static_cast<float>(std::atof(e)) : 3.0f;
            }();
            di.buck.buck_gain = buck_gain;
            di.buck.decay_per_s = buck_decay;
            di.buck_mem = &sm.body_buck;
            // The field the machine is pressing on him with THIS step. It is
            // the same |g_eff| the chain integrates, so the memory and the
            // motion can never disagree about how hard he was hit.
            // g_eff = g - a_frame, computed EXACTLY as the measurement rig
            // computes it (tools/sled_probe.cpp) and from the very vectors this
            // frame's chain integrates -- so the memory and the motion can
            // never disagree about how hard he was hit, and the calibration
            // measured on the probe still describes the shipped code.
            di.g_eff_mag = glm::length(bin.gravity_dir *
                                           sm.body_par.gravity_mps2 -
                                       bin.frame_accel_mps2);
            const BodyDriveOut dout = body_chain_drive(sm.body, sm.body_par, di);
            sm.body_dbg_allow_m = dout.allow_max_m;
        }
    }

    // ---- *** R4a RUNG 2: THE DRAWN LEGS FOLLOW THE CHAIN ---------------
    //
    // LADDER 7.3 stage 2 -- "foot load -> 0; boots leave the running boards;
    // leg IK releases from board_socket" -- then stage 3, "legs trail from the
    // hips as a damped chain."
    //
    // * PLACED HERE, between the chain step and the scarf, and the slot is
    // load-bearing in both directions: AFTER the chain so the targets are this
    // frame's, and BEFORE the scarf so the scarf's own bone overrides are the
    // last word (this block re-runs the settle sweep, which skips overridden
    // nodes -- so it cannot touch the scarf, the arms or the hands).
    //
    // *** AND IT IS GATHER / CALL / SOLVE / SETTLE, WITH NO ARITHMETIC IN IT.
    // This TU is compiled ONLY into the `seads` executable, so ctest cannot
    // execute one line of it. Every decision lives in render/body_blend.*,
    // which is in the LIBRARY and gated. That split is the whole reason the
    // first draft of this rung was thrown away.
    static const bool blend_off = [] {
        const char* e = std::getenv("SEADS_BODY_BLEND");
        return e != nullptr && std::atoi(e) == 0;
    }();
    if (!blend_off && sm.body_ok && sm.body.primed && sm.leg[0][0] >= 0 &&
        sm.leg[1][0] >= 0) {
        // ---- the weight: stage 2's OWN release, on the LIVE reference -----
        // !! The LIVE reference, never the frozen at-rest split. The supports
        // are frictionless and fall as cos(roll), so a frozen reference reads
        // 60 deg of roll on a PARKED machine as half the boards unloaded and
        // peels his boots in the driveway. That is the exact defect Chad's
        // first drive found on the stage weight (handoff 8.1), and the first
        // draft of THIS block re-introduced it on a different signal.
        // ★★★ AND THE SAME CONTACT RULING, because this weight had BOTH
        // halves of the stage weight's defect and nobody had looked. Chad
        // drove the cured stage weight and reported "legs moving flickery
        // erratic" and "the orange frames doing all the flying" -- the chain
        // released and the drawn legs left behind on the boards. Measured: the
        // legs followed on ZERO of the 485 frames he spent inverted in the
        // air. See render/rider_load.h.
        float board_raw = 0.0f;
        if (sm.rl_ok)
            board_raw = static_cast<float>(rider_support_release_free(
                sm.rl_last.board_frac, sm.rl_ref.board_frac,
                rider_air_free_frac(static_cast<double>(rig.air_s),
                                    static_cast<double>(rig.air_grace_s))));
        if (sm.board_rel_epoch != rig.epoch) {
            sm.board_rel_epoch = rig.epoch;
            sm.board_rel_seeded = false;
        }
        if (!sm.board_rel_seeded) {
            sm.board_rel_lp = static_cast<double>(board_raw);
            sm.board_rel_seeded = true;
        } else if (rig.dt_s > 0.0f) {
            sm.board_rel_lp = rider_load_lp_step(
                sm.board_rel_lp, static_cast<double>(board_raw),
                static_cast<double>(rig.dt_s));
        }
        sm.board_release_raw = board_raw;
        sm.board_release = static_cast<float>(sm.board_rel_lp);
        // The A/B handle for the drive: force the release without needing air.
        static const float board_force = [] {
            const char* e = std::getenv("SEADS_BODY_BOARD");
            return e != nullptr ? static_cast<float>(std::atof(e)) : -1.0f;
        }();
        if (board_force >= 0.0f) sm.board_release = std::min(1.0f, board_force);

        // ---- the frames, on the LIVE chain, UNCONDITIONALLY ---------------
        // Called every frame including while pinned, so `last_x` and the debug
        // boxes advance on the same schedule the constraint sees them.
        glm::mat4 bf[kBodyChainStations];
        trail_chain_frames(sm.body, glm::vec3(1.0f, 0.0f, 0.0f), bf);

        // ---- which rig side is the chain table's +X side? MEASURED --------
        // The table's [0] is +X by measured sign. Reading an "_l"/"_r" here
        // would be a guess about a rig convention.
        const int ix0 = wpos(sm.nodes[sm.leg[0][0]].world).x >=
                                wpos(sm.nodes[sm.leg[1][0]].world).x
                            ? 0
                            : 1;
        const int rig_side[2] = {ix0, 1 - ix0};

        BodyBlendIn bi;
        bi.station_p = sm.body.p;
        bi.frame = bf;
        bi.w = sm.board_release;
        for (int b = 0; b < 2; ++b) {
            const int r = rig_side[b];
            bi.hip[b] = wpos(sm.nodes[sm.leg[r][0]].world);
            bi.r3_knee[b] = wpos(sm.nodes[sm.leg[r][1]].world);
            bi.r3_ankle[b] = wpos(sm.nodes[sm.leg[r][2]].world);
            bi.len_thigh[b] = sm.leg_len[r][0];
            bi.len_calf[b] = sm.leg_len[r][1];
        }
        // The SAME seat solid the chain's own keep-out runs, from the same
        // measured constants the chain block feeds `bin` above. The seat is
        // not frame-dependent, so this is a description, not a second model.
        TrailChainInput seat_in;
        seat_in.n_seat_samples = kSeatStations;
        seat_in.seat_z_rear = kSeatZRearM;
        seat_in.seat_z_front = kSeatZFrontM;
        seat_in.seat_x_half = kSeatXHalfM;
        seat_in.seat_y_bottom = kSeatYBottomM;
        {
            const std::array<float, kSeatStations>& prof = seat_profile_y();
            for (int k = 0; k < kSeatStations; ++k)
                seat_in.seat_top_y[k] = prof[static_cast<std::size_t>(k)];
        }
        bi.seat_pr = &sm.body_par;
        bi.seat_in = &seat_in;

        const BodyBlendOut bo = body_blend_legs(bi);

        for (int b = 0; b < 2; ++b) {
            const int r = rig_side[b];
            const BodyBlendLeg& L = bo.leg[b];
            if (!(L.w_eff > 0.0f)) continue;  // stage 0: R3 stands untouched
            // The boot's R3 orientation, which is CARRIED BY THE MACHINE --
            // it spins with the running board. Captured before the re-solve.
            const glm::quat qa =
                glm::quat_cast(glm::mat3(sm.nodes[sm.leg[r][2]].world));
            const glm::vec3 bend_n = body_blend_bend_normal(
                bi.hip[b], L.ankle, L.knee, sm.leg_bend_n[r]);
            solve_chain(sm, sm.leg[r], L.ankle, bend_n, sm.leg_tw[r],
                        sm.leg_len[r]);
            // *** AND THE FOOT MUST TAKE THE BLENDED POSITION. `pose_pass`
            // pins the foot at `foot_w`, whose translation is the BOARD WELD
            // in the ordinary path -- it only becomes the IK target inside the
            // weight-shift branch. Copying that line verbatim (the first draft
            // did) welds the boot back onto the running board while the thigh
            // and calf trail: the one thing this rung exists to stop.
            glm::quat qb = glm::quat_cast(glm::mat3(
                sm.nodes[sm.leg[r][1]].world * sm.nodes[sm.leg[r][2]].local));
            // The short way round -- a quat and its negation are the same
            // rotation, and these are roughly a half-turn apart once the leg
            // folds back. Without the sign fix the boot spins through the seat.
            if (glm::dot(qa, qb) < 0.0f) qb = -qb;
            glm::mat4 foot_w =
                glm::mat4(glm::mat3_cast(glm::slerp(qa, qb, L.w_eff)));
            foot_w[3] = glm::vec4(L.ankle, 1.0f);
            sm.nodes[sm.leg[r][2]].world = foot_w;
            sm.nodes[sm.leg[r][2]].world_override = true;
        }

        // Re-settle: recompose every non-overridden world (the toes under the
        // moved feet). Idempotent for untouched subtrees -- nothing between
        // the pose pass and here has written a node local.
        for (const int i : sm.order) {
            SNode& nd = sm.nodes[i];
            if (nd.parent < 0 || nd.world_override) continue;
            nd.world = sm.nodes[nd.parent].world * nd.local;
        }
    }

    // ---- THE SCARF (docs/SCARF_SPEC.md §4.2) -------------------------------
    // Placed HERE on purpose: after every pose pass (so the settle pass inside
    // pose_pass can never touch the scarf bones) and before the skinning loop
    // (so a future authored cloth skins onto the frames written below in the
    // SAME frame it is solved). The solver itself is pure and lives in
    // render/trail_chain.*; this block is nothing but frames and wiring.
    //
    // SEADS_SCARF=0 is the off switch: no step, no bone override, no draw.
    static const bool scarf_off = [] {
        const char* e = std::getenv("SEADS_SCARF");
        return e != nullptr && std::atoi(e) == 0;
    }();
    static const bool scarf_dbg = std::getenv("SEADS_SCARF_DEBUG") != nullptr;
    // =2: also print the CURRENT surf_vtx every 30 frames, so steady state is
    // separable from the worst-yet latch (a transient and a resting sink need
    // different fixes).
    static const bool scarf_dbg2 = [] {
        const char* e = std::getenv("SEADS_SCARF_DEBUG");
        return e != nullptr && std::atoi(e) >= 2;
    }();
    const bool scarf_live = sm.scarf_ok && sm.scarf_primed && !scarf_off;
    if (scarf_live) {
        // model <-> body is the mount's last factor, R_y(180 deg). COMPUTED,
        // never hand-written as a sign flip (SCARF_SPEC §4.4: "compute it,
        // never write it down") -- the day the mount changes, this follows.
        const glm::mat3 body_from_model = glm::mat3(glm::rotate(
            glm::mat4(1.0f), 3.14159265f, glm::vec3(0.0f, 1.0f, 0.0f)));
        const glm::mat3 model_from_body = glm::transpose(body_from_model);
        TrailChainInput in;
        // Relative wind = -velocity. The machine's own motion IS the wind: no
        // world weather feeds this (SCARF_SPEC §7, named out of scope).
        in.wind_mps = -(model_from_body * rig.vel_body_mps);
        // Gravity, on a sphere: down is -normalize(position), recomputed this
        // frame, never a fixed axis (CLAUDE.md sphere invariants).
        const glm::vec3 down_world =
            glm::vec3(-glm::normalize(glm::dvec3(pos)));
        in.gravity_dir = glm::normalize(
            model_from_body * (glm::mat3(glm::transpose(basis)) * down_world));
        // ★★★ THE FRAME FIELD (R4a; Chad ruled the scarf gets it, 2026-08-27).
        // The two lines above are everything the scarf used to know: how fast
        // it is going, and which way is down. Neither says the FRAME ITSELF is
        // accelerating and rotating, so a hard stop did not throw the scarf
        // forward and a held corner did not swing it outboard -- the same
        // missing terms that stopped the body chain from supermanning.
        //
        // FINITE-DIFFERENCED HERE, deliberately, not taken from the kernel:
        // a_body/alpha_body exist in the kernel only inside SledDebugSubstep
        // under `if (dbg)`, and passing a sink in the shipped game to reach
        // them would be a tape-visible change to shipped behaviour.
        //
        // The difference is over the FRAME (rig.ticks sub-steps of rig.dt_s),
        // because one velocity sample per frame is all render gets -- and it
        // is then held constant across those sub-steps, exactly as the wind
        // and the gravity direction above already are.
        //
        // frame_origin stays 0: body_from_model is a PURE ROTATION, so the two
        // origins coincide, and the body origin IS the system CG (sim/sled.h).
        // Anchoring r anywhere else silently zeroes the lever arm the Euler and
        // centrifugal terms act on, and a zero lever looks exactly like a term
        // that does nothing.
        //
        // SEADS_SCARF_FIELD=0 turns it OFF, and off is BIT-IDENTICAL to the
        // pre-rung scarf (every term multiplies out to (0,0,0)) -- the A/B arm
        // for the drive, same shape as SEADS_SCARF_FLUTTER.
        static const bool field_on = [] {
            const char* e = std::getenv("SEADS_SCARF_FIELD");
            return !(e != nullptr && e[0] == '0');
        }();
        // ★ RE-PRIME ON AN EXTERNAL WRITE. The mount seed and KEY_R autoright
        // teleport the machine and kill its motion; differencing across one
        // reads hundreds of g. The chain's OWN teleport test cannot catch it --
        // the anchor is a MODEL-SPACE pose position and does not move when the
        // machine is respawned. See SledRig::epoch.
        if (sm.scarf_epoch != rig.epoch) {
            sm.scarf_epoch = rig.epoch;
            sm.scarf_frame = TrailFrameTracker{};
        }
        {
            // ★ RAW `rig.ticks`, NOT the capped `steps` below. This is the
            // TRUE sim time the velocity changed over. On a stall the sub-step
            // cap drops time (it never stretches dt), so the chain integrates
            // the same honest acceleration over fewer sub-steps -- dividing by
            // the capped count instead would INFLATE the acceleration exactly
            // when the frame was already struggling.
            const float frame_dt = rig.dt_s * static_cast<float>(rig.ticks);
            const TrailFrameField ff = trail_frame_field(
                sm.scarf_frame, model_from_body * rig.vel_body_mps,
                model_from_body * rig.omega_body_rps, frame_dt);
            if (field_on) {
                in.frame_accel_mps2 = ff.accel_mps2;
                in.frame_omega_rps = ff.omega_rps;
                in.frame_alpha_rps2 = ff.alpha_rps2;
            }
            if (scarf_dbg)
                TraceLog(LOG_INFO,
                         "SCARF-FIELD: |a| %.4f m/s2 |alpha| %.4f rps2 "
                         "|omega| %.4f rps |v| %.4f m/s",
                         static_cast<double>(glm::length(ff.accel_mps2)),
                         static_cast<double>(glm::length(ff.alpha_rps2)),
                         static_cast<double>(glm::length(ff.omega_rps)),
                         static_cast<double>(glm::length(rig.vel_body_mps)));
        }

        // The knot: the neck's POSED world times scarf_01's rest local. Anchor
        // position and ribbon width axis both come out of that one matrix.
        const glm::mat4 neck_w = sm.nodes[sm.n_neck].world;
        const glm::mat4 anchor_w = neck_w * sm.scarf_anchor_local;
        in.anchor = glm::vec3(anchor_w[3]);
        in.width_axis =
            glm::normalize(glm::vec3(anchor_w * glm::vec4(1, 0, 0, 0)));
        // §3b THE TORSO PLANE, from the POSED pelvis and neck every frame. The
        // posed torso leans ~33 deg FORWARD, so a merely-vertical plane would
        // let the scarf hang through the chest; and "back" is DERIVED as the
        // knot's own offset perpendicular to the spine, never an axis literal.
        const glm::vec3 neck_p(neck_w[3]);
        const glm::vec3 pelvis_p(sm.nodes[sm.n_pelvis].world[3]);
        const glm::vec3 torso = neck_p - pelvis_p;
        in.back_origin = neck_p;
        in.back_normal = glm::vec3(0.0f);  // degenerate torso => plane OFF
        if (glm::length(torso) > 1.0e-4f) {
            const glm::vec3 axis = glm::normalize(torso);
            glm::vec3 b = in.anchor - neck_p;
            b -= axis * glm::dot(b, axis);
            if (glm::length(b) > 1.0e-4f) in.back_normal = glm::normalize(b);
        }
        // Stash the sweater frame for the SK-1c back HUD. These are MODEL
        // space here -- the model->eye-relative `mount` is not built until
        // further down -- so they are transformed and published there, once.
        // Same posed pelvis/neck/back_normal the scarf plane just derived: a
        // READ of that frame, never a second derivation.
        g_rb_have = glm::length(in.back_normal) > 0.5f;
        g_rb_pelvis_m = pelvis_p;
        g_rb_neck_m = neck_p;
        g_rb_normal_m = in.back_normal;
        g_rb_width_m = in.width_axis;
        g_rb_surface = sm.scarf_back_surface;
        // R2c-7s(g): roots are AUTHORED off the surface; stand-off is 0.
        // ★ SCARF IDLE FIX (Chad 2026-09-05, "scarf blowing as if there was
        // wind when I was stopped"): this pre-offset used to pair with an
        // add-or-RESET update below — measure on the already-offset anchor,
        // push along bp_normal but pre-offset along back_normal, zero the
        // stored value the frame it succeeds. That is a two-frame oscillator
        // of the chain root, amplitude = the whole 42-79 mm stand-off, at ANY
        // speed. The update below now converges (raw-anchor measurement,
        // one axis, 3 mm deadband — the d23ad731f precedent); this line keeps
        // the held stand-off applied on frames where the banded-plane block
        // is skipped, and is overwritten by the block when it runs.
        const float scarf_standoff_prev = sm.scarf_anchor_standoff;
        in.anchor += in.back_normal * scarf_standoff_prev;
        // ---- the banded drawn-back planes (consult P0-1). Re-skin the few
        // recorded back-quad verts with the POSED joints (rigid, one matrix
        // each), average per ring, and hand the solver one plane per band.
        if (sm.back_n_rings >= 2 && glm::length(in.back_normal) > 0.5f) {
            glm::vec3 cent[kTrailChainMaxBackPlanes + 1];
            int cnt[kTrailChainMaxBackPlanes + 1] = {};
            for (int r = 0; r <= kTrailChainMaxBackPlanes; ++r)
                cent[r] = glm::vec3(0.0f);
            for (const SledModel::BackVert& bv : sm.back_verts) {
                const SPrim& sp = sm.prims[bv.prim];
                const std::size_t jj2 = sp.jidx[4 * bv.vert];
                const glm::mat4 mj = sm.nodes[sm.skin_joints[jj2]].world *
                                     sm.skin_ibm[jj2];
                const glm::vec3 pw(
                    mj * glm::vec4(sp.base_pos[3 * bv.vert],
                                   sp.base_pos[3 * bv.vert + 1],
                                   sp.base_pos[3 * bv.vert + 2], 1.0f));
                cent[bv.ring] += pw;
                ++cnt[bv.ring];
            }
            int nr = 0;
            for (int r = 0; r < sm.back_n_rings; ++r)
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
                    in.bp_t[b] =
                        glm::dot(cent[b] - in.torso_origin, in.torso_axis);
                }
                // ★★★ PUSH EACH PLANE OUT TO THE SURFACE IT IS SUPPOSED TO BE.
                //
                // Chad, 2026-08-24, on the fifth time of asking: "its still
                // sinking into the coat".  He was right and back_clr was a
                // true report of the wrong question.
                //
                // bp_origin above is the midpoint of two ring CENTROIDS -- the
                // AVERAGE of a 520 mm-wide band of drawn back.  The coat's
                // actual surface along the spine, which is precisely where the
                // scarf lies, sits well BEHIND that average.  MEASURED on the
                // posed mesh, plane -> rearmost drawn vert in the same band:
                //
                //     +18, +146, +272, +154, +182, +3 mm
                //
                // So holding the chain kScarfBackMarginM + tube-half (27 mm)
                // off the CENTROID plane still leaves it deep inside the coat.
                // The keep-out was being measured off the middle of his body.
                //
                // Fix: keep the normal from the centroid chain (it is stable --
                // an average is a good ESTIMATOR OF DIRECTION and a bad
                // estimator of position), and slide the origin along that
                // normal out to the rearmost drawn vertex in the band, within
                // the lateral strip the scarf actually occupies.  Then 27 mm
                // off the plane really is 27 mm off the coat.
                const float pr_back_keep = sm.scarf_par.back_keepout_m;
                const glm::vec3 lat_b = glm::cross(in.torso_axis,
                                                   in.back_normal);
                float push[kTrailChainMaxBackPlanes] = {};
                for (const SledModel::BackVert& bv : sm.back_verts) {
                    const SPrim& sp = sm.prims[bv.prim];
                    const std::size_t jj3 = sp.jidx[4 * bv.vert];
                    const glm::mat4 mj3 = sm.nodes[sm.skin_joints[jj3]].world *
                                          sm.skin_ibm[jj3];
                    const glm::vec3 pw(
                        mj3 * glm::vec4(sp.base_pos[3 * bv.vert],
                                        sp.base_pos[3 * bv.vert + 1],
                                        sp.base_pos[3 * bv.vert + 2], 1.0f));
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
                // ★ SCARF-DRAPE slab: each band's coat half-width, off the
                // POSED torso-side verts -- the lateral bound that lets the
                // scarf hang BESIDE the torso at a bent-forward pose instead
                // of wadding above the arch (bp_lat_half's banner).
                if (!sm.side_verts.empty()) {
                    const glm::vec3 lat_n =
                        glm::length(lat_b) > 1.0e-4f ? glm::normalize(lat_b)
                                                     : lat_b;
                    float wb[kTrailChainMaxBackPlanes] = {};
                    float fb[kTrailChainMaxBackPlanes];
                    for (int b = 0; b < kTrailChainMaxBackPlanes; ++b)
                        fb[b] = 1.0e9f;
                    for (const SledModel::BackVert& bv : sm.side_verts) {
                        const SPrim& sp = sm.prims[bv.prim];
                        const std::size_t jj4 = sp.jidx[4 * bv.vert];
                        const glm::mat4 mj4 =
                            sm.nodes[sm.skin_joints[jj4]].world *
                            sm.skin_ibm[jj4];
                        const glm::vec3 pw(
                            mj4 * glm::vec4(sp.base_pos[3 * bv.vert],
                                            sp.base_pos[3 * bv.vert + 1],
                                            sp.base_pos[3 * bv.vert + 2],
                                            1.0f));
                        const float la =
                            std::fabs(glm::dot(pw - in.back_origin, lat_n));
                        for (int b = 0; b + 1 < nr; ++b) {
                            if (bv.ring != b && bv.ring != b + 1) continue;
                            if (la > wb[b]) wb[b] = la;
                            const float sfb = glm::dot(
                                pw - in.bp_origin[b], in.bp_normal[b]);
                            if (sfb < fb[b]) fb[b] = sfb;
                        }
                    }
                    for (int b = 0; b + 1 < nr; ++b) {
                        in.bp_lat_half[b] = wb[b];
                        // the chest surface; live only when truly in front
                        // (bp_front's banner: 0 disables).
                        in.bp_front[b] = fb[b] < 0.0f ? fb[b] : 0.0f;
                    }
                }
                // ★★★ AND STAND THE ANCHOR OFF THE SURFACE, OR THE PLANES
                // ABOVE DO NOTHING.  trail_chain.cpp's banded constraint is
                //
                //     float d = pr.back_keepout_m;
                //     s_anch = dot(in.anchor - bp_origin[b], n);
                //     if (s_anch < d) d = s_anch;      // <- the anchor pin
                //
                // so the permitted depth is CLAMPED TO THE ANCHOR'S OWN DEPTH.
                // `scarf_01` is authored against the BARE SUIT's back, and
                // there is now a COAT over it 42-79 mm further out, so the
                // anchor sits inside the coat and that clamp licenses the whole
                // chain to sit inside it too.  Pushing the planes out without
                // this made s_anch MORE negative and the permitted depth WORSE.
                // Measured before this: surf_clr -0.086 m.
                //
                // R2c-7s(g) set the stand-off to 0 because "roots are AUTHORED
                // off the surface" -- true of the bare suit, false the moment
                // the costume reached the GLB.  Restore it, computed, per frame.
                {
                    // ★ SCARF IDLE FIX: measure on the RAW anchor (undo the
                    // held pre-offset), converge the stored stand-off toward
                    // the total need, and apply it ONCE along the band's own
                    // normal. Growth is instant (a real violation must not
                    // lag a frame); shrink is slow and deadbanded (3 mm, the
                    // pod-0 precedent) so float noise cannot chatter the
                    // chain root — the "wind at a standstill" Chad reported.
                    const glm::vec3 anchor_raw =
                        in.anchor - in.back_normal * scarf_standoff_prev;
                    const float ta = glm::dot(anchor_raw - in.torso_origin,
                                              in.torso_axis);
                    int ba = 0;
                    while (ba + 1 < nr - 1 && ta > in.bp_t[ba + 1]) ++ba;
                    const float s_raw = glm::dot(anchor_raw - in.bp_origin[ba],
                                                 in.bp_normal[ba]);
                    // ★ SCARF-DRAPE: pod-aware, multi-band, and SHARED --
                    // trail_chain_anchor_need (see its banner): bone 0's pod
                    // hangs off the PINNED root the constraint loop never
                    // grades, so the pin is the only thing placing it.
                    const float keep_a =
                        trail_chain_anchor_need(sm.scarf, sm.scarf_par, in);
                    const float need =
                        s_raw < keep_a ? keep_a - s_raw : 0.0f;
                    float so = sm.scarf_anchor_standoff;
                    if (need > so + 0.003f) {
                        so = need;
                    } else if (need < so - 0.003f) {
                        // decay at 50 mm/s of frame time toward the need
                        const float dt_frame =
                            rig.dt_s * static_cast<float>(rig.ticks);
                        so = std::max(need, so - 0.05f * dt_frame);
                    }
                    sm.scarf_anchor_standoff = so;
                    in.anchor = anchor_raw + in.bp_normal[ba] * so;
                    const float s_anch = s_raw;  // for the debug line below
                    if (scarf_dbg)
                        TraceLog(LOG_INFO,
                                 "SCARF: anchor was %.3f m off the coat, stood "
                                 "off by %.3f m",
                                 static_cast<double>(s_anch),
                                 static_cast<double>(
                                     sm.scarf_anchor_standoff));
                }
                if (scarf_dbg) {
                    // How far each plane had to move to REACH the coat. These
                    // are the millimetres the scarf was sinking by.
                    TraceLog(LOG_INFO,
                             "SCARF: plane->surface push mm %.0f %.0f %.0f "
                             "%.0f %.0f %.0f (bands %d)",
                             static_cast<double>(push[0] * 1000.0f),
                             static_cast<double>(push[1] * 1000.0f),
                             static_cast<double>(push[2] * 1000.0f),
                             static_cast<double>(push[3] * 1000.0f),
                             static_cast<double>(push[4] * 1000.0f),
                             static_cast<double>(push[5] * 1000.0f), nr - 1);
                }
                in.bp_t[nr - 1] = glm::dot(cent[nr - 1] - in.torso_origin,
                                           in.torso_axis);
            }
        }
        // The helmet sphere, up the head bone's own +Y (the joint is at the
        // base of the skull; the helmet is not).
        const glm::mat4 head_w = sm.nodes[sm.n_head].world;
        in.head_center =
            glm::vec3(head_w[3]) +
            glm::normalize(glm::vec3(head_w * glm::vec4(0, 1, 0, 0))) *
                kScarfHeadCentreUpM;

        // TICK-derived, never wall time (house law). The cap drops time on a
        // stall; it never stretches dt.
        int steps = rig.ticks;
        if (steps > kScarfMaxSubsteps) steps = kScarfMaxSubsteps;
        if (steps < 0) steps = 0;
        sm.scarf_par.dt_s = rig.dt_s;
        sm.scarf_par.seg_len_m = sm.scarf_seg_len;
        // The short tail's own anchor: scarf_s01's bind-local in the neck,
        // stood off the back exactly like the long chain's root.
        TrailChainInput in_s = in;
        if (sm.scarf_s_ok) {
            const glm::mat4 saw = neck_w * sm.scarf_s_anchor_local;
            in_s.anchor = glm::vec3(saw[3]);
            // ★ SCARF-DRAPE (red-team finding 2): the short tail's bone-0 pod
            // hangs off ITS pinned root too, and the first cut handed it the
            // LONG chain's stand-off -- the exact defect this rung closes,
            // one chain over, in the very region Chad named ("distal to the
            // knots"). Its own pod, its own band, its own stand-off.
            if (in_s.n_back_planes > 0) {
                const float ta_s = glm::dot(in_s.anchor - in_s.torso_origin,
                                            in_s.torso_axis);
                int ba_s = 0;
                while (ba_s + 1 < in_s.n_back_planes &&
                       ta_s > in_s.bp_t[ba_s + 1])
                    ++ba_s;
                const float s_anch_s =
                    glm::dot(in_s.anchor - in_s.bp_origin[ba_s],
                             in_s.bp_normal[ba_s]);
                const float keep_s = trail_chain_anchor_need(
                    sm.scarf_s, sm.scarf_s_par, in_s);
                if (s_anch_s < keep_s)
                    in_s.anchor +=
                        in_s.bp_normal[ba_s] * (keep_s - s_anch_s);
            } else {
                in_s.anchor += in.back_normal * sm.scarf_anchor_standoff;
            }
            sm.scarf_s_par.dt_s = rig.dt_s;
            sm.scarf_s_par.back_keepout_m = sm.scarf_par.back_keepout_m;
            // ★ AND THE FLOOR WITH IT. The short shoulder tail is the same
            // ribbon on the same back; copying only one of the pair would have
            // left it sinking while the long tail rested -- the "moved
            // consumer" trap this repo keeps paying for, one field later.
            sm.scarf_s_par.back_keepout_min_m =
                sm.scarf_par.back_keepout_min_m;
        }
        // ---- the eighth-drive flutter (see the kScarfFlutter* banner): a
        // wind-direction wobble per SUB-STEP, phase from tick time only.
        static const glm::vec2 flutter_scale = [] {
            // SEADS_SCARF_FLUTTER: unset/1 = shipped dials; 0 = off;
            // "amp,freq" = live A/B of both dials.
            const char* e = std::getenv("SEADS_SCARF_FLUTTER");
            if (!e || !*e) return glm::vec2(1.0f, 1.0f);
            float a = 1.0f, f = 1.0f;
            if (std::sscanf(e, "%f,%f", &a, &f) == 1) f = 1.0f;
            return glm::vec2(a, f);
        }();
        const float v_wind = glm::length(in.wind_mps);
        const bool fluttering = flutter_scale.x > 0.0f &&
                                v_wind > kScarfFlutterVMinMps &&
                                rig.dt_s > 0.0f;
        float flut_f = 0.0f, flut_amp = 0.0f, flut_ramp = 0.0f;
        if (fluttering) {
            flut_f = kScarfFlutterStrouhal * v_wind / kScarfFlutterHangM;
            if (flut_f > kScarfFlutterFMaxHz) flut_f = kScarfFlutterFMaxHz;
            flut_f *= flutter_scale.y;
            flut_amp = flutter_scale.x * kScarfFlutterAmpRad *
                       kScarfFlutterVRefMps /
                       (v_wind + kScarfFlutterVRefMps);
            // Red-team P1-1: A(v) is MAXIMAL right at the on-threshold, so
            // crossing kScarfFlutterVMinMps popped a ~0.1 m tip wave in and
            // out discontinuously. Smoothstep the first 1 m/s above the gate.
            float ramp = (v_wind - kScarfFlutterVMinMps) * 1.0f;
            if (ramp > 1.0f) ramp = 1.0f;
            flut_ramp = ramp * ramp * (3.0f - 2.0f * ramp);
            flut_amp *= flut_ramp;
        }
        if (rig.dt_s > 0.0f)
            for (int s = 0; s < steps; ++s) {
                TrailChainInput in_f = in;
                TrailChainInput in_sf = in_s;
                if (fluttering) {
                    constexpr float k2pi = 6.2831853f;
                    sm.scarf_flut_ph1 += k2pi * flut_f * rig.dt_s;
                    sm.scarf_flut_ph2 +=
                        k2pi * flut_f * kScarfFlutterGold * rig.dt_s;
                    sm.scarf_flut_phe +=
                        k2pi * flut_f * kScarfFlutterEnvFrac * rig.dt_s;
                    if (sm.scarf_flut_ph1 > k2pi) sm.scarf_flut_ph1 -= k2pi;
                    if (sm.scarf_flut_ph2 > k2pi) sm.scarf_flut_ph2 -= k2pi;
                    if (sm.scarf_flut_phe > k2pi) sm.scarf_flut_phe -= k2pi;
                    const float env =
                        0.72f + 0.28f * std::sin(sm.scarf_flut_phe + 0.9f);
                    const float a1 =
                        flut_amp * env * std::sin(sm.scarf_flut_ph1);
                    const float a2 = 0.55f * flut_amp *
                                     std::sin(sm.scarf_flut_ph2 + 1.7f);
                    // flap axis = horizontal, perpendicular to the wind (the
                    // vertical flap); sway axis = gravity (the lateral wag).
                    const glm::vec3 wdir = in.wind_mps / v_wind;
                    glm::vec3 flap_ax =
                        glm::cross(wdir, in.gravity_dir);
                    const float fl = glm::length(flap_ax);
                    glm::mat3 rot(1.0f);
                    if (fl > 1.0e-4f)
                        rot = glm::mat3(glm::rotate(glm::mat4(1.0f), a1,
                                                    flap_ax / fl));
                    rot = glm::mat3(glm::rotate(glm::mat4(1.0f), a2,
                                                in.gravity_dir)) * rot;
                    in_f.wind_mps = rot * in.wind_mps;
                    in_sf.wind_mps = in_f.wind_mps;
                }
                trail_chain_step(sm.scarf, sm.scarf_par, in_f);
                if (sm.scarf_s_ok)
                    trail_chain_step(sm.scarf_s, sm.scarf_s_par, in_sf);
            }

        // ---- the six bones ARE the deliverable now: the authored fluffy
        // tail pods (scarf_geom.py) are rigid-skinned to scarf_01..06 in the
        // GLB, so writing these worlds IS drawing the scarf. The wrap, knot,
        // bridges and the short LEFT-shoulder tail are rigid to neck_01 and
        // ride the pose pass like any other skinned vertex.
        // The traveling-wave flap (kScarfFlutterTipM banner): displace a COPY
        // of the solved chain, never the solver's own state -- the wave is
        // presentation, the physics never sees it. last_x (ribbon-continuity
        // seed) is copied back so the width axis stays continuous.
        glm::vec3 e_flap(0.0f), e_lat(0.0f);
        if (fluttering) {
            const glm::vec3 wdir = in.wind_mps / v_wind;
            glm::vec3 lat = glm::cross(wdir, in.gravity_dir);
            const float ll = glm::length(lat);
            if (ll > 1.0e-4f) {
                e_lat = lat / ll;
                e_flap = glm::normalize(glm::cross(e_lat, wdir));
            }
        }
        const float flut_env =
            0.72f + 0.28f * std::sin(sm.scarf_flut_phe + 0.9f);
        const auto wave_disp = [&](float s) -> glm::vec3 {
            if (!fluttering) return glm::vec3(0.0f);
            const float g = kScarfFlutterTipM * flutter_scale.x * flut_env *
                            flut_ramp *
                            (kScarfFlutterVRefMps /
                             (v_wind + kScarfFlutterVRefMps)) *
                            s * std::sqrt(s);
            return g * (std::sin(sm.scarf_flut_ph1 -
                                 kScarfFlutterWaveRad * s) * e_flap +
                        0.55f * std::sin(sm.scarf_flut_ph2 -
                                         kScarfFlutterWaveRad * s + 1.7f) *
                            e_lat);
        };
        glm::mat4 fr[kScarfSegments];
        {
            TrailChainState vis = sm.scarf;
            for (int k = 1; k <= vis.n; ++k)
                vis.p[k] += wave_disp(float(k) / float(vis.n));
            // ★ SCARF-DRAPE: the wave never meets the solver, so it was free
            // to put the drawn tail -0.027 m inside the coat at speed
            // (measured). Same law for the copy that is actually drawn.
            trail_chain_present_clamp(vis, sm.scarf_par, in);
            // ★ 2026-09-04 clamp-leak probe (dbg2 only): replicate the
            // clamp's own box test on the CLAMPED copy and print any station
            // it left violated -- the number that says which gate leaks.
            if (scarf_dbg2 && in.n_back_planes > 0) {
                glm::mat4 dbg_fr[kScarfSegments];
                TrailChainState dbg_vis = vis;
                trail_chain_frames(dbg_vis, in.width_axis, dbg_fr);
                for (int i5 = 1; i5 < sm.scarf_par.n_probe_stations; ++i5) {
                    const TrailChainProbe& pb5 = sm.scarf_par.probe[i5][0];
                    const glm::vec3 bx5(dbg_fr[i5][0]), by5(dbg_fr[i5][1]),
                        bz5(dbg_fr[i5][2]);
                    const glm::vec3 c5 = vis.p[i5] + bx5 * pb5.u_m +
                                         by5 * pb5.v_m + bz5 * pb5.w_m;
                    const float t5 = glm::dot(vis.p[i5] - in.torso_origin,
                                              in.torso_axis);
                    int b5 = 0;
                    while (b5 + 1 < in.n_back_planes && t5 > in.bp_t[b5 + 1])
                        ++b5;
                    for (int bb = (b5 > 0 ? b5 - 1 : 0);
                         bb <= (b5 + 1 < in.n_back_planes ? b5 + 1 : b5);
                         ++bb) {
                        const glm::vec3& nn5 = in.bp_normal[bb];
                        const float sup5 =
                            pb5.hx_m * std::fabs(glm::dot(bx5, nn5)) +
                            pb5.hy_m * std::fabs(glm::dot(by5, nn5)) +
                            pb5.hz_m * std::fabs(glm::dot(bz5, nn5));
                        const float sc5 =
                            glm::dot(c5 - in.bp_origin[bb], nn5) - sup5;
                        if (sc5 < -0.005f)
                            TraceLog(LOG_INFO,
                                     "SCARF-LEAK: station %d band %d sc "
                                     "%.3f (t %.3f, tgate [%0.3f..%0.3f], "
                                     "W %.3f F %.3f)",
                                     i5, bb, static_cast<double>(sc5),
                                     static_cast<double>(t5),
                                     static_cast<double>(in.bp_t[0]),
                                     static_cast<double>(
                                         in.bp_t[in.n_back_planes]),
                                     static_cast<double>(
                                         in.bp_lat_half[bb]),
                                     static_cast<double>(in.bp_front[bb]));
                    }
                }
            }
            trail_chain_frames(vis, in.width_axis, fr);
            sm.scarf.last_x = vis.last_x;
        }
        for (int k = 0; k < kScarfSegments; ++k) {
            sm.nodes[sm.scarf_node[k]].world = fr[k];
            sm.nodes[sm.scarf_node[k]].world_override = true;
        }
        if (sm.scarf_s_ok) {
            glm::mat4 frs[kScarfShortSegs];
            // "the shorter one can move a little bit but more simply":
            // the same wave at HALF amplitude, over its 2 links.
            TrailChainState vis_s = sm.scarf_s;
            for (int k = 1; k <= vis_s.n; ++k)
                vis_s.p[k] += 0.5f * wave_disp(float(k) / float(vis_s.n));
            trail_chain_present_clamp(vis_s, sm.scarf_s_par, in_s);
            trail_chain_frames(vis_s, in.width_axis, frs);
            sm.scarf_s.last_x = vis_s.last_x;
            for (int k = 0; k < kScarfShortSegs; ++k) {
                sm.nodes[sm.scarf_s_node[k]].world = frs[k];
                sm.nodes[sm.scarf_s_node[k]].world_override = true;
            }
        }

        if (scarf_dbg) {
            const glm::vec3 chord = sm.scarf.p[sm.scarf.n] - sm.scarf.p[0];
            const glm::vec3 chord_body = body_from_model * chord;
            float head_clear = 1.0e9f, back_clear = 1.0e9f;
            const bool has_back = glm::length(in.back_normal) > 0.5f;
            for (int i = 1; i <= sm.scarf.n; ++i) {
                const float hc =
                    glm::length(sm.scarf.p[i] - in.head_center) -
                    sm.scarf_par.head_keepout_r_m;
                if (hc < head_clear) head_clear = hc;
                if (has_back) {
                    const float bc = glm::dot(sm.scarf.p[i] - in.back_origin,
                                              in.back_normal) -
                                     sm.scarf_par.back_keepout_m;
                    if (bc < back_clear) back_clear = bc;
                }
            }
            // ★ THE NUMBER THAT ANSWERS CHAD'S COMPLAINT.  back_clr above is
            // the chain against the SINGLE chord plane through the neck --
            // which is not what constrains it and not where the coat is.  This
            // is the chain against the BANDED SURFACE planes, in each point's
            // own band: negative means fabric inside the coat.  Five rounds of
            // "fixed" were graded on back_clr, which stayed positive the whole
            // time.  Grade on surf_clr.
            float surf_clr = 1.0e9f;
            if (in.n_back_planes > 0) {
                for (int i = 1; i <= sm.scarf.n; ++i) {
                    const float tt = glm::dot(sm.scarf.p[i] - in.torso_origin,
                                              in.torso_axis);
                    for (int b = 0; b < in.n_back_planes; ++b) {
                        if (tt < in.bp_t[b] || tt > in.bp_t[b + 1]) continue;
                        const float c = glm::dot(sm.scarf.p[i] -
                                                     in.bp_origin[b],
                                                 in.bp_normal[b]);
                        if (c < surf_clr) surf_clr = c;
                    }
                }
            }
            TraceLog(LOG_INFO, "SCARF: surf_clr %.3f m (negative = INSIDE the coat)",
                     static_cast<double>(surf_clr > 1.0e8f ? 0.0f : surf_clr));
            TraceLog(LOG_INFO,
                     "SCARF: |v| %.2f m/s lift %.1f deg az %.1f deg tail "
                     "(%.3f,%.3f,%.3f) head_clr %.3f back_clr %.3f steps %d",
                     static_cast<double>(glm::length(rig.vel_body_mps)),
                     static_cast<double>(
                         trail_chain_lift_rad(sm.scarf, in.gravity_dir) *
                         57.2957795f),
                     static_cast<double>(
                         std::atan2(chord_body.x, -chord_body.z) * 57.2957795f),
                     static_cast<double>(sm.scarf.p[sm.scarf.n].x),
                     static_cast<double>(sm.scarf.p[sm.scarf.n].y),
                     static_cast<double>(sm.scarf.p[sm.scarf.n].z),
                     static_cast<double>(head_clear),
                     static_cast<double>(has_back ? back_clear : 0.0f), steps);
            // ★★★ surf_vtx: THE DRAWN FABRIC against the coat's banded
            // surface, in each vertex's OWN live band -- the same band rule
            // apply_constraints uses. Negative = the drawn scarf is INSIDE
            // the coat, which is the thing the eye sees and the thing neither
            // back_clr nor surf_clr can report.
            //
            // Posed with the SAME rigid one-joint skinning the drawn mesh
            // uses, and read AFTER the bone worlds are written -- so it
            // includes the presentation flutter wave, because that is drawn
            // too and the solver never sees it.
            if (in.n_back_planes > 0 && !sm.scarf_surf_verts.empty()) {
                float surf_vtx = 1.0e9f;   // .ring 0: the solver-driven chain
                float knot_vtx = 1.0e9f;   // .ring 1: the authored wrap/knot
                int worst_bone = -1;
                float worst_t = 0.0f, knot_t = 0.0f;
                for (const SledModel::BackVert& sv : sm.scarf_surf_verts) {
                    const SPrim& sp = sm.prims[sv.prim];
                    const std::size_t jv = sp.jidx[4 * sv.vert];
                    const int nd = sm.skin_joints[jv];
                    // ★ THE FULL 4-WEIGHT BLEND, exactly what the skin pass
                    // draws (2026-09-04): the old slot-0 rigid read placed a
                    // wrap/chain JUNCTION vert 33 cm from where it is drawn
                    // and reported a -31 mm "sink" while the whole chain
                    // streamed 27 cm clear of the coat. An instrument that
                    // measures a shape that is never drawn is the repo's
                    // oldest trap (the raw-vertex-read law).
                    glm::mat4 mjv(0.0f);
                    for (int q = 0; q < 4; ++q) {
                        const float w = sp.jw[4 * sv.vert + q];
                        if (w <= 0.0f) continue;
                        const std::size_t jq = sp.jidx[4 * sv.vert + q];
                        mjv += (sm.nodes[sm.skin_joints[jq]].world *
                                sm.skin_ibm[jq]) *
                               w;
                    }
                    const glm::vec3 pw(
                        mjv * glm::vec4(sp.base_pos[3 * sv.vert],
                                        sp.base_pos[3 * sv.vert + 1],
                                        sp.base_pos[3 * sv.vert + 2], 1.0f));
                    const float tt =
                        glm::dot(pw - in.torso_origin, in.torso_axis);
                    int b = 0;
                    while (b + 1 < in.n_back_planes && tt > in.bp_t[b + 1])
                        ++b;
                    const float c =
                        glm::dot(pw - in.bp_origin[b], in.bp_normal[b]);
                    if (sv.ring == 1) {
                        // ★ MY OWN INSTRUMENT BUG, CAUGHT BY ITS OWN OUTPUT:
                        // the wrap goes ALL THE WAY ROUND the neck, and the
                        // coat's banded planes describe only the BACK. The
                        // verts on the FRONT of the neck are ~300 mm on the
                        // inboard side of a back plane, so the raw minimum
                        // read a constant -0.3072 m that had nothing to do
                        // with any sink -- a number that looks like a finding
                        // and is an artefact of the metric's domain. Only the
                        // back half of the wrap is in the planes' domain.
                        if (glm::dot(pw - in.back_origin, in.back_normal) <=
                            0.0f)
                            continue;
                        if (c < knot_vtx) {
                            knot_vtx = c;
                            knot_t = tt;
                        }
                        continue;
                    }
                    if (c < surf_vtx) {
                        surf_vtx = c;
                        worst_t = tt;
                        worst_bone = -1;
                        for (int k = 0; k < kScarfSegments; ++k)
                            if (nd == sm.scarf_node[k]) worst_bone = k;
                    }
                }
                // ★★★ A DEAD END, RECORDED SO NOBODY REBUILDS IT.
                //
                // Chad, 2026-08-27: "right at knots ... lean back is when it
                // dissapears". So the region that matters is the WRAP/KNOT.
                // Two attempts to measure it with the machinery above BOTH
                // produced large constants that are ARTEFACTS OF THE METRIC'S
                // DOMAIN, not penetration: -0.3072 m over all wrap verts (the
                // wrap goes ALL THE WAY ROUND the neck and the front half is
                // 300 mm inboard of a BACK plane), and -0.1498 m after
                // restricting to the back half -- still meaningless, because
                // the neck itself sits well FORWARD of the coat's back
                // surface, so wrap verts are inboard of a torso back plane by
                // construction. Neither number moved by a micron across a full
                // lean_fwd sweep.
                //
                // THE BANDED PLANES DESCRIBE THE TORSO'S BACK. The wrap does
                // not live in that domain, so NO half-space metric built from
                // them can measure wrap-vs-coat penetration, however it is
                // filtered. Measuring it needs the coat's actual SURFACE at
                // the neck -- which is a Blender/asset measurement against
                // scarf_geom.py, not a render-side plane test.
                //
                // knot_vtx is therefore NOT printed. A misleading instrument is
                // worse than no instrument, and this repo has paid for that
                // exact mistake with back_clr and then surf_clr. The tagged
                // capture (.ring 1) is kept because a correct metric will want
                // it. See the handoff, section 11.
                (void)knot_vtx;
                (void)knot_t;
                // ★ PRINT ONLY ON A NEW WORST. On a drive this emits a few
                // lines that each name a moment, instead of 60 a second that
                // name nothing -- and the LAST line is the answer to "where
                // does it sink". 1 mm of hysteresis so float jitter at a
                // plateau does not spam.
                if (scarf_dbg2) {
                    static int vtx_tick = 0;
                    if (++vtx_tick % 30 == 0) {
                        TraceLog(LOG_INFO, "SCARF: surf_vtx now %.4f m",
                                 static_cast<double>(surf_vtx));
                        // ★ THE LOCAL-SURFACE CHECK (2026-09-03, Chad's
                        // back-lean report vs a green surf_vtx): the band
                        // plane is one number for a 0.52 m-wide band, pushed
                        // out only within the SPINE strip. A scarf vert lying
                        // off-strip (a shoulder) can clear the PLANE and
                        // still sit inside the LOCAL drawn coat. Grade each
                        // scarf vert against the coat verts in its own
                        // lateral+t neighbourhood instead -- pods AND wrap
                        // (this metric is valid for the wrap, unlike the
                        // plane one: it compares against actual local
                        // surface, not a chord).
                        const glm::vec3 lat_ax =
                            glm::cross(in.torso_axis, in.back_normal);
                        struct CV {
                            float t, lat, s;
                        };
                        static std::vector<CV> cvs;
                        cvs.clear();
                        cvs.reserve(sm.back_verts.size());
                        for (const SledModel::BackVert& bv : sm.back_verts) {
                            const SPrim& sp3 = sm.prims[bv.prim];
                            const std::size_t j3 = sp3.jidx[4 * bv.vert];
                            const glm::vec3 pw(
                                sm.nodes[sm.skin_joints[j3]].world *
                                sm.skin_ibm[j3] *
                                glm::vec4(sp3.base_pos[3 * bv.vert],
                                          sp3.base_pos[3 * bv.vert + 1],
                                          sp3.base_pos[3 * bv.vert + 2],
                                          1.0f));
                            cvs.push_back(
                                {glm::dot(pw - in.torso_origin,
                                          in.torso_axis),
                                 glm::dot(pw - in.back_origin, lat_ax),
                                 glm::dot(pw - in.back_origin,
                                          in.back_normal)});
                        }
                        float w_pod = 1.0e9f, w_wrap = 1.0e9f;
                        float w_pod_lat = 0.0f, w_wrap_lat = 0.0f;
                        for (const SledModel::BackVert& sv2 :
                             sm.scarf_surf_verts) {
                            const SPrim& sp3 = sm.prims[sv2.prim];
                            const std::size_t j3 = sp3.jidx[4 * sv2.vert];
                            const glm::vec3 pw(
                                sm.nodes[sm.skin_joints[j3]].world *
                                sm.skin_ibm[j3] *
                                glm::vec4(sp3.base_pos[3 * sv2.vert],
                                          sp3.base_pos[3 * sv2.vert + 1],
                                          sp3.base_pos[3 * sv2.vert + 2],
                                          1.0f));
                            const float tt2 = glm::dot(
                                pw - in.torso_origin, in.torso_axis);
                            const float lat2v =
                                glm::dot(pw - in.back_origin, lat_ax);
                            const float sv_s = glm::dot(
                                pw - in.back_origin, in.back_normal);
                            float loc = -1.0e9f;
                            for (const CV& cv : cvs)
                                if (std::fabs(cv.t - tt2) < 0.06f &&
                                    std::fabs(cv.lat - lat2v) < 0.06f &&
                                    cv.s > loc)
                                    loc = cv.s;
                            if (loc < -1.0e8f) continue;  // no coat here
                            const float c2 = sv_s - loc;
                            if (sv2.ring == 0 && c2 < w_pod) {
                                w_pod = c2;
                                w_pod_lat = lat2v;
                            }
                            // wrap verts: BACK-SIDE ONLY (sv_s well positive)
                            // -- a front-of-neck vert shares (t, lat) with
                            // back-surface coat samples and reads a constant
                            // -0.32 artefact (the documented domain trap).
                            if (sv2.ring == 1 && sv_s > 0.05f &&
                                c2 < w_wrap) {
                                w_wrap = c2;
                                w_wrap_lat = lat2v;
                            }
                        }
                        TraceLog(LOG_INFO,
                                 "SCARF: LOCAL worst pod %.4f m (lat %.3f) "
                                 "wrap %.4f m (lat %.3f) [negative = inside "
                                 "the local drawn coat]",
                                 static_cast<double>(w_pod),
                                 static_cast<double>(w_pod_lat),
                                 static_cast<double>(w_wrap),
                                 static_cast<double>(w_wrap_lat));
                        // ★ THE SOLVER'S OWN VIEW, per particle: which band,
                        // how deep vs that band's plane, how far off the
                        // spine, and the EFFECTIVE keep-out after the
                        // anchor-pin relax -- the number that decides what
                        // the constraint actually enforced.
                        for (int i2 = 1; i2 <= sm.scarf.n; ++i2) {
                            const float tt3 =
                                glm::dot(sm.scarf.p[i2] - in.torso_origin,
                                         in.torso_axis);
                            int b3 = 0;
                            while (b3 + 1 < in.n_back_planes &&
                                   tt3 > in.bp_t[b3 + 1])
                                ++b3;
                            const float s3 = glm::dot(
                                sm.scarf.p[i2] - in.bp_origin[b3],
                                in.bp_normal[b3]);
                            const float l3 =
                                glm::dot(sm.scarf.p[i2] - in.back_origin,
                                         lat_ax);
                            float d3 = sm.scarf_par.back_keepout_m;
                            const float sa3 = glm::dot(
                                in.anchor - in.bp_origin[b3],
                                in.bp_normal[b3]);
                            if (sa3 < d3) d3 = sa3;
                            if (d3 < sm.scarf_par.back_keepout_min_m)
                                d3 = sm.scarf_par.back_keepout_min_m;
                            TraceLog(LOG_INFO,
                                     "SCARF: p[%d] band %d t %.3f s %.3f "
                                     "lat %+.3f d_eff %.3f (anchor s there "
                                     "%.3f)",
                                     i2, b3, static_cast<double>(tt3),
                                     static_cast<double>(s3),
                                     static_cast<double>(l3),
                                     static_cast<double>(d3),
                                     static_cast<double>(sa3));
                        }
                    }
                }
                if (surf_vtx < sm.scarf_surf_worst - 0.001f) {
                    sm.scarf_surf_worst = surf_vtx;
                    // ★ AND THE CHAIN AT THAT MOMENT, under =2: a worst with
                    // no state beside it names a symptom, not a mechanism.
                    if (scarf_dbg2) {
                        const glm::vec3 lat_ax2 =
                            glm::cross(in.torso_axis, in.back_normal);
                        for (int i2 = 1; i2 <= sm.scarf.n; ++i2) {
                            const float tt3 =
                                glm::dot(sm.scarf.p[i2] - in.torso_origin,
                                         in.torso_axis);
                            const float s3 = glm::dot(
                                sm.scarf.p[i2] - in.back_origin,
                                in.back_normal);
                            const float l3 =
                                glm::dot(sm.scarf.p[i2] - in.back_origin,
                                         lat_ax2);
                            TraceLog(LOG_INFO,
                                     "SCARF@worst: p[%d] t %.3f s %.3f "
                                     "lat %+.3f",
                                     i2, static_cast<double>(tt3),
                                     static_cast<double>(s3),
                                     static_cast<double>(l3));
                        }
                    }
                    TraceLog(LOG_INFO,
                             "SCARF: surf_vtx WORST YET %.4f m (NEGATIVE = "
                             "drawn fabric INSIDE the coat) at bone scarf_%02d "
                             "t %.3f, |v| %.1f m/s, |a| %.1f m/s2 "
                             "[surf_clr says %.4f -- it grades the CENTRELINE "
                             "and cannot fail here]",
                             static_cast<double>(surf_vtx), worst_bone + 1,
                             static_cast<double>(worst_t),
                             static_cast<double>(glm::length(rig.vel_body_mps)),
                             static_cast<double>(
                                 glm::length(in.frame_accel_mps2)),
                             static_cast<double>(surf_clr > 1.0e8f ? 0.0f
                                                                  : surf_clr));
                }
            }
            // ★ R4a: the frame field, so the mechanism is MEASURED on a drive
            // rather than assumed. |a| is the uniform term (a hard stop is a
            // few g; a landing is more), |om| the spin the centrifugal and
            // Coriolis terms ride, |al| the snap of a roll starting. All three
            // read 0.00 with SEADS_SCARF_FIELD=0, which is the A/B arm.
            TraceLog(LOG_INFO,
                     "SCARF FIELD: |a| %.2f m/s2 |om| %.2f rad/s |al| %.2f "
                     "rad/s2 epoch %ld",
                     static_cast<double>(glm::length(in.frame_accel_mps2)),
                     static_cast<double>(glm::length(in.frame_omega_rps)),
                     static_cast<double>(glm::length(in.frame_alpha_rps2)),
                     rig.epoch);
        }
    }

    // ---- mount: model -> eye-relative world. Model (0, cg_h, 0) lands on
    // the kernel CG; R_y(180) maps model fwd/left onto body fwd/left.
    const glm::dvec3 rel = pos - eye;
    // (the sweater frame is published just below, once `mount` is built)
    const glm::mat4 mount =
        glm::translate(glm::mat4(1.0f),
                       glm::vec3(static_cast<float>(rel.x),
                                 static_cast<float>(rel.y),
                                 static_cast<float>(rel.z))) *
        glm::mat4(glm::mat3(basis)) *
        // ★ DRIVE-2 FIX ("shroud is halfway through the asphalt"): the
        // kernel's ski contact sits at body-y (susp_x - cg_h) -- mounts at
        // (rest - cg_h), hanging (rest - susp_x) below.  The ski mesh's
        // bottom in model space is (susp_x - sag0), so the origin that puts
        // the DRAWN ski bottom exactly on the KERNEL contact is
        // -(cg_h - sag0), not -cg_h.  The old -cg_h drew the whole machine
        // sag0 (~10 cm) too deep -- invisible in 0.77 m bush snow, half a
        // shroud on plowed road.
        glm::translate(
            glm::mat4(1.0f),
            glm::vec3(0.0f, static_cast<float>(-(cg_h - sag0)), 0.0f)) *
        glm::rotate(glm::mat4(1.0f), 3.14159265f,
                    glm::vec3(0.0f, 1.0f, 0.0f));

    // ---- PUBLISH THE SWEATER FRAME (Chad: "pasted to his white/grey
    // sweater ... currently it is on his head"). The stash above is MODEL
    // space; `mount` is model -> EYE-RELATIVE, so add `eye` back to land in
    // absolute world, which is the space every other draw call names.
    {
        auto to_world = [&](const glm::vec3& p) {
            const glm::vec4 e = mount * glm::vec4(p, 1.0f);
            return glm::dvec3(e.x, e.y, e.z) + eye;
        };
        auto to_dir = [&](const glm::vec3& d) {
            const glm::vec3 w(mount * glm::vec4(d, 0.0f));
            const float L = glm::length(w);
            return L > 1.0e-6f ? w / L : glm::vec3(0.0f);
        };
        g_rider_back.valid = g_rb_have;
        if (g_rb_have) {
            g_rider_back.pelvis = to_world(g_rb_pelvis_m);
            g_rider_back.neck = to_world(g_rb_neck_m);
            g_rider_back.back_normal = to_dir(g_rb_normal_m);
            g_rider_back.width_axis = to_dir(g_rb_width_m);
            g_rider_back.back_surface = g_rb_surface;
        }
    }

    // ---- ★ THE MULLET'S WIND (Sudburian head, 2026-09-04) -----------------
    // Chad ruled NO new bones: the skin stays 44 joints and the Blender
    // 7-bone rig is the motion SPEC only. So this is a vertex pass riding
    // the CPU skinning loop below — scarf precedent for the wind model (the
    // machine's own motion IS the wind, relative wind = -velocity, composed
    // model<-body with the mount's R_y(180)). Two frequencies from the rig's
    // noise spec (fast ripple + slow bow) over a steady backward lean that
    // grows with airspeed; fade and the middle column's 1.6x were baked from
    // the bind curtain at load. SEADS_MULLET=0 kills the pass — the curtain
    // then skins rigid to the head bone, bit-identical to no pass at rest.
    static const bool mullet_off = [] {
        const char* e = std::getenv("SEADS_MULLET");
        return e != nullptr && std::atoi(e) == 0;
    }();
    static double mullet_t = 0.0;
    glm::vec3 mullet_wdir(0.0f);
    float mullet_gust = 0.0f;
    if (!mullet_off) {
        // model <-> body is the mount's last factor, R_y(180 deg) — COMPUTED,
        // never hand-written as a sign flip (SCARF_SPEC §4.4).
        const glm::mat3 body_from_model = glm::mat3(glm::rotate(
            glm::mat4(1.0f), 3.14159265f, glm::vec3(0.0f, 1.0f, 0.0f)));
        const glm::vec3 w =
            -(glm::transpose(body_from_model) * rig.vel_body_mps);
        const float wspd = glm::length(w);
        if (wspd > 0.5f) {
            mullet_wdir = w / wspd;
            mullet_gust = std::clamp(wspd / 15.0f, 0.0f, 1.0f);
        }
        // ★ fly-2 (Chad): "frequency of mullet flapping needs to increase
        // when going fast ... needs to move a bit faster like the scarf."
        // The phase RATE rides the relative wind: idle ~0.5x, ~3.5x by
        // 10 m/s, capped 5x — flutter quickens with speed, amplitude still
        // owned by gust/fade below.
        const float rate = std::clamp(0.5f + 0.30f * wspd, 0.5f, 5.0f);
        mullet_t += static_cast<double>(rig.dt_s) * rig.ticks * rate;
    }
    glm::vec3 mullet_lat(1.0f, 0.0f, 0.0f);
    if (mullet_gust > 0.0f) {
        const glm::vec3 l =
            glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), mullet_wdir);
        const float ll = glm::length(l);
        if (ll > 1.0e-4f) mullet_lat = l / ll;
    }

    // ---- skinning (CPU): joint = world * inverse-bind, verts in model
    // space. Since 2026-08-18 the rider is the Sudburian: ONE skinned prim,
    // 1,176 verts (392 unique) over 42 joints -- 7.7x fewer verts than the
    // legacy figure this block was measured on (0.106 ms/frame at -O2).
    std::vector<glm::mat4> jm(sm.skin_joints.size());
    for (std::size_t j = 0; j < sm.skin_joints.size(); ++j)
        jm[j] = sm.nodes[sm.skin_joints[j]].world * sm.skin_ibm[j];
    // ---- ★ SEADS_RIG_PALETTE_DUMP=<path>: the DRIVEN palette, once ---------
    // INSTRUMENTATION ONLY -- unset, this block reads one env var at load and
    // then costs a branch on a `static bool` per frame; it never writes a
    // vertex. It exists because the scarf-wrap lane learned the hard way that
    // this rig's NODE REST is not its BIND pose (world_rest x IBM is ~300 mm
    // from identity at the collar), so any asset-side weight edit has to be
    // compensated against the palette the ENGINE actually draws with -- and
    // until now there was no way to get that palette out of the running game.
    // Dumps `jm` (world x inverse-bind, one 4x4 per skin joint, column-major
    // glTF order) as JSON on the FIRST frame that reaches the skinning loop,
    // then never again. Park the sled at the station you want pinned, run
    // once, keep the file. Read-once `static const` env idiom, the same one
    // SEADS_SCARF_COLOR uses at load and SEADS_MULLET /
    // SEADS_SLED_CTL_DEBUG use in their lambda form.
    static const char* const palette_dump_path =
        std::getenv("SEADS_RIG_PALETTE_DUMP");
    static bool palette_dumped = false;
    if (palette_dump_path != nullptr && !palette_dumped) {
        palette_dumped = true;  // one shot even if the open fails
        std::FILE* f = std::fopen(palette_dump_path, "wb");
        if (f == nullptr) {
            TraceLog(LOG_WARNING, "RIG: palette dump: cannot open %s",
                     palette_dump_path);
        } else {
            std::fprintf(f, "{\n  \"joints\": %d,\n  \"matrices\": [\n",
                         static_cast<int>(sm.skin_joints.size()));
            for (std::size_t j = 0; j < sm.skin_joints.size(); ++j) {
                const int nd = sm.skin_joints[j];
                std::fprintf(f, "    {\"joint\": %d, \"node\": %d, \"name\": \"%s\",\n",
                             static_cast<int>(j), nd,
                             sm.nodes[nd].name.c_str());
                // column-major flat 16, glTF order (element k = column k/4,
                // row k%4) -- indexed off the glm matrix rather than
                // value_ptr so this needs no new include.
                std::fprintf(f, "     \"m\": [");
                for (int k = 0; k < 16; ++k)
                    std::fprintf(f, "%s%.9g", k ? ", " : "",
                                 static_cast<double>(jm[j][k / 4][k % 4]));
                std::fprintf(f, "]}%s\n",
                             j + 1 < sm.skin_joints.size() ? "," : "");
            }
            std::fprintf(f, "  ]\n}\n");
            std::fclose(f);
            TraceLog(LOG_INFO, "RIG: palette dump -> %s (%d joints)",
                     palette_dump_path,
                     static_cast<int>(sm.skin_joints.size()));
        }
    }
    for (SPrim& sp : sm.prims) {
        if (sp.dent >= 0 && sp.dent != g_helmet_dent) continue;
        if (!sp.skinned) continue;
        if (g_rider_hidden) continue;  // F-POSE: he is at the flak gun
        Mesh& m = sp.mesh;
        for (int v = 0; v < m.vertexCount; ++v) {
            glm::mat4 mtx(0.0f);
            for (int k = 0; k < 4; ++k) {
                const float w = sp.jw[4 * v + k];
                if (w > 0.0f) mtx += jm[sp.jidx[4 * v + k]] * w;
            }
            glm::vec4 p =
                mtx * glm::vec4(sp.base_pos[3 * v], sp.base_pos[3 * v + 1],
                                sp.base_pos[3 * v + 2], 1.0f);
            const glm::vec3 nn = glm::normalize(
                glm::mat3(mtx) * glm::vec3(sp.base_nrm[3 * v],
                                           sp.base_nrm[3 * v + 1],
                                           sp.base_nrm[3 * v + 2]));
            // ★ mullet wind: displacement AFTER skinning, model space. Fade
            // pins the top under the helmet; the free bottom leans back with
            // the relative wind and carries the two-frequency flutter.
            if (sp.is_mullet && mullet_gust > 0.0f) {
                const glm::vec2 wf = sp.wind[static_cast<std::size_t>(v)];
                if (wf.x > 0.0f) {
                    const float px = sp.base_pos[3 * v] * 40.0f;
                    const float t = static_cast<float>(mullet_t);
                    const float ripple = 0.012f * std::sin(t * 20.0f + px);
                    const float bow =
                        0.020f * std::sin(t * 4.5f + px * 0.35f);
                    const glm::vec3 d =
                        wf.x * mullet_gust *
                        (0.045f * mullet_wdir +
                         wf.y * (ripple * mullet_lat + bow * mullet_wdir));
                    p += glm::vec4(d, 0.0f);
                }
            }
            m.vertices[3 * v] = p.x;
            m.vertices[3 * v + 1] = p.y;
            m.vertices[3 * v + 2] = p.z;
            m.normals[3 * v] = nn.x;
            m.normals[3 * v + 1] = nn.y;
            m.normals[3 * v + 2] = nn.z;
        }
        UpdateMeshBuffer(m, 0, m.vertices,
                         m.vertexCount * 3 * static_cast<int>(sizeof(float)),
                         0);
        UpdateMeshBuffer(m, 2, m.normals,
                         m.vertexCount * 3 * static_cast<int>(sizeof(float)),
                         0);
    }

    // ---- draw
    SetShaderValue(sm.shader, sm.loc_sun, &sun_dir[0], SHADER_UNIFORM_VEC3);
    for (const SPrim& sp : sm.prims) {
        if (sp.dent >= 0 && sp.dent != g_helmet_dent) continue;
        // R4c (under the snow) OR F-POSE (at the gun): `skinned` is exactly the rider.
        if ((sm.hide_rider || g_rider_hidden) && sp.skinned) continue;
        // ★★★ THE TEAM KIT PAINTS THE SCARF AND THE HELMET BAND
        // (render/team_kit.h). It is applied HERE, at draw, not at load,
        // because the model is built before the spawn menu asks which side he
        // is on -- and because a per-frame read is the only version that
        // cannot go stale. VALLEY writes the same bytes the GLB already holds
        // (ally blue on the scarf, slag orange on the band), so the shipped
        // rider is unchanged to the last LSB; CENTRAL CITY swaps the pair.
        Color pc = sp.col;
        if (!sp.col_locked && (sp.is_scarf || sp.is_helmet_band)) {
            const TeamKit kit = player_kit();
            const glm::dvec3 t = sp.is_scarf ? kit.scarf : kit.helmet;
            pc = Color{
                static_cast<unsigned char>(std::lround(t.x * 255.0)),
                static_cast<unsigned char>(std::lround(t.y * 255.0)),
                static_cast<unsigned char>(std::lround(t.z * 255.0)), 255};
        }
        const float c3[3] = {pc.r / 255.0f, pc.g / 255.0f, pc.b / 255.0f};
        const float e3[3] = {sp.emit_col.r / 255.0f, sp.emit_col.g / 255.0f,
                             sp.emit_col.b / 255.0f};
        SetShaderValue(sm.shader, sm.loc_color, c3, SHADER_UNIFORM_VEC3);
        SetShaderValue(sm.shader, sm.loc_emit_col, e3, SHADER_UNIFORM_VEC3);
        SetShaderValue(sm.shader, sm.loc_emit, &sp.emit, SHADER_UNIFORM_FLOAT);
        SetShaderValue(sm.shader, sm.loc_gloss, &sp.gloss, SHADER_UNIFORM_FLOAT);
        const glm::mat4 xf = sp.skinned
                                 ? mount
                                 : mount * sm.nodes[sp.node].world * sp.anim;
        DrawMesh(sp.mesh, sm.mat, to_ray_m(xf));
    }

    // ---- ★★★ R4a: THE BODY CHAIN, MADE VISIBLE (SEADS_BODY_CHAIN=1) --------
    // The rung's whole felt question, in Chad's own terms: DOES HIS BODY REST
    // ON THE MACHINE, OR FLOAT OVER IT? The drawn rider is not blended onto
    // this chain yet, so the only honest way to put that question to him is to
    // draw the thing that will carry him -- the 16-link measured chain AND the
    // per-station DRAWN-SURFACE boxes the keep-out actually grades
    // (render/body_chain.h §2, the decision this whole ladder rests on).
    //
    // Boxes, not a centreline and not a ball: the shoulder's 0.246 m lateral
    // half-span is the half-SPAN ACROSS THE SHOULDERS, and a ball of that
    // radius would hold a man's chest 246 mm above a seat his back is a third
    // as thick. If he reads "floating", handoff §5.3 names the suspect before
    // the drive: the torso boxes are conservative, and their lateral/fore-aft
    // extents are clamped by nothing.
    //
    // OFF by default and drawn AFTER the machine, so nothing about the shipped
    // picture moves.
    static const int body_dbg = [] {
        const char* e = std::getenv("SEADS_BODY_CHAIN");
        return e != nullptr ? std::atoi(e) : 0;
    }();
    if (body_dbg > 0 && sm.body_ok && sm.body.primed) {
        const auto W = [&](const glm::vec3& p) {
            const glm::vec4 w = mount * glm::vec4(p, 1.0f);
            return Vector3{w.x, w.y, w.z};
        };
        // ★★★ THE COLOUR IS A RAMP, NOT A LATCH -- and Chad's first drive is
        // why. It used to be `armed = stage_arm > 0.0f`, so ANY nonzero weight
        // painted the whole thing orange. Combined with §8.1 (roll alone gave a
        // small nonzero weight, and a sled is never exactly level) that made
        // orange the permanent state and GREEN A COLOUR HE NEVER SAW: "the
        // green boxes werent there."
        //
        // ★ It is the same disease as the metrics this program keeps paying
        // for, wearing a different hat: a BINARY read of a CONTINUOUS quantity
        // cannot show a stage machine whose whole ruling is that stages 0-3 are
        // CONTINUOUS. The instrument has to be able to show what the mechanism
        // actually does, or it is not an instrument.
        const float t = std::min(1.0f, std::max(0.0f, sm.stage_arm));
        const auto ramp = [&](int r0, int g0, int b0, int r1, int g1, int b1,
                              int a) {
            const auto mix = [&](int lo, int hi) {
                return static_cast<unsigned char>(
                    static_cast<float>(lo) +
                    t * static_cast<float>(hi - lo));
            };
            return Color{mix(r0, r1), mix(g0, g1), mix(b0, b1),
                         static_cast<unsigned char>(a)};
        };
        const Color link_c = ramp(80, 200, 255, 255, 90, 40, 255);
        for (int i = 0; i < sm.body.n; ++i)
            DrawCylinderEx(W(sm.body.p[i]), W(sm.body.p[i + 1]), 0.012f,
                           0.012f, 5, link_c);
        for (int i = 0; i <= sm.body.n; ++i)
            DrawSphere(W(sm.body.p[i]), 0.020f, Color{255, 255, 255, 255});
        // The DRAWN-SURFACE boxes, in the chain's own frames -- the SAME
        // frames the keep-out projects against (trail_chain.cpp's one
        // `chain_axes`, unified this rung precisely so the picture and the
        // constraint cannot fork).
        glm::mat4 bf[kTrailChainMaxSegments];
        {
            TrailChainState vis = sm.body;
            trail_chain_frames(vis, glm::vec3(1.0f, 0.0f, 0.0f), bf);
        }
        const Color box_c = ramp(120, 255, 160, 255, 200, 60, 200);
        for (int i = 0; i <= sm.body.n; ++i) {
            const glm::mat4& f = bf[i < sm.body.n ? i : sm.body.n - 1];
            const glm::vec3 ax(f[0]), ay(f[1]), az(f[2]);
            for (int s = 0; s < 2; ++s) {
                const TrailChainProbe& pr = sm.body_par.probe[i][s];
                if (pr.hx_m <= 0.0f && pr.hy_m <= 0.0f && pr.hz_m <= 0.0f)
                    continue;
                const glm::vec3 c = sm.body.p[i] + ax * pr.u_m + ay * pr.v_m +
                                    az * pr.w_m;
                glm::vec3 crn[8];
                for (int k = 0; k < 8; ++k)
                    crn[k] = c + ax * ((k & 1) ? pr.hx_m : -pr.hx_m) +
                             ay * ((k & 2) ? pr.hy_m : -pr.hy_m) +
                             az * ((k & 4) ? pr.hz_m : -pr.hz_m);
                static const int e12[12][2] = {{0, 1}, {2, 3}, {4, 5}, {6, 7},
                                               {0, 2}, {1, 3}, {4, 6}, {5, 7},
                                               {0, 4}, {1, 5}, {2, 6}, {3, 7}};
                for (const auto& e : e12)
                    DrawLine3D(W(crn[e[0]]), W(crn[e[1]]), box_c);
            }
        }
        // ★ SEADS_BODY_CHAIN=2 -- the ATTRIBUTION line, not a status line. It
        // carries the weight AND the three things that can produce it, because
        // "it is orange" is a symptom and this rung has already been wrong
        // twice about which cause was behind it.
        if (body_dbg > 1) {
            const glm::vec3 up_b =
                glm::mat3(glm::transpose(basis)) *
                glm::vec3(glm::normalize(glm::dvec3(pos)));
            const float tilt_deg =
                57.29578f * std::acos(std::min(1.0f, std::max(-1.0f, up_b.y)));
            // ★ `brel` IS NOT REDUNDANT WITH `arm`, and leaving it out cost a
            // drive. `board_release` is the OTHER weight the drawn legs obey
            // (`bi.w` at the blend call above): stage 2 can move his boots
            // with `arm` at exactly 0, and without this field the log cannot
            // tell that motion from the chain's. And `force` now names BOTH
            // switches -- Chad drove a build with the arm nailed to 1 and read
            // the forcing as a defect in the trigger, so a forcing env var the
            // log cannot see is a confound the log cannot rule out.
            const bool f_arm = std::getenv("SEADS_BODY_ARM") != nullptr;
            const bool f_brd = std::getenv("SEADS_BODY_BOARD") != nullptr;
            const char* force_s = f_arm ? (f_brd ? "ARM+BOARD" : "ARM")
                                        : (f_brd ? "BOARD" : "-");
            TraceLog(LOG_INFO,
                     "R4a stage: t %.2f | arm %.4f (raw %.4f) brel %.4f "
                     "(raw %.4f) | seat %.4f/%.4f board %.4f/%.4f | "
                     "tilt %.1f deg | a %.2f (fd %.2f) |v| %.2f |w| %.2f tk %d "
                     "| air %.2f hull %.3f susp %.4f rolled %d "
                     "| force %s | pose %.2f "
                     "Hz | case %s",
                     static_cast<double>(sm.body_dbg_tick_total) *
                         static_cast<double>(rig.dt_s),
                     static_cast<double>(sm.stage_arm),
                     static_cast<double>(sm.stage_arm_raw),
                     static_cast<double>(sm.board_release),
                     static_cast<double>(sm.board_release_raw),
                     sm.rl_last.seat_frac,
                     sm.rl_ref.seat_frac, sm.rl_last.board_frac,
                     sm.rl_ref.board_frac, static_cast<double>(tilt_deg),
                     static_cast<double>(sm.body_dbg_accel),
                     static_cast<double>(sm.body_dbg_fd),
                     static_cast<double>(sm.body_dbg_v),
                     static_cast<double>(sm.body_dbg_w), sm.body_dbg_ticks,
                     static_cast<double>(rig.air_s),
                     static_cast<double>(rig.hull_engage),
                     static_cast<double>(rig.susp_sum_m), rig.rolled ? 1 : 0,
                     force_s,
                     static_cast<double>(body_pose_hz()),
                     rider_case_name(sm.rl_last.rcase));
        }
    }

    // ---- the headlight beam, drawn after the opaque machine so the hood and
    // the terrain occlude it (depth TEST on, depth WRITE off).
    if (sm.beam_ok) {
        const int nlens = find_node(sm, "headlight_lens");
        const glm::mat4 lensx = mount * sm.nodes[nlens].world;
        // From the BULB: the node origin is the sled root down at the pan, the
        // glass centre is the lamp on the hood (Chad: the beam must "shine
        // forward and outward from the light bulb").
        const glm::vec3 apex(lensx * glm::vec4(sm.lens_center, 1.0f));
        // STRAIGHT AHEAD is the machine's forward, not the lens normal -- the
        // lens sits in a front face raked 38 deg, so its own normal would aim
        // the beam at the snow a few metres out.
        const glm::vec3 f0 =
            glm::normalize(glm::vec3(mount * glm::vec4(0.0f, 0.0f, 1.0f, 0.0f)));
        glm::vec3 upm =
            glm::normalize(glm::vec3(mount * glm::vec4(0.0f, 1.0f, 0.0f, 0.0f)));
        // aim it DOWN at the snow, the way a headlight is actually aimed
        const float da = kBeamDownDeg * 0.01745329f;
        const glm::vec3 fwd =
            glm::normalize(f0 * std::cos(da) - upm * std::sin(da));
        const glm::vec3 rgt = glm::normalize(glm::cross(upm, fwd));
        upm = glm::cross(fwd, rgt);
        glm::mat4 bx(1.0f);
        bx[0] = glm::vec4(rgt, 0.0f);
        bx[1] = glm::vec4(upm, 0.0f);
        bx[2] = glm::vec4(fwd, 0.0f);
        bx[3] = glm::vec4(apex, 1.0f);

        // NIGHT GATE. A real beam is invisible in daylight air and leaving it
        // lit reads as a bug, so the shaft fades out through civil twilight.
        // The lens and the gauges stay hot around the clock.
        const glm::vec3 up = glm::normalize(glm::vec3(pos));
        const float elev = -glm::dot(sun_dir, up);
        float s = (elev + 0.10f) / 0.25f;
        s = s < 0.0f ? 0.0f : (s > 1.0f ? 1.0f : s);
        const float night = 1.0f - s * s * (3.0f - 2.0f * s);
        float intensity = 0.04f + (1.5f - 0.04f) * night;
        // SEADS_SLED_BEAM pins the intensity for A/B without a rebuild, and
        // lets a daylight --smoke shot show the beam (mirrors SEADS_SLED_SAG0).
        // SEADS_SLED_BEAM=0 is the off switch.
        if (const char* e = std::getenv("SEADS_SLED_BEAM"))
            intensity = static_cast<float>(std::atof(e));

        // Two hull crossings from outside, one from within -- without this the
        // beam halves in brightness as the camera enters it.
        const float ta = glm::dot(-apex, fwd);
        const float tc = ta < 0.0f ? 0.0f : ta;
        const float hull = kBeamHullPad * (kBeamR0 + tc * kBeamSpread);
        const bool inside = ta > -kBeamSink && ta < kBeamRange &&
                            glm::length(-apex - ta * fwd) < hull;
        float cross_n = inside ? 1.0f : 0.5f;
        float r0 = kBeamR0, spread = kBeamSpread, range = kBeamRange;
        const float tint[3] = {1.0f, 0.95f, 0.85f};  // halogen

        SetShaderValue(sm.beam_shader, sm.lb_apex, &apex[0], SHADER_UNIFORM_VEC3);
        SetShaderValue(sm.beam_shader, sm.lb_dir, &fwd[0], SHADER_UNIFORM_VEC3);
        SetShaderValue(sm.beam_shader, sm.lb_r0, &r0, SHADER_UNIFORM_FLOAT);
        SetShaderValue(sm.beam_shader, sm.lb_spread, &spread, SHADER_UNIFORM_FLOAT);
        SetShaderValue(sm.beam_shader, sm.lb_range, &range, SHADER_UNIFORM_FLOAT);
        SetShaderValue(sm.beam_shader, sm.lb_intensity, &intensity,
                       SHADER_UNIFORM_FLOAT);
        SetShaderValue(sm.beam_shader, sm.lb_tint, tint, SHADER_UNIFORM_VEC3);
        SetShaderValue(sm.beam_shader, sm.lb_cross, &cross_n, SHADER_UNIFORM_FLOAT);

        BeginBlendMode(BLEND_ADDITIVE);
        rlDisableBackfaceCulling();
        rlDisableDepthMask();
        DrawMesh(sm.beam_mesh, sm.beam_mat, to_ray_m(bx));
        rlEnableDepthMask();
        rlEnableBackfaceCulling();
        EndBlendMode();
    }
    return true;
}

}  // namespace render
