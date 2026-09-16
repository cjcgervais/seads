// S-mapteam — THE COMPLEMENT IS EXECUTABLE (Chad 2026-08-09: "make the allies
// switch back to an opposite blue functionally and with precision the opposite
// of blue on the color wheel of the orange slag color AND VERIFY THIS IS
// RIGHT").
//
// "Verify" is the deliverable, not a claim in a comment. This TU converts the
// two team colours to HSV and requires the thing Chad asked for: hue exactly
// 180 deg apart, at the SAME saturation and the SAME value. It FAILS if either
// colour is later nudged off-complement — which is the whole point, because
// the error mode here is invisible on screen (see the trap leg below).

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>

#include "render/team_color.h"

using Catch::Matchers::WithinAbs;

TEST_CASE("S-mapteam: the slag orange is the sim's own hot stop, unrounded") {
    // render::kSlagOrange feeds BOTH the lava shader's heatColor() hot stop
    // (render/slag.cpp injects it as #define SLAG_HOT) and the enemy faction.
    // Pin the value so a "tidy-up" of either consumer cannot drift it.
    REQUIRE(render::kSlagOrange.x == 1.00);
    REQUIRE(render::kSlagOrange.y == 0.55);
    REQUIRE(render::kSlagOrange.z == 0.12);

    const render::Hsv o = render::rgb_to_hsv(render::kSlagOrange);
    // HSV of (1.00, 0.55, 0.12): V = max = 1.00, S = (max-min)/max = 0.88,
    // H = 60*(G-B)/(max-min) = 60*0.43/0.88 = 29.3181818... deg.
    REQUIRE_THAT(o.h, WithinAbs(29.31818181818182, 1e-9));
    REQUIRE_THAT(o.s, WithinAbs(0.88, 1e-12));
    REQUIRE_THAT(o.v, WithinAbs(1.00, 1e-12));
}

TEST_CASE("S-mapteam: ally blue is the EXACT 180 deg complement of slag") {
    const render::Hsv o = render::rgb_to_hsv(render::kSlagOrange);
    const render::Hsv a = render::rgb_to_hsv(render::kComplementBlue);

    // (a) hue separation is exactly half the wheel.
    REQUIRE_THAT(render::hue_separation_deg(o.h, a.h), WithinAbs(180.0, 1e-9));
    // (b) same saturation, same value — "opposite on the wheel" means the same
    //     point reflected through the axis, not merely some other blue.
    REQUIRE_THAT(a.s, WithinAbs(o.s, 1e-12));
    REQUIRE_THAT(a.v, WithinAbs(o.v, 1e-12));
    // The predicate the loader uses agrees.
    REQUIRE(render::is_exact_complement(render::kSlagOrange,
                                        render::kComplementBlue, 1e-9));

    // FORWARD CHECK, not just a consistency check: reconstruct the complement
    // from the orange by HSV arithmetic and require the shipped constant to be
    // that value. This is what makes the leg non-vacuous — it would still fail
    // if BOTH constants were edited to some other 180-deg pair with the wrong
    // RGB written down.
    const double hc = std::fmod(o.h + 180.0, 360.0);
    const double C = o.v * o.s;  // 0.88
    const double X = C * (1.0 - std::fabs(std::fmod(hc / 60.0, 2.0) - 1.0));
    const double m = o.v - C;  // 0.12
    // hc = 209.318 deg lies in sector 3 ([180,240)) => (R,G,B) = (0, X, C).
    REQUIRE(hc >= 180.0);
    REQUIRE(hc < 240.0);
    REQUIRE_THAT(render::kComplementBlue.x, WithinAbs(0.0 + m, 1e-12));
    REQUIRE_THAT(render::kComplementBlue.y, WithinAbs(X + m, 1e-12));
    REQUIRE_THAT(render::kComplementBlue.z, WithinAbs(C + m, 1e-12));
}

