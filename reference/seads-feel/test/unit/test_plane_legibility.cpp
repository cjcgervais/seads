// RUNG E4 — VISIBILITY (docs/ENEMY_AI_E1_E2_SPEC.md). Chad's fly report
// 2026-08-20: "they are hard to see especially in the white scatter light,
// their tag and color is also a bit hard to see."
//
// WHAT THIS GATE CAN AND CANNOT SEE. This is render work; the suite has no
// framebuffer, so there is NOTHING here that claims a pixel got brighter. What
// IS pinnable is pinned:
//   1. KNOB-OFF IDENTITY. The atmosphere/scatter frame this rung sits in front
//      of is flown-approved SIGNED work. Every E4 dial at its off value must
//      return the identity EXACTLY — not "close", not "within 1e-9". These legs
//      use == on purpose.
//   2. The ON arms actually differ (an off-identity test alone passes happily
//      on a dial that is welded off — the silent-disarm class).
//   3. DETERMINISM of the glint phase: no clock, no rng, and a fleet that does
//      not blink in unison.
//   4. The derived enemy tint's stated PROPERTIES (redder, more saturated,
//      still far from the ally hue), so "deeper saturated red" is executable
//      rather than a comment.
// The visual verdict is Chad's fly / the --smoke A/B pair; see the recipe in
// config/world.toml [plane_legibility].

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>

#include "render/plane_legibility.h"
#include "render/team_color.h"

#ifdef NDEBUG
#error "SEADS gate requires an assert-live build (SPEC 6.1)"
#endif

// --- E4.1 the env-wash fraction ------------------------------------------

TEST_CASE("plane_legibility E4.1: haze frac 1.0 is the identity at every range") {
    // THE knob-off leg. aircraft_haze_frac = 1.0 must leave the mirror
    // shader's u_reflectivity untouched, so refl * scale is the same float the
    // signed build uploads. Exact equality, deliberately.
    for (double d :
         {0.0, 1.0, 250.0, 999.5, 2000.0, 4000.0, 12000.0, 1.0e6}) {
        CHECK(render::aircraft_env_wash_scale(d, 1.0, 2000.0) == 1.0);
        CHECK(render::aircraft_env_wash_scale(d, 1.0, 0.0) == 1.0);
    }
    // A value above 1.0 is not a licence to ADD wash — it is still off.
    CHECK(render::aircraft_env_wash_scale(1500.0, 1.5, 2000.0) == 1.0);
}

TEST_CASE("plane_legibility E4.1: the shipped frac attenuates only at range") {
    const double frac = 0.35, full = 2000.0;
    // At the eye the approved close-up mirror look is untouched.
    CHECK(render::aircraft_env_wash_scale(0.0, frac, full) == 1.0);
    // At and beyond the ramp distance the full fraction applies.
    CHECK(render::aircraft_env_wash_scale(full, frac, full) ==
          Catch::Approx(frac));
    CHECK(render::aircraft_env_wash_scale(9999.0, frac, full) ==
          Catch::Approx(frac));
    // The ON arm DIFFERS from the off arm in the band that matters (2-4 km was
    // Chad's stated distance) — this is the anti-silent-disarm leg.
    CHECK(render::aircraft_env_wash_scale(1000.0, frac, full) < 1.0);
    CHECK(render::aircraft_env_wash_scale(1000.0, frac, full) > frac);
    // Monotone non-increasing in range (a C1 smoothstep, no pop).
    double prev = 2.0;
    for (int i = 0; i <= 40; ++i) {
        const double s =
            render::aircraft_env_wash_scale(i * 100.0, frac, full);
        CHECK(s <= prev + 1e-12);
        prev = s;
    }
    // full_range 0 = the documented degenerate case: apply everywhere, no ramp.
    CHECK(render::aircraft_env_wash_scale(0.0, frac, 0.0) ==
          Catch::Approx(frac));
}

// --- E4.2 the tag size floor ---------------------------------------------

TEST_CASE("plane_legibility E4.2: the tag size floor is off at or below base") {
    CHECK(render::tag_font_px(12, 0.0) == 12);   // the shipped off value
    CHECK(render::tag_font_px(12, 12.0) == 12);  // equal is still off
    CHECK(render::tag_font_px(12, 11.9) == 12);  // below is still off
    CHECK(render::tag_font_px(12, 16.0) == 16);  // the shipped ON value
    CHECK(render::tag_font_px(12, 15.6) == 16);  // rounds, never truncates
}

