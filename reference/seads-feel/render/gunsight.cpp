#include "render/gunsight.h"

#include <cmath>
#include <glm/geometric.hpp>

namespace render {

LeadSolution lead_solution(const sim::SimState& shooter,
                           const sim::SimState& target, double muzzle_speed,
                           double max_range, const glm::dvec3& grav) {
    LeadSolution sol;
    const glm::dvec3 p = target.position - shooter.position;  // shooter->target
    const glm::dvec3 w =
        target.velocity - shooter.velocity;  // relative (inherit)
    const double range = glm::length(p);
    sol.range = range;

    // Fallback aim: straight at the target NOW (pipper stays visible), but
    // valid stays false so the counter never scores an unsolved geometry.
    sol.lead_dir = range > 1e-9
                       ? p / range
                       : shooter.orientation * glm::dvec3{0.0, 0.0, -1.0};

    if (muzzle_speed <= 0.0 || range < 1e-6) return sol;  // degenerate: !valid

    // The shooter->intercept vector at time t (RHS) and the miss function
    // f(t) = |RHS(t)| - muzzle*t. A hit is f(t)=0. The -0.5*grav*t^2 term (grav
    // points down) raises the aim to lead the drop; -shooter.velocity in `w`
    // gives velocity inheritance.
    auto rhs = [&](double t) { return p + w * t - 0.5 * grav * (t * t); };
    auto f = [&](double t) { return glm::length(rhs(t)) - muzzle_speed * t; };

    // f(0) = range > 0. March for the FIRST sign change (the EARLIEST intercept
    // — a fixed-point/Newton seeded near a later root would find the wrong
    // one), then bisect. No crossing within t_max => the target outruns the
    // bullet (receding faster) => unsolved. Robust for every geometry, no NaN,
    // no multi-root ambiguity.
    constexpr double kTMax = kBallisticTMax;  // [s] max flight time considered
    constexpr int kScan = 256;  // 0.039 s cells: fine enough that a real
                                // gun-range intercept's f<0 excursion is never
                                // stepped over (the loader pins muzzle_speed so
                                // f stays monotone within kTMax — Fable P1-3)
    double t_lo = 0.0, t_hi = -1.0;
    for (int i = 1; i <= kScan; ++i) {
        const double t = kTMax * static_cast<double>(i) / kScan;
        if (f(t) <= 0.0) {
            t_hi = t;  // bracket [t_lo, t_hi], f(t_lo) > 0 >= f(t_hi)
            break;
        }
        t_lo = t;
    }
    if (t_hi < 0.0) return sol;  // no intercept within t_max: !valid, fallback

    for (int i = 0; i < 50; ++i) {
        const double tm = 0.5 * (t_lo + t_hi);
        if (f(tm) > 0.0)
            t_lo = tm;
        else
            t_hi = tm;
    }
    const double t = 0.5 * (t_lo + t_hi);
    const glm::dvec3 lead = rhs(t);
    const double llen = glm::length(lead);
    if (t > 1e-9 && llen > 1e-9) {
        sol.lead_dir = lead / llen;
        sol.time_to_intercept = t;
        sol.valid = true;
        sol.in_range = range <= max_range;
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
        return;  // no bandit engaged: neither counter advances (idle time)
    }
    m.has_target = true;
    const LeadSolution sol =
        lead_solution(shooter, *target, gp.muzzle_speed, gp.max_range, grav);
    m.lead_now = sol.lead_dir;
    m.range_now = sol.range;
    m.ttl_now = sol.time_to_intercept;
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
