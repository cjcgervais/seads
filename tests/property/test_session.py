"""Metamorphic properties for the server<->client SESSION loop (netcode layer 5).

Bit-exact C++<->reference parity (the reconstructed dogfight reproduces to the byte) is proven by
the generated-vector gate (src/net/session_test_main.cpp). Here we prove the reference's session
logic holds as a system: the reconstruction is deterministic; the gun KILL replicates over the
lossy wire and the client's HP mirrors the server's authoritative HP exactly (hp is integer-valued,
so the wire carries it losslessly); the own-ship wire reconcile HEALS an injected state desync
(and the desync persists without it); own-ship accuracy stays bounded across transport latencies;
and the kill still replicates under additional packet loss. These are the transport-level analogues
of test_predict / test_interp.
"""
import copy
import sys
from pathlib import Path

from hypothesis import given, settings, strategies as st

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import session_ref as sr
import geo001_ref as g
import snapshot_ref as snap

TICKS = int(sr.SESSION["ticks"])


def _with(**overrides):
    """A deep copy of SESSION-SK-001 with scalar overrides (lag, render_delay, drops...)."""
    sc = copy.deepcopy(sr.SESSION)
    sc.update(overrides)
    return sc


def test_session_is_deterministic():
    a = sr.run_session(sr.SESSION, reconcile=True)["digest"]
    b = sr.run_session(sr.SESSION, reconcile=True)["digest"]
    assert a == b


def test_kill_replicates_over_the_wire():
    # the client reconstructs the combat outcome from bytes: A6M2 (id 1) dead, P-47 (0, own) and
    # Spitfire (2) alive — and it agrees with the server's authoritative final state.
    res = sr.run_session(sr.SESSION, reconcile=True)
    wf = res["final_wframe"]
    by_id = {int(e.id): e for e in wf.entities}
    assert by_id[1].hp <= 0.0            # target killed
    assert by_id[0].hp > 0.0             # own ship survives
    assert by_id[2].hp > 0.0             # bystander survives
    srv = res["server_states"][TICKS]     # (…, hp@7, …) per aircraft
    assert srv[1][7] <= 0.0 and srv[0][7] > 0.0 and srv[2][7] > 0.0


def test_client_hp_mirrors_server_exactly():
    # weapon-wire fidelity: the client's reconstructed HP equals the server's authoritative HP to
    # the quantum. hp is integer-valued in the kernel, so the 1e3-scale wire carries it LOSSLESSLY
    # (the quantized values are bit-equal) — the kill/HP bars a remote client draws are exact.
    res = sr.run_session(sr.SESSION, reconcile=True)
    by_id = {int(e.id): e for e in res["final_wframe"].entities}
    srv = res["server_states"][TICKS]
    for i in range(len(sr.SESSION["aircraft"])):
        client_q = g.quantize(by_id[i].hp, snap.HP_SCALE)
        server_q = g.quantize(srv[i][7], snap.HP_SCALE)
        assert client_q == server_q


@given(dalt=st.floats(min_value=-40.0, max_value=40.0, allow_nan=False, allow_infinity=False)
       .filter(lambda x: abs(x) > 0.5))
@settings(max_examples=25, deadline=None)
def test_reconcile_heals_own_state_desync(dalt):
    # inject an initial-altitude desync into the client's OWN predictor. With the wire reconcile on,
    # the own ship is snapped back toward the authoritative state each frame (error ~ quantum);
    # without it the desync persists. Metamorphic: reconcile strictly and substantially reduces the
    # own-ship tracking error.
    frames, server_states = sr.run_server(sr.SESSION)
    # measure the STEADY STATE after the first reconcile lands (warmup past lag+snap_every); the
    # pre-heal ticks carry the injected desync in BOTH cases, so a global max wouldn't distinguish.
    warmup = int(sr.SESSION["lag_ticks"]) + int(sr.SESSION["snap_every"])
    healed = sr.own_track_error(sr.SESSION, frames, server_states,
                                perturb_alt=dalt, reconcile=True, warmup=warmup)
    broken = sr.own_track_error(sr.SESSION, frames, server_states,
                                perturb_alt=dalt, reconcile=False, warmup=warmup)
    # position error in METRES. Level flight decouples altitude from horizontal motion (constant
    # density), so an alt desync persists open-loop at ~|dalt| and reconcile pins it to the alt
    # quantum (1e-3 m). Metamorphic: reconcile heals it and is far better than flying it open-loop.
    assert healed < 1e-2          # reconcile pins the own ship to ~the wire quantum
    assert healed < broken        # and it is strictly better than flying the desync open-loop
    assert broken >= abs(dalt) - 1e-6   # the injected desync persists (level flight holds the offset)


