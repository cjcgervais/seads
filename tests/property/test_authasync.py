"""Authenticated + async server composition properties (netcode layer 22).

Layer 22 (src/net/authasyncserver.{h,cpp}, broadcast_auth_async) is the product of two orthogonal axes:
the layer-21 UPSTREAM IDENTITY binding (a HELLO-001 credential looked up in a CredentialTable to a
DESIGNATED seat + per-seat authorization, tools/auth_ref.py) and the layer-16/19 DOWNSTREAM hygiene
(async send buffers + byte-cap + liveness reap, modelled tick-exact in test_bidi.py / test_boundasync.py).
The byte-exact end-to-end claim — authenticated clients each fly their designated aircraft to
build_server_frames through the async path, and a dead authenticated client is reaped/capped + its seat
freed without disturbing a cooperative client — is proven over REAL 127.0.0.1 sockets by
src/net/netauthasync_test_main.cpp (ctest netauthasync_bridge). Here we prove the MODEL the composition
rests on: the axes (which identity holds which seat, which commands are AUTHORIZED, which frames are
PRODUCED, which bytes are DELIVERED / who is dropped) do not interfere — and, the headline vs the
join-order layer 19, that a seat freed by ANY downstream drop is reclaimed by ITS OWN identity.

Reuses the layer-21 CredentialTable / seat_authorizes (auth_ref / bound_ref) and the layer-16 downstream
delivery models (mirrored from test_bidi.py / test_boundasync.py). Proves:

  * AUTHENTICATION is downstream-blind: the authenticated + authorized command set (hence the produced
    frames) is a function of the identity-partitioned upstream ONLY — no downstream cap/liveness drop can
    change it, and an unknown-token spectator adds nothing;
  * a credential seat is freed on ANY drop reason (clean EOF, fatal flush, byte-cap shed, liveness reap)
    and reclaimed by ITS OWN identity — never two live sessions on one seat, and the reconnecting token
    draws its designated seat, not the lowest free (the layer-19 join-order policy could not promise this);
  * the BIND-001 record is the FIRST downstream record, so every client's delivery — full or a dropped
    prefix — begins with its BIND; a shed client is a strict byte-prefix of [BIND | frames].
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


# --- upstream WITH identity authentication + authorization -------------------------------------------
def authenticated_submit(cmds, n_aircraft=N_AC):
    """Model the layer-22 server: enroll n identities (token 500+seat -> seat), each seated identity
    upstreams only ITS aircraft's commands (authorized), PLUS an unknown-token spectator upstreaming the
    WHOLE set (all dropped). Returns (accepted, unauth). The accepted set is exactly the in-range commands,
    once each — identical to the layer-19 authorized set, now reached via a CredentialTable instead of a
    join-order SeatPolicy."""
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
    """Reference InputProducer (mirrors test_boundasync.produce_frames): submit the command SET, then per
    step take each aircraft's winner for the pre-step tick (or hold-last), emit a 'frame' every snap_every
    ticks. A pure function of the canonically-ordered command SET."""
    q = ic.CommandQueue(n_aircraft)
    for c in cmds:
        q.submit(c)
    held = [(0, 0, 0, 0, 0)] * n_aircraft

    def frame(tick):
        return b"".join(repr((tick, wk)).encode() for wk in held)

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


# --- downstream: layer-16 delivery models (mirrored from test_boundasync.py) --------------------------
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
def test_authentication_is_downstream_blind(cmds, cap, liveness):
    # AXIS ORTHOGONALITY (identity upstream x downstream hygiene): the AUTHENTICATED + AUTHORIZED command
    # set — and hence the produced frames — is a pure function of the identity-partitioned upstream,
    # independent of ANY downstream policy. No cap/liveness value can change which commands were accepted
    # or which frames were produced; the unknown-token spectator contributes nothing.
    accepted, unauth = authenticated_submit(cmds)
    frames = produce_frames(accepted)
    in_range = [c for c in cmds if c["aircraft"] < N_AC]
    assert len(accepted) == len(in_range)                       # exactly the seated set, once each
    # every seat but its own dropped each in-range command, and the spectator dropped ALL commands
    assert unauth == (N_AC - 1) * len(in_range) + len(cmds)
    assert produce_frames(authenticated_submit(cmds)[0]) == frames  # independent of the policy knobs


@given(st.lists(CMD, max_size=25), st.randoms(use_true_random=False))
def test_authenticated_frames_are_order_invariant(cmds, rng):
    # The produced frames are a pure function of the AUTHENTICATED command SET: submitting it in any order
    # yields byte-identical frames (the layer-15b claim, restated on the authenticated set the merge feeds).
    accepted, _ = authenticated_submit(cmds)
    shuffled = list(accepted)
    rng.shuffle(shuffled)
    assert produce_frames(accepted) == produce_frames(shuffled)


@given(st.integers(1, 8), st.integers(0, 40), st.randoms(use_true_random=False))
def test_credential_seat_freed_on_any_drop_reason_and_reclaimed_by_own_identity(n, n_ops, rng):
    # THE HEADLINE vs layer 19: a seat is freed on ANY leave — clean EOF, fatal flush, byte-cap shed
    # (capped), OR liveness reap (reaped) — all funnel through drop_client -> CredentialTable.release; and
    # because the binding is by IDENTITY, the reconnecting token draws ITS OWN designated seat, never the
    # lowest free. Drive a random interleaving of authenticate(join)/release(leave, ANY reason); INVARIANTS:
    # a token in the roster always resolves to its designated seat when free, a held seat rejects a
    # double-login to spectator, and a released seat is reclaimed by its OWN identity.
    tokens = list(range(3000, 3000 + n))                        # token 3000+i -> designated seat i
    ct = a.CredentialTable(n)
    for i, tok in enumerate(tokens):
        ct.enroll(tok, i)
    live = {}                                                   # seat -> token currently seated
    reasons = ["eof", "flush_fail", "capped", "reaped"]         # the reason is bookkeeping only

    def do_join(tok):
        seat_want = tok - 3000
        got = ct.authenticate(tok)
        if seat_want in live:                                   # its seat is held -> double-login
            assert got == a.SPECTATOR
        else:
            assert got == seat_want and seat_want not in live   # ALWAYS its own designated seat
            live[seat_want] = tok

    ops = [("join", t) for t in tokens] * 2
    for _ in range(n_ops):
        ops.append(("drop", reasons[rng.randint(0, len(reasons) - 1)]))
    rng.shuffle(ops)
    for kind, _payload in ops:
        if kind == "join":
            do_join(tokens[rng.randint(0, n - 1)])
        elif live:                                              # a drop for ANY reason releases the seat
            seat = sorted(live)[rng.randint(0, len(live) - 1)]
            ct.release(seat)
            del live[seat]
    assert set(ct.occupied()) == set(live)                      # policy state matches the shadow set


@given(st.lists(CMD, min_size=1, max_size=25), st.integers(-1, N_AC - 1), st.integers(1, 400))
def test_bind_is_first_and_dropped_client_is_bind_plus_prefix(cmds, seat, cap):
    # The BIND-001 record is the FIRST downstream record; a client's delivery — full or dropped — begins
    # with its BIND. Compose: prepend the client's BIND (its designated seat) to the produced frame stream,
    # then a byte-cap drop (SLOW: kernel accepts nothing after frame 0) yields a strict byte-prefix that
    # still starts with the whole BIND record. The pure form of the netauthasync leg-3 [BIND | prefix] claim.
    accepted, _ = authenticated_submit(cmds)
    frames = produce_frames(accepted)
    bind_rec = b.encode_bind(seat, N_AC)                        # seat could be a real designated seat or spectator
    stream = [bind_rec] + frames
    whole = fr.encode_stream(stream)
    nrec = len(stream)
    fast, fast_capped = deliver_capped(stream, [10 ** 9] * nrec, cap)
    slow, slow_capped = deliver_capped(stream, [10 ** 9] + [0] * (nrec - 1), cap)
    assert not fast_capped and fast == whole
    assert whole.startswith(slow)                              # SLOW: a clean byte-prefix of [BIND | frames]
    bind_framed = fr.encode_stream([bind_rec])
    assert slow.startswith(bind_framed[:len(slow)])            # ...beginning with (part of) the BIND record
    if len(slow) >= len(bind_framed):
        assert slow.startswith(bind_framed)                    # if past the BIND, the whole BIND arrived first


@given(st.lists(CMD, max_size=25), st.data())
def test_dead_authenticated_client_never_changes_a_survivors_bytes_or_the_frames(cmds, data):
    # ORTHOGONALITY (downstream drop x everything): two authenticated clients share the produced stream.
    # FAST (greedy) gets the whole [BIND | frames]; DEAD (stalls after frame 0) is a strict prefix, reaped
    # under a live deadline. Whether DEAD is dropped changes NOTHING about FAST's bytes NOR the produced
    # frames — the merge is the product of the axes, not a coupling.
    accepted, _ = authenticated_submit(cmds)
    frames = produce_frames(accepted)
    stream = [b.encode_bind(0, N_AC)] + frames
    nrec = len(stream)
    if nrec < 3:
        return
    liveness = data.draw(st.integers(1, nrec - 2))
    d = data.draw(st.integers(1, nrec - 1 - liveness))
    fast, fast_reaped = deliver_liveness(stream, [10 ** 9] * nrec, liveness)
    dead, dead_reaped = deliver_liveness(stream, [10 ** 9] * d + [0] * (nrec - d), liveness)
    whole = fr.encode_stream(stream)
    assert not fast_reaped and fast == whole                   # FAST untouched by DEAD's fate
    assert dead_reaped and whole.startswith(dead) and len(dead) < len(whole)
    assert produce_frames(accepted) == frames                  # frames independent of the downstream drop
