"""Metamorphic / reconciliation properties for client-side prediction (netcode layer 4b).

Bit-exact C++<->reference parity is proven by the generated-vector gate
(src/net/predict_test_main.cpp). Here we prove the reference's reconciliation logic holds:
a correctly-predicting client is seamless, a state-desynced one is healed exactly by the
reconcile (and stays broken without it), the input buffer stays bounded, and the realistic
LOSSY GEO-001+KIN wire reseed path is bounded by the quantum (not exact) — the reason phi/tas
are on the wire at all.
"""
import sys
from pathlib import Path

from hypothesis import given, settings, strategies as st

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import predict_ref as pr

# Truth run is fixed (deterministic scenario) — compute once, reuse across tests.
TRUTH_HASHES, TRUTH_STATES = pr.run_truth()
HEAL = pr.first_heal_tick()


def test_nominal_prediction_is_seamless():
    # a correctly-predicting client reconciles invisibly: predicted == truth every tick.
    r = pr.run_predictor(TRUTH_STATES, reconcile=True, perturb_alt=0.0)
    assert r["in_sync"]
    assert r["first_divergent"] == -1
    assert r["per_tick"] == TRUTH_HASHES


def test_prediction_is_deterministic():
    a = pr.sequence_digest(pr.run_predictor(TRUTH_STATES)["per_tick"])
    b = pr.sequence_digest(pr.run_predictor(TRUTH_STATES)["per_tick"])
    assert a == b


# A state desync of any sign/size must be healed at the SAME tick: reconcile snaps to the exact
# authoritative state, so the magnitude of the drift is irrelevant to when it heals.
@given(st.floats(min_value=-50.0, max_value=50.0, allow_nan=False, allow_infinity=False)
       .filter(lambda x: abs(x) > 1e-9))
@settings(max_examples=40, deadline=None)
def test_reconcile_heals_any_state_desync(dalt):
    r = pr.run_predictor(TRUTH_STATES, reconcile=True, perturb_alt=dalt)
    assert r["first_divergent"] == 1          # perturbed start diverges immediately
    assert r["heal_tick"] == HEAL             # healed exactly at the first reconcile tick
    assert r["in_sync"] is False              # (in_sync means EVERY tick; early ticks differ)
    # but from HEAL onward it tracks truth exactly:
    assert r["per_tick"][HEAL - 1:] == TRUTH_HASHES[HEAL - 1:]


@given(st.floats(min_value=-50.0, max_value=50.0, allow_nan=False, allow_infinity=False)
       .filter(lambda x: abs(x) > 1e-6))
@settings(max_examples=30, deadline=None)
def test_without_reconcile_drift_persists(dalt):
    # the negative control: the same desync without reconcile never recovers on its own.
    r = pr.run_predictor(TRUTH_STATES, reconcile=False, perturb_alt=dalt)
    assert r["in_sync"] is False
    assert r["heal_tick"] == -1


def test_input_buffer_stays_bounded():
    # reconcile drops inputs at/older than each snapshot tick, so the replay buffer never grows
    # past roughly one latency window — prediction is O(lag), not O(ticks).
    sc = pr.SCENARIO
    env = pr.envmod.load_envelope(sc["envelope"])
    p = pr.Predictor(pr._new_kernel(sc["start"]), env)
    snap_every, lag, ticks = sc["snap_every"], sc["lag_ticks"], sc["ticks"]
    worst = 0
    for t in range(1, ticks + 1):
        p.predict(t, pr.command_at(sc["schedule"], t - 1))
        if t % snap_every == 0 and t > lag:
            p.reconcile(t - lag, TRUTH_STATES[t - lag])
        worst = max(worst, len(p.buffer))
    assert worst <= lag + snap_every   # bounded by the latency window, not the run length


def test_lossy_wire_reseed_is_bounded():
    # the realistic remote / late-join path reseeds from the LOSSY GEO-001+KIN wire snapshot.
    # It is NOT bit-exact (quantization), but the error stays within a small bound — this is the
    # justification for carrying phi/tas on the wire (so a kernel can be re-seeded at all).
    err = pr._lossy_reseed_error(TRUTH_STATES)
    assert err < 1e-3            # radians; lat/lon quantum ~1e-7 deg, replayed <= lag ticks
    assert err > 0.0            # and it really is lossy (not secretly exact)
