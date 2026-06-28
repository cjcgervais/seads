#!/usr/bin/env python3
"""
detmath_ref.py — SEADS canonical deterministic-math REFERENCE (ATM-Sphere v1.2r0)

This is the *single source of truth* for SEADS transcendental math. The C++ kernel
(`src/det_math`) MUST reproduce these functions bit-for-bit. Bit-identity holds because:

  * Only IEEE-754 basic ops (+ - * /) and correctly-rounded sqrt are used. These are
    correctly rounded and identical on every conforming platform (and in CPython, whose
    floats ARE C doubles with round-to-nearest-even, no FMA, no x87 excess precision).
  * Every constant is given as an exact hex-float literal, so Python and C++ parse the
    identical bit pattern (no decimal-rounding ambiguity).
  * Polynomials are evaluated in a single fixed Horner order with NO fused multiply-add.

Coefficients follow the public-domain fdlibm minimax kernels (Sun Microsystems). The
`det_math_oracle.py` tool verifies every function against an MPFR ground truth.

NOTHING here may call Python's `math` transcendentals — only `math.sqrt` (correctly rounded).
"""
from math import sqrt as _ieee_sqrt   # correctly-rounded IEEE sqrt (== hardware sqrtsd)

# ---- exact constants (hex floats) -----------------------------------------------------
def _h(s): return float.fromhex(s)

PI       = _h('0x1.921fb54442d18p+1')   # 3.141592653589793
HALF_PI  = _h('0x1.921fb54442d18p+0')   # 1.5707963267948966
TWO_PI   = _h('0x1.921fb54442d18p+2')   # 6.283185307179586
TWO_OVER_PI = _h('0x1.45f306dc9c883p-1')  # 0.6366197723675814

# two-part pi/2 for Cody-Waite reduction (fdlibm split)
PIO2_HI  = _h('0x1.921fb54442d18p+0')   # 1.5707963267948966
PIO2_LO  = _h('0x1.1a62633145c07p-54')  # 6.123233995736766e-17

# sin kernel coefficients (fdlibm S1..S6)
_S1 = _h('-0x1.5555555555549p-3')
_S2 = _h('0x1.111111110f8a6p-7')
_S3 = _h('-0x1.a01a019c161d5p-13')
_S4 = _h('0x1.71de357b1fe7dp-19')
_S5 = _h('-0x1.ae5e68a2b9cebp-26')
_S6 = _h('0x1.5d93a5acfd57cp-33')

# cos kernel coefficients (fdlibm C1..C6)
_C1 = _h('0x1.555555555554cp-5')
_C2 = _h('-0x1.6c16c16c15177p-10')
_C3 = _h('0x1.a01a019cb159p-16')
_C4 = _h('-0x1.27e4f809c52adp-22')
_C5 = _h('0x1.1ee9ebdb4b1c4p-29')
_C6 = _h('-0x1.8fae9be8838d4p-37')

# atan kernel coefficients (exact fdlibm aT0..aT10 hex)
_aT = (
    _h('0x1.555555555550dp-2'),
    _h('-0x1.999999998ebc4p-3'),
    _h('0x1.24924920083ffp-3'),
    _h('-0x1.c71c6fe231671p-4'),
    _h('0x1.745cdc54c206ep-4'),
    _h('-0x1.3b0f2af749a6dp-4'),
    _h('0x1.10d66a0d03d51p-4'),
    _h('-0x1.dde2d52defd9ap-5'),
    _h('0x1.97b4b24760debp-5'),
    _h('-0x1.2b4442c6a6c2fp-5'),
    _h('0x1.0ad3ae322da11p-6'),
)
# atan reference values at hi/lo split points (fdlibm atanhi/atanlo)
_atanhi = (_h('0x1.dac670561bb4fp-2'),  # atan(0.5)
           _h('0x1.921fb54442d18p-1'),  # atan(1.0) = pi/4
           _h('0x1.f730bd281f69bp-1'),  # atan(1.5)
           _h('0x1.921fb54442d18p+0'))  # atan(inf) = pi/2
_atanlo = (_h('0x1.a2b7f222f65e2p-56'),
           _h('0x1.1a62633145c07p-55'),
           _h('0x1.007887af0cbbdp-56'),
           _h('0x1.1a62633145c07p-54'))


def det_sqrt(x):
    """Correctly-rounded IEEE square root (det_math primitive)."""
    return _ieee_sqrt(x)


def _kernel_sin(x):
    # x in ~[-pi/4, pi/4]; fixed Horner, no FMA
    z = x * x
    r = _S2 + z * (_S3 + z * (_S4 + z * (_S5 + z * _S6)))
    return x + x * (z * (_S1 + z * r))


def _kernel_cos(x):
    # x in ~[-pi/4, pi/4]; fixed Horner, no FMA
    z = x * x
    r = z * (_C1 + z * (_C2 + z * (_C3 + z * (_C4 + z * (_C5 + z * _C6)))))
    return 1.0 + (-0.5 * z + z * r)


