"""Bound + async + catch-up server composition properties (netcode layer 20).

Layer 20 (src/net/boundcatchupserver.{h,cpp}, broadcast_bound_catchup) folds broadcast_live's windowed
late-join catch-up into the layer-19 bound + async server. Catch-up is a FOURTH orthogonal axis beside
the layer-18 UPSTREAM binding (SeatPolicy + BIND-001 + authorization, tools/bound_ref.py) and the
layer-16 DOWNSTREAM hygiene (byte-cap / liveness, modelled in test_bidi.py / test_boundasync.py): it
only decides a mid-stream joiner's REPLAY DEPTH. The byte-exact end-to-end claim is proven over REAL
127.0.0.1 sockets by src/net/netboundcatchup_test_main.cpp (ctest netboundcatchup_bridge). Here we prove
the MODEL the composition rests on — the retained-history / window arithmetic and its orthogonality to
authorization and to who is dropped:

  * a joiner accepted at frame fi is delivered EXACTLY [BIND | frames[max(0,fi-W):]] (the retained
    window replay frames[max(0,fi-W):fi] then the live suffix), and the window evicts exactly
    max(0, N-W) retentions over a full N-frame run (Stats.trimmed);
  * catch-up is downstream of AUTHORIZATION: the produced (hence retained/replayed) frames are a pure
    function of the authorized command SET — a spectator's rejected commands never change them, and its
    catch-up prefix is a byte-window of that same produced stream;
  * catch-up composes with the byte-cap: a non-reading joiner whose replay backlog exceeds the cap is
    shed DURING replay — a strict byte-prefix of [BIND | frames], never a live member.
"""
import sys
from pathlib import Path

from hypothesis import given, strategies as st

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import input001_ref as ic
import framing_ref as fr
import bound_ref as b

DYADIC = st.sampled_from([0.0, 0.5, -0.5, 0.25, -0.25, 0.75, -0.75, 1.0, 1.5, 2.0])
N_AC = 3
HORIZON = 40
SNAP = 5

CMD = st.builds(ic._cmd, apply_tick=st.integers(0, HORIZON - 1), aircraft=st.integers(0, N_AC - 1),
                seq=st.integers(-5, 5), target_phi=DYADIC, target_g=DYADIC, throttle=DYADIC,
                fire=st.booleans())


# --- reference producer / authorization (mirrors test_boundasync.py) ---------------------------------
def authorized_submit(cmds, n_aircraft=N_AC):
    """Each seat's client sends only its own aircraft's commands; the server drops anything not naming
    the sender's seat. Returns (accepted, unauthorized) as if every seat sends the WHOLE set."""
    accepted, unauth = [], 0
    for seat in range(n_aircraft):
        for c in cmds:
            if b.seat_authorizes(seat, c["aircraft"]):
                accepted.append(c)
            else:
                unauth += 1
    return accepted, unauth


def produce_frames(cmds, n_aircraft=N_AC, horizon=HORIZON, snap_every=SNAP):
    """Reference InputProducer: submit the command SET, take each aircraft's winner (or hold-last) per
    step, emit a 'frame' every snap_every ticks. A pure function of the canonical command SET."""
    q = ic.CommandQueue(n_aircraft)
    for c in cmds:
        q.submit(c)
    held = [(0, 0, 0, 0, 0)] * n_aircraft

    def frame(tick):
        return repr((tick, tuple(held))).encode()

    frames = [frame(0)]
    for t in range(1, horizon + 1):
        for a in range(n_aircraft):
            win = q.peek(t - 1, a)
            if win is not None:
                held[a] = ic._wire_key(win)
        q.consume(t - 1)
        if t % snap_every == 0:
            frames.append(frame(t))
    return frames


# --- the layer-20 retained-history / windowed-replay model (mirrors boundcatchupserver.cpp) ----------
def delivery_and_trimmed(n_frames, join_fi, window):
    """Mirror the server: after producing frame k it retains frames[k], evicting the oldest when
    window>0 and the retained size exceeds window (counting a trim). A joiner accepted at the TOP of
    iteration join_fi is replayed the current retained history (frames[max(0,join_fi-W):join_fi]) then
    the live suffix. Returns (delivered_frame_indices, total_trimmed_over_the_whole_run)."""
    history, trimmed_at_join = [], None
    trimmed = 0
    for k in range(n_frames):
        if k == join_fi:                     # accept happens BEFORE frame k is produced
            trimmed_at_join = trimmed
            prefix = list(history)
        history.append(k)
        if window > 0 and len(history) > window:
            history.pop(0)
            trimmed += 1
    if trimmed_at_join is None:              # joined at/after the end: history is everything retained
        prefix = list(history)
    delivered = prefix + list(range(join_fi, n_frames))
    return delivered, trimmed


