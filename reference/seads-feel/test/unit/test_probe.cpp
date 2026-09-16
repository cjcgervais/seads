// Target-visibility probe oracle (docs/world_build_plan.md §4; little_planet
// Stage 0). Pins the PURE contrast arithmetic — the part the gate can see (the
// framebuffer readback that feeds it is caller glue in main.cpp, no ctest runs
// seads.exe). Each leg is shaped to catch a break, mutation-verified:
//   - luma-weight swap  -> the Rec.709 green>red>blue ordering fails
//   - max instead of max-min in chroma -> the gray-patch chroma==0 leg fails
//   - drop the fabs / wrong Michelson denom -> the symmetric-contrast leg fails
//   - chroma_delta sign flip -> the "plane pops by color" leg fails

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <glm/glm.hpp>

#include "render/camera.h"  // project_dir (the on-screen guarantee, 2a)
#include "render/probe.h"

using Catch::Approx;
using render::PatchStats;
using render::pixel_chroma;
using render::pixel_luminance;
using render::probe_contrast;
using render::probe_placement;

TEST_CASE("probe: pixel luminance is Rec.709 weighted (green>red>blue)") {
    // The exact channel weights — a coefficient swap moves these.
    REQUIRE(pixel_luminance(1.0, 0.0, 0.0) == Approx(0.2126));
    REQUIRE(pixel_luminance(0.0, 1.0, 0.0) == Approx(0.7152));
    REQUIRE(pixel_luminance(0.0, 0.0, 1.0) == Approx(0.0722));
    // The ordering is the property that matters and that a swap breaks.
    REQUIRE(pixel_luminance(0.0, 1.0, 0.0) > pixel_luminance(1.0, 0.0, 0.0));
    REQUIRE(pixel_luminance(1.0, 0.0, 0.0) > pixel_luminance(0.0, 0.0, 1.0));
    // A neutral gray reads back its own value (weights sum to 1).
    REQUIRE(pixel_luminance(0.4, 0.4, 0.4) == Approx(0.4));
}

TEST_CASE("probe: chroma is a saturation proxy (gray==0, saturated==spread)") {
    // A gray patch is colorless at ANY brightness — this is what lets chroma
    // isolate the plane's livery from a monochrome field
    // (max-instead-of-max-min mutation makes this return the brightness,
    // breaking the ==0).
    REQUIRE(pixel_chroma(0.0, 0.0, 0.0) == Approx(0.0));
    REQUIRE(pixel_chroma(0.5, 0.5, 0.5) == Approx(0.0));
    REQUIRE(pixel_chroma(1.0, 1.0, 1.0) == Approx(0.0));
    // A saturated bandit-red pixel: max-min = 0.8.
    REQUIRE(pixel_chroma(0.8, 0.0, 0.0) == Approx(0.8));
    REQUIRE(pixel_chroma(0.9, 0.3, 0.1) == Approx(0.8));
}

TEST_CASE(
    "probe: luminance contrast is symmetric Michelson (dark-on-bright ==)") {
    // Plane gray 0.8 on background gray 0.2 -> |0.8-0.2|/(0.8+0.2) = 0.6.
    PatchStats bright_plane, dark_bg;
    bright_plane.add(0.8, 0.8, 0.8);
    dark_bg.add(0.2, 0.2, 0.2);
    const render::ProbeContrast a = probe_contrast(bright_plane, dark_bg);
    REQUIRE(a.lum_michelson == Approx(0.6));
    REQUIRE(a.chroma_delta == Approx(0.0));  // both gray

    // SWAP them: a dark plane on a bright field must score the SAME (the AT-6
    // "legibility is |difference|, not sky>ground" lesson; a non-abs Michelson
    // would flip the sign / go negative here).
    PatchStats dark_plane, bright_bg;
    dark_plane.add(0.2, 0.2, 0.2);
    bright_bg.add(0.8, 0.8, 0.8);
    const render::ProbeContrast b = probe_contrast(dark_plane, bright_bg);
    REQUIRE(b.lum_michelson == Approx(0.6));
}

TEST_CASE("probe: a plane pops by CHROMA at equal luminance") {
    // Saturated red plane vs a gray field MATCHED in luminance (michelson ~ 0)
    // still pops: chroma_delta carries the whole read — the "saturated livery
    // is the only color" channel, independent of brightness. A chroma_delta
    // sign flip (bg-plane) makes this go negative.
    PatchStats red_plane, gray_bg;
    red_plane.add(0.6, 0.0, 0.0);            // lum 0.12756, chroma 0.6
    gray_bg.add(0.12756, 0.12756, 0.12756);  // lum 0.12756, chroma 0
    const render::ProbeContrast c = probe_contrast(red_plane, gray_bg);
    REQUIRE(c.lum_michelson ==
            Approx(0.0).margin(1e-6));       // invisible by brightness
    REQUIRE(c.chroma_delta == Approx(0.6));  // fully visible by color
}

