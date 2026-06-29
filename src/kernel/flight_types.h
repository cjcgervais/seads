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
// -> 0). NOTE (B2, v1.6r0): the climb_max/climb_min LUTs are now VESTIGIAL for the kernel — the
// vertical channel is emergent (alt_dot = V*sin gamma), so the kernel no longer consults them.
// They are retained in the schema/struct/tables (cross-toolchain-reproduced) for B3/B4 limit work.
struct Envelope { Lut5 phi_max, roll_rate, climb_max, climb_min;
                  double mass_kg, wing_area_m2, cd0, induced_k, thrust_static_n, v_max_mps; };

// Per-tick command: target bank (rad), commanded load factor n (target_g, dimensionless; B2 —
// replaces the B1 climb rate), and throttle [0,1] (B1). n=1 wings level holds level flight;
// n=1/cos(phi) holds a level coordinated turn; n>1/cos(phi) pitches up (climb is earned).
struct Command { double target_phi; double target_g; double throttle = 0.0; };

}  // namespace seads
