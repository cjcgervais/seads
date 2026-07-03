"""Metamorphic properties for the two-speed blower schedule (ATM-Sphere v1.25r0).

The v1.22r0 single-critical-altitude lapse became, for airframes carrying a two-speed
supercharger (crit_lo_alt_m > 0):

    lapse = max( min(1, sigma(alt)/sigma(crit_lo_alt_m)),                 # LOW (MS) gear
                 gear2_frac * min(1, sigma(alt)/sigma(crit_alt_m)) )      # HIGH (FS) gear

— rated power to crit_lo, falling to the gear-shift altitude (where the branches cross), FLAT
at the dyadic gear2_frac to crit_alt, then falling with the density ratio (flat-fall-flat-fall).
crit_lo_alt_m = 0 (with gear2_frac = 1) is single-speed: the kernel NEVER enters the two-speed
branch, so the v1.22r0 lapse path is reproduced bit-for-bit. Bit-exact C++ <-> Python parity is
proven by GOLDEN-SK-Blower-001 (all three below-crit regimes + the in-flight max() flip) and
GOLDEN-SK-YakLa-001 (the La-7's double gear-shift crossing); here we prove the reference's
branch SEMANTICS through the kernel and the contracts the probe enforces.
"""
import json
import sys
from pathlib import Path

from hypothesis import given, settings, strategies as st

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import ref_kernel as rk          # noqa: E402
import envelopes as envmod       # noqa: E402

RAILS = json.loads((ROOT / "config" / "rails" / "atm.json").read_text(encoding="utf-8"))
ROSTER = ("ki61", "bf109f4", "a6m2", "yak3", "la7", "spitfire_mk5", "p47d", "p51")
TWO_SPEED = ("yak3", "la7", "p51")
G0 = 9.80665
DT = 0.01

# The sealed roster values (WWII blower-gear ratings on the 500 m / sixteenths grids). An
# intentional retune must update BOTH the envelope JSON and this pin — and expect every golden
# containing that airframe to move (that is the point).
SEALED_BLOWER = {
    "p51":          (3000.0, 14.0 / 16.0),   # V-1650-7 two-speed two-stage Merlin
    "la7":          (1500.0, 14.0 / 16.0),   # ASh-82FN
    "yak3":         (1000.0, 15.0 / 16.0),   # VK-105PF-2
    "p47d":         (0.0, 1.0),              # turbo — no gears
    "bf109f4":      (0.0, 1.0),              # hydraulic variable-speed coupling — no kink
    "ki61":         (0.0, 1.0),
    "spitfire_mk5": (0.0, 1.0),              # Merlin 45 single-speed
    "a6m2":         (0.0, 1.0),              # Sakae 12 single-speed
}


def _lapse(e, alt):
    """The v1.25r0 lapse, replicated op-for-op from Kernel::step / ref_kernel.step_scenario."""
    sigma = rk.air_sigma(alt)
    lapse = sigma / rk.air_sigma(e["crit_alt_m"])
    if lapse > 1.0:
        lapse = 1.0
    if e["crit_lo_alt_m"] > 0.0:
        lo_gear = sigma / rk.air_sigma(e["crit_lo_alt_m"])
        if lo_gear > 1.0:
            lo_gear = 1.0
        hi_gear = e["gear2_frac"] * lapse
        lapse = lo_gear if lo_gear > hi_gear else hi_gear
    return lapse


def test_roster_blower_contract_and_flavor():
    # Contract (mirrors tuning_probe.validate_supercharger): single-speed means crit_lo = 0 AND
    # gear2 = 1; two-speed means crit_lo a 500 m multiple strictly inside (0, crit_alt), gear2 a
    # 1/16 multiple with sigma(crit)/sigma(crit_lo) < gear2 < 1 (both gears non-degenerate).
    # Flavor: exactly the two Soviet fighters + the Merlin Mustang carry gears.
    for name in ROSTER:
        e = envmod.load_envelope(name)
        lo, g2 = e["crit_lo_alt_m"], e["gear2_frac"]
        assert (lo, g2) == SEALED_BLOWER[name]
        if lo == 0.0:
            assert g2 == 1.0
        else:
            assert lo / 500.0 == round(lo / 500.0)
            assert 0.0 < lo < e["crit_alt_m"]
            assert g2 * 16.0 == round(g2 * 16.0)
            floor = rk.air_sigma(e["crit_alt_m"]) / rk.air_sigma(lo)
            assert floor < g2 < 1.0
    assert tuple(n for n in ROSTER if SEALED_BLOWER[n][0] > 0.0) == ("yak3", "la7", "p51")


def _one_tick_vnew(e, V, alt, thr=1.0):
    """One wings-level 1 g tick through the kernel; returns the integrated TAS."""
    k = rk.Kernel(RAILS)
    k.aircraft.append(rk.Aircraft(0.0, 0.0, 0.0, 0.0, float(alt), float(V)))
    k.step_scenario([(0.0, 1.0, float(thr))], [e])
    return k.aircraft[0].tas


def _expected_vnew(e, V, alt, thr=1.0):
    """Replicate the kernel's speed integration op-for-op (phi=0, gamma=0, n=1 un-clamped)."""
    sigma = rk.air_sigma(alt)
    qS = 0.5 * rk.RHO0 * sigma * V * V * e["wing_area_m2"]
    CL = (1.0 * e["mass_kg"] * G0) / qS
    D = qS * e["cd0"] + e["induced_k"] * CL * CL * qS
    T = thr * e["thrust_static_n"] * (1.0 - V / e["v_max_mps"]) * _lapse(e, alt)
    if T < 0.0:
        T = 0.0
    return V + ((T - D) / e["mass_kg"]) * DT


