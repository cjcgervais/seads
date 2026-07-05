"""Asymmetric-signature handshake properties (layer 27): SHA-512 + Ed25519 + HELLO-003 + the
verifying PubkeyTable.

Layer 26 verified a client's identity with a SYMMETRIC keyed MAC (SipHash): the server held a
shared secret per token. Layer 27 turns it into an Ed25519 PUBLIC-KEY signature the server VERIFIES
with only a public key — so a full roster leak (every enrolled public key) cannot impersonate any
client. All pure TRANSPORT (outside the kernel / world_hash — no det_math, no seal). Byte parity
with the C++ mirror (src/net/{sha512,ed25519,authsig001,authsigserver}) is pinned by the shared
known-encoding + fixed-key vectors in tools/{sha512_ref,ed25519_ref,authsig_ref}.py +
src/net/netauthsig_test_main.cpp; here we prove the reference is self-consistent and, crucially,
that:

  * SHA-512 matches the OFFICIAL NIST vector and is deterministic + message sensitive;
  * Ed25519 matches its fixed-seed pin (confirmed vs the `cryptography` library in ed25519_ref.py),
    is deterministic, verifies genuine signatures, and REJECTS a tampered signature / wrong message
    / wrong public key (the asymmetric headline);
  * HELLO-003 round-trips every token + signature and enforces its (distinct) version byte;
  * the PubkeyTable VERIFIES: a seat is bound ONLY when a valid signature over the issued nonce
    proves possession of the token's PRIVATE key — a forged/stale/unknown credential is rejected
    into the SAME spectator class, so authentication stays an admission filter that never touches
    the CommandQueue;
  * the verified binding is still a function of IDENTITY (invariant to join order), never
    double-booked, and reclaim-own-seat on reconnect — layers 21/26's guarantees, now on a signature;
  * the challenge is FRESH per connection (reused CHALLENGE-001), so a captured HELLO cannot replay.
"""
import sys
from pathlib import Path

from hypothesis import given, settings, strategies as st

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import sha512_ref as sha
import ed25519_ref as ed
import authsig_ref as asig
import bound_ref as b

TOKEN = st.integers(min_value=-(1 << 40), max_value=(1 << 40))
N = st.integers(min_value=1, max_value=6)
SESSION = (0x1122334455667788, 0x99AABBCCDDEEFF00)


def _seed(base):
    return bytes([(base + i) & 0xFF for i in range(32)])


# ---- SHA-512 primitive -----------------------------------------------------------------
def test_sha512_official_vector():
    assert sha.sha512(b"abc").hex() == (
        "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a"
        "2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f")


@given(st.binary(max_size=200))
def test_sha512_deterministic_and_sized(msg):
    a = sha.sha512(msg)
    assert a == sha.sha512(msg)
    assert len(a) == 64


@given(st.binary(min_size=1, max_size=64))
def test_sha512_message_sensitive(msg):
    flipped = bytes([msg[0] ^ 1]) + msg[1:]
    assert sha.sha512(flipped) != sha.sha512(msg)


# ---- Ed25519 signature -----------------------------------------------------------------
def test_ed25519_fixed_pin():
    seed = bytes(range(32))
    assert ed.public_key(seed).hex() == asig.ed._PIN_PK
    assert ed.sign(seed, b"SEADS-ed25519-pin").hex() == asig.ed._PIN_SIG


@given(st.integers(0, 255), st.binary(max_size=48))
@settings(max_examples=30, deadline=None)
def test_ed25519_roundtrip_and_deterministic(base, msg):
    seed = _seed(base)
    pk = ed.public_key(seed)
    sig = ed.sign(seed, msg)
    assert len(sig) == 64 and len(pk) == 32
    assert ed.sign(seed, msg) == sig            # deterministic
    assert ed.verify(pk, msg, sig)              # genuine verifies


