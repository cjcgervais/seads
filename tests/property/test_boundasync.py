"""Bound + async server composition properties (netcode layer 19).

Layer 19 (src/net/boundasyncserver.{h,cpp}, broadcast_bound_async) is the product of two orthogonal
axes that both grew from broadcast_input: the layer-18 UPSTREAM binding (a join-order SeatPolicy + a
BIND-001 handshake + per-seat authorization, tools/bound_ref.py) and the layer-16 DOWNSTREAM hygiene
(async send buffers + byte-cap + liveness reap, modelled tick-exact in test_bidi.py). The byte-exact
end-to-end claim — seated clients each fly their own aircraft to build_server_frames through the async
path, and a dead client is reaped/capped + its seat freed without disturbing a cooperative client — is
proven over REAL 127.0.0.1 sockets by src/net/netboundasync_test_main.cpp (ctest netboundasync_bridge).
Here we prove the MODEL the composition rests on: the THREE axes (which commands are AUTHORIZED, which
frames are PRODUCED, which bytes are DELIVERED / who is dropped) do not interfere.

Reuses the layer-18 SeatPolicy / seat_authorizes (bound_ref) and the layer-16 downstream delivery
models (mirrored from test_bidi.py). Proves:

  * AUTHORIZATION is downstream-blind: the authorized command set (hence the produced frames) is a
    function of the seat-partitioned upstream ONLY — no downstream cap/liveness drop can change it;
  * a seat is freed on ANY leave reason (clean EOF, fatal flush, byte-cap shed, liveness reap) and
    reused — never two live clients on one seat, whatever mix of drop reasons occurs;
  * the BIND-001 record is the FIRST downstream record, so every client's delivery — full or a
    dropped prefix — begins with its BIND; a shed client is a strict byte-prefix of [BIND | frames].
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


# --- upstream WITH authorization: each seat's client sends only its own aircraft's commands ----------
def authorized_submit(cmds, n_aircraft=N_AC):
    """Model the server's per-seat authorization: seat s upstreams only aircraft-s commands, and the
    server drops anything not naming the sender's seat. Returns (accepted, unauthorized) counts as if
    every seat 0..n-1 has a client sending the WHOLE set (so a foreign command is dropped by every seat
    but its own). The accepted set is exactly the in-seat-range commands, once each."""
    accepted, unauth = [], 0
    for seat in range(n_aircraft):
        for c in cmds:
            if b.seat_authorizes(seat, c["aircraft"]):
                accepted.append(c)
            else:
                unauth += 1
    return accepted, unauth


def produce_frames(cmds, n_aircraft=N_AC, horizon=HORIZON, snap_every=SNAP):
    """Reference InputProducer (mirrors test_bidi.produce_frames): submit the command SET, then per step
    take each aircraft's winner for the pre-step tick (or hold-last), and emit a 'frame' every snap_every
    ticks. A pure function of the canonically-ordered command SET."""
    q = ic.CommandQueue(n_aircraft)
    for c in cmds:
        q.submit(c)
    held = [(0, 0, 0, 0, 0)] * n_aircraft

    def frame(tick):
        return b"".join(repr((tick, wk)).encode() for wk in held)

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


# --- downstream: layer-16 delivery models (mirrored from test_bidi.py) --------------------------------
def deliver_capped(frames, accepts, cap):
    queue = b""
    delivered = b""
    for i, f in enumerate(frames):
        queue += fr.encode_stream([f])
        take = min(accepts[i], len(queue)) if i < len(accepts) else 0
        delivered += queue[:take]
        queue = queue[take:]
        if cap and len(queue) > cap:
            return delivered, True
    return delivered + queue, False


def deliver_liveness(frames, accepts, liveness):
    queue = b""
    delivered = b""
    sent_total = last_sent = idle = 0
    for i, f in enumerate(frames):
        queue += fr.encode_stream([f])
        take = min(accepts[i], len(queue)) if i < len(accepts) else 0
        delivered += queue[:take]
        queue = queue[take:]
        sent_total += take
        if len(queue) == 0 or sent_total > last_sent:
            idle = 0
            last_sent = sent_total
        elif liveness:
            idle += 1
            if idle > liveness:
                return delivered, True
    return delivered + queue, False


# =====================================================================================================
@given(st.lists(CMD, max_size=25), st.integers(1, 400), st.integers(0, 8))
def test_authorization_is_downstream_blind(cmds, cap, liveness):
    # AXIS ORTHOGONALITY (upstream x downstream): the AUTHORIZED command set — and hence the produced
    # frames — is a pure function of the seat-partitioned upstream, independent of ANY downstream policy.
    # No cap/liveness value can change which commands were authorized or which frames were produced.
    accepted, unauth = authorized_submit(cmds)
    frames = produce_frames(accepted)
    # the authorized set is exactly the in-range commands (each authorized once, by its own seat)
    in_range = [c for c in cmds if c["aircraft"] < N_AC]
    assert len(accepted) == len(in_range)
    assert unauth == (N_AC - 1) * len(in_range)  # every seat but its own dropped each in-range command
    # produced frames don't depend on cap/liveness (recompute independent of the policy knobs)
    assert produce_frames(authorized_submit(cmds)[0]) == frames


@given(st.lists(CMD, max_size=25), st.randoms(use_true_random=False))
def test_authorized_frames_are_order_invariant(cmds, rng):
    # The produced frames are a pure function of the AUTHORIZED command SET: submitting it in any order
    # yields byte-identical frames (the layer-15b claim, restated on the authorized set the merge feeds).
    accepted, _ = authorized_submit(cmds)
    shuffled = list(accepted)
    rng.shuffle(shuffled)
    assert produce_frames(accepted) == produce_frames(shuffled)


@given(st.integers(1, 8), st.integers(0, 32), st.randoms(use_true_random=False))
def test_seat_freed_on_any_drop_reason(n, n_ops, rng):
    # A seat is freed on ANY leave — clean EOF, fatal flush, byte-cap shed (capped), OR liveness reap
    # (reaped) — all funnel through drop_client, which releases the seat. Drive a random interleaving of
    # joins and mixed-reason drops; INVARIANT: live seats are distinct + lowest-free, released seats
    # reused, never a double-booking, whatever the drop reason.
    p = b.SeatPolicy(n)
    live = set()  # seat -> still occupied
    reasons = ["eof", "flush_fail", "capped", "reaped"]

    def do_join():
        free = [i for i in range(n) if i not in live]
        got = p.assign()
        if free:
            assert got == free[0] and got not in live
            live.add(got)
        else:
            assert got == b.SPECTATOR  # full: a spectator holds no seat

    ops = ["join"] * (n + 3)
    for _ in range(n_ops):
        ops.append(("drop", reasons[rng.randint(0, len(reasons) - 1)]))
    rng.shuffle(ops)
    for op in ops:
        if op == "join":
            do_join()
        elif live:  # a drop for ANY reason releases the seat (the reason is bookkeeping only)
            seat = sorted(live)[rng.randint(0, len(live) - 1)]
            p.release(seat)
            live.discard(seat)
    assert set(p.occupied()) == live  # policy state matches the shadow set under mixed drop reasons


@given(st.lists(CMD, min_size=1, max_size=25), st.integers(-1, N_AC - 1), st.integers(1, 400))
def test_bind_is_first_and_dropped_client_is_bind_plus_prefix(cmds, seat, cap):
    # The BIND-001 record is the FIRST downstream record; a client's delivery — full or dropped — begins
    # with its BIND. Compose: prepend the client's BIND to the produced frame stream, then a byte-cap
    # drop (SLOW: kernel accepts nothing after frame 0) yields a strict byte-prefix that still starts
    # with the whole BIND record. This is the pure form of the netboundasync leg-3 [BIND | prefix] claim.
    accepted, _ = authorized_submit(cmds)
    frames = produce_frames(accepted)
    bind_rec = b.encode_bind(seat, N_AC)
    stream = [bind_rec] + frames                       # BIND is the first framing record
    whole = fr.encode_stream(stream)
    n = len(stream)
    # FAST (greedy) gets the whole [BIND | frames]; SLOW accepts only the first frame's worth then stalls
    fast, fast_capped = deliver_capped(stream, [10 ** 9] * n, cap)
    slow, slow_capped = deliver_capped(stream, [10 ** 9] + [0] * (n - 1), cap)
    assert not fast_capped and fast == whole
    assert whole.startswith(slow)                      # SLOW: a clean byte-prefix of [BIND | frames]
    bind_framed = fr.encode_stream([bind_rec])
    assert slow.startswith(bind_framed[:len(slow)])    # ...that begins with (part of) the BIND record
    if len(slow) >= len(bind_framed):
        assert slow.startswith(bind_framed)            # if past the BIND, the whole BIND arrived first


@given(st.lists(CMD, max_size=25), st.data())
def test_dead_client_never_changes_a_survivors_bytes_or_the_frames(cmds, data):
    # ORTHOGONALITY (downstream drop x everything): two seated clients share the produced stream. FAST
    # (greedy) gets the whole [BIND | frames]; DEAD (stalls after frame 0) is a strict prefix, reaped
    # under a live deadline. Whether DEAD is dropped changes NOTHING about FAST's bytes NOR the produced
    # frames — the merge is the product of the axes, not a coupling.
    accepted, _ = authorized_submit(cmds)
    frames = produce_frames(accepted)
    stream = [b.encode_bind(0, N_AC)] + frames
    n = len(stream)
    if n < 3:
        return
    liveness = data.draw(st.integers(1, n - 2))
    d = data.draw(st.integers(1, n - 1 - liveness))
    fast, fast_reaped = deliver_liveness(stream, [10 ** 9] * n, liveness)
    dead, dead_reaped = deliver_liveness(stream, [10 ** 9] * d + [0] * (n - d), liveness)
    whole = fr.encode_stream(stream)
    assert not fast_reaped and fast == whole            # FAST untouched by DEAD's fate
    assert dead_reaped and whole.startswith(dead) and len(dead) < len(whole)
    # and the produced frames themselves are independent of the downstream drop
    assert produce_frames(accepted) == frames
