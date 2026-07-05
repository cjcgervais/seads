#!/usr/bin/env python3
"""
ed25519_ref.py — SEADS Ed25519 signature REFERENCE (ATM-Sphere netcode layer 27).

Ed25519 (Bernstein et al.; RFC 8032) is the modern standard PUBLIC-KEY signature. Netcode
layer 27 uses it to turn layer 21's abstracted i64 "token" into an ASYMMETRIC identity: each
client holds a 32-byte PRIVATE seed; the server holds ONLY the derived 32-byte PUBLIC key. A
client proves possession by SIGNING the server's fresh challenge; the server VERIFIES with the
public key alone. The headline over layer 26's symmetric SipHash MAC: the server never holds a
secret that could sign, so a full ROSTER LEAK (every enrolled public key) still cannot
impersonate any client — only the private-key holder can produce a valid signature.

This is the canonical RFC 8032 reference algorithm (Bernstein's slow, obviously-correct
reference), with the hash H wired to our own sha512_ref (no hashlib) so the whole primitive is
self-contained and pinnable. It is reproducible cross-impl (the whole SEADS promise) because it
is 100% big-INTEGER arithmetic — no floating point, no libm. Ed25519 signing is DETERMINISTIC
(the per-message nonce r is a hash of the key + message), so a given (seed, message) yields ONE
canonical 64-byte signature; any correct Ed25519 in any language produces the same bytes. The
C++ mirror (src/net/ed25519, a transcription of TweetNaCl's known-correct fixed-limb code)
produces byte-identical public keys and signatures — proven by the shared fixed-seed pin below
and the bridge's cross-impl agreement. TRANSPORT, outside the kernel/world_hash — no det_math,
no seal (rides v1.26r0).

Usage:  python tools/ed25519_ref.py        # internal self-test (RFC-8032 algorithm + fixed pin)
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import sha512_ref as _sha

_b = 256
_q = 2 ** 255 - 19
# group order L = 2^252 + 27742317777372353535851937790883648493
_L = 2 ** 252 + 27742317777372353535851937790883648493


def _H(m):
    return _sha.sha512(m)


def _inv(x):
    return pow(x, _q - 2, _q)


_d = (-121665 * _inv(121666)) % _q
_I = pow(2, (_q - 1) // 4, _q)


def _xrecover(y):
    xx = (y * y - 1) * _inv(_d * y * y + 1)
    x = pow(xx, (_q + 3) // 8, _q)
    if (x * x - xx) % _q != 0:
        x = (x * _I) % _q
    if x % 2 != 0:
        x = _q - x
    return x


_By = (4 * _inv(5)) % _q
_Bx = _xrecover(_By)
_B = [_Bx % _q, _By % _q]


def _edwards(P, Q):
    x1, y1 = P
    x2, y2 = Q
    x3 = (x1 * y2 + x2 * y1) * _inv(1 + _d * x1 * x2 * y1 * y2)
    y3 = (y1 * y2 + x1 * x2) * _inv(1 - _d * x1 * x2 * y1 * y2)
    return [x3 % _q, y3 % _q]


def _scalarmult(P, e):
    if e == 0:
        return [0, 1]
    Q = _scalarmult(P, e // 2)
    Q = _edwards(Q, Q)
    if e & 1:
        Q = _edwards(Q, P)
    return Q


def _bit(h, i):
    return (h[i // 8] >> (i % 8)) & 1


def _encodeint(y):
    bits = [(y >> i) & 1 for i in range(_b)]
    return bytes(sum(bits[i * 8 + j] << j for j in range(8)) for i in range(_b // 8))


def _encodepoint(P):
    x, y = P
    bits = [(y >> i) & 1 for i in range(_b - 1)] + [x & 1]
    return bytes(sum(bits[i * 8 + j] << j for j in range(8)) for i in range(_b // 8))


def _decodeint(s):
    return sum(2 ** i * _bit(s, i) for i in range(_b))


def _isoncurve(P):
    x, y = P
    return (-x * x + y * y - 1 - _d * x * x * y * y) % _q == 0


def _decodepoint(s):
    y = sum(2 ** i * _bit(s, i) for i in range(_b - 1))
    x = _xrecover(y)
    if x & 1 != _bit(s, _b - 1):
        x = _q - x
    P = [x, y]
    if not _isoncurve(P):
        raise ValueError("decoding point that is not on curve")
    return P


def _secret_scalar(h):
    return 2 ** (_b - 2) + sum(2 ** i * _bit(h, i) for i in range(3, _b - 2))


def _Hint(m):
    h = _H(m)
    return sum(2 ** i * _bit(h, i) for i in range(2 * _b))


def public_key(seed):
    """32-byte private seed -> 32-byte Ed25519 public key."""
    seed = bytes(seed)
    if len(seed) != 32:
        raise ValueError("seed must be 32 bytes")
    h = _H(seed)
    a = _secret_scalar(h)
    A = _scalarmult(_B, a)
    return _encodepoint(A)


def sign(seed, msg):
    """Deterministic Ed25519 signature: 32-byte seed + message bytes -> 64-byte signature."""
    seed = bytes(seed)
    if len(seed) != 32:
        raise ValueError("seed must be 32 bytes")
    msg = bytes(msg)
    h = _H(seed)
    a = _secret_scalar(h)
    pk = _encodepoint(_scalarmult(_B, a))
    r = _Hint(h[_b // 8:_b // 4] + msg)
    R = _scalarmult(_B, r)
    S = (r + _Hint(_encodepoint(R) + pk + msg) * a) % _L
    return _encodepoint(R) + _encodeint(S)


def verify(pk, msg, sig):
    """True iff `sig` (64 bytes) is a valid Ed25519 signature on `msg` under public key `pk`."""
    pk = bytes(pk)
    sig = bytes(sig)
    msg = bytes(msg)
    if len(sig) != 64 or len(pk) != 32:
        return False
    try:
        R = _decodepoint(sig[:32])
        A = _decodepoint(pk)
    except ValueError:
        return False
    S = _decodeint(sig[32:])
    h = _Hint(sig[:32] + pk + msg)
    return _scalarmult(_B, S) == _edwards(R, _scalarmult(A, h))


# ---- self-test -------------------------------------------------------------------------
# A FIXED-SEED regression pin, shared BIT-FOR-BIT with the C++ mirror (src/net/ed25519) and the
# authsig bridge. Because Ed25519 sign/verify are deterministic and produce canonical bytes, a
# correct C++ transcription reproduces exactly these values — that cross-impl agreement, plus the
# RFC-8032 algorithm fidelity and the SHA-512 official-vector anchor beneath it, is the anchor.
_PIN_SEED = bytes(range(32))  # 00 01 02 ... 1f
_PIN_MSG = b"SEADS-ed25519-pin"


def _selftest():
    fails = 0

    # round-trip: a genuine signature verifies; tampering (sig / message / key) is rejected.
    for si in range(4):
        seed = bytes([(si * 37 + i) & 0xFF for i in range(32)])
        pk = public_key(seed)
        for mi in range(3):
            msg = bytes([(mi * 91 + j) & 0xFF for j in range(mi * 5)])  # 0,5,10 bytes
            sg = sign(seed, msg)
            if len(sg) != 64 or len(pk) != 32:
                print(f"FAIL sizes seed{si} msg{mi}"); fails += 1
            if not verify(pk, msg, sg):
                print(f"FAIL genuine sig rejected seed{si} msg{mi}"); fails += 1
            # flip one signature bit -> reject
            bad = bytearray(sg); bad[0] ^= 1
            if verify(pk, msg, bytes(bad)):
                print(f"FAIL tampered sig accepted seed{si} msg{mi}"); fails += 1
            # a different message under the same sig -> reject
            if verify(pk, msg + b"x", sg):
                print(f"FAIL sig valid under wrong message seed{si} msg{mi}"); fails += 1
            # a different public key -> reject (the asymmetric headline)
            other_pk = public_key(bytes([(si * 37 + i + 1) & 0xFF for i in range(32)]))
            if verify(other_pk, msg, sg):
                print(f"FAIL sig valid under wrong key seed{si} msg{mi}"); fails += 1

    # determinism: signing is a pure function of (seed, message).
    if sign(_PIN_SEED, _PIN_MSG) != sign(_PIN_SEED, _PIN_MSG):
        print("FAIL signing not deterministic"); fails += 1

    # FIXED PIN (the cross-impl anchor). Recorded here after the first green run; the C++ mirror
    # asserts the SAME bytes.
    pk = public_key(_PIN_SEED)
    sg = sign(_PIN_SEED, _PIN_MSG)
    if _PIN_PK is not None and pk.hex() != _PIN_PK:
        print(f"FAIL pin pubkey: {pk.hex()} != {_PIN_PK}"); fails += 1
    if _PIN_SIG is not None and sg.hex() != _PIN_SIG:
        print(f"FAIL pin sig: {sg.hex()} != {_PIN_SIG}"); fails += 1

    if fails == 0:
        print("RESULT: Ed25519 REFERENCE SELFTEST PASS")
        print(f"  PIN pubkey = {pk.hex()}")
        print(f"  PIN sig    = {sg.hex()}")
        return 0
    print(f"RESULT: Ed25519 REFERENCE SELFTEST FAIL ({fails})")
    return 1


# The fixed-seed pin, independently confirmed byte-for-byte against the `cryptography` library's
# Ed25519 (a widely-audited external implementation) for seed = 00..1f, msg = "SEADS-ed25519-pin".
_PIN_PK = "03a107bff3ce10be1d70dd18e74bc09967e4d6309ba50d5f1ddc8664125531b8"
_PIN_SIG = ("7cba67f3c33e2fc333bb5568d2c7358d54d732903df5c022dc46838eb0317599"
            "aefc4f06518b1a5d33a539592d53fa7eb08af4acdc95719491adfff39365390f")

if __name__ == "__main__":
    sys.exit(_selftest())
