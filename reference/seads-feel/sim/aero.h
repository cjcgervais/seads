#pragma once

#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "sim/fields.h"
#include "sim/params.h"
#include "sim/state.h"
#include "sim/world.h"
#include "world/heightfield.h"  // E16 — the terrain-relative deck reference
#include "world/tunnel_net.h"  // T1 — the full-air tunnel union term below

// The aero/authority pieces of the plant, factored out because SPEC §7
// requires them defined ONCE and read identically at all four sites:
// plant torque (step.cpp), controller inversion (§9.4, Section 4), the
// braking law's derived alpha_max (§9.3), and AT-18's analytic formula.
// A second implementation of any of these anywhere is an H1 finding.

namespace sim {

inline double q_dyn(double rho, double speed) {
    return 0.5 * rho * speed * speed;
}

// Atmosphere fraction (MB-atm): the SOFT-ceiling taper. Exactly 1 at/below
// atm_taper_alt (the fight band is untouched — every golden flies below it),
// Gaussian falloff above (zero slope at the taper start, steepening with
// altitude). Scales BOTH the density seen by q and the engine's thrust, so
// lift, drag, control authority, and power thin together — the honest
// service ceiling emerges where T_max*atm_frac == minimum drag (~8 km at the
// shipped table) instead of being a wall. sigma <= 0 -> structurally OFF
// (== 1 everywhere). Defined ONCE here (the four-site H1 discipline): the
// plant (step.cpp), the controller inversion (plant_invert), the braking
// law's derived accel, AT-18, and AT-12's config-side reconstruction all
// call THIS.
inline double atm_frac(double altitude, const AircraftParams& p) {
    if (p.atm_taper_sigma <= 0.0) return 1.0;
    const double d = altitude - p.atm_taper_alt;
    if (d <= 0.0) return 1.0;
    return std::exp(-d * d / (2.0 * p.atm_taper_sigma * p.atm_taper_sigma));
}

// Density at altitude — the ONE expression q is built from (MB-atm).
inline double rho_at(double altitude, const AircraftParams& p) {
    return p.rho * atm_frac(altitude, p);
}

// R6 — smoothstep "inside" profile: EXACTLY 1.0 at/below the core boundary
// (d <= 0), C1 down to EXACTLY 0.0 at/beyond d >= soft. The exact 0/1 plateaus
// are structural: they make the vacuum tail unrepresentable (no subnormal
// underflow — the R5 kGravTailCut lesson, here for free) and give the union
// below its exact no-op identities. `d` is the signed distance PAST the core.
inline double atm_falloff(double d, double soft) {
    if (d <= 0.0) return 1.0;
    if (d >= soft || soft <= 0.0) return 0.0;
    const double t = d / soft;
    return 1.0 - t * t * (3.0 - 2.0 * t);  // 1 - smoothstep
}

// R6 — SPATIAL atmosphere fraction (MASTER_PLAN §3.C). The H1 density site:
// the plant (step.cpp), the controller inversion (plant_invert) and its
// damp_ff q, the derived helpers (ang_accel_max_derived / load_factor), and
// the wind audio ALL call THIS — a re-derived copy is a fork (AT-18b's ρ-fork
// detector, extended with the spatial leg, fires if the plant and the
// inversion sample different density at a bubble edge).
//
// Null env/atm => the literal scalar atm_frac(altitude(position,p), p): the
// SAME arithmetic as the frozen v3 kernel, bit-for-bit (the goldens prove it —
// altitude() is the one shared helper, never hand-inlined differently).
//
// Live: atm_frac(alt) * u, where u = 1 - Π(1 - u_i) is the complement-product
// UNION of the deck and every bubble dome (Fable-BEFORE §1). The union has the
// exact identities a smooth-max lacks: any component at 1 => u == 1.0 exactly
// (in-bubble-core flight is bit-identical to baseline — a free AT), and it
// never exceeds 1 in overlaps (a p-norm/LSE max would fly denser-than-sea-
// level air where two towns overlap). The spatial factor only REMOVES air.
inline double atm_frac_at(const glm::dvec3& position, const Environment* env,
                          const AircraftParams& p) {
    const double alt = altitude(position, p);
    if (env == nullptr || env->atm == nullptr) {
        return atm_frac(alt, p);  // the frozen-kernel path — bit-identical
    }
    const AtmosphereField& af = *env->atm;

    // Deck: full air below deck_agl_m AGL (bare sphere at R in R6), fading up.
    // RUNG E16: over the TERRAIN instead, when the field says so and a ground
    // field exists — the ONE elevation query is world::HeightField::radius_at,
    // the same one the crash surface uses (sim/ground.h's anti-fork rule).
    // deck_terrain_relative == false => `deck_ref` is 0.0 and this line is the
    // R6 expression, bit-for-bit.
    const double deck_ref =
        (af.deck_terrain_relative && env->ground != nullptr)
            ? env->ground->radius_at(glm::normalize(position)) - p.R
            : 0.0;
    double one_minus =
        1.0 - atm_falloff(alt - deck_ref - af.deck_agl_m, af.deck_soft_m);

    // Bubble domes: horizontal great-circle edge x vertical ceiling, unioned.
    if (!af.bubbles.empty()) {
        const glm::dvec3 up = glm::normalize(position);
        for (const AtmosphereField::Bubble& b : af.bubbles) {
            const double c = glm::clamp(
                glm::dot(up, glm::normalize(b.center_dir)), -1.0, 1.0);
            const double arc =
                p.R * std::acos(c);  // surface distance to center

            // TRUE-ELLIPSE horizontal radius (conquest, Chad's 2026-07-25
            // ruling): minor_radius_m > 0 replaces the isotropic
            // ground_radius_m with a DIRECTION-DEPENDENT effective radius
            // r_eff(theta) = a*b / sqrt((b*cos)^2 + (a*sin)^2) — the standard
            // polar-form ellipse radius, `a` = ground_radius_m (semi-major),
            // `b` = minor_radius_m (semi-minor), theta = the query point's
            // bearing around the center relative to major_axis. When
            // minor_radius_m == 0 this whole branch is skipped and `radius_h`
            // below is EXACTLY b.ground_radius_m — the pre-ellipse expression,
            // same operations (bit-identical, not just numerically close).
            double radius_h = b.ground_radius_m;
            if (b.minor_radius_m > 0.0) {
                // Guard the arc~0 degenerate: deep inside the dome the
                // bearing is undefined (any direction), so just use the
                // semi-minor radius (a safe, well-defined lower bound; the
                // point is far inside either way this arc is tiny).
                constexpr double kArcEps = 1e-6;
                if (arc < kArcEps) {
                    radius_h = b.minor_radius_m;
                } else {
                    // Re-orthogonalize major_axis against center_dir
                    // (defensive: a hand-authored axis need not be exactly
                    // tangent) and guard |m| ~ 0 -> falls back circular
                    // (treat as a degenerate axis, use ground_radius_m).
                    const glm::dvec3 center_n = glm::normalize(b.center_dir);
                    glm::dvec3 m = b.major_axis -
                                   center_n * glm::dot(b.major_axis, center_n);
                    const double m_len = glm::length(m);
                    if (m_len < 1e-9) {
                        radius_h = b.ground_radius_m;
                    } else {
                        m /= m_len;
                        // Bearing of the query point around the center:
                        // t = the query's own tangent direction at the center.
                        glm::dvec3 t = up - center_n * c;
                        const double t_len = glm::length(t);
                        if (t_len < 1e-12) {
                            radius_h = b.minor_radius_m;
                        } else {
                            t /= t_len;
                            const double cos_th =
                                glm::clamp(glm::dot(t, m), -1.0, 1.0);
                            const double sin_th =
                                std::sqrt(std::max(0.0, 1.0 - cos_th * cos_th));
                            const double a_maj = b.ground_radius_m;
                            const double b_min = b.minor_radius_m;
                            const double denom =
                                std::sqrt((b_min * cos_th) * (b_min * cos_th) +
                                          (a_maj * sin_th) * (a_maj * sin_th));
                            radius_h = (denom > 1e-9) ? (a_maj * b_min / denom)
                                                      : b_min;
                        }
                    }
                }
            }

            // S-domeround (docs/airdome_round_spec.md §1): a superellipse of
            // revolution about the bubble axis, replacing the old
            // independent-axis product (a cylinder: horizontal edge x
            // vertical ceiling multiplied as two separate soft walls, which
            // meets at a hard right-angle CORNER). arc/radius_h and
            // alt_p/H combine in ONE meridional norm `s` so the corner
            // rounds. Reduces EXACTLY to the old expression on both pure
            // axes (alt_p=0 => the horizontal edge; arc=0 => the vertical
            // ceiling with H) — proof in the spec §1.2 — so a purely
            // horizontal or purely vertical probe is untouched bit-for-bit;
            // only the corner moves, by design (spec §5 leg 3 is the test
            // that proves this is not a no-op).
            const double alt_p = std::max(alt, 0.0);
            const double H = (b.dome_h_m > 0.0) ? b.dome_h_m : b.ceiling_m;
            double u;
            if (radius_h <= 0.0 || H <= 0.0) {
                u = 0.0;  // extinct faction: matches today's extinct handling
            } else {
                const double rho = std::sqrt(arc * arc + alt_p * alt_p);
                if (rho <= 0.0) {
                    u = 1.0;  // dead centre at ground: deep inside, u == 1
                } else {
                    const double n =
                        af.dome_exponent > 0.0 ? af.dome_exponent : 3.0;
                    const double s = std::pow(
                        std::pow(arc / radius_h, n) + std::pow(alt_p / H, n),
                        1.0 / n);
                    const double d = rho * (s - 1.0) / s;
                    const double w = std::pow(alt_p / H, n) / std::pow(s, n);
                    const double soft =
                        b.edge_soft_m * (1.0 - w) + b.ceil_soft_m * w;
                    u = atm_falloff(d, soft);
                }
            }
            one_minus *= (1.0 - u);
        }
    }

    // T1 — the tunnel union term: a contained mine holds FULL air regardless of
    // the bubble war (tunnel_staging ruling). Same complement-product
    // identities as the deck/bubbles: inside the net sd<=0 => u_t==1.0 exactly
    // => one_minus
    // == 0.0 => the baseline taper restored exactly; beyond sd>=soft u_t==0.0
    // exactly => bit-identical no-op. The term only RESTORES air the war
    // removed, never exceeds baseline.
    if (env->tunnels != nullptr) {
        one_minus *= (1.0 - atm_falloff(env->tunnels->signed_distance(position),
                                        env->tunnels->soft_m));
    }

    return atm_frac(alt, p) * (1.0 - one_minus);
}

// R6 — SPATIAL density. The position+env counterpart to rho_at(altitude,p);
// null => literally p.rho * atm_frac(altitude(position,p), p).
inline double rho_at(const glm::dvec3& position, const Environment* env,
                     const AircraftParams& p) {
    return p.rho * atm_frac_at(position, env, p);
}

// The floored authority pressure max(q, q_att_floor) — SPEC §7's "defined
// once" expression. q stays recoverable at v -> 0 (the §9.6 prop-wash fib).
inline double q_eff(double q, const AircraftParams& p) {
    return std::max(q, p.q_att_floor);
}

// Compression ramp delta_max_eff(V): full deflection below v_full, linear
// ramp to min_frac at v_redline, clamped beyond. A plant property.
inline double delta_max_eff(double V, const AircraftParams& p) {
    if (V <= p.v_full) return 1.0;
    if (V >= p.v_redline) return p.min_frac;
    return 1.0 + (p.min_frac - 1.0) * (V - p.v_full) / (p.v_redline - p.v_full);
}

// Cl(alpha) = clamp(Cl_alpha * alpha, +/-Cl_max) — linear then a hard stall
// cap.
inline double lift_coeff(double alpha, const AircraftParams& p) {
    return std::clamp(p.Cl_alpha * alpha, -p.Cl_max, p.Cl_max);
}

// ---- MB-flaps primitives (SPEC §0) — defined ONCE here (the H1 four-site
// discipline): the plant (step.cpp), the flaps energy fork-detector's
// config-side reconstruction, and any telemetry all call THESE. Force-only:
// nothing here feeds the torque/authority model.

// Over-speed washout of the flap LIFT shift: 1 at/below flap_wash_lo,
// smoothstep down to exactly 0 at/above flap_wash_hi. Continuous fade (a
// stateless multiplicative gate, the S7-loop-invert precedent — the
// hysteresis rule targets discrete switches). The DRAG tax is deliberately
// NOT washed out: over-speed flaps become speed brakes (the documented
// default felt behavior; auto-retract is the one-line alternative).
inline double flap_lift_frac(double V, const AircraftParams& p) {
    if (V <= p.flap_wash_lo) return 1.0;
    if (V >= p.flap_wash_hi) return 0.0;
    const double t = (V - p.flap_wash_lo) / (p.flap_wash_hi - p.flap_wash_lo);
    return 1.0 - t * t * (3.0 - 2.0 * t);  // 1 - smoothstep
}

// Lift-curve SHIFT (added AFTER lift_coeff's stall clamp, so the BASE curve's
// stall AoA — and the aoa_max <= stall load check — are unchanged; the flap
// raises the ceiling Cl at every AoA, which lowers stall/corner SPEED).
inline double flap_dCl(double flap, double V, const AircraftParams& p) {
    return p.dCl_flap * flap * flap_lift_frac(V, p);
}

// Parasitic drag tax, quadratic in deflection: the combat detent stays cheap,
// landing flap pays full (deflection drag physics, and the energy-game trade).
inline double flap_dCd0(double flap, const AircraftParams& p) {
    return p.dCd0_flap * flap * flap;
}

// Gear drag, linear in extension (a binary device riding a slew).
inline double gear_dCd0(double gear, const AircraftParams& p) {
    return p.dCd0_gear * gear;
}

// The TOTAL lift coefficient the plant actually flies: base curve + the flap
// shift, with the same structural deployment gate step.cpp uses (flap == 0
// returns lift_coeff bit-exactly — no +0.0 corners). Defined ONCE so the
// plant AND the true-n instrument (load_factor below) read the identical
// composition — a load_factor on the bare lift_coeff under-reads the felt G
// by up to ~2 g with landing flaps in the slow turn fight (the MB-flaps diff
// red-team P1-2 H1 fork).
inline double total_lift_coeff(double alpha, double flap, double V,
                               const AircraftParams& p) {
    double cl = lift_coeff(alpha, p);
    if (flap > 0.0) cl += flap_dCl(flap, V, p);
    return cl;
}

// Angle conventions (SPEC §7, body frame +X right +Y up -Z forward):
//   alpha = atan2(-v_b.y, -v_b.z)  positive = nose above velocity
//   beta  = atan2( v_b.x, -v_b.z)  positive = velocity right of nose
// Take a unit body-frame velocity DIRECTION so the v->0 guard (held v-hat)
// composes: callers pass current_vhat() transformed into the body frame.
inline double alpha_of(const glm::dvec3& v_body_dir) {
    return std::atan2(-v_body_dir.y, -v_body_dir.z);
}

inline double beta_of(const glm::dvec3& v_body_dir) {
    return std::atan2(v_body_dir.x, -v_body_dir.z);
}

// THE v-hat guard predicate (SPEC §9.6 channel 1): below eps, hold the
// caller's last valid direction. Sim and controller each guard their OWN
// held copy (the seam forbids sharing state), but the predicate is this one
// expression — a re-derived `speed >= eps` branch anywhere else is an
// H1-class fork (the two layers would disagree at exactly speed == eps).
inline glm::dvec3 guarded_dir(const glm::dvec3& velocity,
                              const glm::dvec3& last_dir, double eps) {
    const double speed = glm::length(velocity);
    return speed >= eps ? velocity / speed : last_dir;
}

// The plant-side v-hat guard (SPEC §7): below v_dir_eps the sim holds the
// last valid direction carried in SimState. Shared by step() and telemetry
// so the instrument can never disagree with the plant about "which way".
inline glm::dvec3 current_vhat(const SimState& s, const AircraftParams& p) {
    return guarded_dir(s.velocity, s.last_vhat, p.v_dir_eps);
}

// World -> body direction, the one composition feeding alpha_of/beta_of at
// every site (plant step, telemetry, control/extract). Hand-composing
// inverse(q) * v at each caller is the same drift surface as a re-derived
// eps branch — one helper, zero forks.
inline glm::dvec3 body_dir_of(const glm::dquat& orientation,
                              const glm::dvec3& world_dir) {
    return glm::inverse(orientation) * world_dir;
}

// AT-18 analytic formula: peak angular acceleration [rad/s^2] at full
// deflection (Input = 1) on one axis — c * max(q, q_att_floor) *
// delta_max_eff(V) / I. This is "alpha_max" in the braking-law sense
// (angular accel), NOT angle of attack.
// Density-agnostic core: the peak-ang-accel FORMULA lives here ONCE; the
// altitude body and the R6 position+env overload both feed it their density,
// so a spatial migration can never fork the formula from the altitude path.
inline double ang_accel_max_at_rho(double c_axis, double I_axis, double V,
                                   double rho, const AircraftParams& p) {
    return c_axis * q_eff(q_dyn(rho, V), p) * delta_max_eff(V, p) / I_axis;
}
// Altitude body — the null-atm / test / harness path (no bubbles present).
inline double ang_accel_max_derived(double c_axis, double I_axis, double V,
                                    double altitude, const AircraftParams& p) {
    return ang_accel_max_at_rho(c_axis, I_axis, V, rho_at(altitude, p), p);
}
// R6 SPATIAL overload — the LIVE braking-margin estimate; density at position
// so the authority estimate matches the plant inside a thinning bubble edge.
inline double ang_accel_max_derived(double c_axis, double I_axis, double V,
                                    const glm::dvec3& position,
                                    const Environment* env,
                                    const AircraftParams& p) {
    return ang_accel_max_at_rho(c_axis, I_axis, V, rho_at(position, env, p), p);
}

// True load factor n = lift / weight = q(V)*S*Cl_total / (m*g) (SPEC §16
// CQ1's "true n"). Defined ONCE here so the controller telemetry and the HUD
// read the IDENTICAL expression — a re-derived copy in render/ would be the
// same H1-class fork as a second alpha/beta or q_eff (S2 lesson). Uses the
// SIGNED total_lift_coeff — the lift the plant ACTUALLY flies, including the
// MB-flaps shift (a bare-lift_coeff n under-reads up to ~2 g with landing
// flaps, exactly in the slow turn fight — diff red-team P1-2); inverted
// flight reads negative n. Unguarded speed: a display/telemetry quantity,
// not a V-division (no vMin needed). `flap` is the SLEWED SimState.flap.
// Density-agnostic core: the true-n FORMULA lives here ONCE (altitude body +
// R6 position+env overload both feed it their density — no spatial fork).
inline double load_factor_at_rho(double alpha, double speed, double rho,
                                 double flap, const AircraftParams& p) {
    return q_dyn(rho, speed) * p.S * total_lift_coeff(alpha, flap, speed, p) /
           (p.mass * p.g);
}
// Altitude body — the null-atm / test / harness path.
inline double load_factor(double alpha, double speed, double altitude,
                          double flap, const AircraftParams& p) {
    return load_factor_at_rho(alpha, speed, rho_at(altitude, p), flap, p);
}
// R6 SPATIAL overload — the LIVE HUD / telemetry n; density at position so the
// G-meter reads the true lift inside a thinning bubble (Fable-BEFORE §5: a
// stale-density instrument at the bubble edge reads as a scripted wall).
inline double load_factor(double alpha, double speed,
                          const glm::dvec3& position, const Environment* env,
                          double flap, const AircraftParams& p) {
    return load_factor_at_rho(alpha, speed, rho_at(position, env, p), flap, p);
}

}  // namespace sim
