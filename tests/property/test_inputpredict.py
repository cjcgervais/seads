"""Predictive INPUT CLIENT properties (netcode layer 17) — the round-trip loop closed.

The layer-4b Predictor driven against the layer-15b/16 authoritative input server. Cross-impl byte
parity with the C++ mirror (src/net/inputclient) is pinned by the two shared digests in
tools/inputpredict_ref.py + src/net/netpredict_test_main.cpp; here we prove the reference is
self-consistent and that the three defining behaviors hold:

  * SEAMLESS — a client that predicts its own aircraft from the SAME commands it upstreams reconciles
    INVISIBLY against the canonical authoritative state: predicted == authoritative EVERY tick, the
    reconcile a zero-correction no-op (the round-trip theorem);
  * BOUNDED — reconciling against the decoded, lossy protocol-7 wire (the realistic path) keeps the
    reconstructed own ship within a few wire quanta of the authoritative trajectory, and is reproducible;
  * HEAL — a command the client applied locally but the server DROPPED as STALE makes it mispredict; the
    reconcile heals it exactly at the first authoritative frame clearing the bad input, and a
    no-reconcile control stays broken forever (the reconcile, not luck, is load-bearing).
"""
import sys
from pathlib import Path

from hypothesis import given, settings, strategies as st

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import inputpredict_ref as ip


def test_seamless_canonical_in_sync_and_pinned():
    """A correctly-predicting client is in sync every tick; its digest matches the pin."""
    _, states = ip.run_truth()
    r = ip.run_client(states, reconcile=True, source="canonical")
    assert r["in_sync"] is True
    assert r["first_divergent"] == -1
    assert r["max_pos_err"] == 0.0
    assert r["digest"] == ip.PIN_CANONICAL_DIGEST


def test_seamless_predicted_equals_truth_hashes():
    """The seamless per-tick own hashes ARE the authoritative truth hashes (zero correction)."""
    truth_hashes, states = ip.run_truth()
    r = ip.run_client(states, reconcile=True, source="canonical")
    assert r["per_tick"] == truth_hashes


def test_canonical_digest_reproducible():
    _, states = ip.run_truth()
    a = ip.run_client(states, source="canonical")["digest"]
    b = ip.run_client(states, source="canonical")["digest"]
    assert a == b == ip.PIN_CANONICAL_DIGEST


def test_wire_reconcile_bounded_and_pinned():
    """Reconciling against the lossy wire is bounded (not bit-exact) and reproducible."""
    _, states = ip.run_truth()
    r = ip.run_client(states, reconcile=True, source="wire")
    assert r["max_pos_err"] > 0.0            # lossy: not bit-exact ...
    assert r["max_pos_err"] < 1e-3           # ... but tightly bounded by the wire quantum
    assert r["digest"] == ip.PIN_WIRE_DIGEST
    assert ip.run_client(states, source="wire")["digest"] == ip.PIN_WIRE_DIGEST


def test_wire_and_canonical_digests_differ():
    """The lossy-wire reconstruction is genuinely different from the bit-exact canonical one."""
    assert ip.PIN_WIRE_DIGEST != ip.PIN_CANONICAL_DIGEST


def test_stale_dropped_command_heals():
    """A locally-applied command the server dropped STALE mispredicts, then heals at the next
    authoritative frame that clears the whole divergent (hold-last) window."""
    _, states = ip.run_truth()
    local = ip._stale_schedule()
    healed = ip.run_client(states, local_schedule=local, reconcile=True, source="canonical")
    assert healed["first_divergent"] == int(ip.INPUT_SK["stale_cmd"][0]) + 1
    assert healed["heal_tick"] == ip._first_heal_tick()
    assert healed["in_sync"] is False        # it did diverge (then healed at the very end window)


def test_no_reconcile_control_never_heals():
    """Same misprediction WITHOUT reconcile stays broken forever (reconcile is load-bearing)."""
    _, states = ip.run_truth()
    local = ip._stale_schedule()
    broken = ip.run_client(states, local_schedule=local, reconcile=False, source="canonical")
    assert broken["in_sync"] is False
    assert broken["heal_tick"] == -1
    assert broken["first_divergent"] == int(ip.INPUT_SK["stale_cmd"][0]) + 1


@given(perturb=st.sampled_from([float.fromhex("0x1p-20"), 1e-4, 1e-2, 0.5]))
@settings(max_examples=4, deadline=None)
def test_perturbed_initial_state_heals_under_canonical(perturb):
    """ANY finite initial-state desync is snapped back exactly by the first canonical reconcile and
    stays in sync thereafter — the classic reconciliation guarantee against the authoritative server."""
    _, states = ip.run_truth()
    r = ip.run_client(states, reconcile=True, source="canonical", perturb_alt=perturb)
    assert r["first_divergent"] == 1         # the perturbation shows immediately
    assert r["heal_tick"] > 0                # ... and is healed
    assert r["in_sync"] is False             # (diverged before the heal)
    ctrl = ip.run_client(states, reconcile=False, source="canonical", perturb_alt=perturb)
    assert ctrl["heal_tick"] == -1           # no-reconcile control never recovers
