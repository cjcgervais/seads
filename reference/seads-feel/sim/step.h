#pragma once

#include "sim/params.h"
#include "sim/state.h"

namespace sim {

// The plant. PURE: no I/O, no clock, no render (SPEC §5).
// Section 2: full force-based flight model (SPEC §7) — thrust along body -Z,
// lift with hard stall cap, parasitic + induced drag, and the ENTIRE
// authority model tau = c * max(q, q_att_floor) * delta_max_eff(V) * Input
// (the H1 ruling: dynamic pressure applied exactly once, here).
// Semi-implicit Euler in world space; quaternion integrate + renormalize.
SimState step(const SimState& state, const Inputs& inputs,
              const AircraftParams& params, double dt);

}  // namespace sim
