"""Metamorphic properties for the supercharger critical altitude (ATM-Sphere v1.22r0).

The B5 thrust density scaling T *= sigma became T *= lapse with
    lapse = min(1, sigma(alt) / sigma(crit_alt_m))
— below its per-airframe critical altitude an engine holds RATED power, above it power falls
with the density ratio. crit_alt_m is a sealed envelope scalar constrained to a multiple of
500 m inside [0, 8000] so sigma(crit_alt_m) is EXACTLY a sealed ISA-LUT node, and
crit_alt_m = 0 reproduces the B5 thrust bit-for-bit (sigma / 1.0 is exact in IEEE-754).
Bit-exact C++ <-> Python parity is proven by GOLDEN-SK-Supercharger-001 (an in-flight branch
flip) + the 13 regenerated goldens; here we prove the reference's branch SEMANTICS through the
kernel (one-tick bit-exact replication of the thrust path on both sides of the comparison) and
the physics it buys (rated power below crit -> acceleration IMPROVES with altitude there;
sigma-ratio lapse above crit -> it degrades there).
"""
import json
import sys
from pathlib import Path

from hypothesis import given, settings, strategies as st

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import ref_kernel as rk          # noqa: E402
import detmath_ref as dm         # noqa: E402
import envelopes as envmod       # noqa: E402

RAILS = json.loads((ROOT / "config" / "rails" / "atm.json").read_text(encoding="utf-8"))
ROSTER = ("ki61", "bf109f4", "a6m2", "yak3", "la7", "spitfire_mk5", "p47d", "p51")
G0 = 9.80665
DT = 0.01

# The sealed roster values (WWII rated/full-throttle heights on the 500 m grid). An intentional
# retune must update BOTH the envelope JSON and this pin — and expect every golden containing
# that airframe to move (that is the point).
SEALED_CRIT = {
    "p47d": 8000.0,          # turbosupercharged R-2800 — rated past the band top
    "p51": 7500.0,           # two-stage Packard Merlin
    "bf109f4": 6000.0,
    "spitfire_mk5": 6000.0,
    "la7": 4500.0,
    "a6m2": 4500.0,
    "ki61": 4000.0,
    "yak3": 3000.0,          # the low-altitude brawler
}


def test_roster_crit_contract_and_flavor():
    # Contract: every crit_alt_m a multiple of 500 inside [0, 8000] (sigma(crit) is a sealed
    # node). Flavor: the turbo P-47D tops the roster, the two-stage P-51 is second, the
    # low-altitude Yak-3 sits strictly at the bottom.
    crits = {}
    for name in ROSTER:
        c = envmod.load_envelope(name)["crit_alt_m"]
        assert c == SEALED_CRIT[name]
        assert 0.0 <= c <= 8000.0
        assert c / 500.0 == round(c / 500.0)
        crits[name] = c
    assert crits["p47d"] == max(crits.values())
    assert crits["p51"] == max(v for k, v in crits.items() if k != "p47d")
    assert crits["yak3"] == min(crits.values())
    assert all(crits["yak3"] < v for k, v in crits.items() if k != "yak3")


def _one_tick_vnew(e, V, alt, thr=1.0):
    """One wings-level 1 g tick through the kernel; returns the integrated TAS."""
    k = rk.Kernel(RAILS)
    k.aircraft.append(rk.Aircraft(0.0, 0.0, 0.0, 0.0, float(alt), float(V)))
    k.step_scenario([(0.0, 1.0, float(thr))], [e])
    return k.aircraft[0].tas


def _expected_vnew(e, V, alt, thr=1.0, crit=None):
    """Replicate the kernel's speed integration op-for-op (phi=0, gamma=0, n=1 un-clamped)."""
    sigma = rk.air_sigma(alt)
    q = 0.5 * rk.RHO0 * sigma * V * V
    qS = q * e["wing_area_m2"]
    L = 1.0 * e["mass_kg"] * G0
    CL = L / qS
    D = qS * e["cd0"] + e["induced_k"] * CL * CL * qS
    sig_c = rk.air_sigma(e["crit_alt_m"] if crit is None else crit)
    lapse = sigma / sig_c
    if lapse > 1.0:
        lapse = 1.0
    T = thr * e["thrust_static_n"] * (1.0 - V / e["v_max_mps"]) * lapse
    if T < 0.0:
        T = 0.0
    Vdot = (T - D) / e["mass_kg"] - G0 * dm.det_sin(0.0)
    return V + Vdot * DT


