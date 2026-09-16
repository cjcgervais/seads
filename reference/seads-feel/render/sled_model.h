#pragma once
// ★ WINTER S3-ART GLTF WIRING (GLTF_WIRING_HANDOFF.md, render/app ONLY).
// The Blender hero sled (vehicle_program/blender/indy650.glb) drawn in
// drive_mode, its SLED_ROOT channels posed from sim::SledState each frame.
// The house one-number rule: every value here arrives FROM the kernel's
// state (susp_x, steer_actual, rider_lat/fwd/up) -- this module holds NO
// animation state of its own, only the rest pose it loaded.
//
// Blender drivers/constraints do NOT survive glTF export, so the channel
// wiring the rig contract describes as "drivers" is re-implemented here as
// node-transform overrides on the same named nodes the contract names
// (CH_steer_pivot, CH_susp_L/R/T, rider_rig bones), plus a 2-bone IK that
// re-welds hands to grip sockets and boots to board sockets -- the IK the
// .blend solved live.

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

// ★ R4c: the man, once he is off the machine, is `sim::WalkerState` -- kernel
// state like everything else on this rig. FORWARD-DECLARED, not included: this
// header sits on every render TU's include path. render/ may include sim/
// (tools/graph/layer_rules.toml); sled_model.cpp takes the full header.
namespace sim {
struct WalkerState;
// ★★★ GAIT LADDER G1 (docs/PLAN_20260904_gait_ladder.md): the per-foot
// terrain pin/plant, stepped beside the walker in app/. Forward-declared for
// the same reason `WalkerState` is -- this header sits on every render TU's
// include path, and `sled_model.cpp` takes the full `sim/gait.h`.
struct GaitState;
}

