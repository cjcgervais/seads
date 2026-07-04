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


def _smooth_states():
    _hashes, states = rp.pred.run_truth(rp.SMOOTH_SK)
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


# --- LAYER 25: reconcile smoothing (hide the maneuver-correction pop) on SMOOTH-SK-001 ------------

def test_smoothing_shrinks_the_pop():
    """The smoothed display's worst single-tick state jump is a fraction of the hard snap's (the
    headline): blending spreads the correction over ~1/smooth ticks instead of landing it in one."""
    st_ = _smooth_states()
    snap = rp.run_remote_client_smoothed(st_, rp.SMOOTH_SK, source="canonical", smooth=1.0)
    smooth = rp.run_remote_client_smoothed(st_, rp.SMOOTH_SK, source="canonical", smooth=0.25)
    assert snap["max_jump"] > 0.0
    assert smooth["max_jump"] < snap["max_jump"]     # smoothing genuinely hides the pop
    assert snap["max_jump"] / smooth["max_jump"] > 2.0   # by a wide margin (~1/smooth = 4x)


def test_smooth_one_is_exactly_the_coast():
    """The degenerate identity: smooth=1 is the hard snap ⇒ its digest equals the layer-24 coast
    bit-for-bit (the coast target is byte-identical; only the render blend differs for smooth<1)."""
    st_ = _smooth_states()
    coast = rp.run_remote_client(st_, rp.SMOOTH_SK, source="canonical")
    snap = rp.run_remote_client_smoothed(st_, rp.SMOOTH_SK, source="canonical", smooth=1.0)
    assert snap["digest"] == coast["digest"]


def test_smoothed_digests_pinned_and_reproducible():
    st_ = _smooth_states()
    canon = rp.run_remote_client_smoothed(st_, rp.SMOOTH_SK, source="canonical")
    wire = rp.run_remote_client_smoothed(st_, rp.SMOOTH_SK, source="wire")
    assert canon["digest"] == rp.PIN_SMOOTH_CANON_DIGEST
    assert wire["digest"] == rp.PIN_SMOOTH_WIRE_DIGEST
    assert rp.PIN_SMOOTH_CANON_DIGEST != rp.PIN_SMOOTH_WIRE_DIGEST
    # reproducible
    assert rp.run_remote_client_smoothed(st_, rp.SMOOTH_SK, source="canonical")["digest"] == canon["digest"]


def test_smoothing_trades_accuracy_for_smoothness():
    """The honest bound: smoothing HIDES the pop but LAGS the truth during the transient — the
    smoothed now-error is >= the hard snap's, yet still bounded (smoothness bought with lag)."""
    st_ = _smooth_states()
    coast = rp.run_remote_client(st_, rp.SMOOTH_SK, source="canonical")
    smooth = rp.run_remote_client_smoothed(st_, rp.SMOOTH_SK, source="canonical", smooth=0.25)
    assert smooth["max_pos_err"] >= coast["max_pos_err"]   # laggier ...
    assert smooth["max_pos_err"] < 1e-1                    # ... but bounded


def test_smoothing_is_monotone_in_the_factor():
    """More smoothing (smaller factor) ⇒ a smaller worst pop but a larger transient lag — the two
    move oppositely, exactly the accuracy/smoothness trade the factor dials."""
    st_ = _smooth_states()
    factors = [1.0, 0.5, 0.25, 0.125]
    runs = [rp.run_remote_client_smoothed(st_, rp.SMOOTH_SK, source="canonical", smooth=s)
            for s in factors]
    jumps = [r["max_jump"] for r in runs]
    lags = [r["max_pos_err"] for r in runs]
    # smaller factor -> smaller pop (jumps strictly decrease as smoothing increases)
    assert all(jumps[i] > jumps[i + 1] for i in range(len(jumps) - 1))
    # smaller factor -> larger lag (now-error strictly increases)
    assert all(lags[i] < lags[i + 1] for i in range(len(lags) - 1))


@given(smooth=st.sampled_from([1.0, 0.5, 0.25, 0.125, 0.0625]),
       drops=st.lists(st.sampled_from([20, 40, 60, 80, 100, 120]), min_size=0, max_size=4, unique=True))
@settings(max_examples=16, deadline=None)
def test_any_factor_and_loss_stays_bounded_and_reproducible(smooth, drops):
    """For ANY smoothing factor and ANY loss set the smoothed display is bounded and its digest is
    reproducible — smoothing never breaks determinism (pure IEEE blend) or the bound (reseeds land)."""
    st_ = _smooth_states()
    a = rp.run_remote_client_smoothed(st_, rp.SMOOTH_SK, source="wire", smooth=smooth,
                                      drop_emit_ticks=tuple(drops))
    b = rp.run_remote_client_smoothed(st_, rp.SMOOTH_SK, source="wire", smooth=smooth,
                                      drop_emit_ticks=tuple(drops))
    assert a["digest"] == b["digest"]
    assert a["max_pos_err"] < 1e-1