TEST_CASE("probe: empty / zero patches never divide by zero") {
    PatchStats empty_a, empty_b;
    const render::ProbeContrast c = probe_contrast(empty_a, empty_b);
    REQUIRE(c.lum_michelson == Approx(0.0));
    REQUIRE(c.chroma_delta == Approx(0.0));
    REQUIRE(c.lum_peak_michelson == Approx(0.0));
    REQUIRE(c.chroma_peak == Approx(0.0));
}

TEST_CASE("probe: the PEAK stat catches a sub-pixel bright/colored speck") {
    // A far bandit is a bright/colored SPECK in a patch of ground: the MEAN
    // washes it out (P1-1: "small" must not read as "low contrast"), but the
    // peak pixel still registers. 24 gray-0.3 background-ish pixels + ONE
    // bandit-bright pixel in the plane box; background is gray 0.3.
    // 48 gray-0.3 pixels + a bright-white pixel (pops by LUMINANCE) + a
    // saturated-red pixel (pops by CHROMA — note a red pixel's LUMINANCE is
    // low, 0.21 weight, so the two channels need two different specks).
    PatchStats plane, bg;
    for (int i = 0; i < 48; ++i) plane.add(0.3, 0.3, 0.3);
    plane.add(0.9, 0.9, 0.9);   // bright white: max_lum = 0.9
    plane.add(0.9, 0.1, 0.05);  // bandit red: max_chroma = 0.85
    for (int i = 0; i < 48; ++i) bg.add(0.3, 0.3, 0.3);
    const render::ProbeContrast c = probe_contrast(plane, bg);
    // Means barely move -> low mean contrast; the peak pixels pop.
    REQUIRE(c.lum_michelson < 0.05);
    REQUIRE(c.chroma_delta < 0.05);
    REQUIRE(c.lum_peak_michelson == Approx(0.5).margin(0.02));  // |0.9-0.3|/1.2
    REQUIRE(c.chroma_peak == Approx(0.85).margin(0.02));  // max-min = 0.85
}

TEST_CASE("probe: busy ground with NO plane does not fake a peak pop") {
    // Fable r2 P1-3: at range the plane box is mostly ground texture. Comparing
    // a plane MAX against a bg MEAN reports a pop from ground variance alone
    // (max-of-N > mean). Here the "plane" box holds the SAME ground texture as
    // the background and no plane pixel — the peak must read 0, not a phantom.
    PatchStats plane, bg;
    for (int i = 0; i < 12; ++i) {
        plane.add(0.2, 0.2, 0.2);
        plane.add(0.4, 0.4, 0.4);
        bg.add(0.2, 0.2, 0.2);
        bg.add(0.4, 0.4, 0.4);
    }
    const render::ProbeContrast c = probe_contrast(plane, bg);
    REQUIRE(c.lum_peak_michelson == Approx(0.0));  // vs-mean would give ~0.14
    REQUIRE(c.chroma_peak == Approx(0.0));
}

TEST_CASE("probe: an empty background patch reads NO contrast, not full pop") {
    // Fable r2 P1-2: a clipped/off-screen sample can leave the bg patch empty.
    // michelson(plane, 0) would be 1.0 (a spurious "fully visible"); the guard
    // reports 0 (no-data) and the caller flags n_bg==0 separately.
    PatchStats plane, bg;
    for (int i = 0; i < 9; ++i) plane.add(0.9, 0.9, 0.9);
    const render::ProbeContrast c = probe_contrast(plane, bg);  // bg empty
    REQUIRE(c.lum_michelson == Approx(0.0));
    REQUIRE(c.lum_peak_michelson == Approx(0.0));
    REQUIRE(c.chroma_peak == Approx(0.0));
}

TEST_CASE(
    "probe: peak michelson is symmetric (a DARK speck on bright ground)") {
    // AT-6 symmetry at the peak: a dark plane pixel on a bright field pops as
    // hard as a bright one. A peak that only tracked max_lum would miss this.
    PatchStats plane, bg;
    for (int i = 0; i < 24; ++i) plane.add(0.8, 0.8, 0.8);
    plane.add(0.1, 0.1, 0.1);  // one dark plane pixel
    for (int i = 0; i < 24; ++i) bg.add(0.8, 0.8, 0.8);
    const render::ProbeContrast c = probe_contrast(plane, bg);
    REQUIRE(c.lum_peak_michelson > 0.7);  // |0.1-0.8|/(0.1+0.8) = 0.778
}

