#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

// Pure celestial core (docs/little_planet_plan.md Stage 1 ★; the
// Fable-formalized structural model, plan §"Structural model"). ONE inertial
// celestial frame + ONE per-frame daily wheel. The sim sphere is INERTIALLY
// FIXED — the ground never moves in world coords; the celestial sphere (sun,
// moon, stars) wheels about the fixed tilt axis â. A rotating GROUND is BANNED
// (breaks the sim frame). This module is that wheel, as pure math.
//
// PURE glm DOUBLE — no raylib, no clock, in seads_render_core so the gate pins
// it headlessly (the sphere_param pattern). Shaders later receive only finished
// vectors/matrices (never a raw large `t` — float sin(ω·43200) is driver
// garbage). Time is TICK-DERIVED by the caller: t_cel = tick_count · sim_dt
// (Stage 1 also adds tick_count); this module only takes a double t_cel.
//
// SEAM (little_planet_plan.md standing rules, greppable): celestial time NEVER
// feeds sim/ or control/ or drone AI; â is render-only CONTENT (drawn, never
// read by any up/bank/level/aim computation) so Polaris is NOT a §6 fixed-axis
// violation — the naming here uses spin_axis / declination / ra/dec, never
// north/heading/lat/lon.
//
// ★ AUTHORISED EXCEPTION (WINTER_LAW §3.7, SC1_COLD_SPEC.md §2, 2026-08-11):
// app/main.cpp derives a SCALAR TEMPERATURE from celestial time (sun_sin_elev
// + a night_phase both computed app-side from this module's pure functions)
// and writes it into sim::SledParams::air_temp_c / snow_hardness once per
// tick, before step_sled. This is celestial time reaching sim/ through the
// APP, by ONE named, ruled path — not this module reaching into sim/ itself
// (this file still contains zero sim/ includes, zero clock reads) and not a
// second clock (the SAME t_cel that wheels the sun feeds both). The §6
// fixed-axis prohibition (â never read by up/bank/level/aim) is UNCHANGED:
// temperature reads no axis, only the scalar elevation the night art already
// reads. No other sim/ or control/ or drone-AI path may follow this precedent
// without its own WINTER_LAW ruling.

