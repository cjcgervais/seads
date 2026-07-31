// MB-aim (SPEC §0, docs/mb_aim_plan.md): the rate-keyed mouse acceleration
// curve. The legality rails under test:
//  - memoryless pure gain shape (no state — pure function, nothing to pin);
//  - keyed on the frame-rate-NORMALIZED rate |delta|/frame_dt, never the raw
//    per-frame delta — pinned by the POWER-OF-TWO SCALE INVARIANCE leg (the
//    plan red-team P1-2 form: per-call `aim_curve(4d, 4dt) == 4*aim_curve(d,
//    dt)` bit-exact, because rounding commutes with 2^k scaling — NOT a
//    sums-over-frames construction, whose sequential += exactness claim is
//    false past 4 terms and would have grown a tolerance hide-band);
//  - knob-off arm (gain_max = 1) structurally bit-identical (P2-1);
//  - the quantization guard (P1-3): fps-scaled 1-2 px rate noise never
//    curves, so slow tracking stays in the precision zone at ANY fps.
//
// Shape probes follow the MB-rud band-placement discipline: sample INSIDE the
// band against the config-recomputed formula oracle AND at the boundary-exact
// edges — two outside-the-band samples would pass a flipped/shifted band
// bit-identically.

#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <cstdio>

#include "config/load_controller.h"
#include "input/aim_curve.h"

#ifdef NDEBUG
#error "SEADS gate requires an assert-live build (SPEC 6.1)"
#endif

namespace {

// Hand-built params (retune-immune; the loaded toml's ranges are pinned in
// test_load_controller). Band 400..2400 px/s so in-band t values are exact
// binary fractions. Defaults of ControllerParams are the OFF arm (gain_max
// 1.0) — the helper turns the curve ON.
control::ControllerParams curve_params() {
    control::ControllerParams p;
    p.aim_curve_knee = 400.0;
    p.aim_curve_rate_hi = 2400.0;
    p.aim_curve_gain_max = 2.5;
    p.aim_curve_expo = 1.5;
    p.aim_curve_quant_px = 3.0;
    return p;
}

}  // namespace

TEST_CASE("aim_curve: shape - linear zone, knee edge, in-band oracle, cap") {
    const control::ControllerParams p = curve_params();
    const double dt = 0.25;  // exact; mag = rate*dt stays > quant_px here

    // Below the knee: pure linear (bit-identical passthrough).
    {
        const glm::dvec2 c = input::aim_curve(25.0, 0.0, dt, p);  // 100 px/s
        CHECK(c.x == 25.0);
        CHECK(c.y == 0.0);
    }
    // AT the knee, boundary-exact (rate == 400.0 exactly; <= keeps it linear —
    // the S4a "pin the predicate with an exactly representable magnitude").
    {
        const glm::dvec2 c = input::aim_curve(100.0, 0.0, dt, p);
        CHECK(c.x == 100.0);
    }
    // INSIDE the band against the recomputed formula oracle: rate 1400 px/s
    // => t = (1400-400)/(2400-400) = 0.5 exactly.
    {
        const double dx = 350.0;  // 1400 px/s at dt = 0.25
        const glm::dvec2 c = input::aim_curve(dx, 0.0, dt, p);
        const double g_oracle = 1.0 + (p.aim_curve_gain_max - 1.0) *
                                          std::pow(0.5, p.aim_curve_expo);
        CHECK(c.x == dx * g_oracle);
        CHECK(c.x > dx);  // non-vacuous: the curve actually bit
    }
    // AT rate_hi: t = 1, pow(1,e) == 1, gain == gain_max exactly (2.5 is
    // binary-friendly; pinned against the composed expression, P2-2).
    {
        const glm::dvec2 c = input::aim_curve(600.0, 0.0, dt, p);  // 2400 px/s
        CHECK(c.x == 600.0 * 2.5);
    }
    // ABOVE rate_hi — the ONLY probe pinning the min() cap (at rate_hi itself
    // t = 1 with or without the cap): 9600 px/s still gains exactly gain_max.
    {
        const glm::dvec2 c = input::aim_curve(2400.0, 0.0, dt, p);
        CHECK(c.x == 2400.0 * 2.5);
    }
    // Monotone: same delta, shrinking frame window (rising rate) — the gain
    // never decreases.
    {
        double prev = 0.0;
        for (double w = 0.25; w >= 1.0 / 1024.0; w *= 0.5) {
            const glm::dvec2 c = input::aim_curve(64.0, 0.0, w, p);
            CHECK(c.x >= prev);
            prev = c.x;
        }
        CHECK(prev == 64.0 * 2.5);  // the sweep reached the cap
    }
}

