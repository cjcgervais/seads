#!/usr/bin/env python3
"""
geo001_ref.py — SEADS canonical GEO-001 wire codec REFERENCE (ATM-Sphere).

GEO-001 is a SEALED RAIL (config/rails/atm.json -> world.wire):
    format        = "GEO-001"
    latlon_scale  = 10000000   (1e7)   lat/lon degrees  -> i64
    bearing_scale = 1000000    (1e6)   bearing degrees  -> i64
    alt_scale     = 1000       (1e3)   altitude metres  -> i64
    encoding      = "ZigZag+LEB128"

This module is the *single source of truth* for the wire bytes. The C++ codec
(`src/net/geo001`) MUST reproduce these bytes BIT-FOR-BIT. The cross-impl gate
generates vectors from this reference (`tools/gen_geo001_vectors.py`) and the C++
test asserts byte-identical encoding + exact round-trip, exactly as det_math is
mirrored against `tools/detmath_ref.py`.

Bit-identity holds because the codec is INTEGER-only (ZigZag + LEB128 over i64/u64),
and the float->fixed quantization uses a single, explicitly-specified rounding rule
(round half AWAY from zero, via floor(x+0.5)/ceil(x-0.5)) evaluated on IEEE-754
doubles — identical in CPython and C++. No libm, no FMA, no platform-dependent round().

NOTE: This is TRANSPORT, outside the kernel/world_hash. The det_math transcendental
ban does not apply here (no sin/cos/etc. are used); the only float op is value*scale,
kept reproducible. Never feed wire-decoded bits back into the kernel without going
through the canonical (un-quantized) sim path — the wire is lossy by quantization.

Usage:  python tools/geo001_ref.py            # runs an internal self-test
"""
import math

# ---- sealed GEO-001 scales (must match config/rails/atm.json world.wire) --------------
LATLON_SCALE  = 10_000_000   # 1e7
BEARING_SCALE = 1_000_000    # 1e6
ALT_SCALE     = 1_000        # 1e3

_MASK64 = (1 << 64) - 1
_I64_MIN = -(1 << 63)
_I64_MAX = (1 << 63) - 1


# ---- fixed-point quantization (round half away from zero) ------------------------------
def quantize(value, scale):
    """degrees/metres (float) -> fixed-point i64. Round half away from zero.

    floor(x+0.5) for x>=0 and ceil(x-0.5) for x<0 are exact on IEEE doubles and
    identical in C++ (std::floor/std::ceil), so the wire is cross-impl reproducible.
    """
    s = value * scale
    q = math.floor(s + 0.5) if s >= 0.0 else math.ceil(s - 0.5)
    if q < _I64_MIN or q > _I64_MAX:
        raise OverflowError(f"quantized value {q} does not fit in i64")
    return int(q)


def dequantize(q, scale):
    """fixed-point i64 -> float. Inverse of quantize up to the 1/scale quantum."""
    return q / scale


# ---- ZigZag (signed <-> unsigned interleave, protobuf sint64 mapping) ------------------
def zigzag_encode(n):
    """i64 -> u64. Small-magnitude signed ints map to small unsigned ints."""
    n &= _MASK64                       # interpret as two's-complement 64-bit
    # (n << 1) ^ (n >> 63), arithmetic on the 64-bit pattern
    sign = _MASK64 if (n >> 63) else 0  # arithmetic-shift replacement of the sign bit
    return ((n << 1) & _MASK64) ^ sign


def zigzag_decode(z):
    """u64 -> i64."""
    z &= _MASK64
    u = (z >> 1) ^ (_MASK64 if (z & 1) else 0)
    return u - (1 << 64) if u >= (1 << 63) else u


# ---- LEB128 (unsigned, little-endian base-128 varint) ----------------------------------
def leb128_encode_u64(u):
    """u64 -> bytes (1..10). High bit of each byte = 'more bytes follow'."""
    u &= _MASK64
    out = bytearray()
    while True:
        b = u & 0x7F
        u >>= 7
        if u:
            out.append(b | 0x80)
        else:
            out.append(b)
            return bytes(out)


def leb128_decode_u64(data, pos=0):
    """bytes -> (u64, next_pos). Reads at most 10 bytes (ceil(64/7))."""
    result = 0
    shift = 0
    for i in range(10):
        if pos >= len(data):
            raise ValueError("LEB128: truncated varint")
        b = data[pos]
        pos += 1
        result |= (b & 0x7F) << shift
        if not (b & 0x80):
            return result & _MASK64, pos
        shift += 7
    raise ValueError("LEB128: varint exceeds 10 bytes (overlong)")


