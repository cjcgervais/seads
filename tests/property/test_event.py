"""Metamorphic properties for the reliable combat-EVENT channel (netcode layer 6).

Bit-exact C++<->reference parity (the reconstructed event log reproduces to the byte, incl. the
K-consecutive-blackout bound) is proven by the generated-vector gate (src/net/event_test_main.cpp).
Here we prove the reference's redundant-journal logic holds as a SYSTEM across loss patterns:

  * determinism — the reconstructed-log digest is reproducible;
  * full reconstruction under the scenario's isolated single drops (redundancy delivers everything);
  * redundancy tolerates ANY single dropped frame (K=4 => >= 1 carrier always survives);
  * SOUNDNESS across arbitrary loss — the applied log is always a strictly-seq-increasing
    SUBSEQUENCE of the server log (dedup works, no event is invented, no head-of-line stall);
  * the KILL is reliably delivered under heavy NON-consecutive loss (it rides every later frame);
  * the RELIABILITY BOUND — a K-consecutive blackout of an event's whole window-lifetime permanently
    loses exactly that event, and the journal RESYNCS (later events still land).

These are the transient-event analogue of test_session's state-replication properties.
"""
import sys
from pathlib import Path

from hypothesis import given, settings, strategies as st

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import event_ref as ev
import session_ref as sr

SCENARIO = sr.SESSION
# emitted event-window ticks: tick 0 + every snap_every up to ticks (what the transport can drop)
_EMIT_TICKS = sorted(ev.run_event_server(SCENARIO)[1].keys())
_SERVER = ev.run_event_server(SCENARIO)[0]
_SERVER_SEQS = [e.seq for e in _SERVER]


def _seqs(applied):
    return [e.seq for e in applied]


def test_event_reconstruction_is_deterministic():
    a = ev.run_events(SCENARIO)["applied"]
    b = ev.run_events(SCENARIO)["applied"]
    assert ev.event_digest(a) == ev.event_digest(b)


def test_full_reconstruction_under_standard_drops():
    # the redundant journal delivers EVERY event even though the transport drops 5 frames: the
    # client's applied log equals the server's authoritative log exactly.
    res = ev.run_events(SCENARIO)
    assert _seqs(res["applied"]) == _SERVER_SEQS
    assert ev.event_digest(res["applied"]) == ev.event_digest(_SERVER)


def test_server_stream_is_nonvacuous_and_ends_in_a_kill():
    # the P-47's burst must actually produce a multi-event sequence culminating in the A6M2's death,
    # else the reliability claims are vacuous.
    assert len(_SERVER) >= 2
    kill = ev.kill_event(_SERVER)
    assert kill is not None and kill.target == 1 and kill.hp_after_milli == 0
    assert sum(e.killed for e in _SERVER) == 1     # exactly one kill


@given(drop=st.sampled_from(_EMIT_TICKS))
@settings(max_examples=len(_EMIT_TICKS), deadline=None)
def test_any_single_dropped_frame_still_fully_reconstructs(drop):
    # redundancy: with K=4 every event rides >= 2 emitted frames here, so losing any ONE frame still
    # leaves a carrier — the client recovers the full log regardless of WHICH frame is dropped.
    applied = ev.run_events(SCENARIO, drop_emit_ticks=[drop])["applied"]
    assert _seqs(applied) == _SERVER_SEQS


@given(drops=st.lists(st.sampled_from(_EMIT_TICKS), min_size=0, max_size=12, unique=True))
@settings(max_examples=60, deadline=None)
def test_applied_is_always_a_sound_subsequence(drops):
    # SOUNDNESS for ANY loss pattern: the applied log is a strictly-seq-increasing SUBSEQUENCE of the
    # server log. Strictly increasing => dedup works + no head-of-line stall; subsequence => the
    # client never INVENTS an event and every field it reports matches the server's authoritative one.
    applied = ev.run_events(SCENARIO, drop_emit_ticks=drops)["applied"]
    seqs = _seqs(applied)
    assert all(seqs[i] < seqs[i + 1] for i in range(len(seqs) - 1))   # strictly increasing, de-duped
    assert set(seqs).issubset(set(_SERVER_SEQS))                      # never invents an event
    by_seq = {e.seq: e for e in _SERVER}
    for e in applied:
        assert e.as_tuple() == by_seq[e.seq].as_tuple()              # fields match the server exactly


@given(extra=st.lists(st.sampled_from([t for t in _EMIT_TICKS if t >= 55]),
                      min_size=0, max_size=8, unique=True))
@settings(max_examples=30, deadline=None)
def test_kill_reliably_delivered_under_nonconsecutive_loss(extra):
    # the kill event (created once firing has walked hp to 0) rides EVERY subsequent frame — no newer
    # event ever pushes it out. So as long as we don't black out a full K-run of consecutive frames
    # (these drops are a sparse subset), the kill is always reconstructed with its exact tick + hp.
    # Keep the extra drops non-adjacent to avoid a >=K consecutive blackout of the kill's carriers.
    extra = [t for i, t in enumerate(sorted(extra)) if i == 0 or t - sorted(extra)[i - 1] > 5]
    drops = sorted(set(SCENARIO["drop_emit_ticks"]) | set(extra))
    applied = ev.run_events(SCENARIO, drop_emit_ticks=drops)["applied"]
    kill = ev.kill_event(applied)
    srv_kill = ev.kill_event(_SERVER)
    assert kill is not None and kill.as_tuple() == srv_kill.as_tuple()


@given(start=st.integers(min_value=0, max_value=6))
@settings(max_examples=7, deadline=None)
def test_k_consecutive_blackout_bound(start):
    # RELIABILITY BOUND on a controlled 10-event stream (one new event per frame => each ages out
    # after exactly K newer frames). A blackout of K consecutive emitted frames [b .. b+K-1] loses
    # EXACTLY the events whose entire in-window lifetime falls inside it, and the journal resyncs.
    # Emits at ticks 1..10 (snap_every=1, lag 0); event i created at tick i+1.
    events = [ev.Event(seq=i, tick=i + 1, target=1, damage_milli=1000,
                       hp_after_milli=(10 - i) * 1000, killed=1 if i == 9 else 0)
              for i in range(10)]
    emit_ticks = list(range(1, 11))
    windows = ev._make_windows_from_events(events, emit_ticks)
    K = ev.EVENT_WINDOW_K
    blackout = list(range(start + 1, start + 1 + K))    # K consecutive emitted frames

    sc = {"ticks": 10, "lag_ticks": 0, "drop_emit_ticks": []}
    applied, _ = ev.run_event_client(sc, windows, drop_emit_ticks=blackout)
    got = set(_seqs(applied))

    # an event seq s rides emits [s+1 .. s+K] (K frames, until K newer events push it out; the tail
    # events ride every later frame). It is lost iff ALL its carriers are in the blackout.
    def carriers(s):
        first = s + 1
        # it stays in-window until K newer events exist; newer event j created at tick j+1, so it is
        # in the last-K window of emit e while (#events with tick<=e that are newer than s) < K.
        last = min(10, s + K)                 # aged out once s+K exists (created at tick s+K+1)
        return set(range(first, last + 1))
    expect_lost = {s for s in range(10) if carriers(s).issubset(set(blackout))}
    expect_kept = set(range(10)) - expect_lost

    assert got == expect_kept
    assert got.isdisjoint(expect_lost) and len(expect_lost) >= 1   # the bound genuinely bites
    # resync: applied seqs are still strictly increasing (no stall on the lost gap)
    seqs = _seqs(applied)
    assert all(seqs[i] < seqs[i + 1] for i in range(len(seqs) - 1))
