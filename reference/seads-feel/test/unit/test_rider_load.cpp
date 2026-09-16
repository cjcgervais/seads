// ★★★ R4a THE ARMING RUNG -- THE STAGE SELECTOR GATE.
//
// docs/R4A_THROW_RULING_20260825.md §3 item 2: "`seat_load_frac` /
// `board_load_frac` are PROMOTED from diagnostics to the stage-1/2/3
// selector." docs/R4A_PHASE0_MODEL.md is the model. render/rider_load.* is the
// ONE implementation of it -- moved verbatim out of tools/sled_probe.cpp this
// rung so the shipped game and the instrument cannot fork.
//
// ★ WHAT THIS FILE DELIBERATELY DOES NOT DO: pin the asset's own numbers.
// The stations, the mass and the inertia come from indy650.glb and are
// re-measured by the probe on every run (`seads_sled_probe griphold` prints
// them and `asserted_legs` goes red on them). Re-typing them here would be THE
// LAW broken an eighth time -- a constant that describes the shipped table
// stops describing it the moment the table moves. So every leg below is either
//   * a PROPERTY that must hold for any admissible model (balance closes,
//     no support ever pulls down, the case split is exhaustive), swept over a
//     range rather than sampled at a point; or
//   * a statement about the ARM FORMULA itself, which is this rung's own new
//     ruling and has no asset in it.
//
// The fixture is a DECLARED test rod, close to the real geometry (the probe's
// own printed stations, 2026-08-27) so the sweeps land in the same regime --
// but nothing asserts the fixture's values, only what the model does with any
// of them.
//
// ASCII names only: a non-ASCII Catch2 name is silently never run in this repo.

#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <glm/glm.hpp>

#include "render/rider_load.h"

using Catch::Approx;
using render::kRiderBoardEdge;
using render::kRiderFree;
using render::kRiderLoadG;
using render::kRiderSeated;
using render::kRiderSeatEdge;
using render::rider_load_solve;
using render::rider_stage_arm;
using render::rider_air_free_frac;
using render::rider_support_release;
using render::rider_support_release_free;
using render::rider_stage_arm_free;
using render::RiderLoad;
using render::RiderLoadModel;

namespace {

// A DECLARED fixture rod, not the asset. Shaped like the measured Sudburian on
// the measured machine so the sweeps exercise the same branches, and stated as
// a fixture so nobody mistakes it for a re-typed measurement.
RiderLoadModel fixture() {
    RiderLoadModel M;
    M.m_r = 87.5;
    M.rider_frac = 87.5 / 331.0;
    M.I_pitch = 7.09;
    M.cg_body = glm::dvec3(0.0, 0.354, 0.357);
    M.grip_body = glm::dvec3(0.0, 0.323, -0.269);
    M.boot_body = glm::dvec3(0.0, -0.133, -0.102);
    M.seat_body = glm::dvec3(0.0, 0.220, 0.585);
    return M;
}

const glm::dvec3 kZero(0.0);
const glm::dvec3 kGLevel(0.0, -kRiderLoadG, 0.0);

RiderLoad at_ay(const RiderLoadModel& M, double ay_g) {
    return rider_load_solve(M, glm::dvec3(0.0, ay_g * kRiderLoadG, 0.0), kZero,
                            kZero, kGLevel, kZero);
}

float arm_at(const RiderLoadModel& M, double ay_g, double sref, double bref) {
    const RiderLoad L = at_ay(M, ay_g);
    return rider_stage_arm(L.seat_frac, L.board_frac, sref, bref);
}

}  // namespace

