// SEADS flight input/envelope types (ATM-Sphere v1.6r0). Shared by the kernel and the
// generated envelope_tables.h / scenario_params.h so constants and code use one type.
#pragma once

namespace seads {

// 5-point lookup table: x strictly increasing (TAS m/s), y the tabulated value.
struct Lut5 { double x[5]; double y[5]; };

// Per-aircraft tuning envelope in radian units (deg->rad baked into nodes at gen time).
// phi_max (rad), roll_rate (rad/s) drive the bank dynamics. The scalar block (B1, seal v1.5r0)
// holds the longitudinal-energy aero params: mass (kg), wing area S (m^2), parasitic drag coeff
// cd0, induced-drag factor k (Cd_i = k*CL^2), static thrust T0 (N), and V_max (m/s, prop thrust
// -> 0). The B3 block (seal v1.7r0) adds the V-n limits: cl_max (max usable lift coefficient ->
// the aerodynamic load-factor ceiling n_aero = cl_max*qS/(m*g0), i.e. accelerated stall) and the
// per-airframe structural g limits n_max_struct / n_min_struct (retiring the B2 global [-3, 9]
// placeholder). NOTE (B2, v1.6r0): the climb_max/climb_min LUTs are VESTIGIAL for the kernel — the
// vertical channel is emergent (alt_dot = V*sin gamma), so the kernel no longer consults them.
// They are retained in the schema/struct/tables (cross-toolchain-reproduced) for B4 tuning work.
// The G3 block (seal v1.11r0) adds the per-airframe weapon roster: hp_start (airframe toughness =
// starting hitpoints), muzzle_v_mps (added to firer TAS), damage_per_round (carried by each round),
// rof_interval_ticks (min ticks between shots; the kernel gates firing with a per-aircraft cooldown).
// The G4 block (seal v1.13r0) adds ammo_start: the per-airframe magazine size (abstract rounds). The
// kernel gates firing on ammo > 0 and decrements one round per shot; at 0 the gun falls silent.
// The convergence block (seal v1.15r0) adds convergence_m: the per-airframe gun harmonization range.
// SEADS models a single centerline battery, so it is realized as VERTICAL boresight zeroing — a
// fired round's initial gamma is offset up by the flat-fire drop-compensation angle so its
// trajectory crosses the aim line at convergence_m (see Kernel::spawn_projectile_).
// The region-toughness block (seal v1.20r0) adds engine_frac/wing_frac/tail_frac: per-airframe
// fractions of hp_start sizing the ENGINE/WING/TAIL region sub-pools at Kernel::add (v1.18r0
// introduced the pools with global 0.375/0.5/0.25 constants; those survive as the envelope-less
// defaults). Each is a positive multiple of 1/8 and <= 1 (tuning_probe enforces it) so every pool
// value is exact in f64 and milli-exact on the WEAPON-001 wire.
// The supercharger block (seal v1.22r0) adds crit_alt_m: the altitude (m) up to which the engine
// holds RATED power — the kernel thrust lapse is min(1, sigma(alt)/sigma(crit_alt_m)), one
// comparison + one divide. A multiple of 500 inside [0, 8000] (tuning_probe enforces it) so
// sigma(crit) is exactly a sealed ISA-LUT node; 0 = "no supercharger" = the B5 T *= sigma
// bit-for-bit. thrust_static_n/v_max_mps were re-anchored with it (tools/supercharger_retune.py).
// The two-speed blower block (seal v1.25r0) adds crit_lo_alt_m (the LOW/MS gear's full-throttle
// height, a 500 m multiple in (0, crit_alt_m) — a sealed LUT node) and gear2_frac (the HIGH/FS
// gear's rated-power fraction, a 1/16 multiple with sigma(crit_alt)/sigma(crit_lo) < g2 < 1):
// the lapse becomes max(min(1, sigma/sigma(crit_lo)), gear2_frac*min(1, sigma/sigma(crit_alt)))
// — flat-fall-flat-fall. crit_lo_alt_m = 0 (gear2_frac = 1) = single-speed = the v1.22r0 lapse
// path bit-for-bit (the two-speed branch is never entered). See Kernel::step.
// Scalar field order MUST match tools/envelopes.py AERO_FIELDS (the single source of truth).
struct Envelope { Lut5 phi_max, roll_rate, climb_max, climb_min;
                  double mass_kg, wing_area_m2, cd0, induced_k, thrust_static_n, v_max_mps,
                         cl_max, n_max_struct, n_min_struct,
                         hp_start, muzzle_v_mps, damage_per_round, rof_interval_ticks,
                         ammo_start, convergence_m,
                         engine_frac, wing_frac, tail_frac,
                         crit_alt_m,
                         crit_lo_alt_m, gear2_frac; };

// Per-tick command: target bank (rad), commanded load factor n (target_g, dimensionless; B2 —
// replaces the B1 climb rate), throttle [0,1] (B1), and the gun trigger fire (G1, v1.9r0). n=1
// wings level holds level flight; n=1/cos(phi) holds a level coordinated turn; n>1/cos(phi)
// pitches up (climb is earned). fire=true spawns one ballistic round this tick from the firer's
// post-step muzzle state (see Kernel::spawn_projectile_).
struct Command { double target_phi; double target_g; double throttle = 0.0; bool fire = false; };

}  // namespace seads
