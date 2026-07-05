"""Certificate-PKI server composition properties (netcode layers 31 + 32).

Layer 31 (src/net/authcertasyncserver.{h,cpp}, broadcast_authcert_async) folds the layer-16/19 DOWNSTREAM
hygiene (async send buffers + byte-cap + liveness reap) onto layer 30's CA-CERTIFICATE binding (a
CHALLENGE-001 nonce + a verified HELLO-004 = [CA-signed CERT-001, Ed25519 possession proof] -> a DESIGNATED
seat, tools/cert_ref.py). Layer 32 (authcertcatchupserver.{h,cpp}, broadcast_authcert_catchup) adds layer-20
windowed late-join CATCH-UP. The byte-exact end-to-end claims are proven over REAL 127.0.0.1 sockets by
src/net/netauthcertasync_test_main.cpp / netauthcertcatchup_test_main.cpp (ctest netauthcert_async_bridge /
netauthcert_catchup_bridge). Here we prove the MODEL the composition rests on — the four axes (which
certified identity PROVES which seat, which commands are AUTHORIZED, which frames are PRODUCED, which bytes
are DELIVERED / who is dropped, and a joiner's REPLAY DEPTH) do not interfere — now with admission gated on a
CA-signed certificate the server verifies under ONE trusted CA public key (CaTable), not a per-client roster.

Reuses the layer-30 CaTable + CERT-001 issuance (cert_ref) + Ed25519 (ed25519_ref) and mirrors the
layer-16/20 delivery + window models. Proves the certified admission is downstream-blind + replay-depth-blind,
a certified seat is freed on ANY drop and reclaimed by ITS OWN identity, a self-signed / revoked / stale-epoch
credential is admitted to nothing, and a mid-stream joiner's delivery is [BIND | frames[max(0,fi-W):]] with
trimmed == max(0, N-W).
"""
import sys
from pathlib import Path

from hypothesis import given, settings, strategies as st

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import input001_ref as ic
import framing_ref as fr
import bound_ref as b
import cert_ref as cert
import ed25519_ref as ed

DYADIC = st.sampled_from([0.0, 0.5, -0.5, 0.25, -0.25, 0.75, -0.75, 1.0, 1.5, 2.0])
N_AC = 3
HORIZON = 40
SNAP = 5
SESSION = (0x1122334455667788, 0x99AABBCCDDEEFF00)

CMD = st.builds(ic._cmd, apply_tick=st.integers(0, HORIZON - 1), aircraft=st.integers(0, N_AC - 1),
                seq=st.integers(-5, 5), target_phi=DYADIC, target_g=DYADIC, throttle=DYADIC,
                fire=st.booleans())


def _seed(base):
    return bytes([(base + i) & 0xFF for i in range(32)])


CA_SEED = _seed(0xC0)
CA_PUB = ed.public_key(CA_SEED)


# --- upstream WITH verified CA-certificate authentication + authorization ------------------------------
def cert_submit(cmds, n_aircraft=N_AC):
    """Model the layer-31/32 server: the server trusts ONE CA public key. Issue one CA-signed certificate
    per identity (token 500+seat -> seat, its own keypair), each seated identity presents its certificate,
    signs the challenge, and upstreams only ITS aircraft's commands (authorized), PLUS a SELF-SIGNED-cert
    forger and an unknown-token spectator, both upstreaming the WHOLE set (all dropped). Returns
    (accepted, unauth)."""
    ct = cert.CaTable(n_aircraft, CA_PUB)
    seeds = {500 + seat: _seed(0x40 + seat) for seat in range(n_aircraft)}
    certs = {tok: cert.issue_cert(CA_SEED, tok, tok - 500, 0, ed.public_key(seeds[tok])) for tok in seeds}
    seat_of = {}
    for seat in range(n_aircraft):
        tok = 500 + seat
        nonce = cert.derive_nonce(*SESSION, seat)
        csig = cert.sign_challenge(seeds[tok], nonce, tok)
        seat_of[tok] = ct.authenticate(certs[tok], nonce, csig)  # a valid CA cert + proof -> its seat
    # a SELF-SIGNED forger: an attacker mints a token-500 (seat 0) cert with its OWN key -> non-CA -> spectator
    fnonce = cert.derive_nonce(*SESSION, 900)
    self_signed = cert.issue_cert(_seed(0xF0), 500, 0, 0, ed.public_key(seeds[500]))
    good = cert.sign_challenge(seeds[500], fnonce, 500)
    assert ct.authenticate(self_signed, fnonce, good) == cert.SPECTATOR
    # an unknown token with a malformed certificate -> spectator
    spectator = ct.authenticate(b"\x01\x02", cert.derive_nonce(*SESSION, 901), b"\x00" * 64)
    assert spectator == cert.SPECTATOR
    accepted, unauth = [], 0
    for _tok, seat in seat_of.items():
        for c in cmds:
            if b.seat_authorizes(seat, c["aircraft"]):
                accepted.append(c)
            else:
                unauth += 1
    for c in cmds:                                               # the spectator authorizes nothing
        assert not b.seat_authorizes(spectator, c["aircraft"])
        unauth += 1
    return accepted, unauth


