// Pure celestial core (docs/little_planet_plan.md Stage 1 ★). The gate pins the
// structural model headlessly (the module is raylib-free glm-double). Each leg
// is shaped to catch a break; mutation targets noted per leg (verified by
// editing render/celestial.cpp: flip the wheel sign, drop the +omega_year in
// the sidereal rate, drop the moon inclination term, break the ecliptic basis).

#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <glm/glm.hpp>

#include "render/celestial.h"

using Catch::Approx;
using render::CelestialConfig;
using render::CelestialParams;
using render::make_celestial;

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kR = 15000.0;  // the sim sphere radius (sun_distance >= 30*R)

CelestialConfig std_cfg() {
    CelestialConfig c;  // defaults are the plan's numbers; make tilt exact
    c.orbit_normal = {0.0, 1.0, 0.0};
    c.tilt_deg = 23.5;
    c.tilt_lean = {1.0, 0.0, 0.0};
    c.day_period_s = 3600.0;    // SOLAR day
    c.year_period_s = 43200.0;  // 12 days
    c.moon_period_s = 5400.0;
    c.moon_inclination_deg = 8.0;
    c.epoch_day_frac = 0.0;
    c.epoch_year_frac = 0.0;
    c.epoch_moon_frac = 0.0;
    return c;
}

double wrap_pi(double a) {
    while (a > kPi) a -= 2.0 * kPi;
    while (a < -kPi) a += 2.0 * kPi;
    return a;
}
}  // namespace

TEST_CASE("celestial: the derived frame is well-formed (tilt, orthogonality)") {
    const CelestialParams c = make_celestial(std_cfg(), kR);
    const double tilt = 23.5 * kPi / 180.0;
    // â is tilted from the orbit normal by exactly tilt_deg.
    REQUIRE(glm::dot(c.spin_axis, c.orbit_normal) == Approx(std::cos(tilt)));
    REQUIRE(glm::length(c.spin_axis) == Approx(1.0));
    // ê₁ is the equinox line: ⟂ â AND ⟂ the orbit normal.
    REQUIRE(glm::dot(c.ecliptic_e1, c.spin_axis) == Approx(0.0).margin(1e-12));
    REQUIRE(glm::dot(c.ecliptic_e1, c.orbit_normal) ==
            Approx(0.0).margin(1e-12));
    // ê₂ carries the obliquity: dot(ê₂, â) = +sin(tilt).
    REQUIRE(glm::dot(c.ecliptic_e2, c.spin_axis) == Approx(std::sin(tilt)));
    // Equatorial basis is orthonormal about â.
    REQUIRE(glm::dot(c.equator_x, c.spin_axis) == Approx(0.0).margin(1e-12));
    REQUIRE(glm::dot(c.equator_y, c.spin_axis) == Approx(0.0).margin(1e-12));
    REQUIRE(glm::dot(c.equator_x, c.equator_y) == Approx(0.0).margin(1e-12));
    // RA handedness (P2-1): a star at RA=90, Dec=0 lands on +equator_y. A
    // left-handed flip of equator_y (cross(equator_x,â)) mirrors the sky
    // east-west about Polaris — the Dipper pointer stars would point the wrong
    // way in Stage 4. Every other radec_to_dir call has sin(RA)=0, so this is
    // the one leg that exercises the equator_y coefficient.
    REQUIRE(glm::length(render::radec_to_dir(c, kPi / 2.0, 0.0) -
                        c.equator_y) == Approx(0.0).margin(1e-12));
}

TEST_CASE("celestial: Polaris is fixed (the wheel axis never moves)") {
    const CelestialParams c = make_celestial(std_cfg(), kR);
    // A star exactly on â (dec = +90) is Polaris; it must be invariant under
    // the wheel at every time (W rotates ABOUT â). Mutation: a wheel about any
    // other axis moves it.
    const glm::dvec3 polaris = render::radec_to_dir(c, 0.0, kPi / 2.0);
    REQUIRE(glm::length(polaris - c.spin_axis) == Approx(0.0).margin(1e-12));
    for (double t : {0.0, 137.0, 4000.0, 43200.0}) {
        const glm::dvec3 w = render::star_world(c, polaris, t);
        REQUIRE(glm::length(w - c.spin_axis) == Approx(0.0).margin(1e-9));
    }
}