def _reduce_half_pi(x):
    """Cody-Waite reduction by pi/2. Returns (n_mod_4, r). Assumes |x| modest (SEADS
    angles are pre-wrapped to [-2pi, 2pi]); no Payne-Hanek path is needed."""
    fn = x * TWO_OVER_PI
    if fn >= 0.0:
        n = float(int(fn + 0.5))
    else:
        n = float(int(fn - 0.5))
    r = (x - n * PIO2_HI) - n * PIO2_LO
    return (int(n) & 3), r


def det_sin(x):
    q, r = _reduce_half_pi(x)
    if q == 0:
        return _kernel_sin(r)
    elif q == 1:
        return _kernel_cos(r)
    elif q == 2:
        return -_kernel_sin(r)
    else:
        return -_kernel_cos(r)


def det_cos(x):
    q, r = _reduce_half_pi(x)
    if q == 0:
        return _kernel_cos(r)
    elif q == 1:
        return -_kernel_sin(r)
    elif q == 2:
        return -_kernel_cos(r)
    else:
        return _kernel_sin(r)


def det_tan(x):
    return det_sin(x) / det_cos(x)


def _kernel_atan(x):
    # |x| <= 1 ; fixed Horner over even/odd split (fdlibm form)
    z = x * x
    w = z * z
    s1 = z * (_aT[0] + w * (_aT[2] + w * (_aT[4] + w * (_aT[6] + w * (_aT[8] + w * _aT[10])))))
    s2 = w * (_aT[1] + w * (_aT[3] + w * (_aT[5] + w * (_aT[7] + w * _aT[9]))))
    return x - x * (s1 + s2)


def det_atan(x):
    # Faithful port of fdlibm s_atan.c reduction regions (7/16, 11/16, 19/16, 39/16).
    sign = -1.0 if x < 0.0 else 1.0
    ax = -x if x < 0.0 else x
    if ax >= _h('0x1.0p+66'):                 # |x| >= 2^66: atan -> +/- pi/2
        return sign * (_atanhi[3] + _atanlo[3])
    if ax < _h('0x1.0p-27'):                  # |x| < 2^-27: atan(x) ~= x
        return x
    if ax < 0.4375:                           # 7/16
        idx = -1
        xr = ax
    elif ax < 0.6875:                         # 11/16  id 0
        idx = 0
        xr = (2.0 * ax - 1.0) / (2.0 + ax)
    elif ax < 1.1875:                         # 19/16  id 1
        idx = 1
        xr = (ax - 1.0) / (ax + 1.0)
    elif ax < 2.4375:                         # 39/16  id 2
        idx = 2
        xr = (ax - 1.5) / (1.0 + 1.5 * ax)
    else:                                     # id 3
        idx = 3
        xr = -1.0 / ax
    z = xr * xr
    w = z * z
    s1 = z * (_aT[0] + w * (_aT[2] + w * (_aT[4] + w * (_aT[6] + w * (_aT[8] + w * _aT[10])))))
    s2 = w * (_aT[1] + w * (_aT[3] + w * (_aT[5] + w * (_aT[7] + w * _aT[9]))))
    if idx < 0:
        return sign * (xr - xr * (s1 + s2))
    zz = _atanhi[idx] - ((xr * (s1 + s2) - _atanlo[idx]) - xr)
    return sign * zz


def det_atan2(y, x):
    if x == 0.0 and y == 0.0:
        return 0.0
    if x == 0.0:
        return HALF_PI if y > 0.0 else -HALF_PI
    z = det_atan(y / x)
    if x > 0.0:
        return z
    # x < 0
    if y >= 0.0:
        return z + PI
    return z - PI


def det_asin(x):
    # asin(x) = atan2(x, sqrt(1 - x*x)) ; exact-leaning for the SEADS domain
    if x >= 1.0:
        return HALF_PI
    if x <= -1.0:
        return -HALF_PI
    return det_atan2(x, det_sqrt((1.0 - x) * (1.0 + x)))


# ---- canonical wraps ------------------------------------------------------------------
def wrap_pi(a):
    """Normalize to (-pi, pi] using only +,-,*,/ and a deterministic floor."""
    # k = round(a / (2pi)); a - k*2pi
    q = a / TWO_PI
    if q >= 0.0:
        k = float(int(q + 0.5))
    else:
        k = float(int(q - 0.5))
    r = a - k * TWO_PI
    # nudge into (-pi, pi]
    if r <= -PI:
        r += TWO_PI
    elif r > PI:
        r -= TWO_PI
    return r


def wrap_2pi(a):
    """Normalize to [0, 2pi)."""
    q = a / TWO_PI
    k = float(int(q)) if q >= 0.0 else float(int(q) - 1)
    r = a - k * TWO_PI
    if r < 0.0:
        r += TWO_PI
    elif r >= TWO_PI:
        r -= TWO_PI
    return r


if __name__ == "__main__":
    import math
    # quick smoke: compare against libm at a few points (NOT the formal oracle)
    pts = [0.0, 0.1, 0.7853981633974483, 1.0, 1.5, -0.3, 2.0, 3.0]
    worst = 0.0
    for p in pts:
        worst = max(worst, abs(det_sin(p) - math.sin(p)), abs(det_cos(p) - math.cos(p)))
    for p in [0.0, 0.25, 0.6, -0.9, 0.999]:
        worst = max(worst, abs(det_asin(p) - math.asin(p)))
    print(f"smoke OK; worst |det - libm| = {worst:.3e}")
