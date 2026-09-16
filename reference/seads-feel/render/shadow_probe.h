#pragma once

#include <cmath>
#include <glm/glm.hpp>

// ROW 9 PRE-BUILD MEASUREMENT (docs/snow_R5_immersion_ledger.md, ROW 9): the
// pure geometry behind the shadow-as-landing-instrument probe. NO shader, NO
// shadow, NO rendering change lives here or downstream of here -- this header
// exists so the question "can the own-aircraft shadow cue exist in this world
// at all?" is answered with NUMBERS before anyone writes a shadow system.
//
// Follows the render/probe.h convention: PURE glm double, raylib-free, in a
// header the gate can pin headlessly; the printing glue is a standalone
// instrument (tools/shadow_probe.cpp, the seads_sled_probe pattern -- "this is
// the instrument; the tests are the gate"). Nothing here takes a clock: t_cel
// is always passed in by the caller.
//
// Closed forms pinned by test/unit/test_shadow_probe.cpp:
//  - flat-ground shadow offset  d = h / tan(elev)
//  - penumbra width             w = 2 * tan(ang_diam/2) * slant
//  - shadow point on the R-sphere = first hit of the anti-sun ray (exact on
//    the bare sphere; the DEM relief is metres against a 15 km radius, and the
//    landing case is a flat snowfield by construction)

namespace render {

// Horizontal distance from the caster's ground sub-point to its shadow on a
// FLAT ground, for altitude h and sun elevation elev_rad (> 0). The classic
// h/tan(e). Caller guards elev_rad > 0 (no sun above the horizon = no shadow).
inline double shadow_flat_offset_m(double h, double elev_rad) {
    return h / std::tan(elev_rad);
}

// Penumbra (soft-edge) width of a shadow cast by a FINITE-ANGULAR-SIZE sun at
// slant distance slant_m from the caster: the umbra->light transition band is
// the sun's angular diameter projected over the caster->ground distance.
// w = 2 * tan(diam/2) * slant  (~= diam_rad * slant for small angles).
// ⚠ ROW-9 CORRECTION: the shipped sun_angular_diameter_deg is 1.40
// (config/world.toml -- "Chad: 2x again = 4x orig"), NOT the 0.35 the consult
// used. Callers must feed the SHIPPED value.
inline double penumbra_width_m(double sun_ang_diam_rad, double slant_m) {
    return 2.0 * std::tan(0.5 * sun_ang_diam_rad) * slant_m;
}

// Sun elevation above the local horizontal at a ground point whose local up is
// `up` (unit): asin of the vertical component of the (unit) sun direction.
inline double sun_elevation_rad(const glm::dvec3& sun_dir,
                                const glm::dvec3& up) {
    return std::asin(glm::clamp(glm::dot(sun_dir, up), -1.0, 1.0));
}

// Where the caster's shadow lands on the bare R-sphere: march the anti-sun ray
// from `caster` (|caster| > R) and take the FIRST intersection with the
// sphere. sun_dir points TO the sun (unit); the ray direction is -sun_dir.
// hit == false when the ray misses (sun below the local horizon -- the shadow
// runs off the planet's limb, i.e. there is no shadow).
struct ShadowHit {
    bool hit = false;
    glm::dvec3 point{0.0};  // world shadow point on the sphere
    double slant_m = 0.0;   // caster -> shadow distance along the ray
};

inline ShadowHit shadow_on_sphere(const glm::dvec3& caster,
                                  const glm::dvec3& sun_dir, double R) {
    ShadowHit s;
    const glm::dvec3 d = -glm::normalize(sun_dir);
    const double b = glm::dot(caster, d);
    const double c = glm::dot(caster, caster) - R * R;
    const double disc = b * b - c;
    if (disc < 0.0) return s;  // ray misses the sphere entirely
    const double t = -b - std::sqrt(disc);  // FIRST (near) root
    if (t <= 0.0) return s;  // sphere behind the ray start (sun below ground)
    s.hit = true;
    s.slant_m = t;
    s.point = caster + t * d;
    return s;
}

// Great-circle ground distance between two directions (need not be unit),
// on the R-sphere. Uses the half-chord asin form -- acos(dot) loses ~R*1e-8 m
// of precision for nearly-parallel directions, which matters exactly at the
// small offsets this probe measures.
inline double arc_between_m(const glm::dvec3& a, const glm::dvec3& b,
                            double R) {
    const glm::dvec3 an = glm::normalize(a), bn = glm::normalize(b);
    return R * 2.0 * std::asin(glm::clamp(0.5 * glm::length(an - bn),
                                          0.0, 1.0));
}

}  // namespace render
