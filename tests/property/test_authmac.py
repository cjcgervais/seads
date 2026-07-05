"""Strong-credential handshake properties (layer 26): SipHash-2-4 MAC + CHALLENGE-001 / HELLO-002 +
the verifying SecretTable.

Layer 21 bound a client's seat to an abstracted i64 `token` the server merely LOOKED UP — a public
username with no proof of possession. Layer 26 turns it into a keyed MAC over a server-issued
challenge that the server VERIFIES against a pre-shared secret. All pure TRANSPORT (outside the
kernel / world_hash — no det_math, no seal). Byte parity with the C++ mirror
(src/net/{siphash,authmac001,authmacserver}) is pinned by the shared known-encoding vectors in
tools/authmac_ref.py + src/net/netauthmac_test_main.cpp; here we prove the reference is
self-consistent and, crucially, that:

  * SipHash-2-4 matches the OFFICIAL reference vector and is deterministic + key/message sensitive;
  * CHALLENGE-001 / HELLO-002 round-trip every value and enforce their (distinct) version bytes;
  * the SecretTable VERIFIES: a seat is bound ONLY when a valid MAC over the issued nonce proves
    possession of the token's secret — a forged/stale/unknown credential is rejected into the SAME
    spectator class, so authentication stays an admission filter that never touches the CommandQueue;
  * the verified binding is still a function of IDENTITY (invariant to join order), never
    double-booked, and reclaim-own-seat on reconnect — layer 21's guarantees, now gated on a proof;
  * the challenge is FRESH per connection (distinct nonces), so a captured HELLO cannot be replayed.
"""
import sys
from pathlib import Path

from hypothesis import given, strategies as st

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import siphash_ref as sip
import authmac_ref as am
import bound_ref as b

U64 = st.integers(min_value=0, max_value=(1 << 64) - 1)
TOKEN = st.integers(min_value=-(1 << 40), max_value=(1 << 40))
N = st.integers(min_value=1, max_value=16)
SESSION = (0x1122334455667788, 0x99AABBCCDDEEFF00)


# ---- SipHash-2-4 primitive -------------------------------------------------------------
def test_siphash_official_vector():
    k0 = int.from_bytes(bytes(range(8)), "little")
    k1 = int.from_bytes(bytes(range(8, 16)), "little")
    assert sip.siphash24(k0, k1, bytes(range(15))) == 0xA129CA6149BE45E5


@given(U64, U64, st.binary(max_size=64))
def test_siphash_deterministic(k0, k1, msg):
    assert sip.siphash24(k0, k1, msg) == sip.siphash24(k0, k1, msg)
    assert 0 <= sip.siphash24(k0, k1, msg) <= (1 << 64) - 1


@given(U64, U64, st.binary(min_size=1, max_size=32))
def test_siphash_key_and_message_sensitive(k0, k1, msg):
    base = sip.siphash24(k0, k1, msg)
    assert sip.siphash24(k0 ^ 1, k1, msg) != base            # a 1-bit key change
    flipped = bytes([msg[0] ^ 1]) + msg[1:]                  # a 1-bit message change
    assert sip.siphash24(k0, k1, flipped) != base


# ---- CHALLENGE-001 / HELLO-002 codecs --------------------------------------------------
def test_known_encoding_pins():
    # the SAME bytes asserted in the C++ bridge (src/net/netauthmac_test_main.cpp LEG 3)
    assert am.encode_challenge(7) == bytes([0x01, 0x0E])          # nonce 7 -> zz 14
    assert am.encode_hello2(7, 1) == bytes([0x02, 0x0E, 0x02])    # token 7 (zz 14), mac 1 (zz 2)


@given(U64)
def test_challenge_roundtrip(nonce):
    dec, pos = am.decode_challenge(am.encode_challenge(nonce))
    assert pos == len(am.encode_challenge(nonce))
    assert dec == nonce


@given(TOKEN, U64)
def test_hello2_roundtrip(token, mac):
    dec, pos = am.decode_hello2(am.encode_hello2(token, mac))
    assert pos == len(am.encode_hello2(token, mac))
    assert dec["token"] == token and dec["mac"] == mac


@given(st.binary(min_size=0, max_size=4))
def test_version_bytes_enforced(tail):
    # CHALLENGE is v1, HELLO-002 is v2 — not interchangeable, and neither accepts the other's version.
    for bad in (0x00, 0x02, 0xFF):
        try:
            am.decode_challenge(bytes([bad]) + tail)
            assert False, "challenge wrong version must raise"
        except ValueError:
            pass
    for bad in (0x00, 0x01, 0xFF):
        try:
            am.decode_hello2(bytes([bad]) + tail)
            assert False, "hello2 wrong version must raise"
        except ValueError:
            pass


# ---- the verifying SecretTable ---------------------------------------------------------
def _present(table, token, k0, k1, counter):
    """Emulate one client: read the fresh nonce, compute its MAC, authenticate."""
    nonce = am.derive_nonce(SESSION[0], SESSION[1], counter)
    mac = am.compute_mac(k0, k1, nonce, token)
    return table.authenticate(token, nonce, mac)


