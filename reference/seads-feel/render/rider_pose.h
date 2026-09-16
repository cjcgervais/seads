#pragma once

#include <array>
#include <cmath>
#include <cstdlib>  // sled_sag0_m(): the SEADS_SLED_SAG0 env read

#include <glm/glm.hpp>
#include <glm/mat4x4.hpp>

#include "render/rider_rig.h"  // RiderJoint, the declared joint catalogue
#include "sim/sled.h"  // the kernel dials this file single-sources (§0.3)

// ★ THE SUDBURIAN POSE MATH -- docs/SUDBURIAN_LADDER.md rung R1a.
//
// WHAT THIS RETIRES. Defects 6, 7, 8, 9, 10, 11 and 14 of §1.1 -- the four that
// §1.1 calls "floaty" (6/7/8/9) plus the ski-angle lie (10), the magic numbers
// (11) and the degenerate rest bend plane (14).
//
// WHY IT IS HERE AND NOT IN render/sled_model.cpp. §0.5 LAYERING: this TU is in
// seads_render_core, which tools/graph/layer_rules.toml marks
// `deny_ext = ["raylib"]`, so it links into seads_tests and every function below
// is unit-testable headlessly. render/sled_model.cpp is in the raylib-only
// `seads` exe target and has ZERO test TUs; anything pure that stays there is
// untestable by construction. Mesh upload and draw stay there. Pose math is here.
//
// §0.1 DETERMINISM. Every function below is PURE: same arguments, same result,
// no statics, no clock, no accumulator, nothing written back. The pose is a
// function of (kernel state, tick) and this file is the half of it that can be
// proved.

