"""Round-trip / framing properties for the GEO-001 snapshot serialization.

Byte-exact C++<->reference parity is proven by the generated-vector gate
(src/net/snapshot_test_main.cpp). Here we prove the reference framing is self-consistent:
multi-entity snapshots round-trip within one quantum, the header (protocol/tick/count) is
preserved, empty worlds work, and from_kernel/to_kernel invert within tolerance."""
import sys
from pathlib import Path

from hypothesis import given, strategies as st

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import geo001_ref as g
import snapshot_ref as s

LAT = st.floats(min_value=-90.0, max_value=90.0, allow_nan=False, allow_infinity=False)
LON = st.floats(min_value=-180.0, max_value=180.0, allow_nan=False, allow_infinity=False)
BEAR = st.floats(min_value=0.0, max_value=360.0, allow_nan=False, allow_infinity=False)
ALT = st.floats(min_value=-100.0, max_value=8000.0, allow_nan=False, allow_infinity=False)
ID = st.integers(min_value=0, max_value=(1 << 31) - 1)
TICK = st.integers(min_value=0, max_value=(1 << 40))

ENTITY = st.builds(s.EntityState, ID, LAT, LON, BEAR, ALT)


@given(TICK, st.lists(ENTITY, max_size=8))
def test_snapshot_roundtrip(tick, entities):
    snap = s.Snapshot(tick, entities)
    wire = s.encode_snapshot(snap)
    dec, pos = s.decode_snapshot(wire)
    assert pos == len(wire)
    assert dec.protocol == s.SNAPSHOT_PROTOCOL
    assert dec.server_tick == tick
    assert len(dec.entities) == len(entities)
    for a, b in zip(entities, dec.entities):
        assert a.id == b.id
        assert abs(a.lat_deg - b.lat_deg) <= 1.0 / g.LATLON_SCALE
        assert abs(a.lon_deg - b.lon_deg) <= 1.0 / g.LATLON_SCALE
        assert abs(a.bearing_deg - b.bearing_deg) <= 1.0 / g.BEARING_SCALE
        assert abs(a.alt_m - b.alt_m) <= 1.0 / g.ALT_SCALE


def test_empty_snapshot():
    wire = s.encode_snapshot(s.Snapshot(0, []))
    dec, pos = s.decode_snapshot(wire)
    assert pos == len(wire) and dec.entities == []


@given(st.lists(ENTITY, min_size=1, max_size=4))
def test_snapshot_is_self_delimiting(entities):
    # decoding stops exactly at the snapshot's end even with trailing bytes following.
    wire = s.encode_snapshot(s.Snapshot(7, entities))
    dec, pos = s.decode_snapshot(wire + b"\xde\xad\xbe\xef")
    assert pos == len(wire)
    assert len(dec.entities) == len(entities)


@given(LAT, LON, BEAR, ALT)
def test_from_to_kernel_roundtrip(lat_deg, lon_deg, bearing_deg, alt):
    # build kernel-radian inputs, go to wire degrees and back; within one quantum (radians).
    import math
    lat_r, lon_r, psi_r = (math.radians(lat_deg), math.radians(lon_deg), math.radians(bearing_deg))
    e = s.from_kernel(3, lat_r, lon_r, psi_r, alt)
    back_lat, back_lon, back_psi, back_alt = s.to_kernel(e)
    assert abs(back_lat - lat_r) <= 2e-9
    assert abs(back_lon - lon_r) <= 2e-9
    assert abs(back_psi - psi_r) <= 2e-9
    assert back_alt == alt


def test_protocol_constant_on_wire():
    # the protocol version is the first field; a decoder can gate on it.
    wire = s.encode_snapshot(s.Snapshot(0, []))
    proto, _ = g.decode_i64(wire, 0)
    assert proto == s.SNAPSHOT_PROTOCOL
