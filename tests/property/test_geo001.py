"""Metamorphic / round-trip properties for the GEO-001 wire codec.

The byte-exact C++<->reference parity is proven by the generated-vector gate
(src/net/geo001_test_main.cpp). Here we prove the *reference itself* is self-consistent
over the full quantized domain: every encode->decode is lossless on i64, ZigZag and LEB128
are exact inverses, and GeoPoint quantization round-trips within one quantum. The decode
side is what guarantees "C++ encodes, Python decodes" holds (C++ encode == ref bytes, and
ref decode(bytes) == value)."""
import sys
from pathlib import Path

from hypothesis import given, strategies as st

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import geo001_ref as g

I64 = st.integers(min_value=-(1 << 63), max_value=(1 << 63) - 1)
U64 = st.integers(min_value=0, max_value=(1 << 64) - 1)

# SEADS field domains (degrees / metres), kept inside the i64 quantized range.
LAT = st.floats(min_value=-90.0, max_value=90.0, allow_nan=False, allow_infinity=False)
LON = st.floats(min_value=-180.0, max_value=180.0, allow_nan=False, allow_infinity=False)
BEAR = st.floats(min_value=0.0, max_value=360.0, allow_nan=False, allow_infinity=False)
ALT = st.floats(min_value=-100.0, max_value=8000.0, allow_nan=False, allow_infinity=False)


@given(I64)
def test_zigzag_roundtrip(n):
    assert g.zigzag_decode(g.zigzag_encode(n)) == n


@given(I64)
def test_zigzag_magnitude(n):
    # ZigZag keeps small magnitudes small: |n| roughly halves the unsigned value.
    z = g.zigzag_encode(n)
    assert z == (2 * n if n >= 0 else -2 * n - 1)


@given(U64)
def test_leb128_roundtrip(u):
    wire = g.leb128_encode_u64(u)
    got, pos = g.leb128_decode_u64(wire)
    assert got == u and pos == len(wire)
    assert len(wire) <= 10


@given(I64)
def test_i64_field_roundtrip(v):
    wire = g.encode_i64(v)
    got, pos = g.decode_i64(wire)
    assert got == v and pos == len(wire)


@given(LAT, LON, BEAR, ALT)
def test_point_roundtrip_within_quantum(lat, lon, bearing, alt):
    wire = g.encode_point(lat, lon, bearing, alt)
    dec, pos = g.decode_point(wire)
    assert pos == len(wire)
    assert abs(dec["lat"] - lat) <= 1.0 / g.LATLON_SCALE
    assert abs(dec["lon"] - lon) <= 1.0 / g.LATLON_SCALE
    assert abs(dec["bearing"] - bearing) <= 1.0 / g.BEARING_SCALE
    assert abs(dec["alt"] - alt) <= 1.0 / g.ALT_SCALE


@given(st.integers(min_value=-(1 << 40), max_value=(1 << 40)))
def test_quantize_dequantize_consistency(q):
    # dequantize then quantize must recover the original fixed-point integer exactly.
    assert g.quantize(g.dequantize(q, g.LATLON_SCALE), g.LATLON_SCALE) == q


def test_scales_match_rails():
    import json
    root = Path(__file__).resolve().parents[2]
    wire = json.loads((root / "config/rails/atm.json").read_text(encoding="utf-8"))["rails"]["wire"]
    assert wire["format"] == "GEO-001"
    assert wire["latlon_scale"] == g.LATLON_SCALE
    assert wire["bearing_scale"] == g.BEARING_SCALE
    assert wire["alt_scale"] == g.ALT_SCALE
    assert wire["encoding"] == "ZigZag+LEB128"
