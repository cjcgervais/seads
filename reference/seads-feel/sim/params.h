#pragma once

// Plant + world parameters (SPEC §7, §13, Appendix). Data, not code: the
// single source of truth is config/aircraft.toml — the controller reads the
// SAME loaded struct for its plant inversion (H1: no forked params). Fields
// default to zero on purpose: an unloaded params struct must fail loudly,
// not fly plausibly. Tests that need a synthetic airframe construct one
// explicitly.

namespace sim {

struct AircraftParams {
    // World (SPEC §6). Replaces Section 1's WorldParams — one struct, one toml.
    double R = 0.0;    // planet radius [m] — the feel knob
    double g = 0.0;    // gravity magnitude [m/s^2]; direction always -local_up
    double rho = 0.0;  // air density [kg/m^3] at/below atm_taper_alt
    // Atmosphere taper (MB-atm, SPEC §0): the SOFT ceiling. Density AND
    // thrust scale by atm_frac(h) = exp(-max(0, h - atm_taper_alt)^2 /
    // (2*atm_taper_sigma^2)) — exactly 1 through the fight band (zero
    // gradient at the taper start: no felt line in the sky), Gaussian
    // falloff above, so lift, drag, control authority, and engine all thin
    // TOGETHER (one number, everywhere q or T_max appears — the H1
    // single-source discipline). atm_taper_sigma = 0 disables STRUCTURALLY
    // (atm_frac == 1 at every altitude — the knob-off strict-superset arm).
    double atm_taper_alt = 0.0;    // [m] full performance at/below this
    double atm_taper_sigma = 0.0;  // [m] Gaussian falloff width (0 = OFF)
    double sim_dt = 0.0;  // fixed sim tick [s] — owned by the app loop, stored
                          // here so the tick is config, not a scattered literal

    // Airframe (SPEC §7 forces).
    double mass = 0.0;                // [kg]
    double S = 0.0;                   // wing area [m^2]
    double Cl_max = 0.0;              // hard stall cap on Cl(alpha)
    double Cl_alpha = 0.0;            // lift slope [/rad]
    double Cd0 = 0.0;                 // parasitic drag
    double k_induced = 0.0;           // induced drag factor: Cd = Cd0 + k*Cl^2
    double T_max = 0.0;               // max thrust [N], along body -Z
    double throttle_slew_rate = 0.0;  // [1/s] throttle slews toward the
                                      // commanded target in the plant (§9.8)

    // Inertia, diagonal, body axes (SPEC §7): x = pitch, y = yaw, z = roll.
    double I_pitch = 0.0;  // [kg m^2]
    double I_yaw = 0.0;
    double I_roll = 0.0;

    // Authority (SPEC §7, the H1 ruling — the sim owns ALL of this):
    //   tau_axis = c_axis * max(q, q_att_floor) * delta_max_eff(V) * Input_axis
    double c_pitch = 0.0;  // control effectiveness [m^3-ish], per axis
    double c_yaw = 0.0;
    double c_roll = 0.0;
    double q_att_floor = 0.0;  // [Pa] q-independent authority floor (§9.6 fib)
    double damp_pitch =
        0.0;  // light angular damping: tau_d = -damp * max(q,floor) * omega
    double damp_yaw = 0.0;
    double damp_roll = 0.0;

    // Compression ramp delta_max_eff(V) — a PLANT property (SPEC §7):
    // 1.0 below v_full, linear ramp down to min_frac at v_redline, clamped
    // after.
    double v_full = 0.0;     // [m/s]
    double v_redline = 0.0;  // [m/s]
    double min_frac = 0.0;   // effective deflection fraction at/after redline

    // MB-flaps (SPEC §0): combat + landing flaps and landing gear —
    // FORCE-ONLY devices (lift-curve shift + parasitic drag tax; the torque
    // model, controller inversion, and AT-18 never see them). flap/gear are
    // deflection/extension FRACTIONS in [0,1]; the plant slews toward the
    // commanded target (deploy is physics). The flap LIFT shift washes out
    // over flap_wash_lo..flap_wash_hi (smoothstep -> 0; over-speed flaps
    // keep their DRAG, becoming speed brakes — the documented default; the
    // alternative felt behavior is auto-retract, Chad rules at the stick).
    double dCl_flap = 0.0;      // Cl shift at full flap (landing)
    double dCd0_flap = 0.0;     // parasitic drag at full flap (tax ~ flap^2)
    double flap_slew = 0.0;     // [1/s] deploy rate (fraction per second)
    double flap_combat = 0.0;   // (0,1) the combat detent's fraction
    double flap_wash_lo = 0.0;  // [m/s] lift shift full at/below
    double flap_wash_hi = 0.0;  // [m/s] lift shift zero at/above
    double dCd0_gear = 0.0;     // parasitic drag at gear full-down (~ gear)
    double gear_slew = 0.0;     // [1/s] extension rate

    // S-wvane (SPEC §0, Chad's ruling 2026-07-11 "I don't want the cocking —
    // snap to the centre every time"): fuselage side-force from sideslip,
    // F = -q*S*Cy_beta*sin(b)*cos(b) perpendicular to velocity in the lateral
    // plane — a crabbed airframe straightens FLAT (the path rotates to the
    // nose) instead of holding the crab forever; tau_beta = 2m/(rho*V*S*Cy).
    // Cd_beta prices the crab (Cd += Cd_beta*sin^2(b)) so a held full skid is
    // an honest brake, not a dragless flat turn. BOTH default 0.0 =
    // structurally OFF (bit-identical pre-S-wvane expressions).
    double Cy_beta = 0.0;  // side-force slope [/rad-ish, sin2b/2 shape]
    double Cd_beta = 0.0;  // sideslip drag: Cd += Cd_beta * sin^2(beta)

    // Numeric guards.
    double v_dir_eps =
        0.0;  // [m/s] below this the sim holds last-valid v-hat (§7)
};

}  // namespace sim