TEST_CASE("celestial: declination is +-sin(tilt), zero at equinox") {
    const CelestialParams c = make_celestial(std_cfg(), kR);
    const double tilt = 23.5 * kPi / 180.0;
    // theta = phi_year + omega_year*t; equinox at theta = 0 and pi, solstice at
    // +-pi/2. With phi_year=0: t=0 is an equinox.
    REQUIRE(render::sun_declination(c, 0.0) == Approx(0.0).margin(1e-9));
    const double quarter = c.year_period_s / 4.0;  // theta = +pi/2 -> +tilt
    REQUIRE(render::sun_declination(c, quarter) == Approx(tilt));
    const double three_q =
        3.0 * c.year_period_s / 4.0;  // theta = +3pi/2 -> -tilt
    REQUIRE(render::sun_declination(c, three_q) == Approx(-tilt));
    const double half = c.year_period_s / 2.0;  // theta = +pi -> equinox
    REQUIRE(render::sun_declination(c, half) == Approx(0.0).margin(1e-9));
}

TEST_CASE(
    "celestial: the SOLAR day recurs (sun azimuth returns after "
    "day_period_s)") {
    const CelestialParams c = make_celestial(std_cfg(), kR);
    // The sun's azimuth about â (its hour angle over a fixed world meridian)
    // must return after exactly one SOLAR day. The obliquity leaves a small
    // equation-of-time wobble (~few deg); the KEY is that dropping the
    // +omega_year from the sidereal wheel (raw 2π/day) leaves it ~2π/12 = 30
    // deg short — the mutation this pins.
    const auto azimuth = [&](double t) {
        const glm::dvec3 s = render::sun_dir(c, t);
        return std::atan2(glm::dot(s, c.equator_y), glm::dot(s, c.equator_x));
    };
    for (double t0 : {0.0, 900.0, 1800.0}) {
        const double d = wrap_pi(azimuth(t0 + c.day_period_s) - azimuth(t0));
        REQUIRE(std::fabs(d) < 0.12);  // recurs (wobble only); raw-omega ~0.52
    }
}

TEST_CASE("celestial: the sun closes exactly after one year") {
    const CelestialParams c = make_celestial(std_cfg(), kR);
    // omega_day*year = (2π/day + 2π/year)*12day = 13*2π (integer turns), and
    // the ecliptic angle advances 2π, so the WORLD sun direction is identical.
    for (double t : {0.0, 500.0, 12345.0}) {
        const glm::dvec3 a = render::sun_dir(c, t);
        const glm::dvec3 b = render::sun_dir(c, t + c.year_period_s);
        REQUIRE(glm::length(a - b) == Approx(0.0).margin(1e-9));
    }
}

TEST_CASE(
    "celestial: the sun laps the star catalog once a year (winter vs summer)") {
    const CelestialParams c = make_celestial(std_cfg(), kR);
    // A catalog star on the equinox line (RA=0, Dec=0). At t=0 the sun sits on
    // it (a DAY / with-the-sun constellation); half a year later the sun is
    // opposite (a NIGHT / winter constellation) — the "Orion in winter" content
    // and the sun-laps-stars drift in one pin. The star and sun share the
    // wheel, so their angle is the inertial ecliptic drift (W cancels).
    const glm::dvec3 star = render::radec_to_dir(c, 0.0, 0.0);
    const auto angle_to_sun = [&](double t) {
        const glm::dvec3 s = render::sun_dir(c, t);
        const glm::dvec3 sw = render::star_world(c, star, t);
        return std::acos(std::clamp(glm::dot(s, sw), -1.0, 1.0));
    };
    REQUIRE(angle_to_sun(0.0) < 30.0 * kPi / 180.0);  // with the sun
    const double half = c.year_period_s / 2.0;
    REQUIRE(angle_to_sun(half) > 150.0 * kPi / 180.0);  // opposite (night)
    // Closes after a full year.
    REQUIRE(angle_to_sun(c.year_period_s) ==
            Approx(angle_to_sun(0.0)).margin(1e-6));
}

