#pragma once
// ★★★ GAIT LADDER G1 — FEET MEET THE GROUND (docs/PLAN_20260904_gait_ladder.md
// §4). Builder rung; master lane is Fable, `D:\seads_sandboxes\gait`.
//
// WHY THIS FILE EXISTS, AND WHY IT IS BESIDE `sim/walker.{h,cpp}` AND NOT
// INSIDE IT.
//
// `sim/walker.h` is FROZEN SURFACE (R4c / `adef92479`, restated by the plan's
// L-FROZEN): `WalkerState` does not grow, `step_walker`'s signature does not
// move. The walker already publishes everything a foot needs to plant itself
// -- `gait_phase` (distance-advanced, monotonic), `stride_m`, `depth_m`,
// `pos`/`heading` -- and G1's whole job is to turn that ONE phase into TWO
// world-space foot placements. That is new state (one pin + one predictive
// plant per foot) and a new pure step function, so it gets a new TU rather
// than growing the frozen one.
//
// ★ L-NPC (ruling 2, PLAN §1.2): `GaitState` is a VALUE TYPE with no globals
// and no player lookups inside the math below -- N copies are legal, and G8
// spawns a crowd of them. Every function here takes its state, params and
// world by reference/value and touches nothing else.
//
// ★ L-PIN: a stance foot is a stored WORLD point (+ yaw) for the WHOLE stance
// window. `target_w` during stance is a bit-for-bit copy of `pin_w` -- it is
// never recomputed, so it cannot drift even if the terrain under it does.
//
// ★★★ RED-TEAM FIX (G1b, P1): THE GROUND GUARD IS A TEMPORAL-GLITCH FILTER,
// NOT A SPATIAL ONE. G1's first cut compared every new ground sample against
// the last-accepted radius NO MATTER WHERE IT WAS TAKEN -- so a normal 7-13%
// grade (an entirely ordinary hill) tripped the same ">0.15 m" guard meant
// for a glitched field, and a REJECTED sample never updated the history, so
// the foot LATCHED at the wrong radius forever (a full-strut leg). The fix:
// the guard only ever compares TWO READS OF THE SAME POINT (stance-entry
// re-sampling the exact direction swing-start already sampled for that
// plant), and even then it does not reject-and-hold -- it RATE-LIMITS,
// moving the accepted radius toward the new sample by at most 0.15 m per
// event, so a real glitch converges out within a few steps instead of
// latching forever. A sample at a DIFFERENT point (the next plant, a stride
// away) is accepted outright: a grade is not a glitch, and the guard must
// never fight ordinary terrain.
//
// ★★★ THE INDEX CROSSOVER, NAMED ONCE HERE SO IT IS NEVER RE-DISCOVERED. This
// header's contract says `foot[0] = left, foot[1] = right` (the plan's own
// words). But `render/rider_rig.h`'s `RiderChainSpec::model_side` runs the
// OTHER way round: the Sudburian's anatomical `_l` bones carry `model_side 1`
// and `_r` carries `model_side 0` (the crossover is measured and stated
// there). And `render/sled_model.cpp`'s existing (pre-G1) leg-swing consumer
// reads `sm.leg[s]` with phase offset `(s == 0 ? 0.0 : 0.5)` -- and `sm.leg[0]`
// IS `model_side 0`, i.e. the anatomical RIGHT leg. So in the code's own
// reality: RIGHT gets phase+0.0, LEFT gets phase+0.5. This header keeps BOTH
// facts true at once by putting the LEFT foot (index 0, per this header's own
// contract) at phase+0.5 and the RIGHT foot (index 1) at phase+0.0 --
// `step_gait`'s body says so again, in place, where it is computed. The
// consumer in `render/sled_model.cpp` is the one place that has to cross the
// wire back (`sm.leg[0]` <- `gait.foot[1]`, `sm.leg[1]` <- `gait.foot[0]`),
// and it says so too. Two crossovers, cancelling, both named: that is the
// alternative to guessing which one a first read of this file would be.

#include <glm/vec3.hpp>

#include "sim/walker.h"
#include "world/snowpack.h"

