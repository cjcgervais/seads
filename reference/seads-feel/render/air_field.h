#pragma once

#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>

// S-airdome (docs/bubble_atmosphere_spec.md §1.1) — the render-side, raylib-
// free MIRROR of sim::atm_frac_at's SPATIAL factor (sim/aero.h). The visual
// atmosphere amount at a world point is this factor WITHOUT the altitude
// taper (atm_frac(alt)) and WITHOUT the tunnel term (a mine interior has no
// sky) — see the spec §1.1 for why both are excluded.
//
// H1 discipline: this is a SECOND implementation of sim::atm_frac_at's union
// math (a GLSL copy is unavoidable — no GL context in sim/ — so it is fenced
// two ways instead of one: this header is pinned bit-for-bit against the LIVE
// sim function in test/unit/test_air_field.cpp (>=200 sample points, 1e-9
// tolerance, mutation-verified), and render/air_field_glsl.cpp's kAirFieldGLSL
// is a line-by-line float transliteration of THIS file (not of sim/aero.h
// directly) so there is exactly one text this code is derived from.
//
// render/ never includes sim/ (SPEC §5's render-reads-state-only rule is about
// SimState, not about a data mirror like this — AtmosphereField's shape is
// copied by app/main.cpp into this struct every frame; render/ itself stays
// free of a sim/ include).

namespace render {

constexpr int kMaxAirBubbles = 4;

// Plain render-side mirror of sim::AtmosphereField + the [atmosphere]
// render-tuning dials the air march needs. The geometry fields (deck_agl_m,
// deck_soft_m, planet_R, bubbles[]) are the spec's literal AirField; the
// tuning fields below them (haze_density..mie_tint_day) have NO sim-side
// mirror to pin against (they are look dials, not plant physics), so
// test_air_field.cpp's cross-check against sim::atm_frac_at exercises the
// geometry fields only — see the deliverable report for this coverage limit
// stated explicitly.
struct AirField {
    // false => air_at() returns 1.0 EVERYWHERE (the spatial factor sim::
    // atm_frac_at returns when env->atm == nullptr is exactly 1 — no deck, no
    // bubbles — so the render mirror must read the SAME "full air, spatially
    // uniform" world in that case; never a phantom vacuum).
    bool enabled = false;
    double deck_agl_m = 120.0;
    double deck_soft_m = 200.0;
    double planet_R = 15000.0;

    struct Bubble {
        glm::dvec3 center_dir{0.0, 1.0, 0.0};
        glm::dvec3 major_axis{
            0.0};           // unit tangent, semi-major dir; 0 = circular
        double a = 6000.0;  // ground_radius_m (semi-major arc radius)
        double b = 0.0;     // minor_radius_m (semi-minor); 0 = circular
        // S-domeround (docs/airdome_round_spec.md §1.3/§4): this field now
        // CARRIES H, the volume-preserving dome centre height — "folded
        // into the existing per-bubble ceiling uniform" per the spec,
        // rather than adding a second uniform array. The caller (app/
        // main.cpp) copies sim::AtmosphereField::Bubble::dome_h_m here, NOT
        // the raw config ceiling_m.
        double ceiling_m = 4959.7;
        double edge_soft_m = 1200.0;
        double ceil_soft_m = 600.0;
    };
    Bubble bubbles[kMaxAirBubbles]{};
    int bubble_count = 0;

    // S-domeround: the superellipse roundness exponent, shared field-wide —
    // the sim-side mirror of sim::AtmosphereField::dome_exponent. Default
    // 3.0 matches the sim-side default (a bare AirField gets the shipped
    // dome shape).
    double dome_exponent = 3.0;