// --- E4.2 the derived enemy tint -----------------------------------------

TEST_CASE("plane_legibility E4.2: enemy tint shift 0 is bit-identical") {
    const glm::dvec3 base = render::kSlagOrange;
    // Bit-identical, not approximately: the tag, the livery and the glint all
    // route through this call every frame, so an HSV round-trip that lands a
    // half-ULP off would silently repaint the signed slag orange.
    const glm::dvec3 off = render::enemy_legibility_tint(base, 0.0);
    CHECK(off.x == base.x);
    CHECK(off.y == base.y);
    CHECK(off.z == base.z);
    // Negative is off too (a fat-fingered config cannot invert the shift).
    const glm::dvec3 neg = render::enemy_legibility_tint(base, -1.0);
    CHECK(neg.x == base.x);
    CHECK(neg.y == base.y);
    CHECK(neg.z == base.z);
}

TEST_CASE("plane_legibility E4.2: the shifted tint is a deeper saturated red") {
    const glm::dvec3 base = render::kSlagOrange;
    const glm::dvec3 red = render::enemy_legibility_tint(base, 1.0);
    const render::Hsv hb = render::rgb_to_hsv(base);
    const render::Hsv hr = render::rgb_to_hsv(red);
    // It moved at all (anti-silent-disarm).
    CHECK(std::fabs(red.x - base.x) + std::fabs(red.y - base.y) +
              std::fabs(red.z - base.z) > 0.05);
    // REDDER: the hue landed on the stated target, away from orange.
    CHECK(hr.h == Catch::Approx(render::kEnemyLegibleHueDeg).margin(1e-6));
    CHECK(hr.h < hb.h);
    // MORE SATURATED: the whole point is that it cannot read as haze-grey.
    CHECK(hr.s > hb.s);
    CHECK(hr.s == Catch::Approx(render::kEnemyLegibleSat).margin(1e-9));
    // DEEPER: off the V = 1.00 ceiling, which is what buys luminance contrast
    // against a blown-white scatter sky.
    CHECK(hr.v < hb.v);
    CHECK(hr.v == Catch::Approx(render::kEnemyLegibleVal).margin(1e-9));
    // Still nowhere near the ally hue — the factions must not converge.
    const render::Hsv ha = render::rgb_to_hsv(render::kComplementBlue);
    CHECK(render::hue_separation_deg(hr.h, ha.h) > 140.0);
    // And still separated in LUMINANCE from the ally blue (the colour-blind
    // fallback cue: red darkens under protanopia, blue does not).
    const auto luma = [](const glm::dvec3& c) {
        return 0.2126 * c.x + 0.7152 * c.y + 0.0722 * c.z;
    };
    CHECK(std::fabs(luma(red) - luma(render::kComplementBlue)) > 0.05);
    // Half-way is between the two, so the dial is a real continuum.
    const render::Hsv hh =
        render::rgb_to_hsv(render::enemy_legibility_tint(base, 0.5));
    CHECK(hh.h < hb.h);
    CHECK(hh.h > hr.h);
}

TEST_CASE("plane_legibility E4.2: hsv_to_rgb inverts rgb_to_hsv") {
    // The tint is authored in HSV; if the round-trip drifts, the "shift 0 is
    // bit-identical" contract above is the only thing standing between a
    // rounding bug and a quietly repainted faction.
    for (const glm::dvec3& c :
         {render::kSlagOrange, render::kComplementBlue, render::kOxygenGreen,
          glm::dvec3{0.2, 0.4, 0.9}, glm::dvec3{0.7, 0.7, 0.7}}) {
        const glm::dvec3 rt = render::hsv_to_rgb(render::rgb_to_hsv(c));
        CHECK(rt.x == Catch::Approx(c.x).margin(1e-12));
        CHECK(rt.y == Catch::Approx(c.y).margin(1e-12));
        CHECK(rt.z == Catch::Approx(c.z).margin(1e-12));
    }
}

// --- E4.3 the engagement glint -------------------------------------------

TEST_CASE("plane_legibility E4.3: the glint is off at its off values") {
    CHECK_FALSE(render::glint_lit(0, 0, 0, 0.25));    // period 0
    CHECK_FALSE(render::glint_lit(0, 0, 48, 0.0));    // duty 0
    CHECK_FALSE(render::glint_lit(123, 4, -1, 0.5));  // negative period
    CHECK(render::glint_radius_m(3000.0, 0.0, 0.55) == 0.0);  // size 0 = off
}