def produce_frames(cmds, n_aircraft=N_AC, horizon=HORIZON, snap_every=SNAP):
    """Reference InputProducer (mirrors test_authsigservers.produce_frames): a pure function of the
    canonically-ordered command SET."""
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


# --- downstream: layer-16 delivery models (mirrored from test_authsigservers.py) ----------------------
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


# --- catch-up: the layer-20 window model (joiner at fi gets history[max(0,fi-W):]) --------------------
def catchup_suffix(frames, fi, window):
    start = max(0, fi - window) if window > 0 else 0
    trimmed = max(0, len(frames) - window) if window > 0 else 0
    return frames[start:], trimmed


# =====================================================================================================
@given(st.lists(CMD, max_size=20), st.integers(1, 400), st.integers(0, 8))
@settings(max_examples=25, deadline=None)
def test_cert_admission_is_downstream_and_replay_blind(cmds, cap, liveness):
    # AXIS ORTHOGONALITY: the CERTIFIED + AUTHORIZED command set (hence the produced frames) is a pure
    # function of the identity-partitioned upstream, independent of ANY downstream policy OR catch-up window.
    # A self-signed forger and an unknown-token spectator contribute nothing.
    accepted, unauth = cert_submit(cmds)
    frames = produce_frames(accepted)
    in_range = [c for c in cmds if c["aircraft"] < N_AC]
    assert len(accepted) == len(in_range)                       # exactly the seated set, once each
    assert unauth == (N_AC - 1) * len(in_range) + len(cmds)
    assert produce_frames(cert_submit(cmds)[0]) == frames       # independent of the policy knobs


@given(st.lists(CMD, max_size=20), st.randoms(use_true_random=False))
@settings(max_examples=25, deadline=None)
def test_cert_frames_are_order_invariant(cmds, rng):
    accepted, _ = cert_submit(cmds)
    shuffled = list(accepted)
    rng.shuffle(shuffled)
    assert produce_frames(accepted) == produce_frames(shuffled)


@given(st.integers(1, 6), st.integers(0, 30), st.randoms(use_true_random=False))
@settings(max_examples=20, deadline=None)
def test_cert_seat_freed_on_any_drop_and_reclaimed_by_own_identity(n, n_ops, rng):
    # A seat is freed on ANY leave (EOF / fatal flush / byte-cap shed / liveness reap) -> CaTable.release;
    # and because the binding is by the CERTIFICATE's designated seat, the reconnecting token draws ITS OWN
    # seat, never the lowest free. Drive a random join(present+sign)/leave interleaving.
    tokens = list(range(3000, 3000 + n))
    seeds = {t: _seed((t * 5) & 0xFF) for t in tokens}
    ct = cert.CaTable(n, CA_PUB)
    certs = {t: cert.issue_cert(CA_SEED, t, i, 0, ed.public_key(seeds[t])) for i, t in enumerate(tokens)}
    live = {}
    ctr = [0]

    def do_join(tok):
        seat_want = tok - 3000
        nonce = cert.derive_nonce(*SESSION, ctr[0]); ctr[0] += 1
        got = ct.authenticate(certs[tok], nonce, cert.sign_challenge(seeds[tok], nonce, tok))
        if seat_want in live:
            assert got == cert.SPECTATOR
        else:
            assert got == seat_want and seat_want not in live
            live[seat_want] = tok

    ops = [("join", t) for t in tokens] * 2 + [("drop", None)] * n_ops
    rng.shuffle(ops)
    for kind, _ in ops:
        if kind == "join":
            do_join(tokens[rng.randint(0, n - 1)])
        elif live:
            seat = sorted(live)[rng.randint(0, len(live) - 1)]
            ct.release(seat)
            del live[seat]
    assert set(ct.occupied()) == set(live)


