// S-globelook (v4 rung 3) — the freelook globe-inertia helper
// (render::OrbitInertia): the held orbit carries an angular velocity — instant
// grab while the mouse moves, exponential coast while held-and-still, capped,
// wall-absorbed on clamp contact, wiped on release. These legs pin the PURE
// law (the flight-audio pattern: main.cpp owns only the glue, which no ctest
// reaches — the green gate is blind to caller glue, so the helper carries the
// whole tested mechanism):
//   1. tau = 0 is the structural OFF arm: every method inert, nothing lingers;
//   2. instant grab: an input frame after a coast REPLACES the velocity with
//      the hand's rate (never blends — the DCC convention, memo trap #4);
//   3. coast decay: v halves every tau*ln2, and the position integrates it
//      (non-no-op fixture: v0 is REQUIRE'd past eps before any ratio);
//   4. cap: a seed above the cap clamps to the cap (both signs);
//   5. clamp contact: coasting into the pitch floor zeroes the pitch velocity
//      while yaw keeps coasting (per-axis wall absorb);
//   6. frame-rate consistency: two 8 ms coast frames == one 16 ms frame (the
//      exact-integral form is dt-correct by construction — pinned here so a
//      future Euler rewrite can't sneak frame-rate dependence back in);
//   7. EMA blend: two back-to-back grabs at different rates land the velocity
//      the exact EMA factor toward the second — NEITHER raw rate (kills a
//      frozen-EMA mutant and a replace-always mutant);
//   8. pend window: pend(dt) then grab(d, dt) seeds d/(2 dt) — the pending
//      frame's wall time counts (kills a deleted-pend mutant);
//   9. the P1 staircase: an integer-mouse slow drag (1 count per 4 frames) at
//      the shipped dials contributes ZERO coast motion with the stillness
//      dwell on, vs the pinned 3.67x amplification of the dwell-0
//      (pre-dwell) arm.

#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>

#include "render/camera.h"

#ifdef NDEBUG
#error \
    "SEADS gate requires an assert-live build (SPEC 6.1); configure with CMAKE_BUILD_TYPE=Debug"
#endif

using Catch::Approx;

namespace {

constexpr double kTau = 0.2;   // [s] the shipped coast decay
constexpr double kCap = 1.5;   // [rad/s] a test cap (~86 deg/s)
constexpr double kDt = 0.016;  // [s] a 60 fps frame

// Seed the helper's coast velocity via a fresh-gesture grab at a known rate
// (rate [rad/s] over one frame window), then one still frame is the caller's
// to take.
render::OrbitInertia seeded(double rate_yaw, double rate_pitch,
                            double cap = kCap) {
    render::OrbitInertia oi;
    oi.grab(rate_yaw * kDt, rate_pitch * kDt, kDt, kTau, cap);
    return oi;
}

}  // namespace

TEST_CASE("orbit inertia: tau 0 is the structural OFF arm - helper inert") {
    render::OrbitInertia oi;
    // grab with tau = 0 stores nothing.
    oi.grab(0.1, -0.1, kDt, 0.0, kCap);
    CHECK(oi.v_yaw == 0.0);
    CHECK(oi.v_pitch == 0.0);
    CHECK(oi.ema_yaw == 0.0);
    CHECK(oi.hand_on == false);
    // coast with tau = 0 emits zero delta ALWAYS — and wipes any velocity a
    // mid-flight tau retune might have left behind (nothing lingers).
    oi.v_yaw = 1.0;
    oi.v_pitch = -1.0;
    const auto d = oi.coast(kDt, 0.0, kCap);
    CHECK(d.yaw == 0.0);
    CHECK(d.pitch == 0.0);
    CHECK(oi.v_yaw == 0.0);
    CHECK(oi.v_pitch == 0.0);
}

TEST_CASE(
    "orbit inertia: instant grab replaces the coast velocity - no "
    "blend") {
    render::OrbitInertia oi = seeded(1.2, 0.0);  // below kCap: seed unclamped
    REQUIRE(oi.v_yaw == Approx(1.2));
    // Coast a frame: the hand left the globe; v decays off 1.2.
    (void)oi.coast(kDt, kTau, kCap);
    const double v_coasting = oi.v_yaw;
    REQUIRE(v_coasting > 1.0);  // non-no-op: a real coast velocity survives
    // New input while coasting: the hand's rate REPLACES the coast exactly —
    // a fresh gesture reseeds the EMA, so no trace of the ~1.1 coast (or
    // the old 1.2 estimate) blends in.
    const double hand_rate = 0.5;
    oi.grab(hand_rate * kDt, 0.0, kDt, kTau, kCap);
    CHECK(oi.v_yaw == Approx(hand_rate));
    CHECK(oi.v_yaw != Approx(v_coasting).epsilon(0.01));
}

