#!/usr/bin/env python3
"""
sha512_ref.py — SEADS SHA-512 REFERENCE (ATM-Sphere netcode layer 27).

SHA-512 (FIPS 180-4) is the collision-resistant hash Ed25519 (RFC 8032) is built on: the
private scalar, the deterministic per-message nonce r, and the challenge scalar h are all
SHA-512 outputs. Netcode layer 27 turns layer 21's abstracted i64 "token" into an ASYMMETRIC
public-key identity (Ed25519), and Ed25519 needs exactly this hash — so it lives here as a
standalone, separately-pinnable primitive (like siphash_ref.py was for the layer-26 MAC).

Why it is reproducible cross-impl (the whole SEADS promise), even though it is "crypto":
SHA-512 is 100% 64-bit UNSIGNED integer arithmetic — add (mod 2^64), xor, and, not, and
rotate/shift. There is NO floating point, NO libm, NO platform-dependent behaviour. C++
`uint64_t` wraps by definition and Python masks to 64 bits here, so the two produce
byte-identical output on any toolchain / architecture. This is TRANSPORT, OUTSIDE the kernel
and the world_hash — it feeds no bits back into the sim and touches no det_math (the
transcendental ban does not apply; there are no transcendentals here). The C++ mirror is
src/net/sha512.

Pinned to the OFFICIAL NIST vector SHA-512("abc") =
  ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a
  2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f
A real cross-project anchor: any correct SHA-512 in any language reproduces it.

Usage:  python tools/sha512_ref.py        # internal self-test (incl. the official vector)
"""
import sys

_MASK64 = (1 << 64) - 1

# Round constants: first 64 bits of the fractional parts of the cube roots of the first 80 primes.
_K = [
    0x428A2F98D728AE22, 0x7137449123EF65CD, 0xB5C0FBCFEC4D3B2F, 0xE9B5DBA58189DBBC,
    0x3956C25BF348B538, 0x59F111F1B605D019, 0x923F82A4AF194F9B, 0xAB1C5ED5DA6D8118,
    0xD807AA98A3030242, 0x12835B0145706FBE, 0x243185BE4EE4B28C, 0x550C7DC3D5FFB4E2,
    0x72BE5D74F27B896F, 0x80DEB1FE3B1696B1, 0x9BDC06A725C71235, 0xC19BF174CF692694,
    0xE49B69C19EF14AD2, 0xEFBE4786384F25E3, 0x0FC19DC68B8CD5B5, 0x240CA1CC77AC9C65,
    0x2DE92C6F592B0275, 0x4A7484AA6EA6E483, 0x5CB0A9DCBD41FBD4, 0x76F988DA831153B5,
    0x983E5152EE66DFAB, 0xA831C66D2DB43210, 0xB00327C898FB213F, 0xBF597FC7BEEF0EE4,
    0xC6E00BF33DA88FC2, 0xD5A79147930AA725, 0x06CA6351E003826F, 0x142929670A0E6E70,
    0x27B70A8546D22FFC, 0x2E1B21385C26C926, 0x4D2C6DFC5AC42AED, 0x53380D139D95B3DF,
    0x650A73548BAF63DE, 0x766A0ABB3C77B2A8, 0x81C2C92E47EDAEE6, 0x92722C851482353B,
    0xA2BFE8A14CF10364, 0xA81A664BBC423001, 0xC24B8B70D0F89791, 0xC76C51A30654BE30,
    0xD192E819D6EF5218, 0xD69906245565A910, 0xF40E35855771202A, 0x106AA07032BBD1B8,
    0x19A4C116B8D2D0C8, 0x1E376C085141AB53, 0x2748774CDF8EEB99, 0x34B0BCB5E19B48A8,
    0x391C0CB3C5C95A63, 0x4ED8AA4AE3418ACB, 0x5B9CCA4F7763E373, 0x682E6FF3D6B2B8A3,
    0x748F82EE5DEFB2FC, 0x78A5636F43172F60, 0x84C87814A1F0AB72, 0x8CC702081A6439EC,
    0x90BEFFFA23631E28, 0xA4506CEBDE82BDE9, 0xBEF9A3F7B2C67915, 0xC67178F2E372532B,
    0xCA273ECEEA26619C, 0xD186B8C721C0C207, 0xEADA7DD6CDE0EB1E, 0xF57D4F7FEE6ED178,
    0x06F067AA72176FBA, 0x0A637DC5A2C898A6, 0x113F9804BEF90DAE, 0x1B710B35131C471B,
    0x28DB77F523047D84, 0x32CAAB7B40C72493, 0x3C9EBE0A15C9BEBC, 0x431D67C49C100D4C,
    0x4CC5D4BECB3E42B6, 0x597F299CFC657E2A, 0x5FCB6FAB3AD6FAEC, 0x6C44198C4A475817,
]

