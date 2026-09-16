#pragma once

#include <cmath>
#include <glm/glm.hpp>
#include <limits>
#include <vector>

#include "sim/environment.h"
#include "sim/params.h"

// R5 — the GravityField Escape Ceiling (MASTER_PLAN §3.B). The one designed
// deviation from constant g: full gravity through the fight band, a Gaussian
// falloff above h_g0, tuned so a redline vertical zoom from 4 km JUST escapes
// (~238 m/s vs the 245 redline). The field is central and conservative, so
// the specific-energy accounting below is exact for ARBITRARY trajectories,
// not just radial ones (Fable-BEFORE verification, 2026-07-15).
//
// Everything here is the H1 single source: the plant (sim/step.cpp), the
// climb AT, and the later R7 escape telemetry + bandit will_escape() all
// consume THESE functions — a re-derived copy anywhere is a fork.

namespace sim {

// Defined here (environment.h only forward-declares the field types). The
// felt values live in config/game.toml [gravity] (the tuning contract); the
// defaults are the §3.B design numbers so explicit test constructions match
// the spec table. Read iff Environment.grav != nullptr.
struct GravityField {
    double h_g0_m = 5000.0;     // full g at/below this altitude [m]
    double sigma_g_m = 1500.0;  // Gaussian falloff width above [m]
};

// R6 — the AtmosphereField (MASTER_PLAN §3.C). Makes air a SPATIAL quantity:
// the global deck (breathable near any ground) unioned with N town "bubble"
// domes. Outside all of them the air thins toward vacuum, and from that
// gradient the SOFT ceiling/edge walls EMERGE — thrust, lift, AND control
// authority all starve together (all ∝ the same density the plant applies and
// the controller inverts), so the pilot's momentum carries him out into thin
// air and he SAGS back, rather than hitting a scripted push. Static in R6
// (config-defined test bubbles); the economy owns the bubble dimensions LIVE
// in R15. Read iff Environment.atm != nullptr.
//
// H1 single source with sim/aero.h::atm_frac: the SPATIAL fraction MULTIPLIES
// the existing vertical taper (bubbles can only REMOVE air, never add above
// the R5-approved service-ceiling / escape-sky envelope). The sampler is
// sim::atm_frac_at (sim/aero.h — the density site); this struct is pure data.
struct AtmosphereField {
    // The global deck: full air below deck_agl_m above the surface, smoothly
    // fading to zero by deck_agl_m + deck_soft_m. Keeps the whole planet
    // breathable and landable OUTSIDE the bubbles (the go-anywhere ruling).
    // AGL is measured over the bare sphere at R in R6 (terrain-relative deck
    // is a later refinement — the deck is a soft floor, not a crash surface).
    double deck_agl_m = 120.0;
    double deck_soft_m = 200.0;
    // ★★★ RUNG E16 — THAT "LATER REFINEMENT", BUILT AND MEASURED. false = the
    // R6 bare-sphere deck, bit-for-bit; true = the deck is measured from the
    // TERRAIN (env->ground->radius_at), which is what the dial above says it
    // is ("full air below deck_agl_m above the surface") and what the
    // go-anywhere ruling needs it to be.
    //
    // WHY IT STOPPED BEING A REFINEMENT AND BECAME A DEFECT: R6 landed on a
    // bare sphere, where the two frames are the same thing. The real DEM then
    // landed under it (relief_scale 350 m) and nothing went red — the
    // recurring law of this ladder, a constant that DESCRIBES the world
    // silently stops describing it when the world moves. Measured on the
    // shipped assets/sudbury_dem.png: 69% of the surface stands ABOVE the
    // full-air deck (120 m) and 26% above its fade (320 m), so over most of
    // the planet the "go-anywhere breathable floor" is UNDERGROUND. You cannot
    // land outside a bubble there, and an aeroplane crossing between domes has
    // no lane to fly. See docs/ENEMY_AI_E1_E2_SPEC.md §E16.
    //
    // ⚠ It is read ONLY where env->ground is live; with no ground field the
    // sampler is the bare-sphere deck either way (the superset firewall).
    bool deck_terrain_relative = false;

