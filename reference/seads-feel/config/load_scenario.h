#pragma once

#include <string>

#include "combat/kill.h"  // combat::CombatSetup (bandit return-fire config)
#include "drone/drone.h"
#include "render/gunsight.h"
#include "sim/params.h"
#include "sim/sled.h"  // sim::SledComfort (RC [sled_comfort] dials)

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
//
// FIX 5 (Fable: single-source drag k):
// cannon_drag_k is the primary weapon's drag constant, loaded from
// world.toml [guns] cannon_drag_k_per_m and threaded here so the pipper
// (GunsightParams::drag_k) is BY CONSTRUCTION on the SAME value as the round —
// no scenario.toml duplicate, no silent-fork risk. Production callers (main.cpp)
// MUST pass world.guns.cannon_drag_k after loading world config. Load-only test
// callers that don't exercise the ballistic solver may pass 0 (vacuum, harmless).
// sim_dt threads the fixed sim tick into GunsightParams::fire_dt so the pipper's
// discretization-lag correction (FIX 2) matches weapon::advance exactly.

namespace cfg {

struct ScenarioParams {
    drone::DroneParams drone{};
    render::GunsightParams gunsight{};
    combat::CombatSetup combat{};  // bandit return-fire + difficulty (from [combat])
    combat::DamageParams damage{};  // component damage feel dials (from [damage])
    // RC roll-comfort dials (from [sled_comfort]) — §0b ruling: "all the
    // mechanisms available for tuning" without a recompile. The committed toml
    // values reproduce sim::SledComfort{} exactly (asserted by a test), so
    // loading is bit-neutral until Chad turns a dial.
    sim::SledComfort sled_comfort{};
};

ScenarioParams load_scenario_toml(const std::string& path,
                                  const sim::AircraftParams& ap,
                                  double cannon_drag_k = 0.0);

}  // namespace cfg