    // Render-tuning dials (config/world.toml [atmosphere], the NEW keys).
    // Defaults here mirror the RETUNED config/world.toml values (quality fix
    // pass, docs/airdome_report.md) — a lone struct-default use (e.g. a unit
    // test) gets the dialed-in numbers, not the original untuned spec starts.
    double haze_density = 0.25;    // air_haze_density: always-on in-air haze
    double weather_gain = 0.30;    // air_weather_gain: weather thickening gain
    double aerial_gain = 0.35;     // ground extinction gain
    double tau_scale_m = 2600.0;   // path length of unit air == 1 optical depth
    double march_max_m = 55000.0;  // sky ray march cap
    int march_steps_sky = 20;
    int march_steps_ground = 6;
    // The [atmosphere] haze_overcast_density CEILING on the weather thickening
    // term (quality fix pass item 7): supersedes the "loaded, validated, never
    // read" orphan the initial S-airdome landing left behind — density =
    // uAirHazeDensity + min(weather*weather_gain, this), so a full-overcast
    // weather cell can thicken air only up to this ceiling, keeping the prior
    // haze_overcast_density tuning meaningful instead of silently inert.
    double haze_overcast_density = 0.90;
    float mie_tint_day[3] = {0.92f, 0.94f, 0.98f};
};

// R6's exact "inside" smoothstep profile (sim/aero.h atm_falloff): 1.0 at/
// below the core boundary, C1 down to 0.0 at/beyond d >= soft.
inline double air_falloff(double d, double soft) {
    if (d <= 0.0) return 1.0;
    if (d >= soft || soft <= 0.0) return 0.0;
    const double t = d / soft;
    return 1.0 - t * t * (3.0 - 2.0 * t);
}

// The direction-dependent ellipse radius (sim/aero.h atm_frac_at's inline
// branch / world/faction_bubbles.h::ellipse_r_eff) — the SAME polar-form
// ellipse radius, transliterated a THIRD time here (sim, world, render each
// need their own copy-free-of-that-layer's-forbidden-dependency; the H1
// single-text discipline is enforced by the cross-check test, not by sharing
// code across the sim/world/render boundary).
inline double air_ellipse_radius(const glm::dvec3& center_dir,
                                 const glm::dvec3& major_axis, double a,
                                 double b, const glm::dvec3& up, double c,
                                 double arc) {
    if (b <= 0.0) return a;
    constexpr double kArcEps = 1e-6;
    if (arc < kArcEps) return b;
    const glm::dvec3 center_n = glm::normalize(center_dir);
    glm::dvec3 m = major_axis - center_n * glm::dot(major_axis, center_n);
    const double m_len = glm::length(m);
    if (m_len < 1e-9) return a;
    m /= m_len;
    glm::dvec3 t = up - center_n * c;
    const double t_len = glm::length(t);
    if (t_len < 1e-12) return b;
    t /= t_len;
    const double cos_th = glm::clamp(glm::dot(t, m), -1.0, 1.0);
    const double sin_th = std::sqrt(std::max(0.0, 1.0 - cos_th * cos_th));
    const double denom =
        std::sqrt((b * cos_th) * (b * cos_th) + (a * sin_th) * (a * sin_th));
    return denom > 1e-9 ? (a * b / denom) : b;
}

// The spatial (deck UNION bubbles) air fraction at `pos` (a world position,
// meters, relative to the planet center) — sim::atm_frac_at WITHOUT the
// atm_frac(alt) taper multiply and WITHOUT the tunnel union term (spec §1.1).
inline double air_at(const glm::dvec3& pos, const AirField& f) {
    if (!f.enabled) return 1.0;
    const double r = glm::length(pos);
    const double alt = r - f.planet_R;
    double one_minus = 1.0 - air_falloff(alt - f.deck_agl_m, f.deck_soft_m);
    if (f.bubble_count > 0 && r > 0.0) {
        const glm::dvec3 up = pos / r;
        for (int i = 0; i < f.bubble_count; ++i) {
            const AirField::Bubble& b = f.bubbles[i];
            const double c = glm::clamp(
                glm::dot(up, glm::normalize(b.center_dir)), -1.0, 1.0);
            const double arc = f.planet_R * std::acos(c);
            const double radius_h = air_ellipse_radius(
                b.center_dir, b.major_axis, b.a, b.b, up, c, arc);
            // S-domeround (docs/airdome_round_spec.md §1) — the SAME
            // superellipse-of-revolution law as sim/aero.h::atm_frac_at
            // (this mirror is pinned against it in test_air_field.cpp).
            // b.ceiling_m CARRIES H here (see the struct comment above).
            const double alt_p = std::max(alt, 0.0);
            const double H = b.ceiling_m;
            double u;
            if (radius_h <= 0.0 || H <= 0.0) {
                u = 0.0;
            } else {
                const double rho = std::sqrt(arc * arc + alt_p * alt_p);
                if (rho <= 0.0) {
                    u = 1.0;
                } else {
                    const double n =
                        f.dome_exponent > 0.0 ? f.dome_exponent : 3.0;
                    const double s = std::pow(
                        std::pow(arc / radius_h, n) + std::pow(alt_p / H, n),
                        1.0 / n);
                    const double d = rho * (s - 1.0) / s;
                    const double w = std::pow(alt_p / H, n) / std::pow(s, n);
                    const double soft =
                        b.edge_soft_m * (1.0 - w) + b.ceil_soft_m * w;
                    u = air_falloff(d, soft);
                }
            }
            one_minus *= (1.0 - u);
        }
    }
    return 1.0 - one_minus;
}

// ---------------------------------------------------------------------------
// S-marchbound (Chad's fly, 2026-08-09: a world-anchored "tire track" lattice
// artifact banding the sky, worst NEAR THE DOME BOUNDARIES).
//
// ATTRIBUTION: march STRIDE, not the dither hash. The sky march ran a fixed
// `march_steps_sky` (20) across a blind `march_max_m` (55 km), so ds = 2750 m
// — more than TWICE the 1200 m bubble edge-soft width it has to resolve. The
// dome shell was being sampled at a stride coarser than the shell itself,
// which quantizes the falloff into concentric bands locked to the geometry
// (hence world-anchored, and hence worst at the boundary where the gradient
// lives). The per-pixel jitter added in the quality fix pass was dithering
// the symptom; it could not fix an under-resolved integrand.
//
// THE FIX: march only the segment that can CONTAIN air. air_march_range
// returns a conservative [t0,t1] bracket — the union of the deck shell and a
// bounding sphere per bubble — so the SAME 20 steps land inside the air
// instead of being spent on vacuum. Inside a dome that turns ds ~2750 m into
// a few hundred metres (well under the soft width) and the banding goes away
// at the source. A ray that meets no air at all returns an EMPTY range, so
// vacuum short-circuits to tau == 0 exactly (strengthening the
// black-by-construction property, and cheaper too).
//
// The bracket must never CLIP real air, or the dome would be visibly cut. The
// per-bubble radius is a proved upper bound, not an eyeballed pad: for a point
// at arc `s` and altitude `alt` over the bubble's ground centre,
//     |p - c|^2 = alt^2 + 2R(R+alt)(1 - cos(s/R)) <= alt^2 + s^2 (1 + alt/R)
// using 1-cos(x) <= x^2/2. And air_at is exactly 0 beyond arc = r_eff + soft
// and beyond alt = H + soft (both directions shown in test_air_field.cpp),
// with soft <= max(edge_soft, ceil_soft). Substituting those two maxima gives
// the radius below; kMarchBoundMargin is float-arithmetic insurance on top.

constexpr double kMarchBoundMargin = 1.02;

// Ray/sphere entry-exit parameters for the sphere at `c` of radius `rad`.
// Returns {t0, t1} with t1 < t0 when the ray misses (the caller's emptiness
// test). Not clamped to the ray's forward half — the caller clamps.
inline glm::dvec2 air_sphere_interval(const glm::dvec3& o, const glm::dvec3& dir,
                                      const glm::dvec3& c, double rad) {
    const glm::dvec3 oc = o - c;
    const double b = glm::dot(oc, dir);
    const double cc = glm::dot(oc, oc) - rad * rad;
    const double disc = b * b - cc;
    if (disc < 0.0) return glm::dvec2(1.0, -1.0);  // miss
    const double sq = std::sqrt(disc);
    return glm::dvec2(-b - sq, -b + sq);
}

// The conservative march bracket along `dir` from `eye_pos`, clipped to
// [0, max_dist]. Returns {t0, t1}; t1 <= t0 means "no air anywhere on this
// ray" (march nothing, tau == 0). A disabled field is full air everywhere, so
// it brackets the whole ray — never a phantom vacuum, matching air_at.
inline glm::dvec2 air_march_range(const glm::dvec3& eye_pos,
                                  const glm::dvec3& dir, double max_dist,
                                  const AirField& f) {
    if (!f.enabled) return glm::dvec2(0.0, max_dist);
    double t0 = 1e30, t1 = -1e30;
    // The global low deck: air can exist anywhere below this shell radius.
    const double r_deck = f.planet_R + f.deck_agl_m + f.deck_soft_m;
    const glm::dvec2 deck =
        air_sphere_interval(eye_pos, dir, glm::dvec3(0.0), r_deck);
    if (deck.y > deck.x) {
        t0 = std::min(t0, deck.x);
        t1 = std::max(t1, deck.y);
    }
    for (int i = 0; i < f.bubble_count; ++i) {
        const AirField::Bubble& b = f.bubbles[i];
        const double soft_max = std::max(b.edge_soft_m, b.ceil_soft_m);
        const double a_max = std::max(b.a, b.b) + soft_max;
        const double h_max = b.ceiling_m + soft_max;
        if (a_max <= 0.0 || h_max <= 0.0) continue;
        const double rad =
            kMarchBoundMargin *
            std::sqrt(h_max * h_max +
                      a_max * a_max * (1.0 + h_max / f.planet_R));
        const glm::dvec3 c = glm::normalize(b.center_dir) * f.planet_R;
        const glm::dvec2 iv = air_sphere_interval(eye_pos, dir, c, rad);
        if (iv.y > iv.x) {
            t0 = std::min(t0, iv.x);
            t1 = std::max(t1, iv.y);
        }
    }
    t0 = std::max(t0, 0.0);
    t1 = std::min(t1, max_dist);
    if (t1 <= t0) return glm::dvec2(0.0, -1.0);  // empty: no air on this ray
    return glm::dvec2(t0, t1);
}

// S-domeround (docs/airdome_round_spec.md §2, Chad's 2026-08-09 ruling: "if
// there is thin air somewhere then no weather there. Weather only in the
// bubbles."): gate a raw weather SCALAR (not just the visual haze) by the
// local spatial air fraction at `eye_pos` -- pure, so the mechanism is
// directly unit-testable; app/main.cpp calls this instead of inlining the
// multiply, so every consumer (precip, the `\` force-haze debug key, any
// future weather-driven audio) agrees.
inline double gate_weather_by_air(double raw_weather, const glm::dvec3& eye_pos,
                                  const AirField& f) {
    return raw_weather * air_at(eye_pos, f);
}

}  // namespace render
