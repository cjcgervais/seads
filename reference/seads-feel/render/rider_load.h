#pragma once
// ★ R4a -- THE RIDER LOAD MODEL: the planar sagittal rod, and THE STAGE
// SELECTOR.
//
// docs/R4A_PHASE0_MODEL.md is the contract; docs/R4A_THROW_RULING_20260825.md
// §3 item 2 is the ruling that made this file necessary:
//
//     "`seat_load_frac` / `board_load_frac` are PROMOTED from diagnostics to
//      the stage-1/2/3 selector."
//
// The model itself is NOT new. It was built and red-teamed in R4a Phase 0 and
// has lived, private, inside tools/sled_probe.cpp ever since -- measured over
// the tape corpus, its four cases enumerated, its admissibility postcondition
// mechanised. What this rung does is MOVE it, unchanged, into a pure
// translation unit that BOTH the probe and the shipped game can read.
//
// ★★★ AND THE REASON IS THE HOUSE LAW, NOT TIDINESS. A stage machine in
// render that re-derived these loads would be a SECOND implementation of a
// measured model, and this repo has paid for that shape before ("the one CG
// model in the tree", render::rider_cg). The probe's published numbers -- the
// case mix, the exceedance ladder, every percentile in
// docs/SESSION_HANDOFF_20260825_r4a_phase0.md -- describe THIS code. If the
// game ran a copy, those numbers would stop describing the game the moment
// either copy moved, and nothing would go red.
//
// PURE: glm + std only. No raylib, no clock, no asset, no getenv. It lives in
// seads_render_core and is unit-tested headlessly
// (test/unit/test_rider_load.cpp). It NEVER writes to sim/, and no tape can
// see it -- the loads are DERIVED render-side from already-shipped kernel
// state, which is what keeps this whole rung untapeable (see
// render/trail_chain.h's frame-field banner for the same rule).
//
// ⚠ WHAT THE MODEL IS BLIND TO, restated here so no caller reads it as more
// than it is (R4A_PHASE0_MODEL.md §4.5):
//   - it is PLANAR SAGITTAL. Lateral force goes entirely through the hands by
//     construction (H_x); the seat and boards are frictionless, so NO lateral
//     content reaches N_s or N_b. A side-throw is invisible to it.
//   - the gyroscopic term is dropped. MEASURED to be ~0 for this body in this
//     pose (I_roll - I_yaw = -0.0136 kg m^2, 0.23 %); re-check if the pose
//     moves materially.
//   - limb compliance is absent: he is one rigid rod.

#include <glm/vec3.hpp>

