#pragma once
// ★★★ R4a RUNG 2b -- THE BODY CHAIN'S DRIVE POLICY, MOVED WHERE IT CAN FAIL.
//
// WHY THIS FILE EXISTS. Chad drove rung 2 and reported "the legs were going
// all over erratically, broken." Six gate cases were green. They were green
// because every one of them graded `body_blend_legs` against a STATIC chain
// handed to it as a fixture -- and the defect was not in the blend at all. It
// was in the three decisions that produced the chain:
//
//   * the pin/release branch          (`stage_arm <= 0.0f`)
//   * the return-to-pose target       (`pose_p[i] = pin[i]`)
//   * the stiffness rule              (`base / max(arm, 1e-3)`)
//
// All three lived in `render/sled_model.cpp`, which CMakeLists compiles ONLY
// into the `seads` executable. No test links that TU, so no test could execute
// one line of them. The rung's own banner called that block "gather / call /
// solve / settle, with no arithmetic in it" -- and it was not true: a branch on
// a stage weight, a choice of spring target and a stiffness law are the stage
// machine, not wiring.
//
// ★ THE LESSON, WHICH THIS PROGRAM HAS NOW PAID FOR THREE TIMES: a gate that
// cannot run is not a gate, and the way you find out is that the drive
// disagrees with the suite. So the policy moved here, into the library, and
// `sled_model.cpp` keeps only what is genuinely ungradable -- reading the rig.
//
// ★★★ THE DEFECT ITSELF: THE SPRING WAS AIMED AT A POSE THE KEEP-OUT FORBIDS.
// `render/body_chain.h` states the priming law in its own header -- the SEATED
// pose is illegal against the body chain's seat keep-out, measured, by design
// (pelvis 52.5 mm inside, thigh 38.6 mm), and the chain must be primed in the
// pose it ARMS in. The arming rung then made that same seated pin the
// permanent attractor of the return-to-pose spring. So the spring pulled him
// into the seat and the constraint pass threw him out, every substep, forever,
// and the Verlet integrator read each ejection as velocity. Measured on the
// shipped wiring: the ankle station left the drawn man by 0.49 m in ONE
// substep and peaked at 1.21 m, never settling.
//
// The fix is not a damper and not a clamp, and it is not a relocation either.
// The first cut PROJECTED the seated pin out of the solid and measured what
// that costs: the pelvis moves 115 mm, the worst leg station 73 mm -- a man
// sitting above his own machine. So instead the keep-out is told how deep the
// POSE already sits (`body_chain_seat_allowance`) and grades everything against
// that. At the pose the push is exactly zero, so the spring has nothing to
// fight; go deeper than the pose and it still pushes back -- to the pose, not
// out of the seat. Nobody is moved. Same probes, same frames, same escape.

#include <glm/vec3.hpp>

#include "render/trail_chain.h"
#include "sim/rider_grip.h"