@given(st.lists(CMD, min_size=1, max_size=20), st.integers(-1, N_AC - 1), st.integers(1, 400))
@settings(max_examples=20, deadline=None)
def test_bind_is_first_and_dropped_client_is_bind_plus_prefix(cmds, seat, cap):
    # The BIND-001 record is the FIRST downstream record after the (already-sent) CHALLENGE; a byte-cap drop
    # yields a strict byte-prefix of [BIND | frames]. (The CHALLENGE is sent synchronously before the async
    # buffer, so the buffered stream the cap governs begins with the BIND.)
    accepted, _ = cert_submit(cmds)
    frames = produce_frames(accepted)
    bind_rec = b.encode_bind(seat, N_AC)
    stream = [bind_rec] + frames
    whole = fr.encode_stream(stream)
    nrec = len(stream)
    fast, fast_capped = deliver_capped(stream, [10 ** 9] * nrec, cap)
    slow, slow_capped = deliver_capped(stream, [10 ** 9] + [0] * (nrec - 1), cap)
    assert not fast_capped and fast == whole
    assert whole.startswith(slow)
    bind_framed = fr.encode_stream([bind_rec])
    if len(slow) >= len(bind_framed):
        assert slow.startswith(bind_framed)


@given(st.lists(CMD, max_size=20), st.data())
@settings(max_examples=20, deadline=None)
def test_dead_cert_client_never_changes_a_survivors_bytes_or_the_frames(cmds, data):
    accepted, _ = cert_submit(cmds)
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
    assert not fast_reaped and fast == whole
    assert dead_reaped and whole.startswith(dead) and len(dead) < len(whole)
    assert produce_frames(accepted) == frames


@given(st.lists(CMD, max_size=20), st.integers(0, HORIZON // SNAP), st.integers(0, HORIZON // SNAP))
@settings(max_examples=25, deadline=None)
def test_catchup_window_suffix_and_trimmed(cmds, kjoin, window):
    # LAYER 32: a joiner at frame kJoin under window W receives EXACTLY frames[max(0,kJoin-W):], and the
    # server trims max(0, N-W) — the catch-up window is admission-blind. The joiner's delivery is
    # [BIND(spectator) | that suffix].
    accepted, _ = cert_submit(cmds)
    frames = produce_frames(accepted)
    n = len(frames)
    kjoin = min(kjoin, n)
    suffix, trimmed = catchup_suffix(frames, kjoin, window)
    start = max(0, kjoin - window) if window > 0 else 0
    assert suffix == frames[start:]
    assert trimmed == (max(0, n - window) if window > 0 else 0)
    delivery = fr.encode_stream([b.encode_bind(cert.SPECTATOR, N_AC)] + suffix)
    assert delivery == fr.encode_stream([b.encode_bind(-1, N_AC)]) + fr.encode_stream(suffix)


@given(st.lists(CMD, max_size=20), st.integers(0, HORIZON // SNAP))
@settings(max_examples=20, deadline=None)
def test_catchup_is_authorization_blind(cmds, window):
    # A self-signed-cert spectator upstreaming the WHOLE set changes nothing: its commands are all rejected,
    # the produced stream is unchanged, and it still catches up the window suffix.
    accepted, _ = cert_submit(cmds)
    frames = produce_frames(accepted)
    assert produce_frames(accepted) == frames
    suffix, _ = catchup_suffix(frames, len(frames), window)
    assert suffix == frames[max(0, len(frames) - window):] if window > 0 else suffix == frames


@given(st.lists(CMD, max_size=20), st.integers(0, N_AC - 1))
@settings(max_examples=20, deadline=None)
def test_revocation_and_rotation_reject_into_the_spectator_class(cmds, victim_seat):
    # The headline over the layer-27/28/29 fixed roster: a REVOKED token and a STALE-epoch (post-rotation)
    # certificate both reject to SPECTATOR and thus contribute NOTHING to the produced frames — exactly like
    # an unknown token. Compose against a healthy baseline: revoking/rotating the victim never changes the
    # frames produced by the OTHER seats' commands.
    ct = cert.CaTable(N_AC, CA_PUB)
    seeds = {500 + s: _seed(0x40 + s) for s in range(N_AC)}
    certs = {t: cert.issue_cert(CA_SEED, t, t - 500, 0, ed.public_key(seeds[t])) for t in seeds}
    ctr = [0]

    def present(tok, epoch_cert=None):
        nonce = cert.derive_nonce(*SESSION, ctr[0]); ctr[0] += 1
        c = epoch_cert if epoch_cert is not None else certs[tok]
        return ct.authenticate(c, nonce, cert.sign_challenge(seeds[tok], nonce, tok))

    victim = 500 + victim_seat
    ct.revoke(victim)
    assert present(victim) == cert.SPECTATOR                     # revoked -> no seat
    ct.unrevoke(victim)
    ct.set_min_epoch(victim, 5)
    assert present(victim) == cert.SPECTATOR                     # stale epoch-0 cert -> no seat
    rotated = cert.issue_cert(CA_SEED, victim, victim_seat, 5, ed.public_key(seeds[victim]))
    assert present(victim, rotated) == victim_seat               # re-certified at the floor -> its own seat
