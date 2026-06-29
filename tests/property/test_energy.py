"""Metamorphic properties for the B2 lift-&-pitch flight model (ATM-Sphere v1.6r0).

Bit-exact C++<->reference parity for the kernel is proven by the scenario goldens
(GOLDEN-SK-{Sphere,Turn,Climb,TurnClimb,Accel,Pitch}-001) and the lockstep/predict generated-
vector gates. Here we prove the *physics* of the reference holds: idle decelerates, throttle
reaches an equilibrium, turning bleeds energy (induced drag), and — new in B2 — pitch is real:
a commanded load factor n turns the velocity vector (flight-path angle gamma is a stored state),
altitude is EARNED, pulling g climbs AND bleeds speed, pushing over dives AND rebuilds speed, and
level flight (n=1 wings level, or n=1/cos(phi) in a bank) holds altitude.
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


def _fly(envname, phi_deg, g_cmd, throttle, ticks, tas0=140.0, alt0=2000.0):
    """Run one aircraft for `ticks` at a fixed command; return a trace of (tas, alt, gamma, psi)
    tuples (len ticks+1, state[0] = initial)."""
    k = rk.Kernel(RAILS)
    k.aircraft.append(rk.Aircraft(0.0, 0.0, 0.0, 0.0, alt0, tas0))
    env = envmod.load_envelope(envname)
    cmd = (rk.deg2rad(phi_deg), float(g_cmd), float(throttle))
    ac = k.aircraft[0]
    trace = [(ac.tas, ac.alt, ac.gamma, ac.psi)]
    for _ in range(ticks):
        k.step_scenario([cmd], [env])
        trace.append((ac.tas, ac.alt, ac.gamma, ac.psi))
    return trace


def _tas(trace):
    return [s[0] for s in trace]


@given(env=st.sampled_from(ROSTER))
@settings(max_examples=len(ROSTER), deadline=None)
def test_idle_wings_level_decelerates(env):
    # 1 g wings level, zero throttle: gamma stays ~0 and drag monotonically bleeds speed to V_MIN.
    tr = _fly(env, 0.0, 1.0, 0.0, ticks=400, tas0=160.0)
    tas = _tas(tr)
    for a, b in zip(tas, tas[1:]):
        assert b <= a + 1e-12
        assert b >= rk.V_MIN - 1e-9
    assert tas[-1] < tas[0] - 1.0                       # meaningfully slower
    assert abs(tr[-1][2]) < 1e-6                        # gamma held ~0 (level)


@given(env=st.sampled_from(ROSTER))
@settings(max_examples=len(ROSTER), deadline=None)
def test_throttle_reaches_equilibrium(env):
    # Full throttle, 1 g wings level: TAS approaches a fixed point — the per-tick change shrinks.
    tr = _tas(_fly(env, 0.0, 1.0, 1.0, ticks=8000, tas0=120.0))
    early = abs(tr[100] - tr[99])
    late = abs(tr[-1] - tr[-2])
    assert late <= early + 1e-12                        # converging, not diverging
    assert late < 1e-3                                  # effectively settled after 80 s


@given(env=st.sampled_from(ROSTER), thr=st.floats(min_value=0.3, max_value=1.0))
@settings(max_examples=24, deadline=None)
def test_sustained_turn_bleeds_more_than_level(env, thr):
    # Same throttle/time: a sustained LEVEL turn (n=1/cos(phi)) bleeds more than wings-level 1 g
    # flight (higher load factor -> higher induced drag). Both hold ~level so gravity is no confound.
    g_turn = 1.0 / math.cos(math.radians(55.0))
    level = _fly(env, 0.0, 1.0, thr, ticks=600, tas0=150.0)
    turn = _fly(env, 55.0, g_turn, thr, ticks=600, tas0=150.0)
    assert _tas(turn)[-1] < _tas(level)[-1]


@given(env=st.sampled_from(ROSTER), thr=st.floats(min_value=0.3, max_value=1.0))
@settings(max_examples=24, deadline=None)
def test_pull_g_climbs_and_costs_speed(env, thr):
    # Pulling g>1 wings level rotates the nose up: altitude is EARNED (climbs) AND it ends slower
    # than level at the same throttle (induced drag + gravity-along-path = energy bleed in a pull).
    level = _fly(env, 0.0, 1.0, thr, ticks=500, tas0=160.0)
    pull = _fly(env, 0.0, 1.6, thr, ticks=500, tas0=160.0)
    assert pull[-1][1] > level[-1][1] + 1.0            # climbed (alt up)
    assert pull[-1][2] > 1e-3                           # positive flight-path angle
    assert _tas(pull)[-1] < _tas(level)[-1]            # and slower (energy traded for height)


@given(env=st.sampled_from(ROSTER))
@settings(max_examples=len(ROSTER), deadline=None)
def test_pushover_dives_and_builds_speed(env):
    # Pushing to <1 g wings level drops the nose: gamma goes negative, altitude is lost, and the
    # dive REBUILDS speed vs 1 g level at the same throttle (gravity now adds energy).
    level = _fly(env, 0.0, 1.0, 0.5, ticks=400, tas0=140.0)
    dive = _fly(env, 0.0, 0.3, 0.5, ticks=400, tas0=140.0)
    assert dive[-1][1] < level[-1][1] - 1.0            # descended
    assert dive[-1][2] < -1e-3                          # negative flight-path angle
    assert _tas(dive)[-1] > _tas(level)[-1]            # dive built speed relative to level


@given(env=st.sampled_from(ROSTER), bank=st.floats(min_value=0.0, max_value=50.0))
@settings(max_examples=24, deadline=None)
def test_level_flight_equilibrium_holds_altitude(env, bank):
    # Commanding the level load factor n=1/cos(phi) holds gamma ~0 and altitude ~constant — the
    # core B2 invariant (a 1 g wings-level case at bank=0, a sustained level turn otherwise).
    g_level = 1.0 / math.cos(math.radians(bank))
    tr = _fly(env, bank, g_level, 0.8, ticks=400, tas0=150.0, alt0=3000.0)
    # gamma stays small throughout (a few-degree transient during roll-in, since n is set for the
    # target bank while phi is still slewing — bigger at steeper bank); altitude barely moves over
    # 4 s. Swept worst case over all envelopes x bank[0,50] is ~2.3 deg / ~15 m; bound with margin.
    assert max(abs(s[2]) for s in tr) < 6e-2           # < ~3.4 deg
    assert abs(tr[-1][1] - tr[0][1]) < 25.0            # < 25 m drift over 4 s


@given(env=st.sampled_from(ROSTER))
@settings(max_examples=len(ROSTER), deadline=None)
def test_higher_g_turns_faster_instantaneously(env):
    # Sustained vs instantaneous turn: at the SAME bank, a harder pull (higher n) yaws the velocity
    # vector faster NOW (more heading change over a short window) but bleeds more energy.
    soft = _fly(env, 45.0, 1.4142135623730951, 0.9, ticks=100, tas0=180.0)
    hard = _fly(env, 45.0, 3.0, 0.9, ticks=100, tas0=180.0)
    dpsi_soft = abs(soft[-1][3] - soft[0][3])
    dpsi_hard = abs(hard[-1][3] - hard[0][3])
    assert dpsi_hard > dpsi_soft                        # harder pull turns faster instantaneously
    assert _tas(hard)[-1] < _tas(soft)[-1]             # at the cost of more energy


def test_pitch_is_deterministic():
    # Same inputs -> identical trace incl. gamma (no wall-clock / RNG leakage into the model).
    a = _fly("p51", 30.0, 1.8, 0.8, ticks=500)
    b = _fly("p51", 30.0, 1.8, 0.8, ticks=500)
    assert a == b