// ---- 1. The model closes. Any admissible solve must satisfy Newton on both
// axes AND the moment about the rider CG -- for every case, over a wide sweep.
// This is the postcondition tools/sled_probe.cpp's `admissible()` asserts on
// the real model; here it runs over a two-dimensional sweep so all four cases
// are visited.
TEST_CASE("rider load: force and moment balance close in every case") {
    const RiderLoadModel M = fixture();
    int seen[4] = {0, 0, 0, 0};
    double worst_f = 0.0, worst_m = 0.0, worst_neg = 0.0;
    for (int i = -40; i <= 40; ++i) {
        for (int j = -30; j <= 30; ++j) {
            const double ay = 0.1 * static_cast<double>(i) * kRiderLoadG;
            const double alx = 0.5 * static_cast<double>(j);
            const glm::dvec3 alpha(alx, 0.0, 0.0);
            const RiderLoad L = rider_load_solve(M, glm::dvec3(0.0, ay, 0.0),
                                                 kZero, alpha, kGLevel, kZero);
            ++seen[L.rcase];
            // vertical: N_s + N_b + H_y == m_r * A.y
            const double v = M.m_r * L.A.y;
            worst_f = std::max(worst_f, std::abs(L.N_s + L.N_b + L.H_y - v));
            // moment about the rider CG. The solve's own identity (see its
            // CASE-F comment):  K + dz_h*H_y + z_m*N_m - M_h == 0  with
            // K = I_pitch*alpha_x - dy_h*H_z, and M_h non-zero only in CASE F.
            const double dz_h = M.grip_body.z - M.cg_body.z;
            const double dy_h = M.grip_body.y - M.cg_body.y;
            const double zn = L.z_m_defined ? L.z_m * L.N_m : 0.0;
            worst_m =
                std::max(worst_m, std::abs(M.I_pitch * alpha.x - dy_h * L.H_z +
                                           dz_h * L.H_y + zn - L.M_h));
            // no support ever PULLS HIM DOWN onto the machine
            worst_neg = std::min(worst_neg, std::min(L.N_s, L.N_b));
        }
    }
    INFO("worst force residual " << worst_f << " N, moment " << worst_m
                                 << " N m, most negative support "
                                 << worst_neg);
    CHECK(worst_f < 1.0e-6);
    CHECK(worst_m < 1.0e-6);
    CHECK(worst_neg >= 0.0);
    // NON-VACUITY: a sweep that only ever visits CASE S proves nothing about
    // the other three. All four must appear or this leg is a fixture bug.
    CHECK(seen[kRiderSeated] > 0);
    CHECK(seen[kRiderSeatEdge] > 0);
    CHECK(seen[kRiderBoardEdge] > 0);
    CHECK(seen[kRiderFree] > 0);
}

// ---- 2. ★★★ THE LIVENESS LADDER. The corpus measurement for this rung is a
// TABLE OF ZEROS -- 0.00 % arm occupancy on every replayable tape -- and a
// table of zeros is what a DEAD selector and an unexercised one both look
// like. This is the leg that tells them apart, and it is the same ladder
// `seads_sled_probe stagesel` prints before it touches a tape.
TEST_CASE("rider load: the arming weight is alive, and free fall arms it") {
    const RiderLoadModel M = fixture();
    const RiderLoad rest = at_ay(M, 0.0);
    REQUIRE(rest.rcase == kRiderSeated);
    const double sref = rest.seat_frac;
    const double bref = rest.board_frac;
    REQUIRE(sref > 1.0e-3);
    REQUIRE(bref > 1.0e-3);

    // seated, and every LOAD case above it, is stage 0.
    CHECK(arm_at(M, 0.0, sref, bref) == Approx(0.0f));
    CHECK(arm_at(M, +1.0, sref, bref) == Approx(0.0f));
    CHECK(arm_at(M, +3.0, sref, bref) == Approx(0.0f));
    // FREE FALL is stage 3, exactly.
    CHECK(arm_at(M, -1.0, sref, bref) == Approx(1.0f));
    // ... and so is anything beyond it (the machine dropping away faster
    // than gravity cannot un-arm him).
    CHECK(arm_at(M, -2.0, sref, bref) == Approx(1.0f));
    // MONOTONE and CONTINUOUS between: SUDBURIAN_LADDER §7.3's "stages 0-3 are
    // continuous and reversible". No threshold, no step.
    float prev = 0.0f, biggest_step = 0.0f;
    for (int k = 0; k <= 100; ++k) {
        const float a = arm_at(M, -0.01 * static_cast<double>(k), sref, bref);
        CHECK(a >= prev - 1.0e-6f);
        if (k > 0) biggest_step = std::max(biggest_step, a - prev);
        prev = a;
    }
    CHECK(prev == Approx(1.0f));
    INFO("biggest single 0.01 g step in the ladder: " << biggest_step);
    CHECK(biggest_step < 0.05f);
    // REVERSIBLE: it is a pure function of the loads, so coming back down the
    // same axis returns the same numbers.
    CHECK(arm_at(M, -0.5, sref, bref) == Approx(arm_at(M, -0.5, sref, bref)));
    CHECK(arm_at(M, 0.0, sref, bref) == Approx(0.0f));
}

