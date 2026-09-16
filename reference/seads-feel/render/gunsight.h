#pragma once

#include <cmath>

#include <glm/glm.hpp>

#include "sim/state.h"

// Lead-angle gunsight (SPEC §0 S8-drone; Chad's ruling "calculate the lead
// angle for a hit"). A READ-ONLY deflection-shooting instrument: given a
// target's position + velocity and a notional muzzle speed, solve for where to
// point the nose so a shot fired NOW would intercept, draw a lead pipper there,
// and (Stage 3) count time-on-target. NO projectile is spawned, NO damage — it
// honors the no-weapons scope fence (TEACHING A.2); it only computes and shows
// the solution. Joins the honest ledger as an INSTRUMENT, never a gate (SPEC
// §14).
//
// PURE and raylib-free (render_core): lead_solution / on_target are pure math,
// so the app (draw + Stage-3 meter) and the tests share the same code.
//
// BALLISTIC (S8-drone follow-up, Chad's "specific velocity + gravity drop"):
// the bullet leaves along the aim at muzzle_speed RELATIVE to the shooter
// (inherits shooter.velocity) and falls under gravity `grav` (= g *
// gravity_dir(shooter), world) over its flight. The intercept is solved
// iteratively (no closed form with gravity). At ~850 m/s the drop is a minor
// correction at gun range (faithful); the target-motion + own-velocity lead
// dominates. (grav = 0 reduces to the straight-line non-inheriting solve.)

