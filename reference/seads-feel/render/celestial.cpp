#include "render/celestial.h"

#include <cmath>
#include <stdexcept>

namespace render {

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;
}  // namespace

CelestialParams make_celestial(const CelestialConfig& cfg, double R) {
    CelestialParams c;

    // The sun is a finite-position disc; too near and the eye-relative diffuse
    // error grows and the disc parallax over a flight becomes a warp. >= 30*R
    // (>= not >: the shipped 450 km sits EXACTLY on 30*R = 450 km). Deferred
    // from Stage 1 — landed here, the first stage that consumes the sun.
    if (!(cfg.sun_distance_m >= 30.0 * R))
        throw std::runtime_error(
            "celestial: sun_distance_m must be >= 30*R (finite-disc parallax "
            "bound)");

    const glm::dvec3 n = glm::normalize(cfg.orbit_normal);
    // The lean direction, projected into the plane ⟂ n (a pure "which way does
    // the pole tip" hint; its component along n is meaningless).
    glm::dvec3 lean = cfg.tilt_lean - glm::dot(cfg.tilt_lean, n) * n;
    if (glm::length(lean) < 1e-9)
        throw std::runtime_error(
            "celestial: tilt_lean is parallel to orbit_normal (no lean "
            "direction for the axial tilt)");
    lean = glm::normalize(lean);

    if (!(cfg.day_period_s > 0.0) || !(cfg.year_period_s > 0.0))
        throw std::runtime_error("celestial: day/year period must be positive");
    // The year is asserted an INTEGER number of SOLAR days (the sidereal wheel
    // is derived; a raw 2π/day would put noon-to-noon off by ~1/N).
    const double days_per_year = cfg.year_period_s / cfg.day_period_s;
    if (std::fabs(days_per_year - std::round(days_per_year)) > 1e-6 ||
        std::round(days_per_year) < 2.0)
        throw std::runtime_error(
            "celestial: year_period_s must be an integer (>=2) multiple of "
            "day_period_s");

    c.tilt_rad = cfg.tilt_deg * kPi / 180.0;
    c.orbit_normal = n;
    // â: tip the orbit normal by tilt toward the lean direction.
    c.spin_axis =
        glm::normalize(std::cos(c.tilt_rad) * n + std::sin(c.tilt_rad) * lean);
    // Ecliptic in-plane basis. ê₁ ⟂ n AND ⟂ â (â ∈ span(n, lean)), so it is the
    // equinox line and dot(ê₁,â)=0. ê₂ = lean, in the ecliptic-adjacent plane
    // with dot(ê₂,â)=+sin(tilt).
    c.ecliptic_e1 = glm::normalize(glm::cross(n, lean));
    c.ecliptic_e2 = glm::cross(c.ecliptic_e1, n);  // == lean (unit)
    // Equatorial basis for RA/Dec (pole = â). equator_x ⟂ â (reuse ê₁), and
    // equator_y completes a right-handed frame about â.
    c.equator_x = c.ecliptic_e1;
    c.equator_y = glm::normalize(glm::cross(c.spin_axis, c.equator_x));

    c.day_period_s = cfg.day_period_s;
    c.year_period_s = cfg.year_period_s;
    c.moon_period_s = cfg.moon_period_s;
    c.omega_year = kTwoPi / cfg.year_period_s;
    // Sidereal wheel. The sky wheels about +â so the SOLAR day — noon to noon
    // over a fixed meridian — recurs (net apparent sun rate = omega_day -
    // omega_year, magnitude 2π/day). Chad flew it 2026-07-12 and the
    // sun/moon/stars crossed the sky the WRONG way (a mirror of Earth), so the
    // wheel is now RETROGRADE: net rate = -2π/day. This is NOT a naive sign flip
    // of the whole wheel — that leaves the omega_year term double-counted and
    // the solar day ~1.05 rad/day short (the trap the solar-day test pins).
    // Recompute so |net| stays EXACTLY 2π/day: subtract the day term instead of
    // adding it (was `+ kTwoPi/day`).
    c.omega_day = c.omega_year - kTwoPi / cfg.day_period_s;
    c.omega_moon = kTwoPi / cfg.moon_period_s;
    c.phi_day = kTwoPi * cfg.epoch_day_frac;
    c.phi_year = kTwoPi * cfg.epoch_year_frac;
    c.phi_moon = kTwoPi * cfg.epoch_moon_frac;
    c.moon_inclination_rad = cfg.moon_inclination_deg * kPi / 180.0;

    c.sun_distance_m = cfg.sun_distance_m;
    c.sun_angular_radius_rad = 0.5 * cfg.sun_angular_diameter_deg * kPi / 180.0;
    c.sun_intensity = cfg.sun_intensity;
    c.sun_glare_rad = cfg.sun_glare_deg * kPi / 180.0;
    c.moon_angular_radius_rad =
        0.5 * cfg.moon_angular_diameter_deg * kPi / 180.0;
    c.moon_intensity = cfg.moon_intensity;
    c.star_brightness = cfg.star_brightness;
    return c;
}

