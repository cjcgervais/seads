// ROW 9 shadow-cue probe -- the pure geometry legs (render/shadow_probe.h).
// The probe instrument (tools/shadow_probe.cpp) prints the numbers; these
// closed-form pins are the gate. No shadow is rendered anywhere -- this is
// measurement-only work (docs/snow_R5_immersion_ledger.md ROW 9).

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <glm/glm.hpp>

#include "render/celestial.h"
#include "render/shadow_probe.h"

using Catch::Approx;

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kD2R = kPi / 180.0;
}  // namespace

TEST_CASE("shadow offset closed form: h / tan(elev)", "[shadow_probe]") {
    // 45 deg: offset equals height exactly.
    CHECK(render::shadow_flat_offset_m(10.0, 45.0 * kD2R) ==
          Approx(10.0).epsilon(1e-12));
    // 15 deg, 50 m: 50 / tan(15 deg) = 186.60254...
    CHECK(render::shadow_flat_offset_m(50.0, 15.0 * kD2R) ==
          Approx(50.0 / std::tan(15.0 * kD2R)).epsilon(1e-12));
    CHECK(render::shadow_flat_offset_m(50.0, 15.0 * kD2R) ==
          Approx(186.60254).margin(1e-4));
    // Vertical sun: shadow directly underneath.
    CHECK(render::shadow_flat_offset_m(25.0, 90.0 * kD2R) ==
          Approx(0.0).margin(1e-9));
}

TEST_CASE("penumbra closed form: 2 tan(diam/2) x slant  -- SHIPPED 1.40 deg",
          "[shadow_probe]") {
    const double diam = 1.40 * kD2R;  // config/world.toml:1068, NOT 0.35
    // ~7.33 m at 300 m slant (the ledger's 'mostly blur vs a 10 m span').
    CHECK(render::penumbra_width_m(diam, 300.0) ==
          Approx(2.0 * std::tan(0.70 * kD2R) * 300.0).epsilon(1e-12));
    CHECK(render::penumbra_width_m(diam, 300.0) == Approx(7.331).margin(5e-3));
    // ~1.22 m at 50 m slant, ~0.37 m at 15 m (flare-height slant at 8 deg).
    CHECK(render::penumbra_width_m(diam, 50.0) == Approx(1.222).margin(2e-3));
    CHECK(render::penumbra_width_m(diam, 15.0) == Approx(0.367).margin(2e-3));
    // The consult's 0.35 deg value is exactly 4x sharper -- the correction.
    CHECK(render::penumbra_width_m(0.35 * kD2R, 300.0) ==
          Approx(render::penumbra_width_m(diam, 300.0) / 4.0).epsilon(1e-4));
    // Linear in slant.
    CHECK(render::penumbra_width_m(diam, 200.0) ==
          Approx(2.0 * render::penumbra_width_m(diam, 100.0)).epsilon(1e-12));
}

TEST_CASE("sun elevation from dot(sun, up)", "[shadow_probe]") {
    const glm::dvec3 up(0.0, 0.0, 1.0);
    CHECK(render::sun_elevation_rad(glm::dvec3(0, 0, 1), up) ==
          Approx(0.5 * kPi).epsilon(1e-12));
    CHECK(render::sun_elevation_rad(glm::dvec3(1, 0, 0), up) ==
          Approx(0.0).margin(1e-12));
    const double e = 30.0 * kD2R;
    const glm::dvec3 s(std::cos(e), 0.0, std::sin(e));
    CHECK(render::sun_elevation_rad(s, up) == Approx(e).epsilon(1e-12));
}

TEST_CASE("shadow on sphere: vertical sun hits the sub-point exactly",
          "[shadow_probe]") {
    const double R = 15000.0;
    const glm::dvec3 caster(0.0, 0.0, R + 50.0);
    const auto hit = render::shadow_on_sphere(caster, glm::dvec3(0, 0, 1), R);
    REQUIRE(hit.hit);
    CHECK(hit.slant_m == Approx(50.0).epsilon(1e-12));
    CHECK(hit.point.z == Approx(R).epsilon(1e-12));
    CHECK(std::abs(hit.point.x) < 1e-9);
    CHECK(std::abs(hit.point.y) < 1e-9);
}