TEST_CASE("celestial: moon phase is independent of season (its own epoch)") {
    CelestialConfig a = std_cfg();
    CelestialConfig b = std_cfg();
    a.epoch_moon_frac = 0.0;  // new-ish
    b.epoch_moon_frac = 0.5;  // opposite phase, SAME season+time-of-day
    const CelestialParams ca = make_celestial(a, kR);
    const CelestialParams cb = make_celestial(b, kR);
    const double t = 1234.0;
    // The season (sun) is untouched by the moon epoch...
    REQUIRE(glm::length(render::sun_dir(ca, t) - render::sun_dir(cb, t)) ==
            Approx(0.0).margin(1e-12));
    REQUIRE(render::sun_declination(ca, t) ==
            Approx(render::sun_declination(cb, t)));
    // ...but the moon MOVES (a half-period epoch offset puts it on the opposite
    // side of its orbit), so the phase dot(moon,sun) differs — the Stage-5/6
    // full-moon vs new-moon-at-the-same-season cells are reachable only because
    // of this independent epoch.
    REQUIRE(glm::length(render::moon_dir(ca, t) - render::moon_dir(cb, t)) >
            1.0);
    const double phase_a =
        glm::dot(render::moon_dir(ca, t), render::sun_dir(ca, t));
    const double phase_b =
        glm::dot(render::moon_dir(cb, t), render::sun_dir(cb, t));
    REQUIRE(std::fabs(phase_a - phase_b) > 1e-6);
}

TEST_CASE(
    "celestial: the moon inclination widens its declination to tilt+incl") {
    const CelestialParams c = make_celestial(std_cfg(), kR);
    const double tilt = 23.5 * kPi / 180.0;
    const double incl = 8.0 * kPi / 180.0;
    // At psi = pi/2 (t = moon_period/4, phi_moon=0) the moon reaches its max
    // declination; dot(moon, â) = sin(tilt + incl) by angle addition. A
    // mutation dropping the inclination term reads sin(tilt) instead.
    const double t = c.moon_period_s / 4.0;
    const glm::dvec3 m = render::moon_dir(c, t);
    REQUIRE(glm::dot(m, c.spin_axis) == Approx(std::sin(tilt + incl)));
    REQUIRE(std::sin(tilt + incl) > std::sin(tilt));  // genuinely wider
}

TEST_CASE(
    "celestial: the moon rides the daily wheel (not just its own orbit)") {
    // P1-1: dot(moon, â) is INVARIANT under the wheel (a rotation about â), so
    // the inclination + phase legs are blind to a dropped sky_wheel — the moon
    // would drift at only its orbital rate (wrong speed AND direction across
    // the sky) with the gate green. Pin the diurnal motion: at zero inclination
    // the inertial moon is cos(psi)ê₁ + sin(psi)ê₂, and moon_dir must be that
    // WHEELED — cross-checked against star_world (the known-wheeled path) as
    // the oracle.
    CelestialConfig f = std_cfg();
    f.moon_inclination_deg = 0.0;
    const CelestialParams c = make_celestial(f, kR);
    const double t = 900.0;  // wheel is a large rotation here (not identity)
    const double psi = c.phi_moon + (2.0 * kPi / c.moon_period_s) * t;
    const glm::dvec3 m_inertial =
        std::cos(psi) * c.ecliptic_e1 + std::sin(psi) * c.ecliptic_e2;
    const glm::dvec3 expected =
        render::star_world(c, m_inertial, t);  // wheeled
    REQUIRE(glm::length(render::moon_dir(c, t) - expected) ==
            Approx(0.0).margin(1e-12));
    // ...and the wheel genuinely moved it (else the pin above is vacuous under
    // a wheel that happens to be ~identity): drop-wheel would leave m_inertial.
    REQUIRE(glm::length(expected - m_inertial) > 0.3);
}