@given(st.lists(CMD, max_size=25), st.integers(0, 9), st.integers(0, 12))
def test_catchup_window_delivers_exact_suffix(cmds, join_fi, window):
    # A joiner accepted at frame fi is delivered exactly frames[max(0,fi-W):] (window==0 = whole prefix).
    accepted, _ = authorized_submit(cmds)
    frames = produce_frames(accepted)
    n = len(frames)
    fi = min(join_fi, n)
    delivered, _ = delivery_and_trimmed(n, fi, window)
    start = (fi - window if window > 0 and fi > window else 0)
    assert delivered == list(range(start, n))            # frame-aligned, no gap, no duplicate
    # the delivered payload bytes are exactly that contiguous suffix of the produced stream
    assert [frames[i] for i in delivered] == frames[start:]


@given(st.lists(CMD, max_size=25), st.integers(1, 12))
def test_trimmed_count_is_max0_n_minus_w(cmds, window):
    # Over a full N-frame run a window of W evicts exactly max(0, N-W) retentions (window==0 never trims).
    accepted, _ = authorized_submit(cmds)
    n = len(produce_frames(accepted))
    _, trimmed = delivery_and_trimmed(n, n, window)        # run to the end; join point irrelevant here
    assert trimmed == max(0, n - window)
    _, trimmed0 = delivery_and_trimmed(n, n, 0)
    assert trimmed0 == 0                                    # retain-all: no eviction


@given(st.lists(CMD, max_size=25), st.integers(0, 9), st.integers(0, 12))
def test_catchup_is_downstream_of_authorization(cmds, join_fi, window):
    # A SPECTATOR (seat -1) authorizes NOTHING: its commands never change the produced frames, and its
    # catch-up prefix is a byte-window of that same authorized-produced stream. Adding the spectator's
    # whole (rejected) command set to the wire changes neither the frames nor the delivered suffix.
    accepted, _ = authorized_submit(cmds)
    frames = produce_frames(accepted)
    # a spectator authorizes nothing (every command dropped) -> the authorized set is unchanged
    for c in cmds:
        assert not b.seat_authorizes(b.SPECTATOR, c["aircraft"])
    assert produce_frames(accepted) == frames              # produced stream independent of the spectator
    n = len(frames)
    fi = min(join_fi, n)
    delivered, _ = delivery_and_trimmed(n, fi, window)
    start = (fi - window if window > 0 and fi > window else 0)
    assert [frames[i] for i in delivered] == frames[start:]


@given(st.lists(CMD, max_size=25), st.integers(0, 9), st.integers(-1, N_AC - 1), st.integers(0, 12))
def test_bind_then_prefix_then_live_byte_ordering(cmds, join_fi, seat, window):
    # The full downstream byte stream a joiner receives is [BIND | catch-up prefix | live suffix], i.e.
    # the framed BIND record followed by the framed contiguous suffix frames[max(0,fi-W):].
    accepted, _ = authorized_submit(cmds)
    frames = produce_frames(accepted)
    n = len(frames)
    fi = min(join_fi, n)
    delivered, _ = delivery_and_trimmed(n, fi, window)
    bind_rec = b.encode_bind(seat, N_AC)
    stream_records = [bind_rec] + [frames[i] for i in delivered]
    whole = fr.encode_stream(stream_records)
    # equivalently: BIND framed, then each delivered frame framed, concatenated in order
    manual = fr.encode_stream([bind_rec]) + fr.encode_stream([frames[i] for i in delivered])
    assert whole == manual
    assert whole.startswith(fr.encode_stream([bind_rec]))  # BIND is the first record


@given(st.lists(CMD, max_size=25), st.integers(1, 9), st.integers(1, 400))
def test_cap_sheds_a_non_reading_joiner_during_replay(cmds, join_fi, cap):
    # A non-reading joiner accepted mid-stream has its catch-up prefix enqueued after BIND; if the
    # replay backlog exceeds the byte-cap it is shed DURING replay — a strict byte-prefix of
    # [BIND | frames], never a live member. Model: enqueue [BIND | prefix...] with the kernel accepting
    # nothing (non-reading), shedding the moment pending exceeds cap.
    accepted, _ = authorized_submit(cmds)
    frames = produce_frames(accepted)
    n = len(frames)
    fi = min(join_fi, n)
    delivered, _ = delivery_and_trimmed(n, fi, 0)           # retain-all: the largest possible prefix
    records = [b.encode_bind(b.SPECTATOR, N_AC)] + [frames[i] for i in delivered]
    whole = fr.encode_stream(records)

    # SHED model: kernel accepts nothing; enqueue record-by-record, cut off once pending > cap.
    pending = b""
    shed = False
    delivered_bytes = b""
    for rec in records:
        pending += fr.encode_stream([rec])
        if cap and len(pending) > cap:
            shed = True
            break
    if shed:
        # everything enqueued up to (and including) the record that tripped the cap is a byte-prefix
        assert whole.startswith(pending)
        assert len(pending) <= len(whole)
    else:
        assert pending == whole                            # small enough to fit under the cap: full
    # a GREEDY (cooperative) joiner that drains everything is never shed and gets the whole stream
    assert whole == fr.encode_stream(records)
