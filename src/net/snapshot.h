// SEADS GEO-001 snapshot serialization (netcode layer 2). Mirrors tools/snapshot_ref.py
// BIT-FOR-BIT. Frames a multi-aircraft world snapshot over the GEO-001 wire codec for the
// 20 Hz snapshot cadence (physics stays 100 Hz).
//
// This is a SEPARATE, quantized transport — NOT the canonical hashing snapshot
// (Kernel::snapshot(), raw LE f64) which stays the source of truth for the world_hash.
// The wire snapshot is lossy, downstream of the sim, never fed back as canonical.
//
// Field scope (rail-faithful) — TWO sections since seal v1.4r0:
//   1. GEO section: n × (id, GeoPoint[lat,lon,bearing,alt]) over GEO-001 (geography-only;
//      the sealed codec + its parity vectors are UNTOUCHED).
//   2. KIN section: n × (id, phi_q, tas_q) over the auxiliary wire.kin (KIN-001) block —
//      phi ×1e6 (micro-degree, mirrors bearing), tas ×1e3 (mm/s, mirrors alt). Present iff
//      protocol >= 2. phi/tas are NON-geographic, so they live OUTSIDE the GeoPoint.
// Section 1 alone (protocol 1) feeds remote interpolation; section 2 adds bank+speed so a
// client can re-seed a kernel for client-side prediction (layer 4b) and late-join.
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

constexpr int64_t SNAPSHOT_PROTOCOL = 2;  // snapshot framing version (>=2 => KIN section)

// Auxiliary KIN-001 scales (must match config/rails/atm.json wire.kin). phi is degrees ->
// reuses the bearing-style 1e6; tas is m/s -> reuses the alt-style 1e3.
constexpr int64_t PHI_SCALE   = 1000000;  // 1e6
constexpr int64_t SPEED_SCALE = 1000;     // 1e3

// One aircraft on the wire. GEO fields in GEO-001 units (degrees / metres); KIN fields in
// KIN-001 units (phi degrees, tas m/s). phi/tas default 0 for protocol-1 callers.
struct EntityState {
    int64_t id;
    double lat_deg;
    double lon_deg;
    double bearing_deg;
    double alt_m;
    double phi_deg = 0.0;
    double tas_mps = 0.0;
};

struct Snapshot {
    int64_t protocol = SNAPSHOT_PROTOCOL;
    int64_t server_tick = 0;       // 100 Hz tick index at capture
    std::vector<EntityState> entities;
};

// Build a wire EntityState from raw kernel state (radians/metres). The kernel `psi` heading
// maps to GEO-001 `bearing`; `phi` (bank, rad) -> KIN phi (deg); `tas` passes through (m/s).
// Transmits faithfully (no lon/heading normalization).
EntityState from_kernel(int64_t id, double lat_rad, double lon_rad, double psi_rad,
                        double alt_m, double phi_rad = 0.0, double tas_mps = 0.0);

void encode_snapshot(const Snapshot& s, std::vector<uint8_t>& out);
bool decode_snapshot(const uint8_t* data, size_t len, size_t& pos, Snapshot& out);

}  // namespace netsnap
}  // namespace seads