TEST_CASE(
    "orbit inertia: coast velocity halves every tau ln2 and the "
    "position integrates it") {
    render::OrbitInertia oi = seeded(1.0, -0.4);
    const double v0 = oi.v_yaw;
    REQUIRE(v0 > 0.1);  // the fixture fires (never a 0/0 ratio)
    // Coast for exactly tau*ln2 of wall time in uneven steps.
    const double half_life = kTau * std::log(2.0);
    const double steps[] = {0.4, 0.25, 0.2, 0.15};  // fractions of half_life
    double integral = 0.0;
    for (double f : steps) integral += oi.coast(f * half_life, kTau, kCap).yaw;
    CHECK(oi.v_yaw == Approx(0.5 * v0).epsilon(1e-9));
    // The emitted position deltas telescope to the exact integral
    // v0 * tau * (1 - 1/2).
    CHECK(integral == Approx(v0 * kTau * 0.5).epsilon(1e-9));
    // Pitch rides the same law with its own sign.
    CHECK(oi.v_pitch == Approx(0.5 * -0.4).epsilon(1e-9));
}

TEST_CASE("orbit inertia: seed above the cap clamps to the cap") {
    const render::OrbitInertia hi = seeded(10.0 * kCap, -10.0 * kCap);
    CHECK(hi.v_yaw == kCap);
    CHECK(hi.v_pitch == -kCap);
    // A legal seed below the cap is untouched (the cap is a ceiling, not a
    // normalizer).
    const render::OrbitInertia lo = seeded(0.5 * kCap, 0.0);
    CHECK(lo.v_yaw == Approx(0.5 * kCap));
}

TEST_CASE(
    "orbit inertia: coasting into the pitch floor zeroes the pitch "
    "velocity - yaw keeps coasting") {
    // Downward pitch coast + a live yaw coast, driven through the CALLER's
    // clamp-detection shape (clamp changed the value -> wall absorb).
    render::OrbitInertia oi = seeded(1.0, -1.0);
    REQUIRE(oi.v_pitch < -0.1);  // non-no-op: a real coast aims at the floor
    const double pitch_floor = -0.5;
    const double pitch_max = 0.9;
    double pitch = pitch_floor + 0.001;  // one frame from the wall
    const auto d = oi.coast(kDt, kTau, kCap);
    const double pitch_next = pitch + d.pitch;
    pitch = std::clamp(pitch_next, pitch_floor, pitch_max);
    REQUIRE(pitch != pitch_next);  // the wall was actually hit
    oi.hit_pitch_clamp();
    CHECK(oi.v_pitch == 0.0);
    // The next still frame: pitch is dead, yaw still glides.
    const auto d2 = oi.coast(kDt, kTau, kCap);
    CHECK(d2.pitch == 0.0);
    CHECK(d2.yaw > 0.0);
}

TEST_CASE(
    "orbit inertia: back-to-back grabs blend by the EMA factor - "
    "neither raw rate") {
    // Red-team P2-1: two consecutive moving-hand frames at different rates.
    // The second grab must move the estimate toward its rate by EXACTLY
    // a = 1 - exp(-w/kRateEmaTau) and land on NEITHER raw rate — killing a
    // frozen-EMA mutant (ema never updates while hand_on: v stays r1) AND a
    // replace-always mutant (every grab reseeds: v jumps to r2).
    const double r1 = 1.0;
    const double r2 = 0.3;  // both below kCap — the clamp never masks the law
    render::OrbitInertia oi = seeded(r1, 0.0);
    REQUIRE(oi.hand_on);  // premise: the second grab is a CONTINUING gesture
    oi.grab(r2 * kDt, 0.0, kDt, kTau, kCap);
    const double a = 1.0 - std::exp(-kDt / render::OrbitInertia::kRateEmaTau);
    CHECK(oi.v_yaw == Approx(r1 + a * (r2 - r1)).epsilon(1e-12));
    CHECK(std::abs(oi.v_yaw - r1) > 0.05);  // moved off the first rate...
    CHECK(std::abs(oi.v_yaw - r2) > 0.05);  // ...but not onto the second
}

TEST_CASE(
    "orbit inertia: a pend frame widens the rate window - seed is delta "
    "over the TRUE window") {
    // Red-team P2-2: pend(kDt) then grab(d over kDt) must seed d/(2 kDt) —
    // the pending 0-tick frame's wall time counts toward the rate window
    // (kills a deleted-pend mutant, which would seed the 2x-hot
    // instantaneous d/kDt).
    render::OrbitInertia oi;
    oi.pend(kDt);
    const double d = 0.02;
    oi.grab(d, 0.0, kDt, kTau, kCap);
    CHECK(oi.v_yaw == Approx(d / (2.0 * kDt)).epsilon(1e-12));
}