TEST_CASE("plane_legibility E4.3: the glint phase is deterministic") {
    // Same frame + same index => same answer, always. No clock, no rng: this
    // is the whole reason the phase is FrameInfo::frame_count and not a timer.
    for (int f = 0; f < 200; ++f)
        for (int i = 0; i < 5; ++i)
            CHECK(render::glint_lit(f, i, 48, 0.22) ==
                  render::glint_lit(f, i, 48, 0.22));
    // Periodic with the stated period.
    for (int f = 0; f < 200; ++f)
        CHECK(render::glint_lit(f, 2, 48, 0.22) ==
              render::glint_lit(f + 48, 2, 48, 0.22));
    // A negative ordinal folds back into range rather than reading off the end.
    CHECK(render::glint_lit(-48, 3, 48, 0.22) ==
          render::glint_lit(0, 3, 48, 0.22));
}

TEST_CASE("plane_legibility E4.3: the fleet does not blink in unison") {
    // A 5-ship strobing as one block reads as a render bug, not as aircraft.
    // There must exist a frame where any two pilots disagree.
    for (int a = 0; a < 5; ++a) {
        for (int b = a + 1; b < 5; ++b) {
            bool differs = false;
            for (int f = 0; f < 48 && !differs; ++f)
                differs = render::glint_lit(f, a, 48, 0.22) !=
                          render::glint_lit(f, b, 48, 0.22);
            CHECK(differs);
        }
    }
}

TEST_CASE("plane_legibility E4.3: the duty cycle is the lit fraction") {
    const int period = 48;
    const double duty = 0.25;
    int lit = 0;
    for (int f = 0; f < period; ++f)
        if (render::glint_lit(f, 0, period, duty)) ++lit;
    CHECK(lit == 12);  // floor(0.25 * 48)
    // A subtle blink, not a flash-bang: well under half the period.
    CHECK(static_cast<double>(lit) / period < 0.5);
}

TEST_CASE("plane_legibility E4.3: the angular floor keeps a far bead visible") {
    const double size = 0.55, mrad = 0.55;
    // Close in, the metre size wins (the bead stays a bead on the canopy).
    CHECK(render::glint_radius_m(100.0, size, mrad) == Catch::Approx(size));
    // At Chad's stated 2-4 km the angular floor takes over — a fixed 0.55 m
    // sphere at 4 km subtends ~0.14 mrad, i.e. under a pixel: invisible
    // exactly where the report says the problem is.
    CHECK(render::glint_radius_m(4000.0, size, mrad) > size);
    CHECK(render::glint_radius_m(4000.0, size, mrad) ==
          Catch::Approx(4000.0 * mrad * 1e-3));
    // mrad 0 = pure metre size (the floor is itself a dial that can be off).
    CHECK(render::glint_radius_m(4000.0, size, 0.0) == Catch::Approx(size));
    // Monotone in range.
    CHECK(render::glint_radius_m(2000.0, size, mrad) <=
          render::glint_radius_m(4000.0, size, mrad));
}

// --- the struct's own defaults -------------------------------------------

TEST_CASE("plane_legibility: the struct defaults are the OFF frame") {
    // Any caller that does not fill FrameInfo::legibility (tests, the probe,
    // the harness, the fallback draw path) must get today's frame.
    const render::LegibilityParams d{};
    CHECK(render::aircraft_env_wash_scale(3000.0, d.aircraft_haze_frac,
                                          d.haze_full_range_m) == 1.0);
    CHECK(render::tag_font_px(12, d.tag_min_px) == 12);
    CHECK(d.tag_outline_px == 0.0);
    const glm::dvec3 t =
        render::enemy_legibility_tint(render::kSlagOrange, d.enemy_tint_shift);
    CHECK(t.x == render::kSlagOrange.x);
    CHECK(t.y == render::kSlagOrange.y);
    CHECK(t.z == render::kSlagOrange.z);
    CHECK(render::glint_radius_m(3000.0, d.glint_size_m, d.glint_min_mrad) ==
          0.0);
    CHECK_FALSE(
        render::glint_lit(7, 2, d.glint_period_frames, d.glint_duty));
}
