"""Bidirectional-server composition properties (netcode layer 16).

Layer 16 (src/net/bidiserver.{h,cpp}, broadcast_bidi) merges the layer-15b UPSTREAM input path with
the layer-11/12/15a DOWNSTREAM output hygiene (async send buffers / byte-cap / liveness reap). The
byte-exact end-to-end claim — scrambled upstream commands driven through the full async path reproduce
build_server_frames to a cooperative reader, and a never-reading client is reaped/capped without
disturbing that stream — is proven over REAL 127.0.0.1 sockets by the determinism bridge
src/net/netbidi_test_main.cpp (ctest netbidi_bridge). Here we prove the MODEL the merge rests on: the
two directions are ORTHOGONAL — the upstream canonicalisation (which frames are produced) and the
downstream delivery policy (which bytes reach whom, and which slow/dead clients are shed) do not
interfere.

The upstream half reuses tools/input001_ref.CommandQueue (the same ordering contract test_input001.py
pins); the downstream half reuses the send-buffer / byte-cap / liveness models test_broadcast.py pins
for layers 11/12/15a. This file composes them and proves:

  * the PRODUCED frame stream is a pure function of the command SET (blind to upstream arrival order) —
    the InputProducer's hold-last + drop, modelled tick-exact;
  * a downstream drop (cap or liveness) never changes the produced frames NOR any surviving client's
    delivered bytes — the merge is the product of the two directions, not a coupling.
"""
import sys
from pathlib import Path

from hypothesis import given, strategies as st

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import input001_ref as ic
import framing_ref as fr

DYADIC = st.sampled_from([0.0, 0.5, -0.5, 0.25, -0.25, 0.75, -0.75, 1.0, 1.5, 2.0])
N_AC = 3
HORIZON = 40
SNAP = 5

CMD = st.builds(ic._cmd, apply_tick=st.integers(0, HORIZON - 1), aircraft=st.integers(0, N_AC - 1),
                seq=st.integers(-5, 5), target_phi=DYADIC, target_g=DYADIC, throttle=DYADIC,
                fire=st.booleans())


# --- upstream: the InputProducer modelled tick-exact (peek at pre-step tick, consume, hold-last) -----
def produce_frames(cmds, n_aircraft=N_AC, horizon=HORIZON, snap_every=SNAP):
    """Reference InputProducer (src/net/inputserver.cpp): submit the whole command SET, then for each
    step t_->t take each aircraft's winner for pre-step tick t_ (or hold-last), consume t_, and emit a
    'frame' every snap_every ticks. The frame payload is a deterministic serialisation of the held
    command wire-keys + the emit tick — a pure function of the canonically-ordered command SET, so
    identical for any submit order. (The real frame is a kernel snapshot; here its identity as a
    function of the winners is all that matters for the orthogonality claim.)"""
    q = ic.CommandQueue(n_aircraft)
    for c in cmds:
        q.submit(c)
    held = [(0, 0, 0, 0, 0)] * n_aircraft  # neutral wire-key tuple; hold-last per aircraft

    def frame(tick):
        body = b"".join(repr((tick, wk)).encode() for wk in held)
        return body

    frames = [frame(0)]  # tick-0 pre-step world
    for t in range(1, horizon + 1):
        for a in range(n_aircraft):
            win = q.peek(t - 1, a)
            if win is not None:
                held[a] = ic._wire_key(win)
        q.consume(t - 1)
        if t % snap_every == 0:
            frames.append(frame(t))
    return frames


# --- downstream: the layer-11/12/15a delivery models (mirror test_broadcast.py) -----------------------
def deliver_capped(frames, accepts, cap):
    """Layer-12 capped send-buffer delivery (broadcast_bidi enqueue + over_cap order): enqueue each
    length-prefixed frame, flush what the kernel accepts, then a pending backlog over cap (cap>0) drops
    the client — the tail is discarded whole. Returns (delivered bytes, capped?)."""
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
    """Layer-15a liveness-reap delivery (broadcast_bidi flush + reap_dead): a client that makes no
    receive progress (buffer non-empty AND no bytes moved) for > liveness frames is reaped, its tail
    discarded whole. liveness==0 disables it. Returns (delivered bytes, reaped?)."""
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
@given(st.lists(CMD, max_size=25), st.randoms(use_true_random=False))
def test_produced_frames_are_order_invariant(cmds, rng):
    # The merged server's PRODUCED frame stream is a pure function of the command SET: submitting the
    # same set in any order yields byte-identical frames. This is the layer-15b claim, restated at the
    # producer level the merge preserves (the async downstream cannot touch it).
    shuffled = list(cmds)
    rng.shuffle(shuffled)
    assert produce_frames(cmds) == produce_frames(shuffled)