def _shift_alt(e):
    """Gear-shift altitude (display-grade): where sigma(alt) = gear2 * sigma(crit_lo)."""
    target = e["gear2_frac"] * rk.air_sigma(e["crit_lo_alt_m"])
    a = e["crit_lo_alt_m"]
    while a < e["crit_alt_m"] and rk.air_sigma(a) > target:
        a += 1.0
    return a


@given(env=st.sampled_from(TWO_SPEED))
@settings(max_examples=len(TWO_SPEED), deadline=None)
def test_two_speed_lapse_bit_exact_through_the_kernel(env):
    # One-tick bit-exact replication in EVERY regime: rated below crit_lo, at crit_lo, the
    # falling MS branch, both sides of the gear shift, the flat FS band, at crit, above crit.
    e = envmod.load_envelope(env)
    lo, crit = e["crit_lo_alt_m"], e["crit_alt_m"]
    sh = _shift_alt(e)
    alts = [0.0, max(0.0, lo - 250.0), lo, 0.5 * (lo + sh), sh - 100.0, sh + 100.0,
            0.5 * (sh + crit), crit, min(8000.0, crit + 500.0)]
    for alt in alts:
        assert _one_tick_vnew(e, 120.0, alt) == _expected_vnew(e, 120.0, alt)


@given(env=st.sampled_from(tuple(n for n in ROSTER if SEALED_BLOWER[n][0] == 0.0)))
@settings(max_examples=5, deadline=None)
def test_single_speed_never_enters_the_branch(env):
    # For crit_lo_alt_m = 0 the two-speed code path is UNREACHABLE, so the tick equals the
    # v1.22r0 replication with no gear terms at all — the additive-fields-are-inert proof.
    e = envmod.load_envelope(env)
    assert e["crit_lo_alt_m"] == 0.0 and e["gear2_frac"] == 1.0
    for alt in (0.0, 800.0, e["crit_alt_m"], min(8000.0, e["crit_alt_m"] + 1000.0), 7750.0):
        sigma = rk.air_sigma(alt)
        v122 = sigma / rk.air_sigma(e["crit_alt_m"])
        if v122 > 1.0:
            v122 = 1.0
        assert _lapse(e, alt) == v122
        assert _one_tick_vnew(e, 120.0, alt) == _expected_vnew(e, 120.0, alt)


@given(env=st.sampled_from(TWO_SPEED))
@settings(max_examples=len(TWO_SPEED), deadline=None)
def test_lapse_shape_flat_fall_flat_fall(env):
    # The schedule's shape: rated (exactly 1) below crit_lo; strictly falling in the MS branch;
    # EXACTLY the dyadic gear2_frac constant across the FS flat band; strictly falling above
    # crit; and never above 1 nor below gear2_frac * sigma(8000)/sigma(crit).
    e = envmod.load_envelope(env)
    lo, crit, g2 = e["crit_lo_alt_m"], e["crit_alt_m"], e["gear2_frac"]
    sh = _shift_alt(e)
    assert _lapse(e, 0.0) == 1.0
    assert _lapse(e, lo) == 1.0                       # sigma(lo)/sigma(lo) == 1 exactly
    a, step = lo, 100.0
    while a + step <= sh - 1.0:                       # MS branch: strictly falling
        assert _lapse(e, a) > _lapse(e, a + step)
        a += step
    a = sh + 2.0
    while a <= crit:                                  # FS flat band: the dyadic constant, exact
        assert _lapse(e, a) == g2
        a += 250.0
    assert _lapse(e, crit) == g2                      # sigma(crit)/sigma(crit) == 1 exactly
    a = crit
    while a + 250.0 <= 8000.0:                        # above crit: strictly falling again
        assert _lapse(e, a) > _lapse(e, a + 250.0)
        a += 250.0
    for alt in (0.0, lo, sh + 50.0, crit, 8000.0):
        assert g2 * rk.air_sigma(8000.0) / rk.air_sigma(crit) <= _lapse(e, alt) <= 1.0


def test_blower_golden_setup_not_degenerate():
    # Guards GOLDEN-SK-Blower-001's raison d'être: identical schedules from the same start,
    # BELOW the Yak-3's MS-gear full-throttle height (so all three below-crit regimes + the
    # gear-shift flip are ahead of it), beside a genuinely single-speed control ship.
    doc = json.loads((ROOT / "config" / "scenarios" / "GOLDEN-SK-Blower-001.json")
                     .read_text(encoding="utf-8"))
    a0, a1 = doc["aircraft"]
    assert (a0["envelope"], a1["envelope"]) == ("yak3", "bf109f4")
    assert a0["schedule"] == a1["schedule"]
    assert a0["start"]["alt_m"] == a1["start"]["alt_m"] == 800.0
    yak = envmod.load_envelope("yak3")
    bf = envmod.load_envelope("bf109f4")
    assert a0["start"]["alt_m"] < yak["crit_lo_alt_m"]     # rated LOW gear at the start
    assert bf["crit_lo_alt_m"] == 0.0                      # the control ship has no gears
    assert bf["crit_alt_m"] >= 6000.0                      # and never runs out in this band


def test_sea_level_rated_power_is_low_gear():
    # At sea level BOTH mins clamp for a two-speed airframe and the max picks the LOW gear's
    # rated 1.0 (not gear2_frac) — the low gear is what full power at the deck means.
    for name in TWO_SPEED:
        e = envmod.load_envelope(name)
        assert _lapse(e, 0.0) == 1.0
        assert _lapse(e, 0.0) > e["gear2_frac"]