@given(st.integers(0, 255), st.binary(min_size=1, max_size=48), st.integers(0, 63))
@settings(max_examples=30, deadline=None)
def test_ed25519_rejects_tampering(base, msg, bit):
    seed = _seed(base)
    pk = ed.public_key(seed)
    sig = ed.sign(seed, msg)
    bad = bytearray(sig); bad[bit // 8] ^= (1 << (bit % 8))
    assert not ed.verify(pk, msg, bytes(bad))               # tampered signature
    assert not ed.verify(pk, msg + b"!", sig)               # wrong message
    other = ed.public_key(_seed((base + 1) & 0xFF))
    if other != pk:
        assert not ed.verify(other, msg, sig)               # wrong public key (the headline)


# ---- HELLO-003 codec -------------------------------------------------------------------
def test_hello3_known_encoding_pin():
    # the SAME bytes asserted in the C++ bridge (src/net/netauthsig_test_main.cpp LEG 3)
    assert asig.encode_hello3(7, bytes([0xAA, 0xBB])) == bytes([0x03, 0x0E, 0x02, 0xAA, 0xBB])


@given(TOKEN, st.integers(0, 255))
@settings(max_examples=30, deadline=None)
def test_hello3_roundtrip(token, base):
    sig = ed.sign(_seed(base), b"m")
    dec, pos = asig.decode_hello3(asig.encode_hello3(token, sig))
    assert pos == len(asig.encode_hello3(token, sig))
    assert dec["token"] == token and dec["sig"] == sig


@given(st.binary(min_size=0, max_size=4))
def test_hello3_version_enforced(tail):
    # HELLO-003 is v3 — a decoder rejects HELLO-001 (v1) / HELLO-002 (v2) / any other version byte.
    for bad in (0x00, 0x01, 0x02, 0xFF):
        try:
            asig.decode_hello3(bytes([bad]) + tail)
            assert False, "hello3 wrong version must raise"
        except ValueError:
            pass


# ---- the verifying PubkeyTable ---------------------------------------------------------
def _present(table, token, seed, counter):
    """Emulate one client: read the fresh nonce, sign it, authenticate."""
    nonce = asig.derive_nonce(SESSION[0], SESSION[1], counter)
    sig = asig.sign_challenge(seed, nonce, token)
    return table.authenticate(token, nonce, sig)


@given(N, st.data())
@settings(max_examples=40, deadline=None)
def test_valid_signature_binds_designated_seat_invariant_to_order(n, data):
    # Enroll a random bijection token->seat with a per-token keypair; authenticate in a SHUFFLED order.
    # A client holding the correct PRIVATE key signs and draws EXACTLY its designated seat, regardless
    # of order. The server enrolls ONLY public keys.
    seats = list(range(n))
    tokens = data.draw(st.lists(st.integers(-1000, 1000), min_size=n, max_size=n, unique=True))
    seeds = {t: _seed(data.draw(st.integers(0, 255))) for t in tokens}
    roster = dict(zip(tokens, seats))

    pt = asig.PubkeyTable(n)
    for tok, seat in roster.items():
        pt.enroll(tok, seat, ed.public_key(seeds[tok]))

    order = data.draw(st.permutations(tokens))
    for i, tok in enumerate(order):
        assert _present(pt, tok, seeds[tok], i) == roster[tok]
    assert sorted(pt.occupied()) == seats


@given(N, st.data())
@settings(max_examples=30, deadline=None)
def test_forged_or_wrong_key_is_rejected(n, data):
    # A client that knows the (public) token AND the whole public roster but NOT the private key
    # cannot authenticate — the capability layer 26's server-held secret could not deny. Any wrong
    # key -> spectator, seat left free.
    tokens = list(range(100, 100 + n))
    seeds = {t: _seed((t * 7) & 0xFF) for t in tokens}
    pt = asig.PubkeyTable(n)
    for i, tok in enumerate(tokens):
        pt.enroll(tok, i, ed.public_key(seeds[tok]))

    tok = data.draw(st.sampled_from(tokens))
    wrong = _seed(data.draw(st.integers(0, 255)))
    nonce = asig.derive_nonce(SESSION[0], SESSION[1], 0)
    if ed.public_key(wrong) != ed.public_key(seeds[tok]):
        forged = asig.sign_challenge(wrong, nonce, tok)
        assert pt.authenticate(tok, nonce, forged) == asig.SPECTATOR   # forgery rejected
        assert pt.occupied() == []                                     # no seat taken
    assert _present(pt, tok, seeds[tok], 1) == tok - 100               # the genuine holder succeeds


@given(N, st.integers(0, 255))
@settings(max_examples=30, deadline=None)
def test_replayed_signature_under_a_different_nonce_is_rejected(n, base):
    tok = 100
    seed = _seed(base)
    pt = asig.PubkeyTable(n)
    pt.enroll(tok, 0, ed.public_key(seed))
    n1 = asig.derive_nonce(SESSION[0], SESSION[1], 1)
    n2 = asig.derive_nonce(SESSION[0], SESSION[1], 2)
    sig1 = asig.sign_challenge(seed, n1, tok)
    if n1 != n2:
        assert pt.authenticate(tok, n2, sig1) == asig.SPECTATOR   # stale nonce
    assert pt.authenticate(tok, n1, sig1) == 0                    # valid under its own nonce


@given(N, st.integers(min_value=0, max_value=8))
@settings(max_examples=20, deadline=None)
def test_unknown_token_is_a_spectator(n, extra):
    pt = asig.PubkeyTable(n)
    for seat in range(n):
        pt.enroll(1000 + seat, seat, ed.public_key(_seed(seat)))
    for k in range(extra):
        assert _present(pt, 90000 + k, _seed(k), k) == asig.SPECTATOR


@given(N, st.integers(0, 20), st.randoms(use_true_random=False))
@settings(max_examples=15, deadline=None)
def test_no_double_booking_and_reclaim(n, n_leaves, rng):
    # A random join/leave interleaving, every join a VERIFIED signature. A seat is never double-held;
    # a second live login is a spectator; a reconnecting identity reclaims ITS OWN seat.
    tokens = list(range(2000, 2000 + n))
    seeds = {t: _seed((t * 3) & 0xFF) for t in tokens}
    pt = asig.PubkeyTable(n)
    for i, tok in enumerate(tokens):
        pt.enroll(tok, i, ed.public_key(seeds[tok]))
    live = {}
    counter = [0]

    def join(tok):
        seat_want = tok - 2000
        got = _present(pt, tok, seeds[tok], counter[0]); counter[0] += 1
        if seat_want in live:
            assert got == asig.SPECTATOR
        else:
            assert got == seat_want
            live[seat_want] = tok

    ops = [("join", t) for t in tokens] * 2 + [("leave", None)] * n_leaves
    rng.shuffle(ops)
    for kind, tok in ops:
        if kind == "join":
            join(tok)
        elif live:
            seat = min(live)
            pt.release(seat)
            del live[seat]
    assert set(pt.occupied()) == set(live)


@given(st.lists(st.integers(0, 1000), min_size=2, max_size=8, unique=True))
def test_challenge_is_fresh_per_accept(counters):
    # reused CHALLENGE-001: distinct accept counters -> distinct nonces (no HELLO replay to a later
    # connection).
    nonces = [asig.derive_nonce(SESSION[0], SESSION[1], c) for c in counters]
    assert len(set(nonces)) == len(nonces)


@given(st.integers(-1, 8), st.integers(0, 8))
def test_seat_authorizes_reused(seat, aircraft):
    # authorization is UNCHANGED from layers 18/21/26 — a seated client commands only its own seat, a
    # spectator commands nothing — so the verified binding composes with it verbatim.
    assert b.seat_authorizes(seat, aircraft) == (seat >= 0 and aircraft == seat)