TEST_CASE("probe: placement puts a HIGH bandit below the sphere horizon") {
    // On the R=15 km sphere, a level plane at 3 km must depress well below
    // LOCAL HORIZONTAL to see ground (horizon dip acos(15/18) = 33.6 deg) — a
    // small fixed depression points at the void sky (the P0-1 flat-earth bug).
    const double R = 15000.0;
    const double terrain = R + 600.0;            // relief top shell
    const glm::dvec3 pos{R + 3000.0, 0.0, 0.0};  // 3 km over the +X pole
    const glm::dvec3 up{1.0, 0.0, 0.0};          // local up = +X
    const glm::dvec3 nose{0.0, 0.0, -1.0};       // level, perpendicular to up
    const double margin = 5.0 * 3.14159265358979323846 / 180.0;
    const render::ProbePlacement p =
        probe_placement(pos, nose, R, terrain, margin, /*range=*/1000.0);

    REQUIRE(p.horizon_dip ==
            Approx(std::acos(R / (R + 3000.0))));  // ~0.586 rad
    REQUIRE(p.depress == Approx(p.horizon_dip + margin));
    // The sightline points toward the ground (downward along -up), NOT the sky.
    REQUIRE(glm::dot(p.dir, up) < 0.0);
    REQUIRE(p.below_horizon);  // the ray reaches GUARANTEED ground (< R)
    REQUIRE(p.above_terrain);  // target above the relief top -> not buried
    // The bandit sits between the ground (R) and the player (R+3000).
    REQUIRE(p.target_alt > 0.0);
    REQUIRE(p.target_alt < 3000.0);
}

TEST_CASE("probe: dip is measured against R base, not the relief-top shell") {
    // Fable r2 P0-1: using the relief-top shell (R+relief) for the DIP leaves
    // the depressed ray grazing ABOVE the base sphere (background = sky) while
    // the flags read valid. The dip MUST be acos(R/|pos|); a mutation to
    // acos(terrain/|pos|) moves this value and is caught here. Mid-altitude so
    // the cell is genuinely valid (below_horizon AND above_terrain).
    const double R = 15000.0;
    const double terrain = R + 600.0;
    const glm::dvec3 pos{R + 2000.0, 0.0, 0.0};
    const glm::dvec3 nose{0.0, 0.0, -1.0};
    const double margin = 5.0 * 3.14159265358979323846 / 180.0;
    const render::ProbePlacement p =
        probe_placement(pos, nose, R, terrain, margin, /*range=*/1000.0);
    // Pins the dip radius: acos(15000/17000) = 0.490 rad, NOT
    // acos(15600/17000).
    REQUIRE(p.horizon_dip == Approx(std::acos(R / (R + 2000.0))));
    REQUIRE(p.below_horizon);
    REQUIRE(p.above_terrain);
}

TEST_CASE("probe: placement flags an infeasible far range at low altitude") {
    // From 500 m up, a 3 km-range target on a ground-silhouette ray would be
    // BELOW the surface (the ground is only ~2.6 km along the depressed ray) —
    // the probe must FLAG it, not silently measure a buried target.
    const double R = 15000.0;
    const double terrain = R + 600.0;
    const glm::dvec3 pos{R + 500.0, 0.0, 0.0};
    const glm::dvec3 nose{0.0, 0.0, -1.0};
    const double margin = 5.0 * 3.14159265358979323846 / 180.0;
    const render::ProbePlacement p =
        probe_placement(pos, nose, R, terrain, margin, /*range=*/3000.0);
    REQUIRE(p.below_horizon);  // the ray does reach guaranteed ground...
    REQUIRE_FALSE(
        p.above_terrain);  // ...but the 3 km target is below relief top
    REQUIRE(p.target_alt < 600.0);  // buried within the relief shell -> flagged
}

TEST_CASE(
    "probe: ndc_to_pixel matches the reticle map (center, corners, shift)") {
    // Shared with draw.cpp to_px (P1-2). NDC +y = up; y is flipped to pixels.
    const render::PixelCoord c = render::ndc_to_pixel(0.0, 0.0, 0.0, 1280, 720);
    REQUIRE(c.x == 640);
    REQUIRE(c.y == 360);
    const render::PixelCoord tr =
        render::ndc_to_pixel(1.0, 1.0, 0.0, 1280, 720);
    REQUIRE(tr.x == 1280);
    REQUIRE(tr.y == 0);  // ndc +y=up -> top of screen
    const render::PixelCoord bl =
        render::ndc_to_pixel(-1.0, -1.0, 0.0, 1280, 720);
    REQUIRE(bl.x == 0);
    REQUIRE(bl.y == 720);
    // A positive lens shift slides the sampled row DOWN (larger py), matching
    // the 3D frustum's downward scene shift.
    const render::PixelCoord s = render::ndc_to_pixel(0.0, 0.0, 0.1, 1280, 720);
    REQUIRE(s.y == 396);  // (0.5 - (0 - 0.1)*0.5)*720
}

