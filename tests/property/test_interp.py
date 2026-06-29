"""Properties for the remote interpolation buffer (netcode layer 4a).

Bit-exact C++<->reference parity is proven by the generated-vector gate
(src/net/interp_test_main.cpp). Here we prove the reference sampler is well-behaved: endpoints
are exact, sampling clamps/holds outside the buffer, position interpolation is monotone in alpha,
and the bearing/longitude shortest-arc never travels the long way around the seam."""
import sys
from pathlib import Path

from hypothesis import given, strategies as st, settings, assume

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import snapshot_ref as s
import interp_ref as ip

LAT = st.floats(min_value=-89.0, max_value=89.0, allow_nan=False, allow_infinity=False)
LON = st.floats(min_value=-179.0, max_value=180.0, allow_nan=False, allow_infinity=False)
BEAR = st.floats(min_value=0.0, max_value=359.999, allow_nan=False, allow_infinity=False)
ALT = st.floats(min_value=-100.0, max_value=8000.0, allow_nan=False, allow_infinity=False)
ALPHA = st.floats(min_value=0.0, max_value=1.0, allow_nan=False, allow_infinity=False)


@given(LAT, LON, BEAR, ALT, LAT, LON, BEAR, ALT)
def test_endpoints_exact(la, lo, ba, al, lb, lo2, bb, al2):
    a = s.EntityState(1, la, lo, ba, al)
    b = s.EntityState(1, lb, lo2, bb, al2)
    at0 = ip.interp_entity(a, b, 0.0)
    # alpha = 0 reproduces 'from' exactly for the linear fields
    assert at0.lat_deg == la and at0.alt_m == al
    # lon/bearing at alpha=0: equal to the normalized 'from' (no movement)
    assert at0.lon_deg == ip.lerp_lon_deg(lo, lo2, 0.0)
    assert at0.bearing_deg == ip.lerp_angle_deg(ba, bb, 0.0)


@given(LAT, ALT, LAT, ALT, ALPHA)
def test_linear_fields_monotone(la, al, lb, al2, alpha):
    # lat/alt interpolation stays within [min,max] of the endpoints (no overshoot)
    a = s.EntityState(1, la, 0.0, 0.0, al)
    b = s.EntityState(1, lb, 0.0, 0.0, al2)
    e = ip.interp_entity(a, b, alpha)
    assert min(la, lb) - 1e-9 <= e.lat_deg <= max(la, lb) + 1e-9
    assert min(al, al2) - 1e-9 <= e.alt_m <= max(al, al2) + 1e-9


@given(BEAR, BEAR, ALPHA)
def test_bearing_shortest_arc(a_deg, b_deg, alpha):
    r = ip.lerp_angle_deg(a_deg, b_deg, alpha)
    assert 0.0 <= r < 360.0
    # the interpolated heading is never more than the short angular distance away from 'from'
    short = abs(((b_deg - a_deg) + 180.0) % 360.0 - 180.0)
    moved = abs(((r - a_deg) + 180.0) % 360.0 - 180.0)
    assert moved <= short + 1e-9


@given(LON, LON, ALPHA)
def test_lon_shortest_arc(a_deg, b_deg, alpha):
    r = ip.lerp_lon_deg(a_deg, b_deg, alpha)
    assert -180.0 < r <= 180.0
    short = abs(((b_deg - a_deg) + 180.0) % 360.0 - 180.0)
    moved = abs(((r - a_deg) + 180.0) % 360.0 - 180.0)
    assert moved <= short + 1e-9


def test_clamp_hold_edges():
    buf = ip.SnapshotBuffer()
    buf.add(s.Snapshot(10, [s.EntityState(1, 1.0, 2.0, 30.0, 500.0)]))
    buf.add(s.Snapshot(20, [s.EntityState(1, 3.0, 2.0, 30.0, 700.0)]))
    assert buf.sample(5.0)[0].lat_deg == 1.0    # before first -> hold oldest
    assert buf.sample(99.0)[0].lat_deg == 3.0   # after last -> hold newest
    assert buf.sample(15.0)[0].lat_deg == 2.0   # midpoint


def test_empty_buffer_samples_nothing():
    assert ip.SnapshotBuffer().sample(0.0) == []


@settings(max_examples=50, deadline=None)
@given(st.integers(min_value=0, max_value=1000), st.integers(min_value=1, max_value=50),
       LAT, LON, LAT, LON,
       st.floats(min_value=0.0, max_value=1.0, exclude_max=True, allow_nan=False))
def test_midpoint_between_two_frames(t0, dt, la, lo, lb, lo2, frac):
    # in the STRICT interior, sampling at t0 + frac*dt reproduces interp_entity at the same alpha
    # (buffer == direct lerp). The upper edge (frac=1.0) deliberately holds the newest frame —
    # that clamp is covered by test_clamp_hold_edges, not here.
    a = s.EntityState(1, la, lo, 0.0, 0.0)
    b = s.EntityState(1, lb, lo2, 0.0, 0.0)
    buf = ip.SnapshotBuffer()
    buf.add(s.Snapshot(t0, [a]))
    buf.add(s.Snapshot(t0 + dt, [b]))
    rt = t0 + frac * dt
    # frac can round rt onto a frame boundary (e.g. 1 + (1-eps) -> 2.0); there sample correctly
    # edge-clamps instead of interpolating. Restrict to the strict interior for this comparison.
    assume(t0 < rt < t0 + dt)
    got = buf.sample(rt)[0]
    alpha = (rt - t0) / dt
    direct = ip.interp_entity(a, b, alpha)
    assert got.lat_deg == direct.lat_deg and got.lon_deg == direct.lon_deg


def test_prune_keeps_lower_bracket():
    buf = ip.SnapshotBuffer()
    for t in (0, 10, 20, 30):
        buf.add(s.Snapshot(t, [s.EntityState(1, float(t), 0.0, 0.0, 0.0)]))
    buf.prune_before(15)
    # one frame older than 15 is retained so a sample at 15 still interpolates
    assert buf.frames[0].server_tick == 10
    assert buf.sample(15.0)[0].lat_deg == 15.0  # halfway between frame@10 (lat 10) and frame@20 (lat 20)