namespace render {

// Raw [celestial] config (degrees / seconds / [0,1) fractions — the config
// boundary). The loader fills this from world.toml and calls make_celestial
// once.
struct CelestialConfig {
    glm::dvec3 orbit_normal{0.0, 1.0,
                            0.0};         // ecliptic pole (orbit plane normal)
    double tilt_deg = 23.0;               // axial tilt of the spin axis
    glm::dvec3 tilt_lean{1.0, 0.0, 0.0};  // which way â leans from the normal
    double day_period_s = 3600.0;         // SOLAR day (what the player feels)
    double year_period_s = 43200.0;       // asserted integer x day_period_s
    double epoch_day_frac = 0.0;          // three INDEPENDENT phase offsets
    double epoch_year_frac = 0.0;
    double epoch_moon_frac = 0.0;
    double sun_angular_diameter_deg = 0.35;
    double sun_distance_m = 450000.0;
    double sun_intensity = 1.0;
    double sun_glare_deg = 3.0;
    double moon_angular_diameter_deg = 1.5;
    double moon_period_s = 5400.0;
    double moon_inclination_deg = 8.0;
    double moon_intensity = 0.12;
    double star_brightness = 0.8;
};

// Derived, ready-to-use constants (built once). All directions unit, INERTIAL
// world frame. omega_day is the SIDEREAL wheel rate (derived from the solar day
// + the yearly motion) so the SOLAR day is what recurs over a ground point.
struct CelestialParams {
    glm::dvec3 spin_axis{0.0, 1.0, 0.0};     // â (Polaris direction), unit
    glm::dvec3 orbit_normal{0.0, 1.0, 0.0};  // n̂, unit
    glm::dvec3 ecliptic_e1{1.0, 0.0, 0.0};   // ê₁: ⟂ â AND n̂ (the equinox line)
    glm::dvec3 ecliptic_e2{0.0, 0.0, 1.0};   // ê₂: dot(ê₂,â) = +sin(tilt)
    glm::dvec3 equator_x{1.0, 0.0, 0.0};     // RA=0 reference (= ê₁), ⟂ â
    glm::dvec3 equator_y{0.0, 0.0, 1.0};     // = cross(â, equator_x)
    double tilt_rad = 0.0;
    double day_period_s = 0.0;  // SOLAR day (carried for time-scaling/debug)
    double year_period_s = 0.0;
    double moon_period_s = 0.0;
    double omega_day = 0.0;   // sidereal wheel (RETROGRADE: omega_year - 2π/day) [rad/s]
    double omega_year = 0.0;  // 2π/year [rad/s]
    double omega_moon = 0.0;  // 2π/moon_period [rad/s]
    double phi_day = 0.0;     // = 2π·epoch_day_frac
    double phi_year = 0.0;
    double phi_moon = 0.0;
    double moon_inclination_rad = 0.0;
    // Visual scalars, carried for the render stages (inert in the math here).
    double sun_distance_m = 0.0;
    double sun_angular_radius_rad = 0.0;
    double sun_intensity = 0.0;
    double sun_glare_rad = 0.0;  // PARKED: the sun halo is the deferred Stage-3
                                 // haze-gated Mie forward-scatter, not a disc glare
                                 // (Chad ruled the always-on disc halo off, 3a)
    double moon_angular_radius_rad = 0.0;
    double moon_intensity = 0.0;
    double star_brightness = 0.0;
};

// Build the derived params. Asserts: tilt_lean not parallel to orbit_normal
// (needs a lean direction); year_period_s a positive integer multiple of
// day_period_s (the year assert is against SOLAR days); sun_distance_m >= 30*R
// (the finite-disc parallax bound, R = the sim sphere radius, single-source).
// Throws std::runtime_error on a bad config so the loader surfaces it like
// every other strict check.
CelestialParams make_celestial(const CelestialConfig& cfg, double R);

// The daily wheel W(t) = R(â, omega_day·t + phi_day) — applied to ALL celestial
// content (sun, moon, stars) to map inertial → world.
glm::dquat sky_wheel(const CelestialParams& c, double t_cel);

// Sun world direction (unit). The inertial sun rides the ecliptic circle at
// -omega_year (retrograde to the wheel) so its apparent azimuth over a fixed
// ground meridian advances at exactly the SOLAR rate; W(t) then wheels it.
glm::dvec3 sun_dir(const CelestialParams& c, double t_cel);

// Moon world direction (unit): its own inclined circle (node line = ê₁) at
// omega_moon, wheeled by W(t). Phase (Stage 5) = dot(moon_dir, sun_dir).
glm::dvec3 moon_dir(const CelestialParams& c, double t_cel);

// Illuminated fraction of the moon disc from the planet [0,1] (Stage 5):
// (1 - dot(moon_dir, sun_dir))/2. 1 = full (moon opposite sun), 0 = new. Pure
// geometry; wheel-invariant (W rotates both dirs equally). Drives the disc's
// honest phase AND the phase-scaled ground moonlight.
double moon_illum_fraction(const CelestialParams& c, double t_cel);

// Sun declination [rad] (angle above the equatorial plane ⟂ â): ±tilt over the
// year, 0 at equinox. Invariant under the wheel (W rotates about â).
double sun_declination(const CelestialParams& c, double t_cel);

// Season phase ∈ [0,1) from the yearly angle. The VALUE necessarily wraps
// 0.999→0 at the new year; "circular" is a CONSUMER contract — Stage 6 must
// read it via sin/cos or a circle distance, NEVER lerp the raw scalar (that IS
// the sawtooth). Pinned to close after one year.
double season_phase(const CelestialParams& c, double t_cel);

// Authored sky content (RA/Dec, radians) → INERTIAL unit direction. Dec
// measured from the equatorial plane; RA about â from equator_x. Polaris
// (dec=+π/2) → â.
glm::dvec3 radec_to_dir(const CelestialParams& c, double ra_rad,
                        double dec_rad);

// Map an inertial catalog direction (a star) to world at t_cel: W(t)·dir. A
// star on â (Polaris) is fixed; every other star wheels about it.
glm::dvec3 star_world(const CelestialParams& c, const glm::dvec3& catalog_dir,
                      double t_cel);

// Aurora shell hit colatitude (Stage 8): the angle from the spin axis â of the
// point where the view ray (from `eye`, unit `dir`) meets the aurora shell at
// radius shell_r about the planet center. The eye is ALWAYS inside the shell
// (aurora_height_m is set above the max flyable altitude + asserted at load), so
// the interior ray has EXACTLY one positive root — no branch, no discontinuity
// (Fable-after P1-1). Pure geometry; the sky-FS aurora() mirrors this so the
// curtain hangs on the auroral OVAL about the fixed geographic pole (â is the
// inertial ground axis, so the oval does NOT wheel with the stars). Returns a
// colatitude in [0, π]; â is render CONTENT (read for a glow, never by aim).
double aurora_shell_colatitude(const glm::dvec3& eye, const glm::dvec3& dir,
                               const glm::dvec3& spin_axis, double shell_r);

}  // namespace render