namespace render {

// The ballistic solver's absolute max flight time (arcade cap). Exposed so
// the scenario loader can assert muzzle_speed is fast enough to cover the max
// closing speed — the retune tripwire (Fable P1-3 / FIX 4). The world-frame
// iteration in lead_solution further caps its search at the TOF to reach
// 1.1*max_range (where the monotone property provably holds under drag), so
// the solver never chases the saturated far-envelope. kBallisticTMax is a
// hard ceiling for weapon::advance (projectile lifetime), not the search cap.
constexpr double kBallisticTMax = 10.0;  // [s]

// ---------------------------------------------------------------------------
// SINGLE-SOURCED ballistic helpers (Fable spec Q3 / P0 provenance).
// These three are the ONLY place log1p and expm1 appear in the repo; both
// tokens exist EXACTLY ONCE each — here. weapon::harmonization_rise and
// render::lead_solution both call these; no copy of the drag law lives
// anywhere else (grep log1p/expm1 must hit each token once, in this file).
// ---------------------------------------------------------------------------

// Dragged time-of-flight over distance d at world launch speed a with
// drag constant k.  FIX-2 lag uses this to bump the effective distance
// before calling dragged_tof.
//   k > 0:  t = expm1(k*d) / (k*a)     [continuous quadratic drag TOF]
//   k <= 0: t = d / a                    [vacuum; expm1 branch undefined at k=0]
// Guards: k<=0 or a<=0 -> vacuum formula.
inline double dragged_tof(double d, double a, double k) {
    if (k <= 0.0 || a <= 0.0) return (a > 0.0) ? d / a : 0.0;
    return std::expm1(k * d) / (k * a);
}

// Dragged gravity droop SCALAR (distance fallen along grav_dir) for a round
// with world launch speed a and drag k after flight time t under gravity mag g.
//   k > 0, a > 0:  D = (g/alpha^2) * (S^2/4 + S/2 - 0.5*log1p(S))
//                  where alpha = k*a, S = alpha*t   [FIX-3 Fable P1]
//   k<=0 or a<=0:  D = 0.5*g*t^2                   [vacuum, exact limit]
// The CALLER multiplies by grav_dir to get the vector; this returns the
// scalar so weapon::harmonization_rise (which has a scalar +Y gravity) can
// also call it — no vec3 coupling.
// Property: g=0 => D=0 exactly (the vacuum seam, bit-identical; Fable Q1).
inline double dragged_droop_dist(double t, double a, double k, double g) {
    if (g <= 0.0) return 0.0;
    if (k <= 0.0 || a <= 0.0) return 0.5 * g * t * t;
    const double alpha = k * a;
    const double S     = alpha * t;
    // D(t) = (g/alpha^2) * (S^2/4 + S/2 - 0.5*log1p(S))
    return (g / (alpha * alpha)) * (0.25 * S * S + 0.5 * S - 0.5 * std::log1p(S));
}

// FIX-2 discretization lag distance: weapon::advance uses right-Riemann
// drag (vel *= 1/(1+k|v|dt)), so the discrete round travels less distance
// per unit time than the continuous model.  Adding this to the effective
// range before dragged_tof returns the corrected (larger) TOF that the
// discrete integrator actually needs.
//   lag = 0.5 * fire_dt * a * (1 - 1/(1+S))  where S = expm1(k*a*t) [k>0]
//   k=0: lag = 0  (no drag -> no lag)
// Guards: k<=0 or a<=0 or fire_dt<=0 -> 0.
inline double drag_lag_dist(double t, double a, double k, double fire_dt) {
    if (k <= 0.0 || a <= 0.0 || fire_dt <= 0.0) return 0.0;
    const double v_t = a / (1.0 + k * a * t);
    return 0.5 * fire_dt * (a - v_t);
}

// Tune data (config/scenario.toml [gunsight] + derived from world.toml [guns]);
// WWII cannon-class defaults.
struct GunsightParams {
    double muzzle_speed = 805.0;  // [m/s] notional projectile speed
    double hit_cone_cos =
        0.9994;                // cos(hit cone half-angle); the loader stores
                               //   cos(hit_cone_deg) (~2 deg default)
    double max_range = 500.0;  // [m] effective gun range (in-range for a hit)
    double track_range =
        2000.0;  // [m] engagement range: a bandit inside this
                 //   (and within the cone) is the ENGAGED
                 //   target — the pipper shows and the meter
                 //   denominator runs. Beyond it there is no
                 //   pipper (a 14 km-away bandit isn't engaged).
    double track_cone_cos = 0.707;  // cos(engagement half-angle off the nose):
                                    //   a bandit 85 deg off boresight is not a
                                    //   shot (Fable P2-8). Loader: cos(deg).
    double drag_k = 0.0;  // [1/m] quadratic drag constant for the PRIMARY
                          //   weapon (cannon k=0.00080, iter-7 arcade retune); 0 = vacuum (Fable
                          //   P0: SINGLE-SOURCED from world.toml [guns]
                          //   cannon_drag_k_per_m via load_scenario — NOT read
                          //   from scenario.toml, so the pipper and round are
                          //   BY CONSTRUCTION on ONE drag law, no silent fork).
    double fire_dt = 1.0 / 120.0;  // [s] sim tick — the discrete step used by
                                   //   weapon::advance (= AircraftParams::sim_dt,
                                   //   threaded in by load_scenario). The pipper
                                   //   corrects for the right-Riemann-sum lag of
                                   //   the integrator by half a step (FIX 2).
    // Cant-aware honest pipper (Task A, iter-8): the convergence_range and
    // hub_muzzle_body feed weapon::harmonization_rise (the SINGLE droop-source) so
    // lead_solution can subtract the gun cant's own droop contribution and leave
    // only the RESIDUAL droop the pilot must hold over. Single-sourced from
    // world.toml [guns]; threaded by load_scenario / main.cpp. Default =
    // pure-lead fallback (convergence_range=0 disables, harmonization_rise not called).
    double convergence_range = 500.0;  // [m] harmonization range (world.toml)
    glm::dvec3 hub_muzzle_body{0.0, 0.0, -2.870};  // hub cannon muzzle body pos
    // Out-of-envelope pipper cue (Task B.3): the hit radius used to determine
    // whether the predicted net droop exceeds what the drone's hit sphere can
    // absorb. If |predicted_miss| > hit_radius_m → red pipper (out of lethal
    // envelope). Single-sourced from drone.hit_radius_m in scenario.toml.
    double hit_radius_m = 8.0;  // [m] hit sphere radius (drone collision radius)
};

// The deflection solution. lead_dir is the WORLD unit direction from the
// shooter to the intercept point (where to point the nose). valid = a real
// positive-time intercept exists; in_range = valid AND within max_range. When
// !valid, lead_dir falls back to straight-at-the-target so the pipper stays
// visible, but the on-target counter must never score an unsolved geometry
// (valid gates it).
// predicted_miss: estimated vertical miss at intercept range after cant correction
//   (the residual droop the cant does NOT cover — positive = high, negative = low).
//   Used for the out-of-envelope red pipper cue (Task B.3).
// out_of_envelope: true when !valid OR |predicted_miss| > hit_radius (round
//   won't connect at the current range even if aim is perfect).
struct LeadSolution {
    glm::dvec3 lead_dir{0.0, 0.0, -1.0};
    double time_to_intercept = 0.0;
    double range = 0.0;
    bool valid = false;
    bool in_range = false;
    double predicted_miss = 0.0;   // [m] net residual droop at intercept (cant-corrected)
    bool out_of_envelope = false;  // shot won't connect at this range (red pipper cue)
};

// Solve the ballistic intercept for the aim direction with quadratic drag.
// Solves in the WORLD frame (FIX 1, Fable P0): the intercept geometry accounts
// for the shooter's own velocity so the solved lead matches the WORLD-velocity
// round exactly.  drag_k=0 reduces to the vacuum solve (guarded).  `grav` is
// the world gravity accel at the shooter (= -g*up).  `fire_dt` is the sim tick
// used by weapon::advance — the solver corrects for its right-Riemann-sum lag
// (FIX 2).  Pure; every degenerate case handled (no NaN): bad muzzle/drag,
// target on the shooter, unreachable geometry (discriminant < 0).
// Single-sourced with weapon::advance via weapon::apply_drag — Fable P0.
//
// Honest-pipper (Task A, iter-8): the cant-aware droop correction.
// gp_convergence_range > 0 and gp_hub_muzzle enable the cant subtraction:
//   net_droop = dragged_droop(t) - (range/conv) * harmonization_rise(hub)
// so the pipper auto-holds-over by RESIDUAL droop the cant no longer covers.
// At convergence_range the two cancel and pipper ≈ boresight.
// At g=0 (grav≈0) droop=0 and cant_delta=0 → pure-lead (fallback, unchanged).
// Defaults: 0 / zero-vec → pure-lead mode (all existing test callers unchanged).
// gp_hit_radius_m > 0: used to set out_of_envelope when |predicted_miss| exceeds it.
LeadSolution lead_solution(const sim::SimState& shooter,
                           const sim::SimState& target, double muzzle_speed,
                           double max_range, const glm::dvec3& grav,
                           double drag_k = 0.0, double fire_dt = 1.0 / 120.0,
                           double gp_convergence_range = 0.0,
                           const glm::dvec3& gp_hub_muzzle = glm::dvec3{0.0},
                           double gp_hit_radius_m = 0.0);

// "Guns on the solution": the nose (boresight) points within the hit cone of a
// VALID, in-range lead solution. shooter_nose is the world nose direction
// (normalized defensively).
bool on_target(const glm::dvec3& shooter_nose, const LeadSolution& sol,
               double hit_cone_cos);

// Time-on-target meter (Stage 3): the tracking instrument. Advanced ONCE per
// SIM tick (in app::tick, so it is frame-rate independent — the AT-9
// discipline, not per render frame), against the ENGAGED target (the nearest
// bandit ahead AND within track_range, selected by the caller which sees the
// fleet). total_ticks counts ticks the engaged bandit is within GUN range (a
// real firing opportunity); hit_ticks counts ticks the nose is on the solution
// — so fraction() = "of the time a bandit was in gun range ahead, what % did I
// have guns on it" (a MEANINGFUL number, not diluted by 14 km approaches).
// on_target_now / lead_now / range_now are the last-tick snapshot the HUD reads
// (SINGLE source: the pipper and the % come from here, never a render-time
// recompute). An instrument, never a gate (SPEC §14).
struct OnTargetMeter {
    long long total_ticks = 0;  // ticks with a target engaged
    long long hit_ticks = 0;    // ticks on the solution (in cone + range)
    // Hysteresis state (the "every gate hysteretic" rule, Fable P1-2): the
    // engaged bandit LATCHES (the selection caller keeps it until it clearly
    // leaves or a challenger is materially closer) so the pipper doesn't strobe
    // between crisscrossing bandits; the gun-range in/out cue latches too.
    int engaged_index = -1;      // the currently-engaged fleet slot, or -1
    bool range_latched = false;  // last tick's hysteretic in-gun-range state
    bool has_target = false;     // a target was engaged last tick (draw pipper)
    bool on_target_now = false;    // last tick: nose on the solution + in range
    double range_now = 0.0;       // last tick range to the engaged target [m]
    double ttl_now = 0.0;         // last tick time-to-intercept [s]
    double predicted_miss_now = 0.0;  // last tick net residual droop [m] (Task B.3)
    bool out_of_envelope_now = false;  // last tick out-of-lethal-envelope flag (red pipper)
    // Closure rate: negative = closing (range decreasing), positive = opening.
    // Derived from the RELATIVE velocity component along the LOS (read-only,
    // no clock/RNG). closure_rate = dot(v_tgt - v_shooter, LOS_unit).
    // Positive = receding, negative = approaching. Zero when no target. (Task E)
    double closure_rate_now = 0.0;  // [m/s] along LOS (+ = opening, - = closing)
    glm::dvec3 lead_now{0.0, 0.0, -1.0};  // last tick lead direction (world)
    double fraction() const {
        return total_ticks > 0 ? static_cast<double>(hit_ticks) /
                                     static_cast<double>(total_ticks)
                               : 0.0;
    }
};

// Advance the meter one sim tick against `target` (nullptr = no bandit engaged
// this tick -> counts neither, clears has_target). shooter_nose is the world
// nose direction; `grav` the world gravity accel at the shooter. PURE.
void meter_tick(OnTargetMeter& m, const sim::SimState& shooter,
                const glm::dvec3& shooter_nose, const sim::SimState* target,
                const GunsightParams& gp, const glm::dvec3& grav);

}  // namespace render