namespace sim {

// One foot, entirely in WORLD space (a planet-radius sphere, doubles, exactly
// like `WalkerState::pos` -- CLAUDE.md's sphere invariants apply here too).
struct GaitFoot {
    // The stance anchor: where this foot last touched down, snapped to the
    // sampled ground. Held fixed for the whole stance window (L-PIN).
    glm::dvec3 pin_w{0.0};
    // The foot's yaw at the moment the pin was set (radians, an arbitrary but
    // stable local convention -- see `yaw_of` in gait.cpp). Published for a
    // future rung (G5's turn-plant); G1 does not consume it.
    double yaw = 0.0;
    bool in_stance = true;
    // The predictive swing target, computed once at swing start and held
    // until the foot plants (or the next swing recomputes it).
    glm::dvec3 plant_w{0.0};
    // Slope pitch at the plant, radians: + is toe-up (heel low, toe high),
    // from a heel/toe pair of ground samples 0.22 m apart along heading.
    double pitch_rad = 0.0;
    // False only for a default-constructed `GaitFoot` that `step_gait` has
    // never seen: the seed branch reads this, not `in_stance`, because a
    // fresh foot has no stance to be "in" yet.
    bool valid = false;

    // --- fields beyond the plan's literal struct text, and why they exist:
    // consumption (`render/sled_model.cpp`) may not run gait math (L-PURE),
    // so the FINAL blended world target for THIS frame has to be a published
    // fact, not something render re-derives from `pin_w`/`plant_w` and the
    // walker's own swing curve. `target_w` is exactly that: `pin_w` verbatim
    // in stance, the pin-toward-plant Hermite blend (reusing
    // `walker_foot_offset`'s shape, never a second curve) in swing.
    glm::dvec3 target_w{0.0};
    // The ground-sample safety net (see the red-team banner above): the last
    // ACCEPTED drive radius AND the direction it was read at, carried across
    // steps so a temporal glitch at ONE point converges out instead of
    // latching, while a sample at a genuinely different point (ordinary
    // terrain) is never fought. `last_ground_r < 0.0` = no history yet (the
    // very first sample at a point is always accepted outright).
    double last_ground_r = -1.0;
    glm::dvec3 last_ground_dir{0.0};

    // ★★★ G2i-d -- THE SWING'S OWN ORIGIN, and it exists for exactly ONE
    // event: a TOUCHDOWN that catches this foot mid-swing. The swing blend
    // is driven by `gait_phase`, which keeps advancing the whole time he is
    // in the air (L-PHASE: a rung may READ the phase, never reset it), so
    // the frame he lands the phase can say "you are 60% through a step"
    // while the boot is actually hanging under his hip in the landing pose.
    // Blending from the re-anchored pin at t=0.60 would teleport the foot
    // 60% of a stride forward in one frame. `swing_t0` records the blend
    // parameter AT the touchdown and the swing is re-parameterised onto
    // [t0, 1] -> [0, 1]: the foot resumes its step FROM WHERE IT IS and
    // still arrives at the same plant on the same phase. `swing_up0` does
    // the same for the lift so the boot does not pop up either. Both are 0
    // on every ordinary swing -- and the remap is GUARDED on `t0 > 0`, so
    // the no-hop path runs the identical arithmetic it always did.
    double swing_t0 = 0.0;
    double swing_up0 = 0.0;
};

struct GaitState {
    GaitFoot foot[2];  // 0 = left, 1 = right (this header's own convention;
                       // see the crossover banner above for how a consumer
                       // maps it onto `model_side`)

    // --- GAIT LADDER G2 -- THE PELVIS LIVES (PLAN §4) -----------------------
    // How far the pelvis sits below its rest height, metres, ALWAYS >= 0:
    // "over-reach lowers the pelvis, never straightens the knee." A
    // critically damped spring's settled output (see `gait_spring_step` and
    // `step_gait`'s use of it) -- `pelvis_drop_vel_mps` is that spring's own
    // velocity, carried across steps so the response is second-order and
    // cannot ring, not a static hidden inside the function.
    double pelvis_drop_m = 0.0;
    double pelvis_drop_vel_mps = 0.0;
    // Vertical bob (+up/-down, metres), lateral sway (metres, + toward
    // `left = cross(up, fwd)`) and Trendelenburg roll (radians, + toward
    // `left` about `fwd`) -- periodic functions of phase, published fresh
    // every step (the gait cycle IS the smoothing; see the pure functions
    // below for the sign conventions and why no per-side crossover applies
    // to these three).
    double bob_m = 0.0;
    double sway_m = 0.0;
    double roll_rad = 0.0;

