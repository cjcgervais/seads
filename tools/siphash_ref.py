#!/usr/bin/env python3
"""
siphash_ref.py — SEADS SipHash-2-4 keyed-MAC REFERENCE (ATM-Sphere netcode layer 26).

SipHash-2-4 (Aumasson & Bernstein, 2012) is a real, standard keyed pseudo-random function
used everywhere as a short-input MAC (hash-table DoS defence, syncookies, message auth). It
is EXACTLY the primitive netcode layer 26 needs to turn the abstracted i64 "token" of layer
21 (HELLO-001) into a credential the server can VERIFY: the client proves it possesses a
pre-shared 128-bit secret by MAC'ing a server-issued challenge nonce, and the server
recomputes the same MAC and compares. A forger who knows only the public token (a username)
cannot produce the MAC.

Why this is reproducible cross-impl (the whole SEADS promise), even though it is "crypto":
SipHash is 100% 64-bit UNSIGNED integer arithmetic — add (mod 2^64), xor, and rotate. There
is NO floating point, NO libm, NO platform-dependent behaviour. C++ `uint64_t` wraps by
definition and Python masks to 64 bits here, so the two produce byte-identical output on any
toolchain / architecture. This is TRANSPORT, OUTSIDE the kernel and the world_hash — it feeds
no bits back into the sim and touches no det_math (the transcendental ban does not apply;
there are no transcendentals here anyway). The C++ mirror is src/net/siphash.

The implementation is pinned to the OFFICIAL SipHash-2-4 test vector (the reference
`vectors_sip64` from the paper's C reference): key = 000102...0f (little-endian split into
two u64), message = the 15 bytes 00 01 ... 0e, output = 0xa129ca6149be45e5. That is a real
cross-project anchor — any correct SipHash-2-4 in any language reproduces it.

Usage:  python tools/siphash_ref.py        # internal self-test (incl. the official vector)
"""
import sys

_MASK64 = (1 << 64) - 1


def _rotl(x, b):
    """64-bit left rotate."""
    return ((x << b) | (x >> (64 - b))) & _MASK64


def _sipround(v0, v1, v2, v3):
    v0 = (v0 + v1) & _MASK64
    v1 = _rotl(v1, 13)
    v1 ^= v0
    v0 = _rotl(v0, 32)
    v2 = (v2 + v3) & _MASK64
    v3 = _rotl(v3, 16)
    v3 ^= v2
    v0 = (v0 + v3) & _MASK64
    v3 = _rotl(v3, 21)
    v3 ^= v0
    v2 = (v2 + v1) & _MASK64
    v1 = _rotl(v1, 17)
    v1 ^= v2
    v2 = _rotl(v2, 32)
    return v0, v1, v2, v3


def siphash24(k0, k1, data):
    """SipHash-2-4 keyed MAC over `data` (bytes) with the 128-bit key (k0, k1) — each a u64
    (the low/high halves). Returns a u64. c=2 compression rounds, d=4 finalization rounds."""
    k0 &= _MASK64
    k1 &= _MASK64
    data = bytes(data)
    n = len(data)

    v0 = 0x736F6D6570736575 ^ k0
    v1 = 0x646F72616E646F6D ^ k1
    v2 = 0x6C7967656E657261 ^ k0
    v3 = 0x7465646279746573 ^ k1

    # full 8-byte little-endian blocks
    end = n - (n % 8)
    for off in range(0, end, 8):
        m = int.from_bytes(data[off:off + 8], "little")
        v3 ^= m
        v0, v1, v2, v3 = _sipround(v0, v1, v2, v3)
        v0, v1, v2, v3 = _sipround(v0, v1, v2, v3)
        v0 ^= m

    # last block: the remaining <8 bytes in the low bytes, with len&0xff in the top byte
    b = (n & 0xFF) << 56
    tail = data[end:]
    for i, byte in enumerate(tail):
        b |= byte << (8 * i)
    v3 ^= b
    v0, v1, v2, v3 = _sipround(v0, v1, v2, v3)
    v0, v1, v2, v3 = _sipround(v0, v1, v2, v3)
    v0 ^= b

    v2 ^= 0xFF
    for _ in range(4):
        v0, v1, v2, v3 = _sipround(v0, v1, v2, v3)
    return (v0 ^ v1 ^ v2 ^ v3) & _MASK64


def u64_le(x):
    """A u64 (or i64 bit pattern) as 8 little-endian bytes."""
    return (x & _MASK64).to_bytes(8, "little")


# ---- self-test -------------------------------------------------------------------------
def _selftest():
    fails = 0

    # OFFICIAL SipHash-2-4 reference vector: key = 00 01 .. 0f, msg = 00 01 .. 0e (15 bytes).
    k0 = int.from_bytes(bytes(range(8)), "little")        # 0x0706050403020100
    k1 = int.from_bytes(bytes(range(8, 16)), "little")    # 0x0f0e0d0c0b0a0908
    msg = bytes(range(15))
    out = siphash24(k0, k1, msg)
    if out != 0xA129CA6149BE45E5:
        print(f"FAIL official vector: {out:#018x} != 0xa129ca6149be45e5"); fails += 1

    # Determinism: same input -> same output; a 1-bit key change -> different MAC.
    if siphash24(1, 2, b"seads") != siphash24(1, 2, b"seads"):
        print("FAIL not deterministic"); fails += 1
    if siphash24(1, 2, b"seads") == siphash24(1, 3, b"seads"):
        print("FAIL key-insensitive"); fails += 1
    # A 1-bit message change -> different MAC.
    if siphash24(1, 2, b"seads") == siphash24(1, 2, b"teads"):
        print("FAIL message-insensitive"); fails += 1

    # Empty and multi-block messages hash without error and stay stable.
    for m in (b"", b"\x00", bytes(8), bytes(16), bytes(31)):
        a = siphash24(0xDEADBEEF, 0xFEEDFACE, m)
        b = siphash24(0xDEADBEEF, 0xFEEDFACE, m)
        if a != b or not (0 <= a <= _MASK64):
            print(f"FAIL block len {len(m)}"); fails += 1

    if fails == 0:
        print("RESULT: SipHash-2-4 REFERENCE SELFTEST PASS (official vector matched)")
        return 0
    print(f"RESULT: SipHash-2-4 REFERENCE SELFTEST FAIL ({fails})")
    return 1


if __name__ == "__main__":
    sys.exit(_selftest())
