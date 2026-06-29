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

Field scope (rail-faithful)
----------------------------
GEO-001 defines wire scales for lat/lon (1e7), bearing (1e6), alt (1e3) ONLY. The kernel's
per-aircraft state is (lat, lon, psi, phi, alt, tas); psi is the heading => GEO-001 bearing.
We transmit position + heading + altitude per aircraft — sufficient for REMOTE INTERPOLATION
(the ~100 ms buffer). `phi` (bank) and `tas` have NO GEO-001 scale, so carrying them on the
wire would be a rail change (reseal). Prediction state is therefore DEFERRED to a later layer
(reseal GEO-001 with new scales, or an auxiliary non-geographic block). This omission is
deliberate and documented, not a silent cap.

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

SNAPSHOT_PROTOCOL = 1   # GEO-001 snapshot framing version


class EntityState:
    """One aircraft on the wire, in GEO-001 transport units (degrees / metres)."""
    __slots__ = ("id", "lat_deg", "lon_deg", "bearing_deg", "alt_m")

    def __init__(self, id, lat_deg, lon_deg, bearing_deg, alt_m):
        self.id = id
        self.lat_deg = lat_deg
        self.lon_deg = lon_deg
        self.bearing_deg = bearing_deg
        self.alt_m = alt_m

    def __eq__(self, o):
        return (self.id == o.id and self.lat_deg == o.lat_deg and self.lon_deg == o.lon_deg
                and self.bearing_deg == o.bearing_deg and self.alt_m == o.alt_m)


class Snapshot:
    __slots__ = ("protocol", "server_tick", "entities")

    def __init__(self, server_tick, entities, protocol=SNAPSHOT_PROTOCOL):
        self.protocol = protocol
        self.server_tick = server_tick
        self.entities = list(entities)


def from_kernel(id, lat_rad, lon_rad, psi_rad, alt_m):
    """Build a wire EntityState from raw kernel state (radians/metres). The kernel `psi`
    heading maps to GEO-001 `bearing`. Transmits state faithfully (no lon/heading
    normalization — that's a presentation concern for the client)."""
    return EntityState(id, lat_rad * RAD2DEG, lon_rad * RAD2DEG, psi_rad * RAD2DEG, alt_m)


def to_kernel(entity):
    """Inverse of from_kernel up to the GEO-001 quantum: degrees -> radians."""
    return (entity.lat_deg * DEG2RAD, entity.lon_deg * DEG2RAD,
            entity.bearing_deg * DEG2RAD, entity.alt_m)


# ---- wire framing: header (protocol, tick, n) then n * (id, GeoPoint) ------------------
def encode_snapshot(snap):
    out = bytearray()
    out += g.encode_i64(snap.protocol)
    out += g.encode_i64(snap.server_tick)
    out += g.encode_i64(len(snap.entities))
    for e in snap.entities:
        out += g.encode_i64(e.id)
        out += g.encode_point(e.lat_deg, e.lon_deg, e.bearing_deg, e.alt_m)
    return bytes(out)


def decode_snapshot(data, pos=0):
    protocol, pos = g.decode_i64(data, pos)
    server_tick, pos = g.decode_i64(data, pos)
    n, pos = g.decode_i64(data, pos)
    if n < 0:
        raise ValueError("snapshot: negative entity count")
    entities = []
    for _ in range(n):
        eid, pos = g.decode_i64(data, pos)
        pt, pos = g.decode_point(data, pos)
        entities.append(EntityState(eid, pt["lat"], pt["lon"], pt["bearing"], pt["alt"]))
    return Snapshot(server_tick, entities, protocol), pos


# ---- self-test -------------------------------------------------------------------------
def _selftest():
    fails = 0

    # empty snapshot round-trip
    s0 = Snapshot(0, [])
    d0, p0 = decode_snapshot(encode_snapshot(s0))
    if p0 != len(encode_snapshot(s0)) or d0.protocol != 1 or d0.entities:
        print("FAIL empty snapshot"); fails += 1

    # multi-aircraft round-trip within one quantum, incl. the sealed golden start (0,0)
    ents = [from_kernel(1, 0.0, 0.0, 0.7853981633974483, 1000.0),       # 45 deg heading
            from_kernel(2, 0.5, -1.2, 3.0, 7999.5),
            from_kernel(7, -0.3, 2.9, 6.2, 0.0)]
    snap = Snapshot(12345, ents)
    wire = encode_snapshot(snap)
    dec, pos = decode_snapshot(wire)
    if pos != len(wire) or dec.server_tick != 12345 or len(dec.entities) != 3:
        print("FAIL multi header/count"); fails += 1
    for a, b in zip(ents, dec.entities):
        if a.id != b.id: print(f"FAIL id {a.id}"); fails += 1
        if abs(a.lat_deg - b.lat_deg) > 1.0 / g.LATLON_SCALE: print("FAIL lat"); fails += 1
        if abs(a.lon_deg - b.lon_deg) > 1.0 / g.LATLON_SCALE: print("FAIL lon"); fails += 1
        if abs(a.bearing_deg - b.bearing_deg) > 1.0 / g.BEARING_SCALE: print("FAIL bear"); fails += 1
        if abs(a.alt_m - b.alt_m) > 1.0 / g.ALT_SCALE: print("FAIL alt"); fails += 1

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