    // ★ G2i-d: was he in the air LAST step? The touchdown is an EDGE, and
    // `GaitAir` only carries a level (`height_m`), so the edge has to be
    // remembered on this side. Nothing else reads it.
    bool was_air = false;
};

// ---------------------------------------------------------------------------
// ★★★ GAIT LADDER G2 -- THE PELVIS LIVES (+ THE RULED RUN SPEED), PLAN §4.
//
// G1 shipped two feet that plant and hold; the debt it left, named and
// measured in its own banner (`gait.cpp`'s stance-entry comment, and PLAN
// §6's G1c ledger entry) is that a RIGID-HEIGHT hip cannot span a real
// stride -- late stance rides the 0.98*reach clamp on ordinary flat ground,
// which is a straight-kneed lunge by construction however good the feet
// underneath it are. A human does not solve this by straightening a joint
// past where it goes; a human LOWERS THE PELVIS. That is the whole of G2's
// physics: pelvis height, sway and roll are new PUBLISHED numbers, still
// pure, still stepped beside (never inside) the frozen walker.
//
// ★ THE SEAM (why `GaitBodyGeom`'s defaults are constants, not a live
// measurement): the three lengths this rung's drop law needs -- the leg
// chain, the hip half-span, the standing pelvis height -- are already
// MEASURED, in `render/rider_rig.cpp` (thigh 0.453 + calf 0.455 = 0.908 m
// chain, 0.095 m hip half-span) and in the standing rest pose (~0.980 m
// pelvis height -- the same number `render/sled_model.cpp`'s pre-G2 fallback
// used at its `n_pelvis` node length). `sim/` cannot include `render/` (the
// walker would stop being testable off a bare snowpack, and L-PURE runs the
// other way: render consumes sim, never the reverse), so this header carries
// those three numbers as its own DEFAULTED parameters, MIRRORING the
// measurement rather than reading it live. If the rig is ever re-measured,
// this default and `rider_rig.cpp`'s row move together, by hand, together --
// there is no other seam today.
struct GaitBodyGeom {
    double leg_reach_m = 0.908;      // thigh 0.453 + calf 0.455, rider_rig.cpp
    double hip_half_span_m = 0.095;  // rider_rig.cpp's measured hip offset
    double pelvis_rest_h_m = 0.980;  // standing hip height above the ground
    // The envelope the drop law keeps clear of, short of render's own
    // 0.98*reach safety-net clamp (sled_model.cpp) on purpose: this law
    // should never itself be the thing riding the net.
    // ★★★ RED-TEAM FIX (G2, own build round): 0.96 left only an 18 mm gap
    // to render's own 0.98*reach safety net -- and a critically damped
    // spring TRACKING A RAMPING DEMAND (the hip-to-pin distance grows
    // through the whole of a stance, not just at footfall) always carries a
    // nonzero steady-state lag behind that ramp. Measured on this rung's own
    // smoke run (a one-off stderr probe): the spring lagged the live demand by
    // ~0.1-0.17 m during an ordinary stance -- enough, against an 18 mm
    // margin, to ride the render clamp for most of every stance. 0.85 widens
    // the margin to ~120 mm, past the measured lag with headroom, without
    // asking the spring to be unrealistically stiff (this is a LOOK dial,
    // not a physics one -- the RENDER clamp at 0.98 stays the one true
    // "never send an unreachable target" net; this margin exists so the drop
    // law is normally the thing doing the work, not the net).
    double reach_frac = 0.85;
};

// One critically damped spring step, `omega_rps` rad/s: exposed so a test can
// grade convergence (settles on target, never overshoots) on the bare
// function, and so `step_gait`'s pelvis-drop uses this rather than a second,
// silently different copy. `v` is the caller's OWN spring velocity (stored on
// `GaitState`, never a static -- L-NPC).
double gait_spring_step(double x, double& v, double target, double omega_rps,
                        double dt_s);

// How much of a walk-vs-run gait this is, purely from the SAME blend
// `walker_stance_frac` already computes -- NEVER a new threshold (L-DEPTH):
// 0 at the measured walk floor (stance_frac 0.68), 1 at the measured jog
// ceiling (stance_frac 0.38), continuous between and clamped at the ends.
double gait_run_blend_t(double stance_frac);

// The pelvis's vertical bob, lateral sway and Trendelenburg roll: pure
// functions of phase and stance fraction, exposed so `test_gait.cpp` can
// grade period, sign and phase without stepping a whole walker. The
// contact-relative offset is the measured mid-stance for a walk and a fixed
// 8% cycle lag past contact for a run (plan §4 G2), blended by
// `gait_run_blend_t`. Amplitudes are the plan's own: bob 0.035 m walk /
// 0.09 m run, sway 0.045 m walk / 0.02 m run, roll ~1.5 degrees (the caller
// passes them in so a test can probe arbitrary values).
//
// Sign convention, and why no per-foot crossover applies here (contrast the
// index convention on `GaitFoot` above): these three describe the WHOLE
// pelvis along axes a consumer already holds -- `up` and
// `left = cross(up, fwd)`, the SAME construction `step_gait` uses internally
// and the same construction a render consumer builds from its own
// `flight_up_model` / `flight_fwd_model`. A proper rotation carries a cross
// product's sense with it, so the sign published here lands correctly in
// model space with no per-side wire to cross.
//   `gait_vertical_bob_m`          + is UP (a rise), - is DOWN (the low
//                                  point, timed at mid-stance for a walk).
//   `gait_lateral_sway_m`          + is toward `left` -- toward whichever
//                                  foot is in stance (the drunk test: wrong
//                                  sign sways AWAY from the stance foot).
//   `gait_trendelenburg_roll_rad`  a rotation about `fwd`: + tips the pelvis
//                                  toward `left`, i.e. toward the SWING side
//                                  at the same instant the sway leans toward
//                                  stance -- opposite sign from the sway by
//                                  construction (see gait.cpp).
double gait_vertical_bob_m(double phase01, double stance_frac,
                          double bob_walk_m, double bob_run_m);
double gait_lateral_sway_m(double phase01, double stance_frac,
                          double sway_walk_m, double sway_run_m);
double gait_trendelenburg_roll_rad(double phase01, double stance_frac,
                                   double roll_amp_rad);

// ---------------------------------------------------------------------------
// ★★★ GAIT LADDER G2i -- THE SHIFT KEY. Chad, 2026-09-05: "make the shift key
// jump press and also speed up his run a tad while pressing and holding; for
// about 5 seconds a dash will start on a jump press when landing from it",
// clarified: "holding shift for jump sprint sequence -- sprint only if shift
// is held."
//
// ★★★ WHY THE HOP IS NOT A `WalkerMode`, AND THAT IS THE WHOLE DESIGN. The
// obvious place to put "he is in the air" is `WalkerMode::Falling` -- and it
// is the wrong place twice over. `WalkerMode`'s own banner says the ladder is
// ONE-WAY past stage 4: Falling leads to Buried/Down and then to the crawls
// and the get-up, i.e. a deliberate hop would throw the man on his face and
// cost him four seconds of getting back up. And `WalkerState` is FROZEN
// SURFACE (L-FROZEN) -- it does not grow. So the hop lives BESIDE the walker,
// exactly the way `GaitState` does: a value type, stepped by a pure function,
// N copies legal (L-NPC -- a millwright NPC that can hop needs one of these
// and nothing else). The walker never learns about it; it keeps walking, and
// the hop is a VERTICAL OFFSET that render spends and that `step_gait` lifts
// the feet by, so the man leaves the ground without leaving `Afoot`.
//
// ★ AND THE SPEED-UP IS A MULTIPLIER, NOT A ROW (L-DEPTH / L-ONE-DIAL). The
// three depth anchors in `WalkerParams` are the ruled shape of "how fast can a
// man move through THIS snow"; a sprint does not change that shape, it scales
// it. So `hop_speed_mul` returns ONE number the caller multiplies all three
// anchors by before stepping the walker -- the depth curve, and `walker_stride`
// which reads the cap through it, follow for free with no second stride law.
struct HopParams {
    // The hop itself. 3.2 m/s off the ground against 9.81 is a ~0.52 m rise
    // and ~0.65 s of air: a man's standing hop, not a video-game leap. Both
    // are named dials because the FEEL is Chad's to sign, not a measurement.
    double jump_speed_mps = 3.2;
    double gravity_mps2 = 9.81;
    // "speed up his run a tad while pressing and holding" -- 1.15 takes the
    // ruled 5.5 m/s hardpack cap to ~6.3 and the deep-snow trudge from 0.6 to
    // ~0.69, i.e. the SAME tad everywhere, which is what a multiplier means.
    double sprint_cap_mul = 1.15;
    // "for about 5 seconds a dash will start on a jump press"
    double dash_charge_s = 5.0;
    double dash_cap_mul = 1.8;
    double dash_decay_s = 1.7;
    // ★★★ THE ONE PLACE THIS RUNG HAD TO DECIDE SOMETHING CHAD DID NOT SAY,
    // AND IT IS A DIAL SO HE CAN OVERRULE IT WITH A NUMBER. The spec as handed
    // down says the arm is cleared "when Shift is released" -- but the arming
    // gesture is HOLDING Shift, and the trigger is a Shift PRESS, and you
    // cannot press a key you are already holding without letting go of it
    // first. Cleared strictly on release, the dash would be unreachable by
    // construction. So the arm survives a release SHORTER than this window --
    // long enough to re-press, too short to be "he stopped sprinting".
    double dash_arm_grace_s = 0.6;