TEST_CASE("celestial: the authored epoch phases are honored (not droppable)") {
    // P1-2: every other leg uses epoch fracs = 0, so phi_day/phi_year are
    // droppable with the gate green — yet the committed config ships nonzero
    // epochs (the spawn time-of-day + season the probe relies on to reach every
    // cell). Pin all three phase sites.
    const double tilt = 23.5 * kPi / 180.0;
    const CelestialParams base = make_celestial(std_cfg(), kR);  // all epochs 0

    CelestialConfig yc = std_cfg();
    yc.epoch_year_frac = 0.25;  // theta(0) = pi/2 -> the +tilt solstice at t=0
    const CelestialParams cy = make_celestial(yc, kR);
    REQUIRE(render::sun_declination(cy, 0.0) ==
            Approx(tilt));                                   // phi_year in decl
    REQUIRE(render::season_phase(cy, 0.0) == Approx(0.25));  // phi_year here

    CelestialConfig dc = std_cfg();
    dc.epoch_day_frac = 0.25;  // rotates the wheel a quarter turn at t=0
    const CelestialParams cd = make_celestial(dc, kR);
    // phi_day rotates ALL content: the sun sits a quarter-turn away at t=0.
    REQUIRE(glm::length(render::sun_dir(cd, 0.0) - render::sun_dir(base, 0.0)) >
            0.3);
}

TEST_CASE("celestial: season_phase is periodic (closes after one year)") {
    // P2-2: the phase repeats each year (circular), so a Stage-6 consumer sees
    // the same season at t and t+year. Coverage for season_phase, which no
    // other leg exercised.
    const CelestialParams c = make_celestial(std_cfg(), kR);
    for (double t : {0.0, 3000.0, 20000.0}) {
        REQUIRE(render::season_phase(c, t + c.year_period_s) ==
                Approx(render::season_phase(c, t)));
    }
}

TEST_CASE("celestial: make_celestial rejects a bad config") {
    // tilt_lean parallel to the orbit normal — no lean direction.
    CelestialConfig par = std_cfg();
    par.tilt_lean = {0.0, 1.0, 0.0};  // == orbit_normal
    REQUIRE_THROWS(make_celestial(par, kR));
    // year not an integer number of solar days.
    CelestialConfig frac = std_cfg();
    frac.year_period_s = 43000.0;  // 43000/3600 = 11.94...
    REQUIRE_THROWS(make_celestial(frac, kR));
}

// Moon phase (Stage 5). moon_illum_fraction = (1 - dot(moon_dir, sun_dir))/2 is
// the illuminated fraction: 0 new, 1 full. Pinned three ways: the closed form,
// the range, and that the moon ACTUALLY sweeps its phases over a moon period
// (min near 0, max near 1) — a mutation that froze the phase (e.g. dropped the
// sun term) would pass a lone value check but fail the sweep.
TEST_CASE("celestial: moon phase is the illuminated fraction, and sweeps 0..1") {
    const CelestialParams c = make_celestial(std_cfg(), kR);
    double lo = 1.0, hi = 0.0;
    for (int i = 0; i < 400; ++i) {
        const double t = c.moon_period_s * i / 400.0;
        const double f = render::moon_illum_fraction(c, t);
        REQUIRE(f >= 0.0);
        REQUIRE(f <= 1.0);
        // Closed form matches the dirs (not a re-derivation — the same call the
        // shader's ground fill + disc phase both consume).
        const double d = glm::dot(render::moon_dir(c, t), render::sun_dir(c, t));
        REQUIRE(f == Approx(0.5 * (1.0 - d)));
        lo = std::min(lo, f);
        hi = std::max(hi, f);
    }
    REQUIRE(lo < 0.1);  // reaches ~new moon
    REQUIRE(hi > 0.9);  // reaches ~full moon
}

// The moon DISC's phase SIGN (Fable-after P0-1: +sqrt is the far-side normal and
// globally inverts the phase — full renders dark). This mirrors the sky-FS
// disc-CENTER expression in C++: n_center = -toMoon, lit ~ dot(n_center, toSun)
// = -dot(toMoon, toSun) = -dot(moon_dir, sun_dir) = 2*frac - 1. So the disc
// center is LIT at full moon and DARK at new — pinned here so a sign flip in
// moon_disc() (or the fraction) is caught headlessly, not only by Chad's eye.
TEST_CASE("celestial: moon disc center is lit at full moon, dark at new (sign)") {
    const CelestialParams c = make_celestial(std_cfg(), kR);
    // Find a near-full and a near-new time over a moon period.
    double t_full = 0.0, t_new = 0.0, f_full = 0.0, f_new = 1.0;
    for (int i = 0; i < 400; ++i) {
        const double t = c.moon_period_s * i / 400.0;
        const double f = render::moon_illum_fraction(c, t);
        if (f > f_full) { f_full = f; t_full = t; }
        if (f < f_new)  { f_new = f;  t_new = t; }
    }
    // disc-center lit-ness (the shader's smoothstep(-eps,eps, dot(n_center,toSun))
    // argument) == 2*frac - 1: +1 at full, -1 at new.
    const auto center_lit = [&](double t) {
        return -glm::dot(render::moon_dir(c, t), render::sun_dir(c, t));
    };
    REQUIRE(center_lit(t_full) > 0.8);   // full moon: center brightly lit
    REQUIRE(center_lit(t_new) < -0.8);   // new moon: center dark
    REQUIRE(center_lit(t_full) == Approx(2.0 * f_full - 1.0));
}

