#pragma once

#include <string>

#include "control/params.h"
#include "sim/params.h"

// TOML -> ControllerParams (SPEC §9). Lives in config/ (with the data), not
// in control/ (which is pure — no I/O, SPEC §5). Strict like load_aircraft:
// every key present, every value sane, or it throws.
//
// It takes the ALREADY-loaded AircraftParams because the controller's
// validity is defined against the airframe: AoA_max <= plant stall alpha
// (SPEC §9.3b), the critical-damping rule K_w >= 4*I*K_theta, and the ZOH
// ceiling K_w*sim_dt/I <= 0.5 (SPEC §9.4) — a table that would oscillate by
// construction must fail the load, not the flight. Angles/rates convert from
// degrees (the config boundary) to radians here, once.

namespace cfg {

control::ControllerParams load_controller_toml(const std::string& path,
                                               const sim::AircraftParams& ap);

}  // namespace cfg