    // ★★★ G2i-b -- THE AIR POSE. Chad, flying G2i: "jump should have a knee
    // forward or both knees bent a bit while in the air because they kind of
    // hang back there right now... let's fix the jump to make it look a
    // little more believable."
    //
    // ★ THE LAW IS ONE LINE: THE TUCK IS THE HEIGHT. G2i rode both foot
    // targets rigidly up with the body, so the legs kept the stride pose they
    // left the ground in and trailed behind him -- the hang he saw. The fix
    // is a tuck, and the honest driver for it is the hop's OWN normalized
    // height: he pushes OFF a straight leg (height 0 -> tuck 0), folds as he
    // rises, is fully tucked over the top, and unfolds on the way down so the
    // legs are already reaching for the snow when they meet it (height 0
    // again -> tuck 0, and the landing is an ordinary stance entry with no
    // snap). It is symmetric, continuous, needs no clock and no branch, and a
    // hop cut short by a slope simply never reaches full tuck -- L-DEPTH's
    // spirit, one law with named dials.
    //
    // `tuck_full_frac` is the fraction of the NOMINAL apex (v0^2/2g, from the
    // two dials above) at which the tuck is already complete, so it holds
    // through the top of the arc instead of peaking for one frame. ★ IT IS
    // ALSO THE FOLD'S SPEED, and that is what set it: a parabola's height
    // climbs FASTEST at the push-off, so a small fraction folds the legs in a
    // tenth of a second (measured: 0.55 moved the tuck 0.117 per 1/120 s
    // frame -- a snap, not a fold). 0.85 spreads the same fold over ~0.18 s
    // and still reaches full tuck before the apex, holding it across the top.
    double tuck_full_frac = 0.85;
    // A straight scale on the whole thing: 0 restores G2i's rigid ride
    // exactly, 1 is the posture below.
    double tuck_gain = 1.0;
};

// ★★★ G2i-b -- WHERE THE TUCKED FOOT GOES, relative to that side's own HIP,
// in the man's own frame. These are TARGET points (sole level, the same
// currency `GaitFoot::target_w` is in): the render consumer lifts them by the
// measured ankle height (0.10 m, `flak::GunnerAnthro::ankle_up_m`) to get the
// ankle the leg chain actually ends at, so the ANKLE sits 0.10 m above each
// `drop` below.
//
// ★ RE-RULED, Chad flying the first cut (knees at ~85 deg, boots up under
// the hips): "I thought just a slight bend in the knees would be good, not
// knees up to the nipple line, I just didn't want the legs to fly backwards
// during the jump -- can we do a happy medium." So the defaults below are
// the happy medium: against the 0.908 m leg chain they put the LEAD knee at
// ~60 deg of flexion and the TRAIL at ~50 -- legs mostly DOWN, a clearly
// bent knee, the lead one still reading forward -- and still short of the
// solver's 0.98*reach clamp (worst reach ~0.91 of the chain).
struct AirTuckParams {
    // THE LEAD KNEE DRIVES FORWARD ("a knee forward"): the swing-phase leg at
    // takeoff, foot forward AND up, which is what puts the knee out in front.
    double lead_drop_m = 0.85;
    double lead_fwd_m = 0.24;
    // ... and the TRAIL leg hangs nearly under the hips with its own slight
    // fold -- heel a touch back and up, nothing flying out behind him.
    double trail_drop_m = 0.92;
    double trail_back_m = 0.08;
};

// ★★★ G2i-d -- WHERE THE FEET GO ON THE WAY DOWN. Chad, flying G2i-c: "when
// he is going to land his feet are swinging and trailing back -- I didn't
// want that at all. I want it to look like a normal jump is all."
//
// ★ THE LAW: WHILE HE IS AIRBORNE THE JUMP OWNS THE LEGS, NEVER THE STRIDE.
// G2i-b/-c blended the tuck AGAINST THE LIVE GAIT TARGETS -- the pin/plant
// swing blend, still driven by a `gait_phase` that keeps advancing off his
// speed the whole time he is off the ground, against two ground points
// sampled BEFORE the push-off and left behind in the world at 5.5 m/s. So
// the tuck was the only thing hiding a mid-air stride: as `tuck01` faded on
// the descent (it is height-driven, hence 0 at BOTH ends of the arc) the
// legs were handed straight back to that stride and swung out behind him.
// That is what he saw, and no tuck shape could have fixed it.
//
// The fix is an ENDPOINT, not another curve. On the ASCENT the tuck still
// blends off the stride he actually left the ground on -- that half was
// right. On the DESCENT it blends off a LANDING POSE instead: both feet
// under their own hips, near-extended, reaching for the snow. Nothing on
// the way down is a function of the gait phase.
//
// Same currency as `AirTuckParams` (sole level, relative to that side's hip;
// render adds the measured 0.10 m ankle height on top).
struct AirLandParams {
    // Both boots hang under the hip. The lead one -- the knee that drove
    // forward at the top -- reaches a couple of centimetres out in front,
    // which is what a heel strike looks like; the trail one a hair behind.
    // 0.96 m of drop is a 0.86 m ankle reach against the 0.908 m chain
    // (0.947 of it): a leg that reads STRAIGHTENING, still inside the
    // solver's 0.98*reach clamp, and only 2 cm shy of where a standing boot
    // hangs -- so the stance the touchdown pins is where the boot already
    // was (L-PIN: targets never snap).
    double lead_drop_m = 0.96;
    double lead_fwd_m = 0.04;
    double trail_drop_m = 0.96;
    double trail_back_m = 0.02;
    // ★ HOW FAST THE LANDING POSE TAKES OVER, in the honest currency of a
    // fall: DOWNWARD SPEED. 0 at the apex (where `vert_vel_mps` is 0), full
    // once he is dropping faster than this. Velocity, not height, because it
    // is the one quantity that is exactly 0 at the top of EVERY arc --
    // including a hop cut short by rising ground, which never reaches its
    // nominal apex at all -- so the ascent endpoint and the descent endpoint
    // meet with no step at the changeover, whatever `tuck_gain` is set to.
    // At the ruled 3.2 m/s jump this is reached ~0.12 s past the apex, while
    // the tuck is still pinned at 1.0, so the stride is gone from the pose
    // BEFORE the tuck starts to unfold -- the live gait target's weight in
    // the blend, (1-tuck01)*(1-land01), never exceeds ~2% on any descent
    // frame.
    double full_at_fall_mps = 1.2;
};

// Everything the gait needs to know about a hop, in ONE value -- so the two
// numbers that must agree (how high he is, how tucked he is) cannot be handed
// in separately and disagree. Default-constructed = he is on the ground, and
// `step_gait` is then bit-identical to G2h's.
struct GaitAir {
    // `HopState::height_m`.
    double height_m = 0.0;
    // `HopState::tuck01`, 0..1.
    double tuck01 = 0.0;
    // `HopState::lead_side`, in THIS file's foot indices (0 = left, 1 =
    // right). Anything outside {0,1} simply means neither foot leads and both
    // take the trail posture.
    int lead_side = 1;
    AirTuckParams tuck{};
    // ★ G2i-d: `HopState::vert_vel_mps` -- the sign tells the pose which HALF
    // of the arc he is on, and the magnitude how far into the descent. A
    // default-constructed value (0) is the apex, i.e. still the ascent
    // endpoint, which is also G2i-c's behaviour exactly.
    double vert_vel_mps = 0.0;
    AirLandParams land{};
};

// What the key did this tick. `press` is the EDGE (raylib's IsKeyPressed),
// `down` the level (IsKeyDown) -- the app reads both from the keyboard and
// this file never sees a key code (L-NPC: an NPC hands in its own two bools).
struct HopInputs {
    bool shift_down = false;
    bool shift_press = false;
    // ★ G2i-b: the walker's `gait_phase` at the moment of the press, and the
    // ONLY reason this machine knows anything about walking -- it decides
    // WHICH knee leads (see `HopState::lead_side`). Read on the launch frame
    // and never again, so the lead is fixed for the whole hop.
    double gait_phase01 = 0.0;
};

struct HopState {
    // The ballistic hop. `height_m` is metres above the surface he would
    // otherwise be standing on -- render adds it along his own up, and
    // `step_gait` lifts both foot targets by it so the legs travel WITH him
    // instead of reaching back down to the snow.
    bool airborne = false;
    double height_m = 0.0;
    double vert_vel_mps = 0.0;
    // Was THIS hop launched off an armed charge? Read at the landing, which
    // is where Chad put the dash ("a dash will start on a jump press when
    // landing from it") -- the press arms the landing, the landing spends it.
    bool jump_carries_dash = false;

