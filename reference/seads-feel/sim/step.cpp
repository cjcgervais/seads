#include "sim/step.h"

#include <algorithm>

#include "sim/aero.h"
#include "sim/invariants.h"
#include "sim/world.h"

namespace sim {

SimState step(const SimState& state, const Inputs& inputs,
              const AircraftParams& p, double dt) {
    assert_state_valid(state);

    SimState next = state;

    // Throttle: the instructor is passthrough (SPEC §9.8); the PLANT slews
    // the engine toward the commanded target — spool is physics, not UI.
    const double throttle_cmd =
        std::clamp(static_cast<double>(inputs.throttle), 0.0, 1.0);
    const double slew_cap = p.throttle_slew_rate * dt;
    next.throttle = state.throttle + std::clamp(throttle_cmd - state.throttle,
                                                -slew_cap, slew_cap);

    // Flaps/gear (MB-flaps, SPEC §0): the same slew pattern — deploy is
    // physics, not UI. At a 0 command from a clean state both stay exactly
    // 0.0 (clamp(0-0) == 0), so the undeployed plant is bit-identical.
    const double flap_cmd =
        std::clamp(static_cast<double>(inputs.flap_cmd), 0.0, 1.0);
    const double flap_cap = p.flap_slew * dt;
    next.flap =
        state.flap + std::clamp(flap_cmd - state.flap, -flap_cap, flap_cap);
    const double gear_cmd =
        std::clamp(static_cast<double>(inputs.gear_cmd), 0.0, 1.0);
    const double gear_cap = p.gear_slew * dt;
    next.gear =
        state.gear + std::clamp(gear_cmd - state.gear, -gear_cap, gear_cap);

    // ---- Forces, world frame (SPEC §7) --------------------------------
    // Gravity: from the center, recomputed this tick (SPEC §6.1: no fixed
    // down axis, world-space integration only).
    const glm::dvec3 grav_accel = p.g * gravity_dir(state.position);
    assert_gravity_radial(state.position, grav_accel);

    const double speed = glm::length(state.velocity);
    // Plant-side v-hat guard: hold last valid direction below v_dir_eps —
    // the ONE predicate (sim/aero.h), shared with telemetry, so plant and
    // instrument can never disagree at exactly speed == eps.
    next.last_vhat = current_vhat(state, p);
    const glm::dvec3 vhat = next.last_vhat;

    const glm::dvec3 nose = state.orientation * glm::dvec3{0.0, 0.0, -1.0};
    const glm::dvec3 body_right = state.orientation * glm::dvec3{1.0, 0.0, 0.0};

    // Atmosphere taper (MB-atm, SPEC §0): ONE fraction thins density AND
    // thrust above atm_taper_alt — the soft service ceiling (T*f == D_min at
    // ~8 km on the shipped table). Exactly 1 through the fight band.
    const double f_atm = atm_frac(altitude(state.position, p), p);

    // Thrust along body -Z, lapsing with the atmosphere (prop-like, T ∝ f).
    const double thrust_mag = p.T_max * f_atm * next.throttle;
    const glm::dvec3 thrust = thrust_mag * nose;

    // Lift and drag. Magnitudes use the TRUE q (from actual speed, at the
    // ALTITUDE density) — the q_att_floor fib applies to control authority
    // only, never to forces. Directions use the guarded vhat; below
    // v_dir_eps that direction is stale but q makes the force negligible.
    const double q = q_dyn(p.rho * f_atm, speed);
    const glm::dvec3 v_body_dir = body_dir_of(state.orientation, vhat);
    const double alpha = alpha_of(v_body_dir);

    // Flaps/gear force deltas (MB-flaps, SPEC §0): a lift-curve SHIFT (added
    // AFTER the stall clamp — stall AoA unchanged, stall SPEED lowered) +
    // parasitic drag taxes, through the sim/aero.h single-source primitives.
    // total_lift_coeff is the SAME composition the true-n instrument reads
    // (load_factor — diff red-team P1-2: a bare-lift_coeff fork under-reads
    // the felt G ~2 g with landing flaps). Forces read next.* (the slewed
    // position this tick applies, the throttle pattern), gated structurally
    // on deployment so the clean airframe's expressions are the bit-identical
    // pre-flap ones (no +0.0 corner cases; the induced-drag term deliberately
    // uses the FLAPPED Cl — flap lift costs induced drag, the honest price).
    const double cl_total = total_lift_coeff(alpha, next.flap, speed, p);
    double cd0_total = p.Cd0;
    if (next.flap > 0.0) {
        cd0_total += flap_dCd0(next.flap, p);
    }
    if (next.gear > 0.0) {
        cd0_total += gear_dCd0(next.gear, p);
    }
    // S-wvane (SPEC §0, 2026-07-11): a crabbed fuselage pays drag — the
    // honest price of the side force below (a dragless held skid would be a
    // free 5-g flat turn; the MB-flaps lift+drag pairing precedent). Gated
    // structurally: Cd_beta = 0 keeps the pre-S-wvane expression bit-exact.
    const double beta = beta_of(v_body_dir);
    if (p.Cd_beta > 0.0) {
        const double sb = std::sin(beta);
        cd0_total += p.Cd_beta * sb * sb;
    }

    // Lift: perpendicular to velocity, in the plane spanned by velocity and
    // the body vertical (constructed as body_right x vhat — unit at zero
    // sideslip, degenerate only when velocity is along body X, where lift
    // is legitimately zero).
    glm::dvec3 lift{0.0};
    // S-wvane fuselage side-force (SPEC §0, Chad 2026-07-11 "I don't want the
    // cocking — snap to the centre every time"): F = -q*S*Cy_beta*sin(b)cos(b)
    // along side_axis (= body_right projected perpendicular to v), so a
    // crabbed airframe's flight path rotates FLAT toward the nose
    // (tau_beta = 2m/(rho*V*S*Cy_beta), ~1.1 s at V=140 on the shipped
    // table). Shape sin(b)cos(b): linear at small beta, zero at the side-on
    // pole (+-90 deg) AND at tail-slide (+-180) — graceful full-circle, no
    // clamps; rear hemisphere pushes |beta| toward 90, a q-small geometry.
    // Workless BY CONSTRUCTION: side_axis is perpendicular to the PRE-tick
    // vhat, the same v AT-12's per-tick work m*v*(v'-v) dots against — the
    // energy gate stays exact without touching its mirror (the lift scope
    // argument: a side-force defect moves the trajectory, never the energy;
    // the semi-implicit KE surplus is the same class as lift's).
    glm::dvec3 side{0.0};
    const glm::dvec3 lift_axis = glm::cross(body_right, vhat);
    const double lift_axis_len = glm::length(lift_axis);
    if (lift_axis_len > 1e-9) {
        lift = (q * p.S * cl_total / lift_axis_len) * lift_axis;
        if (p.Cy_beta > 0.0) {
            // NaN-free proof (plan-audit P0-1): vhat is unit and lift_axis is
            // perpendicular to vhat by cross construction, so
            // |cross(vhat, lift_axis)| == lift_axis_len EXACTLY — division by
            // lift_axis_len IS the normalize, and the degeneracy is inherited
            // 1:1 from the guard above. The one degenerate geometry
            // (v along body +-X, beta = +-90 deg) is exactly where
            // sin(b)cos(b) = 0.
            const glm::dvec3 side_axis =
                glm::cross(vhat, lift_axis) / lift_axis_len;
            side = -(q * p.S * p.Cy_beta * std::sin(beta) * std::cos(beta)) *
                   side_axis;
        }
    }

    // Drag: opposite velocity, parasitic + induced.
    const glm::dvec3 drag =
        -(q * p.S * (cd0_total + p.k_induced * cl_total * cl_total)) * vhat;

    // ---- Semi-implicit Euler, translation (SPEC §7) -------------------
    // Velocity from forces, then position from the NEW velocity.
    // S-wvane applies as a separate conditional add: at Cy_beta = 0 the accel
    // EXPRESSION below is the bit-exact pre-S-wvane one (no +0.0 vector into
    // the sum — the MB-flaps +-0.0 corner-case discipline).
    glm::dvec3 accel = grav_accel + (thrust + lift + drag) / p.mass;
    if (p.Cy_beta > 0.0) {
        accel += side / p.mass;
    }
    next.velocity = state.velocity + accel * dt;
    next.position = state.position + next.velocity * dt;
    // On the pure-ballistic path (no wing, no thrust) the applied impulse
    // must be exactly radial — the Section-1 tripwire stays live where the
    // physics says it must hold.
    if (p.S == 0.0 && thrust_mag == 0.0) {
        assert_applied_impulse_radial(state.position, state.velocity,
                                      next.velocity);
    }

    // ---- Torques, body frame (SPEC §7 — the sim owns the ENTIRE
    // authority model; dynamic pressure applied exactly ONCE, here) ------
    //   tau = c * max(q, q_att_floor) * delta_max_eff(V) * Input
    // plus light per-axis angular damping on the same floored pressure
    // (so ballistic-mode attitude-hold still has damping at v ~ 0).
    const double Q = q_eff(q, p);
    const double delta = delta_max_eff(speed, p);
    const auto deflect = [](float in) {
        return std::clamp(static_cast<double>(in), -1.0, 1.0);
    };
    const glm::dvec3 tau{
        p.c_pitch * Q * delta * deflect(inputs.pitch) -
            p.damp_pitch * Q * state.angular_vel.x,
        p.c_yaw * Q * delta * deflect(inputs.yaw) -
            p.damp_yaw * Q * state.angular_vel.y,
        p.c_roll * Q * delta * deflect(inputs.roll) -
            p.damp_roll * Q * state.angular_vel.z,
    };

    // Semi-implicit rotation: angular velocity from torques, then the
    // quaternion from the NEW omega (q_dot = 0.5 * q * omega_body),
    // renormalized — the renorm is a §6.1 every-tick invariant.
    next.angular_vel =
        state.angular_vel +
        dt * glm::dvec3{tau.x / p.I_pitch, tau.y / p.I_yaw, tau.z / p.I_roll};
    const glm::dquat omega{0.0, next.angular_vel.x, next.angular_vel.y,
                           next.angular_vel.z};
    next.orientation = glm::normalize(state.orientation +
                                      0.5 * dt * (state.orientation * omega));

    assert_state_valid(next);
    return next;
}

}  // namespace sim