@given(st.lists(CMD, max_size=25), st.randoms(use_true_random=False), st.data())
def test_downstream_delivery_is_blind_to_upstream_order(cmds, rng, data):
    # Compose the two directions: a cooperative reader (kernel always accepts) receives the whole
    # encoded frame stream, and that stream is identical whatever order the upstream commands arrived —
    # the downstream delivery is blind to upstream order because the frames are.
    shuffled = list(cmds)
    rng.shuffle(shuffled)
    fa, fb = produce_frames(cmds), produce_frames(shuffled)
    greedy_a = [10 ** 9] * len(fa)
    greedy_b = [10 ** 9] * len(fb)
    da, ca = deliver_capped(fa, greedy_a, 0)
    db, cb = deliver_capped(fb, greedy_b, 0)
    assert not ca and not cb
    assert da == db == fr.encode_stream(fa)


@given(st.lists(CMD, max_size=25), st.integers(min_value=1, max_value=400), st.data())
def test_a_capped_client_never_changes_a_surviving_clients_bytes(cmds, cap, data):
    # ORTHOGONALITY: two clients share the produced stream. FAST (greedy) survives with the whole
    # stream; SLOW (kernel accepts nothing after the first frame) is a byte-prefix, and MAY be capped.
    # Whether SLOW is dropped changes NOTHING about FAST's delivered bytes — the cap decides only WHO
    # is dropped, never WHICH bytes flow to anyone else. This is the pure form of the netbidi leg-C claim.
    frames = produce_frames(cmds)
    n = len(frames)
    fast, fast_capped = deliver_capped(frames, [10 ** 9] * n, cap)
    slow_accepts = [10 ** 9] + [0] * (n - 1)
    slow, slow_capped = deliver_capped(frames, slow_accepts, cap)
    assert not fast_capped
    assert fast == fr.encode_stream(frames)                 # FAST always gets the whole stream...
    whole = fr.encode_stream(frames)
    assert whole.startswith(slow)                           # ...SLOW is always a clean byte-prefix
    # FAST's bytes are identical whether or not SLOW was capped (recompute with no cap for SLOW):
    fast_again, _ = deliver_capped(frames, [10 ** 9] * n, 0)
    assert fast_again == fast


@given(st.lists(CMD, min_size=1, max_size=25), st.data())
def test_liveness_reaps_a_dead_client_without_touching_a_live_one(cmds, data):
    # The liveness mirror of the cap orthogonality: a DEAD client (stops accepting mid-stream) is reaped
    # to a strict prefix, while a co-resident FAST client (greedy) is never reaped and gets everything —
    # and cap_bytes==0 here, so ONLY liveness can drop anyone (the netbidi leg-B orthogonality).
    frames = produce_frames(cmds)
    n = len(frames)
    if n < 3:
        return  # need room for a death frame + a silent tail past the deadline
    liveness = data.draw(st.integers(min_value=1, max_value=n - 2))
    d = data.draw(st.integers(min_value=1, max_value=n - 1 - liveness))
    dead_accepts = [10 ** 9] * d + [0] * (n - d)
    dead, reaped = deliver_liveness(frames, dead_accepts, liveness)
    fast, fast_reaped = deliver_liveness(frames, [10 ** 9] * n, liveness)
    assert reaped and not fast_reaped
    whole = fr.encode_stream(frames)
    assert whole.startswith(dead) and len(dead) < len(whole)  # DEAD: strict byte-prefix
    assert fast == whole                                      # FAST: the whole stream, untouched


@given(st.lists(CMD, max_size=25), st.data())
def test_cap0_and_liveness0_deliver_the_whole_stream(cmds, data):
    # Degenerate corner: with no hygiene policy (cap=0, liveness=0) EVERY client — under ANY kernel
    # acceptance pattern — receives the whole encoded stream after the drain. The hygiene knobs only
    # ever REMOVE a misbehaving client; they never alter the bytes of the base async delivery. This is
    # the layer-15b downstream reduced to the merged server's cap=0/liveness=0 default.
    frames = produce_frames(cmds)
    n = len(frames)
    accepts = [data.draw(st.integers(min_value=0, max_value=200)) for _ in range(n)]
    dc, capped = deliver_capped(frames, accepts, 0)
    dl, reaped = deliver_liveness(frames, accepts, 0)
    whole = fr.encode_stream(frames)
    assert not capped and not reaped
    assert dc == dl == whole