// ---- 3. ★★★ THE ARM IS A PRODUCT, AND THAT IS THE RULING. Stage 3 is
// SUPERMAN -- "nothing below carries". One support still carrying its resting
// share is NOT superman however free the other one is, so the weight must be
// the PRODUCT of the two releases and not a sum, a mean, a min or a max.
// This leg is what a mutation to any of those dies on.
TEST_CASE("rider load: one support still carrying is not superman") {
    // seat fully released, boards still at their full resting share
    CHECK(rider_stage_arm(0.0, 0.33, 0.67, 0.33) == Approx(0.0f));
    // boards fully released, seat still at its full resting share
    CHECK(rider_stage_arm(0.67, 0.0, 0.67, 0.33) == Approx(0.0f));
    // both half released -> a quarter, which no min/max/mean gives
    CHECK(rider_stage_arm(0.335, 0.165, 0.67, 0.33) == Approx(0.25f));
    // both gone -> 1
    CHECK(rider_stage_arm(0.0, 0.0, 0.67, 0.33) == Approx(1.0f));
    // over-loaded (a landing) clamps at 0, never negative
    CHECK(rider_stage_arm(4.0, 2.0, 0.67, 0.33) == Approx(0.0f));
    // a degenerate reference refuses to arm rather than dividing by ~0
    CHECK(rider_stage_arm(0.0, 0.0, 0.0, 0.33) == Approx(0.0f));
    CHECK(rider_stage_arm(0.0, 0.0, 0.67, 0.0) == Approx(0.0f));
}

