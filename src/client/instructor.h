// SEADS mouse-aim instructor — the OUTER loop, grafted onto the sealed kernel (SOLUTION §3.2 /
// SPEC §9.3, adapted). PRESENTATION-ONLY, client side (libm ok — never feeds the sim's bits).
//
// The SOLUTION cascade is TWO loops: an outer pointing loop (angle error -> desired rate, braking-
// aware, clamped) and an inner rate-PI + plant-inversion loop that turns rate into control-surface
// deflections. SEADS's sealed kernel is a 3-DOF COORDINATED point mass whose per-tick Command is
// { target_phi (bank rad), target_g (load factor n) } — exactly the abstraction level of the outer
// loop's OUTPUT. So we port the outer loop ONLY; the kernel IS the inner loop (it slews phi toward
// target_phi at the sealed roll_rate and clamps n by the sealed n_aero/structural limits).
//
// The map, from the kernel's own dynamics (kernel.cpp step):
//   gamma_dot = (g0/V)(n cos phi - cos gamma)   =>  n_trim = cos(gamma)/cos(phi) HOLDS flight path
//   psi_dot   = (g0/V)(n sin phi / cos gamma)   =>  heading turns by BANKING (bank-to-turn)
// So: bank toward the aim (bank-to-turn), and pull n above/below n_trim to point the nose at it.
// (SOLUTION's cosPhiTheta trim is for its plant; on THIS plant it would make every bank descend.)
//
// Honest scope of this first graft: the ABOVE-the-wing-line case is the full bank-to-turn + braking
// pull (the dogfighting 95%). BELOW the wing line the kernel CANNOT roll inverted (phi is clamped to
// the sealed phi_max < 90 deg), so there is no split-S-by-roll: the instructor holds the wings and
// PUSHES (negative g) instead. The hysteretic push-vs-roll gate + curvature refinements are a later
// slice; this is the flyable core to judge by feel.
#pragma once
#include <cmath>
#include "client_frame.h"   // AircraftFrame, Vec3, cross, dot, normalize
#include "kernel.h"         // seads::Command