@given(lag=st.integers(min_value=1, max_value=30))
@settings(max_examples=20, deadline=None)
def test_own_accuracy_bounded_across_latency(lag):
    # across transport latencies the reconciled own ship stays accurate: reconcile corrects against
    # the freshest wire frame, so the open-loop drift between corrections (bounded by ~lag ticks)
    # never accumulates. render_delay tracks lag so remotes still bracket (not used by own error).
    sc = _with(lag_ticks=lag, render_delay=lag + int(sr.SESSION["snap_every"]))
    err = sr.own_track_error(sc, perturb_alt=0.0, reconcile=True)
    assert err < 1e-2             # metres — bounded regardless of latency (prediction is seamless,
    #                               reconcile just re-quantizes, so there is no inter-frame drift)


@given(extra=st.lists(st.sampled_from([15, 25, 35, 45, 65, 75, 95, 115, 135, 165, 185]),
                      min_size=0, max_size=6, unique=True))
@settings(max_examples=25, deadline=None)
def test_kill_replicates_under_extra_packet_loss(extra):
    # drop additional frames on top of the baseline loss set. Once the A6M2 is dead it stays dead in
    # every later frame, so as long as ANY post-kill frame survives (we never drop them all) the
    # client still reconstructs the kill — and the reconstruction stays deterministic.
    drops = sorted(set(sr.SESSION["drop_emit_ticks"]) | set(extra))
    sc = _with(drop_emit_ticks=drops)
    a = sr.run_session(sc, reconcile=True)
    b = sr.run_session(sc, reconcile=True)
    assert a["digest"] == b["digest"]                       # deterministic under loss
    by_id = {int(e.id): e for e in a["final_wframe"].entities}
    assert by_id[1].hp <= 0.0                               # kill still replicated
    assert by_id[0].hp > 0.0 and by_id[2].hp > 0.0


@given(extra=st.lists(st.sampled_from([15, 25, 35, 45, 65, 75, 95, 115, 135, 165, 185]),
                      min_size=0, max_size=6, unique=True))
@settings(max_examples=25, deadline=None)
def test_region_damage_and_scoreboard_replicate_under_loss(extra):
    # v1.19r0: the region pools + kill tally ride the wire, and — like the kill itself — once
    # the astern burst has shot the A6M2's TAIL away and credited the P-47 the state persists in
    # every later frame, so under ANY loss pattern (we never drop them all) the client's freshest
    # frame reconstructs the DAMAGE STATE and the SCOREBOARD exactly: AC1 tail gone (engine/wing
    # intact), AC0 kills=1, bystanders full pools + 0 kills.
    drops = sorted(set(sr.SESSION["drop_emit_ticks"]) | set(extra))
    sc = _with(drop_emit_ticks=drops)
    a = sr.run_session(sc, reconcile=True)
    by_id = {int(e.id): e for e in a["final_wframe"].entities}
    assert by_id[1].tail_hp <= 0.0                          # astern kill: tail pool shot away
    assert by_id[1].engine_hp > 0.0 and by_id[1].wing_hp > 0.0
    assert int(round(by_id[0].kills)) == 1                  # scoreboard: P-47 credited the kill
    assert int(round(by_id[1].kills)) == 0 and int(round(by_id[2].kills)) == 0
    assert by_id[0].tail_hp > 0.0 and by_id[2].tail_hp > 0.0  # never-hit ships keep full pools