// ---- 3b. ★★★ THE CONTACT TERM -- CHAD'S RULING, 2026-08-28, and the gate
// is the measured table from his two drives, not an opinion about it.
//
// The ratio above is a FRICTIONLESS PLANAR model and cannot tell "leaned over
// on snow" from "thrown into the air". Measured on 9079 unforced frames of his
// own riding: on the ground at 15-45 deg of lean -- ordinary banked trail, the
// track visibly compressed -- it released on 42.2 % of frames; and airborne
// INVERTED, where both references degenerate, it PINNED on 40.8 %. Every case
// below is one row of that table.
//
// ⚠ EACH CHECK MUST BE ABLE TO FAIL ON ITS OWN. Deleting the free_frac factor
// kills the ground rows; deleting the degenerate branch kills the inverted-air
// row; reverting to `rider_stage_arm` kills both. The airborne-upright row is
// the one that must NOT move -- two absolute-loss reformulations were replayed
// over the whole log and both cured the ground rows by destroying this one.
TEST_CASE("rider load: the contact term separates leaning from being thrown") {
    const double sref = 0.67, bref = 0.33;

    // -- ON THE GROUND (free_frac 0): it cannot arm, at ANY attitude.
    // leaned far enough that the live load has saturated to zero -- the exact
    // reading that produced the 42.2 %.
    CHECK(rider_stage_arm_free(0.0, 0.0, 0.37, 0.035, 0.0) == Approx(0.0f));
    // and the same instant WITHOUT the term still arms fully, so this case is
    // measuring the term and not the fixture.
    CHECK(rider_stage_arm(0.0, 0.0, 0.37, 0.035) == Approx(1.0f));
    // tipped past 90, references degenerate, still on the ground: HIS OWN
    // STAND RULING (R4A_THROW_RULING 5.6) -- he gets up, he does not fly.
    CHECK(rider_stage_arm_free(0.0, 0.0, 0.0, 0.0, 0.0) == Approx(0.0f));
    // seated and upright on the ground: unchanged, and still zero.
    CHECK(rider_stage_arm_free(sref, bref, sref, bref, 0.0) == Approx(0.0f));

    // -- IN THE AIR (free_frac 1): the shipped ratio is passed through
    // BIT-FOR-BIT. This is the row the arithmetic rewrites destroyed.
    CHECK(rider_stage_arm_free(0.0, 0.0, sref, bref, 1.0) ==
          Approx(rider_stage_arm(0.0, 0.0, sref, bref)));
    CHECK(rider_stage_arm_free(0.335, 0.165, sref, bref, 1.0) ==
          Approx(rider_stage_arm(0.335, 0.165, sref, bref)));
    CHECK(rider_stage_arm_free(0.0, 0.0, sref, bref, 1.0) == Approx(1.0f));

    // -- IN THE AIR, INVERTED: the pin, cured. A degenerate reference means
    // "no support is possible at this attitude", which off the ground is total
    // freedom. The old law returned 0 here and welded him to his seated pose.
    CHECK(rider_stage_arm_free(0.0, 0.0, 0.0, 0.0, 1.0) == Approx(1.0f));
    CHECK(rider_stage_arm(0.0, 0.0, 0.0, 0.0) == Approx(0.0f));  // what it was

    // -- LINEAR IN free_frac (LADDER 7.3: stages 0-3 continuous, reversible).
    for (int k = 0; k <= 10; ++k) {
        const double f = 0.1 * static_cast<double>(k);
        CHECK(rider_stage_arm_free(0.0, 0.0, sref, bref, f) ==
              Approx(static_cast<float>(f)));
    }

    // -- ★ THE DEGENERATE EDGE, WITH A LIVE LOAD ON IT. This is the input the
    // first cut of this gate did not have, and without it the "continuous"
    // claim was pinned only at frac = 0 -- the one fixture where both branches
    // agree by construction, so the assert could not fail. The FIXTURE-NO-OP
    // CLASS, third appearance on this rung.
    //
    // Approaching a degenerate reference while a load SURVIVES, the ratio
    // reads 0 (he is still carried) and the branch then reads 1 (no support is
    // possible). Those are different statements and the step between them is
    // real. This case does not forbid it -- it PINS it, so that if anyone
    // later smooths the edge, or widens the guard, the gate says so out loud.
    CHECK(rider_stage_arm_free(0.05, 0.02, 2.0e-6, 2.0e-6, 1.0) ==
          Approx(0.0f));  // ratio side: still carried, still welded
    CHECK(rider_stage_arm_free(0.05, 0.02, 5.0e-7, 5.0e-7, 1.0) ==
          Approx(1.0f));  // branch side: no support possible at all
    // and with no load surviving, both sides agree -- which is why the
    // measured corpus never saw the step (9079 frames, zero such frames).
    CHECK(rider_stage_arm_free(0.0, 0.0, 2.0e-6, 2.0e-6, 1.0) == Approx(1.0f));
    CHECK(rider_stage_arm_free(0.0, 0.0, 5.0e-7, 5.0e-7, 1.0) == Approx(1.0f));
    // free_frac is clamped, not trusted: over-range and negative both behave.
    CHECK(rider_stage_arm_free(0.0, 0.0, sref, bref, 7.0) == Approx(1.0f));
    CHECK(rider_stage_arm_free(0.0, 0.0, sref, bref, -3.0) == Approx(0.0f));
}

// ★★★ THE BLEND WEIGHT, UNDER THE SAME RULING. This is the weight the DRAWN
// body follows the chain by, and it carried both halves of the stage weight's
// defect after the stage weight was cured -- so the body could be RELEASED and
// DRAWN AS SEATED at the same instant. Chad drove exactly that and described
// it without knowing the mechanism: "legs moving flickery erratic a lot of the
// time ... the orange frames doing all the flying."
//
// ⚠ THE MEASURED FACT THIS CASE EXISTS TO KEEP: on the 485 frames of his drive
// spent INVERTED AND AIRBORNE, the drawn legs followed the chain on ZERO of
// them, because the degenerate guard reads "no support is possible here" as
// "he has lost nothing". That is the missing superman, and it is one sign.
TEST_CASE("rider load: the blend follows the chain when nothing is holding him") {
    const double bref = 0.33;

    // -- ON THE GROUND: no follow, at any attitude. This is the flicker cure;
    // ungated, this weight crossed the visible band 1.97 times a SECOND.
    CHECK(rider_support_release_free(0.0, bref, 0.0) == Approx(0.0));
    CHECK(rider_support_release_free(0.0, 0.0, 0.0) == Approx(0.0));
    // and ungated the SAME instant fully releases -- so this measures the
    // term and not the fixture.
    CHECK(rider_support_release(0.0, bref) == Approx(1.0));

    // -- IN THE AIR, references healthy: the shipped release, passed through.
    CHECK(rider_support_release_free(0.0, bref, 1.0) ==
          Approx(rider_support_release(0.0, bref)));
    CHECK(rider_support_release_free(0.165, bref, 1.0) == Approx(0.5));
    CHECK(rider_support_release_free(bref, bref, 1.0) == Approx(0.0));

    // -- ★ IN THE AIR, INVERTED: the missing superman. A degenerate reference
    // means no support is possible, which off the ground is TOTAL release.
    // The old law returned 0 here and drew him seated while the chain flew.
    CHECK(rider_support_release_free(0.0, 0.0, 1.0) == Approx(1.0));
    CHECK(rider_support_release(0.0, 0.0) == Approx(0.0));  // what it was

    // -- and it scales with the window, so the boots do not pop off the
    // boards the instant a ski leaves the snow (LADDER 7.3: continuous).
    for (int k = 0; k <= 10; ++k) {
        const double f = 0.1 * static_cast<double>(k);
        CHECK(rider_support_release_free(0.0, bref, f) == Approx(f));
        CHECK(rider_support_release_free(0.0, 0.0, f) == Approx(f));
    }
    CHECK(rider_support_release_free(0.0, bref, -2.0) == Approx(0.0));
    CHECK(rider_support_release_free(0.0, bref, 9.0) == Approx(1.0));
}