@given(env=st.sampled_from(ROSTER))
@settings(max_examples=len(ROSTER), deadline=None)
def test_lapse_branches_bit_exact_through_the_kernel(env):
    # The one-tick speed integration matches the replicated thrust path BIT-FOR-BIT on both
    # sides of the critical altitude (and right at it, where lapse == 1 exactly: the node
    # denominator equals the node numerator).
    e = envmod.load_envelope(env)
    crit = e["crit_alt_m"]
    alts = [max(0.0, crit - 1750.0), max(0.0, crit - 500.0), crit,
            min(8000.0, crit + 250.0), min(8000.0, crit + 1750.0), 7750.0]
    for alt in alts:
        assert _one_tick_vnew(e, 120.0, alt) == _expected_vnew(e, 120.0, alt)


@given(env=st.sampled_from(ROSTER))
@settings(max_examples=len(ROSTER), deadline=None)
def test_crit_zero_reproduces_b5_thrust_bit_for_bit(env):
    # crit_alt_m = 0 -> sigma_crit = sigma(0) = 1.0 exactly -> lapse = sigma/1.0 == sigma
    # bit-for-bit -> the whole tick reproduces the B5 (raw T *= sigma) integration exactly.
    assert rk.air_sigma(0.0) == 1.0
    e = dict(envmod.load_envelope(env))
    e["crit_alt_m"] = 0.0
    for alt in (0.0, 800.0, 3210.0, 6900.0):
        sigma = rk.air_sigma(alt)
        assert sigma / rk.air_sigma(0.0) == sigma          # the divide is exact
        assert _one_tick_vnew(e, 120.0, alt) == _expected_vnew(e, 120.0, alt, crit=0.0)


def _fly_level_full_throttle(envname, alt0, ticks=2000, tas0=120.0):
    k = rk.Kernel(RAILS)
    k.aircraft.append(rk.Aircraft(0.0, 0.0, 0.0, 0.0, float(alt0), tas0))
    env = envmod.load_envelope(envname)
    for _ in range(ticks):
        k.step_scenario([(0.0, 1.0, 1.0)], [env])
    return k.aircraft[0]


@given(env=st.sampled_from(("p47d", "p51")))
@settings(max_examples=2, deadline=None)
def test_acceleration_improves_with_altitude_below_crit(env):
    # BELOW the critical altitude the supercharger holds rated power while drag falls with
    # sigma — so the same full-throttle level acceleration ends FASTER up high. (These two
    # airframes keep rated power through 6500 m; the low-crit roster is covered by the
    # above-crit degradation test.)
    low = _fly_level_full_throttle(env, 1000.0)
    high = _fly_level_full_throttle(env, 6500.0)
    assert high.tas > low.tas + 2.0


@given(env=st.sampled_from(tuple(n for n in ROSTER if SEALED_CRIT[n] <= 6000.0)))
@settings(max_examples=6, deadline=None)
def test_acceleration_degrades_above_crit(env):
    # ABOVE the critical altitude thrust falls with sigma/sigma_crit while parasitic drag falls
    # with sigma — the excess shrinks, so acceleration measurably degrades (the B5 behavior,
    # reached once the supercharger runs out).
    crit = SEALED_CRIT[env]
    low = _fly_level_full_throttle(env, crit + 500.0)
    high = _fly_level_full_throttle(env, 8000.0)
    assert low.tas > high.tas + 0.5


def test_supercharger_golden_setup_not_degenerate():
    # Guards GOLDEN-SK-Supercharger-001's raison d'être: identical schedules, the Yak-3 starting
    # BELOW its own critical altitude (so the in-flight upward crossing/branch flip is possible)
    # and the P-51's critical altitude far above anything the profile reaches (so it stays on
    # the rated branch — the contrast ship).
    doc = json.loads((ROOT / "config" / "scenarios" / "GOLDEN-SK-Supercharger-001.json")
                     .read_text(encoding="utf-8"))
    a0, a1 = doc["aircraft"]
    assert (a0["envelope"], a1["envelope"]) == ("yak3", "p51")
    assert a0["schedule"] == a1["schedule"]
    assert a0["start"]["alt_m"] == a1["start"]["alt_m"] == 2700.0
    yak_crit = envmod.load_envelope("yak3")["crit_alt_m"]
    p51_crit = envmod.load_envelope("p51")["crit_alt_m"]
    assert a0["start"]["alt_m"] < yak_crit          # below: the flip is ahead of it
    assert p51_crit >= yak_crit + 3000.0            # the P-51 can't plausibly reach its own