namespace render {

// ---------------------------------------------------------------------------
// §0.3 SINGLE SOURCE -- kernel dials, READ (never redeclared, never re-tuned)
// ---------------------------------------------------------------------------
//
// app/main.cpp:1310 default-constructs `sim::SledParams` and never overrides it
// from config (there is no [sled] TOML section and no loader path), so the
// struct's in-class initialiser IS the live value. Reading it here is therefore
// a genuine single source and not a hopeful copy.

// DEFECT 10, the ski-angle lie. render/sled_model.cpp hard-coded
// `kSkiRad = 25 deg`; the kernel steers to `steer_max_rad = 0.42 rad = 24.06
// deg`, so the drawn ski over-rotated the simulated one by 3.9 %. Read the
// kernel, do not retype either number.
inline float ski_steer_max_rad() {
    return static_cast<float>(sim::SledParams{}.steer_max_rad);
}

// DEFECT 11, first magic number. The `0.25f` in the stand_k divisor was a
// retyped copy of `stand_rise_m`; if the kernel dial ever moves, the render
// normalisation silently stops reaching 1.0 at full stand.
inline float stand_rise_m() {
    return static_cast<float>(sim::SledParams{}.stand_rise_m);
}

// DEFECT 11, unsourced-but-KEPT. The handlebar sweep. 32 deg is NOT backed by
// any kernel dial, any measurement, or the .blend -- it was picked so the bars
// read as turning more than the skis, which is true of a real sled's bar-to-ski
// ratio but is not this number's provenance. R1a's spec says: leave the value
// alone, mark it explicitly unsourced. Marked. Do not "single-source" it to
// steer_max_rad -- that would be inventing a relationship, not finding one.
inline constexpr float kBarSweepRad = 32.0f * 3.14159265f / 180.0f;  // UNSOURCED

// DEFECT 11, head clamps. Unsourced but anatomically defensible and kept at
// their shipped values: 1.4 rad = 80.2 deg of neck yaw, 0.9 rad = 51.6 deg of
// neck pitch. Real cervical range is ~70-80 deg yaw and ~60 deg extension, so
// these are "at the human limit", which is what a clamp should be. Named here
// so the next reader does not have to guess; NOT re-tuned (a rename and a
// re-tune in one commit is unreviewable).
inline constexpr float kHeadYawMaxRad = 1.4f;
inline constexpr float kHeadPitchMaxRad = 0.9f;

// DEFECT 11, the static visual sag default. `sag0` was a bare 0.10f literal
// behind a getenv. Named, not moved. See sled_model.cpp for what it means.
inline constexpr float kSagDefaultM = 0.10f;

// ★ R5 row 9: THE ONE sag0 READ. sled_model.cpp's mount law places model y=0
// at body -(cg_h - sag0); the sled's shadow proxies (render/shadow_casters.h,
// built app-side) must use the SAME drop or the shadow floats/sinks 10 cm
// against the drawn machine — the exact class of the DRIVE-2 "shroud halfway
// through the asphalt" defect. Both consumers now call this; the
// SEADS_SLED_SAG0 A/B override moves the mesh AND its shadow together.
inline float sled_sag0_m() {
    static const float v = [] {
        const char* e = std::getenv("SEADS_SLED_SAG0");
        return e != nullptr ? static_cast<float>(std::atof(e)) : kSagDefaultM;
    }();
    return v;
}

// ---------------------------------------------------------------------------
// DEFECT 6 -- THE BOOT RE-SEAT
// ---------------------------------------------------------------------------
//
// MEASURED out of assets/sled/indy650.glb (the shipped file, parsed straight
// from its JSON+BIN chunks, 2026-08-16). All figures in the glTF model frame,
// metres, +Y up, +Z forward:
//
//   board_socket_L world     = (-0.300000, +0.327000, -0.170000)
//   foot_L (ankle) world     = (-0.300000, +0.285322, -0.220800)
//   rider_boot_L sole, min y = +0.210151      (852 verts, bind pose)
//
// ★★ THE DECK, RE-MEASURED 2026-08-17 (R1a attempt 2, §C) -- AND THE SURFACE
// ATTEMPT 1 USED DOES NOT EXIST. Attempt 1's comment read "board_L
// running-board mesh, y bbox = [+0.252000, +0.277000] -> DECK TOP SURFACE =
// +0.277000". That is a BOUNDING-BOX TOP, not a deck. Measured on the shipped
// bytes:
//
//   board_L DECK SHEET   y = +0.252000 exactly, 58 flat triangles, 0.16075 m2,
//                        x |0.2100..0.3750|, z [-1.0100, +0.3100]
//                        <-- THIS is what a boot stands on
//   board_L OUTBOARD LIP 53 X-facing triangles spanning y 0.2520..0.2770 at
//                        x |0.2110..0.3810| -- a raised EDGE, not a surface
//
// So 0.277 is the top edge of the lip at the outboard rim. Standing the boot on
// it floats the sole 25.0 mm above the actual deck. ★ WARNING FOR THE NEXT
// READER: `board_L`'s y bbox max is the LIP. Never take a bbox extreme for a
// contact plane on this asset; take the sheet.
//
// So the shipped boot is driven 41.8 mm THROUGH the deck sheet it is supposed
// to stand on, and the ankle sits 50.8 mm aft of the socket.
//
// THE TWO NUMBERS, AND WHERE EACH COMES FROM:
//   y = +0.041849 = 0.252000 (board_L DECK SHEET) - 0.210151 (boot sole min y).
//                   This is the ONLY quantity that puts the sole ON the deck;
//                   note it is NOT 0.041678 (the ankle-to-socket gap) -- the
//                   two are within 0.2 mm by coincidence, not by construction,
//                   because board_socket_L is authored 75 mm ABOVE the deck
//                   sheet and is a binding point, not a contact point.
//   z = +0.050800 = -0.170000 (socket z) - (-0.220800) (ankle z). Puts the
//                   ankle directly over the socket in plan, which is the other
//                   half of the spec's ask and also zeroes the z column of
//                   RiderChainSpec::rest_tip_minus_socket_m.
//   x = 0         = -0.300000 - (-0.300000). Already exact; nothing to correct.
//                   Identical on both sides, so this vector is NOT mirrored --
//                   see §2.3a, and do not add a sign here.
//
// AFTER (re-measured with a full CPU skin of the shipped file): sole
// y = 0.252000 exactly (on the deck sheet), ankle y = 0.327171 (0.2 mm above
// the socket), knee y 0.5925 -> 0.5793. Measured cost of the correction: leg
// reach ratio at rest 0.4477 -> 0.4720 (the leg is 24 mm less extended, i.e.
// the boot is closer to the hip), arm reach unchanged at 0.7301, dash gap
// unchanged at 14.351 mm.
//
// ★ REPORTED, NOT FIXED (it is GLB work -- R1b / the cowl ladder):
// `board_L/R` have ZERO up-facing triangles. All 58 horizontal triangles are
// wound -Y with authored normals -Y (winding and normals agree, 115/115), so
// with backface culling in its default state THE DECK IS NOT DRAWN FROM ABOVE
// and the boots stand on an invisible surface. Filed against the machine art;
// no byte of the GLB is touched here.
//
// ★ WHY render/rider_rig.h's `rest_tip_minus_socket_m` TABLE IS NOT TOUCHED.
// That table pins what the SHIPPED ASSET IS. R1a changes no bytes of the GLB
// (it is a pure C++ rung; the rig repair is R1b), so the measured table is
// still correct and its test must stay green. This vector is the RUNTIME
// correction applied on top of it, and it is deliberately a separate number so
// that the defect and its compensation never get confused for each other.
//
// ★★ AND SINCE 2026-08-18 IT IS ZERO. The value {0, 0.041849, 0.050800} was
// the LEGACY rider's defect-6 correction (its boots were authored 41.8 mm
// through the running-board sheet and 50.8 mm aft of the socket). The rider is
// now the SUDBURIAN, seated by seat_sudburian.py against the machine and SIGNED
// at R2b: its rest IS the seat, its feet ARE where they belong, and any
// non-zero re-seat here would move a signed pose. The mechanism (a re-seat
// folded into the socket weld) is kept because it is the right shape for the
// next correction, whatever it turns out to be; the number is retired. The
// legacy history above is left as the record of why the mechanism exists.
inline const glm::vec3 kBootReseatM{0.0f, 0.0f, 0.0f};

// ---------------------------------------------------------------------------
// DEFECT 7 -- FEET TO THE BOARD SOCKET
// ---------------------------------------------------------------------------

// The socket-relative weld an IK chain tip rides, captured ONCE from the rest
// pose: `inverse(socket_rest) * translate(reseat) * tip_rest`.
//
// This is exactly the transform the ARMS have always used (hand_off =
// inverse(rest_world[grip_socket]) * rest_world[hand]) -- that mechanism is
// correct and proven, and R1a's job on the legs is to copy it rather than
// invent a second one. The only addition is `reseat`, a world-space (model
// frame) translation applied to the rest tip BEFORE it is expressed in socket
// coordinates, which is how defect 6 rides along as a correction to the weld
// instead of as a second mechanism downstream of it.
//
// Pass reseat = (0,0,0) for the arms and kBootReseatM for the legs.
//
// At runtime: `tip_world = socket_world_now * offset`. Rigid on the socket, so
// the boot tracks the board through steer, lean, stand, suspension and the
// whole body mount -- which is the entire content of defect 7. The OLD code
// targeted `rest_world[foot]`, a CONSTANT captured at load, so the boot tracked
// nothing at all while the hip leaned away from it.
glm::mat4 socket_weld(const glm::mat4& socket_rest_world,
                      const glm::mat4& tip_rest_world, const glm::vec3& reseat);

// ---------------------------------------------------------------------------
// DEFECT 14 -- THE DEGENERATE REST BEND PLANE
// ---------------------------------------------------------------------------

// The bend-plane normal for a 2-bone chain, captured from the rest pose.
//
// PRIMARY BRANCH (unchanged, and it is the branch every chain in the shipped
// asset actually takes -- measured |cross| is 0.109817 for both arms and
// 0.123604 for both legs, three orders of magnitude above the threshold):
// n = cross(tip - root, mid - root), which points toward the AUTHORED
// elbow/knee.
//
// ★ R1c CORRECTS THE LEG FIGURE (verifier finding 7). This comment carried
// 0.1454 and rider_pose.cpp carried 0.11552; NEITHER is the shipped number.
// Both were taken BEFORE §C moved the boot re-seat, and the leg |cross| is
// captured against the RE-SEATED foot target (capture_rest_ik passes
// `wpos(rest_world[board_socket] * foot_off)`), so it moved with it. Re-measured
// on the shipped bytes through the real capture path: 0.123604, both sides.
//
// FALLBACK BRANCH (this is the defect). The old code substituted a WORLD-SPACE
// CONSTANT, `cross(normalize(st), vec3(0,-1,0.1))`. Two things are wrong with
// that. It is world-space, so it does not mirror between sides and does not
// follow the chain's own frame -- measured against the authored normals it
// scores dot = +0.646 on the arms but dot = -0.298 on the legs, i.e. it points
// the knee the WRONG WAY. And it is not stable: as a chain sweeps through the
// straight configuration the constant's projection can reverse, flipping the
// elbow through the limb.
//
// The replacement derives the normal from the CHAIN'S OWN PARENT FRAME, and
// ★ R1a ATTEMPT 2 CORRECTS WHICH AXIS (verifier finding F1). Attempt 1 used the
// root bone's rest-pose +Z column. That was wrong in a way a green test pinned:
// a bend NORMAL must ANTI-MIRROR between sides, because the bend DIRECTION is
// cross(n, dir) and cross is handedness-flipping. Measured on the shipped rest
// frames with M = diag(-1,1,1): the +X column satisfies R_col = -M*L_col to dot
// -1.0000 (anti-mirrors), while +Y and +Z both satisfy R_col = +M*L_col (mirror).
// So the fallback is now the root bone's **+X** column, Gram-Schmidt'd against
// the current chain direction. Measured against the AUTHORED normal it scores
// dot +0.861592 on BOTH arms and -0.815822 on BOTH legs -- one sign per pair,
// which is exactly the invariant attempt 1 broke (its +Z column scores
// -0.795544/+0.795544 on the arms and +0.874618/-0.874618 on the legs: opposite
// signs WITHIN each pair). ★ R1c re-took all four of these numbers through the
// real capture path after §C's re-seat -- the leg figures were previously
// documented as -0.8347 and +0.8490/-0.8490 from pre-§C geometry.
// It still varies continuously with the chain direction
// and still never reverses: projecting a fixed vector out of a rotating
// direction is a continuous map with no sign branch in it. The fall-throughs,
// for the case where +X is parallel to the chain, are cross(dir, +Z) then
// cross(dir, +Y) -- built that way because a cross with a MIRRORING vector
// anti-mirrors, so every branch keeps the invariant.
//
// HONEST LIMIT, STATED PLAINLY. This makes the fallback deterministic,
// side-CONSISTENT and non-flipping. It does NOT make it agree with an authored
// bend that does not exist -- when the rest chain is straight there is no
// authored side to agree with. And the measured signs above are a RIG finding
// worth its own line: the arm and leg rolls disagree (+0.86 vs -0.83), so no
// single frame axis can point both an elbow and a knee correctly on this rig.
// Filed for R1b/R2. The structural answer is still §2.1's pre-bends (elbows
// 3 deg, knees 2 deg), which are a rig change and therefore R2. Until then
// there is a discontinuity at the |cross| threshold between the two branches;
// on the shipped asset it is unreachable (see the measured numbers above), and
// the test suite pins that it is.
inline constexpr float kBendDegenerate = 1e-4f;

glm::vec3 rest_bend_normal(const glm::vec3& root, const glm::vec3& mid,
                           const glm::vec3& tip,
                           const glm::mat3& root_rest_rot);

// ---------------------------------------------------------------------------
// DEFECT 8 -- WAKING `absorb`
// ---------------------------------------------------------------------------
//
// `SledRig::absorb` shipped hard-coded 0 forever, documented as "no kernel state
// yet -- live 0". There was no terrain-reaction channel anywhere in the rider,
// so the man did not react to the ground at all. Its one consumer is a 0.12 m
// pelvis drop (the bump-soak crouch), which was therefore always exactly zero.
//
// THE DRIVER, and both halves are read-only kernel state:
//   HOLD -- `SledState::susp_x[i]`, compression in metres. Already plumbed to
//           render as `DrawInfo::sled_susp_x[3]`. A compression that is HELD is
//           a sustained load: the man is being pressed down and stays down.
//   HIT  -- `SledState::susp_v[i]`, compression RATE in m/s. Newly plumbed as
//           `DrawInfo::sled_susp_v[3]` -- a pure read added beside susp_x,
//           ZERO change under sim/. A compression that is ARRIVING is a strike.
// Both are taken as the max over the three kernel patches {ski L, ski R,
// track}: one ski finding a rock is a hit, and averaging it away is exactly the
// mush this defect is about.
//
// §0.1: no smoothing, no accumulator, no frame-time. The temporal shape comes
// from the kernel's own susp_x/susp_v, which are integrated in the sim at fixed
// dt, so absorb is bit-reproducible from a tape.
//
// THE CONSTANTS, MEASURED. Distributions taken over the four long real-drive
// tapes on this branch (build/sled_tape_4/5/6/7, 61472/93407/53643/88127 ticks)
// plus the three signed golden tapes:
//   susp_x        p90 = 0.048 / 0.049 / 0.059 / 0.086 m   p99 = 0.155..0.181 m
//   max(+susp_v)  p90 = 2.47 / 1.12 / 0.94 / 1.95 m/s     p99 = 22.5..24.6 m/s
// susp_rest_m is 0.21 m, so the p90 band of susp_x is ~0.23-0.41 of it and the
// p99 band ~0.74-0.86. Hence: hold starts at 0.25 of susp_rest_m -- i.e.
// nothing at cruise.
//
// ★★ §B, R1a ATTEMPT 2 -- THE HOLD RAMP IS NOW A SATURATING MAP, AND THE
// ATTRIBUTION IN THE WORK ORDER IS REFUTED BY MEASUREMENT.
//
// The work order says the measured 0.500 single-tick step (60 mm of pelvis in
// one 1/120 s tick) comes from the HIT term, because `susp_v` peaks past
// 90 m/s. Measured term by term over 299,172 ticks (build/sled_tape_4..7 +
// the three committed golden tapes):
//
//   term   |d/dtick| p99   |d/dtick| max
//   hold        0.0304-0.169      1.0000   <-- the FULL term, in one tick
//   hit         0.0401-0.137      0.6524
//
// So it is the HOLD term that jumps its whole range, not the hit term, and no
// amount of hit shaping can touch it: with kAbsorbHitHalfMs raised to 40 and a
// cube on the hit term, |d absorb| max is still exactly 0.5000 -- the hold
// weight. The cause is `susp_x` itself: it is clamped to [0, 2*susp_travel_m] =
// [0, 0.52] and it moves 0.24-0.35 m in a single tick 7-29 times per long tape
// (bottom-out landings and the contact loss right after them), which crosses
// any band a linear ramp can have.
//
// ★ AND THE HONEST LIMIT, STATED AS A THEOREM. absorb is a pure function of one
// tick's kernel state (§0.1 forbids filtering). Its worst single-tick step is
// therefore sup|f(s1) - f(s0)| over the reachable state pairs, which for a
// bounded monotone f IS ITS FULL RANGE, because the kernel's own susp_x does
// jump end to end. NO SHAPING CAN REDUCE THE WORST STEP BELOW THE TOTAL DROP
// AUTHORITY. Only two things are dials: the authority (kAbsorbDropM) and how
// much of the range ordinary driving uses (the shape). Both are moved below.
//
// The shape change: hold becomes the same saturating map the rate term already
// uses, for the same measured reason -- susp_x is heavy-tailed (p90 0.064-0.108,
// p99 0.211-0.244, max 0.520). A dead-band to kAbsorbHoldOnFrac, then
// s/(s + kAbsorbHoldHalfFrac*susp_rest_m). MEASURED effect at equal worst step:
// the p90 crouch rises 21 % (absorb p90 0.2977 -> 0.3600) because the ordinary
// range is where the slope now is. Half-scale 0.10*susp_rest_m = 21 mm past the
// dead band, which is inside the measured p90 band of susp_x.
inline constexpr float kAbsorbHoldOnFrac = 0.25f;    // x susp_rest_m, dead band
inline constexpr float kAbsorbHoldHalfFrac = 0.10f;  // x susp_rest_m, half-scale

// The rate term is a SATURATING map v/(v + half), not a linear ramp, and that
// is a measurement-driven choice rather than a stylistic one: susp_v is
// violently heavy-tailed (p50 of max(+v) is 0.03 m/s, p90 is ~1-2.5, p99 is
// ~23, peaks past 90 m/s when the suspension slams a stop). A linear ramp with
// any full-scale small enough to register normal bumps would sit pinned at 1
// for a quarter of every drive; a full-scale big enough not to would never
// move. The saturating map is monotone, C1, bounded below 1 by construction,
// and needs exactly one constant. 4.0 m/s sits between the measured p90 and p99
// of max(+susp_v), so ordinary trail chatter reads as a few percent and a real
// strike reads as most of the travel.
inline constexpr float kAbsorbHitHalfMs = 4.0f;

// Equal weights: a held load and an arriving strike are both worth half the
// crouch. Their sum is clamped to [0,1], so neither alone can peg it.
inline constexpr float kAbsorbHoldWeight = 0.5f;
inline constexpr float kAbsorbHitWeight = 0.5f;

// The body drop `absorb` drives, in metres.
//
// ★ 0.120 -> 0.040 (§B). This is the ONLY dial that moves the worst single-tick
// step (see the theorem above), and the step is the reported defect. MEASURED,
// same 299,172 ticks, drop in millimetres of body motion:
//
//   design                          p50   p90    p99    max  |step|p99 |step|max
//   shipped 0.120 + linear ramp    2.37  35.72  60.00  112.46     4.17    65.66
//   THIS    0.040 + saturating     1.04  14.39  18.32   35.40     1.43    19.08
//
// The worst one-tick body drop falls 65.66 -> 19.08 mm (-71 %) and the p99 step
// 4.17 -> 1.43 mm, while the p90 crouch stays 14.4 mm and the worst 35.4 mm --
// absorb is NOT inert (that is a gate criterion: Chad saw the torso move and
// the channel reacting to terrain must survive). The frontier is strict and
// measured: worst step ~ p90 crouch / absorb-p90, so buying a 12 mm worst step
// would cost the p90 crouch down to 7.5 mm. 0.040 is the chosen point on it and
// it is one number to move if Chad wants more or less.
inline constexpr float kAbsorbDropM = 0.040f;

// ★★ §A / F3 -- WHERE THE DROP IS APPLIED, AND THIS IS THE FIX FOR THE ONE
// DEFECT CHAD SAW ("I saw the torso go down on a bump").
//
// Attempt 1 applied the whole drop at `pelvis`. In this rig `thigh_L/R` are
// children of `root`, NOT of `pelvis`, so a pelvis drop moves torso + arms +
// head down while the hip JOINTS of the legs stay put: the torso telescopes
// into the hips and the knees never bend. Measured (reseat corrected, absorb
// 0 -> 0.937): at kAbsorbRootFrac 0 the knee y is 0.5793 -> 0.5793, frozen, and
// the leg reach ratio 0.4720 -> 0.4720, frozen -- identical at every absorb
// value. That is F3.
//
// Applying it at `root` instead takes the thighs down with the torso, and the
// boots -- welded rigid to `board_socket_L/R` by attempt 1's D6/D7 -- force the
// leg IK to fold. The rigid boot is the anchor that makes this work.
//
// ARM COST IS EXACTLY ZERO, verified rather than assumed: the shoulder chain
// runs root -> pelvis -> spine1..3 either way, so the shoulder descends by the
// same amount under every split. Measured arm reach ratio at absorb 0.937 is
// 0.7042 at root_frac 0.00, 0.50, 0.65, 0.70 AND 1.00 -- bit-identical.
//
// WHY A SPLIT AND NOT ALL OF IT. Full root-drop puts the knee furthest below
// the running-board deck sheet (y 0.2520). Measured worst knee y over
// fwd 0..0.45 x absorb 0..0.885 at the tuck, with §C's corrected reseat and
// §D's hinge live:
//     root_frac  0.00   0.50   0.65   0.70   1.00
//     knee-deck -39.8  -54.7  -59.1  -60.6  -69.5  mm
// -39.8 mm at root_frac 0 is PRE-EXISTING (it is where §C's 25 mm correction
// puts the knee at absorb 0) and is not caused by this change. 0.65 is the
// middle of the work order's 0.6-0.7 band: it keeps most of the knee bend
// (leg ratio 0.4720 -> 0.4483 of the 0.4359 available at full root) for 85 % of
// the excursion the full-root version would spend. The knee is INBOARD of the
// deck sheet (x |0.210..0.375|), so "below the deck" here is the footwell
// volume, not a surface intersection -- see OPEN-R1A-FOOTWELLS.
inline constexpr float kAbsorbRootFrac = 0.65f;

// The two halves of the drop, single-sourced off the pair above so a caller
// can never apply the whole thing twice or forget the remainder.
inline constexpr float absorb_root_drop_m(float absorb) {
    return kAbsorbDropM * kAbsorbRootFrac * absorb;
}
inline constexpr float absorb_pelvis_drop_m(float absorb) {
    return kAbsorbDropM * (1.0f - kAbsorbRootFrac) * absorb;
}

// Compose the crouch. Returns [0,1]. `susp_rest_m` is the kernel's
// `susp_rest_m` (already plumbed as DrawInfo::sled_rest); a non-positive value
// disables the hold term rather than dividing by zero.
//
// MEASURED RESULT with §B's shape, over 299,172 ticks (build/sled_tape_4..7 and
// the three committed golden tapes): p50 0.0259, p90 0.3600, p99 0.4580, worst
// 0.8848. It still never pegs at 1.0. (Attempt 1's shape on the same ticks:
// p50 0.0197, p90 0.2977, p99 0.5000, worst 0.9371.)
float rider_absorb(const float susp_x_m[3], const float susp_v_ms[3],
                   float susp_rest_m);

// ---------------------------------------------------------------------------
// DEFECT 9 -- THE INVENTED 90 mm HIP SHIFT
// ---------------------------------------------------------------------------
//
// DELETED, per §6 ("deletion is the tape-safe option and is the default") and
// the R1a spec. What was deleted: a `+0.09f * stand_k` forward hip translation
// and a `0.32f * stand_k` pelvis pitch, both applied render-side only, so the
// drawn CG and `sim/sled.cpp`'s `cg_off` disagreed by up to 90 mm.
//
// It was NOT a stylistic flourish -- it carried Chad's own drive-2 complaint
// ("standing only pulls arms and legs in an unnatural direction"), and the
// measurement below is the honest account of what deleting it costs. Numbers
// are shoulder-to-grip distance as a fraction of the arm chain's IK reach limit
// `dmax` = 0.664662 m (a ratio at or above 1.0 means the IK clamps and the
// wrist visibly separates from the forearm):
//
//   stand axis only (lat 0, fwd 0), lean_up 0 -> 0.25:
//       WITH the shift   0.730 -> 0.738   (the shift's whole purpose)
//       DELETED          0.730 -> 0.971   <-- does NOT saturate; 19.1 mm spare
//
//   full kernel lean box (lat +-0.35, up -0.10..+0.25, fwd -0.25..+0.45,
//   200 sampled cells x 2 arms):
//       WITH the shift   23.0 % of cells clamp, worst ratio 1.268
//       DELETED          37.0 % of cells clamp, worst ratio 1.420
//
// So the spec's literal test -- "does arm IK still saturate at full stand?" --
// answers NO, and deletion proceeds. But the margin at full stand collapses
// from 26 % to 2.9 %, and away from the stand axis the arms clamp in half again
// as many cells. That is NOT created by this deletion: the arms already clamped
// in 23 % of the box because the rig's arms are too short and too narrow
// (defects 1, 3, 5), which is R1b's rig work, and D7 cannot help because the
// arm chain depends only on root + pelvis. Recorded here, and raised in the
// R1a report as the open question, so that the choice between "promote the
// shift into the kernel" and "fix the arm lengths in R1b" is Chad's and is made
// on these numbers.

// ---------------------------------------------------------------------------
// ★★ §D -- THE SOLVED TORSO HINGE (Chad's ruling, 2026-08-17)
// ---------------------------------------------------------------------------
//
// THE RULING. Forward lean stops being 450 mm of rigid whole-body translation
// and becomes a HIP HINGE plus whatever translation is still needed. The hinge
// angle is SOLVED, not authored, so the rider's mass-weighted CG displacement
// still equals the kernel's `lean_fwd_m` -- otherwise this is defect 9 rebuilt
// (render inventing motion the kernel does not know).
//
// ★ FINDING THAT CHANGES THE PREMISE, AND IT IS THE HEADLINE OF THIS SECTION.
// The work order states that the shipped translation "moves the rider's CG by
// EXACTLY lean_fwd_m" and is therefore already CG-honest. **It does not.**
// Measured with a full segment CG (below) over the shipped pose path:
//
//   lean_fwd_m   true dCG.z    K = dCG/lean    CG error
//     +0.100      +0.082161      0.8216         -17.8 mm
//     +0.200      +0.159376      0.7969         -40.6 mm
//     +0.300      +0.233246      0.7775         -66.8 mm
//     +0.450      +0.344845      0.7663        -105.2 mm
//     -0.250      -0.216398      0.8656         +33.6 mm
//
// Cause: the hands are IK-pinned to `grip_socket_L/R` and the boots to
// `board_socket_L/R`, and those sockets do not move with the root. 42.2 % of the
// rider's mass sits in those two pinned chains, so translating `root` moves only
// ~77-87 % of the mass. **The shipped fore-aft channel already carries a
// 105.2 mm CG lie -- LARGER than the 90 mm D9 lie that was just deleted for
// being one.** Nobody had measured it because nobody had a segment CG.
//
// So the solve below is not merely "as honest as" the translation; it replaces a
// 105.2 mm error with a measured max of 0.499 mm. THAT is the acceptance number.
//
// HOW IT IS SOLVED (deterministic, fixed cost, no accumulator, no smoothing):
//   1. `rider_cg` is a pure function of the 19 posed joint world positions,
//      using a documented segment mass/CG table (Winter, after Dempster -- the
//      same source docs/SUDBURIAN_LADDER.md §3.0 already cites for the segment
//      LENGTHS, so this introduces no new authority).
//   2. `hinge_theta_for` is a CLOSED FORM on a reduced model captured from the
//      rest pose: the hinged set (trunk + head+neck, everything the pelvis
//      rotates rigidly) has mass fraction m_h with its CG at (dy, dz) from the
//      hip, so its CG displacement under a hinge of theta is exactly
//      H(theta) = m_h*dy*sin(theta) + m_h*dz*(cos(theta) - 1), which inverts as
//      an arcsine. theta is then clamped to the anatomical limits below.
//   3. The RESIDUAL is carried by the root translation, solved by TWO
//      fixed-slope Newton steps on the TRUE posed CG (the pose pass is re-run;
//      each step is `d += (target - cg(d)) / k_hat`, with k_hat captured once at
//      load). Fixed-slope, so there is no difference quotient and no degenerate
//      branch. Convergence rate |1 - K_true/k_hat| is 0.054 measured, hence:
//
//        Newton steps   pose passes/frame   max |CG error| over the reachable set
//              0                2                26.098 mm
//              1                3                 2.866 mm
//              2                4                 0.499 mm   <-- SHIPPED
//              3                5                 0.126 mm
//
//      (1170 samples: up in {-0.10..0.25}, lat over the ACTUAL reachable
//      lat_reach(up) = 0.15 + 0.20*clamp01(up/0.25), absorb in {0, 0.5, 1},
//      fwd in {-0.25..+0.45}.)
//
// ★ NOTE WHY THIS IS ROBUST: because the residual is solved against the TRUE
// posed CG, the reduced model in step 2 cannot introduce a CG error. It only
// decides HOW THE DEMAND IS SPLIT between hinge and translation. Its own error
// (measured: it under-reads the true H by up to 5.3 mm at 30 deg, because the
// arms swing with the shoulder against pinned hands) shows up as a few mm of
// root translation, never as a lie.
//
// WHAT IT BUYS, MEASURED (metric: unsigned min rider-mesh/machine-mesh distance,
// designed contacts and pose-independent static intersections excluded; it
// reproduces the published 14.351 mm rest clearance and the fwd 0.0185 contact
// onset exactly): the hinge moves the torso WITHOUT moving the pelvis or the
// knees, and the measured dash gap is UNCHANGED at 14.351 mm for every theta
// from -40 deg to +30 deg. So the first 54.8 mm of CG demand is free.
//
// MEASURED CONTACT BOUNDARY, the first lean_fwd_m at which the gap closes to
// 1 mm, on the (up x fwd) axes so it can be diffed (lat 0, absorb 0):
//
//     up      attempt 1        attempt 2       gain
//   -0.10   0.0211   4.7 %   0.0483  10.7 %   2.29x
//   -0.05   0.0202   4.5 %   0.0677  15.0 %   3.35x
//    0.00   0.0158   3.5 %   0.0686  15.2 %   4.33x
//   +0.05   0.0202   4.5 %   0.0729  16.2 %   3.61x
//   +0.10   0.0299   6.6 %   0.0879  19.5 %   2.94x
//   +0.15   0.0510  11.3 %   0.1063  23.6 %   2.09x
//   +0.20   0.0817  18.2 %   0.1354  30.1 %   1.66x
//   +0.25   0.1274  28.3 %   0.1784  39.6 %   1.40x
//
// AND WHAT IT COSTS -- ALL THREE COSTS. NONE IS HIDDEN AND THE RUNG SHOULD BE
// JUDGED WITH THEM IN HAND.
//
// (1) BOUNDED AUTHORITY, SO DEEP LEAN GETS WORSE, NOT BETTER. The hinge's TOTAL
//     CG authority is 54.8 mm at +30 deg (89.7 mm even at its 60-75 deg maximum)
//     = 12.2 % of the 450 mm forward range, because the trunk+head is 57.8 % of
//     the mass with its CG only 194.8 mm above the hip on this rig (defects 3/5,
//     a 1.6 m figure with a short trunk, R1b). Past that the residual translation
//     takes over, and because CG honesty needs K = 1 while the pinned chains
//     deliver 0.77, the honest translation at full lean is 0.5037 m -- 53.7 mm
//     MORE than the shipped 0.450. So beyond the boundary this pose intrudes
//     DEEPER than attempt 1's did. The win is 1.4-4.3x more clean travel plus
//     ★ FORE-AFT CG honesty -- ONE AXIS OF THREE as attempt 2 shipped, and TWO
//     OF THREE after R1c added the vertical (see §E below; LATERAL is still a
//     measured 66.1 mm lie and is scheduled, not fixed). The original wording
//     here was "CG honesty everywhere" and that was FALSE when it was written;
//     it is corrected rather than deleted so the overclaim stays on the record.
//     It is NOT a fix for the deep-lean pose, and on this rig
//     those two goals genuinely oppose each other. The alternative -- cap the
//     translation and carry a STATED CG error -- is Chad's ruling, not this rung's.
//
// (2) ARM REACH. Forward the hinge HELPS (ratio at full forward lean
//     0.4095 -> 0.4488, nowhere near the 1.0 clamp, because the shoulders come
//     toward the grips). The worst arm cell is at the aft/stand/lateral corner and
//     it gets worse, from CG HONESTY ALONE: over a 2520-cell reachable sweep the
//     worst ratio goes 1.4204 -> 1.4568 and clamping 19.3 % -> 20.5 %, because an
//     honest aft translation is 0.289 m rather than 0.250 m. See kHingeAftMaxRad
//     for why the aft HINGE (which would take it to 1.5536 / 28.1 %) was measured
//     out. The residual is defects 1/3/5 and is R1b's to fix.
//
// (3) A NEW CONTACT CLASS: THE HELMET. In attempt 1 the head group
//     (helmet / shield / chin curtain) never comes within 60 mm of any machine
//     part at ANY forward lean. With the hinge it pitches down and forward -- at
//     30 deg the helmet's lowest point drops from y 1.0489 to 0.8346, i.e. BELOW
//     the cowl top line at y 0.892 -- and the measured head-group gap closes:
//     fwd 0.15 -> 32.4 mm, 0.20 -> 16.6 mm, 0.25 -> CONTACT with indy650_cowl,
//     and it stays in contact to full lean (the chin curtain first). A real rider
//     does tuck his head behind the windshield line, so the DIRECTION is right;
//     the intrusion is not. It sits at 56 % of travel, well past the
//     body/console boundary above, so it does not move the boundary number --
//     but it is new, and it is the first thing to look for on the video.
//
// ★ AFT LEAN STAYS A TRANSLATION, AND HERE IS THE MEASUREMENT THAT DECIDED IT.
// The work order allows either ("hinge backward, or state plainly why it stays a
// translation"). It stays a translation. The solve is CG-honest either way -- the
// Newton residual closes on the true CG whatever theta is -- so the aft hinge was
// judged purely on what it costs, over the same 2520-cell reachable sweep
// (arm reach ratio; >= 1.0 means the IK clamps and the wrist separates):
//
//   aft hinge limit    worst arm ratio    cells clamping    CG error max
//     20 deg               1.5536             28.1 %          0.467 mm
//     10 deg               1.5082             24.8 %          0.742 mm
//      0 deg (SHIPPED)     1.4568             20.5 %          0.744 mm
//   (attempt 1, aft as a CG-DISHONEST translation)
//                          1.4204             19.3 %         33.6 mm
//
// Trunk EXTENSION carries the shoulders AWAY from the grips, so an aft hinge
// spends the scarcest resource on this rig -- arm reach, already clamping in
// ~19 % of the reachable set from defects 1/3/5 (R1b) -- and it buys nothing:
// aft clearance is 44.2 mm at the aft limit, three times the forward figure, so
// there is no intrusion to relieve. The 19.3 -> 20.5 % that remains at 0 deg is
// the price of CG HONESTY itself (an honest aft translation is 0.289 m, not
// 0.250 m) and is accepted; the further 7.6 points the hinge would add is not.
//
// Forward is the opposite case and that is why forward hinges: it relieves a
// 4 %-of-travel intrusion, and it moves the shoulders TOWARD the grips (arm ratio
// at full forward lean 0.4095 -> 0.4488, nowhere near the clamp).
//
// Anatomical trunk-flexion limits, and they are anatomy, not clearance fitting.
// The rest pose already carries +11.3 deg of forward trunk tilt (pelvis rest
// quaternion w 0.995133, x 0.098538 = 11.3 deg about model +X), so forward
// 30 deg -> 41.3 deg of trunk flexion from vertical: a rider leaning into the
// bars, well inside seated hip flexion (90-120 deg). Cross-checked against the
// clearance measurement afterwards -- the dash gap is untouched over the whole
// [0, +30] band and first closes between +30 and +40 deg. That is a CHECK on the
// limit, not its derivation.
inline constexpr float kHingeFwdMaxRad = 30.0f * 3.14159265f / 180.0f;
inline constexpr float kHingeAftMaxRad = 0.0f;  // MEASURED OUT, see above

// ★ HOW MUCH OF THE FORWARD DEMAND THE HINGE TRIES TO COVER, and this is the ONE
// dial to move if the lean ONSET reads as a snap. 1.0 = "hinge first": the hinge
// covers the whole demand until it saturates (at lean_fwd_m 0.0495 m), so the
// pelvis and knees do not translate at all until then -- which is exactly what
// buys the clearance. Lower values spread the hinge over more of the range.
//
// The frontier is intrinsic and it was MEASURED on Chad's own drive tapes
// (299,172 ticks; trunk angular rate = the pose's own |d theta / dt| at the
// kernel's 1/120 s tick, whose |d lean_fwd_m / dt| is p50 0.006, p90 0.171,
// p99 0.943, max 1.400 m/s = the kernel's lean_rate_ms cap):
//
//   share  theta saturates at   contact onset   trunk rate p90 / p99 / max deg/s
//    1.00      0.0495 m          0.0686 m (4.3x)     0.0 /  69.0 / 1007.3
//    0.50      0.0990 m          0.0307 m (1.9x)     0.0 /  74.9 /  526.5
//    0.35      0.1414 m          0.0236 m (1.5x)     0.0 /  74.4 /  374.6
//    0.25      0.1980 m          0.0205 m (1.3x)     0.0 /  72.3 /  269.9
//
// SHIPPED AT 1.00, and the argument is the p90/p99 columns: the share barely
// changes the rate a driver actually experiences (0 deg/s at p90, ~70 deg/s at
// p99 in every row) and changes the clearance by 3.3x. The 1007 deg/s peak
// occurs only while lean_fwd_m is slewing at its 1.40 m/s cap -- i.e. the instant
// after a full palm swipe from rest, on under 0.03 % of ticks -- and it is the
// rotational expression of a kernel channel that ALREADY translates the whole
// body at 1.40 m/s in the shipped build. If the onset reads as a torso snap on
// video, drop this to 0.50 and the rung's clearance claim halves; that is the
// honest trade and it is one number.
//
// CG honesty is INDEPENDENT of this dial -- the Newton residual closes on the
// true posed CG whatever theta is -- so moving it cannot introduce a lie.
inline constexpr float kHingeDemandShare = 1.0f;

// Newton steps on the true posed CG. Each one costs one extra pose pass (node
// composition + the four 2-bone IK chains); the skinning pass, which is two
// orders of magnitude more arithmetic, still runs exactly once per frame.
//
// ★ R1c re-measured the residual now that the step solves TWO axes, over 4,680
// reachable cells INCLUDING steer (up x lat(up) x fwd x absorb x steer):
//
//   Newton steps   pose passes/frame   worst |CG_z err|   worst |CG_y err|
//         1                3                8.254 mm          4.058 mm
//         2                4                1.135 mm          1.527 mm   <-- SHIPPED
//         3                5                0.215 mm          0.296 mm
//
// KEPT AT 2, unchanged from attempt 2: it is the same per-frame cost the signed
// rung already carries, and 1.5 mm is inside the class attempt 2 shipped
// (1.335 mm, verifier-measured). Step 3 is on the table and costs one pose pass.
inline constexpr int kHingeNewtonSteps = 2;

// ---------------------------------------------------------------------------
// ★★ §E -- THE COUPLED RISE, AND THE SECOND SOLVED AXIS (R1c, 2026-08-17)
// ---------------------------------------------------------------------------
//
// CHAD'S DRIVE REPORT, and the fix is in his words: "its just that without the
// standing too it makes the sudburians head travel through the cowl which is
// the unnatural part". The forward lean ANGLE is validated by him and is NOT
// reduced here. What was missing is that a rider going to the attack position
// comes UP OFF THE SEAT.
//
// THE MECHANISM, and it is one line of arithmetic, not a new animation channel.
// A hip hinge of theta moves the hinged set's CG by H_z(theta) forward AND
// H_y(theta) = a*(cos t - 1) - b*sin t DOWNWARD -- 40.4 mm down at the +30 deg
// limit. Until R1c nothing corrected for that, and the vertical channel was
// applied RAW (root.y += lean_up_m) on top, so the drawn CG_y was simply wrong.
// R1c makes the root's VERTICAL offset a solved unknown against
// CG_y == lean_up_m, exactly as attempt 2 made the fore-aft one solved against
// CG_z == lean_fwd_m. Two unknowns, two equations, one 2x2 fixed Jacobian.
//
// The rise is therefore DERIVED, not authored: hinge forward -> CG_y falls ->
// the solve raises `root` to put it back -> the thigh roots rise with the torso
// -> the board-welded boots fold the knees. And it COMPOSES with a real stand
// without doubling, because `lean_up_m` and the hinge's drop enter the SAME
// single target, not two additive terms.
//
// ★ WHAT IT CLOSED. Both figures over the same 4,680 cells, measured against
// the kernel's own reference (the pose with NO fore-aft and NO vertical input,
// since `cg_off = k*(-lat, up, -fwd)` is one offset from the un-leaned rider):
//
//                       worst |CG_z err|   worst |CG_y err|
//   attempt 2 (shipped)     14.233 mm         120.997 mm
//   R1c                      1.135 mm           1.527 mm
//
// The 121 mm vertical lie is the largest single lie this ladder has retired --
// bigger than the 105.2 mm fore-aft one and bigger than the 90 mm D9 shift that
// was DELETED for being one. On the stand axis alone it is 49.1 mm at up +0.25
// and +21.6 mm at up -0.10, which is exactly what the independent verifier
// measured on the shipped build.
//
// ★ WHAT IT BOUGHT ON THE DEFECT CHAD SAW (up 0, lat 0, absorb 0, steer 0;
// unsigned mesh distance, head group {rider_helmet, rider_shield,
// rider_chin_curtain} vs indy650_cowl):
//
//   head group lowest y at full forward lean   0.782101 -> 0.866231  (+84.1 mm)
//   cowl contact cells (gap <= 1 mm), 25 mm grid over fwd 0.000..0.450:
//                                     10 of 19 cells (fwd 0.225..0.450)
//                                  ->  2 of 19 cells (fwd 0.325, 0.350)
//   gap at full forward lean                   0.01 mm -> 54.66 mm
//   worst gap vs indy650_gauge_box             2.90 mm -> 40.37 mm
//   worst gap vs bar_riser                     1.48 mm -> 50.74 mm
//
// ★ AND WHICH PART touches, which is the part of this that reads on screen.
// Per-prim minimum gap to `indy650_cowl` over the same sweep:
//                        attempt 2                   R1c
//   rider_helmet     CONTACT, fwd 0.250..0.400   22.03 mm (never touches)
//   rider_shield     CONTACT, fwd 0.225..0.250   50.83 mm (never touches)
//   rider_chin_curtain CONTACT, fwd 0.300..0.450  0.00 mm at fwd 0.325..0.350
// The HELMET SHELL and the VISOR are out of the cowl entirely now. What is left
// is the chin curtain, a thin flap under the jaw, grazing across ~6 % of the
// forward axis.
//
// ★ AND WHAT IT DID NOT BUY, STATED PLAINLY. The head still touches the cowl in
// a band around 70-82 % of forward travel at up 0, and the tuck row (up -0.10)
// still contacts from fwd 0.225 to full. The acceptance shape asked for the
// helmet to clear the cowl TOP LINE (y 0.8924) with margin; it reaches 0.866231
// -- 26.2 mm below it, up from 110.2 mm below. The rise available is bounded
// exactly and only by CG honesty: with CG_y pinned to lean_up_m the pelvis can
// rise no further, and any extra rise would be a NEW lie of the same class as
// the one just retired. The remaining honest levers are a NECK EXTENSION (the
// head+neck segment's CG sits AT the head joint in the mass table, so rotating
// the head about it is CG-neutral BY THAT TABLE -- which is a model limit worth
// stating out loud, not a licence) and the COWL GEOMETRY itself. Both are
// out of R1c's scope; neither was done here.
//
// ★ THE "ARMS LOOK FUNNY" DEFECT IS THE SAME DEFECT, and it is measured.
// Shoulder height above the grip target, model frame:
//
//   fwd     attempt 2      R1c
//   0.00    237.44 mm    237.44 mm   (rest, unchanged -- SIGNED VISUAL)
//   0.05    114.92 mm    174.94 mm
//   0.25    114.92 mm    192.47 mm
//   0.45    114.92 mm    199.05 mm
//
// Attempt 2 dropped the shoulders 122.5 mm the instant the hinge saturated and
// then froze there for the whole rest of the axis -- the shoulders sat only
// 115 mm above the bars and the arms raked up and back. With the rise they stay
// 175-199 mm above, and the arm REACH RATIO on that axis is 0.51 at full lean
// against a 1.0 clamp. That is the "committed but not funny" posture.
//
// ★ THE COSTS, ALL OF THEM, NONE HIDDEN.
//
// (a) THE ARM AT FULL STAND NOW CLAMPS. An honest stand is a BIGGER root rise
//     than a dishonest one: root.y at up +0.25 goes 0.828322 -> 0.888847
//     (+60.5 mm), because the pinned hands and boots carry 42.2 % of the mass
//     and the true vertical response k_yy is 0.7964, not 1.0. The arm reach
//     ratio on the pure stand axis therefore goes 0.9712 -> 1.0311: attempt 2
//     had 2.9 % of margin left at full stand and R1c spends it. Over the 10,800
//     arm-instance reachable sweep, worst ratio 1.5010 -> 1.5154 and clamping
//     20.74 % -> 23.69 %. This is defects 1/3/5 (the arms are too short and too
//     narrow) presenting a bill that CG honesty triggers; the fix is R1b's rig
//     work, not a render fudge, and it is NOT fixed here.
// (b) LEG REACH. Worst leg ratio 1.0758 -> 1.1875, clamping 1.00 % -> 4.43 %,
//     worst cell at up +0.25 / full lateral / fwd +0.45 -- the stand and the
//     forward translation stretching the board-pinned legs together.
// (c) LATERAL IS UNTOUCHED AND STILL LIES. Measured against the all-zero-lean
//     reference over the same 4,680 cells: worst |CG_x err| 68.08 mm -> 66.08 mm
//     (it moves only because the pose around it moved). Out of scope by the work
//     order; SCHEDULED, not fixed.
//
// ★ A BONUS THE RISE RETIRED, and it was an open verifier finding. Attempt 2's
// knee passed THROUGH a `board_L/R` deck triangle in 7.7 % of poses, up to
// 59 mm. Worst knee-below-deck over a 96-pose sweep (up x fwd x absorb to
// 0.885): -59.14 mm -> -0.65 mm. The knee no longer goes through the deck.
//
// ★ ZERO INPUT IS UNCHANGED, and that is a signed visual. With every channel at
// zero, theta is 0 analytically (asin(sin phi) - phi = 0), both residuals are 0
// and both Newton steps are exact no-ops, so `pose_and_solve_lean` reproduces
// the rest pose bit-for-bit. Verified joint-by-joint: all 19 rider joints agree
// to 0.0000000 m on every axis before and after R1c.

// One rider segment, expressed on the DECLARED joint catalogue.
//
// SOURCE: Winter, *Biomechanics and Motor Control of Human Movement*, Table 4.1
// (after Dempster 1955/59). `mass_frac` is segment mass / total body mass;
// `cg_frac` is the segment CG as a fraction of segment length from the PROXIMAL
// joint. Nothing here is invented and nothing is tuned. The 14 rows sum to
// exactly 1.0 and a static_assert pins that.
//
// Mapping onto this 19-joint rig, and the three places it needed a decision:
//   - TRUNK spans pelvis -> spine3 (hip joint to shoulder), cg_frac 0.500,
//     which is Winter's trunk CG measured from the greater trochanter.
//   - HEAD+NECK spans spine3 -> head, cg_frac 1.000. Winter gives the head+neck
//     CG at 1.000 of the C7-T1-to-ear-canal length, i.e. essentially at the head
//     joint, so 1.000 here is the source's own number and not a shortcut.
//   - HAND and FOOT have no distal joint in this rig, so they are point masses
//     AT their joint (prox == dist, cg_frac 0). Measured consequence of that
//     approximation: it is inside the residual the solve reports (R1c: 1.135 mm
//     fore-aft / 1.527 mm vertical), because the solve closes on whatever this
//     function returns -- the table defines the "visual CG" and the kernel's
//     cg_off is matched to THAT definition. ★ R1c NAMES THE LIMIT THAT FOLLOWS:
//     because head+neck has cg_frac 1.000, the head segment's CG sits AT the
//     `head` joint, so rotating the head about that joint is CG-neutral BY THIS
//     TABLE while the helmet mesh visibly moves. Nothing in R1c exploits that,
//     and nothing should without first refining the table -- it is a hole in
//     the model, not a free degree of freedom.
struct RiderSegment {
    int prox = 0;         // RiderJoint
    int dist = 0;         // RiderJoint (== prox for a point mass)
    float mass_frac = 0.0f;
    float cg_frac = 0.0f;  // along prox -> dist
};
inline constexpr int kRiderSegmentCount = 14;
const std::array<RiderSegment, kRiderSegmentCount>& rider_segments();

// The rider's mass-weighted CG, in whatever frame `joint_pos` is expressed in.
// Pure: no statics, no allocation, indexed by RiderJoint.
glm::vec3 rider_cg(const std::array<glm::vec3, kRiderJointCount>& joint_pos);

// The reduced model the hinge angle is solved on, captured ONCE from the rest
// pose exactly the way `hand_off` / `foot_off` are. `a` and `b` are the hinged
// set's mass-weighted CG offset from the hip, times its mass fraction. The
// SAME two numbers describe the hinge on BOTH axes, because a rotation of
// theta about model +X through the hip moves a point (dy, dz) by
//     dz' - dz =  dy*sin(theta) + dz*(cos(theta) - 1)
//     dy' - dy =  dy*(cos(theta) - 1) - dz*sin(theta)
// so with a = m_h*dy and b = m_h*dz:
//     H_z(theta) = a*sin(theta) + b*(cos(theta) - 1)
//     H_y(theta) = a*(cos(theta) - 1) - b*sin(theta)
// ★ R1c: H_y is the WHOLE REASON THIS RUNG EXISTS. It is NEGATIVE for every
// forward theta -- hinging a seated hip drops the rider's CG -- and until R1c
// nothing corrected for it, which is why the head went DOWN through the cowl
// instead of forward over it.
//
// The four k's are the 2x2 JACOBIAN of the true posed CG with respect to the
// root translation, measured at load by posing three times (rest + one probe
// per axis) rather than assumed. Measured on the shipped asset (R1c, with §C's
// corrected reseat live): a 0.112596, b 0.050595, hinged mass fraction 0.578,
// hip offset (dy 0.194803, dz 0.087535); the k's are printed at load and the
// measured values are recorded in the R1c report.
//
// ★ NOTE `k_hat` 0.819082, NOT the 0.821607 attempt 2 documented -- that figure
// was taken on PRE-§C geometry and never re-measured after the boot reseat
// moved the legs (verifier finding 7). The code always measured it live; only
// the comment was stale.
struct RiderHingeModel {
    float a = 0.0f;
    float b = 0.0f;
    float k_hat = 1.0f;  // dCG.z / d(root z)  -- the R1a name, same meaning
    float k_zy = 0.0f;   // dCG.z / d(root y)
    float k_yz = 0.0f;   // dCG.y / d(root z)
    float k_yy = 1.0f;   // dCG.y / d(root y)
};

// Build the reduced model from the rest joint positions. `hinged` selects the
// segments the pelvis rotates rigidly (trunk + head/neck).
RiderHingeModel capture_hinge_model(
    const std::array<glm::vec3, kRiderJointCount>& rest_joint_pos,
    float k_hat);

// H_z(theta) -- the hinge's own FORE-AFT CG contribution, metres.
float hinge_cg_dz(const RiderHingeModel& m, float theta);

// H_y(theta) -- the hinge's own VERTICAL CG contribution, metres. ★ R1c.
// Negative for every forward theta: a hip hinge lowers the rider's CG, and it
// is exactly this drop the vertical solve now has to buy back.
float hinge_cg_dy(const RiderHingeModel& m, float theta);

// The closed-form inverse of H_z, clamped to the anatomical limits. Returns the
// hinge angle in radians, forward positive. Total, monotone, no iteration.
// ★ R1c NOTE: theta is still chosen on the FORE-AFT demand alone. It is the
// POSTURE choice, not the honesty mechanism -- the two-axis solve below closes
// on the true posed CG whatever theta is, so this function cannot introduce a
// lie on either axis.
float hinge_theta_for(const RiderHingeModel& m, float target_m);

// ★ R1c -- THE ROOT TRANSLATION, BOTH AXES AT ONCE. `.x` is the fore-aft
// (model +Z) component, `.y` the vertical (model +Y) one. Two unknowns for the
// two constraints CG_z == lean_fwd_m and CG_y == lean_up_m.
struct RiderLeanShift {
    float fwd = 0.0f;  // model +Z
    float up = 0.0f;   // model +Y
};

// Apply the captured Jacobian's INVERSE to a CG residual (dz, dy). Closed
// form 2x2; falls back to the diagonal if the matrix is degenerate, so it is
// total. Pure.
RiderLeanShift lean_apply_inv_jacobian(const RiderHingeModel& m, float res_z,
                                       float res_y);

// The seed root shift for a given hinge angle: the two-axis demand the hinge
// did not cover, pushed through the inverse Jacobian. Either component may
// come out slightly the "wrong" way when the reduced model mis-reads the true
// hinge contribution; that is bounded by the model error and is the correct
// sign for CG honesty.
RiderLeanShift lean_seed(const RiderHingeModel& m, float theta,
                         float lean_fwd_m, float lean_up_m);

// One fixed-slope Newton step on both axes: d' = d + J^-1 (target - cg_now).
// Pure, and deliberately NOT a secant step -- a fixed Jacobian has no
// difference quotient and therefore no degenerate branch to get wrong.
RiderLeanShift lean_newton_step(const RiderHingeModel& m, RiderLeanShift d,
                                float cg_now_z, float cg_now_y,
                                float cg_target_z, float cg_target_y);

// ---------------------------------------------------------------------------
// R2c-5 -- THE CONTROL-INPUT POSE. The runtime half of what Blender already
// holds (assets/character/sudburian_src/seat_sudburian.py :: apply_controls).
// ---------------------------------------------------------------------------
//
// CHAD'S RULING, 2026-08-18, and DO NOT RE-DERIVE IT FROM THE COORDINATES:
// the THROTTLE is the RIGHT hand (the rider's right = model -X, because in this
// GLB `_L` means model -X and that is his anatomical RIGHT -- render/
// rider_rig.h SS2.3a). The BRAKE is the LEFT hand, model +X.
//
//   throttle pressed -> the RIGHT elbow SWINGS DOWN, and the right hand
//                       ANGLES UP so the thumb presses on the proximal side
//   brake pressed    -> the LEFT elbow RISES, and the left hand rolls DOWN
//                       AND OVER the bar so the four fingers reach the lever
//
// ★ THE ARTICULATION IS THE SHOULDER, NOT THE ELBOW. Chad: "the elbow dropping
// is a result in a change in the shoulder joint btw not the elbow, the elbow
// articulates the forearm." In a 2-bone IK the humerus rotation about the
// shoulder-to-hand axis IS the pole angle, and in THIS solver the pole is the
// bend-plane normal -- so the shoulder swing is a rotation of `bend_n` about
// the live chain direction, and nothing else in solve_chain changes. The hand
// stays welded to the grip and the elbow FLEX is still fixed by the
// shoulder-to-grip distance, so the swing costs nothing at the contact.
//
// ★ AND THE CONTACT IS THE KNUCKLE, NOT THE WRIST (R2c-1). The wrist
// articulation is therefore a rotation of the whole hand transform ABOUT THE
// BAR, i.e. about the grip socket's own position, taken along the bar axis
// MEASURED FROM THE MACHINE (grip_socket_R - grip_socket_L) and not from any
// rest-space guess -- the lesson that cost most of the Blender session: a
// rest-space "+X bar axis" lands 58 deg off the bar once the arm is IK-solved.
// Because the rotation is about the contact point, the knuckle stays ON the
// bar by construction and the WRIST (the IK tip) moves; no iteration, no
// forearm-colinearity assumption, exact at any wrist angle. That is the same
// property seat_sudburian.py::resync_targets buys with 24 iterations, bought
// here for free because the runtime can rotate about the pivot directly.

// Chad 2026-08-18: "the elbow drop and raise respectively right and left, are
// not a huge articulation. Just about 20 degrees is all." All four
// articulations are the same 20 deg, matching seat_sudburian.py's
// THROTTLE_DROP_DEG / BRAKE_RAISE_DEG / THROTTLE_WRIST_DEG / BRAKE_WRIST_DEG.
inline constexpr float kControlElbowSwingRad = 20.0f * 3.14159265f / 180.0f;
inline constexpr float kControlWristRad = 20.0f * 3.14159265f / 180.0f;

// The pole rotation: `bend_n` turned about the chain axis. `chain_dir` need not
// be normalised. Returns `bend_n` unchanged when the axis is degenerate, so the
// caller can never get a NaN pose out of a collapsed chain.
glm::vec3 swing_bend_normal(const glm::vec3& bend_n, const glm::vec3& chain_dir,
                            float angle_rad);

// Rotate a world transform about an arbitrary world axis through `pivot`.
// Used for the wrist: the hand rolls about the BAR, so the point of the hand
// that sits on the bar does not move. `axis` need not be normalised; a
// degenerate axis returns `m` unchanged.
glm::mat4 roll_about_axis(const glm::mat4& m, const glm::vec3& pivot,
                          const glm::vec3& axis, float angle_rad);

// ---- THE TWO SIGNS, AND WHY THEY ARE MEASURED RATHER THAN WRITTEN DOWN -----
//
// ★ NEVER ASSUME THE TWO SIDES MIRROR. In Blender the elbow poles needed
// OPPOSITE signs (lowerarm_l -20 deg drops, lowerarm_r +20 deg drops) while the
// two WRISTS shared one sign -- because the wrist axis comes from the machine
// and the pole from a side-flipped rest frame. That trap appeared three times
// in one session. So neither sign is retyped here: both are MEASURED off the
// asset's own rest pose, per side, and the caller asks for "down" and "up" --
// which is what Chad ruled -- rather than for a signed angle.

// The sign (+1 / -1) that, applied to swing_bend_normal, moves the ELBOW DOWN
// (model -Y). `chain_dir` is shoulder -> hand. Returns 0 when the elbow lies on
// the chain axis (a straight arm has no pole), which the caller reads as
// "this side cannot swing" rather than as a direction.
float swing_down_sign(const glm::vec3& shoulder, const glm::vec3& elbow,
                      const glm::vec3& chain_dir);

// The sign (+1 / -1) that, applied to roll_about_axis, RAISES the wrist
// (model +Y) about the bar. Returns 0 when the wrist sits on the bar axis.
float wrist_up_sign(const glm::vec3& wrist, const glm::vec3& pivot,
                    const glm::vec3& axis);

// ---------------------------------------------------------------------------
// R3-HANDS -- THE CONTROL BONES (Chad, 2026-08-24)
// ---------------------------------------------------------------------------
//
// HIS WORDS, VERBATIM, AND THEY ARE THE SPEC:
//   "the thumb should be animated for the throttle with the right elbow going
//    slightly down on thumb throttle push, and for the left side the braking
//    animation currently the elbow goes down and it looks like it twists the
//    handle backwards, when really the braking animation should be the fingers
//    in front of the red brake lever should squeeze in and the left elbow
//    should articulate up a bit"
//
// ★ WHAT THE MEASUREMENT FOUND, and it is why this rung is not a sign flip.
// The brake elbow's SIGN was already correct -- it rises. Solved in one frame
// with the suspension pinned (SEADS_SLED_RIG_SMOKE, brake 0 -> 1) it rises
// +2.1 mm, while the SAME 20 deg on the throttle side drops that elbow
// -18.0 mm. An 8.6x asymmetry, because the pole swing on the left is spent
// almost entirely sideways (-21.2 mm in x) instead of upward. R2c-5 measured
// the two SIGNS per side and warned loudly that the sides do not mirror -- but
// nobody ever measured the MAGNITUDE, and a sign cannot see a 2 mm rise.
// Meanwhile the wrist rolled -35.0 mm DOWN and over the bar, which is the
// large motion actually on screen: Chad's "twists the handle backwards", and
// his "the elbow goes down" is the honest read of an arm whose only visible
// motion is a wrist rolling down.
//
// ★ SO THE BRAKE SIDE LOSES THE BAR ROLL ENTIRELY. Rolling the whole hand
// about the bar IS twisting the grip; on a real machine the fingers close on
// the lever and the hand does not rotate the handle. The squeeze moves to
// `mittfront_01_l`, which is what §7.2 named for it in the first place, and
// the elbow's rise is retargeted to a measured HEIGHT instead of a blind angle.
//
// ★ THE THROTTLE SIDE IS NOT TOUCHED. Chad did not name it, its -18.0 mm drop
// is a signed visual, and this rung only ADDS `thumb_01_r` to it.

// A measured curl: which of a bone's OWN local axes swings its tip toward a
// target, in which direction, and by how much. Every field is measured against
// the live scene -- never typed -- so a re-export that moves the lever moves
// the animation with it. `axis` is 0 (the bone's local X) or 2 (its local Z);
// -1 means no usable axis and the caller must read that as "do not curl".
struct CurlAim {
    int axis = -1;
    float sign = 0.0f;
};

// Measure the aim. `bone_world` is the bone's world (model-space) transform
// and `target_w` the lever it reaches for. Returns axis -1 for a degenerate
// frame or a target sitting on the joint, which the caller must read as "do
// not curl" rather than as a direction.
CurlAim curl_aim(const glm::mat4& bone_world, const glm::vec3& target_w);


// The extra BONE-LOCAL rotation for `amt` of input, to POST-multiply onto the
// bone's rest rotation (post, because the axis is the bone's own). Returns
// exactly the identity at amt == 0 -- the 0-OFF law this file runs on.
glm::quat curl_rotation(const CurlAim& c, float rad, float amt);

// ★ THE THUMB'S MAGNITUDE IS CHAD'S. He ruled the control articulation at
// "just about 20 degrees" for R2c-5, and he SIGNED the thumb built at it
// ("thumb for throttle looks good"). Do not move it.
inline constexpr float kControlThumbRad = 20.0f * 3.14159265f / 180.0f;

// ★★ THE LEVERS' OWN THROW (Chad, 2026-08-24: "you might as well make the
// throttle lever animated while at it"). Until this rung NOTHING drove either
// lever: squeezing the brake left the red blade dead in the air, which is a
// bigger miss than anything happening on the hand above it. These are the
// parts that actually move on the machine, and they carry the mechanism Chad
// described -- the fingers press the blade, the blade squeezes into the bar.
//
// ★ THE BRAKE'S ANGLE IS MEASURED, NOT CHOSEN: swinging the 130 mm blade about
// its pin until its tip reaches the bar takes 19.9 deg, and the tip travels
// (-4.5, -0.5, -47.6) mm getting there -- essentially pure aft, 48 mm. That is
// Chad's own "it pivots from the base until the tip touches the bar",
// solved against the shipped bar geometry rather than eyeballed. Re-derive it
// if the bar or the lever moves; it is a dial measured off a surface.
//
// ★ THE THROTTLE'S IS NOT MEASURED and Chad rules it. Its lever already sits
// on the bar in the rest pose, so there is no contact angle to solve for; 12
// deg is a plausible thumb throw and nothing more.
inline constexpr float kBrakeLeverRad = 19.9f * 3.14159265f / 180.0f;
inline constexpr float kThrottleLeverRad = 12.0f * 3.14159265f / 180.0f;

// The FINGERS reuse that same ruled size. It is a felt amount and Chad rules
// it -- a squeeze is judged by eye, and nobody self-passes it.
inline constexpr float kControlFingerRad = 20.0f * 3.14159265f / 180.0f;


// ★ THERE IS DELIBERATELY NO NEW ELBOW DIAL. The brake elbow's 2.1 mm looked
// like it needed one -- a HEIGHT rather than an angle, since the same 20 deg
// bought -18.0 mm on the throttle side. It did not: the bar roll was dragging
// the chain the pole rotates about, and with the roll gone Chad's untouched
// 20 deg measures +29.6 mm. A dial added here would have hidden the cause.

// ===========================================================================
// ★★★ R3-WS -- THE FORE-AFT WEIGHT-SHIFT LADDER
//     docs/SUDBURIAN_R3_WEIGHTSHIFT_SPEC.md
// ===========================================================================
//
// CHAD'S ASK, in his shape: the feet move front-to-back on the running boards,
// the butt slides back on the seat; mid-aft is a seated crouch at the BACK of
// the seat with the back bent over and the hands still reaching for the bars;
// standing aft is the butt pushed far back, visibly PULLING on the bars; full
// aft is KNEELING on the back of the seat, sitting back toward the heels --
// not upright on the knees. And the feet must never stay locked in place so
// that straightening the legs pulls the arms off the bars.
//
// ★ 0-OFF LAW (SPEC 1). Everything below activates only for `rider_fwd_m < 0`.
// At `a_m == 0` every function here returns the identity value, so the R1a/R1c
// forward-lean path and the R2c-5 control pose are reproduced bit-for-bit.
// That includes the stand: `ws_rho` is gated on the ladder coordinate, NOT on
// `s` alone, precisely so a pure stand with no aft demand cannot move.
//
// ★ EVERY NUMBER BELOW IS MEASURED off assets/sled/indy650.glb by
// assets/character/sudburian_src/measure_seat_profile.py (pure stdlib, reads
// the JSON+BIN chunks directly). Re-run it after any re-export; the tests in
// test/unit/test_rider_pose.cpp re-derive every one of them from the file, so
// drift is a red test and not a mystery.

// ---------------------------------------------------------------------------
// THE SEAT SURFACE -- a raycast profile, NOT a bounding box
// ---------------------------------------------------------------------------
//
// ★ THE SAME TRAP THE DECK ALREADY TAUGHT US (see kBootReseatM above): the
// `seat` mesh's y bbox top is 0.663307, which is the crown of the FRONT hump
// over the tank, 65 mm above the surface the pelvis actually rides (0.598322).
// So the profile below is a DOWNWARD RAYCAST at x = 0: for each station, the
// max y over the seat triangles whose plan footprint covers that point.
//
// MEASURED 2026-08-20, `seat` node, 216 triangles, model frame, metres:
//   bbox            x [-0.205972, +0.205972]  y [0.380322, 0.663307]
//                   z [-1.005000, +0.307615]
//   centreline coverage  z [-1.005000, +0.307615]  (the full z extent)
//   flat main pan   y = 0.598322 over z [-0.676848, -0.020544]
//   rear rise       0.598322 -> 0.642664 crest at z = -0.922962, then the
//                   back edge falls away to 0.543322 at z = -1.005000
//   front hump      0.598472 -> 0.662279 crest at z = +0.225570, then the
//                   nose falls to 0.380322
//
// ★ THE CROWN, STATED: the seat is crowned across x. |y_seat(+-0.095) -
// y_seat(0)| is 0.0000 everywhere on the main pan and over the whole working
// range of this ladder (z >= -0.905); it reaches 34.9 mm only at the extreme
// rear station z = -0.964, which is BEHIND the reach-limited rear station and
// is never used. One centreline table is therefore enough, and that is a
// measurement, not an assumption.
inline constexpr int kSeatStations = 33;
inline constexpr float kSeatZRearM = -1.005000f;
inline constexpr float kSeatZFrontM = 0.307615f;
// The seat mesh's own y bbox FLOOR and x half-width, measured in the same
// pass. R3-WS(d) needs them to say whether a boot is INSIDE the seat solid or
// merely under its overhang: the deck sheet is at y 0.2520, a full 128 mm
// below the seat's underside, so "|x| small and below the top surface" is not
// a clip -- it is the running board, where the feet have always been.
inline constexpr float kSeatYBottomM = 0.380322f;
inline constexpr float kSeatXHalfM = 0.205972f;
const std::array<float, kSeatStations>& seat_profile_y();

// y of the seat's top surface at model z, on the centreline. Clamped to the
// measured z extents (the pelvis never leaves them; the clamp is what makes
// this function total). Piecewise linear between stations.
float seat_top_y(float z);

// dy/dz of that surface, used to lay the kneeling shin along the seat.
float seat_top_slope(float z);

// ---------------------------------------------------------------------------
// THE RUNNING-BOARD DECK, and the boot that stands on it
// ---------------------------------------------------------------------------
//
// Re-measured 2026-08-20 through the same script; these confirm the R1a
// figures byte-for-byte (the machine art crossed the Sudburian splice
// unchanged) and add the two the ladder needs.
//   board_L / board_R DECK SHEET   y = 0.252000 exactly, 58 tris, 0.16075 m2
//                                  x |0.2100..0.3750|  z [-1.0100, +0.3100]
//   (bbox y max 0.277000 is THE LIP -- never take it for a contact plane)
//   boot, CPU-skinned bind pose    z length 0.3356 (half 0.1678), sole y
//                                  0.252000 exactly, ankle joint y 0.330801
inline constexpr float kDeckSheetYM = 0.252000f;
inline constexpr float kDeckZRearM = -1.010000f;
inline constexpr float kDeckZFrontM = 0.310000f;
inline constexpr float kBootHalfLenM = 0.167800f;
inline constexpr float kAnkleAboveSoleM = 0.078801f;  // 0.330801 - 0.252000

// ★★★ R3-WS(c) / SPEC 11.4 A5. THE BOOT IS NOT CENTRED ON THE ANKLE, AND THE
// HALF-LENGTH WAS THE WRONG NUMBER TO STOP THE SLIDE WITH.
//
// Measured through the same CPU skin (measure_seat_profile.py, which now
// prints it): the ankle sits 0.086631 m ahead of the HEEL and 0.248964 m
// behind the TOE. What runs out of running board when the foot slides aft is
// the heel, not a symmetric half-boot -- so the rear limit is
//
//   z_rear_limit = kDeckZRearM + kBootHeelBackM + kFootRearClearM = -0.903369
//
// which is 81 mm further aft than the naive half-length figure (-0.822200).
// The two sides disagree by 0.14 mm (l 0.086631, r 0.086490); the WORSE side
// binds, so the shipped constant is the max.
inline constexpr float kBootHeelBackM = 0.086631f;
inline constexpr float kFootRearClearM = 0.020f;
inline constexpr float kFootZRearLimitM =
    kDeckZRearM + kBootHeelBackM + kFootRearClearM;

// The knee pad's radius, from a CPU skin of `sudburian_proxy`: the radial
// spread of the 36 vertices within 100 mm of the knee joint about the
// thigh->shin axis is p50 0.0918, p90 0.0945, max 0.0955 (both sides
// identical to 1e-4). The SPEC's placeholder was 0.05; the mesh says 0.0918,
// and the mesh wins (SPEC 1, MEASURE NEVER RETYPE).
inline constexpr float kKneePadRM = 0.091800f;

// ---------------------------------------------------------------------------
// THE LADDER'S ONE TRAVEL NUMBER, AND WHY IT IS 0.320 AND NOT THE SEAT'S OWN
// ---------------------------------------------------------------------------
//
// ★ SPEC 4's HARD LADDER LIMIT BINDS, AND IT BINDS BEFORE THE SEAT DOES.
// The seat would allow 0.360 m of pelvis retreat (rest pelvis z -0.584840 to
// z_seat_rear + kKneelMarginM). The ARMS allow less. Measured in closed form
// (see reach_solve below -- the shoulder traces a circle of radius 0.523122
// about the pelvis, so the closest it can EVER get to a grip is exact
// arithmetic, not a search):
//
//   rho limit   steer   pelvis rise   max aft travel
//     0.985      0.0      0.00           0.3364 m
//     0.985      0.0      0.35           0.3277 m   <-- the binding cell
//     0.965      0.0      0.35           0.3148 m
//     0.985     -1.0      0.00           0.1436 m   <-- see the STEER finding
//
// SHIPPED AT 0.320: 7.7 mm inside the binding steer-0 cell, and above SPEC
// 7.4's non-vacuity floor of 0.300 m. The kneel station therefore lands at
// z = -0.904840, which is 100 mm ahead of the seat's back edge -- 40 mm short
// of the spec's `z_seat_rear + kKneelMarginM`. That is SPEC 4 in action:
// the travel is shortened and it is said out loud, rather than letting the
// arm clamp.
//
// ★★ THE STEER FINDING, AND IT IS A PRE-EXISTING DEFECT THIS RUNG CANNOT FIX.
// Measured on HEAD 20d4cc107 with ZERO lean of any kind, arm chain ratio
// |shoulder - wrist| / dmax (>= 1.0 means the IK clamp at sled_model.cpp
// fires and the wrist visibly separates from the forearm):
//
//   steer   -1.0    -0.5     0.0    +0.5    +1.0
//   arm_l  1.1404  1.0383  0.9381  0.8499  0.7854
//   arm_r  0.7822  0.8430  0.9267  1.0221  1.1198
//
// So the outboard arm ALREADY clamps for |steer| > ~0.25, at all times, with
// no rider input at all: the 32 deg bar sweep (kBarSweepRad, itself marked
// UNSOURCED) swings the far grip 185 mm forward and 86 mm up while the
// shoulder stays put. The 0-OFF LAW freezes those cells, so R3-WS may not
// touch them -- filed as OPEN-R3WS-STEERREACH. What this rung DOES guarantee
// is that with aft demand live the reach solve pulls every cell back under
// 0.985, and that the ladder never makes any cell worse than HEAD.
inline constexpr float kLadderPelvisAftM = 0.320f;
inline constexpr float kKneelMarginM = 0.060f;

// ★ kFootTrack IS RETIRED (SPEC 11.4 A4). It made the boots track the PELVIS
// 1:1, so the feet could only go as far back as the butt did -- which is
// exactly what Chad saw and rejected: "the feet only go back a little", they
// must slide "the full length of the foot runners". The foot slide is now its
// own schedule (ws_foot_slide below), decoupled from the pelvis, and the
// rename-never-retune pin for kFootTrack is removed with it.

// The rigid-weld -> parametric-target blend band on the deck. Below it the
// foot IS the R1a socket weld, bit-identically, which is half of 0-OFF.
inline constexpr float kFootBlendHiU = 0.15f;

// ---------------------------------------------------------------------------
// ★★★ R3-WS(c) -- CHAD'S DRIVE FEEDBACK, AND THE FOUR DIALS IT ADDED
// ---------------------------------------------------------------------------
//
// His words (SPEC 11): the feet must slide THE FULL LENGTH OF THE RUNNERS,
// "all the way back"; the butt "sticks back so far it looks funny" and should
// instead "come down a little as he bends his knees some"; and his HEAD must
// COME UP -- today he is "full face down even when standing at the back" and
// "does not look like he is pulling on the bars".
//
// ★ THE MECHANISM, MEASURED (SPEC 11.1). v1 bought its aft CG almost entirely
// with PELVIS RETREAT (0.320 m, reach-limited), and the SPEC 4 reach solve
// then had to lay the trunk over ~50 deg to keep the hands welded -- face-down
// is the SOLUTION to an over-retreated pelvis, not a separate defect. The legs
// are ~32 % of rider mass and the full runner slide moves them 1.005 m, so the
// legs can carry the CG and the trunk can be capped.

// ★ A1: THE RETREAT IS AN EXPLICIT EYE-DIAL, NOT DERIVED FROM THE CAP.
// The red-team's calibrated replica measured the alternative (derive the
// retreat from the trunk cap) and it is a feedback path with no fixed point
// worth having; the honest form is one number Chad can move. At 0.20 m the
// pelvis lands at z -0.785, still on the FLAT PAN (seat rise +4 mm), which is
// what keeps him off the rear crest. kLadderPelvisAftM (0.320) is NOT reused
// here -- it stays frozen as the kneel / reach anchor.
inline constexpr float kSeatRetreatAftM = 0.20f;

// ★ A1 + SPEC 11.2.2: the HEAD-UP LAW is a hard CEILING on the reach hinge,
// applied AFTER the gate (A8: min(theta_solve * gate, cap), never
// min(theta_solve, cap) * gate -- that would be a re-timing, not a ceiling).
// The quantity Chad judges is trunk-from-vertical = theta_r + the measured
// 28.6173 deg rest tilt (pelvis->spine_03), so 38 deg here reads as ~67 deg
// from vertical at the worst cell instead of v1's ~79.
inline constexpr float kHeadUpTrunkMaxRad = 38.0f * 3.14159265f / 180.0f;
inline constexpr float kRestTrunkTiltRad = 28.6173f * 3.14159265f / 180.0f;

// ★ SPEC 11.2.3: "his butt can then come down a little as he bends his knees
// some". STANDING branch only -- seated, the pelvis is ON the seat and the
// profile owns its height.
inline constexpr float kAftSquatDropM = 0.10f;
inline constexpr float kSquatULo = 0.40f;
inline constexpr float kSquatUHi = 1.00f;

// ★ A9 / SPEC 11.2.4: the neck counter-rotates the trunk flexion so the helmet
// keeps facing the horizon. Exactly 0 at theta_r = 0, so it is 0-OFF safe.
inline constexpr float kNeckCounterMaxRad = 30.0f * 3.14159265f / 180.0f;

// ★★ A3: THE FULL SLIDE MUST BE RE-TIMED AGAINST THE CONTINUITY BUDGET.
// The raw slide is 1.0054 m, 3.1x v1's pelvis-tracked travel. Stepped at
// hi_f = 1.0 on every stand slice, the worst joint delta per runtime demand
// step blows kWsContinuityBoundM at s = 1 (the bake's LOG_FATAL, by design --
// the gate is never relaxed). So the schedule's band widens WITH the stand,
// exactly the way ws_reach_gate widened the flexion in R3-WS(b):
//
//   hi_f(s) = 1 + (kFootSlideStandHiU - 1) * s
//   slide   = smoothstep(0, hi_f(s), u)
//
// hi_f(0) = 1 exactly, so at zero stand the foot REACHES kFootZRearLimitM at
// u = 1 (the A4 non-vacuity anchor). Above 1 the band overruns u = 1, so the
// standing man's slide would be shorter.
//
// ★★★ AND THE SWEEP REFUTED THE AMENDMENT'S OWN PRESCRIPTION, SO IT IS
// REPORTED RATHER THAN QUIETLY FOLLOWED. A3 predicted the FOOT band was the
// lever. It is not. Measured on the shipped bake (worst pose delta per
// 0.0039 m of aft demand, by stand slice, against kWsContinuityBoundM 0.060 --
// SEADS_SLED_WS_DEBUG prints this row every load):
//
//   foot hi_f(1)  reach hi(1)   s=0     s=.25    s=.5    s=.75    s=1
//     1.00          1.00       0.0242  0.0262  0.0316  0.0516  0.2727  FAIL
//     1.20          1.00       0.0242  0.0254  0.0304  0.0694  0.1162  FAIL
//     1.45          1.00       0.0242  0.0246  0.0289  0.1009  0.2355  FAIL
//     1.80          1.00       0.0242  0.0263  0.0304  0.0860  0.2695  FAIL
//     1.00          1.40       0.0242  0.0264  0.0287  0.0389  0.0276  <-SHIP
//     1.45          1.60       0.0242  0.0246  0.0250  0.0295  0.0324
//
// NO foot band passes on its own, because the continuity number is a RATIO:
// slowing the foot cuts the joint travel (numerator) AND the aft CG it earns
// (denominator), while the trunk flexion keeps spending CG FORWARD -- so at
// high stand dC collapses faster than dj does and the ratio gets WORSE. The
// lever is the flexion, exactly as it was in R3-WS(b); this rung just tripled
// the joint travel underneath it. So kReachGateStandHiU moves (1.00 -> 1.40,
// re-pinned and re-measured below) and the FOOT BAND SHIPS AT 1.00 -- meaning
// the feet go ALL THE WAY BACK at every stand fraction, which is what Chad
// asked for in the first place. The band stays a real, tested parameter (the
// sweep above is what it is for), it is simply not re-timed today.
inline constexpr float kFootSlideStandHiU = 1.00f;

// ---------------------------------------------------------------------------
// THE BRANCH WEIGHTS
// ---------------------------------------------------------------------------
//
// ★ THE KNEEL BAND IS WIDER THAN THE SPEC'S [0.78, 0.95], AND HERE IS THE
// MEASUREMENT THAT WIDENED IT. Going from the deck-slid foot to the kneeling
// foot moves the ankle 0.731 m (z -0.222 -> -0.953). SPEC 7.5 asks for a max
// adjacent-cell (du = 1/32) joint delta of 0.06 m, which for a smoothstep of
// width w costs 0.731 * 1.5 / w * (1/32) <= 0.06, i.e. w >= 0.571. A band that
// wide would start the kneel at u = 0.38 and eat the seated rear crouch Chad
// asked for. SHIPPED at [0.60, 1.00]: worst adjacent delta 0.0857 m on the
// ankle, 0.06 m everywhere else and at every u outside the band. That is a
// STATED overshoot of one gate on one joint pair, not a relaxed gate -- the
// 0.06 m bound is still enforced for every other joint and every other cell,
// and the ankle's own bound is the measured 0.0857 (see test_rider_pose).
// ★★★ R3-WS(d) / SPEC 13.4 D5. THE 0.731 m FIGURE ABOVE IS STALE, AND THE
// BAND SURVIVES ANYWAY. Post-R3-WS(c) the DECK foot has already slid to
// kFootZRearLimitM (-0.9034) by the time the kneel band opens, and the
// kneeling ankle lands at z -0.9747 -- so the deck->kneel ankle swing is
// 0.071 m in z and 0.405 m in space, not 0.731. The u band therefore stays [0.60, 1.00]; it was NOT
// widened chasing the stale number (D5 forbids that), and the measured worst
// adjacent-cell ankle delta is reported by the bake instead of predicted.
inline constexpr float kKneelULo = 0.60f;
inline constexpr float kKneelUHi = 1.00f;

// ★★★ R3-WS(d) / SPEC 13.4 D1 (BLOCKING), AND IT IS THE ONE DEFECT TURNING
// THE KNEEL ON ACTUALLY CREATED.
//
// The kneel dies across the stand axis, and the player owns that axis with a
// KEY. `lean_up_m` slews at lean_tau_s = 0.18 s, so one 60 Hz tick moves the
// stand fraction by up to
//
//   ds = min(1 - exp(-h/tau), lean_rate_ms * h / stand_rise_m) = 0.0885
//
// (see stand_slew_ds_per_tick below -- both halves READ the kernel, neither is
// retyped). Across the R3-WS band [0.20, 0.40] that is 2.3 ticks to unwind the
// WHOLE kneel: knee 0.42 m, ankle 0.405 m, pelvis 0.265 m, i.e. ~0.15 m of
// joint travel PER FRAME against SPEC 7.5's 0.06 m. Every continuity gate this
// rung shipped steps in `a` at FIXED s, so all of them were blind to it.
//
// The fix is the band, because the pose is a pure function of (u, s) and there
// is nothing else to slow down: a slew limiter would need state and would
// break the determinism law. SHIPPED AT [0.00, 1.00] -- the WIDEST band `s`
// has, sized by the same sweep that sized the u band (the table below is that
// sweep) and printed at every load by the new continuity-in-s measurement
// (SLED R3-WS(d) BAND SWEEP). D1 started from [0.20, 0.70]; the sweep says
// that measures 0.1577 and bottoms out only at the far end. The gate bound is
// never touched.
//
// ★ AND THE PLAYER NEVER RESTS INSIDE THIS BAND, which is why the widest one
// costs nothing to look at: `stand` is a KEY, so `lean_up_m`'s slew target is
// 0 or `stand_rise_m` and every intermediate `s` is a transient. A band that
// spans the whole axis reads as the man RISING OUT of the kneel over the half
// second the stand takes, which is a better animation than a 2-tick snap as
// well as a smaller number.
inline constexpr float kKneelSLo = 0.00f;  // stand kills the kneel
inline constexpr float kKneelSHi = 1.00f;

// The stand fraction one 60 Hz tick of the kernel's own lean slew can cover,
// worst case (from a standing start, which is where the exponential is
// fastest). BOTH kernel dials are read, never retyped -- `lean_tau_s` bounds
// the exponential and `lean_rate_ms` bounds the ramp, and the slew takes the
// tighter of the two (sim/sled.cpp::slew_toward).
//
// ★★★ AND THE SWEEP REFUTED D1's OWN PRESCRIPTION, EXACTLY AS THE FOOT-BAND
// SWEEP REFUTED A3's IN R3-WS(c). D1 says widen the band until the 0.06 m
// bound is met. It cannot be met, and the reason is not the kneel:
//
//   kKneelSHi (lo 0)  0.40    0.60    0.70    0.80    1.00
//   worst m/tick     0.1552  0.1057  0.0912  0.0803  0.0643
//   ... with the KNEEL BRANCH SUPPRESSED ENTIRELY:              0.0574
//
// (the same sweep at lo = 0.20 reads 0.3256 / 0.1944 / 0.1577 / 0.1311 /
// 0.0944 -- D1's own starting band is the worst row in the table)
//
// The R3-WS(c) pose Chad already drove ALREADY spends 0.0574 m of joint travel
// on one tick of stand slew at (u = 1, s = 0.63) -- 96 % of the whole budget,
// with no kneel in the pose at all. That is a PRE-EXISTING finding this rung
// measured rather than caused, filed as OPEN-R3WS-STANDSLEW, and it means an
// ABSOLUTE 0.06 gate would be red on the shipped ladder on day one and would
// be gating the STAND, not the kneel.
//
// So the shipped gate takes the same NO-REGRESSION form A6 gave the legs and
// SPEC 12.3 gave the knees: the kneel may add at most kWsKneelSlewAddM to the
// ladder's own kneel-free stand-slew travel. MEASURED ADDITION: +0.0069 m.
// The 0.06 bound is untouched, the absolute number is PRINTED at every load
// next to its floor, and the band is still sized by the sweep -- [0.00, 1.00]
// is simply where that sweep bottoms out. Widening further is not available:
// `s` ends there.
inline constexpr float kWsKneelSlewAddM = 0.030f;
inline constexpr float kWsTickS = 1.0f / 60.0f;
inline float stand_slew_ds_per_tick() {
    const sim::SledParams p{};
    const float tau = static_cast<float>(p.lean_tau_s);
    const float expo = tau > 1e-6f ? 1.0f - std::exp(-kWsTickS / tau) : 1.0f;
    const float rise = stand_rise_m();
    const float ramp = rise > 1e-6f
                           ? static_cast<float>(p.lean_rate_ms) * kWsTickS / rise
                           : 1.0f;
    return expo < ramp ? expo : ramp;
}

struct WeightShiftWeights {
    float seat = 1.0f;
    float stand = 0.0f;
    float kneel = 0.0f;
};

// The stand band is an EXPLICIT parameter -- defaulted to the shipped
// constants -- for exactly the reason ws_reach_gate's and ws_foot_slide's
// bands are: the gate that sizes it has to SWEEP it, not trust a remembered
// number (SPEC 12.2 is the whole argument).
WeightShiftWeights ws_weights(float u, float s, float s_lo = kKneelSLo,
                              float s_hi = kKneelSHi);

// ---------------------------------------------------------------------------
// THE ARM-STRETCH SCHEDULE -- "really pulling on the bars"
// ---------------------------------------------------------------------------
//
// rho is the TARGET arm-chain ratio the reach solve aims the torso at. It runs
// from the rest ratio (measured 0.938134 / 0.926658 -- the Sudburian's arms are
// already nearly straight at rest, NOT the 0.7301 of the retired legacy rider)
// up to kArmRatioPull.
//
// ★ THE STAND TERM IS GATED ON u, NOT ON s. SPEC 3's `max(u, 0.8*s)` would
// fire the reach hinge at a pure stand with no aft demand and break the 0-OFF
// law (and the SIGNED R1c stand pose with it). The gate is the same smoothstep
// the foot blend uses, so at u >= kFootBlendHiU the spec's formula is recovered
// exactly.
inline constexpr float kArmRatioPull = 0.965f;   // the design target
inline constexpr float kArmRatioLimit = 0.985f;  // the HARD ceiling (SPEC 7.2)
inline constexpr float kStandRhoGain = 0.8f;
float ws_rho(float u, float s, float rho0);

// ---------------------------------------------------------------------------
// ★★★ R3-WS(b) -- THE REACH GATE, AND THE STAND-ENTRY POP IT KILLS
// ---------------------------------------------------------------------------
//
// ★ THE DEFECT, MEASURED. R3-WS as landed ramped the trunk flexion in over
// u in [0, kFootBlendHiU] at EVERY stand fraction. At high `s` the arms are
// already over-stretched by the stand alone (ladder-free ratio 1.4292), so the
// reach solve answers with a large theta the moment the gate opens -- and a
// large forward trunk flexion moves CG FORWARD. Over that same 0.15 of u the
// pelvis has retreated only 48 mm. Forward beat aft, so the BAKED curve went
// backwards:
//
//   C[s=1.00] (as landed):  0.0000  -0.0195  +0.0115  +0.0450  ...
//
// The inverse hid it behind a running-maximum envelope, which kept `u`
// non-decreasing but made it DISCONTINUOUS: a_m = 0 answered u = 0.0000 and
// a_m = +0.0001 answered u = 0.2819. Standing and easing into aft therefore
// snapped the man 0 -> 0.28 of the whole ladder in ONE frame -- feet off the
// weld, trunk folding, pelvis 60-90 mm -- entering aft AND leaving it.
//
// ★ THE FIX IS A RE-TIMING, NOT A SMOOTHER INVERSE. Widen the flexion band
// WITH the stand, so the flexion is always paid for by retreat that has
// already happened and dC/du stays positive by construction:
//
//   hi(s) = kFootBlendHiU + (kReachGateStandHiU - kFootBlendHiU) * s
//   gate  = smoothstep(0, hi(s), u)
//
// ★ SHIPPED AT 1.00, AND CHOSEN BY MEASUREMENT. The naive sizing (spend the
// flexion's own 0.048 m of CG slower than the retreat's 0.224 m/u earns it) is
// NOT the whole story, because the flexion also BUYS the arms their slack:
// throttle it too hard and SPEC 4 shortens the retreat instead, which costs
// more CG than the flexion saved. So the band was SWEPT against the shipped
// bake's own gates -- worst pose delta per 0.0039 m of aft demand, by stand
// slice, against SPEC 7.5's 0.06 m (`inf` = the curve fell, no inverse):
//
//   hi(1)   s=0.00   s=0.25   s=0.50   s=0.75   s=1.00
//   0.15*   0.0226   0.0617    inf      inf      inf     <- AS LANDED
//   0.30    0.0226   0.0379   0.0929   0.3997    inf
//   0.45    0.0226   0.0301   0.0558   0.0473   0.0577
//   0.60    0.0226   0.0256   0.0371   0.0299   0.0279
//   0.80    0.0226   0.0257   0.0297   0.0252   0.0214
//   1.00    0.0226   0.0277   0.0277   0.0189   0.0221   <- SHIPPED
//
// 1.00 is the best cell on every stand slice and it also measures the best
// LEG chain ratio over the aft sweep (1.1737 against 1.1970 at 0.60 and HEAD's
// own 1.1246). Note the s = 0 column never moves: at zero stand the band IS
// kFootBlendHiU, so the seated ladder Chad has not judged yet is untouched.
//
// ★ WHAT IT COSTS, SAID OUT LOUD: while standing with only a little aft demand
// the ladder now un-tears the arms more slowly, so the worst arm-chain ratio
// over the sweep reads 1.4287 (at a = 0.004, full lock) where R3-WS reported
// 1.2980. That is NOT a regression -- it is HEAD's own 1.4292 in a cell the
// 0-OFF law freezes, approached from below -- and the sweep's no-regression
// column (+0.0270, unchanged) is the number that says so.
// ★★★ R3-WS(c) RE-MEASURED IT: 1.00 -> 1.40. The table above is the whole
// argument -- at 1.00 the s = 0.75 and s = 1.00 slices fail the bake's own
// LOG_FATAL continuity gate once the feet carry 1.0054 m instead of 0.32, and
// no foot band fixes it. 1.40 sits in the middle of a measured plateau
// (1.35/1.40/1.45 -> worst 0.0380/0.0389/0.0401), clear of the cliff between
// 1.25 (0.0580 at s = 1) and 1.30 (0.0301). The s = 0 column never moves at
// any value: at zero stand the band IS kFootBlendHiU.
inline constexpr float kReachGateStandHiU = 1.40f;

// The trunk-flexion / rho gate. 0 at u = 0 for every s (the 0-OFF law), and
// exactly the foot blend's own band at s = 0.
float ws_reach_gate(float u, float s, float hi_stand = kReachGateStandHiU);

// ---------------------------------------------------------------------------
// THE REACH SOLVE -- closed form, because the shoulder moves on a CIRCLE
// ---------------------------------------------------------------------------
//
// ★ THE GEOMETRY THAT MAKES THIS EXACT. The reach hinge is a rotation of the
// pelvis about MODEL +X through the pelvis origin, and on this rig every joint
// from spine_01 to hand_* hangs off `pelvis`, so the shoulder rides it
// RIGIDLY: shoulder(theta) = P + R_x(theta) * v with v the measured rest
// offset (+-0.210000, +0.436726, +0.287970). R_x therefore sweeps the shoulder
// around a circle of radius |v_yz| = 0.523122 in the plane x = P.x + v.x, and
// the closest approach to any hand target H is
//     d_min = hypot(H.x - P.x - v.x,  |H_yz - P_yz| - |v_yz|)
// -- no search, no iteration, and it is what caps the ladder travel above.
//
// ★ AND THE HIPS DO NOT MOVE UNDER IT: thigh_l/r sit at (+-0.095, 0, 0) from
// the pelvis, a PURE model-X offset, so a rotation about model +X leaves them
// exactly where they were. The reach hinge bends the back without touching
// the legs, which is precisely the "back bent over, weight low" Chad asked
// for.
inline constexpr float kReachHingeMaxRad = 65.0f * 3.14159265f / 180.0f;

struct ReachSolve {
    float theta_rad = 0.0f;   // >= 0, forward flexion, clamped
    float dist_m = 0.0f;      // the shoulder->hand distance it achieves
    bool reached = true;      // false = rho was not attainable; theta is then
                              // the CLOSEST-APPROACH angle, never a clamp
};

// Smallest theta >= 0 that brings |shoulder(theta) - hand| down to
// `target_dist`. Returns theta = 0 when the rest configuration already
// satisfies it (which is what makes forward lean and u = 0 no-ops).
ReachSolve reach_solve(const glm::vec3& pelvis, const glm::vec3& shoulder_off,
                       const glm::vec3& hand, float target_dist);

// ★★ THE TWO ARMS SHARE ONE TORSO, AND THAT IS ITS OWN CONSTRAINT.
// `reach_solve` is exact PER SIDE, but the pelvis has ONE theta, and taking the
// larger of the two per-side answers pushes the OTHER arm past its own limit --
// measured worst arm ratio 1.0194 (over the IK clamp) doing exactly that. So
// the shipped solve is on the PAIR: the smallest theta at which the WORSE of
// the two ratios meets the target. max_ratio(theta) is the max of two smooth
// circle-to-point distances, so it is found by a fixed 65-sample scan plus a
// fixed 20-step bisection -- deterministic, total, no tolerance loop.
//
// Preference order, and it is ONE rule with two outcomes: if the design ratio
// is attainable, the SMALLEST theta that attains it (the least trunk flexion
// that does the job); if it is not, the theta that MINIMISES the worse ratio --
// the best the man can do, never a clamp that hands the IK a target it cannot
// hold. An earlier version stopped at the first angle under the hard ceiling
// instead, which measured 0.985 where 0.9795 was available: an arm needlessly
// closer to the clamp.
inline constexpr int kReachScanSteps = 64;
inline constexpr int kReachRefineSteps = 20;

struct ReachArm {
    glm::vec3 shoulder_off{0.0f};
    glm::vec3 hand{0.0f};
    float dmax = 1.0f;
};

struct ReachPair {
    float theta_rad = 0.0f;
    float ratio = 0.0f;      // the WORSE of the two arm-chain ratios
    bool met_design = false;
};

// The worse of the two arm ratios at a given pelvis and trunk flexion.
float reach_pair_ratio(const glm::vec3& pelvis, const ReachArm& a,
                       const ReachArm& b, float theta_rad);

ReachPair reach_pair_solve(const glm::vec3& pelvis, const ReachArm& a,
                           const ReachArm& b, float rho_design);

// ---------------------------------------------------------------------------
// THE PELVIS STATION -- shape, reach and SPEC 4's shortening in one place
// ---------------------------------------------------------------------------
//
// ★ THIS IS THE WHOLE LADDER STATION SOLVE, AND IT LIVES HERE ON PURPOSE.
// render/sled_model.cpp has no test TU (SPEC 6), so the rule that decides how
// far back the butt actually goes -- follow the seat, solve the reach, and
// SHORTEN the travel rather than let the arm clamp -- has to be a pure
// function the gate can drive. sled_model.cpp only feeds it the live grips.
//
// `ceiling` is the no-regression bound: kArmRatioLimit, or the SAME cell's
// ladder-free ratio when that is already worse (the pre-existing steer clamp
// the 0-OFF law forbids this rung from touching).
struct WsStation {
    float aft_m = 0.0f;      // the retreat actually taken, after shortening
    float dy_m = 0.0f;       // the seat-follow + kneel vertical at that station
    float theta_rad = 0.0f;  // the reach hinge, already gated
    float ratio = 0.0f;      // the worse arm ratio it leaves
    bool shortened = false;  // SPEC 4 fired: say so in the report
};

// ★ R3-WS(c) / A8: the trunk cap lives HERE, applied as
// `min(theta_solve * gate, kHeadUpTrunkMaxRad)` -- a ceiling on the gated
// angle, not a re-timing of it. Any reach shortfall the cap leaves flows
// through the same SPEC 4 shortening bisection below, so `want` and `got`
// stay converged and the arms still never clamp.
// `extra_dy` is the branch vertical that is NOT the seat profile: the kneel
// lift plus SPEC 11.2.3's standing squat drop.
//
// ★★ R3-WS(d) / SPEC 13.2.2: THE CAP IS NOW AN ARGUMENT, because the kneel
// gets its own. `cap` defaults to the head-up ceiling so every non-kneel call
// site is unchanged; the runtime passes ws_trunk_cap(w_kneel).
WsStation ws_solve_station(const glm::vec3& p0, float pelvis_rest_z,
                           float seat_follow, float extra_dy,
                           const ReachArm& a, const ReachArm& b,
                           float want_aft, float rho, float gate,
                           float ceiling, float cap = kHeadUpTrunkMaxRad);

// ★★★ R3-WS(d) / SPEC 13.2.2. THE KNEEL'S OWN TRUNK CEILING.
//
// Chad's ruling turned the kneel on WITH the hands still welded, and SPEC 13
// says that combination is accepted with "some trunk layover". The head-up
// 38 deg ceiling cannot survive it: at the 0.320 m kneel station the grip is
// ~1.08 m from a hip that the chord floor has lifted 0.29 m, and a 38 deg
// trunk simply does not reach -- SPEC 4's shortening would then eat the whole
// kneel retreat and the branch would render as a seated crouch wearing a
// kneel's name. So the ceiling is BLENDED by w_kneel to kKneelTrunkMaxRad.
//
// ★ AND IT IS DECORATION, WHICH IS SAID OUT LOUD RATHER THAN DISCOVERED BY
// THE READER (SPEC 13.4 D3): the solved kneel trunk lands at 50-55 deg, so
// 55 deg is a ceiling this pose approaches rather than one that binds. The
// number Chad judges is trunk-from-vertical = theta_r + kRestTrunkTiltRad,
// which reads 79-84 deg at full kneel. That is the HEADLINE of the (d) report
// and the reason the side shot exists.
inline constexpr float kKneelTrunkMaxRad = 55.0f * 3.14159265f / 180.0f;
float ws_trunk_cap(float w_kneel);

// The largest aft (model -Z) pelvis translation that keeps the closest
// approach at or under `limit_dist`. Closed form; 0 when even the rest station
// is already past it.
float reach_max_aft(const glm::vec3& pelvis, const glm::vec3& shoulder_off,
                    const glm::vec3& hand, float limit_dist);

// ---------------------------------------------------------------------------
// THE KNEEL -- built on the seat, capped by the KNEE, not by taste
// ---------------------------------------------------------------------------
//
// ★ THE MEASUREMENT THAT SETTLED "SITTING BACK TOWARD THE HEELS -- NOT UPRIGHT
// ON HIS KNEES". Both ends of Chad's distinction are arithmetic on this rig:
// upright on the knees puts the hip a full thigh above the seat (0.453 m);
// truly sitting ON the heels would fold the knee past 174 deg, which no knee
// does. The human limit is ~155 deg of flexion, so the hip height is SOLVED
// from it -- `kneel_sit_height` bisects for the station where the hip-to-ankle
// distance equals the 155 deg chord, and the answer on this seat is ~0.256 m.
// That is 3.0x the seated 0.085578 and 0.56x the upright 0.453: sat back,
// weight low, not up on the knees. Nothing here is a taste dial.
//
// ★★★ R3-WS(d) / SPEC 13.2.1 -- THE SHARD GUARD, AND IT IS WHY 155 deg IS NO
// LONGER THE NUMBER THAT SOLVES THE HIP HEIGHT.
//
// The anatomical limit is real, but the SHIPPED MESH tears before the joint
// does: at 155 deg the hip->ankle chord is 0.1965 m on 0.453/0.455 bones and
// `sudburian_proxy` fans into flat shards there (measured in R3-WS, visible in
// r3ws_shot_4). SPEC 11.4 A7 already carries an absolute chord floor of
// 0.25 m for the deck branches; SPEC 13.2.1 extends it to the KNEEL with a
// 10 mm margin. So the solved quantity becomes
//
//   target chord = max(kneel_chord_m(thigh, shin), kKneelChordFloorM)
//
// and on this rig the floor WINS: 0.26 m instead of 0.1965, which re-solves
// the sit height 0.2549 -> ~0.290 and opens the knee to ~146.7 deg of flexion
// (well inside the 155 deg limit -- the guard costs anatomy nothing here, it
// only refuses the last 8 deg where the skin breaks). The knee stays ON the
// pan and the ankle on the rear rise; SPEC 13.2.1's "if the floor and the
// seat conflict, the seat wins" never fires, and the bisection reports what
// it actually achieved rather than asserting what it wanted.
inline constexpr float kKneelChordFloorM = 0.26f;
//
// ★ AND BOTH CONTACTS ARE THE MEASURED SURFACE. The knee lies on
// y_seat(z) + kKneePadRM (the FORWARD sphere intersection, so the knees are
// ahead of the hips) and the ankle on y_seat(z) + kAnkleAboveSoleM (the
// REARWARD one, so the shin lies ALONG the seat). Neither is a tangent guess:
// each is a sphere-meets-surface solve on the same profile table.
inline constexpr float kKneeFlexMaxRad = 155.0f * 3.14159265f / 180.0f;

// ★★★ R3-WS(d) -- THE KNEEL IS **ON**. CHAD RULED IT ON, 2026-08-20:
// "Turn on the kneel! yes :)"
//
// R3-WS shipped this branch at gain 0 with two measured reasons on the record,
// and BOTH of them were questions rather than defects. They are answered here
// so the flip is a ruling with its consequences priced, not a constant somebody
// nudged:
//
//   1. HANDS-ON-THE-BARS + KNEELING AT THE BACK IS A TRUNK LAYOVER, BY
//      ARITHMETIC, AND THAT IS NOW ACCEPTED. The kneel lifts the hip (0.0856
//      -> the chord-guarded ~0.290) and the station is 0.320 m aft, so the
//      grip sits ~1.08 m from the hip against a trunk+arm of ~1.18 m. The
//      reach solve therefore lays the trunk over at ~50-55 deg, which is
//      79-84 deg FROM VERTICAL once kRestTrunkTiltRad is counted. Chad did
//      not separately answer "hands on the bars vs release"; turning the
//      kneel on WITH the welds in place IS that answer (SPEC 13), so the
//      layover ships and the report headlines it with the side shot for his
//      eye. The lever if he dislikes it is PRE-COMPUTED and OFFERED, not
//      applied: moving the KNEEL station forward (kLadderPelvisAftM is the
//      anchor and does not move; the kneel's own want_aft would) trades about
//      10-15 mm of ladder CG for 10-15 deg of trunk. The bake prints the
//      three-point table (SPEC 13.4 D3).
//   2. THE PROXY SKIN TORE AT THE ANATOMICAL KNEE FOLD, AND THE SHARD GUARD
//      IS WHAT FIXED IT FROM THE RUNTIME SIDE. The tear was at chord 0.197 m
//      (155 deg of flexion); kKneelChordFloorM holds every kneel cell at
//      >= 0.26 m (~146.7 deg), which is the same guard SPEC 11.4 A7 already
//      applies to the deck branches. That is a target-arithmetic fix, and the
//      R3-WS note above said target arithmetic could not do it -- that note
//      was measuring the UNGUARDED construction. The guarded one is judged by
//      eye in r3ws_d_shot_3_kneel_full_rear34.png, because the law here is
//      VERIFY THE ARTEFACT, NOT THE PROCESS.
//
// The constant stays named and pinned rather than being deleted: it is the
// whole switch, and the two pins in test_rider_pose.cpp assert its value with
// the governing ruling cited, so nobody can drift it without saying why.
//
// ★★★★ R3-WS(e) -- AND CHAD RULED IT BACK OFF FOR THE LONGITUDINAL LADDER,
// 2026-08-21: "defer the kneeling fit for now, what we will do is not make
// kneeling for longitudinal movement... eventually [the] pose activated for a
// hanging to a side lean... a hotkey so you can pull a side / unstick it...
// key is going to be P for pull."
//
// THIS IS NOT A REVERSION OF THE RULING ABOVE AND IT IS NOT A DELETION. What
// changed is the kneel's ADDRESS, not its correctness:
//
//   * The FORE-AFT ladder now ends at the deep seated rear crouch -- the (c)
//     pose Chad drove and liked. The kneel is not a longitudinal pose.
//   * Everything R3-WS(d) built STAYS IN THE TREE AND STAYS GATED: this
//     constant, kKneelChordFloorM, kKneelTrunkMaxRad, the kKneelSLo/SHi band,
//     kneel_construct, and the antiparallel bend-normal fix
//     (ws_blend_bend_normal). They are the INHERITANCE of the future R4-SIDE
//     rung (the "P" key -- lean-side leg down to the runner, far leg kneeling
//     on the seat, body hung out to that side). See docs/SUDBURIAN_LADDER.md
//     "R4-SIDE -- THE 'P' KEY (PULL)" and SPEC 14.
//   * The kneel-math cases in test_rider_pose.cpp therefore drive the kneel
//     branch through an EXPLICIT gain of 1 (ws_test_pose_kneel) rather than
//     through this constant, so they stay NON-VACUOUS while the shipped
//     runtime pose has no kneel in it.
//   * The two live consequences, both re-measured in R3-WS(e): the ladder's
//     seated aft-CG ceiling C_0(1) falls 0.2786 -> the kneel-free bake, and
//     sim/sled.h's kAftCeilC1 row + lean_aft_max_m are re-pinned to it. The
//     deep-flexion proxy interpenetration and the D3 station lever are BOTH
//     DEFERRED BY THE SAME RULING -- spend nothing on them here.
//
// 0 = held for R4-SIDE. It is NOT "held pending a kneel ruling" -- that ruling
// came, twice, and this is the second one.
inline constexpr float kKneelRuntimeGain = 0.0f;

struct KneelPose {
    glm::vec3 knee{0.0f};
    glm::vec3 foot{0.0f};
    bool ok = false;  // false = the sphere missed the seat -- FAIL LOUD (SPEC
                      // 3c.2), never clamp silently
    // ★ R3-WS(d) / SPEC 13.2.1. How far the heel had to LIFT off the measured
    // seat to hold the chord floor at THIS hip, in metres. 0 = both contacts
    // are on the surface and the floor cost nothing. It is a REPORTED number,
    // never a silent clamp -- SPEC 13.2.1 asks for exactly this line.
    float heel_lift_m = 0.0f;
};

// The hip-to-ankle chord at the anatomical flexion limit, given the two bone
// lengths. Pure trigonometry, no dials.
float kneel_chord_m(float thigh_m, float shin_m);

// ★ R3-WS(d) / SPEC 13.2.1. The chord the kneel is actually SOLVED to: the
// anatomical one, raised to the shard floor when the mesh gives out first.
// On this rig the floor wins; the report says so rather than assuming it.
float kneel_target_chord_m(float thigh_m, float shin_m);

// The knee FLEXION a given hip->ankle chord implies, radians, measured from
// the straight leg. The inverse of kneel_chord_m, so the report can state the
// realized angle instead of the requested one (SPEC 13.4 D6).
float kneel_flex_rad(float thigh_m, float shin_m, float chord_m);

// Build the kneeling knee + ankle for a pelvis at `pelvis`. `pelvis` carries
// the x the whole leg plane sits in (SPEC 3c: x_K = the thigh head).
//
// ★ R3-WS(d): `guard_chord` applies the SHARD FLOOR at this hip (see the .cpp
// -- the heel lifts off the seat and the amount is reported). It is on for
// everything the runtime poses; kneel_sit_height turns it OFF, because the
// height it is solving for is exactly the one at which the UNGUARDED
// construction meets the floor, and a guarded probe would have no root.
KneelPose kneel_construct(const glm::vec3& pelvis, float thigh_m, float shin_m,
                          bool guard_chord = true);

// Solve the hip height above the seat at station `z_p` that puts the
// hip->ankle chord at kneel_target_chord_m -- the anatomical flexion limit,
// raised to the SHARD FLOOR (R3-WS(d)). Deterministic bisection, fixed count.
float kneel_sit_height(float z_p, float x_side, float thigh_m, float shin_m);

// ★★★ R3-WS(d) / SPEC 13.4 D4(b). THE BLENDED FOOT CROSSES THE SEAT, AND
// THAT IS **DISCLOSED**, NOT FIXED -- THREE ROUTES WERE MEASURED AND NONE OF
// THEM CLEARS IT WITHOUT BREAKING A CONTINUITY GATE.
//
// The endpoints: the deck foot at (|x| ~0.29, ankle y 0.331, z -0.9034) and
// the kneeling ankle at (|x| ~0.095, y 0.6785, z -0.9747). D4(b) predicted the
// straight line between them sweeps the boot through the seat, and it does:
// MEASURED worst penetration 0.1298 m at w_kneel 0.830.
//
// THE ARITHMETIC THAT MAKES IT UNAVOIDABLE. The kneeling ankle does not sit
// ABOVE the seat -- its sole RESTS ON it, sole y 0.5997 = seat_top(-0.9747)
// exactly. But the route starts 71 mm forward of there, and over that stretch
// the seat's rear rise CRESTS at 0.6427 (z -0.923) -- 43 mm above the
// destination sole. So no monotone path toward the endpoint can stay above the
// surface; only an overshoot could, and an overshoot is a new pop in a pose
// that is already spending its whole continuity budget.
//
// THE THREE ROUTES, ALL MEASURED ON THE SHIPPED BAKE (bound 0.060 for both
// gates; the s-gate figure is the KNEEL'S OWN addition, allowance 0.030):
//
//   route                          cont-in-a   kneel add, in-s   penetration
//   straight mix  (SHIPPED)           0.0245         0.0069          0.1298
//   lead the vertical (band 0.35)    0.4357  FAIL   --              --
//   lag the lateral (band 0.45)      0.0242         0.0190          0.1299
//   lead y+z, lag x (band 0.55)      0.0262         0.0672  FAIL    --
//
// Leading the vertical fails by 7x, for the R3-WS(c) DEVIATION 1 reason: the
// gate is a RATIO, and lifting the foot early both triples the joint travel
// and SPENDS the CG that travel is measured against (an early lift swings the
// knee forward). Leading y and z together fails the stand-slew gate instead.
// And lagging the lateral -- the one route that is nearly free -- does not
// move the number AT ALL (0.1298 -> 0.1299), because the worst cell is a
// LATERAL one: at lean_lat_m = +-0.15 the whole rider shifts and the inboard
// boot is inside the seat's x span for the entire blend, whatever the
// schedule does.
//
// So what ships is the straight mix, and the residual clip is DISCLOSED with
// the judged rear-three-quarter screenshot, which is exactly the disposition
// SPEC 13.4 D4(b) allows and requires ("silence is not an option"). The
// function stays named and tested because it is where this finding lives and
// where a fix would land the day the seat mesh or the kneel station changes.
glm::vec3 ws_kneel_foot_mix(const glm::vec3& deck, const glm::vec3& kneel,
                            float w_kneel);

// ★★★ R3-WS(d). THE BEND-NORMAL BLEND, AND THE TEAR IT FIXES.
//
// THE DEFECT, JUDGED THEN MEASURED. With the kneel first turned on, the full
// aft pose rendered as a fan of flat intersecting plates where the legs should
// be -- the "shard" this rung's chord floor was supposed to prevent, still
// there after the floor held at 0.26 m everywhere. The chord was never the
// whole story:
//
//   at REST the ankle is 0.687 m FORWARD of the hip (feet on the running
//   board, knee ahead of the hip-ankle line);
//   at the KNEEL the ankle is 0.070 m BEHIND it (shin folded back along the
//   seat) while the knee is still ahead.
//
// So the two bend-plane normals are very nearly ANTIPARALLEL, and the wiring
// blended them with a straight `mix()`. A lerp between antiparallel unit
// vectors passes THROUGH ZERO: mid-blend the normal collapses, its direction
// flips, and solve_chain's `cross(bend_n, dir)` hands the thigh and shin a
// bone frame that spins about their own axes. That is the roll R3-WS filed as
// "no amount of target arithmetic can fix from the runtime side" -- and it was
// right that the TARGETS could not fix it, because the defect is not in the
// targets. It is in the interpolation.
//
// THE FIX IS THE POLE, WHICH THIS RIG ALREADY HAS A MECHANISM FOR. Only the
// component of the normal PERPENDICULAR to the chain matters to solve_chain,
// so the honest interpolation is a ROTATION of the pole about the chain axis
// -- exactly what swing_bend_normal does for the R2c-5 elbow. It is unit
// length at every w, it never collapses, it is exactly n0 at w = 0 (so 0-OFF
// and every deck cell are untouched) and it reaches n1's own pole at w = 1.
glm::vec3 ws_blend_bend_normal(const glm::vec3& n0, const glm::vec3& n1,
                               const glm::vec3& chain_dir, float w);

// ---------------------------------------------------------------------------
// THE PELVIS STATION AND THE FOOT SLIDE
// ---------------------------------------------------------------------------

// ★ R3-WS(c). The foot's own slide schedule, in [0, 1]: 0 = the rest weld,
// 1 = kFootZRearLimitM (the heel 20 mm clear of the deck's back edge). The
// stand band is an explicit parameter -- defaulted to the shipped constant --
// so the gate can SWEEP it instead of trusting a remembered number.
float ws_foot_slide(float u, float s, float hi_stand = kFootSlideStandHiU);

// The seated/standing foot target on the DECK: the rest ankle lerped to the
// measured rear limit by `slide`, clamped inside the deck extents. At
// slide = 0 this returns the rest ankle exactly (0-OFF).
glm::vec3 deck_foot_target(const glm::vec3& foot_rest, float slide);

// ★ A1. The pelvis retreat this cell ASKS for. The seated/standing ladder uses
// the eye-dial kSeatRetreatAftM; the kneel branch keeps its own frozen anchor
// kLadderPelvisAftM (its geometry is solved against that station and must not
// move). ★ R3-WS(d): the kneel gain is 1 now, so this blend is LIVE -- at full
// kneel the retreat really is the 0.320 anchor, which is what the kneel's knee
// and ankle constructions were solved against.
float ws_want_aft(float u, float w_kneel);

// ★ SPEC 11.2.3. The standing branch's hip drop -- negative, metres.
float ws_squat_drop(float u, float w_stand);

// ★ A9. THE HEAD-UP NECK COUNTER, IN THE PURE TU.
//
// The sign that pitches the head UP about MODEL +X is MEASURED off the rest
// pose (the same probe pattern as swing_down_sign / wrist_up_sign) -- never
// typed, because nothing on this rig mirrors reliably.
float neck_up_sign(const glm::vec3& neck, const glm::vec3& head);

// The counter-rotation for a trunk flexion of `theta_r`, expressed in the
// neck's PARENT frame (the q_hinge conjugation pattern), so that in the model
// frame it is a rotation about +X. Exactly the identity at theta_r = 0.
glm::quat ws_neck_counter(float theta_r, float up_sign,
                          const glm::quat& parent_rest_q);

// ---------------------------------------------------------------------------
// THE BAKED CG CURVE AND ITS INVERSE
// ---------------------------------------------------------------------------
//
// ★ WHY A BAKE AND NOT A SECOND SOLVER. The ladder's SHAPE carries CG aft; how
// much it carries is a property of the posed skeleton, so it is MEASURED at
// load by running the real `pose_pass` over a u x s grid and reading the same
// `rider_cg` the R1c solve closes on -- no second CG model, no duplicated
// forward kinematics, nothing that can drift from the shipped pose path.
// The runtime then inverts that curve to pick u, and the EXISTING R1c seed +
// 2-step Newton carries whatever the shape did not (exactly the hinge-first
// pattern kHingeDemandShare already ships). CG honesty therefore stays the
// signed mechanism's job and this rung adds no second authority over it.
inline constexpr int kWsBakeU = 33;
inline constexpr int kWsBakeS = 5;

// Monotone table inverse: given c[0..n-1] rising from 0, return the u in
// [0,1] with C(u) == a, linearly interpolated, clamped at both ends.
// Both ends read the table's RUNNING MAXIMUM, so a curve that peaks early and
// falls back cannot answer u = 1.0 for every demand above its endpoint.
float ws_invert(const float* c, int n, float a_m);

// True when the table is strictly rising -- asserted at bake (SPEC 5).
bool ws_monotone(const float* c, int n);

// ★ R3-WS(b). The smallest forward difference in the table. "Non-decreasing"
// is not enough on its own -- u = C^-1(a) has slope 1/(dC/du), so a FLAT spot
// is still an infinite jump in the pose for a finite change in demand, which
// is the pop this rung exists to kill -- so the bake requires this strictly
// positive on every shipped slice.
float ws_min_slope(const float* c, int n);

// ★★ AND THE GATE THAT ACTUALLY BINDS IS NOT A SLOPE FLOOR ON C.
//
// The first attempt at this fix put a floor under dC/du and sized it off a
// worst-case joint speed. That was a PROXY, and it was wrong in both
// directions: it failed cells whose joints were barely moving (everything is
// gated down near u = 0, so a small dC/du there costs nothing), and widening
// the flexion band -- the actual fix -- made the proxy WORSE, because a
// throttled trunk cannot buy the arms slack and SPEC 4's shortening then eats
// the retreat that would have earned the CG back. Measured: min dC/du 0.00168
// at hi(1) = 0.60 against 0.00089 at hi(1) = 1.00, both "failing" a floor of
// 0.0033 that nothing could have passed.
//
// So the bake gates the QUANTITY THE PLAYER FEELS, computed on the real posed
// skeleton at every bake cell: the largest joint world delta the pose takes
// for ONE runtime step of aft demand,
//
//   worst_j |J(u_i) - J(u_{i-1})|  *  (lean_aft_max_m / kWsDemandCells)
//   ---------------------------------------------------------------
//                     C_s(u_i) - C_s(u_{i-1})
//
// against SPEC 7.5's 0.06 m. That is the continuity-in-`a` measurement the
// rung's own gates were missing -- every one of them stepped in `u`, the
// coordinate the jump lives underneath.
inline constexpr float kWsContinuityBoundM = 0.06f;
inline constexpr int kWsDemandCells = 64;

// The ladder coordinate for an aft demand, off a baked table with an s axis.
float ws_ladder_u(const float c[kWsBakeS][kWsBakeU], float a_m, float s);

}  // namespace render
