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
PHI = st.floats(min_value=-90.0, max_value=90.0, allow_nan=False, allow_infinity=False)
TAS = st.floats(min_value=0.0, max_value=400.0, allow_nan=False, allow_infinity=False)
GAMMA = st.floats(min_value=-89.0, max_value=89.0, allow_nan=False, allow_infinity=False)
ID = st.integers(min_value=0, max_value=(1 << 31) - 1)
TICK = st.integers(min_value=0, max_value=(1 << 40))

ENTITY = st.builds(s.EntityState, ID, LAT, LON, BEAR, ALT, PHI, TAS, GAMMA)


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
        assert abs(a.phi_deg - b.phi_deg) <= 1.0 / s.PHI_SCALE
        assert abs(a.tas_mps - b.tas_mps) <= 1.0 / s.SPEED_SCALE
        assert abs(a.gamma_deg - b.gamma_deg) <= 1.0 / s.GAMMA_SCALE


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


@given(LAT, LON, BEAR, ALT, PHI, TAS, GAMMA)
def test_from_to_kernel_roundtrip(lat_deg, lon_deg, bearing_deg, alt, phi_deg, tas, gamma_deg):
    # build kernel-radian inputs, go to wire degrees and back; within one quantum (radians).
    import math
    lat_r, lon_r, psi_r = (math.radians(lat_deg), math.radians(lon_deg), math.radians(bearing_deg))
    phi_r, gamma_r = math.radians(phi_deg), math.radians(gamma_deg)
    e = s.from_kernel(3, lat_r, lon_r, psi_r, alt, phi_r, tas, gamma_r)
    back_lat, back_lon, back_psi, back_alt, back_phi, back_tas, back_gamma = s.to_kernel(e)
    assert abs(back_lat - lat_r) <= 2e-9
    assert abs(back_lon - lon_r) <= 2e-9
    assert abs(back_psi - psi_r) <= 2e-9
    assert back_alt == alt
    assert abs(back_phi - phi_r) <= 2e-9
    assert back_tas == tas
    assert abs(back_gamma - gamma_r) <= 2e-9


def test_protocol_constant_on_wire():
    # the protocol version is the first field; a decoder can gate on it.
    wire = s.encode_snapshot(s.Snapshot(0, []))
    proto, _ = g.decode_i64(wire, 0)
    assert proto == s.SNAPSHOT_PROTOCOL
    assert s.SNAPSHOT_PROTOCOL >= 2  # KIN section carried since seal v1.4r0


@given(st.lists(ENTITY, min_size=1, max_size=4))
def test_protocol1_omits_kin_section(entities):
    # a protocol-1 snapshot carries only the GEO section: it is strictly shorter than the
    # protocol-2 wire, and decode leaves phi/tas at their 0 defaults (back-compat).
    p1 = s.Snapshot(9, entities, protocol=1)
    p2 = s.Snapshot(9, entities, protocol=2)
    w1, w2 = s.encode_snapshot(p1), s.encode_snapshot(p2)
    assert len(w1) < len(w2)
    d1, pos1 = s.decode_snapshot(w1)
    assert pos1 == len(w1) and d1.protocol == 1
    for e in d1.entities:
        assert e.phi_deg == 0.0 and e.tas_mps == 0.0


def test_kin_id_mismatch_rejected():
    # the KIN section repeats each id; a decoder rejects a wire whose KIN id does not match
    # the GEO id at the same index (guards against misaligned sections).
    import pytest
    e = s.EntityState(5, 1.0, 2.0, 3.0, 100.0, 10.0, 50.0)
    out = bytearray()
    out += g.encode_i64(2)          # protocol
    out += g.encode_i64(3)          # server_tick
    out += g.encode_i64(1)          # n
    out += g.encode_i64(e.id)       # GEO id
    out += g.encode_point(e.lat_deg, e.lon_deg, e.bearing_deg, e.alt_m)
    out += g.encode_i64(e.id + 1)   # KIN id — deliberately MISMATCHED
    out += g.encode_i64(g.quantize(e.phi_deg, s.PHI_SCALE))
    out += g.encode_i64(g.quantize(e.tas_mps, s.SPEED_SCALE))
    with pytest.raises(ValueError):
        s.decode_snapshot(bytes(out))