namespace render {

// Measure how deep, per face, the POSE itself already sits in the seat, and
// write it into `pr.seat_allow` -- the allowance the constraint pass then
// subtracts. See `seat_escape_station` in render/trail_chain.cpp for the whole
// argument; the short version is that HE IS SITTING IN THE SEAT, so his seated
// pose is not a violation to resolve, and the spring must be free to pull him
// back to it without the keep-out throwing him out again.
//
// Uses the SAME probes, the SAME station frames and the SAME depth function the
// constraint pass runs. `n` is the SEGMENT count (so `n + 1` stations).
//
// ⚠ IT MUST BE RE-RUN EVERY FRAME THE POSE MOVES. The allowance describes ONE
// pose; carrying a stale one lets him sink into the seat by however far the
// pose has since climbed out of it. This program has paid seven times for a
// constant that stopped describing the thing it was measured from.
void body_chain_seat_allowance(TrailChainParams& pr, const TrailChainInput& in,
                               const glm::vec3* pose, int n,
                               const glm::vec3& width_axis);

// ★★★ THE ARMING MEMORY — Chad's ruling, 2026-08-29:
//
//   "build the arming memory, the buck has to be remembered[,] harder the buck
//    the longer the superman ... only big bucks sent him supermanning long
//    enough and landings hard enough to lose grip. I want it to happen maybe
//    10% of jumps given my normal driving ... it should be at the further end,
//    but it does happen[,] but is a little bit rare"
//
// WHY IT MUST EXIST, MEASURED (tools/sled_probe.cpp, `superman`, 24 airborne
// windows on tape 113 and 25 on tape 100): how much of his furthest excursion
// is still there at touchdown is FLAT against how hard he was bucked --
//
//     by buck      <20    20-60   60-200   200+
//       tape 113   0.361  0.831   0.670    0.770
//       tape 100   0.913  0.876   0.687    0.968
//
// -- and COLLAPSES MONOTONICALLY with time in the air, in both drives:
//
//     by air      <0.15s  0.15-.4  0.4-1s   1s+
//       tape 113   0.963  0.670    0.269    0.322
//       tape 100   0.946  0.870    0.234    0.223
//
// The spring returns him at a fixed rate REGARDLESS OF THE HIT, so past ~0.4 s
// the pull-back always wins and superman can never reach the landing. The
// pull-back has no memory of the buck. That is the whole defect.
//
// THE MECHANISM. One scalar, `mem`, that the buck charges and time discharges,
// dividing the spring's stiffness:
//
//     charge  = buck_gain * max(0, |g_eff| - g0)      <- the EXCESS over rest
//     mem     = max(mem - decay * dt, charge)          <- peak-hold, linear decay
//     stiff   = (base / arm) / (1 + mem)
//
// ★ EVERY PIECE OF THAT IS CONTINUOUS, AND THAT IS NOT A STYLE CHOICE. "not
// every time / if the bump is hard enough" is a MAGNITUDE DEPENDENCE, and this
// ladder's own recorded disease -- three separate rungs -- is a BINARY READ OF
// A CONTINUOUS QUANTITY (the release that cut him loose at arm = 0.0007). There
// is no threshold here and none is to be added: rarity must EMERGE from the
// distribution of his real bucks, exactly as the four pseudo-force terms were
// already shown to produce superman emergently.
//
// ★ WHY LINEAR DECAY AND NOT EXPONENTIAL. His words are "harder the buck the
// LONGER the superman". Under a linear discharge the time spent above any given
// softening is DIRECTLY PROPORTIONAL to the buck that charged it. Under an
// exponential it would go as the LOG of the buck -- a 10x harder hit would buy
// only a couple of extra time constants, which is not what he described.
//
// ★ WHY THE CHARGE IS REFERENCED TO g0 AND THAT IS NOT A SNEAKED-IN THRESHOLD.
// `|g_eff|` is ~1 g in steady riding by construction; the BUCK is the excess
// over the rest state, which is a physical datum, not a cutoff someone picked.
// In free fall the excess is 0, so the memory simply DECAYS through the flight
// -- which is precisely "he may freefall but during that he would pull himself
// back to the seat", and why a bigger charge carries him further into it.
//
// ★ ZERO GAIN IS EXACTLY TODAY'S BUILD. mem stays 0, the divisor is exactly
// 1.0f, and `stiff` is bit-identical to the shipped expression -- so this ships
// OFF and turns on as one dial, the pattern §7.7 mandates.
// ★★★ MOVED TO `sim/rider_grip.h` (R4a §7.7, 2026-08-30), AND RE-EXPORTED HERE
// UNDER ITS OLD NAMES. The kernel now runs this same arming memory -- Chad
// ruled that the kernel grows its own extension rather than reading the render
// chain, and `sim/` cannot include `render/`. So the law moved DOWN and this
// header aliases it: one implementation, three callers (the drawn body, the
// kernel, the probe), and every existing `render::body_buck_step` call site and
// test compiles unchanged against it.
//
// ⚠ THE MOVE IS VERBATIM AND MUST STAY THAT WAY. What Chad signed on 2026-08-30
// was measured through those exact float operations at gain 0.02 / decay 3.0;
// re-deriving them here in double, or "tidying" the discharge-before-peak-hold
// order, would move the shipped feel with nothing on screen to say so. The
// banner above `sim::BuckMemory` carries the whole argument.
using BodyBuckMemory = sim::BuckMemory;
using BodyBuckParams = sim::BuckParams;

inline float body_buck_step(BodyBuckMemory& m, const BodyBuckParams& p,
                            float g_eff_mag, float dt_s) {
    return sim::buck_step(m, p, g_eff_mag, dt_s);
}

struct BodyDriveIn {
    // The drawn man, this frame: station positions baked off the rig.
    const glm::vec3* pin = nullptr;
    int n = 0;  // segments; pin holds n + 1 stations

    // LADDER 7.3's stage weight, CONTINUOUS in [0, 1]. 0 = welded to his pose.
    float arm = 0.0f;

    // The spring's stiffness at full freedom. The rule is
    // `base_hz / max(arm, 1e-3)` -- as he is unweighted, the spring softens
    // and the chain is free to trail. NOT a switch: the arming rung's whole
    // ruling is that stages 0..3 are continuous and reversible.
    //
    // ⚠ READ THE RULE WITH ITS CLAMP OR YOU WILL MIS-AUDIT IT. This expression
    // alone says stiffness runs to 1000 Hz as arm -> 0; it cannot. The solver
    // clamps to `0.25 / (pi * dt)` = 9.55 Hz at the sim's 1/120 s
    // (render/trail_chain.cpp, "a dial that can explode is not a dial"). So
    // EVERY arm below ~0.1 lands in the same clamped band and is visually
    // indistinguishable from pinned; the whole visible range lives in
    // arm ~ [0.1, 1]. That is why a small arm excursion CANNOT produce a large
    // motion -- a claim this rung was audited against, from this header, and
    // the header did not carry the clamp.
    float pose_hz_base = 0.0f;

    // Everything the solver needs that is not policy. `anchor`, `pose_p` and
    // `n_pose` are OVERWRITTEN by the drive -- they are its output, not the
    // caller's input.
    TrailChainInput chain_in;

    int substeps = 1;
    float dt_s = 0.0f;

    // ★ THE ARMING MEMORY. Null = the mechanism is absent and the stiffness
    // expression is bit-identical to the pre-rung one. The state is the
    // CALLER'S because it must persist across frames; the policy is here.
    BodyBuckMemory* buck_mem = nullptr;
    BodyBuckParams buck;
    // |g_eff| this step -- the field the machine is actually pressing on him
    // with, which IS the buck when it spikes above rest.
    float g_eff_mag = 0.0f;
};

struct BodyDriveOut {
    bool pinned = false;         // took the pin branch this call
    float pose_stiff_hz = 0.0f;  // what the spring was actually run at
    // The deepest face-allowance the pose needed. Instrument, not a dial: if
    // this is ever 0 on a seated man the ruling is not in the build.
    float allow_max_m = 0.0f;
    // Instruments for the buck memory, so a drive or a probe can see WHY the
    // spring ran where it did rather than inferring it.
    float buck_divisor = 1.0f;  // what the stiffness was divided by
    float buck_mem = 0.0f;      // the charge left in the memory
};

// The whole policy: pin or release, measure the pose allowance, set the
// stiffness, step. `pr.pose_stiff_hz` and `st` are written; nothing else is.
BodyDriveOut body_chain_drive(TrailChainState& st, TrailChainParams& pr,
                              const BodyDriveIn& in);

}  // namespace render
