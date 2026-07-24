#pragma once

#include <string>

#include "sim/params.h"

// TOML -> AircraftParams. Lives in config/ (with the data), NOT in sim/ —
// sim/ is pure and may not do I/O (SPEC §5). Strict on purpose: every key
// must be present and every value sane, or this throws std::runtime_error.
// A silently-defaulted coefficient is a forked-params bug (H1) waiting to
// happen.

namespace cfg {

sim::AircraftParams load_aircraft_toml(const std::string& path);

}  // namespace cfg
