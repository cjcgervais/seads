#pragma once

#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "sim/params.h"
#include "sim/state.h"

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
inline double ang_accel_max_derived(double c_axis, double I_axis, double V,
                                    double altitude, const AircraftParams& p) {
    return c_axis * q_eff(q_dyn(rho_at(altitude, p), V), p) *
           delta_max_eff(V, p) / I_axis;
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
inline double load_factor(double alpha, double speed, double altitude,
                          double flap, const AircraftParams& p) {
    return q_dyn(rho_at(altitude, p), speed) * p.S *
           total_lift_coeff(alpha, flap, speed, p) / (p.mass * p.g);
}

}  // namespace sim