// Aurora shell colatitude (Stage 8): the ray-shell geometry the sky-FS aurora()
// mirrors. From an eye INSIDE the shell, a ray toward the pole hits near
// colatitude 0; toward the anti-pole, near pi; and every hit is ON the shell.
// Pins the single-positive-root interior case + the sign (Fable-after P1-1/P2-5).
TEST_CASE("celestial: aurora shell colatitude - interior ray hits the oval geometry") {
    const CelestialParams c = make_celestial(std_cfg(), kR);
    const double shell_r = kR + 12000.0;               // R + aurora height
    const glm::dvec3 axis = c.spin_axis;
    const glm::dvec3 eye =
        (kR + 3000.0) * glm::normalize(glm::dvec3(1.0, 0.3, 0.2));  // inside
    REQUIRE(glm::length(eye) < shell_r);               // eye inside the shell

    // A ray aimed at the north-pole point on the shell -> colatitude ~ 0.
    const glm::dvec3 to_n = glm::normalize(axis * shell_r - eye);
    const double rho_n = render::aurora_shell_colatitude(eye, to_n, axis, shell_r);
    REQUIRE(rho_n < 0.15);
    // Aimed at the anti-pole point -> colatitude ~ pi.
    const glm::dvec3 to_s = glm::normalize(-axis * shell_r - eye);
    const double rho_s = render::aurora_shell_colatitude(eye, to_s, axis, shell_r);
    REQUIRE(rho_s > kPi - 0.15);

    // Every hit is ON the shell + colatitude in [0,pi] for a spread of rays.
    for (int i = 0; i < 60; ++i) {
        const double a = 2.0 * kPi * i / 60.0;
        const glm::dvec3 dir = glm::normalize(
            glm::dvec3(std::cos(a), std::sin(a) * 0.7, std::sin(a) * 0.5 + 0.3));
        const double rho =
            render::aurora_shell_colatitude(eye, dir, axis, shell_r);
        REQUIRE(rho >= 0.0);
        REQUIRE(rho <= kPi + 1e-9);
        // Reconstruct t from the returned colatitude's hit: the point at that
        // colatitude along the ray must satisfy |eye + t*dir| == shell_r. Recompute
        // t the same way and check the residual (independent of the acos).
        const double b = glm::dot(eye, dir);
        const double cc = glm::dot(eye, eye) - shell_r * shell_r;
        const double t = -b + std::sqrt(std::max(b * b - cc, 0.0));
        REQUIRE(t > 0.0);  // the single positive interior root
        REQUIRE(glm::length(eye + t * dir) == Approx(shell_r).epsilon(1e-6));
    }
}

TEST_CASE("celestial: sun_distance must be >= 30*R (exact boundary)") {
    // The finite-disc parallax bound, landed in Stage 2. The shipped 450 km
    // sits EXACTLY on 30*R = 450 km, so the comparison MUST be >= (a > would
    // reject the committed config). Mutation-verify by editing celestial.cpp >=
    // -> >: the boundary case below then throws.
    CelestialConfig cfg = std_cfg();
    cfg.sun_distance_m = 30.0 * kR;  // exactly on the boundary
    REQUIRE_NOTHROW(make_celestial(cfg, kR));

    // A hair below the boundary must throw (a finite epsilon, not float noise).
    cfg.sun_distance_m = 30.0 * kR - 1.0;
    REQUIRE_THROWS(make_celestial(cfg, kR));
}