namespace seads {
namespace client {

// Feel knobs (all data — tune by flying, never hard-code numbers in the body). Radians / SI.
struct InstructorTuning {
    double k_theta      = 3.0;    // outer proportional gain (1/s): desired pull rate = k_theta * e
    double a_brake      = 6.0;    // braking decel (rad/s^2): sqrt(2*a_brake*e) caps arrival rate
    double max_bank     = 1.20;   // commanded bank ceiling (rad ~ 69 deg); kernel re-clamps to phi_max
    double n_max_adv    = 8.0;    // advisory pull ceiling (kernel n_aero/structural has final say)
    double n_min_adv    = -3.0;   // advisory push floor
    double deadzone     = 0.0035; // ~0.2 deg: below this the pointing term is 0 (hold trim)
    double rearm        = 0.0044; // ~0.25 deg: hysteretic re-arm
    double v_min        = 10.0;   // floor on every V-division (G-clamp diverges, not vanishes, at v->0)
    double cos_phi_floor= 0.087;  // clamp cos(phi) (>= ~85 deg) so n_trim never blows up
};

// Instructor internal state carried across ticks (deadzone latch). Pure: caller owns it.
struct InstructorState {
    bool deadzoned = false;
};

// Braking-aware pointing law (SOLUTION §2.6): the desired rate never exceeds what a_brake can shed
// before reaching the target. Signed.
inline double sqrt_law(double e, double k, double a_brake, double w_max) {
    double m = k * std::fabs(e);
    double b = std::sqrt(2.0 * a_brake * std::fabs(e));
    if (b < m) m = b;
    if (w_max < m) m = w_max;
    return (e >= 0.0) ? m : -m;
}

// One instructor tick: aim (world unit vector, from AimState) + the aircraft frame + kernel state ->
// a Command { target_phi, target_g }. throttle/fire are the caller's to fill.
struct InstructorOut {
    Command cmd;        // target_phi, target_g (throttle/fire left default)
    double e;           // total pointing error (rad) — for the HUD / reticle-gap readout
    double bank_err;    // commanded roll delta (rad)
    bool pushing;       // below-wing-line push regime this tick
};

inline InstructorOut instructor_step(Vec3 aim, const AircraftFrame& f, double phi, double gamma,
                                     const InstructorTuning& p, InstructorState& st) {
    InstructorOut o;
    const double g0 = 9.80665;
    const double R  = 15000.0;
    double V = f.speed; if (V < p.v_min) V = p.v_min;

    // Total pointing error and the rotation axis that would carry the nose onto the aim.
    double ca = dot(aim, f.nose); if (ca > 1.0) ca = 1.0; else if (ca < -1.0) ca = -1.0;
    double e = std::acos(ca);
    Vec3 rax = cross(f.nose, aim);              // |rax| = sin(e); direction = nose->aim rotation axis
    double om_pull = dot(rax, f.right);         // component the PULL actuator (about +right) supplies
    double om_yaw  = dot(rax, f.body_up);       // component that needs a BANK to convert to a turn

    // Deadzone with hysteresis: hold trim (no pointing command) when parked on the aim.
    if (st.deadzoned && e > p.rearm) st.deadzoned = false;
    if (!st.deadzoned && e < p.deadzone) st.deadzoned = true;

    // Flight-path-holding trim load (the baseline n that keeps gamma steady at this bank).
    double cphi = std::cos(phi);
    double acphi = std::fabs(cphi);
    if (acphi < p.cos_phi_floor) cphi = (cphi < 0 ? -p.cos_phi_floor : p.cos_phi_floor);
    double n_trim = std::cos(gamma) / cphi;

    double target_phi = phi;    // default: hold current bank
    double wb_des = 0.0;        // desired body pitch rate (about +right); + = pull, - = push
    o.pushing = false;

    if (!st.deadzoned) {
        // Advisory pull/push rate ceilings from the g budget (kernel has the final clamp).
        double w_pull_max = (p.n_max_adv - n_trim) * g0 * std::fabs(cphi) / V;
        double w_push_max = (n_trim - p.n_min_adv) * g0 * std::fabs(cphi) / V;  // magnitude
        if (w_pull_max < 0.0) w_pull_max = 0.0;
        if (w_push_max < 0.0) w_push_max = 0.0;

        if (om_pull >= 0.0) {
            // ABOVE the wing line: bank-to-turn + pull. Roll so the pull axis aligns with the needed
            // rotation, then pull toward the aim, gated so we don't pull hard while still rolling in.
            double bank_err = std::atan2(-om_yaw, om_pull);
            target_phi = phi + bank_err;
            double align = std::cos(bank_err); if (align < 0.0) align = 0.0;
            wb_des = sqrt_law(e, p.k_theta, p.a_brake, w_pull_max) * align;
            o.bank_err = bank_err;
        } else {
            // BELOW the wing line: cannot roll inverted (phi clamp) -> hold the wings and PUSH.
            o.pushing = true;
            target_phi = phi;                       // no roll command; nose-down via negative g
            wb_des = -sqrt_law(e, p.k_theta, p.a_brake, w_push_max);
            o.bank_err = 0.0;
        }
    } else {
        o.bank_err = 0.0;
    }

    // Clamp the commanded bank (kernel re-clamps to phi_max regardless).
    if (target_phi >  p.max_bank) target_phi =  p.max_bank;
    if (target_phi < -p.max_bank) target_phi = -p.max_bank;

    // Body pitch rate -> extra load beyond trim: gamma_dot ~ (g0/V)*(dN*cos phi) => dN = wb*V/(g0 cphi)
    double dN = wb_des * V / (g0 * std::fabs(cphi));
    double target_g = n_trim + dN;
    if (target_g > p.n_max_adv) target_g = p.n_max_adv;
    if (target_g < p.n_min_adv) target_g = p.n_min_adv;

    o.cmd = Command{target_phi, target_g};
    o.e = e;
    return o;
}

}  // namespace client
}  // namespace seads
