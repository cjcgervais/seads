#!/usr/bin/env python3
"""
snapshot_ref.py — SEADS GEO-001 snapshot serialization REFERENCE (netcode layer 2).

Frames a multi-aircraft world snapshot over the GEO-001 wire codec (tools/geo001_ref.py)
for the 20 Hz snapshot cadence (physics stays 100 Hz). This is the *single source of truth*
for the snapshot wire bytes; the C++ codec (`src/net/snapshot`) MUST reproduce them
bit-for-bit, proven by the generated-vector gate exactly as geo001/det_math are.

Relationship to the canonical hashing snapshot
----------------------------------------------
`Kernel::snapshot()` (raw little-endian f64) stays the SOURCE OF TRUTH for the world_hash.
THIS wire snapshot is a SEPARATE, quantized transport — lossy, downstream of the sim, never
fed back as canonical. The two must not be confused.

Field scope (rail-faithful) — THREE sections since seal v1.12r0
--------------------------------------------------------------
The kernel's per-aircraft state is (lat, lon, psi, phi, alt, tas, gamma, hp, fire_cd); psi is
the heading. A snapshot carries the world in up to THREE back-to-back sections, all
self-delimiting (LEB128):

  1. GEO section — n × (id, GeoPoint) over GEO-001 scales lat/lon 1e7, bearing 1e6, alt 1e3.
     GEO-001 stays GEOGRAPHY-ONLY (the sealed codec + its parity vectors are UNTOUCHED).
  2. KIN section — n × (id, phi_q, tas_q [, gamma_q]) over the auxiliary NON-geographic rail
     block `wire.kin`: phi 1e6, tas 1e3, and — for KIN-002 (protocol >= 3, seal v1.6r0) —
     gamma 1e6. Present iff protocol >= 2; gamma carried iff protocol >= 3.
  3. WEAPON section (WEAPON-001, protocol >= 4, seal v1.12r0) — the Step 7 gunnery state over
     the auxiliary rail block `wire.weapon`:
       a) per-aircraft  n × (id, hp_q, fire_cd_q)        hp 1e3, fire_cd 1e3
       b) a projectile count m, then m × (pid, GeoPoint[lat,lon,bearing,alt], damage_q, ttl,
          owner)                                          damage 1e3; ttl/owner are EXACT i64
          (kernel u32 counters, carried losslessly — not quantized).
     Present iff protocol >= 4. The projectile `bearing` is the round's heading psi.

Section 1 alone (protocol 1) supports REMOTE INTERPOLATION (the ~100 ms buffer). Section 2
adds bank+speed(+gamma) so a client can re-seed a kernel for CLIENT-SIDE PREDICTION (layer 4b)
and late-join. Section 3 (this seal) adds the gunnery state — per-aircraft hitpoints + fire-rate
cooldown and the live ballistic rounds — so multiplayer can REPLICATE the dogfight (draw tracer
rounds, HP bars, and kills on a remote client) instead of reading them out-of-band from the
local kernel. Carrying phi/tas was a rail reseal (v1.3r0 → v1.4r0); gamma was KIN-002 (v1.6r0);
the weapon block is WEAPON-001 (v1.12r0, protocol 3 → 4).

Units: the kernel stores lat/lon/psi in RADIANS. We convert to DEGREES (the GEO-001 native
unit) before quantizing, via exact hex-float constants so C++ and Python agree to the bit. hp,
fire_cd, tas, damage pass through (already SI / count units). ttl/owner are integer counters.

Usage:  python tools/snapshot_ref.py        # internal self-test
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import geo001_ref as g

# ---- exact constants (hex floats, identical bits in C++ and CPython) -------------------
PI       = float.fromhex('0x1.921fb54442d18p+1')   # 3.141592653589793 (matches det_math)
RAD2DEG  = 180.0 / PI                              # one IEEE division — same double both sides
DEG2RAD  = PI / 180.0

SNAPSHOT_PROTOCOL = 4   # framing version (>=2 KIN; >=3 KIN-002 gamma; >=4 WEAPON-001 gunnery)

# Auxiliary KIN-002 scales (must match config/rails/atm.json wire.kin). Bank (phi) and the
# flight-path angle (gamma) are angles in degrees, so they reuse the bearing-style 1e6
# (micro-degree); true airspeed (tas) is metres/second, so it reuses the alt-style 1e3 (mm/s).
PHI_SCALE   = 1_000_000   # 1e6
SPEED_SCALE = 1_000       # 1e3
GAMMA_SCALE = 1_000_000   # 1e6  (flight-path angle, KIN-002 — seal v1.6r0)

# Auxiliary WEAPON-001 scales (must match config/rails/atm.json wire.weapon). hp, fire_cd and
# damage are small magnitudes (O(1)..O(150)) carried to milli-resolution (1e3); the kernel's
# values are integer-valued so this is exact in practice but transmits any double faithfully.
HP_SCALE     = 1_000      # 1e3  (hitpoints; may be negative on a dead frame — ZigZag handles sign)
FIRECD_SCALE = 1_000      # 1e3  (fire-rate cooldown, ticks remaining)
DAMAGE_SCALE = 1_000      # 1e3  (per-round damage carried by the projectile)


class EntityState:
    """One aircraft on the wire. GEO fields in GEO-001 units (degrees / metres); KIN fields in
    KIN units (phi/gamma degrees, tas m/s); WEAPON fields hp/fire_cd (kernel units). All of
    phi/tas/gamma/hp/fire_cd default to 0.0 for callers building lower-protocol frames."""
    __slots__ = ("id", "lat_deg", "lon_deg", "bearing_deg", "alt_m", "phi_deg", "tas_mps",
                 "gamma_deg", "hp", "fire_cd")

    def __init__(self, id, lat_deg, lon_deg, bearing_deg, alt_m, phi_deg=0.0, tas_mps=0.0,
                 gamma_deg=0.0, hp=0.0, fire_cd=0.0):
        self.id = id
        self.lat_deg = lat_deg
        self.lon_deg = lon_deg
        self.bearing_deg = bearing_deg
        self.alt_m = alt_m
        self.phi_deg = phi_deg
        self.tas_mps = tas_mps
        self.gamma_deg = gamma_deg
        self.hp = hp
        self.fire_cd = fire_cd

    def __eq__(self, o):
        return (self.id == o.id and self.lat_deg == o.lat_deg and self.lon_deg == o.lon_deg
                and self.bearing_deg == o.bearing_deg and self.alt_m == o.alt_m
                and self.phi_deg == o.phi_deg and self.tas_mps == o.tas_mps
                and self.gamma_deg == o.gamma_deg and self.hp == o.hp
                and self.fire_cd == o.fire_cd)


class ProjectileState:
    """One ballistic round on the wire (WEAPON-001). GEO fields (lat/lon/bearing=psi/alt) over
    GEO-001; damage in kernel units (carried per round); ttl/owner are integer counters carried
    EXACTLY (the round's time-to-live in ticks and the firing aircraft index)."""
    __slots__ = ("id", "lat_deg", "lon_deg", "bearing_deg", "alt_m", "damage", "ttl", "owner")

    def __init__(self, id, lat_deg, lon_deg, bearing_deg, alt_m, damage, ttl, owner):
        self.id = id
        self.lat_deg = lat_deg
        self.lon_deg = lon_deg
        self.bearing_deg = bearing_deg
        self.alt_m = alt_m
        self.damage = damage
        self.ttl = ttl
        self.owner = owner

    def __eq__(self, o):
        return (self.id == o.id and self.lat_deg == o.lat_deg and self.lon_deg == o.lon_deg
                and self.bearing_deg == o.bearing_deg and self.alt_m == o.alt_m
                and self.damage == o.damage and self.ttl == o.ttl and self.owner == o.owner)


class Snapshot:
    __slots__ = ("protocol", "server_tick", "entities", "projectiles")

    def __init__(self, server_tick, entities, projectiles=None, protocol=SNAPSHOT_PROTOCOL):
        self.protocol = protocol
        self.server_tick = server_tick
        self.entities = list(entities)
        self.projectiles = list(projectiles) if projectiles else []


def from_kernel(id, lat_rad, lon_rad, psi_rad, alt_m, phi_rad=0.0, tas_mps=0.0, gamma_rad=0.0,
                hp=0.0, fire_cd=0.0):
    """Build a wire EntityState from raw kernel state (radians/metres). The kernel `psi`
    heading maps to GEO-001 `bearing`; `phi`/`gamma` (rad) -> KIN degrees; `tas` passes through
    (m/s); `hp`/`fire_cd` (WEAPON-001) pass through (kernel units). Transmits state faithfully
    (no lon/heading normalization — that's a presentation concern for the client)."""
    return EntityState(id, lat_rad * RAD2DEG, lon_rad * RAD2DEG, psi_rad * RAD2DEG, alt_m,
                       phi_rad * RAD2DEG, tas_mps, gamma_rad * RAD2DEG, hp, fire_cd)


def proj_from_kernel(id, lat_rad, lon_rad, psi_rad, alt_m, damage, ttl, owner):
    """Build a wire ProjectileState from raw kernel projectile state (radians/metres). The
    round's heading `psi` maps to GEO-001 `bearing`; `damage` passes through; `ttl`/`owner`
    are integer counters carried exactly."""
    return ProjectileState(id, lat_rad * RAD2DEG, lon_rad * RAD2DEG, psi_rad * RAD2DEG, alt_m,
                           damage, ttl, owner)


def to_kernel(entity):
    """Inverse of from_kernel up to the quantum: degrees -> radians, tas passthrough.
    Returns (lat_rad, lon_rad, psi_rad, alt_m, phi_rad, tas_mps, gamma_rad)."""
    return (entity.lat_deg * DEG2RAD, entity.lon_deg * DEG2RAD,
            entity.bearing_deg * DEG2RAD, entity.alt_m,
            entity.phi_deg * DEG2RAD, entity.tas_mps, entity.gamma_deg * DEG2RAD)


# ---- wire framing --------------------------------------------------------------------
# header (protocol, tick, n)
#   GEO section:    n * (id, GeoPoint[lat,lon,bearing,alt])          <- GEO-001, geography-only
#   KIN section:    n * (id, phi_q, tas_q [, gamma_q])   [iff protocol >= 2]    <- aux block
#   WEAPON section: [iff protocol >= 4]                              <- aux block WEAPON-001
#       n * (id, hp_q, fire_cd_q)
#       m  (projectile count)
#       m * (pid, GeoPoint[lat,lon,bearing,alt], damage_q, ttl, owner)
# All sections are self-delimiting (LEB128); the GEO-001 codec is reused verbatim (untouched).
def encode_snapshot(snap):
    out = bytearray()
    out += g.encode_i64(snap.protocol)
    out += g.encode_i64(snap.server_tick)
    out += g.encode_i64(len(snap.entities))
    for e in snap.entities:                       # GEO section
        out += g.encode_i64(e.id)
        out += g.encode_point(e.lat_deg, e.lon_deg, e.bearing_deg, e.alt_m)
    if snap.protocol >= 2:                         # KIN section (auxiliary, non-geographic)
        for e in snap.entities:
            out += g.encode_i64(e.id)
            out += g.encode_i64(g.quantize(e.phi_deg, PHI_SCALE))
            out += g.encode_i64(g.quantize(e.tas_mps, SPEED_SCALE))
            if snap.protocol >= 3:                  # KIN-002: gamma (flight-path angle)
                out += g.encode_i64(g.quantize(e.gamma_deg, GAMMA_SCALE))
    if snap.protocol >= 4:                          # WEAPON section (WEAPON-001, gunnery state)
        for e in snap.entities:                     # per-aircraft hp + fire-rate cooldown
            out += g.encode_i64(e.id)
            out += g.encode_i64(g.quantize(e.hp, HP_SCALE))
            out += g.encode_i64(g.quantize(e.fire_cd, FIRECD_SCALE))
        out += g.encode_i64(len(snap.projectiles))  # live ballistic rounds
        for p in snap.projectiles:
            out += g.encode_i64(p.id)
            out += g.encode_point(p.lat_deg, p.lon_deg, p.bearing_deg, p.alt_m)
            out += g.encode_i64(g.quantize(p.damage, DAMAGE_SCALE))
            out += g.encode_i64(p.ttl)              # exact integer counter (kernel u32)
            out += g.encode_i64(p.owner)            # exact integer index  (kernel u32)
    return bytes(out)


def decode_snapshot(data, pos=0):
    protocol, pos = g.decode_i64(data, pos)
    server_tick, pos = g.decode_i64(data, pos)
    n, pos = g.decode_i64(data, pos)
    if n < 0:
        raise ValueError("snapshot: negative entity count")
    entities = []
    for _ in range(n):                            # GEO section
        eid, pos = g.decode_i64(data, pos)
        pt, pos = g.decode_point(data, pos)
        entities.append(EntityState(eid, pt["lat"], pt["lon"], pt["bearing"], pt["alt"]))
    if protocol >= 2:                             # KIN section
        for k in range(n):
            kid, pos = g.decode_i64(data, pos)
            if kid != entities[k].id:
                raise ValueError("snapshot: KIN id does not match GEO section")
            phi_q, pos = g.decode_i64(data, pos)
            tas_q, pos = g.decode_i64(data, pos)
            entities[k].phi_deg = g.dequantize(phi_q, PHI_SCALE)
            entities[k].tas_mps = g.dequantize(tas_q, SPEED_SCALE)
            if protocol >= 3:                         # KIN-002: gamma (flight-path angle)
                gamma_q, pos = g.decode_i64(data, pos)
                entities[k].gamma_deg = g.dequantize(gamma_q, GAMMA_SCALE)
    projectiles = []
    if protocol >= 4:                             # WEAPON section (WEAPON-001)
        for k in range(n):                        # per-aircraft hp + fire_cd
            wid, pos = g.decode_i64(data, pos)
            if wid != entities[k].id:
                raise ValueError("snapshot: WEAPON id does not match GEO section")
            hp_q, pos = g.decode_i64(data, pos)
            firecd_q, pos = g.decode_i64(data, pos)
            entities[k].hp = g.dequantize(hp_q, HP_SCALE)
            entities[k].fire_cd = g.dequantize(firecd_q, FIRECD_SCALE)
        m, pos = g.decode_i64(data, pos)
        if m < 0:
            raise ValueError("snapshot: negative projectile count")
        for _ in range(m):
            pid, pos = g.decode_i64(data, pos)
            pt, pos = g.decode_point(data, pos)
            dmg_q, pos = g.decode_i64(data, pos)
            ttl, pos = g.decode_i64(data, pos)
            owner, pos = g.decode_i64(data, pos)
            projectiles.append(ProjectileState(pid, pt["lat"], pt["lon"], pt["bearing"],
                                               pt["alt"], g.dequantize(dmg_q, DAMAGE_SCALE),
                                               ttl, owner))
    return Snapshot(server_tick, entities, projectiles, protocol), pos


# ---- self-test -------------------------------------------------------------------------
def _selftest():
    fails = 0

    # empty snapshot round-trip (default protocol 4; empty KIN/WEAPON sections are present but
    # carry only the zero projectile count for the weapon block).
    s0 = Snapshot(0, [])
    d0, p0 = decode_snapshot(encode_snapshot(s0))
    if p0 != len(encode_snapshot(s0)) or d0.protocol != 4 or d0.entities or d0.projectiles:
        print("FAIL empty snapshot"); fails += 1

    # multi-aircraft + live rounds round-trip within one quantum, incl. the sealed golden start.
    # from_kernel carries phi (bank) + tas + gamma + hp + fire_cd; proj_from_kernel carries rounds.
    ents = [from_kernel(1, 0.0, 0.0, 0.7853981633974483, 1000.0, 0.0, 250.0, 0.0, 150.0, 0.0),  # P-47 hp150
            from_kernel(2, 0.5, -1.2, 3.0, 7999.5, 0.6981317007977318, 140.0, 0.3, 70.0, 9.0),  # A6M2 hp70, cd9
            from_kernel(7, -0.3, 2.9, 6.2, 0.0, -1.0471975511965976, 95.5, -0.5, 0.0, 0.0)]     # dead (hp0)
    projs = [proj_from_kernel(100, 0.01, 0.02, 0.7853981633974483, 1005.0, 12.0, 248, 0),       # p47 .50cal
             proj_from_kernel(101, 0.49, -1.19, 3.0, 7990.0, 40.0, 5, 1)]                        # a6m2 20mm
    snap = Snapshot(12345, ents, projs)
    wire = encode_snapshot(snap)
    dec, pos = decode_snapshot(wire)
    if pos != len(wire) or dec.server_tick != 12345 or len(dec.entities) != 3:
        print("FAIL multi header/count"); fails += 1
    if dec.protocol != 4:
        print("FAIL protocol != 4"); fails += 1
    if len(dec.projectiles) != 2:
        print("FAIL projectile count"); fails += 1
    for a, b in zip(ents, dec.entities):
        if a.id != b.id: print(f"FAIL id {a.id}"); fails += 1
        if abs(a.lat_deg - b.lat_deg) > 1.0 / g.LATLON_SCALE: print("FAIL lat"); fails += 1
        if abs(a.lon_deg - b.lon_deg) > 1.0 / g.LATLON_SCALE: print("FAIL lon"); fails += 1
        if abs(a.bearing_deg - b.bearing_deg) > 1.0 / g.BEARING_SCALE: print("FAIL bear"); fails += 1
        if abs(a.alt_m - b.alt_m) > 1.0 / g.ALT_SCALE: print("FAIL alt"); fails += 1
        if abs(a.phi_deg - b.phi_deg) > 1.0 / PHI_SCALE: print("FAIL phi"); fails += 1
        if abs(a.tas_mps - b.tas_mps) > 1.0 / SPEED_SCALE: print("FAIL tas"); fails += 1
        if abs(a.gamma_deg - b.gamma_deg) > 1.0 / GAMMA_SCALE: print("FAIL gamma"); fails += 1
        if abs(a.hp - b.hp) > 1.0 / HP_SCALE: print("FAIL hp"); fails += 1
        if abs(a.fire_cd - b.fire_cd) > 1.0 / FIRECD_SCALE: print("FAIL fire_cd"); fails += 1
    for a, b in zip(projs, dec.projectiles):
        if a.id != b.id: print(f"FAIL proj id {a.id}"); fails += 1
        if abs(a.lat_deg - b.lat_deg) > 1.0 / g.LATLON_SCALE: print("FAIL proj lat"); fails += 1
        if abs(a.lon_deg - b.lon_deg) > 1.0 / g.LATLON_SCALE: print("FAIL proj lon"); fails += 1
        if abs(a.bearing_deg - b.bearing_deg) > 1.0 / g.BEARING_SCALE: print("FAIL proj bear"); fails += 1
        if abs(a.alt_m - b.alt_m) > 1.0 / g.ALT_SCALE: print("FAIL proj alt"); fails += 1
        if abs(a.damage - b.damage) > 1.0 / DAMAGE_SCALE: print("FAIL proj damage"); fails += 1
        if a.ttl != b.ttl: print("FAIL proj ttl (must be exact)"); fails += 1
        if a.owner != b.owner: print("FAIL proj owner (must be exact)"); fails += 1

    # a protocol-3 snapshot (KIN-002 shape) carries phi/tas/gamma but NOT the weapon block.
    s3 = Snapshot(8, [from_kernel(1, 0.1, 0.2, 1.0, 500.0, 0.5, 130.0, 0.4, 99.0, 2.0)],
                  [proj_from_kernel(9, 0.1, 0.2, 1.0, 500.0, 25.0, 100, 0)], protocol=3)
    d3, q3 = decode_snapshot(encode_snapshot(s3))
    if q3 != len(encode_snapshot(s3)) or d3.protocol != 3: print("FAIL proto3 frame"); fails += 1
    if abs(d3.entities[0].gamma_deg - 0.4 * RAD2DEG) > 1.0 / GAMMA_SCALE:
        print("FAIL proto3 gamma"); fails += 1
    if d3.entities[0].hp != 0.0 or d3.entities[0].fire_cd != 0.0 or d3.projectiles:
        print("FAIL proto3 should not carry WEAPON (KIN-002)"); fails += 1

    # a protocol-2 snapshot (KIN-001 shape) carries phi/tas but NOT gamma/weapon (back-compat).
    s2 = Snapshot(8, [from_kernel(1, 0.1, 0.2, 1.0, 500.0, 0.5, 130.0, 0.4, 99.0, 2.0)], protocol=2)
    d2, q2 = decode_snapshot(encode_snapshot(s2))
    if q2 != len(encode_snapshot(s2)) or d2.protocol != 2: print("FAIL proto2 frame"); fails += 1
    if abs(d2.entities[0].phi_deg - 0.5 * RAD2DEG) > 1.0 / PHI_SCALE:
        print("FAIL proto2 phi"); fails += 1
    if d2.entities[0].gamma_deg != 0.0 or d2.entities[0].hp != 0.0:
        print("FAIL proto2 should not carry gamma/weapon"); fails += 1

    # a protocol-1 snapshot omits KIN+WEAPON (back-compat): geo round-trips, the rest stays 0.
    s1 = Snapshot(7, [from_kernel(1, 0.1, 0.2, 1.0, 500.0, 0.5, 130.0, 0.4, 99.0, 2.0)], protocol=1)
    d1, q1 = decode_snapshot(encode_snapshot(s1))
    if q1 != len(encode_snapshot(s1)) or d1.protocol != 1: print("FAIL proto1 frame"); fails += 1
    if (d1.entities[0].phi_deg != 0.0 or d1.entities[0].tas_mps != 0.0
            or d1.entities[0].gamma_deg != 0.0 or d1.entities[0].hp != 0.0):
        print("FAIL proto1 should not carry KIN/WEAPON"); fails += 1

    # rad->deg conversion sanity (pi -> 180 deg exactly via the shared constant path)
    e = from_kernel(0, PI, 0.0, 0.0, 0.0)
    if abs(e.lat_deg - 180.0) > 1e-9:
        print(f"FAIL rad2deg {e.lat_deg}"); fails += 1

    if fails == 0:
        print("RESULT: GEO-001 SNAPSHOT REFERENCE SELFTEST PASS")
        return 0
    print(f"RESULT: GEO-001 SNAPSHOT REFERENCE SELFTEST FAIL ({fails})")
    return 1


if __name__ == "__main__":
    sys.exit(_selftest())