// ★★ THE WINDOW, AND IT IS GATED HERE BECAUSE IT COULD NOT BE GATED WHERE IT
// WAS. These four lines of policy shipped inside render/sled_model.cpp, the TU
// CMake compiles only into the executable -- the disease render/body_drive.h
// exists to name, walked into on the rung that cured it. They are worth a case
// of their own because they can kill the entire stage machine in silence:
// `grace_s` is SHARED with the roll readout, whose documented off-config is
// "grace huge", and huge grace means the rider's legs never trail again.
TEST_CASE("rider load: the air window is a ramp, and no window is not no freedom") {
    // the shipped window: a ramp to full freedom over rolled_grace_s.
    CHECK(rider_air_free_frac(0.00, 0.20) == Approx(0.0));
    CHECK(rider_air_free_frac(0.05, 0.20) == Approx(0.25));
    CHECK(rider_air_free_frac(0.10, 0.20) == Approx(0.5));
    CHECK(rider_air_free_frac(0.20, 0.20) == Approx(1.0));
    CHECK(rider_air_free_frac(2.79, 0.20) == Approx(1.0));  // his longest send
    // touching the ground is zero freedom at any window.
    CHECK(rider_air_free_frac(0.0, 1.0e6) == Approx(0.0));
    // ★ A NON-POSITIVE WINDOW IS NOT ZERO FREEDOM. "There is no window" and
    // "he is never free" are different statements; only one is true, and the
    // first cut of this policy shipped the false one.
    CHECK(rider_air_free_frac(0.5, 0.0) == Approx(1.0));
    CHECK(rider_air_free_frac(0.5, -1.0) == Approx(1.0));
    CHECK(rider_air_free_frac(0.0, 0.0) == Approx(0.0));
    // negative air time is not negative freedom.
    CHECK(rider_air_free_frac(-1.0, 0.20) == Approx(0.0));
}

// ---- 4. CASE F IS THE TOP OF THE LADDER, and the two agree. The weight
// reaching 1 and the enumeration reporting FREE (or the seat edge at exactly
// zero load) are two independent statements about the same instant; if they
// ever disagree the stage machine and the instrument have forked.
TEST_CASE("rider load: the weight reaches one exactly where nothing carries") {
    const RiderLoadModel M = fixture();
    const RiderLoad rest = at_ay(M, 0.0);
    const double sref = rest.seat_frac, bref = rest.board_frac;
    int n_one = 0, n_carry = 0;
    for (int k = -30; k <= 10; ++k) {
        const double ay = 0.1 * static_cast<double>(k);
        const RiderLoad L = at_ay(M, ay);
        const float a = rider_stage_arm(L.seat_frac, L.board_frac, sref, bref);
        const bool carries = (L.N_s + L.N_b) > 1.0e-9;
        if (a >= 1.0f) {
            ++n_one;
            CHECK_FALSE(carries);
        }
        if (carries) ++n_carry;
    }
    CHECK(n_one > 0);
    CHECK(n_carry > 0);
}

