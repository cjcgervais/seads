// SEADS GEO-001 snapshot serialization (netcode layer 2). Mirrors tools/snapshot_ref.py
// BIT-FOR-BIT. Frames a multi-aircraft world snapshot over the GEO-001 wire codec for the
// 20 Hz snapshot cadence (physics stays 100 Hz).
//
// This is a SEPARATE, quantized transport — NOT the canonical hashing snapshot
// (Kernel::snapshot(), raw LE f64) which stays the source of truth for the world_hash.
// The wire snapshot is lossy, downstream of the sim, never fed back as canonical.
//
// Field scope (rail-faithful): GEO-001 defines scales for lat/lon/bearing/alt ONLY, so we
// transmit position + heading + altitude per aircraft (sufficient for remote interpolation).
// phi (bank) and tas have no GEO-001 scale — carrying them would be a rail reseal — so
// prediction state is deliberately DEFERRED to a later layer.
#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

#include "geo001.h"

namespace seads {
namespace netsnap {

// rad/deg conversion via exact hex-float constants (identical bits in C++ and CPython).
constexpr double PI_      = 0x1.921fb54442d18p+1;  // 3.141592653589793 (matches det_math)
constexpr double RAD2DEG  = 180.0 / PI_;           // one IEEE division — same double both sides
constexpr double DEG2RAD  = PI_ / 180.0;

constexpr int64_t SNAPSHOT_PROTOCOL = 1;  // GEO-001 snapshot framing version

// One aircraft on the wire, in GEO-001 transport units (degrees / metres).
struct EntityState {
    int64_t id;
    double lat_deg;
    double lon_deg;
    double bearing_deg;
    double alt_m;
};

struct Snapshot {
    int64_t protocol = SNAPSHOT_PROTOCOL;
    int64_t server_tick = 0;       // 100 Hz tick index at capture
    std::vector<EntityState> entities;
};

// Build a wire EntityState from raw kernel state (radians/metres). The kernel `psi` heading
// maps to GEO-001 `bearing`. Transmits faithfully (no lon/heading normalization).
EntityState from_kernel(int64_t id, double lat_rad, double lon_rad, double psi_rad,
                        double alt_m);

void encode_snapshot(const Snapshot& s, std::vector<uint8_t>& out);
bool decode_snapshot(const uint8_t* data, size_t len, size_t& pos, Snapshot& out);

}  // namespace netsnap
}  // namespace seads