TEST_CASE(
    "orbit inertia: integer-mouse staircase - the dwell keeps the coast "
    "out of a slow drag") {
    // The rung-3 red-team P1 counter-example, scripted at the SHIPPED dials
    // (tau 0.2 s, cap 90 deg/s, dwell 0.075 s, orbit sensitivity 0.20
    // deg/px — constants mirrored here; the loader leg pins the table). An
    // integer mouse in a SLOW drag emits 1 count every 4 frames at ~60 fps:
    // the count frame grabs, the 3 gap frames are still. PRE-DWELL
    // (dwell = 0) every gap frame coast-classified — it cleared hand_on and
    // dropped its wall time from the rate window, so each count reseeded v
    // to the INSTANTANEOUS step/kDt (4x the true hand rate) and then glided
    // on it. Measured amplification of the whole drag, pinned below as the
    // counter-example the dwell = 0 arm re-opens: 3.667x the commanded
    // motion (= 1 + tau*(1 - exp(-3 kDt/tau))/kDt; the red-team measured
    // ~3.7x at 1 count/4 frames, ~2x at 1 count/2, and a lone 1-px nudge
    // gliding 2.4 deg vs the 0.2 commanded). The hand was MOVING the whole
    // time — the mouse quantizer, not the hand, left the globe. WITH the
    // dwell the 48 ms gaps (< 75 ms) classify PEND: the coast contributes
    // ZERO over the drag and the seed is delta/true-window.
    constexpr double kPi = 3.14159265358979323846;
    constexpr double kDwell = 0.075;                    // shipped inertia_dwell
    constexpr double kCapShip = 90.0 * kPi / 180.0;     // shipped inertia_cap
    const double step = 0.20 * kPi / 180.0;  // 1 count in orbit radians
    const int kCycles = 25;
    const auto run_drag = [&](double dwell, render::OrbitInertia& oi) {
        double coast_sum = 0.0;
        for (int c = 0; c < kCycles; ++c) {
            oi.grab(step, 0.0, kDt, kTau, kCapShip);
            for (int f = 0; f < 3; ++f)
                coast_sum += oi.coast(kDt, kTau, kCapShip, dwell).yaw;
        }
        return coast_sum;
    };
    render::OrbitInertia on;
    render::OrbitInertia off;
    const double contrib_on = run_drag(kDwell, on);
    const double contrib_off = run_drag(0.0, off);
    const double commanded = kCycles * step;
    // Dwell ON: the coast NEVER engages during the drag (every gap < dwell)
    // — the applied motion is exactly the commanded staircase.
    CHECK(contrib_on == 0.0);
    // ...and the surviving estimate is the HONEST average hand rate
    // step/(4 kDt) (the EMA converges onto delta/true-window), not the 4x
    // instantaneous reseed.
    CHECK(on.v_yaw == Approx(step / (4.0 * kDt)).epsilon(1e-3));
    // Dwell 0 (the pre-dwell arm): the pinned counter-example — the same
    // drag gains ~2.7x the commanded motion from coast alone.
    REQUIRE(std::abs(contrib_off) > 0.0);  // the counter-example fires
    const double amp = (commanded + contrib_off) / commanded;
    CHECK(amp > 3.0);
    CHECK(amp == Approx(3.667).margin(0.01));
    // End of drag, dwell on: the hand stops after the last count. The last
    // gap already banked 48 ms of stillness, so ONE more still frame stays
    // PEND (64 ms < 75 ms, zero motion, hand still officially on), and the
    // frame after that (80 ms ~ the 5th still frame since the count — the
    // toml's "~4-5 frames" cost) ENTERS coast: the flick's glide begins at
    // the honest rate, with the rate window ZEROED on entry so this rest
    // never dilutes the NEXT gesture's instant-grab seed.
    const auto d4 = on.coast(kDt, kTau, kCapShip, kDwell);
    CHECK(d4.yaw == 0.0);
    CHECK(on.hand_on);
    const auto d5 = on.coast(kDt, kTau, kCapShip, kDwell);
    CHECK(d5.yaw > 0.0);
    CHECK_FALSE(on.hand_on);
    CHECK(on.window_dt == 0.0);
}

TEST_CASE("orbit inertia: two 8 ms coast frames match one 16 ms frame") {
    render::OrbitInertia one = seeded(1.2, -0.7);
    render::OrbitInertia two = one;
    const auto d_one = one.coast(0.016, kTau, kCap);
    const auto d2a = two.coast(0.008, kTau, kCap);
    const auto d2b = two.coast(0.008, kTau, kCap);
    // Exact-integral coasting: the emitted position delta AND the surviving
    // velocity are partition-independent (fp-tight, not a loose tolerance —
    // this leg exists to kill a v*dt Euler rewrite, which errs at ~dt/tau
    // ~ 4% here).
    CHECK(d2a.yaw + d2b.yaw == Approx(d_one.yaw).epsilon(1e-12));
    CHECK(d2a.pitch + d2b.pitch == Approx(d_one.pitch).epsilon(1e-12));
    CHECK(two.v_yaw == Approx(one.v_yaw).epsilon(1e-12));
    CHECK(two.v_pitch == Approx(one.v_pitch).epsilon(1e-12));
}
