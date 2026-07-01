// SEADS GEO-001 snapshot serialization (netcode layer 2). Mirrors tools/snapshot_ref.py
// BIT-FOR-BIT. Frames a multi-aircraft world snapshot over the GEO-001 wire codec for the
// 20 Hz snapshot cadence (physics stays 100 Hz).
//
// This is a SEPARATE, quantized transport — NOT the canonical hashing snapshot
// (Kernel::snapshot(), raw LE f64) which stays the source of truth for the world_hash.
// The wire snapshot is lossy, downstream of the sim, never fed back as canonical.
//
// Field scope (rail-faithful) — THREE sections since seal v1.12r0:
//   1. GEO section: n × (id, GeoPoint[lat,lon,bearing,alt]) over GEO-001 (geography-only;
//      the sealed codec + its parity vectors are UNTOUCHED).
//   2. KIN section: n × (id, phi_q, tas_q [, gamma_q]) over the auxiliary wire.kin block —
//      phi ×1e6 (micro-degree, mirrors bearing), tas ×1e3 (mm/s, mirrors alt), and — for
//      KIN-002 (protocol >= 3, seal v1.6r0) — gamma ×1e6 (flight-path angle, micro-degree).
//      Present iff protocol >= 2; gamma carried iff protocol >= 3. NON-geographic, OUTSIDE GeoPoint.
//   3. WEAPON section (WEAPON-001, protocol >= 4, seal v1.12r0) over wire.weapon — the Step 7
//      gunnery state: per-aircraft n × (id, hp_q, fire_cd_q [, ammo_q]) (hp/fire_cd ×1e3; and —
//      for the G4 magazine, protocol >= 5, seal v1.14r0 — ammo ×1 (a pure integer counter, so
//      unit scale is exact + compact like ttl/owner; carried iff protocol >= 5)), then a
//      projectile count m and m × (pid, GeoPoint, damage_q ×1e3, ttl, owner). ttl/owner are
//      EXACT i64 (kernel u32 counters, NOT quantized). Present iff protocol >= 4.
// Section 1 alone (protocol 1) feeds remote interpolation; section 2 adds bank+speed (+gamma in
// KIN-002) so a client can re-seed a kernel for client-side prediction (layer 4b) and late-join;
// section 3 (this seal) adds hitpoints/fire-rate + the live rounds so multiplayer can REPLICATE
// the dogfight (draw tracers, HP bars, kills) instead of reading them from the local kernel.
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

constexpr int64_t SNAPSHOT_PROTOCOL = 5;  // (>=2 KIN; >=3 KIN-002 gamma; >=4 WEAPON-001 gunnery; >=5 ammo)

// Auxiliary KIN-002 scales (must match config/rails/atm.json wire.kin). phi and gamma are angles
// in degrees -> reuse the bearing-style 1e6; tas is m/s -> reuses the alt-style 1e3.
constexpr int64_t PHI_SCALE   = 1000000;  // 1e6
constexpr int64_t SPEED_SCALE = 1000;     // 1e3
constexpr int64_t GAMMA_SCALE = 1000000;  // 1e6 (flight-path angle, KIN-002 — seal v1.6r0)

// Auxiliary WEAPON-001 scales (must match config/rails/atm.json wire.weapon). hp/fire_cd/damage
// are small magnitudes carried to milli-resolution (1e3). hp may be negative on a dead frame.
constexpr int64_t HP_SCALE     = 1000;    // 1e3
constexpr int64_t FIRECD_SCALE = 1000;    // 1e3
constexpr int64_t DAMAGE_SCALE = 1000;    // 1e3
constexpr int64_t AMMO_SCALE   = 1;       // 1e0 (magazine rounds — integer counter, exact+compact; protocol >= 5)

// One aircraft on the wire. GEO fields in GEO-001 units (degrees / metres); KIN fields in KIN
// units (phi/gamma degrees, tas m/s); WEAPON fields hp/fire_cd (kernel units). All of
// phi/tas/gamma/hp/fire_cd default 0 for callers building lower-protocol frames.
struct EntityState {
    int64_t id;
    double lat_deg;
    double lon_deg;
    double bearing_deg;
    double alt_m;
    double phi_deg = 0.0;
    double tas_mps = 0.0;
    double gamma_deg = 0.0;
    double hp = 0.0;        // WEAPON-001: hitpoints (hp<=0 == dead)
    double fire_cd = 0.0;   // WEAPON-001: fire-rate cooldown (ticks remaining)
    double ammo = 0.0;      // WEAPON-001 (v1.14r0, protocol >= 5): magazine rounds remaining
};

// One ballistic round on the wire (WEAPON-001). GEO fields (bearing == round heading psi);
// damage carried per round; ttl (ticks remaining) and owner (firing aircraft index) are EXACT.
struct ProjectileState {
    int64_t id;
    double lat_deg;
    double lon_deg;
    double bearing_deg;
    double alt_m;
    double damage = 0.0;
    int64_t ttl = 0;
    int64_t owner = 0;
};

struct Snapshot {
    int64_t protocol = SNAPSHOT_PROTOCOL;
    int64_t server_tick = 0;       // 100 Hz tick index at capture
    std::vector<EntityState> entities;
    std::vector<ProjectileState> projectiles;  // WEAPON-001 (protocol >= 4)
};

// Build a wire EntityState from raw kernel state (radians/metres). The kernel `psi` heading
// maps to GEO-001 `bearing`; `phi`/`gamma` (rad) -> KIN degrees; `tas` passes through (m/s);
// `hp`/`fire_cd`/`ammo` (WEAPON-001) pass through (kernel units). Transmits faithfully (no normalization).
EntityState from_kernel(int64_t id, double lat_rad, double lon_rad, double psi_rad,
                        double alt_m, double phi_rad = 0.0, double tas_mps = 0.0,
                        double gamma_rad = 0.0, double hp = 0.0, double fire_cd = 0.0,
                        double ammo = 0.0);

// Build a wire ProjectileState from raw kernel projectile state (radians/metres). The round's
// heading `psi` maps to GEO-001 `bearing`; `damage` passes through; `ttl`/`owner` carried exactly.
ProjectileState proj_from_kernel(int64_t id, double lat_rad, double lon_rad, double psi_rad,
                                 double alt_m, double damage, int64_t ttl, int64_t owner);

void encode_snapshot(const Snapshot& s, std::vector<uint8_t>& out);
bool decode_snapshot(const uint8_t* data, size_t len, size_t& pos, Snapshot& out);

}  // namespace netsnap
}  // namespace seads
