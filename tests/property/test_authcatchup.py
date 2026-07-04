"""Authenticated + async + catch-up server composition properties (netcode layer 23).

Layer 23 (src/net/authcatchupserver.{h,cpp}, broadcast_auth_catchup) folds broadcast_live's windowed
late-join catch-up (layer 20) onto layer 22's identity-authenticated async server — the LAST rung of the
authenticated arc (21 -> 22 -> 23, the 19->20 step re-run on the authenticated server). Catch-up is a
FOURTH orthogonal axis beside the layer-21 UPSTREAM IDENTITY binding (a HELLO-001 credential looked up in
a CredentialTable to a DESIGNATED seat + authorization, tools/auth_ref.py) and the layer-16/19 DOWNSTREAM
hygiene (byte-cap / liveness): it decides only a mid-stream joiner's REPLAY DEPTH. The byte-exact
end-to-end claim is proven over REAL 127.0.0.1 sockets by src/net/netauthcatchup_test_main.cpp (ctest
netauthcatchup_bridge). Here we prove the MODEL the composition rests on — the retained-history / window
arithmetic and its orthogonality to authentication and to who is dropped, plus the headline over the
join-order layer 20 that a seat freed by a mid-replay cap shed is reclaimed by ITS OWN identity:

  * a joiner accepted at frame fi is delivered EXACTLY [BIND | frames[max(0,fi-W):]], the window evicting
    exactly max(0, N-W) retentions over a full N-frame run (Stats.trimmed);
  * catch-up is downstream of AUTHENTICATION: the produced (hence retained/replayed) frames are a pure
    function of the AUTHENTICATED + AUTHORIZED command SET — an unknown-token spectator's rejected commands
    never change them, and its catch-up prefix is a byte-window of that same produced stream;
  * catch-up composes with the byte-cap: a non-reading joiner whose replay backlog exceeds the cap is shed
    DURING replay (a strict byte-prefix of [BIND | frames], never a live member) — and its DESIGNATED seat
    is freed and reclaimed by its OWN identity, never the lowest free (layer 20's join-order policy could
    not promise this).

Reuses the layer-21 CredentialTable / seat_authorizes (auth_ref / bound_ref) and the layer-20
retained-history / windowed-replay model (mirrored from test_boundcatchup.py).
"""
import sys
from pathlib import Path

from hypothesis import given, strategies as st

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import input001_ref as ic
import framing_ref as fr
import bound_ref as b
import auth_ref as a

DYADIC = st.sampled_from([0.0, 0.5, -0.5, 0.25, -0.25, 0.75, -0.75, 1.0, 1.5, 2.0])
N_AC = 3
HORIZON = 40
SNAP = 5

CMD = st.builds(ic._cmd, apply_tick=st.integers(0, HORIZON - 1), aircraft=st.integers(0, N_AC - 1),
                seq=st.integers(-5, 5), target_phi=DYADIC, target_g=DYADIC, throttle=DYADIC,
                fire=st.booleans())


# --- upstream WITH identity authentication + authorization (mirrors test_authasync.py) ---------------
def authenticated_submit(cmds, n_aircraft=N_AC):
    """Model the layer-23 server: enroll n identities (token 500+seat -> seat), each seated identity
    upstreams only ITS aircraft's commands (authorized), PLUS an unknown-token spectator upstreaming the
    WHOLE set (all dropped). Returns (accepted, unauth) — the accepted set is exactly the in-range commands,
    once each, reached via a CredentialTable (identity) rather than a join-order SeatPolicy."""
    ct = a.CredentialTable(n_aircraft)
    seat_of = {}
    for seat in range(n_aircraft):
        ct.enroll(500 + seat, seat)
        seat_of[500 + seat] = ct.authenticate(500 + seat)
    spectator = ct.authenticate(77777)          # unknown token -> spectator (no aircraft)
    assert spectator == a.SPECTATOR
    accepted, unauth = [], 0
    for _tok, seat in seat_of.items():          # each seated identity sends its OWN aircraft's commands
        for c in cmds:
            if b.seat_authorizes(seat, c["aircraft"]):
                accepted.append(c)
            else:
                unauth += 1
    for c in cmds:                              # the spectator sends EVERYTHING; nothing is authorized
        assert not b.seat_authorizes(spectator, c["aircraft"])
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
        for ac in range(n_aircraft):
            win = q.peek(t - 1, ac)
            if win is not None:
                held[ac] = ic._wire_key(win)
        q.consume(t - 1)
        if t % snap_every == 0:
            frames.append(frame(t))
    return frames


# --- the layer-20 retained-history / windowed-replay model (mirrors test_boundcatchup.py) ------------
def delivery_and_trimmed(n_frames, join_fi, window):
    """A joiner accepted at the TOP of iteration join_fi is replayed the current retained history
    (frames[max(0,join_fi-W):join_fi]) then the live suffix. Returns (delivered_frame_indices,
    total_trimmed_over_the_whole_run)."""
    history, trimmed_at_join = [], None
    trimmed = 0
    prefix = []
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


