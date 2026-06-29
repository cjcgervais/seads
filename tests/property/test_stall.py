"""Metamorphic properties for the B3 limits & stall flight model (ATM-Sphere v1.7r0).

B3 bounds the achievable load factor n per-airframe by BOTH a structural g limit
(n_min_struct..n_max_struct) AND the C_Lmax aerodynamic ceiling
    n_aero = cl_max * qS / (m*g0)      (qS = 1/2 * rho0 * V^2 * S)
so n = clamp(n_cmd, max(n_min_struct, -n_aero), min(n_max_struct, n_aero)). Below the corner
speed V* = stall*sqrt(n_max_struct) the aero ceiling is the binding limit and the turn collapses
(accelerated stall); above it the structural limit binds. The 1 g stall speed (where n_aero = 1)
falls out for free. These properties prove the *physics* of the reference; bit-exact C++<->Python
parity is proven by GOLDEN-SK-Stall-001 + the lockstep/predict vector gates.

Key trick: a 1-tick wings-level (phi=0, gamma=0) step makes the achieved load factor recoverable.
The kernel integrates gamma = (g0/Vnew)*(n-1)*dt, so n = 1 + gamma*Vnew/(g0*dt) recovers the
EXACT clamped n the kernel used (the Vnew cancels) — letting us read what the limiter produced.
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


def _n_aero(e, V):
    qS = 0.5 * rk.RHO0 * V * V * e["wing_area_m2"]
    return e["cl_max"] * qS / (e["mass_kg"] * G0)


def _n_expected(e, V, g_cmd):
    na = _n_aero(e, V)
    hi = min(e["n_max_struct"], na)
    lo = max(e["n_min_struct"], -na)
    return max(lo, min(g_cmd, hi))


def _achieved_n(envname, V, g_cmd, throttle=0.0):
    """Run ONE wings-level tick at speed V commanding g_cmd; recover the load factor the kernel
    actually used (after the structural + aerodynamic limiter)."""
    k = rk.Kernel(RAILS)
    k.aircraft.append(rk.Aircraft(0.0, 0.0, 0.0, 0.0, 4000.0, V))   # phi=0, gamma=0
    e = envmod.load_envelope(envname)
    k.step_scenario([(0.0, float(g_cmd), float(throttle))], [e])
    ac = k.aircraft[0]
    return 1.0 + ac.gamma * ac.tas / (G0 * DT)


def _corner(e):
    # V* where n_aero == n_max_struct  ->  the V-n diagram corner.
    return math.sqrt(2.0 * e["n_max_struct"] * e["mass_kg"] * G0 / (rk.RHO0 * e["wing_area_m2"] * e["cl_max"]))


@given(env=st.sampled_from(ROSTER))
@settings(max_examples=len(ROSTER), deadline=None)
def test_limiter_matches_clamp_formula(env):
    # The achieved load factor equals the structural+aero clamp at a spread of speeds / commands.
    e = envmod.load_envelope(env)
    for V in (60.0, 90.0, 120.0, 160.0):
        for g in (0.5, 1.5, 4.0, 9.0, -2.0):
            assert abs(_achieved_n(env, V, g) - _n_expected(e, V, g)) < 1e-9


@given(env=st.sampled_from(ROSTER))
@settings(max_examples=len(ROSTER), deadline=None)
def test_accelerated_stall_caps_load_factor(env):
    # Slow + hard pull: the wing cannot make the commanded g; n saturates at n_aero < commanded
    # AND below the structural limit. Over-commanding harder yields the SAME capped n (a true
    # ceiling, not unbounded lift) -> the accelerated stall.
    e = envmod.load_envelope(env)
    V = 55.0
    n9 = _achieved_n(env, V, 9.0)
    n50 = _achieved_n(env, V, 50.0)
    assert abs(n9 - n50) < 1e-9                      # saturated: more command, same n
    assert n9 < e["n_max_struct"] - 1e-6            # aero-limited, not structural
    assert n9 < 9.0 - 1e-6                           # genuinely capped below the command
    assert abs(n9 - _n_aero(e, V)) < 1e-9           # capped exactly at the C_Lmax ceiling


@given(env=st.sampled_from(ROSTER))
@settings(max_examples=len(ROSTER), deadline=None)
def test_structural_limit_binds_above_corner(env):
    # Fast + hard pull: lift is available, so the per-airframe STRUCTURAL g limit binds. Commanding
    # 9 g or 50 g both yield exactly n_max_struct (and the wing could make more aerodynamically).
    e = envmod.load_envelope(env)
    V = max(160.0, _corner(e) * 1.3)
    assert _n_aero(e, V) > e["n_max_struct"]         # above corner: aero ceiling is slack
    n = _achieved_n(env, V, 9.0)
    assert abs(n - e["n_max_struct"]) < 1e-9
    assert abs(_achieved_n(env, V, 50.0) - e["n_max_struct"]) < 1e-9


@given(env=st.sampled_from(ROSTER))
@settings(max_examples=len(ROSTER), deadline=None)
def test_corner_speed_crossover(env):
    # The corner speed is exactly where max-achievable-g switches from structural to aerodynamic:
    # just above it you can pull full structural g; just below it you cannot.
    e = envmod.load_envelope(env)
    vc = _corner(e)
    above = _achieved_n(env, vc * 1.02, 99.0)
    below = _achieved_n(env, vc * 0.98, 99.0)
    assert abs(above - e["n_max_struct"]) < 1e-3     # full structural g available above corner
    assert below < e["n_max_struct"] - 1e-3          # aero-limited below corner
    assert below < above                             # monotone: slower -> less available g


@given(env=st.sampled_from(ROSTER))
@settings(max_examples=len(ROSTER), deadline=None)
def test_one_g_stall_speed_consistent(env):
    # The 1 g stall speed (n_aero == 1) matches the envelope's declared stall_tas_mps: below it the
    # wing cannot even hold level flight (max n < 1 -> the aircraft must descend / mush).
    e = envmod.load_envelope(env)
    doc = json.loads((ROOT / "data" / "tuning" / "envelopes" / f"{env}.json").read_text())
    v_stall_1g = math.sqrt(2.0 * e["mass_kg"] * G0 / (rk.RHO0 * e["wing_area_m2"] * e["cl_max"]))
    assert abs(v_stall_1g - float(doc["tuning"]["stall_tas_mps"])) < 0.5
    # just below the 1 g stall speed, even commanding lots of g cannot reach n = 1
    assert _achieved_n(env, v_stall_1g * 0.95, 9.0) < 1.0


@given(env=st.sampled_from(ROSTER))
@settings(max_examples=len(ROSTER), deadline=None)
def test_limits_do_not_bind_in_normal_flight(env):
    # Conservative extension: at cruise speed a gentle g passes through UN-clamped (the limiter is
    # invisible to ordinary flight — this is why every pre-B3 golden stayed byte-identical).
    assert abs(_achieved_n(env, 160.0, 1.5) - 1.5) < 1e-9
    assert abs(_achieved_n(env, 160.0, 1.0) - 1.0) < 1e-9


def test_over_corner_turn_is_unsustainable_and_stalls():
    # A sustained turn demanding more than the corner-speed load factor is UNSUSTAINABLE: induced
    # drag bleeds energy, speed decays toward the floor, and the wing can no longer make the
    # demanded g (the aero ceiling has fallen below the command) — the accelerated-stall spiral.
    k = rk.Kernel(RAILS)
    k.aircraft.append(rk.Aircraft(0.0, 0.0, 0.0, math.radians(50.0), 4000.0, 140.0))
    e = envmod.load_envelope("ki61")
    cmd = (math.radians(50.0), 2.0, 0.2)
    ac = k.aircraft[0]
    v0 = ac.tas
    for _ in range(2400):
        k.step_scenario([cmd], [e])
    assert ac.tas < 0.5 * v0                 # energy collapsed: bled well into the stall region
    assert ac.tas <= 60.0                    # near the floor
    assert _n_aero(e, ac.tas) < 2.0 - 1e-6   # can no longer generate the demanded 2 g (stalled)


def test_stall_is_deterministic():
    # Same inputs -> identical trace through the limiter (no wall-clock / RNG leakage).
    def run():
        k = rk.Kernel(RAILS)
        k.aircraft.append(rk.Aircraft(0.0, 0.0, 0.0, math.radians(50.0), 4000.0, 140.0))
        e = envmod.load_envelope("ki61")
        out = []
        for _ in range(800):
            k.step_scenario([(math.radians(50.0), 6.0, 0.2)], [e])
            ac = k.aircraft[0]
            out.append((ac.tas, ac.gamma, ac.psi, ac.alt))
        return out
    assert run() == run()