// ---- 7. ★★★ THE CREST, AND THE MUTANT THAT WALKED THROUGH EVERYTHING ELSE.
// The R4a Phase-0 fix replaced a `V <= 0` CASE-F test with `W_supp < 0`: the
// sign of the resultant VERTICAL demand is only a PROXY for "nothing below
// carries", and reading it discards every real solution where the hands haul
// DOWN on the bar while the seat presses up -- every crest and every
// fall-away. It under-reported the grip by up to 572 N and dumped the balance
// into the wrist couple.
//
// ★★★ AND A MUTATION BACK TO THAT WRONG RULE PASSED CASES 1-6. It had to:
// CASE F is a self-CONSISTENT solution (balance closes, no support pulls down,
// and CASE A is still reached from the V > 0 branch so even the non-vacuity
// count stays green). Only a leg that knows WHICH branch is correct can see
// it -- and it is not academic, because under the wrong rule the arming weight
// jumps to a full superman on every crest.
//
// This is tools/sled_probe.cpp's asserted leg (iv) brought into the gate: with
// W_supp >= 0 and V <= 0, SOMETHING BELOW MUST STILL CARRY.
TEST_CASE("rider load: on a crest a contact stays pinned and the arm stays 0") {
    const RiderLoadModel M = fixture();
    const RiderLoad rest = at_ay(M, 0.0);
    const double sref = rest.seat_frac, bref = rest.board_frac;
    int n_crest = 0, n_free_edge = 0;
    double worst_arm = 0.0;
    // alpha_x < 0 pitches so that W_supp > 0 across the V = 0 crossing (the
    // seat carries and the hands haul DOWN); alpha_x > 0 flips it.
    for (int sgn = -1; sgn <= 1; sgn += 2) {
        for (int i = -30; i <= 10; ++i) {
            const double ay = 0.05 * static_cast<double>(i) * kRiderLoadG;
            const RiderLoad L = rider_load_solve(
                M, glm::dvec3(0.0, ay, 0.0), kZero,
                glm::dvec3(10.0 * static_cast<double>(sgn), 0.0, 0.0), kGLevel,
                kZero);
            if (L.V_dem > 0.0) continue;
            if (L.W_supp >= 0.0) {
                ++n_crest;
                // a contact MUST still be pinned ...
                CHECK(L.rcase != kRiderFree);
                CHECK((L.N_s + L.N_b) >= 0.0);
                // ... and therefore the man is NOT supermanning.
                const float a =
                    rider_stage_arm(L.seat_frac, L.board_frac, sref, bref);
                worst_arm = std::max(worst_arm, static_cast<double>(a));
                CHECK(a < 1.0f);
            } else {
                ++n_free_edge;
                CHECK(L.rcase == kRiderFree);
            }
        }
    }
    // NON-VACUITY, both ways: the sweep must actually straddle the V = 0
    // crossing on BOTH sides of the W_supp sign, or it proves nothing.
    INFO("crest samples " << n_crest << ", free-edge samples " << n_free_edge
                          << ", worst arm on a crest " << worst_arm);
    CHECK(n_crest > 0);
    CHECK(n_free_edge > 0);
}

