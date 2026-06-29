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

Field scope (rail-faithful) — TWO sections since seal v1.4r0
------------------------------------------------------------
The kernel's per-aircraft state is (lat, lon, psi, phi, alt, tas); psi is the heading.
A snapshot now carries it in TWO back-to-back sections, both self-delimiting (LEB128):

  1. GEO section — n × (id, GeoPoint) over GEO-001 scales lat/lon 1e7, bearing 1e6, alt 1e3.
     GEO-001 stays GEOGRAPHY-ONLY (the sealed codec + its parity vectors are UNTOUCHED).
  2. KIN section — n × (id, phi_q, tas_q) over the auxiliary NON-geographic rail block
     `wire.kin` (KIN-001): phi 1e6 (micro-degree, mirrors bearing), tas 1e3 (mm/s, mirrors
     alt). Same ZigZag+LEB128 field codec; phi/tas are NOT geography so they live OUTSIDE
     the GeoPoint. The KIN section is present iff protocol >= 2.

Section 1 alone (protocol 1) supports REMOTE INTERPOLATION (the ~100 ms buffer). Section 2
adds bank+speed so a client can re-seed a kernel for CLIENT-SIDE PREDICTION (layer 4b) and
late-join. Carrying phi/tas was a rail reseal (v1.3r0 → v1.4r0): GEO-001 had no scale for
them, so seal v1.4r0 added the `wire.kin` block rather than polluting the geographic record.

Units: the kernel stores lat/lon/psi in RADIANS. We convert to DEGREES (the GEO-001 native
unit) before quantizing, via exact hex-float constants so C++ and Python agree to the bit.

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

SNAPSHOT_PROTOCOL = 2   # snapshot framing version (>=2 => carries the KIN section)

# Auxiliary KIN-001 scales (must match config/rails/atm.json wire.kin). Bank (phi) is an
# angle in degrees, so it reuses the bearing-style 1e6 (micro-degree); true airspeed (tas)
# is metres/second, so it reuses the alt-style 1e3 (mm/s).
PHI_SCALE   = 1_000_000   # 1e6
SPEED_SCALE = 1_000       # 1e3


class EntityState:
    """One aircraft on the wire. GEO fields in GEO-001 units (degrees / metres); KIN fields
    in KIN-001 units (phi degrees, tas m/s). phi/tas default to 0.0 for protocol-1 callers."""
    __slots__ = ("id", "lat_deg", "lon_deg", "bearing_deg", "alt_m", "phi_deg", "tas_mps")

    def __init__(self, id, lat_deg, lon_deg, bearing_deg, alt_m, phi_deg=0.0, tas_mps=0.0):
        self.id = id
        self.lat_deg = lat_deg
        self.lon_deg = lon_deg
        self.bearing_deg = bearing_deg
        self.alt_m = alt_m
        self.phi_deg = phi_deg
        self.tas_mps = tas_mps

    def __eq__(self, o):
        return (self.id == o.id and self.lat_deg == o.lat_deg and self.lon_deg == o.lon_deg
                and self.bearing_deg == o.bearing_deg and self.alt_m == o.alt_m
                and self.phi_deg == o.phi_deg and self.tas_mps == o.tas_mps)


class Snapshot:
    __slots__ = ("protocol", "server_tick", "entities")

    def __init__(self, server_tick, entities, protocol=SNAPSHOT_PROTOCOL):
        self.protocol = protocol
        self.server_tick = server_tick
        self.entities = list(entities)


def from_kernel(id, lat_rad, lon_rad, psi_rad, alt_m, phi_rad=0.0, tas_mps=0.0):
    """Build a wire EntityState from raw kernel state (radians/metres). The kernel `psi`
    heading maps to GEO-001 `bearing`; `phi` (bank, rad) -> KIN phi (deg); `tas` passes
    through (m/s). Transmits state faithfully (no lon/heading normalization — that's a
    presentation concern for the client)."""
    return EntityState(id, lat_rad * RAD2DEG, lon_rad * RAD2DEG, psi_rad * RAD2DEG, alt_m,
                       phi_rad * RAD2DEG, tas_mps)