@given(N, st.data())
def test_valid_proof_binds_designated_seat_invariant_to_order(n, data):
    # Enroll a random bijection token->seat with a per-token secret; authenticate in a SHUFFLED order.
    # A client holding the correct secret proves possession and draws EXACTLY its designated seat,
    # regardless of order — layer 21's headline, now gated on a verified MAC.
    seats = list(range(n))
    tokens = data.draw(st.lists(st.integers(-1000, 1000), min_size=n, max_size=n, unique=True))
    secrets = {t: (data.draw(U64), data.draw(U64)) for t in tokens}
    roster = dict(zip(tokens, seats))

    st_tbl = am.SecretTable(n)
    for tok, seat in roster.items():
        st_tbl.enroll(tok, seat, *secrets[tok])

    order = data.draw(st.permutations(tokens))
    for i, tok in enumerate(order):
        assert _present(st_tbl, tok, *secrets[tok], i) == roster[tok]
    assert sorted(st_tbl.occupied()) == seats


@given(N, st.data())
def test_forged_or_wrong_secret_is_rejected(n, data):
    # A client that knows the (public) token but NOT its secret cannot authenticate — the exact attack
    # layer 21 could not stop. Any wrong key -> spectator, seat left free.
    tokens = list(range(100, 100 + n))
    secrets = {t: (data.draw(U64.filter(lambda x: x != 0)), data.draw(U64)) for t in tokens}
    st_tbl = am.SecretTable(n)
    for i, tok in enumerate(tokens):
        st_tbl.enroll(tok, i, *secrets[tok])

    tok = data.draw(st.sampled_from(tokens))
    k0, k1 = secrets[tok]
    wrong = data.draw(st.tuples(U64, U64).filter(lambda kk: kk != (k0, k1)))
    assert _present(st_tbl, tok, *wrong, 0) == am.SPECTATOR   # forgery rejected
    assert st_tbl.occupied() == []                            # no seat taken
    assert _present(st_tbl, tok, k0, k1, 1) == tok - 100      # the genuine holder still succeeds


@given(N, st.data())
def test_replayed_mac_under_a_different_nonce_is_rejected(n, data):
    # A MAC captured for one challenge nonce does not authenticate under a DIFFERENT nonce — the
    # freshness the server's per-connection challenge buys.
    tok = 100
    k0, k1 = data.draw(U64), data.draw(U64)
    st_tbl = am.SecretTable(n)
    st_tbl.enroll(tok, 0, k0, k1)
    c1 = data.draw(st.integers(0, 1000))
    c2 = data.draw(st.integers(0, 1000).filter(lambda c: c != c1))
    n1 = am.derive_nonce(SESSION[0], SESSION[1], c1)
    n2 = am.derive_nonce(SESSION[0], SESSION[1], c2)
    mac1 = am.compute_mac(k0, k1, n1, tok)
    # nonces are fresh per accept, and the captured mac is bound to n1 -> useless under n2.
    if n1 != n2:
        assert st_tbl.authenticate(tok, n2, mac1) == am.SPECTATOR
    assert st_tbl.authenticate(tok, n1, mac1) == 0           # valid under its own nonce


@given(N, st.integers(min_value=0, max_value=32))
def test_unknown_token_is_a_spectator(n, extra):
    st_tbl = am.SecretTable(n)
    for seat in range(n):
        st_tbl.enroll(1000 + seat, seat, 0xA + seat, 0xB + seat)
    for k in range(extra):
        assert _present(st_tbl, 90000 + k, 1, 2, k) == am.SPECTATOR


@given(N, st.lists(st.integers(0, 15), max_size=32), st.randoms(use_true_random=False))
def test_no_double_booking_and_reclaim(n, release_seq, rng):
    # A random join/leave interleaving, every join a VERIFIED proof. A seat is never double-held; a
    # second live login is a spectator; a reconnecting identity reclaims ITS OWN seat.
    tokens = list(range(2000, 2000 + n))
    secrets = {t: (t * 3 + 1, t * 5 + 2) for t in tokens}
    st_tbl = am.SecretTable(n)
    for i, tok in enumerate(tokens):
        st_tbl.enroll(tok, i, *secrets[tok])
    live = {}
    counter = [0]

    def join(tok):
        seat_want = tok - 2000
        got = _present(st_tbl, tok, *secrets[tok], counter[0]); counter[0] += 1
        if seat_want in live:
            assert got == am.SPECTATOR
        else:
            assert got == seat_want
            live[seat_want] = tok

    ops = [("join", t) for t in tokens] * 2 + [("leave", None)] * len(release_seq)
    rng.shuffle(ops)
    for kind, tok in ops:
        if kind == "join":
            join(tok)
        elif live:
            seat = min(live)
            st_tbl.release(seat)
            del live[seat]
    assert set(st_tbl.occupied()) == set(live)


@given(st.lists(st.integers(0, 1000), min_size=2, max_size=8, unique=True))
def test_challenge_is_fresh_per_accept(counters):
    # distinct accept counters -> distinct nonces (a captured HELLO can't be replayed to a later
    # connection because the later connection issues a different challenge).
    nonces = [am.derive_nonce(SESSION[0], SESSION[1], c) for c in counters]
    assert len(set(nonces)) == len(nonces)


@given(st.integers(-1, 8), st.integers(0, 8))
def test_seat_authorizes_reused(seat, aircraft):
    # authorization is UNCHANGED from layers 18/21 — a seated client commands only its own seat, a
    # spectator commands nothing — so the verified binding composes with it verbatim.
    assert b.seat_authorizes(seat, aircraft) == (seat >= 0 and aircraft == seat)