// ---- 8. ★★★ ROLLING THE MACHINE IS NOT SUPERMAN -- CHAD'S FIRST DRIVE.
//
// He drove the chain and reported it arming "even if tipped over". Measured
// against the FROZEN at-rest upright reference, with NO acceleration anywhere:
// 45 deg of roll read arm 0.086, 60 deg read 0.250, 90 deg read a full 1.000.
//
// The cause is this model's own documented blindness: the supports are
// FRICTIONLESS and carry only along body +Y, so tipping swings that axis off
// vertical and both loads fall as cos(roll). The model is RIGHT -- a
// frictionless seat cannot hold a man sideways -- and it is the wrong
// question. A man lying on his tipped-over machine is not supermanning; by
// Chad's own ruling (R4A_THROW_RULING §5.6) that is the STAND self-right
// branch.
//
// The fix adds no constant: the reference is the SAME solve at the SAME
// attitude with the ACCELERATION ZEROED, so the weight measures support LOST
// TO ACCELERATION and not which way down points. This case pins both halves --
// roll alone must never arm, and a real unweighting at that same roll still
// must.
TEST_CASE("rider load: rolling the machine is not superman") {
    const RiderLoadModel M = fixture();
    int n_rolled = 0, n_armed_at_roll = 0;
    double worst_static = 0.0;
    for (int k = 0; k <= 12; ++k) {
        const double roll =
            static_cast<double>(k) * 15.0 * 3.14159265358979 / 180.0;
        const glm::dvec3 gb(std::sin(roll) * kRiderLoadG,
                            -std::cos(roll) * kRiderLoadG, 0.0);
        // the LIVE reference: same attitude, same lean, zero acceleration
        const RiderLoad ref =
            rider_load_solve(M, kZero, kZero, kZero, gb, kZero);
        // (a) AT REST at this roll, the live solve IS the reference, so there
        //     is nothing lost and the arm must be exactly 0.
        const RiderLoad at_rest =
            rider_load_solve(M, kZero, kZero, kZero, gb, kZero);
        const float a_static =
            rider_stage_arm(at_rest.seat_frac, at_rest.board_frac,
                            ref.seat_frac, ref.board_frac);
        worst_static = std::max(worst_static, static_cast<double>(a_static));
        CHECK(a_static == Approx(0.0f));
        if (k > 0) ++n_rolled;
        // (b) ... and a REAL unweighting at the same roll must still arm,
        //     or the fix has simply deafened the selector. FREE FALL is the
        //     machine accelerating ALONG gravity, a_body == g_body, so the
        //     demand A = a_G - g_body is exactly zero and nothing can carry.
        const RiderLoad falling =
            rider_load_solve(M, gb, kZero, kZero, gb, kZero);
        const float a_fall =
            rider_stage_arm(falling.seat_frac, falling.board_frac,
                            ref.seat_frac, ref.board_frac);
        if (a_fall >= 0.99f) ++n_armed_at_roll;
    }
    INFO("worst STATIC arm over the roll sweep " << worst_static
                                                 << ", rolls that still arm in"
                                                    " free fall "
                                                 << n_armed_at_roll << "/"
                                                 << (n_rolled + 1));
    CHECK(worst_static == Approx(0.0));
    // NON-VACUITY: the sweep must actually roll, and the selector must still
    // be alive at rolls where the machine is upright enough to have support.
    CHECK(n_rolled == 12);
    CHECK(n_armed_at_roll >= 6);
}

// ---- 5. The house low-pass is the house low-pass. R4A_PHASE0_MODEL.md §5.3:
// tau = 0.1 s, and h is the SUBSTEP, never dt. The named mutation for it is
// "use dt instead of h", and this leg is what that dies on.
TEST_CASE("rider load: the low pass is the house filter at tau 0.1 s") {
    const double h = 1.0 / 120.0;
    double x = 0.0;
    // one tau of substeps must land on 1 - 1/e
    const int n = static_cast<int>(0.1 / h + 0.5);
    for (int i = 0; i < n; ++i) x = render::rider_load_lp_step(x, 1.0, h);
    CHECK(x == Approx(1.0 - std::exp(-1.0)).margin(0.01));
    // it never overshoots, and it is a contraction toward the target
    double y = 0.0;
    for (int i = 0; i < 1000; ++i) {
        y = render::rider_load_lp_step(y, 1.0, h);
        CHECK(y <= 1.0);
    }
    CHECK(y == Approx(1.0).margin(1.0e-3));
    // a zero step moves nothing
    CHECK(render::rider_load_lp_step(0.4, 1.0, 0.0) == Approx(0.4));
}

// ---- 6. The frame map is the kernel's K2 map, and it is an involution on the
// two flipped axes. A silent sign flip here would mirror every station and
// every load with nothing red to say so.
TEST_CASE("rider load: the model to body map is sim sled K2") {
    const glm::dvec3 m(1.0, 2.0, 3.0);
    const glm::dvec3 b = render::rider_load_to_body(m, 0.464);
    CHECK(b.x == Approx(-1.0));
    CHECK(b.y == Approx(2.0 - 0.464));
    CHECK(b.z == Approx(-3.0));
    // applied twice with the drop undone it is the identity
    const glm::dvec3 back = render::rider_load_to_body(b, -0.464);
    CHECK(back.x == Approx(m.x));
    CHECK(back.y == Approx(m.y));
    CHECK(back.z == Approx(m.z));
}