TEST_CASE("shadow on sphere converges to the flat closed form as R grows",
          "[shadow_probe]") {
    const double e = 45.0 * kD2R, h = 50.0;
    const glm::dvec3 sun(std::cos(e), 0.0, std::sin(e));
    // Huge sphere: ground offset == h/tan(e) to 1e-6 relative.
    {
        const double R = 1.0e9;
        const auto hit =
            render::shadow_on_sphere(glm::dvec3(0, 0, R + h), sun, R);
        REQUIRE(hit.hit);
        const double arc =
            render::arc_between_m(hit.point, glm::dvec3(0, 0, 1), R);
        CHECK(arc == Approx(render::shadow_flat_offset_m(h, e)).epsilon(1e-6));
        CHECK(hit.slant_m == Approx(h / std::sin(e)).epsilon(1e-6));
    }
    // The game sphere (R = 15 km): within 1% of flat at 45 deg / 50 m, and
    // the curvature pushes the shadow slightly FARTHER (the ground falls away
    // down-sun).
    {
        const double R = 15000.0;
        const auto hit =
            render::shadow_on_sphere(glm::dvec3(0, 0, R + h), sun, R);
        REQUIRE(hit.hit);
        const double arc =
            render::arc_between_m(hit.point, glm::dvec3(0, 0, 1), R);
        const double flat = render::shadow_flat_offset_m(h, e);
        CHECK(arc == Approx(flat).epsilon(0.01));
        CHECK(arc > flat);
    }
}

TEST_CASE("shadow on sphere: sun below the horizon casts nothing",
          "[shadow_probe]") {
    const double R = 15000.0;
    const glm::dvec3 caster(0.0, 0.0, R + 50.0);
    // Sun 10 deg BELOW the local horizon: the anti-sun ray climbs away.
    const double e = -10.0 * kD2R;
    const glm::dvec3 sun(std::cos(e), 0.0, std::sin(e));
    CHECK_FALSE(render::shadow_on_sphere(caster, sun, R).hit);
}

TEST_CASE("the rider sits on the celestial equator (probe A premise)",
          "[shadow_probe]") {
    // The shipped [celestial] values (config/world.toml) + the projection axis
    // +Z (render/sudbury_gis.gen.h lock line). spin_axis = cos(tilt)*n +
    // sin(tilt)*lean lives entirely in the X-Y plane, so dot(a, +Z) == 0:
    // the rider's ground point is EXACTLY on the celestial equator -- half of
    // every solar day is night, at every season.
    render::CelestialConfig cfg;
    cfg.orbit_normal = {0.0, 1.0, 0.0};
    cfg.tilt_deg = 23.5;
    cfg.tilt_lean = {1.0, 0.0, 0.0};
    cfg.day_period_s = 300.0;
    cfg.year_period_s = 3600.0;
    cfg.sun_distance_m = 450000.0;
    const render::CelestialParams cel = render::make_celestial(cfg, 15000.0);
    CHECK(std::abs(glm::dot(cel.spin_axis, glm::dvec3(0, 0, 1))) < 1e-12);
    // Consequence, measured through the REAL sun_dir: the daily max elevation
    // equals 90 - |declination| at the equator. Check at an arbitrary time:
    // elevation never exceeds that bound over one day.
    const glm::dvec3 up(0.0, 0.0, 1.0);
    double max_e = -1e9, t_peak = 0.0;
    for (double t = 0.0; t < 300.0; t += 0.05) {
        const double e =
            render::sun_elevation_rad(render::sun_dir(cel, t), up);
        if (e > max_e) {
            max_e = e;
            t_peak = t;
        }
    }
    const double dec_at_peak = std::abs(render::sun_declination(cel, t_peak));
    CHECK(max_e == Approx(0.5 * kPi - dec_at_peak).margin(0.02));
}
