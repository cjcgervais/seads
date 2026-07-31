#pragma once

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

// The ballistic solver's max flight time considered (arcade cap). Exposed so
// the scenario loader can assert muzzle_speed is fast enough that (a) a real
// in-range intercept's TTI stays under it and (b) f(t) is strictly decreasing
// (single root, no scan-cell skip) — see load_scenario (the retune tripwire,
// Fable P1-3).
constexpr double kBallisticTMax = 10.0;  // [s]

// Tune data (config/scenario.toml [gunsight]); WWII cannon-class defaults.
struct GunsightParams {
    double muzzle_speed = 850.0;  // [m/s] notional projectile speed
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
};

// The deflection solution. lead_dir is the WORLD unit direction from the
// shooter to the intercept point (where to point the nose). valid = a real
// positive-time intercept exists; in_range = valid AND within max_range. When
// !valid, lead_dir falls back to straight-at-the-target so the pipper stays
// visible, but the on-target counter must never score an unsolved geometry
// (valid gates it).
struct LeadSolution {
    glm::dvec3 lead_dir{0.0, 0.0, -1.0};
    double time_to_intercept = 0.0;
    double range = 0.0;
    bool valid = false;
    bool in_range = false;
};

// Solve the ballistic intercept for the aim direction: find t>0 and unit `n`
// with shooter.pos + (shooter.vel + muzzle*n)*t + 0.5*grav*t^2 == target.pos +
// target.vel*t. `grav` is the world gravity accel at the shooter (= -g*up).
// Fixed-point iteration on t = |RHS(t)|/muzzle where RHS(t) = p + w*t -
// 0.5*grav*t^2, p = target-shooter, w = target.vel-shooter.vel; lead_dir =
// normalize(RHS(t)). Pure; every degenerate case handled (no NaN): bad muzzle
// speed, target on the shooter, non-convergence / no positive-t intercept.
LeadSolution lead_solution(const sim::SimState& shooter,
                           const sim::SimState& target, double muzzle_speed,
                           double max_range, const glm::dvec3& grav);

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
    bool on_target_now = false;  // last tick: nose on the solution + in range
    double range_now = 0.0;      // last tick range to the engaged target [m]
    double ttl_now = 0.0;        // last tick time-to-intercept [s]
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