TEST_CASE("S-mapteam: the plausible channel-reversal is REJECTED") {
    // THE TRAP this test exists for. Reversing the channels of the orange —
    // (1.00, 0.55, 0.12) -> (0.12, 0.55, 1.00) — looks exactly like the
    // complement and is NOT: its hue is 210.68 deg, i.e. 181.36 deg away. On
    // screen the two blues are indistinguishable, so only an executable check
    // can hold Chad's "with precision".
    const glm::dvec3 trap{0.12, 0.55, 1.00};
    const render::Hsv t = render::rgb_to_hsv(trap);
    const render::Hsv o = render::rgb_to_hsv(render::kSlagOrange);
    REQUIRE_THAT(t.h, WithinAbs(210.6818181818182, 1e-9));
    // hue_separation_deg reports the SHORTEST arc, so a 181.36 deg wheel gap
    // reads as 178.64 — the miss is 1.36 deg on the wrong side of half a turn.
    REQUIRE_THAT(render::hue_separation_deg(o.h, t.h),
                 WithinAbs(178.63636363636363, 1e-9));
    REQUIRE_FALSE(render::is_exact_complement(render::kSlagOrange, trap, 1e-6));
    // ... and it is not merely a hue miss: S also differs (0.88 vs 0.88 holds,
    // but the hue alone is disqualifying), so the loose 1e-6 loader tolerance
    // still rejects it by ~6 orders of magnitude.
    REQUIRE(std::fabs(render::hue_separation_deg(o.h, t.h) - 180.0) > 1.0);
}

TEST_CASE("S-mapteam: the three tactical hues separate in LIGHTNESS too") {
    // Colour-blind safety is not left to hue: the map's three tactical classes
    // must also be ordered and separated in luma, so they survive a total loss
    // of chroma (and so the shape language is a redundancy, not the only cue).
    const auto luma = [](const glm::dvec3& c) {
        return 0.2126 * c.x + 0.7152 * c.y + 0.0722 * c.z;
    };
    const double l_obj = luma(render::kOxygenGreen);
    const double l_ally = luma(render::kComplementBlue);
    const double l_enemy = luma(render::kSlagOrange);
    REQUIRE(l_obj < l_ally);
    REQUIRE(l_ally < l_enemy);
    REQUIRE(l_ally - l_obj > 0.08);
    REQUIRE(l_enemy - l_ally > 0.08);
}

TEST_CASE("S-mapteam: rgb_to_hsv agrees with known reference colours") {
    // Guard the instrument itself — a broken converter would make every leg
    // above vacuous (the "flat instrument certifies a flat controller" class).
    const auto h = [](double r, double g, double b) {
        return render::rgb_to_hsv(glm::dvec3{r, g, b}).h;
    };
    REQUIRE_THAT(h(1.0, 0.0, 0.0), WithinAbs(0.0, 1e-12));    // red
    REQUIRE_THAT(h(1.0, 1.0, 0.0), WithinAbs(60.0, 1e-12));   // yellow
    REQUIRE_THAT(h(0.0, 1.0, 0.0), WithinAbs(120.0, 1e-12));  // green
    REQUIRE_THAT(h(0.0, 1.0, 1.0), WithinAbs(180.0, 1e-12));  // cyan
    REQUIRE_THAT(h(0.0, 0.0, 1.0), WithinAbs(240.0, 1e-12));  // blue
    REQUIRE_THAT(h(1.0, 0.0, 1.0), WithinAbs(300.0, 1e-12));  // magenta
    // The wrap seam: a hue just BELOW red must report ~360, not a negative.
    REQUIRE(h(1.0, 0.0, 0.02) > 350.0);
    REQUIRE(h(1.0, 0.0, 0.02) < 360.0);
    // Greys: saturation 0, hue reported 0 (undefined, pinned so it is stable).
    const render::Hsv grey = render::rgb_to_hsv(glm::dvec3{0.4, 0.4, 0.4});
    REQUIRE(grey.s == 0.0);
    REQUIRE(grey.h == 0.0);
    REQUIRE_THAT(grey.v, WithinAbs(0.4, 1e-12));
    // Separation is symmetric and never exceeds a half-turn.
    REQUIRE_THAT(render::hue_separation_deg(350.0, 10.0),
                 WithinAbs(20.0, 1e-12));
    REQUIRE_THAT(render::hue_separation_deg(10.0, 350.0),
                 WithinAbs(20.0, 1e-12));
    REQUIRE_THAT(render::hue_separation_deg(0.0, 200.0),
                 WithinAbs(160.0, 1e-12));
}
