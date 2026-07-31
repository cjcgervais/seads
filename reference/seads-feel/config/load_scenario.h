#pragma once

#include <string>

#include "drone/drone.h"
#include "render/gunsight.h"
#include "sim/params.h"

// TOML -> ScenarioParams (SPEC §0 S8-drone). Lives in config/ (with the data),
// not in drone/ (pure — no I/O, SPEC §5). Strict like load_aircraft /
// load_controller: every key present, every value sane, or it throws. Angles
// convert from degrees (the config boundary) to radians here, once.
//
// Returns the domain param structs directly (the load_aircraft /
// load_controller pattern — config returns a struct from a lower/sibling pure
// layer, never duplicating its field list). drone/ is a sibling of config, so
// there is no layering inversion.
//
// Takes the already-loaded AircraftParams (like load_controller): the drone's
// DOCILE-ENVELOPE guards are defined against the airframe — speed above stall,
// muzzle_speed fast enough to keep the ballistic solve monotone (Fable P1-1/3).

namespace cfg {

struct ScenarioParams {
    drone::DroneParams drone{};
    render::GunsightParams gunsight{};
};

ScenarioParams load_scenario_toml(const std::string& path,
                                  const sim::AircraftParams& ap);

}  // namespace cfg