namespace render {

// Channel values for one frame. Kernel conventions throughout:
// steer +1 = LEFT, lean_lat +LEFT, lean_up + standing / - tucked,
// lean_fwd + forward, susp_m = compression [m] in kernel patch order
// {ski L, ski R, track}.
struct SledRig {
    float steer = 0.0f;
    float susp_m[3] = {0.0f, 0.0f, 0.0f};
    float lean_lat_m = 0.0f;
    float lean_up_m = 0.0f;
    float lean_fwd_m = 0.0f;
    // ★ DEFECT 8 RETIRED (SUDBURIAN_LADDER §1.1, rung R1a). This was hard-coded
    // 0 forever -- "no kernel state yet -- live 0" -- so the rider had NO
    // terrain-reaction channel at all, which is a quarter of what "floaty" is.
    // It is now composed by render::rider_absorb() (render/rider_pose.h, pure
    // and unit-tested) from SledState::susp_x + susp_v, both read-only, zero
    // kernel change. Still [0,1]; still exercised by the channel smoke env var.
    float absorb = 0.0f;  // [0,1] rider crouch (bump soak)
    // ★ R4a SELF-RIGHT: the leg that shoves. Chad, after driving v3: "we didnt
    // have any animation of the leg actually pushing the sled. The leg should
    // extend to push it visually otherwise it looks terrible."
    // Signed [-1,1], + = he is bracing LEFT, so the RIGHT leg is the one
    // straightening into the machine. 0 = not righting, and then every foot
    // target below is bit-identical to the signed R3 pose.
    float right_push = 0.0f;
    // ★ R2c-5 THE CONTROL INPUTS, and they are the KERNEL's own, not a second
    // animation state: sim::SledInputs::throttle / ::brake, the exact numbers
    // step_sled was handed this tick. throttle poses the RIGHT arm (elbow down,
    // hand up, thumb in), brake the LEFT (elbow up, hand down over the bar) --
    // the ruling and the geometry are at render/rider_pose.h :: R2c-5.
    float throttle = 0.0f;  // [0,1]
    float brake = 0.0f;     // [0,1]
    float cam_yaw = 0.0f, cam_pitch = 0.0f;  // head follows the camera (rad)
    // ★ THE SCARF CHANNEL (docs/SCARF_SPEC.md §4, ladder §5). The trailing
    // chain needs the machine's own motion and the sim's own clock, and gets
    // BOTH from the kernel -- render holds no animation state and reads no
    // wall clock (CLAUDE.md house law).
    //   vel_body_mps: sim::SledState::velocity rotated into the BODY frame
    //                 (+X right, +Y up, -Z forward), so forward travel at V is
    //                 (0, 0, -V) and the relative wind is its negation.
    //   ticks:        fixed sim ticks consumed THIS frame (app::FrameResult),
    //                 = the number of solver sub-steps to run. 0 on a frame the
    //                 accumulator did not fire: the scarf poses, it does not
    //                 advance.
    //   dt_s:         the sim dt (params.sim_dt). One fixed sub-step, every
    //                 sub-step, forever.
    glm::vec3 vel_body_mps{0.0f};
    int ticks = 0;
    float dt_s = 0.0f;
    // ★ R4a, AND CHAD RULED THE SCARF GETS IT (2026-08-27). The scarf solves
    // in the machine's MODEL SPACE -- a frame that accelerates and rotates --
    // so until now it knew only "how fast am I going" (vel_body_mps) and
    // "which way is down" (gravity, rotated in each frame). It did NOT know
    // the machine was throwing it around: brake hard and it kept trailing
    // instead of flying forward past his shoulder; hold a corner and it hung
    // straight back instead of swinging outboard.
    //
    //   omega_body_rps: sim::SledState::angular_vel, which is ALREADY body
    //                   frame [rad/s]. A pure read of shipped kernel state --
    //                   render finite-differences it for alpha rather than
    //                   taking the kernel's own, because the kernel's copy
    //                   lives only inside SledDebugSubstep under `if (dbg)`
    //                   and wiring a sink into the shipped game to reach it
    //                   would be a tape-visible change (trail_chain.h).
    //   epoch:          bumped by the app on every write to `sled` OUTSIDE
    //                   step_sled -- the mount seed and KEY_R autoright, the
    //                   same two sites that emit a tape O record. A finite
    //                   difference across one of those reads a teleport as
    //                   hundreds of g, and the model-space anchor does NOT
    //                   move on a respawn, so the solver's own teleport test
    //                   cannot see it. This is that discontinuity, named at
    //                   its source rather than guessed at from a threshold.
    glm::vec3 omega_body_rps{0.0f};
    long epoch = 0;
    // ★★★ R4a THE ARMING RUNG. The three SledParams the rider-load rod
    // model needs, and they arrive from the kernel like everything else here
    // (the one-number rule): a render-side copy of any of them would be a
    // second opinion about the machine's own mass budget.
    //   rider_mass_kg / mass_kg  set the rider fraction cg_off is scaled by;
    //   cg_height_m              sets the model->body DROP (sim/sled.h K2).
    // All three ZERO means "not supplied" and the stage machine stays OFF --
    // a rider-load model built on a zero mass would divide by it.
    double rider_mass_kg = 0.0;
    double mass_kg = 0.0;
    double cg_height_m = 0.0;
    // ★ R4a INSTRUMENT ONLY -- the contact story, for SEADS_BODY_CHAIN=2.
    // The rider-load selector is frictionless and planar, so it cannot tell
    // "leaned over on snow" from "inverted in the air"; these say which it
    // was. Nothing but the log line reads them. See render/draw.h.
    float air_s = 0.0f;        // sim::SledState::air_s
    // ★ SHIPPED (not instrument): sim::SledComfort::rolled_grace_s, the
    // window `free_frac` normalises air_s by. See render/rider_load.h.
    float air_grace_s = 0.0f;
    float hull_engage = 0.0f;  // ::hull_engage_lp
    float susp_sum_m = 0.0f;   // sum ::susp_x
    bool rolled = false;       // ::rolled
    // ★★★ R4a §7.3 STAGE 4 -- THE ONE-WAY LATCH, READ STRAIGHT OFF THE KERNEL
    // (`sim::SledState::grip.attached`). TRUE for every frame this rung has
    // ever drawn and for every reference/probe solve in this file, so the
    // departure is off-by-default in the strongest sense: the branches it gates
    // cannot be entered by a rig that does not say so.
    bool grip_attached = true;
    // ★★★ R4c: the man himself, non-owning, stepped by the kernel. NULL is a
    // legal state and means "nobody has ever come off in this session" -- the
    // draw then takes every branch it took before this rung existed.
    const sim::WalkerState* walker = nullptr;
    // ★★★ GAIT LADDER G1: his feet, stepped beside `walker` in app/ off the
    // SAME `sim::WalkerState` this rig already carries. NULL is legal and
    // means the pre-G1 hip-hang law: see the `gait_on` branch in
    // `pose_pass`'s legs section, which falls back to it exactly the way a
    // null `walker` falls back to the seated/welded pose. `SEADS_WALK_FOOTDROP`
    // survives as an additive trim on top of either path.
    const sim::GaitState* gait = nullptr;
    // ★★★ GAIT LADDER G2i -- THE SHIFT JUMP. `sim::HopState::height_m`, the
    // metres he is off the ground in a deliberate hop. It rides the SAME
    // channel the burial sink already rides (a world-up displacement folded
    // into `flight_off_model`), for the same reason: render must not have a
    // second opinion about where the man is. 0 on every frame he is standing
    // on the snow, which is every frame this repo drew before G2i.
    float hop_height_m = 0.0f;
    // ★★★ GAIT LADDER G2j -- THE PUMP-REPAIR WORK POSE. `work` is the phase
    // of the wrench cycle in [0,1) (published by `sim::step_repair_work`, one
    // owner) and `work_on` says the man is at the pump with his hands on it;
    // `work_blow` is the one-shot hammer-fist at the moment the pump comes
    // whole, 1 at the strike's start and decaying to 0. All three are 0/false
    // in every other mode, and the arm branch that reads them is gated on
    // `work_on` alone -- the signed riding pose and the walking arms are
    // untouched by construction.
    bool work_on = false;
    float work_phase = 0.0f;
    // ⚠ TWO FIELDS, NOT ONE, AND THE REASON IS A SIGN: `work_blow` is the
    // SWING (+1 fist raised, -1 driven down) and it CROSSES ZERO twice in
    // every blow, so it cannot also serve as the "a blow is happening"
    // test. `work_blow_on` is that test.
    bool work_blow_on = false;
    float work_blow = 0.0f;
    // ★★★ ST-5, THE HAND HOOK (Chad 2026-09-04, VERBATIM: "onto the hand hook
    // to make exception and grasp the stock of the launcher").
    //
    // This is a TARGET SUBSTITUTION, not a second solver. The hands-on-bars
    // weld below (sled_model.cpp, "IK: hands welded to the (steering) grips")
    // keeps every line it has: all this does is move the point it welds TO,
    // from the machine's `grip_socket_L/R` toward a world point the caller
    // names, and the existing two-bone arm IK bends the elbow to follow. The
    // launcher's own frame is solved in render/draw.cpp BEFORE this draw runs
    // precisely so the target is not one solve stale.
    //
    // ⚠ WEIGHT 0 IS BIT-IDENTICAL, and that is the contract this rung ships
    // under (this file is the r4a lane's): at 0 not one term below is
    // evaluated and the signed riding pose is untouched, which is the state
    // every frame drawn before ST-5 was in and every frame with the launcher
    // stowed still is.
    struct HandGrip {
        // ABSOLUTE world (doubles -- the drive surface is a planet radius from
        // the origin), the point the hand's contact should sit on.
        glm::dvec3 pos{0.0};
        // The world axis of the grip CYLINDER, so the hand keeps the wrap it
        // holds a handlebar with and only the thing it wraps changes. Its
        // SIGN is not load-bearing: the weld picks the end that his knuckles
        // already point at, measured against the live bar axis.
        glm::dvec3 lat{0.0};
        float weight = 0.0f;  // [0,1]; 0 = the bars, untouched
    };
    // ⚠ ANATOMICAL, not the model side the rig indexes its chains by: [0] is
    // his LEFT hand, [1] his RIGHT. sled_model.cpp maps them onto its own
    // sides through the throttle side it already DERIVES from the shipped
    // geometry at load (capture_rest_ik) -- the one place in this file that
    // knows a left from a right, and it knows it by measurement.
    HandGrip hand_grip[2];
    // ★★★ ST-5 THE SWEEP (Chad 2026-09-05, VERBATIM: "the sudburian shall
    // follow the aim ... they shall twist head and torso in addition to the
    // arms"). The launcher's aim, EXPRESSED IN THIS MAN'S OWN FRAME:
    // `sting_aim_az` positive to his RIGHT off the machine's heading,
    // `sting_aim_el` positive UP off the machine's horizontal. Both radians.
    //
    // ⚠ THEY ARE ANGLES, NOT A DIRECTION, and that is what keeps this file
    // out of the mode business: render/draw.cpp projects the world aim ray
    // into the sled basis and hands over two numbers, so sled_model.cpp never
    // learns what a PlayerMode is, never sees a world vector, and cannot
    // disagree with the launcher about which way the machine faces.
    //
    // ⚠ INERT BY THE SAME KEY AS EVERYTHING ELSE IN THIS HOOK: the twist is
    // scaled by the larger hand-grip weight, so at weight 0 -- every frame
    // with the launcher stowed, every probe pass, every load-time capture --
    // these two are multiplied by zero and the signed riding pose is
    // bit-identical whatever they hold.
    float sting_aim_az = 0.0f;
    float sting_aim_el = 0.0f;
};

// Draw the hero sled. pos = kernel CG (world), basis = body->world
// (+X right, +Y up, -Z forward), cg_h = CG height above the running
// surface. Lazy-loads the GLB on first call; returns false when the model
// is unavailable (caller falls back to the procedural placeholder).
bool sled_model_draw(const glm::dvec3& pos, const glm::dmat3& basis,
                     const glm::dvec3& eye, const glm::vec3& sun_dir,
                     double cg_h, const SledRig& rig);

// ★ THE SUDBURIAN'S BACK, AS THE RIG POSED IT THIS FRAME (Chad 2026-08-25:
// "make it pasted to his white/grey sweater ... currently it is on his head").
// This is NOT a new measurement: it is the SAME posed pelvis/neck torso frame
// and derived back normal the scarf's §3b torso plane already runs on
// (render/sled_model.cpp), published so the SK-1c back HUD can sit ON the
// sweater instead of guessing an offset off the sled body frame -- which is
// how it ended up at head height. `valid` is false on any frame the hero rig
// did not draw (GLB missing, placeholder forced), and the HUD then falls back.
struct RiderBack {
    bool valid = false;
    // ABSOLUTE world (doubles: the drive surface is a planet radius from the
    // origin, and a float world position there is metres of slop).
    glm::dvec3 pelvis{0.0};
    glm::dvec3 neck{0.0};
    // World DIRECTIONS -- unit, float is exact enough for a rotation.
    glm::vec3 back_normal{0.0f};  // out of the sweater's back
    glm::vec3 width_axis{0.0f};   // across the shoulders
    float back_surface = 0.0f;    // [m] suit surface behind the torso plane
};
// The frame the LAST sled_model_draw() posed. Read it after that call.
const RiderBack& sled_model_rider_back();

// ★ MULTIDENT: cosmetic crash-damage helmet (0 = pristine, 1..4 = baked
// dent variants; U cycles in-game; crash logic may set it directly).
void sled_helmet_dent_set(int degree);
int sled_helmet_dent_get();

// ★ F-POSE: hide the sled's rider (all skinned prims) while the player mans
// the flak gun -- render/flak_gunner.cpp draws him AT the gun instead. Set
// per frame by app/main.cpp; false = the picture this machine has always
// drawn, bit for bit.
void sled_rider_hide_set(bool hidden);

}  // namespace render
