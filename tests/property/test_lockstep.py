"""Lockstep invariants for the loopback desync tripwire (netcode layer 3).

Byte-exact C++<->reference parity of the per-tick hash sequence is proven by the generated-vector
gate (src/net/lockstep_test_main.cpp). Here we prove the reference harness itself is sound:
two kernels stepped from the SAME randomized timeline stay bit-identical every tick, ANY desync
trips the tripwire, and the sequence digest is reproducible. Net code stays outside the kernel:
inputs are sim Commands (bank / g-command / throttle), never wire bits."""
import sys
from pathlib import Path

import pytest
from hypothesis import given, strategies as st, settings

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import lockstep_ref as ls

# Random shared timelines: a few aircraft, a handful of phases, modest tick counts (fast).
BANK = st.floats(min_value=-80.0, max_value=80.0, allow_nan=False, allow_infinity=False)
# commanded load factor (g): bounded so a few hundred ticks keeps gamma well clear of the
# cos(gamma)->0 vertical singularity (determinism holds regardless; this keeps it physical).
GLOAD = st.floats(min_value=-1.0, max_value=5.0, allow_nan=False, allow_infinity=False)
ENVNAMES = ["ki61", "bf109f4", "spitfire_mk5", "p47d", "a6m2", "yak3", "la7", "p51"]


def _phase(start_tick, bank, g_cmd, throttle=0.5):
    return {"start_tick": start_tick, "bank_deg": bank, "g_cmd": g_cmd, "throttle": throttle}


@st.composite
def scenarios(draw, max_ac=3, max_ticks=80):
    ticks = draw(st.integers(min_value=5, max_value=max_ticks))
    n_ac = draw(st.integers(min_value=1, max_value=max_ac))
    aircraft = []
    for _ in range(n_ac):
        env = draw(st.sampled_from(ENVNAMES))
        lat = draw(st.floats(min_value=-60.0, max_value=60.0, allow_nan=False))
        lon = draw(st.floats(min_value=-170.0, max_value=170.0, allow_nan=False))
        psi = draw(st.floats(min_value=0.0, max_value=359.0, allow_nan=False))
        alt = draw(st.floats(min_value=100.0, max_value=7900.0, allow_nan=False))
        tas = draw(st.floats(min_value=80.0, max_value=180.0, allow_nan=False))
        # 1..3 phases with strictly increasing start ticks inside [0, ticks)
        n_ph = draw(st.integers(min_value=1, max_value=3))
        starts = sorted(set(draw(st.lists(st.integers(min_value=0, max_value=ticks - 1),
                                          min_size=n_ph, max_size=n_ph))))
        if not starts or starts[0] != 0:
            starts = [0] + starts
        sched = [_phase(s, draw(BANK), draw(GLOAD)) for s in starts]
        aircraft.append({"envelope": env,
                         "start": {"lat_deg": lat, "lon_deg": lon, "psi_deg": psi,
                                   "phi_deg": 0.0, "alt_m": alt, "tas_mps": tas},
                         "schedule": sched})
    return {"id": "LOCKSTEP-PROP", "ticks": ticks, "aircraft": aircraft}


@settings(max_examples=60, deadline=None)
@given(scenarios())
def test_random_timeline_stays_in_sync(scenario):
    r = ls.run_lockstep(scenario=scenario)
    assert r["in_sync"], f"diverged at tick {r['divergent_tick']}"
    assert len(r["per_tick"]) == scenario["ticks"]


@settings(max_examples=40, deadline=None)
@given(scenarios())
def test_any_state_desync_trips(scenario):
    # nudging one kernel's initial altitude by a tiny exact amount must trip the tripwire.
    r = ls.run_lockstep(scenario=scenario, perturb=(0, "alt", float.fromhex("0x1p-20")))
    assert not r["in_sync"]
    assert r["divergent_tick"] == 1  # state perturbation shows in the very first snapshot


def test_sealed_scenario_in_sync_and_reproducible():
    a = ls.run_lockstep()
    b = ls.run_lockstep()
    assert a["in_sync"] and b["in_sync"]
    assert ls.sequence_digest(a["per_tick"]) == ls.sequence_digest(b["per_tick"])


def test_command_desync_trips():
    # if the two ends ever feed DIFFERENT inputs, the hashes must diverge — that is exactly the
    # condition the tripwire exists to catch. Drive A and B with one differing command tick.
    k_a, sched, envs = ls.build_kernel()
    k_b, _, _ = ls.build_kernel()
    import hashlib
    diverged = -1
    for t in range(50):
        cmds = ls.commands_at(sched, t)
        cmds_b = list(cmds)
        if t == 10:  # inject a one-tick input desync on aircraft 0
            phi, g_cmd, thr = cmds_b[0]
            cmds_b[0] = (phi + 0.1, g_cmd, thr)
        k_a.step_scenario(cmds, envs)
        k_b.step_scenario(cmds_b, envs)
        ha = hashlib.sha256(k_a.snapshot(t + 1)).hexdigest()
        hb = hashlib.sha256(k_b.snapshot(t + 1)).hexdigest()
        if ha != hb:
            diverged = t + 1
            break
    assert diverged == 11  # the desynced input tick (t=10) shows at tick 11