def to_kernel(entity):
    """Inverse of from_kernel up to the quantum: degrees -> radians, tas passthrough.
    Returns (lat_rad, lon_rad, psi_rad, alt_m, phi_rad, tas_mps)."""
    return (entity.lat_deg * DEG2RAD, entity.lon_deg * DEG2RAD,
            entity.bearing_deg * DEG2RAD, entity.alt_m,
            entity.phi_deg * DEG2RAD, entity.tas_mps)


# ---- wire framing --------------------------------------------------------------------
# header (protocol, tick, n)
#   GEO section: n * (id, GeoPoint[lat,lon,bearing,alt])          <- GEO-001, geography-only
#   KIN section: n * (id, phi_q, tas_q)   [present iff protocol >= 2]   <- KIN-001, aux block
# Both sections are self-delimiting (LEB128); GEO-001 codec is reused verbatim (untouched).
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
    return Snapshot(server_tick, entities, protocol), pos


# ---- self-test -------------------------------------------------------------------------
def _selftest():
    fails = 0

    # empty snapshot round-trip (default protocol 2; empty KIN section is zero-length)
    s0 = Snapshot(0, [])
    d0, p0 = decode_snapshot(encode_snapshot(s0))
    if p0 != len(encode_snapshot(s0)) or d0.protocol != 2 or d0.entities:
        print("FAIL empty snapshot"); fails += 1

    # multi-aircraft round-trip within one quantum, incl. the sealed golden start (0,0).
    # from_kernel now carries phi (bank, rad) + tas (m/s) into the KIN section.
    ents = [from_kernel(1, 0.0, 0.0, 0.7853981633974483, 1000.0, 0.0, 250.0),   # 45deg hdg, wings level
            from_kernel(2, 0.5, -1.2, 3.0, 7999.5, 0.6981317007977318, 140.0),  # +40deg bank
            from_kernel(7, -0.3, 2.9, 6.2, 0.0, -1.0471975511965976, 95.5)]     # -60deg bank
    snap = Snapshot(12345, ents)
    wire = encode_snapshot(snap)
    dec, pos = decode_snapshot(wire)
    if pos != len(wire) or dec.server_tick != 12345 or len(dec.entities) != 3:
        print("FAIL multi header/count"); fails += 1
    if dec.protocol != 2:
        print("FAIL protocol != 2"); fails += 1
    for a, b in zip(ents, dec.entities):
        if a.id != b.id: print(f"FAIL id {a.id}"); fails += 1
        if abs(a.lat_deg - b.lat_deg) > 1.0 / g.LATLON_SCALE: print("FAIL lat"); fails += 1
        if abs(a.lon_deg - b.lon_deg) > 1.0 / g.LATLON_SCALE: print("FAIL lon"); fails += 1
        if abs(a.bearing_deg - b.bearing_deg) > 1.0 / g.BEARING_SCALE: print("FAIL bear"); fails += 1
        if abs(a.alt_m - b.alt_m) > 1.0 / g.ALT_SCALE: print("FAIL alt"); fails += 1
        if abs(a.phi_deg - b.phi_deg) > 1.0 / PHI_SCALE: print("FAIL phi"); fails += 1
        if abs(a.tas_mps - b.tas_mps) > 1.0 / SPEED_SCALE: print("FAIL tas"); fails += 1

    # a protocol-1 snapshot omits the KIN section (back-compat): geo round-trips, phi/tas stay 0.
    s1 = Snapshot(7, [from_kernel(1, 0.1, 0.2, 1.0, 500.0, 0.5, 130.0)], protocol=1)
    d1, q1 = decode_snapshot(encode_snapshot(s1))
    if q1 != len(encode_snapshot(s1)) or d1.protocol != 1: print("FAIL proto1 frame"); fails += 1
    if d1.entities[0].phi_deg != 0.0 or d1.entities[0].tas_mps != 0.0:
        print("FAIL proto1 should not carry KIN"); fails += 1

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
