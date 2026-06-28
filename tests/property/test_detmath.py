"""Metamorphic relations for SEADS det_math (no exact oracle needed).

These are tolerance-based properties (FP-aware) per the metamorphic-testing discipline:
identities that must hold regardless of the underlying implementation.
"""
import math
from hypothesis import given, strategies as st, settings

import detmath_ref as dm

ANG = st.floats(min_value=-6.283185307179586, max_value=6.283185307179586,
                allow_nan=False, allow_infinity=False)
UNIT = st.floats(min_value=-0.999999, max_value=0.999999,
                 allow_nan=False, allow_infinity=False)


@given(ANG)
def test_pythagorean(x):
    s, c = dm.det_sin(x), dm.det_cos(x)
    assert abs(s * s + c * c - 1.0) < 1e-12


@given(ANG)
def test_sin_odd_cos_even(x):
    assert abs(dm.det_sin(-x) + dm.det_sin(x)) < 1e-12
    assert abs(dm.det_cos(-x) - dm.det_cos(x)) < 1e-12


@given(ANG)
def test_tan_is_sin_over_cos(x):
    c = dm.det_cos(x)
    if abs(c) < 1e-3:
        return
    assert abs(dm.det_tan(x) - dm.det_sin(x) / c) < 1e-12


@given(UNIT)
def test_asin_sin_roundtrip(u):
    # asin(x) in [-pi/2, pi/2]; sin(asin(x)) == x
    a = dm.det_asin(u)
    assert -dm.HALF_PI - 1e-12 <= a <= dm.HALF_PI + 1e-12
    assert abs(dm.det_sin(a) - u) < 1e-10


@given(st.floats(min_value=-1e6, max_value=1e6, allow_nan=False, allow_infinity=False))
def test_atan2_against_libm(v):
    # det_atan2(y,1) == atan(y); compare to libm within a loose tolerance (oracle is separate)
    assert abs(dm.det_atan2(v, 1.0) - math.atan2(v, 1.0)) < 1e-9


@given(st.floats(min_value=1e-9, max_value=1e9, allow_nan=False, allow_infinity=False))
def test_sqrt_square_roundtrip(x):
    r = dm.det_sqrt(x)
    assert abs(r * r - x) <= 1e-9 * x


@given(st.floats(min_value=-100.0, max_value=100.0, allow_nan=False, allow_infinity=False))
def test_wrap_pi_range(a):
    r = dm.wrap_pi(a)
    assert -dm.PI - 1e-9 < r <= dm.PI + 1e-9


@given(st.floats(min_value=-100.0, max_value=100.0, allow_nan=False, allow_infinity=False))
def test_wrap_2pi_range(a):
    r = dm.wrap_2pi(a)
    assert 0.0 - 1e-9 <= r < dm.TWO_PI + 1e-9