TEST_CASE("aim_curve: knob-off arm (gain_max = 1) is structurally identity") {
    control::ControllerParams p = curve_params();
    p.aim_curve_gain_max = 1.0;  // the OFF knob
    const double probes[][3] = {
        {2.0, 1.0, 1.0 / 256.0},     // sub-quant
        {40.0, 20.0, 1.0 / 256.0},   // would be deep in the band
        {5000.0, 0.0, 1.0 / 256.0},  // far above rate_hi
        {0.0, 0.0, 0.25},            // zero delta
    };
    for (const auto& pr : probes) {
        const glm::dvec2 c = input::aim_curve(pr[0], pr[1], pr[2], p);
        CHECK(c.x == pr[0]);
        CHECK(c.y == pr[1]);
    }
    // The early return preserves even the signed zero and a denormal — no
    // arithmetic runs at all (P2-1: bit-identity by construction).
    const glm::dvec2 z = input::aim_curve(-0.0, 5e-324, 1.0 / 256.0, p);
    CHECK(std::signbit(z.x));
    CHECK(z.y == 5e-324);
}

TEST_CASE("aim_curve: power-of-two scale invariance (frame-rate key)") {
    const control::ControllerParams p = curve_params();
    // The same physical hand motion sampled with a 4x longer frame window
    // delivers 4x the delta over 4x the dt — the NORMALIZED rate is identical,
    // so the gain is identical and the curved delta scales exactly by 4.
    // Bit-exact because rounding commutes with 2^k scaling at every step
    // (mag: (4dx)^2 = 16 dx^2, fl-sum scales, sqrt(16 s) = 4 sqrt(s); the
    // rate division sees the same real quotient; then fl((4dx)*g) =
    // 4*fl(dx*g)). Mutations this kills: re-keying the gain on the raw
    // per-frame delta (the rail's forbidden shape — g(4d) != g(d)) and a
    // constant/clamped rate window (rate would scale 4x).
    struct Probe {
        double dx, dy, dt;
    };
    // NOTE all probe dts stay >= 1/1024 so the qtr leg's 0.25*dt never dips
    // below kAimCurveDtFloor (1e-4) — the floor would break the 2^k
    // commutation for a smaller probe (diff red-team P3).
    const Probe probes[] = {
        {26.25, -35.0, 1.0 / 32.0},  // mag 43.75 exact -> 1400 px/s, IN-BAND
        {48.0, -20.0, 1.0 / 256.0},  // mag 52 exact -> 13312 px/s, cap region
        {6.0, 8.0, 1.0 / 16.0},      // mag 10 -> 160 px/s, sub-knee
    };
    for (const Probe& pr : probes) {
        const glm::dvec2 one = input::aim_curve(pr.dx, pr.dy, pr.dt, p);
        const glm::dvec2 four =
            input::aim_curve(4.0 * pr.dx, 4.0 * pr.dy, 4.0 * pr.dt, p);
        const glm::dvec2 qtr =
            input::aim_curve(0.25 * pr.dx, 0.25 * pr.dy, 0.25 * pr.dt, p);
        CHECK(four.x == 4.0 * one.x);
        CHECK(four.y == 4.0 * one.y);
        CHECK(qtr.x == 0.25 * one.x);
        CHECK(qtr.y == 0.25 * one.y);
    }
    // Non-vacuous: the in-band probe actually curved.
    const glm::dvec2 c =
        input::aim_curve(probes[0].dx, probes[0].dy, probes[0].dt, p);
    CHECK(std::abs(c.x) > std::abs(probes[0].dx));
}