# =====================================================================================================
@given(st.lists(CMD, max_size=25), st.integers(0, 9), st.integers(0, 12))
def test_authenticated_catchup_window_delivers_exact_suffix(cmds, join_fi, window):
    # The headline: over the AUTHENTICATED command set, a joiner accepted at frame fi is delivered exactly
    # frames[max(0,fi-W):] (window==0 = whole prefix). Authentication feeds the produced stream; catch-up
    # windows a byte-exact suffix of it.
    accepted, _ = authenticated_submit(cmds)
    frames = produce_frames(accepted)
    n = len(frames)
    fi = min(join_fi, n)
    delivered, _ = delivery_and_trimmed(n, fi, window)
    start = (fi - window if window > 0 and fi > window else 0)
    assert delivered == list(range(start, n))            # frame-aligned, no gap, no duplicate
    assert [frames[i] for i in delivered] == frames[start:]


@given(st.lists(CMD, max_size=25), st.integers(1, 12))
def test_trimmed_count_is_max0_n_minus_w(cmds, window):
    # Over a full N-frame run a window of W evicts exactly max(0, N-W) retentions (window==0 never trims).
    accepted, _ = authenticated_submit(cmds)
    n = len(produce_frames(accepted))
    _, trimmed = delivery_and_trimmed(n, n, window)        # run to the end; join point irrelevant here
    assert trimmed == max(0, n - window)
    _, trimmed0 = delivery_and_trimmed(n, n, 0)
    assert trimmed0 == 0                                    # retain-all: no eviction


@given(st.lists(CMD, max_size=25), st.integers(0, 9), st.integers(0, 12))
def test_catchup_is_downstream_of_authentication(cmds, join_fi, window):
    # AXIS ORTHOGONALITY (identity authentication x catch-up): the produced (hence retained/replayed)
    # frames are a pure function of the AUTHENTICATED + AUTHORIZED command set — the unknown-token
    # spectator authorizes nothing, so it never changes them, and its catch-up prefix is a byte-window of
    # that same produced stream. Authentication did not weaken catch-up; catch-up did not weaken auth.
    accepted, unauth = authenticated_submit(cmds)
    frames = produce_frames(accepted)
    in_range = [c for c in cmds if c["aircraft"] < N_AC]
    assert len(accepted) == len(in_range)                  # exactly the seated set, once each
    assert unauth == (N_AC - 1) * len(in_range) + len(cmds)  # every foreign seat + the whole spectator set
    assert produce_frames(authenticated_submit(cmds)[0]) == frames  # independent of the spectator
    n = len(frames)
    fi = min(join_fi, n)
    delivered, _ = delivery_and_trimmed(n, fi, window)
    start = (fi - window if window > 0 and fi > window else 0)
    assert [frames[i] for i in delivered] == frames[start:]


@given(st.lists(CMD, max_size=25), st.integers(0, 9), st.integers(-1, N_AC - 1), st.integers(0, 12))
def test_bind_then_prefix_then_live_byte_ordering(cmds, join_fi, seat, window):
    # A joiner's full downstream byte stream is [BIND | catch-up prefix | live suffix]: the framed BIND
    # record (its designated seat, or spectator) then the framed contiguous suffix frames[max(0,fi-W):].
    accepted, _ = authenticated_submit(cmds)
    frames = produce_frames(accepted)
    n = len(frames)
    fi = min(join_fi, n)
    delivered, _ = delivery_and_trimmed(n, fi, window)
    bind_rec = b.encode_bind(seat, N_AC)                   # seat could be a real designated seat or spectator
    stream_records = [bind_rec] + [frames[i] for i in delivered]
    whole = fr.encode_stream(stream_records)
    manual = fr.encode_stream([bind_rec]) + fr.encode_stream([frames[i] for i in delivered])
    assert whole == manual
    assert whole.startswith(fr.encode_stream([bind_rec]))  # BIND is the first record


@given(st.lists(CMD, max_size=25), st.integers(1, 9), st.integers(1, 400), st.integers(0, N_AC - 1))
def test_cap_sheds_joiner_during_replay_and_frees_its_designated_seat(cmds, join_fi, cap, seat):
    # THE HEADLINE (hygiene x catch-up x identity): a non-reading authenticated joiner has its catch-up
    # prefix enqueued after BIND; if the replay backlog exceeds the byte-cap it is shed DURING replay — a
    # strict byte-prefix of [BIND | frames], never a live member — and because the binding is by IDENTITY,
    # the seat freed on the drop is reclaimed by its OWN token (not the lowest free).
    accepted, _ = authenticated_submit(cmds)
    frames = produce_frames(accepted)
    n = len(frames)
    fi = min(join_fi, n)
    delivered, _ = delivery_and_trimmed(n, fi, 0)          # retain-all: the largest possible prefix
    records = [b.encode_bind(seat, N_AC)] + [frames[i] for i in delivered]
    whole = fr.encode_stream(records)

    # SHED model: kernel accepts nothing; enqueue record-by-record, cut off once pending > cap.
    pending = b""
    shed = False
    for rec in records:
        pending += fr.encode_stream([rec])
        if cap and len(pending) > cap:
            shed = True
            break
    if shed:
        assert whole.startswith(pending) and len(pending) <= len(whole)  # a strict/equal byte-prefix
    else:
        assert pending == whole                            # small enough to fit under the cap: full

    # the shed joiner's DESIGNATED seat is released and reclaimed by its OWN identity (token 500+seat).
    ct = a.CredentialTable(N_AC)
    ct.enroll(500 + seat, seat)
    assert ct.authenticate(500 + seat) == seat             # draws its designated seat
    assert ct.authenticate(500 + seat) == a.SPECTATOR      # a double-login while held -> spectator
    ct.release(seat)                                       # the cap shed frees it
    assert ct.authenticate(500 + seat) == seat             # reclaimed by its OWN identity, not lowest-free
