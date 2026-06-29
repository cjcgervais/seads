#!/usr/bin/env python3
"""
interp_ref.py — SEADS remote interpolation REFERENCE (netcode layer 4a).

A client renders REMOTE aircraft ~100 ms in the past so it always has two received snapshots to
interpolate between (smooth motion despite 20 Hz updates / jitter / loss). This is the buffer +
sampler that does it. It is a CLIENT-side, downstream-only presentation concern: it consumes
DECODED GEO-001 snapshots (tools/snapshot_ref.py) and produces interpolated EntityStates for the
renderer. It NEVER feeds the sim — net code stays strictly outside the kernel.

Why this needs NO reseal
------------------------
Interpolation uses only lat/lon/bearing/alt — all already on the GEO-001 wire (layer 2). It does
NOT need bank (phi) or speed (tas), so it carries no new wire fields. (Full client-side
*prediction* of your OWN aircraft DOES need phi/tas and is a rail reseal — deferred to layer 4b.)

Determinism / cross-impl parity
-------------------------------
The math is pure IEEE +,-,*,/ and comparisons — NO transcendentals, so it stays off the det_math
path (the C++ mirror lives in seads_net, which links neither det_math nor the kernel). Under the
strict-FP / no-FMA flags those ops are individually rounded and reproduce BIT-FOR-BIT across
MSVC/GCC/Clang × x64/AArch64, exactly like the geo001/snapshot codecs. The generated-vector gate
asserts the C++ sampler reproduces these doubles bit-for-bit.

Design choices (deliberate, documented)
---------------------------------------
- **Linear interpolation** of lat/lon/alt in wire units (degrees/metres). NOT spherical slerp:
  slerp needs sin/cos/acos (transcendentals → det_math or non-reproducible), and over a ~100 ms
  window on R=15 km the chord error is negligible and this is lossy presentation anyway. Documented,
  not silent.
- **Shortest-arc** interpolation of bearing (heading wraps at 360°): interpolate along the shorter
  direction so 350°→10° passes through 0°, not backwards through 180°. Bearings are assumed in
  [0,360) as the kernel produces them (psi is wrap_2pi'd).
- **Clamp/hold at the buffer edges** (no extrapolation): before the oldest frame → hold oldest;
  after the newest → hold newest (freeze). Dead-reckoning past the newest snapshot is a later
  concern (needs velocity/prediction). Holding is the safe, jitter-free default.
- **Entity set = the 'from' frame.** An entity present in 'from' but not 'to' holds its 'from'
  value; an entity that first appears in 'to' renders once it becomes a 'from' frame. Simple,
  avoids fade-in/out flicker policy here.

Usage:  python tools/interp_ref.py        # internal self-test
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import snapshot_ref as s


def lerp(a, b, alpha):
    """Linear interpolation, exact op order shared with the C++ mirror: a + (b - a) * alpha."""
    return a + (b - a) * alpha


def lerp_angle_deg(a, b, alpha):
    """Shortest-arc interpolation of two bearings in degrees, output in [0,360) (heading)."""
    diff = b - a
    if diff > 180.0:
        diff = diff - 360.0
    elif diff <= -180.0:
        diff = diff + 360.0
    r = a + diff * alpha
    # sequential (not elif): a +360 correction on a tiny-negative r can round to exactly 360.0,
    # which the second test then folds back to canonical [0,360).
    if r < 0.0:
        r = r + 360.0
    if r >= 360.0:
        r = r - 360.0
    return r


def lerp_lon_deg(a, b, alpha):
    """Shortest-arc interpolation of two longitudes in degrees, output in (-180,180].
    Longitude wraps at +/-180 (the antimeridian), so crossing it must take the short way
    around the sphere, not race linearly back through 0. Latitude does NOT wrap (it stays in
    [-90,90]) so it uses plain lerp()."""
    diff = b - a
    if diff > 180.0:
        diff = diff - 360.0
    elif diff <= -180.0:
        diff = diff + 360.0
    r = a + diff * alpha
    # sequential (not elif) for the same rounding-to-boundary reason as lerp_angle_deg.
    if r > 180.0:
        r = r - 360.0
    if r <= -180.0:
        r = r + 360.0
    return r


def interp_entity(e_from, e_to, alpha):
    """Interpolate one entity (same id) between two wire EntityStates at fraction alpha.
    lat: plain lerp (no wrap). lon: shortest-arc across the antimeridian. bearing: shortest-arc
    heading. alt: plain lerp."""
    return s.EntityState(
        e_from.id,
        lerp(e_from.lat_deg, e_to.lat_deg, alpha),
        lerp_lon_deg(e_from.lon_deg, e_to.lon_deg, alpha),
        lerp_angle_deg(e_from.bearing_deg, e_to.bearing_deg, alpha),
        lerp(e_from.alt_m, e_to.alt_m, alpha),
    )


class SnapshotBuffer:
    """Time-ordered buffer of decoded snapshots; samples interpolated entities at a render tick.

    Frames are kept sorted by server_tick ascending. add() expects non-decreasing server_tick
    (the normal arrival order); a frame with a duplicate tick replaces the prior one. The render
    tick is in 100 Hz tick units (the same base as server_tick) and may be fractional; the caller
    chooses the ~100 ms delay (render_tick = latest_tick - delay_ticks)."""

    def __init__(self):
        self.frames = []  # list of Snapshot, ascending server_tick

    def add(self, snap):
        t = snap.server_tick
        # replace on duplicate tick, else insert keeping ascending order (append is the fast path)
        for i, f in enumerate(self.frames):
            if f.server_tick == t:
                self.frames[i] = snap
                return
            if f.server_tick > t:
                self.frames.insert(i, snap)
                return
        self.frames.append(snap)

    def prune_before(self, tick):
        """Drop frames strictly older than `tick`, keeping at most one as the lower bracket."""
        keep = [f for f in self.frames if f.server_tick >= tick]
        # retain the newest dropped frame so a sample at `tick` still has a lower bracket
        older = [f for f in self.frames if f.server_tick < tick]
        if older:
            keep = [older[-1]] + keep
        self.frames = keep

    def sample(self, render_tick):
        """Return the list of interpolated EntityStates at render_tick (clamp/hold at edges)."""
        if not self.frames:
            return []
        if render_tick <= self.frames[0].server_tick:
            return [interp_entity(e, e, 0.0) for e in self.frames[0].entities]
        if render_tick >= self.frames[-1].server_tick:
            return [interp_entity(e, e, 0.0) for e in self.frames[-1].entities]
        # find bracket [i, i+1] with t_i <= render_tick < t_{i+1}
        i = 0
        while i + 1 < len(self.frames) and self.frames[i + 1].server_tick <= render_tick:
            i += 1
        f0, f1 = self.frames[i], self.frames[i + 1]
        t0, t1 = f0.server_tick, f1.server_tick
        alpha = (render_tick - t0) / (t1 - t0)
        by_id = {e.id: e for e in f1.entities}
        out = []
        for e in f0.entities:
            g = by_id.get(e.id)
            out.append(interp_entity(e, g, alpha) if g is not None else interp_entity(e, e, 0.0))
        return out


def _selftest():
    fails = 0

    def E(i, lat, lon, bear, alt):
        return s.EntityState(i, lat, lon, bear, alt)

    # midpoint interpolation of position + altitude
    buf = SnapshotBuffer()
    buf.add(s.Snapshot(0, [E(1, 0.0, 0.0, 10.0, 1000.0)]))
    buf.add(s.Snapshot(10, [E(1, 2.0, -4.0, 50.0, 2000.0)]))
    r = buf.sample(5.0)
    e = r[0]
    if not (e.lat_deg == 1.0 and e.lon_deg == -2.0 and e.alt_m == 1500.0 and e.bearing_deg == 30.0):
        print(f"FAIL midpoint: {e.lat_deg},{e.lon_deg},{e.bearing_deg},{e.alt_m}"); fails += 1

    # endpoints: alpha=0 -> 'from' exactly, render at/after last -> 'to' (hold)
    if buf.sample(0.0)[0].lat_deg != 0.0:
        print("FAIL alpha=0 endpoint"); fails += 1
    if buf.sample(10.0)[0].lat_deg != 2.0 or buf.sample(99.0)[0].lat_deg != 2.0:
        print("FAIL hold-after-last"); fails += 1
    if buf.sample(-5.0)[0].lat_deg != 0.0:
        print("FAIL hold-before-first"); fails += 1

    # bearing shortest arc across the 0/360 seam: 350 -> 10 midpoint is 0, not 180
    b = SnapshotBuffer()
    b.add(s.Snapshot(0, [E(1, 0.0, 0.0, 350.0, 0.0)]))
    b.add(s.Snapshot(10, [E(1, 0.0, 0.0, 10.0, 0.0)]))
    mid = b.sample(5.0)[0].bearing_deg
    if mid != 0.0:
        print(f"FAIL bearing seam: {mid}"); fails += 1

    # longitude shortest arc across the antimeridian: 170 -> -170 midpoint is +/-180, not 0
    lon = SnapshotBuffer()
    lon.add(s.Snapshot(0, [E(1, 0.0, 170.0, 0.0, 0.0)]))
    lon.add(s.Snapshot(10, [E(1, 0.0, -170.0, 0.0, 0.0)]))
    lmid = lon.sample(5.0)[0].lon_deg
    if lmid != 180.0:
        print(f"FAIL lon seam: {lmid} (expected 180, not 0)"); fails += 1
    # latitude does NOT wrap: 80 -> -80 midpoint is 0 (straight through, no seam)
    lat = SnapshotBuffer()
    lat.add(s.Snapshot(0, [E(1, 80.0, 0.0, 0.0, 0.0)]))
    lat.add(s.Snapshot(10, [E(1, -80.0, 0.0, 0.0, 0.0)]))
    if lat.sample(5.0)[0].lat_deg != 0.0:
        print("FAIL lat no-wrap"); fails += 1

    # multi-entity, entity only in 'from' holds its value
    m = SnapshotBuffer()
    m.add(s.Snapshot(0, [E(1, 0.0, 0.0, 0.0, 0.0), E(2, 10.0, 10.0, 90.0, 500.0)]))
    m.add(s.Snapshot(4, [E(1, 4.0, 0.0, 0.0, 0.0)]))               # entity 2 absent in 'to'
    rr = m.sample(2.0)
    held = next(x for x in rr if x.id == 2)
    moved = next(x for x in rr if x.id == 1)
    if held.lat_deg != 10.0 or moved.lat_deg != 2.0:
        print(f"FAIL hold-missing-entity: held={held.lat_deg} moved={moved.lat_deg}"); fails += 1

    if fails == 0:
        print("RESULT: REMOTE INTERPOLATION REFERENCE SELFTEST PASS")
        return 0
    print(f"RESULT: REMOTE INTERPOLATION REFERENCE SELFTEST FAIL ({fails})")
    return 1


if __name__ == "__main__":
    sys.exit(_selftest())