TEST_CASE(
    "probe: pointing the camera AT the target keeps a high bandit on-screen") {
    // 2a: above ~1800 m the (dip + margin) depression exceeds the half-FOV, so
    // a LEVEL chase camera loses the below-horizon bandit off the bottom of the
    // frame (the all-zero cells this session's sweep surfaced). The fix points
    // the SHIPPED probe camera straight at the target via probe_camera_forward.
    const double kPi = 3.14159265358979323846;
    const double R = 15000.0, terrain = R + 600.0;
    const glm::dvec3 pos{R + 3000.0, 0.0, 0.0};  // 3 km over +X pole
    const glm::dvec3 up{1.0, 0.0, 0.0};
    const glm::dvec3 nose{0.0, 0.0, -1.0};
    const double margin = 5.0 * kPi / 180.0;
    const render::ProbePlacement p =
        probe_placement(pos, nose, R, terrain, margin, /*range=*/1000.0);
    const double fovy = 60.0 * kPi / 180.0, aspect = 1280.0 / 720.0;
    const glm::dvec3 tdir = glm::normalize(p.target - pos);

    // The shipped camera-forward looks straight at the bandit -> dead-center.
    const glm::dvec3 cam_fwd = render::probe_camera_forward(p, pos);
    const render::ScreenPoint sp =
        render::project_dir(tdir, cam_fwd, up, fovy, aspect);
    REQUIRE(sp.in_front);
    REQUIRE(std::fabs(sp.x) < 1.0);
    REQUIRE(std::fabs(sp.y) < 1.0);

    // Mutation / the OLD level-camera behavior: the 38.6-deg-depressed target
    // falls BELOW the bottom edge (ndc_y ~ -1.38). This is WHY the re-point
    // exists — a level forward must project it off-screen.
    const glm::dvec3 level_fwd{0.0, 0.0, -1.0};
    const render::ScreenPoint lsp =
        render::project_dir(tdir, level_fwd, up, fovy, aspect);
    REQUIRE(lsp.y < -1.0);
}

TEST_CASE("probe: ABOVE geometry elevates the bandit against SKY") {
    // 2a look-up cell: elevate above local horizontal so the ray climbs OFF the
    // sphere (no guaranteed ground) and the bandit silhouettes against sky.
    const double kPi = 3.14159265358979323846;
    const double R = 15000.0, terrain = R + 600.0;
    const glm::dvec3 pos{R + 3000.0, 0.0, 0.0};
    const glm::dvec3 up{1.0, 0.0, 0.0};
    const glm::dvec3 nose{0.0, 0.0, -1.0};
    const double margin = 5.0 * kPi / 180.0;
    const render::ProbePlacement p =
        probe_placement(pos, nose, R, terrain, margin, /*range=*/1500.0,
                        render::ProbeGeometry::Above);
    REQUIRE_FALSE(p.below_horizon);         // upward ray reaches no ground
    REQUIRE(glm::dot(p.dir, up) > 0.0);     // points UP, above horizontal
    REQUIRE(p.depress == Approx(-margin));  // elevation reported negative
    REQUIRE(p.clear_of_terrain);            // eye (3 km) above the relief top
    REQUIRE(p.above_terrain);               // target up in the sky
    REQUIRE(p.target_alt > 3000.0);  // higher than the eye (climbing away)
}

TEST_CASE("probe: ABOVE geometry flags an eye inside the relief shell") {
    // The elevated sightline is clear only when the EYE sits above the relief
    // top; below it a near peak can occlude the ray prefix -> flag invalid.
    const double kPi = 3.14159265358979323846;
    const double R = 15000.0, terrain = R + 600.0;
    const glm::dvec3 pos{R + 400.0, 0.0,
                         0.0};  // eye BELOW the 600 m relief top
    const glm::dvec3 nose{0.0, 0.0, -1.0};
    const double margin = 5.0 * kPi / 180.0;
    const render::ProbePlacement p =
        probe_placement(pos, nose, R, terrain, margin, /*range=*/1500.0,
                        render::ProbeGeometry::Above);
    REQUIRE_FALSE(p.clear_of_terrain);
}