    // The charge. `shift_held_s` accumulates while held and resets on a
    // release longer than the grace window; `dash_armed` latches once the
    // charge is full and is spent by the landing that follows.
    double shift_held_s = 0.0;
    double shift_off_s = 0.0;
    bool dash_armed = false;

    // The burst, counting DOWN from `dash_decay_s` to zero.
    double dash_s = 0.0;

    // One-shot edges for the app (a landing thump, a launch grunt): true only
    // on the step they happened.
    bool launched = false;
    bool landed = false;

    // ★★★ G2i-b -- THE AIR POSE, as a sim-side quantity so an NPC that hops
    // inherits it with the rest of this value type and render only APPLIES
    // it (L-PURE). `tuck01` is how folded the legs are, 0 on the ground and
    // at both ends of the arc, 1 over the top. `lead_side` is which foot
    // drives its knee forward -- the one that was SWINGING when he left the
    // ground, in `GaitFoot`'s indices (0 = left, 1 = right), latched at the
    // launch so it cannot flicker mid-air as the phase keeps advancing.
    double tuck01 = 0.0;
    int lead_side = 1;
};

// One step of the hop/sprint/dash machine, pure and total: every branch below
// is decided by `prev` and `in` alone.
HopState step_hop(const HopState& prev, const HopInputs& in, double dt_s,
                  const HopParams& p = HopParams{});

// The factor the caller scales `WalkerParams`' three speed anchors by before
// stepping the walker. 1.0 when he is neither sprinting nor dashing, so an
// untouched Shift key leaves the walk bit-identical to G2h's. The dash decays
// back to whichever of sprint/walk is live underneath it rather than to 1.0 --
// a burst that ended while he is still holding Shift must land on the sprint,
// not drop through it.
double hop_speed_mul(const HopState& s, const HopInputs& in,
                     const HopParams& p = HopParams{});

// ---------------------------------------------------------------------------
// ★★★ GAIT LADDER G2j -- THE PUMP-REPAIR WORK ANIMATION. Chad, on the U key
// (`app::ModeEvent::InteractKey` -> `app::PlayerMode::Repairing`): "make him
// look like he is working with his hands in front... a holding still out front
// with his left hand and a torquing of a wrench in another, then a hammer fist
// with the right as the settling of the machine blow."
//
// Three motions and only two of them are cyclic, so the state is a phase and a
// one-shot -- and BOTH live here rather than in `render/`, because a render
// pass that integrates its own clock is exactly the L-PURE violation this
// ladder has lost two rungs to. Value-typed and pure like everything else in
// this file: an AI millwright (`combat/reinforce.h`'s seam) gets one of these
// per NPC and the same three numbers reach the same pose code.
struct RepairWorkParams {
    // A ratchet is a slow pull and a quick reset, never a sine. `cycle_hz` is
    // the whole stroke rate (Chad's "torquing" reads at roughly one a second);
    // `torque_frac` is how much of that cycle is the LOADED half -- above 0.5
    // by definition, or it is not a ratchet.
    double cycle_hz = 1.2;
    double torque_frac = 0.62;
    // The settling blow: raise the fist, drive it down, recover. One-shot.
    double blow_s = 0.55;
};

struct RepairWork {
    bool on = false;       // his hands are on the machine
    double phase = 0.0;    // [0,1) through the ratchet stroke
    double blow_s = 0.0;   // seconds of hammer-fist LEFT, 0 = not striking
};

// One step. `working` is the level (PlayerMode::Repairing); `finish_edge` is
// the single frame the pump came whole (`ModeAction::FinishRepair`) and is
// what fires the blow. ⚠ THE BLOW OUTLIVES `working` ON PURPOSE: finishing the
// repair is exactly what takes the man OUT of Repairing, so a blow gated on
// `on` would be a hammer nobody ever sees swing.
RepairWork step_repair_work(const RepairWork& prev, bool working,
                            bool finish_edge, double dt_s,
                            const RepairWorkParams& p = RepairWorkParams{});

// The ratchet stroke as ONE number in [-1, +1]: +1 at the top of the pull,
// -1 at the bottom, with the return leg travelled in the shorter remainder of
// the cycle. Exposed so a test can grade the ASYMMETRY (the loaded half is
// slower than the reset) without posing a skeleton.
double repair_wrench_stroke(double phase01, double torque_frac);

// The settling blow as ONE number in [-1, +1]: 0 at rest, rising to +1 with
// the fist up, crossing hard to -1 at the strike, easing back to 0. Takes the
// SECONDS REMAINING (so it reads `RepairWork::blow_s` directly) and the total.
double repair_blow_swing(double blow_left_s, double blow_total_s);

// One step of the gait, pure: reads the walker's PUBLISHED state (never a
// second ground query's worth of walker internals) and the same snowpack the
// walker itself samples from (§R5+: "a foot is just another contact patch").
//
// `prev` is the caller's own copy from last step -- NOT a reference into any
// shared/global instance (L-NPC): two callers stepping two `GaitState`s over
// two `WalkerState`s do not and cannot see each other.
//
// `body` defaults to the measured mirror above; a caller with a live rig
// measurement (render owns those) may hand its own in without moving this
// signature again.
//
// ★ G2i: `air` is the hop -- default-constructed (height 0) on every frame he
// is on the ground, and then this function is bit-identical to G2h's; positive
// height while he is in a hop, in which case both feet leave stance and both
// targets ride up with him. ★ G2i-b: and `air.tuck01` folds them, blending
// that rigid ride toward the air posture in `AirTuckParams`. Defaulted, so
// every existing caller and every existing test keeps its exact meaning.
GaitState step_gait(const GaitState& prev, const sim::WalkerState& walker,
                    const sim::WalkerParams& params,
                    const world::SnowpackField& field, double dt_s,
                    const GaitBodyGeom& body = GaitBodyGeom{},
                    const GaitAir& air = GaitAir{});

}  // namespace sim