# ---- field codec: i64 fixed-point <-> wire bytes ---------------------------------------
def encode_i64(value):
    """i64 -> GEO-001 wire bytes (ZigZag then LEB128)."""
    return leb128_encode_u64(zigzag_encode(value))


def decode_i64(data, pos=0):
    """GEO-001 wire bytes -> (i64, next_pos)."""
    u, pos = leb128_decode_u64(data, pos)
    return zigzag_decode(u), pos


# ---- GeoPoint record: canonical fixed field order lat, lon, bearing, alt ---------------
# Field order is part of the wire contract: encoders/decoders on both ends MUST agree.
GEO_FIELDS = ("lat", "lon", "bearing", "alt")
_FIELD_SCALE = {"lat": LATLON_SCALE, "lon": LATLON_SCALE,
                "bearing": BEARING_SCALE, "alt": ALT_SCALE}


def encode_point(lat, lon, bearing, alt):
    """(lat,lon,bearing,alt) floats -> concatenated GEO-001 wire bytes."""
    out = bytearray()
    for name, v in zip(GEO_FIELDS, (lat, lon, bearing, alt)):
        out += encode_i64(quantize(v, _FIELD_SCALE[name]))
    return bytes(out)


def decode_point(data, pos=0):
    """wire bytes -> ({lat,lon,bearing,alt} floats, next_pos)."""
    vals = {}
    for name in GEO_FIELDS:
        q, pos = decode_i64(data, pos)
        vals[name] = dequantize(q, _FIELD_SCALE[name])
    return vals, pos


# ---- self-test -------------------------------------------------------------------------
def _selftest():
    fails = 0

    # ZigZag known mapping (protobuf sint64): 0->0, -1->1, 1->2, -2->3, 2->4 ...
    for n, z in [(0, 0), (-1, 1), (1, 2), (-2, 3), (2, 4),
                 (_I64_MAX, _MASK64 - 1), (_I64_MIN, _MASK64)]:
        if zigzag_encode(n) != z or zigzag_decode(z) != n:
            print(f"FAIL zigzag {n} <-> {z}"); fails += 1

    # LEB128 known encodings
    for u, wire in [(0, b"\x00"), (1, b"\x01"), (127, b"\x7f"),
                    (128, b"\x80\x01"), (300, b"\xac\x02"),
                    (16384, b"\x80\x80\x01")]:
        if leb128_encode_u64(u) != wire:
            print(f"FAIL leb128 enc {u}"); fails += 1
        if leb128_decode_u64(wire)[0] != u:
            print(f"FAIL leb128 dec {u}"); fails += 1

    # i64 round-trip across boundaries
    for v in [0, 1, -1, 63, 64, -64, 8191, 8192, _I64_MIN, _I64_MAX,
              900000000, -1800000000, 360000000, 8000000]:
        w = encode_i64(v)
        got, pos = decode_i64(w)
        if got != v or pos != len(w):
            print(f"FAIL i64 round-trip {v}"); fails += 1

    # quantize rounding (round half away from zero)
    for value, scale, exp in [(0.0, ALT_SCALE, 0), (1.5, 1, 2), (-1.5, 1, -2),
                              (2.5, 1, 3), (-2.5, 1, -3), (45.5, LATLON_SCALE, 455000000)]:
        if quantize(value, scale) != exp:
            print(f"FAIL quantize {value}*{scale} -> {quantize(value,scale)} != {exp}"); fails += 1

    # GeoPoint round-trip within one quantum
    pt = (45.1234567, -179.9999999, 359.999999, 7999.999)
    enc = encode_point(*pt)
    dec, pos = decode_point(enc)
    if pos != len(enc):
        print("FAIL point length"); fails += 1
    for name, v in zip(GEO_FIELDS, pt):
        if abs(dec[name] - v) > 1.0 / _FIELD_SCALE[name]:
            print(f"FAIL point {name}: {dec[name]} vs {v}"); fails += 1

    if fails == 0:
        print("RESULT: GEO-001 REFERENCE SELFTEST PASS")
        return 0
    print(f"RESULT: GEO-001 REFERENCE SELFTEST FAIL ({fails})")
    return 1


if __name__ == "__main__":
    import sys
    sys.exit(_selftest())
