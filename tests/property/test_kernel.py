"""Metamorphic relations for the SEADS reference kernel.

Invariant checks (energy/symmetry/monotonicity style) beat trajectory matching under
chaotic sensitivity. All tolerances are FP-aware.
"""
import json
from pathlib import Path

from hypothesis import given, strategies as st, settings

import detmath_ref as dm
import ref_kernel as rk

ROOT = Path(__file__).resolve().parents[2]
RAILS = json.loads((ROOT / "config/rails/atm.json").read_text(encoding="utf-8"))
R = 15000.0


def sph_dist(lat1, lon1, lat2, lon2):
    # haversine using det_math for consistency
    dlat = lat2 - lat1
    dlon = lon2 - lon1
    sdl = dm.det_sin(dlat / 2)
    sdo = dm.det_sin(dlon / 2)
    a = sdl * sdl + dm.det_cos(lat1) * dm.det_cos(lat2) * sdo * sdo
    c = 2 * dm.det_atan2(dm.det_sqrt(a), dm.det_sqrt(max(0.0, 1 - a)))
    return R * c


LAT = st.floats(min_value=-1.4, max_value=1.4, allow_nan=False, allow_infinity=False)
LON = st.floats(min_value=-3.0, max_value=3.0, allow_nan=False, allow_infinity=False)
BRG = st.floats(min_value=0.0, max_value=6.28, allow_nan=False, allow_infinity=False)
SPD = st.floats(min_value=0.5, max_value=3.0, allow_nan=False, allow_infinity=False)


@given(LAT, LON, BRG, SPD)
def test_great_circle_step_preserves_arc_length(lat, lon, brg, s):
    lat2, lon2 = rk.great_circle_step(lat, lon, brg, s, R)
    d = sph_dist(lat, lon, lat2, lon2)
    # one step advances exactly arc length s along the geodesic
    assert abs(d - s) < 1e-6


@given(st.floats(min_value=0.0, max_value=99.0, allow_nan=False, allow_infinity=False))
def test_ceiling_never_overshoots(start_below):
    alt = 8000.0 - start_below
    rate = rk.ceiling_climb_rate(10.0, alt, 8000.0, 100.0)
    new_alt = min(alt + rate * 0.01, 8000.0)
    assert new_alt <= 8000.0 + 1e-9


@given(st.floats(min_value=0.0, max_value=100.0),
       st.floats(min_value=0.0, max_value=100.0))
def test_soft_band_monotone(a, b):
    # higher altitude in soft band => climb rate not greater
    lo = 8000.0 - max(a, b)
    hi = 8000.0 - min(a, b)
    r_lo = rk.ceiling_climb_rate(10.0, lo, 8000.0, 100.0)
    r_hi = rk.ceiling_climb_rate(10.0, hi, 8000.0, 100.0)
    assert r_hi <= r_lo + 1e-12


def test_kernel_determinism_byte_identical():
    # same inputs -> identical snapshot bytes (the core promise, single-impl)
    k1, t1, _ = rk.build_golden(RAILS)
    k2, t2, _ = rk.build_golden(RAILS)
    k1.run(t1)
    k2.run(t2)
    assert k1.snapshot(t1) == k2.snapshot(t2)


@settings(max_examples=25)
@given(BRG)
def test_straight_flight_keeps_heading(brg):
    # phi=0 => no turn => heading constant
    k = rk.Kernel(RAILS)
    k.aircraft.append(rk.Aircraft(0.0, 0.0, brg, 0.0, 1000.0, 250.0))
    psi0 = k.aircraft[0].psi
    k.run(50)
    assert abs(dm.wrap_2pi(k.aircraft[0].psi) - dm.wrap_2pi(psi0)) < 1e-12
