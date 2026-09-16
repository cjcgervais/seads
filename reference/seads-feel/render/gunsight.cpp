#include "render/gunsight.h"

#include <cmath>
#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>  // glm::conjugate (world→body rotation)

#include "weapon/ballistics.h"  // apply_drag + harmonization_rise — SINGLE-SOURCE
                                 // (Fable P0: pipper + round use ONE drag law,
                                 //  and the cant correction is ONE source: weapon::harmonization_rise)

namespace render {

LeadSolution lead_solution(const sim::SimState& shooter,
                           const sim::SimState& target, double muzzle_speed,
                           double max_range, const glm::dvec3& grav,
                           double drag_k, double fire_dt,
                           double gp_convergence_range,
                           const glm::dvec3& gp_hub_muzzle,
                           double gp_hit_radius_m) {
    LeadSolution sol;
    const glm::dvec3 p = target.position - shooter.position;  // shooter->target
    const glm::dvec3 v_tgt = target.velocity;
    const glm::dvec3 v_s   = shooter.velocity;
    const double range = glm::length(p);
    sol.range = range;

    // Fallback aim: straight at the target NOW (pipper stays visible), but
    // valid stays false so the counter never scores an unsolved geometry.
    sol.lead_dir = range > 1e-9
                       ? p / range
                       : shooter.orientation * glm::dvec3{0.0, 0.0, -1.0};

    if (muzzle_speed <= 0.0 || range < 1e-6) return sol;  // degenerate: !valid

    // --------------------------------------------------------------------------
    // FIX 1 (Fable P0): WORLD-FRAME SOLVE.
    //
    // The round is spawned at world velocity  v_world = v_s + a*u  where u is
    // the world muzzle direction and a is the round's world launch SPEED along
    // u (a >= 0).  Quadratic drag acts on the WORLD velocity, so the relative-
    // frame solve (old code: dragged_range with v0=muzzle_speed) is only exact
    // when the shooter is at rest.  For a moving shooter the two diverge by
    // ~6.5 m at 400 m in a 150 m/s dogfight.
    //
    // WORLD-FRAME ITERATION (Fable §FIX1, converges in ~5 iters):
    //   I = p + v_tgt*t - droop(t)*grav_hat  // intercept with gravity droop
    //   u = normalize(I)
    //   a = dot(u,v_s) + sqrt(dot(u,v_s)^2 - (|v_s|^2 - m^2))
    //       // world launch speed: ensures |v_s + a*u| >= m, exact for vacuum
    //   t = (exp(k*|I|) - 1) / (k*a)   // dragged TOF at world launch speed a
    // with FIX-2 lag correction.
    //
    // HONEST PIPPER (Task A, iter-8; Chad's ruling):
    //   The pipper accounts for gravity droop so a round fired when the nose is
    //   on the pipper HITS at any in-envelope range. HOWEVER, the gun is physically
    //   canted up by Δ = weapon::harmonization_rise(...) for the primary cannon,
    //   which already compensates droop at convergence_range. To avoid double-
    //   compensating, we subtract the cant's KNOWN vertical rise from the solve.
    //
    //   At range r, the cant contributes:  rise_cant = (r / conv) * Δ
    //   Full droop at t:                   droop(t) = dragged_droop_dist(t, a, k, g)
    //   Net pipper hold-over:              net_droop = droop(t) - rise_cant
    //
    //   This means:
    //     • At convergence_range (r = conv): rise_cant ≈ droop(t_conv), net ≈ 0
    //       → pipper ≈ boresight (same as Model A behavior)
    //     • Beyond convergence_range: droop grows faster than rise_cant,
    //       net > 0 → pipper automatically holds ABOVE boresight by the residual
    //     • g = 0 or g_scalar = 0: droop = 0, rise_cant = 0 → pure-lead (fallback)
    //
    //   Single-sourced: Δ from weapon::harmonization_rise (SAME function the battery
    //   uses); g_scalar from GunsightParams (threaded from ap.g). The cant angle
    //   approximation θ_h ≈ Δ/conv is valid for small angles (Δ ≈ 2.4 m at 500 m
    //   → θ ≈ 4.8 mrad = 0.28 deg — well within small-angle regime).
    //   The hub cannon is the single pipper reference (5 muzzles, one reticle;
    //   the MG's slightly-different cant is an accepted approximation).
    //
    // FIX 2 (Fable P1): DISCRETIZATION LAG CORRECTION (KEPT).
    //   weapon::advance uses right-Riemann drag: v *= 1/(1+k|v|dt), then
    //   pos += v_new*dt.  The discrete round travels LESS distance in a given
    //   time than the continuous model predicts.  Adding the half-step lag to the
    //   effective distance corrects the TOF so the pipper and the round agree.
    //   At k=0 the lag is 0 (no drag → no lag).
    //
    // At k=0 the whole formula reduces exactly to the vacuum intercept solve.
    // Discriminant < 0 means the target is unreachable (v_s component exceeds
    // muzzle_speed on that axis): fall back to straight-at-target, !valid.
    // --------------------------------------------------------------------------

    const double m  = muzzle_speed;
    const double k  = drag_k;
    const double vs2 = glm::dot(v_s, v_s);  // |v_s|^2

    // FIX 4 (Fable P2): far-envelope cap. The iteration diverges when the
    // target recedes faster than the bullet; we catch divergence when t_new
    // exceeds the cap. Cap at the time to travel 1.1*max_range at muzzle_speed:
    // for in-range intercepts the solved t is always well below this, and it
    // stays within the monotone region where the dragged distance is strictly
    // monotone. For out-of-range targets the iteration is still run to produce
    // a valid lead direction (the pipper shows even beyond max_range; in_range
    // flags the distinction). Absolute ceiling: kBallisticTMax (the projectile
    // lifetime, also used by weapon::advance).
    const double kTMax = kBallisticTMax;

    // GRAVITY: unit vector opposite to grav (the "up" direction for droop).
    // grav = -g * up_world, so grav_hat = normalize(grav) = -up_world.
    // When grav ≈ 0 or g_scalar ≈ 0, the droop term is zero.
    const double g_mag = glm::length(grav);
    const bool use_droop = (g_mag > 1e-9);
    // grav_hat points DOWN (same direction as grav = -g * up).
    const glm::dvec3 grav_hat = use_droop ? (grav / g_mag) : glm::dvec3{0.0};

    // Cant correction: the FULL body-frame aim offset for the PRIMARY (hub)
    // cannon — SINGLE-SOURCED with the round via weapon::harmonization_offset
    // (the SAME function weapon::muzzle_world_dir uses). off folds the gravity
    // droop rise Δg AND the velocity-inheritance cancel (Fable C6c), so the
    // pipper subtracts EXACTLY what the gun is canted by — no fork. Rotated into
    // WORLD (shooter.orientation * off) it tracks the airframe at any bank, which
    // fixes the old world-up scalar cant that forked banked shots (Fable P1-1).
    // When g_scalar = 0 (vacuum / no conv), off = 0 exactly (vacuum seam).
    const double g_scalar = (gp_convergence_range > 0.0 && use_droop) ? g_mag : 0.0;
    const glm::dvec3 v_body_sh =
        glm::conjugate(shooter.orientation) * shooter.velocity;  // world→body
    const glm::dvec3 cant_off = (g_scalar > 0.0)
        ? weapon::harmonization_offset(
              gp_hub_muzzle, gp_convergence_range, m, k, g_scalar, fire_dt,
              v_body_sh)
        : glm::dvec3{0.0};
    // World-frame cant vector at the convergence point (body +Y/+X rotated by
    // the airframe attitude). At identity + rest this is (0, Δg, 0) = Δg·(−grav_hat),
    // so the wings-level rest solve is bit-identical to the old scalar form.
    const glm::dvec3 cant_world = shooter.orientation * cant_off;

    // FIX 2: discretization lag — thin wrapper; guards match the helper exactly.
    auto lag_dist = [&](double t, double a_world) -> double {
        return drag_lag_dist(t, a_world, k, fire_dt);
    };

    // World-frame iteration: start at t = |p|/m.
    // Convergence: each step refines t by computing how long the dragged round
    // takes to reach the predicted intercept. Divergence (t growing beyond kTMax)
    // means the target is unreachable — return !valid immediately.
    double t_iter = range / m;
    glm::dvec3 dir_iter = sol.lead_dir;  // initial guess: straight at target
    bool converged = false;
    double a_final = m;  // world launch speed at convergence (for predicted_miss)

    for (int iter = 0; iter < 40; ++iter) {
        // Honest-pipper intercept: include droop, then subtract cant contribution.
        // At range r to intercept, cant's rise ≈ (r / convergence_range) * Δ
        // (small-angle: cant angle θ ≈ Δ/conv, rise at range r ≈ r*θ).
        // droop is along grav_hat (DOWN), so we add -grav_hat * net_droop upward.
        // net_droop = droop(t) - cant_rise. If net_droop < 0 (over-corrected
        // by cant at sub-convergence range), pipper dips below boresight —
        // physically correct (cant over-compensates; rounds hit HIGH there).
        const glm::dvec3 I_lead = p + v_tgt * t_iter;
        const double I_lead_len = glm::length(I_lead);
        double droop_scalar = 0.0;
        glm::dvec3 cant_vec{0.0};
        if (use_droop && I_lead_len > 1e-9) {
            // Approximate world launch speed for droop calculation (use m as
            // first estimate; iterate refines t which refines a_new below).
            droop_scalar = dragged_droop_dist(t_iter, a_final, k, g_mag);
            // Cant contribution at the intercept range, in the WORLD frame
            // (body-correct at any bank): scales linearly from 0 at the muzzle
            // to cant_world at conv. Single-sourced with the round's physical
            // cant via harmonization_offset (Fable P1-1 frame fix + C6c).
            cant_vec = (gp_convergence_range > 0.0)
                ? (I_lead_len / gp_convergence_range) * cant_world
                : glm::dvec3{0.0};
        }
        // Intercept: target lead + gravity droop (UP, along -grav_hat) MINUS the
        // gun cant (world vector). The nose/pipper aims BELOW the gun by the cant
        // so a canted round lands on target; subtracting the full cant vector
        // (not a world-up scalar) keeps the pipper honest at any bank.
        const glm::dvec3 I = I_lead + (-grav_hat) * droop_scalar - cant_vec;
        const double I_len = glm::length(I);
        if (I_len < 1e-9) {
            converged = true;
            break;  // target on top of shooter (degenerate but "converged")
        }

        const glm::dvec3 u = I / I_len;

        // Round's world launch speed along u.
        // From |v_s + a*u|^2 = m^2: a^2 + 2*a*dot(u,v_s) + vs2 - m^2 = 0.
        // The LARGER root gives the physical (prograde) solution.
        const double b     = glm::dot(u, v_s);
        const double disc  = b * b - (vs2 - m * m);
        if (disc < 0.0) return sol;  // unreachable: !valid, fallback kept
        const double a_new = b + std::sqrt(disc);
        if (a_new <= 0.0) return sol;  // unphysical: !valid
        a_final = a_new;

        // FIX 2: add lag to effective distance so the continuous TOF formula
        // returns the corrected (larger) time matching the discrete integrator.
        const double eff_dist = I_len + lag_dist(t_iter, a_new);

        // Dragged TOF over the effective world distance at launch speed a.
        // Calls the SINGLE-SOURCED helper (render/gunsight.h); no expm1 here.
        const double t_new = dragged_tof(eff_dist, a_new, k);

        // Divergence or no-physical-solution: target unreachable.
        if (t_new <= 0.0 || t_new > kTMax) return sol;  // !valid, fallback kept

        // Muzzle world direction.
        const glm::dvec3 launch_world = a_new * u - v_s;
        const double lw_len = glm::length(launch_world);
        if (lw_len > 1e-9) dir_iter = launch_world / lw_len;

        // Convergence check: t changed by less than 1 ns (sub-mm at any speed).
        if (std::abs(t_new - t_iter) < 1e-9) {
            t_iter    = t_new;
            converged = true;
            break;
        }
        t_iter = t_new;
    }

    // Final validity: iteration converged to a positive time with a good direction.
    if (converged && t_iter > 1e-9 && glm::length(dir_iter) > 1e-9) {
        sol.lead_dir = dir_iter;
        sol.time_to_intercept = t_iter;
        sol.valid = true;
        sol.in_range = range <= max_range;

        // Predicted miss: residual net droop at the solved range — the error
        // that would result if a round were fired right now. Used by the
        // out-of-envelope red pipper cue (Task B.3).
        // net_droop > 0: pipper already compensates → round hits on aim
        // |predicted_miss| ≈ 0 when the cant-droop balance is good.
        // Re-derive at final t_iter and a_final.
        if (use_droop) {
            const double droop_f = dragged_droop_dist(t_iter, a_final, k, g_mag);
            // Cant's UP (−grav_hat) component at the INTERCEPT range (P2 fix:
            // match droop_f's t_iter geometry rather than the current LOS range).
            const double r_icept = glm::length(p + v_tgt * t_iter);
            // The out-of-envelope CUE must reflect ONLY the GRAVITY holdover the
            // cant covers. The inheritance-cancel part of cant_world produces NO
            // miss for a nose-on-pipper shot (it aligns DEPARTURE with the
            // sightline), so folding it into a miss metric fires a FALSE red
            // pipper under hard G — exactly when the pilot pulls lead (Fable P1,
            // iter-15 red-team). Use the gravity Δg-only vector here; the AIM
            // solve above keeps subtracting the FULL cant_world (correct).
            const double dg_cue = (g_scalar > 0.0)
                ? weapon::harmonization_rise(gp_hub_muzzle, gp_convergence_range,
                                             m, k, g_scalar, fire_dt)
                : 0.0;
            const double cant_up = glm::dot(
                shooter.orientation * glm::dvec3{0.0, dg_cue, 0.0}, -grav_hat);
            const double cant_f  = (gp_convergence_range > 0.0)
                ? (r_icept / gp_convergence_range) * cant_up
                : 0.0;
            // predicted_miss is the uncompensated droop (positive = round hits
            // low; negative = round hits high i.e. over-compensated).
            // The pipper aims to make this ZERO — so after cant correction the
            // residual is near zero. This is non-zero only when the pipper solve
            // hasn't fully converged with the cant term (e.g. very long range).
            sol.predicted_miss = droop_f - cant_f;
        }
        // out_of_envelope: true when !valid OR residual droop exceeds hit radius.
        // The pipper compensates net_droop in the solve, so predicted_miss
        // should be near-zero for in-envelope shots. Out_of_envelope fires red
        // when the geometry is unreachable (!valid), or when gp_hit_radius_m > 0
        // and |predicted_miss| > hit_radius_m (shot still misses even with the
        // cant-droop correction applied — e.g. extreme range or grav mismatch).
        sol.out_of_envelope = (!sol.valid) ||
            (gp_hit_radius_m > 0.0 &&
             std::abs(sol.predicted_miss) > gp_hit_radius_m);
    }
    return sol;
}

bool on_target(const glm::dvec3& shooter_nose, const LeadSolution& sol,
               double hit_cone_cos) {
    if (!sol.valid || !sol.in_range) return false;
    const double nlen = glm::length(shooter_nose);
    if (nlen < 1e-9) return false;
    return glm::dot(shooter_nose / nlen, sol.lead_dir) >= hit_cone_cos;
}

void meter_tick(OnTargetMeter& m, const sim::SimState& shooter,
                const glm::dvec3& shooter_nose, const sim::SimState* target,
                const GunsightParams& gp, const glm::dvec3& grav) {
    if (target == nullptr) {
        m.has_target = false;
        m.on_target_now = false;
        m.range_latched = false;  // disengaged: drop the in-range latch
        m.closure_rate_now = 0.0;
        m.out_of_envelope_now = false;
        return;  // no bandit engaged: neither counter advances (idle time)
    }
    m.has_target = true;
    const LeadSolution sol =
        lead_solution(shooter, *target, gp.muzzle_speed, gp.max_range, grav,
                      gp.drag_k, gp.fire_dt,
                      gp.convergence_range, gp.hub_muzzle_body,
                      gp.hit_radius_m);
    m.lead_now = sol.lead_dir;
    m.range_now = sol.range;
    m.ttl_now = sol.time_to_intercept;
    m.predicted_miss_now = sol.predicted_miss;
    m.out_of_envelope_now = sol.out_of_envelope || !sol.valid;
    // Closure rate (Task E): dot(v_relative, LOS_unit). LOS = target→shooter,
    // so positive closure_rate = APPROACHING (use the SEADS convention that
    // negative = opening / receding, positive = closing).
    // v_relative = target.velocity - shooter.velocity; LOS_unit = (pos_target - pos_shooter) / range.
    // closure_rate = -dot(v_relative, LOS_unit) where LOS_unit points shooter→target.
    // Positive means gap is closing (range decreasing), matching pilot intuition.
    if (sol.range > 1e-6) {
        const glm::dvec3 los_unit =
            (target->position - shooter.position) / sol.range;  // shooter→target
        const glm::dvec3 v_rel = target->velocity - shooter.velocity;
        // Positive = target moving away (opening); negate for "closure" (positive = closing).
        m.closure_rate_now = -glm::dot(v_rel, los_unit);
    } else {
        m.closure_rate_now = 0.0;
    }
    // In-gun-range with HYSTERESIS (enter at max_range, exit at 1.1*max_range)
    // so the green cue + the TOT denominator don't flicker for a target
    // orbiting the boundary (Fable P1-2). on_target reads this latched
    // in_range, not the raw.
    const double range_out =
        m.range_latched ? gp.max_range * 1.1 : gp.max_range;
    m.range_latched = sol.valid && sol.range <= range_out;
    LeadSolution scored = sol;
    scored.in_range = m.range_latched;
    m.on_target_now = on_target(shooter_nose, scored, gp.hit_cone_cos);
    // Count the denominator only when in (hysteretic) gun range — a real firing
    // opportunity; else the % is diluted by the out-of-range approach.
    if (m.range_latched) {
        m.total_ticks += 1;
        if (m.on_target_now) m.hit_ticks += 1;
    }
}

}  // namespace render
