"""Remote-aircraft PREDICTION properties (netcode layer 24) — predict OTHERS, not just interpolate.

A client has no remote's input commands (authorization: only its own seat's), so it DEAD-RECKONS a
remote to "now" with the sealed no-arg kinematic tail, reseeding on each authoritative snapshot.
Cross-impl byte parity with the C++ mirror (src/net/remotepredict) is pinned by the two shared digests
in tools/remotepredict_ref.py + src/net/netremotepredict_test_main.cpp; here we prove the reference is
self-consistent and that the defining behaviors hold:

  * TRACKS "NOW" — over the steady window the coast's position error vs the true "now" is a tiny
    fraction of the layer-4a interpolation baseline's structural render-lag error (prediction removes
    the lag interpolation is stuck with);
  * BOUNDED, REPRODUCIBLE — reseeding against the decoded, lossy protocol-7 wire stays within a small
    bound of the authoritative trajectory and yields a reproducible parity digest (like the canonical one);
  * RECONCILE IS LOAD-BEARING — with reconcile the coast stays bounded across the tick-90 maneuver; a
    no-reconcile control (spawn-seeded, never corrected) drifts without bound;
  * LOSS SET is deterministic — dropping emit frames changes the digest reproducibly.
"""
import sys
from pathlib import Path

from hypothesis import given, settings, strategies as st

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import remotepredict_ref as rp


def _states():
    _hashes, states = rp.pred.run_truth(rp.REMOTE_SK)
    return states


def test_coast_beats_interpolation_at_now():
    """The coast's "now" error is far tighter than interpolation's render-lag error (the headline)."""
    states = _states()
    coast = rp.coast_now_error(states, source="canonical")
    interp = rp.interp_now_error(states)
    assert coast > 0.0            # dead-reckoning is not free ...
    assert coast < interp         # ... but it removes interpolation's structural render lag
    assert interp / coast > 100.0  # by a wide margin over the steady window


def test_canonical_digest_pinned_and_reproducible():
    states = _states()
    a = rp.run_remote_client(states, source="canonical")
    b = rp.run_remote_client(states, source="canonical")
    assert a["digest"] == b["digest"] == rp.PIN_CANONICAL_DIGEST


def test_wire_reseed_bounded_and_pinned():
    """Reseeding against the lossy wire is bounded (not bit-exact) and reproducible."""
    states = _states()
    r = rp.run_remote_client(states, reconcile=True, source="wire")
    assert r["max_pos_err"] > 0.0     # dead-reckoning drift is real ...
    assert r["max_pos_err"] < 1e-2    # ... but bounded across the maneuver
    assert r["digest"] == rp.PIN_WIRE_DIGEST
    assert rp.run_remote_client(states, source="wire")["digest"] == rp.PIN_WIRE_DIGEST


def test_wire_and_canonical_digests_differ():
    """The lossy-wire reconstruction is genuinely different from the full-precision one."""
    assert rp.PIN_WIRE_DIGEST != rp.PIN_CANONICAL_DIGEST


def test_reconcile_is_load_bearing():
    """With reconcile the coast stays bounded across the maneuver; no-reconcile drifts without bound."""
    states = _states()
    bounded = rp.run_remote_client(states, reconcile=True, source="canonical")["max_pos_err"]
    drift = rp.run_remote_client(states, reconcile=False, source="canonical")["max_pos_err"]
    assert bounded > 0.0
    assert drift > 10.0 * bounded


def test_loss_set_deterministic():
    """Dropping emit frames changes the digest reproducibly (a deterministic, not random, degradation)."""
    states = _states()
    drops = (20, 40, 60)
    a = rp.run_remote_client(states, source="wire", drop_emit_ticks=drops)["digest"]
    b = rp.run_remote_client(states, source="wire", drop_emit_ticks=drops)["digest"]
    clean = rp.run_remote_client(states, source="wire")["digest"]
    assert a == b            # reproducible under the same loss set
    assert a != clean        # loss genuinely changes the reconstruction


def test_no_reconcile_control_seeds_from_spawn_only():
    """The no-reconcile coast free-runs from the spawn state — it reseeds from zero frames."""
    states = _states()
    r = rp.run_remote_client(states, reconcile=False, source="canonical")
    assert r["delivered"] == 0


@given(drops=st.lists(st.sampled_from([5, 10, 15, 20, 30, 45, 60, 90, 120]),
                      min_size=0, max_size=6, unique=True))
@settings(max_examples=12, deadline=None)
def test_any_loss_set_stays_bounded_and_reproducible(drops):
    """Under ANY downstream loss set the wire coast stays bounded (reseeds still land) and its digest
    is reproducible — loss destroys information but never determinism."""
    states = _states()
    a = rp.run_remote_client(states, source="wire", drop_emit_ticks=tuple(drops))
    b = rp.run_remote_client(states, source="wire", drop_emit_ticks=tuple(drops))
    assert a["digest"] == b["digest"]
    assert a["max_pos_err"] < 5e-2     # even with gaps, reseeds keep it bounded (generous margin)
