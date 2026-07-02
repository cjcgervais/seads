"""Metamorphic properties for the B5 ISA atmosphere (ATM-Sphere v1.21r0).

Bit-exact C++ <-> Python parity for the sigma(alt) LUT is proven by the goldens (in particular
GOLDEN-SK-Altitude-001, where altitude is the only independent variable, and the re-designed
GOLDEN-SK-YakLa-001 dive/zoom profiles). Here we prove the *physics* of the reference holds:
the sealed LUT is a faithful stand-in for the ICAO ISA power law it was generated from, it
clamps and stays monotone, and the kernel genuinely consumes it — the same airframe with the
same commands performs measurably worse aloft (thrust) and its aerodynamic load-factor ceiling
scales exactly with the density ratio.
"""
import json
import math
import sys
from pathlib import Path

from hypothesis import given, settings, strategies as st

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import ref_kernel as rk          # noqa: E402
import envelopes as envmod       # noqa: E402

RAILS = json.loads((ROOT / "config" / "rails" / "atm.json").read_text(encoding="utf-8"))
ROSTER = ("ki61", "bf109f4", "a6m2", "yak3", "la7", "spitfire_mk5", "p47d", "p51")
G0 = 9.80665
DT = 0.01


def _isa_power_law(h):
    # The OFFLINE provenance formula (tools/gen_isa_lut.py) — ICAO ISA troposphere. The kernel
    # never evaluates this; the sealed spec is the LUT. Reproduced here so the test documents
    # (and guards) where the sealed nodes came from.
    T0, L, RS = 288.15, 0.0065, 287.05287
    return ((T0 - L * h) / T0) ** (G0 / (RS * L) - 1.0)


def test_sigma_lut_endpoints_nodes_and_monotone():
    # Sea level is exactly 1.0; every node equals the power law it was generated from (bit-for-bit
    # — the generator seals f64(pow(...)) verbatim); strictly decreasing at nodes AND midpoints.
    assert rk.air_sigma(0.0) == 1.0
    for h, v in zip(rk.ISA_SIGMA_ALT, rk.ISA_SIGMA):
        assert v == _isa_power_law(h)
    samples = [rk.air_sigma(h) for h in [x * 250.0 for x in range(33)]]  # nodes + midpoints
    for a, b in zip(samples, samples[1:]):
        assert b < a
    assert all(0.0 < s <= 1.0 for s in samples)


@given(h=st.floats(min_value=0.0, max_value=8000.0))
@settings(max_examples=200, deadline=None)
def test_sigma_lut_tracks_the_power_law(h):
    # Between nodes the piecewise-linear LUT stays within ~2.2e-4 of the ISA power law (the
    # worst-case chord error of a 500 m segment near the ground) — the fidelity claim of the ADR.
    assert abs(rk.air_sigma(h) - _isa_power_law(h)) < 2.5e-4


def test_sigma_clamps_outside_the_band():
    # lut_eval clamps at the end nodes (alt is kernel-clamped to [0, 8000] anyway — belt and
    # braces for any future caller).
    assert rk.air_sigma(-100.0) == rk.ISA_SIGMA[0]
    assert rk.air_sigma(9000.0) == rk.ISA_SIGMA[-1]


def _fly_level_full_throttle(envname, alt0, ticks=2000, tas0=120.0):
    k = rk.Kernel(RAILS)
    k.aircraft.append(rk.Aircraft(0.0, 0.0, 0.0, 0.0, float(alt0), tas0))
    env = envmod.load_envelope(envname)
    for _ in range(ticks):
        k.step_scenario([(0.0, 1.0, 1.0)], [env])
    return k.aircraft[0]


@given(env=st.sampled_from(ROSTER))
@settings(max_examples=len(ROSTER), deadline=None)
def test_altitude_degrades_acceleration(env):
    # The SAME airframe under the SAME full-throttle level command accelerates measurably slower
    # at 7000 m (sigma ~0.48) than at 1000 m (sigma ~0.91): engine power scales with density.
    low = _fly_level_full_throttle(env, 1000.0)
    high = _fly_level_full_throttle(env, 7000.0)
    assert low.tas > high.tas + 4.0


@given(env=st.sampled_from(ROSTER))
@settings(max_examples=len(ROSTER), deadline=None)
def test_aero_ceiling_scales_exactly_with_sigma(env):
    # At an aero-saturated speed the achieved load factor IS n_aero = cl_max*q*S/(m*g0), and q
    # carries sigma linearly -> the one-tick achieved n at two altitudes must sit in EXACTLY the
    # ratio of the sigmas (the gamma-recovery trick from test_stall reads the limiter's output).
    def achieved_n(alt, V=55.0):
        k = rk.Kernel(RAILS)
        k.aircraft.append(rk.Aircraft(0.0, 0.0, 0.0, 0.0, float(alt), V))
        e = envmod.load_envelope(env)
        k.step_scenario([(0.0, 50.0, 0.0)], [e])   # over-command: saturates at n_aero
        ac = k.aircraft[0]
        return 1.0 + ac.gamma * ac.tas / (G0 * DT)
    n_lo, n_hi = achieved_n(500.0), achieved_n(6500.0)
    s_lo, s_hi = rk.air_sigma(500.0), rk.air_sigma(6500.0)
    assert n_hi < n_lo                                   # thinner air -> lower ceiling
    assert math.isclose(n_hi * s_lo, n_lo * s_hi, rel_tol=1e-9)