    // Town bubble domes (spherical caps). center_dir is a UNIT direction from
    // planet center; ground_radius_m is the cap's surface (great-circle arc)
    // radius; ceiling_m the dome height AMSL; edge_soft_m / ceil_soft_m the
    // smoothstep transition widths. Km-scale softness makes a felt-but-not-
    // scripted wall (Fable-BEFORE §5: 50 m at cruise is a 0.35 s cliff that
    // reads as a script). The felt values live in config/game.toml [bubble].
    // TRUE-ELLIPSE bubbles (conquest rung, Chad's 2026-07-25 ruling): a bubble
    // is normally a CIRCLE (ground_radius_m, isotropic). Setting minor_radius_m
    // > 0 turns it into an ELLIPSE — ground_radius_m becomes the semi-MAJOR
    // arc radius `a`, minor_radius_m the semi-minor arc radius `b`, and
    // major_axis a UNIT tangent at center_dir (re-orthogonalized + normalized
    // defensively in the sampler) pointing along the semi-major axis. Default
    // major_axis{0.0} + minor_radius_m 0.0 = LEGACY CIRCULAR: the sampler's
    // ellipse branch is gated off (`minor_radius_m > 0.0`) so every existing
    // circular bubble (bare Bubble{center_dir, radius, ...}) is bit-identical
    // to the pre-ellipse code (same operations, not just numerically close —
    // see sim/aero.h's atm_frac_at and its dedicated bit-identity test).
    struct Bubble {
        glm::dvec3 center_dir{0.0, 1.0, 0.0};
        double ground_radius_m = 6000.0;
        double ceiling_m = 4000.0;
        double edge_soft_m = 1200.0;
        double ceil_soft_m = 600.0;
        glm::dvec3 major_axis{
            0.0};  // unit tangent, semi-major direction; 0 = circular
        double minor_radius_m = 0.0;  // semi-minor arc radius `b`; 0 = circular
        // S-domeround (docs/airdome_round_spec.md §1.3): the volume-
        // preserving DOME centre height H, distinct from ceiling_m (which
        // stays the config "felt ceiling" / volume knob). H > ceiling_m for
        // any finite n (a dome holds less air than a cylinder of the same
        // height, so H must rise to preserve the volume). 0.0 sentinel =>
        // the sampler falls back to H = ceiling_m literally (the
        // bubble_ceiling_volume_preserve=false reading) — every existing
        // Bubble{...5-field aggregate} construction in this tree gets this
        // fallback unless a builder sets it explicitly (world/faction_
        // bubbles.h, app/main.cpp).
        double dome_h_m = 0.0;
    };
    std::vector<Bubble> bubbles;

    // S-domeround (docs/airdome_round_spec.md §1.1): the superellipse-of-
    // revolution roundness exponent `n` shared by every bubble in this
    // field (Chad's ONE fly-dial, config/game.toml [atmosphere]
    // bubble_dome_exponent). n=2 => true ellipsoid dome; n=3 => the shipped
    // "domed with a slight shoulder"; n>=32 => the old flat-topped cylinder
    // within a tight bound (never bit-identical off-axis — only the two
    // pure axes are exact at any n, by construction, see the spec). Default
    // 3.0 so a bare AtmosphereField (e.g. a test fixture) gets the shipped
    // dome shape, not a silently-reverted cylinder.
    double dome_exponent = 3.0;
};

// Far-tail clip (Fable-BEFORE P1-2): at x = (h - h_g0)/sigma >= 26 the
// magnitude is EXACT 0.0. Without it, g underflows into subnormals near
// x ~ 27.3 (h ~ 46 km at defaults) where length()/normalize() lose enough
// precision to spuriously trip the radial invariants on a drifting escaped
// plane. The clip makes that regime unrepresentable. The binding integral
// below needs NO matching branch: the clipped tail mass is
// K*erfc(26/sqrt(2)) ~ 1e-143 J/kg — beyond double precision at kJ scale.
// Structural constant, not a dial: it exists for the FP format, not feel.
constexpr double kGravTailCutX = 26.0;

// Gravity MAGNITUDE at altitude [m/s^2]. Null env/grav returns the literal
// p.g — the all-null path is the identical arithmetic to the frozen v3
// kernel, bit-for-bit (the goldens prove it).
inline double g_at(double altitude_m, const Environment* env,
                   const AircraftParams& p) {
    if (env == nullptr || env->grav == nullptr) {
        return p.g;
    }
    const GravityField& gf = *env->grav;
    if (altitude_m <= gf.h_g0_m) {
        return p.g;  // full g through the fight band (zero gradient at h_g0)
    }
    const double x = (altitude_m - gf.h_g0_m) / gf.sigma_g_m;
    if (x >= kGravTailCutX) {
        return 0.0;
    }
    return p.g * std::exp(-0.5 * x * x);
}

// Binding energy above altitude h [J/kg]: integral of g_at from h to
// infinity, closed form via erfc (verified against independent quadrature by
// the R5 AT — the golden-blesses-bug counterpart).
//   h <= h_g0:  g*(h_g0 - h) + K
//   h >  h_g0:  K * erfc((h - h_g0) / (sigma*sqrt(2)))
// with K = g*sigma*sqrt(pi/2) ~ 18.44 kJ/kg at the §3.B table. Null env/grav
// => +infinity: a constant-g world is infinitely bound, escape is
// unrepresentable (will_escape() is structurally false — Fable-BEFORE P2-2).
inline double binding_energy_above(double altitude_m, const Environment* env,
                                   const AircraftParams& p) {
    if (env == nullptr || env->grav == nullptr) {
        return std::numeric_limits<double>::infinity();
    }
    const GravityField& gf = *env->grav;
    constexpr double kPi = 3.14159265358979323846;
    const double K = p.g * gf.sigma_g_m * std::sqrt(kPi / 2.0);
    if (altitude_m <= gf.h_g0_m) {
        return p.g * (gf.h_g0_m - altitude_m) + K;
    }
    return K * std::erfc((altitude_m - gf.h_g0_m) /
                         (gf.sigma_g_m * std::sqrt(2.0)));
}

// Specific energy [J/kg]: E > 0 <=> escaped (irreversible — the Apparition's
// verdict); E <= 0 <=> bound (the Drift falls back eventually). Exact for
// any velocity direction (central conservative field). Null => -infinity
// (bound forever).
inline double specific_energy(double speed, double altitude_m,
                              const Environment* env, const AircraftParams& p) {
    return 0.5 * speed * speed - binding_energy_above(altitude_m, env, p);
}

}  // namespace sim