# Initial hash: first 64 bits of the fractional parts of the square roots of the first 8 primes.
_H0 = [
    0x6A09E667F3BCC908, 0xBB67AE8584CAA73B, 0x3C6EF372FE94F82B, 0xA54FF53A5F1D36F1,
    0x510E527FADE682D1, 0x9B05688C2B3E6C1F, 0x1F83D9ABFB41BD6B, 0x5BE0CD19137E2179,
]


def _rotr(x, n):
    return ((x >> n) | (x << (64 - n))) & _MASK64


def sha512(data):
    """SHA-512 of `data` (bytes) -> 64 bytes. FIPS 180-4."""
    data = bytearray(data)
    n = len(data)
    # padding: 0x80, then zeros, then the 128-bit big-endian bit length.
    data.append(0x80)
    while len(data) % 128 != 112:
        data.append(0x00)
    data += (n * 8).to_bytes(16, "big")

    h = list(_H0)
    for base in range(0, len(data), 128):
        blk = data[base:base + 128]
        w = [int.from_bytes(blk[i * 8:i * 8 + 8], "big") for i in range(16)]
        for i in range(16, 80):
            s0 = _rotr(w[i - 15], 1) ^ _rotr(w[i - 15], 8) ^ (w[i - 15] >> 7)
            s1 = _rotr(w[i - 2], 19) ^ _rotr(w[i - 2], 61) ^ (w[i - 2] >> 6)
            w.append((w[i - 16] + s0 + w[i - 7] + s1) & _MASK64)
        a, b, c, d, e, f, g, hh = h
        for i in range(80):
            S1 = _rotr(e, 14) ^ _rotr(e, 18) ^ _rotr(e, 41)
            ch = (e & f) ^ (~e & g)
            t1 = (hh + S1 + ch + _K[i] + w[i]) & _MASK64
            S0 = _rotr(a, 28) ^ _rotr(a, 34) ^ _rotr(a, 39)
            maj = (a & b) ^ (a & c) ^ (b & c)
            t2 = (S0 + maj) & _MASK64
            hh = g
            g = f
            f = e
            e = (d + t1) & _MASK64
            d = c
            c = b
            b = a
            a = (t1 + t2) & _MASK64
        h = [(x + y) & _MASK64 for x, y in zip(h, [a, b, c, d, e, f, g, hh])]
    return b"".join(x.to_bytes(8, "big") for x in h)


# ---- self-test -------------------------------------------------------------------------
def _selftest():
    fails = 0
    vectors = {
        b"": ("cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce"
              "47d0d13c5d85f2b0ff8318d2877eec2f63b931bd47417a81a538327af927da3e"),
        b"abc": ("ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a"
                 "2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f"),
        (b"abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmn"
         b"hijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu"):
            ("8e959b75dae313da8cf4f72814fc143f8f7779c6eb9f7fa17299aeadb6889018"
             "501d289e4900f7e4331b99dec4b5433ac7d329eeb6dd26545e96e55b874be909"),
    }
    for msg, hexd in vectors.items():
        got = sha512(msg).hex()
        if got != hexd:
            print(f"FAIL sha512(len {len(msg)}): {got} != {hexd}")
            fails += 1

    # determinism + sensitivity
    if sha512(b"seads") != sha512(b"seads"):
        print("FAIL not deterministic"); fails += 1
    if sha512(b"seads") == sha512(b"seadt"):
        print("FAIL message-insensitive"); fails += 1
    # a multi-block message (>112 bytes) exercises the padding boundary
    long_msg = bytes(range(200))
    if len(sha512(long_msg)) != 64:
        print("FAIL long message"); fails += 1

    if fails == 0:
        print("RESULT: SHA-512 REFERENCE SELFTEST PASS (NIST vectors matched)")
        return 0
    print(f"RESULT: SHA-512 REFERENCE SELFTEST FAIL ({fails})")
    return 1


if __name__ == "__main__":
    sys.exit(_selftest())