TEST_CASE(
    "aim_curve: quantization guard keeps slow tracking linear at any "
    "fps (P1-3)") {
    // A 2 px frame at 256 fps reads 512 px/s — above the shape params' knee —
    // but is quantization noise, not a flick. The guard passes it through.
    {
        const control::ControllerParams p = curve_params();
        const glm::dvec2 c = input::aim_curve(2.0, 0.0, 1.0 / 256.0, p);
        CHECK(c.x == 2.0);  // mutation quant_px -> 0: rate 512 curves, fails
        // AT the guard boundary, exactly representable (S4a): mag == quant_px
        // == 3.0 (rate 768 > knee) still passes through — the guard is
        // inclusive (<=; a < mutant curves this probe).
        const glm::dvec2 b = input::aim_curve(3.0, 0.0, 1.0 / 256.0, p);
        CHECK(b.x == 3.0);
        // A real flick (8 px at 256 fps = 2048 px/s) still curves.
        const glm::dvec2 f = input::aim_curve(8.0, 0.0, 1.0 / 256.0, p);
        CHECK(f.x > 8.0);
    }
    // The end-to-end form of the defect: a constant SLOW hand rate (160 px/s,
    // below any knee) delivered through an integer-pixel device quantizer at
    // 64 fps vs 256 fps. knee = 200 for this leg so the fps-scaled noise
    // (1 px at 256 fps reads 256 px/s > knee) WOULD curve without the guard;
    // with it, both streams pass through untouched and the totals over the
    // same wall second are exactly equal.
    {
        control::ControllerParams p = curve_params();
        p.aim_curve_knee = 200.0;
        p.aim_curve_rate_hi = 2200.0;
        const double hand_rate = 160.0;  // px/s, true motion
        const auto run = [&](double dt, int frames) {
            double pos = 0.0, prev_floor = 0.0, total = 0.0;
            for (int i = 0; i < frames; ++i) {
                pos += hand_rate * dt;
                const double fl = std::floor(pos);
                const double delta = fl - prev_floor;  // integer px report
                prev_floor = fl;
                total += input::aim_curve(delta, 0.0, dt, p).x;
            }
            return total;
        };
        const double slow = run(1.0 / 64.0, 64);    // 1 s at 64 fps
        const double fast = run(1.0 / 256.0, 256);  // 1 s at 256 fps
        std::printf("[aim_curve quant] slow=%.17g fast=%.17g\n", slow, fast);
        CHECK(slow == 160.0);  // every quantized delta passed through raw
        CHECK(fast == 160.0);  // mutation quant_px -> 0: fast inflates > 160
    }
}

TEST_CASE("aim_curve: one shared scalar gain preserves direction") {
    const control::ControllerParams p = curve_params();
    const double dx = 30.0, dy = 17.0, dt = 1.0 / 256.0;  // deep in-band rate
    const glm::dvec2 c = input::aim_curve(dx, dy, dt, p);
    REQUIRE(c.x > dx);  // non-vacuous: it curved
    // cross(curved, raw) ~ 0: both components carry the SAME g. Tolerance,
    // not exact (fl(dx*g)*dy vs fl(dy*g)*dx differ in the last ulp) — this is
    // a direction claim, not a frame-rate one (P2-2). A per-axis-curve
    // refactor (gx != gy) lands ~1e0 here.
    CHECK(std::abs(c.x * dy - c.y * dx) < 1e-9 * std::abs(c.x * dy));
}

TEST_CASE("aim_curve: the dt floor bounds a 0-dt frame's rate") {
    const control::ControllerParams p = curve_params();
    // Totality: a 0-dt frame stays finite and lands exactly on the cap. NOTE
    // this alone does NOT pin the floor — an unfloored 50/0.0 = +inf flows
    // through min(t, 1) to the identical capped output (the diff red-team ran
    // the floor-deletion mutant against this leg alone and it SURVIVED).
    const glm::dvec2 c = input::aim_curve(50.0, 0.0, 0.0, p);
    CHECK(std::isfinite(c.x));
    CHECK(c.x == 50.0 * 2.5);
    // The floor's real contract: a degenerate window cannot manufacture a
    // rate beyond |delta|/kAimCurveDtFloor. Pinned where floored/unfloored
    // DISTINGUISH (diff red-team P2): knee 1e6 px/s, delta 50 px at dt 1e-6 —
    // floored rate 50/1e-4 = 5e5 <= knee -> passthrough; unfloored 5e7 would
    // curve to the cap. The floor-deletion mutant fails HERE.
    control::ControllerParams hp = curve_params();
    hp.aim_curve_knee = 1.0e6;
    hp.aim_curve_rate_hi = 2.0e6;
    const glm::dvec2 f = input::aim_curve(50.0, 0.0, 1.0e-6, hp);
    CHECK(f.x == 50.0);
}
