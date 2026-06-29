// SEADS flight input/envelope types (ATM-Sphere v1.3r0). Shared by the kernel and the
// generated envelope_tables.h / scenario_params.h so constants and code use one type.
#pragma once

namespace seads {

// 5-point lookup table: x strictly increasing (TAS m/s), y the tabulated value.
struct Lut5 { double x[5]; double y[5]; };

// Per-aircraft tuning envelope in radian units (deg->rad baked into nodes at gen time).
// phi_max (rad), roll_rate (rad/s), climb_max/min (m/s).
struct Envelope { Lut5 phi_max, roll_rate, climb_max, climb_min; };

// Per-tick command: target bank (rad) and target climb rate (m/s).
struct Command { double target_phi; double target_climb; };

}  // namespace seads