namespace render {

// The model's own g. R4A_PHASE0_MODEL.md's number, not the kernel's dial.
inline constexpr double kRiderLoadG = 9.80665;

// MODEL (+X right, +Y up, +Z forward) -> KERNEL BODY (+X right, +Y up,
// +Z AFT), sim/sled.h's K2 block:  body = (-m.x, m.y - DROP, -m.z),
// DROP = cg_height_m - render::kSagDefaultM. Both halves of DROP are read
// from their owners, never retyped. OPEN-KWS1-SAG (sim/sled.h K2) is
// inherited, not created: every moment arm in the solve is a DIFFERENCE of two
// points taken through this same map, so DROP cancels out of the moment
// equation entirely.
glm::dvec3 rider_load_to_body(const glm::dvec3& model_pos, double drop);

enum RiderCase {
    kRiderSeated = 0,
    kRiderSeatEdge = 1,
    kRiderBoardEdge = 2,
    kRiderFree = 3
};

// The BAKED half of the model: the rider's mass, his pitch inertia and the
// three contact stations, all in BODY axes and all taken in the REST pose.
// Everything that moves per step arrives as an argument to the solve -- the
// lean `d` is what carries the pose change, exactly as it does in the probe.
struct RiderLoadModel {
    double m_r = 0.0;           // rider mass [kg], from SledParams
    double rider_frac = 0.0;    // rider_mass_kg / mass_kg -- cg_off's scale
    double I_pitch = 0.0;       // about body +X through the rider CG
    glm::dvec3 cg_body{0.0};    // rider CG
    glm::dvec3 grip_body{0.0};  // the two grips, collapsed to their mean
    glm::dvec3 boot_body{0.0};  // the two boots, ditto
    glm::dvec3 seat_body{0.0};  // the pelvis
};

struct RiderLoad {
    glm::dvec3 A{0.0};  // the demand vector, body axes [m/s^2]
    double alpha_x = 0.0;
    double H_z = 0.0, H_y = 0.0;  // hand force, + = AFT / UP [N]
    // ★ LATERAL. Chad asked "what about the lean direction?" -- and the answer
    // is that it was already here. The lean `d` carries a lateral component
    // (rider_lat_m), it shifts r_G, and A.x has been computed every substep
    // since Phase 0 and read ONLY by a printf. The seat and boards are
    // frictionless in this model, so by the SAME argument that makes
    // H_z = m_r*A.z "equation (1), always", nothing below can carry lateral
    // either: ALL of it goes through the hands.
    double H_x = 0.0;                        // + = his RIGHT [N]
    double grip3_n = 0.0;                    // hypot(H_x, H_y, H_z)
    double N_s = 0.0, N_b = 0.0, N_m = 0.0;  // seat, board, resultant [N]
    double z_m = 0.0;       // support station, RELATIVE TO THE RIDER CG [m]
    double z_m_body = 0.0;  // the same, in body coords [m]
    double M_h = 0.0;       // wrist couple [N m] -- non-zero only in CASE F
    double V_dem = 0.0;     // m_r * A.y: the resultant VERTICAL demand. The old
                            // CASE-F test read its sign. Exposed because a leg
                            // that means to exercise the V <= 0 branch must be
                            // able to PROVE it got there -- mine did not, and
                            // the mutant walked straight through it.
    double W_supp = 0.0;    // -(K + dz_h*V): the support the family can carry.
                            // N_m(z) = W_supp / (z - dz_h) and every station
                            // has z - dz_h > 0, so W_supp >= 0 IS the exact
                            // statement "something below can carry" -- for
                            // either sign of V. CASE F is legal only below 0.
    double grip_n = 0.0;
    double seat_frac = 0.0, board_frac = 0.0;
    int rcase = kRiderSeated;
    bool z_m_defined = false;
};

// ONE substep of the model. Closed form: four cases, each a division and a
// multiply. No iteration, no tolerance, no convergence check.
//   a_body  the machine's own acceleration, BODY axes [m/s^2]
//   omega   body angular velocity [rad/s]
//   alpha   body angular acceleration [rad/s^2]
//   g_body  gravity, BODY axes (magnitude kRiderLoadG, pointing DOWN)
//   d       the lean: (-rider_lat_m, rider_up_m, -rider_fwd_m), body axes [m]
RiderLoad rider_load_solve(const RiderLoadModel& M, const glm::dvec3& a_body,
                           const glm::dvec3& omega, const glm::dvec3& alpha,
                           const glm::dvec3& g_body, const glm::dvec3& d);

// The house low-pass, the same shape sim/sled.cpp:87 `slew_toward` gives
// SledState::hull_engage_lp: tau = 0.1 s, rate_cap = 1e9 (the filter is the
// only shaping), and h is the SUBSTEP, never dt (R4A_PHASE0_MODEL.md §5.3).
//
// ★ It SETTLES. An exponential decaying toward an exact zero target snaps to
// exactly 0 below this floor, because a consumer downstream branches on
// `<= 0` and an asymptote left it latched open for a whole drive. A numeric
// noise floor on a zero target only -- see the .cpp for why that is not the
// latch disease wearing a new hat.
inline constexpr double kRiderLoadLpZeroFloor = 1.0e-4;
double rider_load_lp_step(double cur, double target, double h);

const char* rider_case_name(int c);

// ★★★ THE ARMING WEIGHT -- SUDBURIAN_LADDER §7.3 stages 0-3, and it is a
// CONTINUOUS, REVERSIBLE function of the ruled selector with NO THRESHOLD IN
// IT AT ALL.
//
//     u_seat  = 1 - min(1, seat_load_frac  / seat_ref)     stage 1 UNWEIGHTED
//     u_board = 1 - min(1, board_load_frac / board_ref)    stage 2 BOARDS FREE
//     arm     = u_seat * u_board                           stage 3 SUPERMAN
//
// The two references are the model's OWN at-rest split -- level gravity, no
// acceleration, no lean -- computed from this same solve by the caller and
// NEVER typed. THE LAW, paid for seven times in this repo: a constant that
// describes the shipped table stops describing it the moment the table moves.
// So there is no constant; re-export the rider and both references follow him.
//
// The product is 1 exactly at CASE F (nothing below carries, which IS "the
// body extends behind the anchor") and 0 whenever either support still carries
// its resting share. Nothing in between is a dial anybody chose.
//
// ★ MEASURED over the tape corpus (`seads_sled_probe stagesel <tape>`):
//   - 0.00 % occupancy at EVERY level on every replayable tape, settle
//     excluded -- the rod model never leaves CASE S on Chad's driving, because
//     the corpus has essentially no airtime (griphold's own PEAK context reads
//     air_s = 0.000). It cannot false-arm on anything he has driven, which is
//     exactly §7.9's "release must be rare".
//   - ⚠ AND THEREFORE THE CORPUS CANNOT PROVE IT FIRES. A table of zeros is
//     what a DEAD arm and an unexercised one both look like. Liveness is
//     proved synthetically instead, on the one axis that unweights a rider:
//     sweep a_body.y from a hard landing to FREE FALL and the weight reads
//     0.00 / 0.04 / 0.16 / 0.36 / 0.64 / 1.00 at 0 / -0.2 / -0.4 / -0.6 /
//     -0.8 / -1.0 g. The probe prints that ladder before it touches a tape and
//     test_rider_load.cpp pins it.
// ★★★ ONE SUPPORT'S RELEASE, in [0, 1]: how much of the share this support
// carried at the reference it has now LOST. `rider_stage_arm` is the product
// of the two, so stage 2 (the boots) and stage 3 (superman) read ONE
// expression and cannot fork. Returns double -- see the .cpp for why that is
// not a style choice.
//
// ⚠ THE REFERENCE MUST BE THE LIVE ONE. Pass the frozen at-rest split and you
// re-introduce the defect Chad's first drive found: the supports are
// frictionless and fall as cos(roll), so 60 deg of roll on a PARKED machine
// reads board_frac 0.1656 against a rest 0.3312 and releases half way. See
// docs/SESSION_HANDOFF_20260827_r4a_arming.md 8.1.
double rider_support_release(double frac, double ref);

float rider_stage_arm(double seat_frac, double board_frac, double seat_ref,
                      double board_ref);

// ★★★ THE CONTACT TERM -- CHAD'S RULING, 2026-08-28, carried by two logs.
//
// WHAT THE LOGS SAID. `rider_stage_arm` above is a FRICTIONLESS PLANAR model:
// it knows only what presses straight into the seat and the boards. It cannot
// tell "leaned over on snow" from "thrown into the air", and measured on his
// own driving (9079 frames, nothing forced) that produced two defects that are
// the same defect seen from both ends:
//
//   * ON THE GROUND, LEANED 15-45 deg -- ordinary banked trail riding, the
//     track visibly compressed into the snow -- the legs released on 42.2 % of
//     frames. Cause: at tilt the live support saturates at EXACTLY zero
//     through a solver case boundary while its zero-accel reference is still
//     0.37, and `1 - frac/ref` reads a vanishing denominator as total loss.
//   * IN THE AIR, INVERTED, both references degenerate to 0 and the guard
//     returns 0 -- so 40.8 % of those frames PINNED the chain to his seated
//     pose while he was upside down and thrown. Perfect where it does not
//     matter (airborne and upright arms on 100 % of frames) and dead where it
//     does.
//
// ⚠ THE FIX IS NOT IN THE ARITHMETIC, AND THAT WAS MEASURED, NOT ASSUMED. Two
// absolute-loss reformulations were replayed over all 7372 frames of the first
// log: both cured the leaning to 0.00 % AND KILLED GENUINE AIR (median arm
// 1.000 -> 0.210; air above 0.5 from 90.9 % to 0.0 %). The RATIO is what makes
// air work -- it is scale-free. So the ratio stays exactly as it is, tests and
// all, and what changes is that the selector is finally told the one fact it
// never had: WAS THE MACHINE STILL CARRYING HIM.
//
// `free_frac` in [0, 1] is how free of the ground he is -- 0 while anything
// touches, 1 once he is properly off it. The law:
//
//     arm = free_frac * (no support possible at this attitude ? 1 : ratio)
//
// and every row of the measured table falls out of it, INCLUDING the one that
// was already right:
//
//   ground, upright        free 0            -> 0    (unchanged, was 5.9 %)
//   ground, leaned 15-90   free 0            -> 0    (the 42.2 %, cured)
//   ground, tipped >90     free 0            -> 0    (HIS OWN STAND RULING,
//                                                     R4A_THROW_RULING 5.6 --
//                                                     a man on a tipped
//                                                     machine at rest stands
//                                                     up, he does not fly)
//   air, upright           free 1, ratio 1   -> 1    (unchanged, was 100 %)
//   air, INVERTED          free 1, degenerate-> 1    (the pin, cured)
//
// ★ THE DEGENERATE BRANCH IS NOT A SPECIAL CASE, it is the honest reading of
// what a degenerate reference MEANS: "at this attitude no support is possible
// at all." Off the ground that is total freedom, which is 1, not 0.
//
// ⚠ AND THE EDGE IS NOT PROVABLY CONTINUOUS -- an earlier draft of this
// comment claimed it was, on the argument that "the live load has already
// saturated, so the ratio has already reached 1 on the approach." That is not
// a theorem. If a live load survives while the reference falls to 0+ (the
// omega^2 r and alpha terms in a_G can demand support in a fast tumble), the
// ratio reads 0 right up to the edge and then the branch takes it to 1: a
// full-scale cliff, the exact opposite of the claim. What is TRUE is weaker
// and measured: across 9079 frames of Chad's driving there is no such frame,
// the new law makes ZERO airborne frame-to-frame jumps above 0.5 where the
// old law made twelve, and the 0.1 s low-pass downstream bounds the slew in
// any case. Pinned by the gate at the edge WITH a live load, which is the
// input that can tell the two apart.
//
// ⚠ `free_frac` MUST BE DERIVED FROM A MEASURED WINDOW, NOT A TYPED ONE. The
// caller builds it as `air_s / rolled_grace_s` -- the kernel's OWN "how long
// off the ground before this stops counting as contact" (sim/sled.h, 0.20 s),
// the same window the roll latch is gated on. Inventing a second number here
// would be the fork this program has paid for seven times.
float rider_stage_arm_free(double seat_frac, double board_frac, double seat_ref,
                           double board_ref, double free_frac);

// ★ HOW FREE OF THE GROUND HE IS, in [0, 1] -- and it lives HERE, in the
// library, because it is POLICY. The first cut of this ruling left these four
// lines at the call site in render/sled_model.cpp, which CMake compiles only
// into the `seads` executable: no test links that TU. That is the exact
// disease render/body_drive.h was written to name, walked into again on the
// rung that cured it, and it matters more here than it did there because
// these lines can kill the whole stage machine silently (below).
//
// `air_s` is sim::SledState::air_s -- time since ANY contact. It resets not
// only on a loaded patch but on hull load and on CG proximity (1.2x the cg
// height, sim/sled.cpp), which is WHY the STAND ruling holds by mechanism: a
// machine lying on its side on the snow reads grounded, so it cannot arm.
//
// ⚠⚠ `grace_s` IS SHARED WITH ANOTHER CONSUMER AND ITS OFF SETTING KILLS US.
// It is sim::SledComfort::rolled_grace_s, whose OWN documented off-config is
// "persist 0.0 + grace huge" (sim/sled.h). Huge grace => free_frac ~ 0
// forever => THE LADDER GOES SILENTLY DEAD, no error, no red test, just a
// rider who never trails again. A scenario.toml can set it (config/, via
// SledParams::comfort). This function cannot stop that, but it refuses to
// pretend: a non-positive window is treated as NO RAMP -- contact or not,
// binary -- rather than as zero freedom, because "there is no window" and
// "he is never free" are different statements and only one of them is true.
double rider_air_free_frac(double air_s, double grace_s);

// ★★★ ONE SUPPORT'S RELEASE, UNDER THE SAME RULING -- and it is a separate
// function only because the stage weight uses the PRODUCT of two of these and
// must apply `free_frac` once, not twice.
//
// WHY IT EXISTS. `rider_support_release` above is the blend weight the DRAWN
// body follows the chain by, and it carried BOTH halves of the defect the
// stage weight was just cured of, untouched:
//
//   * it is not contact-gated, so it flickered across the visible band 1.97
//     times a SECOND on Chad's own riding -- his words, watching it: "legs
//     moving flickery erratic a lot of the time";
//   * and its degenerate guard returns 0, which reads "no support is possible
//     at this attitude" as "he has lost nothing". So on the 485 frames of his
//     drive where he was INVERTED AND AIRBORNE, the drawn legs followed the
//     chain on exactly ZERO of them. The chain flew and the legs stayed
//     planted on the boards -- again in his words, "the orange frames doing
//     all the flying".
//
// That is why there was no superman. Two selectors, one cured and one not, so
// the body could be released and drawn as seated at the same instant.
//
// Measured over the same 9079 frames: superman-capable frames (chain armed AND
// legs following) 3.55 % -> 10.76 %; chain-flies-legs-planted 7.21 % -> 0.00 %;
// blend crossings 1.97/s -> 0.21/s; and on real air the legs follow on 31 % ->
// 100 %, inverted 0 % -> 100 %.
double rider_support_release_free(double frac, double ref, double free_frac);

}  // namespace render