glm::dquat sky_wheel(const CelestialParams& c, double t_cel) {
    return glm::angleAxis(c.omega_day * t_cel + c.phi_day, c.spin_axis);
}

glm::dvec3 sun_dir(const CelestialParams& c, double t_cel) {
    // Inertial sun on the ecliptic circle. The basis is LEFT-handed about the
    // orbit normal (ê₁×ê₂ = -n̂ by construction), so +theta is RETROGRADE about
    // n̂. The apparent motion over a fixed meridian nets to the SOLAR rate
    // (|omega_day - omega_year| = 2π/day), noon-to-noon recurs (the solar-day
    // test). The daily wheel is now RETROGRADE (Chad's 2026-07-12 direction fix
    // — see make_celestial), so omega_day - omega_year = -2π/day. DO NOT "fix"
    // the +theta to -theta — that re-introduces the ~1.05 rad/day drift.
    const double theta = c.phi_year + c.omega_year * t_cel;
    const glm::dvec3 s_inertial =
        std::cos(theta) * c.ecliptic_e1 + std::sin(theta) * c.ecliptic_e2;
    return glm::normalize(sky_wheel(c, t_cel) * s_inertial);
}

glm::dvec3 moon_dir(const CelestialParams& c, double t_cel) {
    // Moon on its own circle, inclined by moon_inclination about the node line
    // ê₁; its own rate/phase (independent of the sun => moon phase is set by
    // epoch_moon_frac independent of season).
    const double psi = c.phi_moon + c.omega_moon * t_cel;
    const glm::dvec3 in_plane =
        std::cos(c.moon_inclination_rad) * c.ecliptic_e2 +
        std::sin(c.moon_inclination_rad) * c.orbit_normal;
    const glm::dvec3 m_inertial =
        std::cos(psi) * c.ecliptic_e1 + std::sin(psi) * in_plane;
    return glm::normalize(sky_wheel(c, t_cel) * m_inertial);
}

double aurora_shell_colatitude(const glm::dvec3& eye, const glm::dvec3& dir,
                               const glm::dvec3& spin_axis, double shell_r) {
    // |eye + t·dir|² = shell_r²  =>  t² + 2(eye·dir)t + (|eye|² - shell_r²) = 0.
    // Inside the shell => |eye|² - shell_r² < 0 => the roots straddle 0 => the
    // single positive root is -b + sqrt(b² - c) (b = eye·dir, c < 0 => disc > 0).
    const double b = glm::dot(eye, dir);
    const double c = glm::dot(eye, eye) - shell_r * shell_r;
    const double t = -b + std::sqrt(std::max(b * b - c, 0.0));
    const glm::dvec3 hit = glm::normalize(eye + t * dir);
    return std::acos(glm::clamp(glm::dot(hit, spin_axis), -1.0, 1.0));
}

double moon_illum_fraction(const CelestialParams& c, double t_cel) {
    // Illuminated fraction of the disc seen from the planet = (1 - cos ψ)/2,
    // ψ = elongation = angle between the directions TO the moon and TO the sun.
    // Full moon (moon opposite sun, ψ=π) => 1; new moon (ψ=0) => 0. Sun-at-
    // infinity, so the direction to the sun at the moon == at the planet (no
    // correction). W(t) rotates both dirs equally => the dot is wheel-invariant.
    const double d = glm::dot(moon_dir(c, t_cel), sun_dir(c, t_cel));
    return 0.5 * (1.0 - glm::clamp(d, -1.0, 1.0));
}

double sun_declination(const CelestialParams& c, double t_cel) {
    const double theta = c.phi_year + c.omega_year * t_cel;
    // sinδ = dot(ŝ, â) = sin(theta)·sin(tilt) (ê₁⟂â; dot(ê₂,â)=sin tilt). W(t)
    // preserves it (rotation about â).
    return std::asin(std::sin(theta) * std::sin(c.tilt_rad));
}

double season_phase(const CelestialParams& c, double t_cel) {
    double p = (c.phi_year + c.omega_year * t_cel) / kTwoPi;
    p -= std::floor(p);  // wrap to [0,1), circular (no sawtooth)
    return p;
}

glm::dvec3 radec_to_dir(const CelestialParams& c, double ra_rad,
                        double dec_rad) {
    return std::cos(dec_rad) * (std::cos(ra_rad) * c.equator_x +
                                std::sin(ra_rad) * c.equator_y) +
           std::sin(dec_rad) * c.spin_axis;
}

glm::dvec3 star_world(const CelestialParams& c, const glm::dvec3& catalog_dir,
                      double t_cel) {
    return sky_wheel(c, t_cel) * catalog_dir;
}

}  // namespace render
