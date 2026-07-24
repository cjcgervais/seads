#pragma once

#include <glm/glm.hpp>

#include "sim/params.h"

namespace sim {

// SPEC §6: there is no global up. Everything below derives from position,
// recomputed each tick — never cached, never a fixed axis.

inline glm::dvec3 local_up(const glm::dvec3& position) {
    return glm::normalize(position);
}

inline glm::dvec3 gravity_dir(const glm::dvec3& position) {
    return -local_up(position);
}

inline double altitude(const glm::dvec3& position,
                       const AircraftParams& params) {
    return glm::length(position) - params.R;
}

}  // namespace sim
