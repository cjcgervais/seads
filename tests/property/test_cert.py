"""Certificate-PKI handshake properties (layer 30): CERT-001 + HELLO-004 + the verifying CaTable
(one trusted CA public key, with revocation and key rotation).

Layer 27 (test_authsig.py) bound each seat to a per-token PUBLIC key ENROLLED on the server. Real
public-key identity, but the roster is FIXED: no enrollment without a server change, no revocation,
no key rotation. Layer 30 moves trust to a CERTIFICATE AUTHORITY: the server holds ONE CA public key
and each client carries a CA-signed CERT-001 certificate binding its (token, seat, epoch, pubkey),
presented in HELLO-004 alongside an Ed25519 possession proof over a fresh challenge. All pure
TRANSPORT (outside the kernel / world_hash — no det_math, no seal). Byte parity with the C++ mirror
(src/net/{cert001,authcertserver}) is pinned by the shared known-encoding + fixed-key vectors in
tools/cert_ref.py + src/net/netauthcert_test_main.cpp; here we prove the reference is self-consistent
and, crucially, that:

  * CERT-001 / HELLO-004 round-trip every (token, seat, epoch) + certificate + signature and enforce
    their (distinct) version bytes;
  * a certificate VERIFIES only under the issuing CA public key — a SELF-SIGNED (non-CA) or TAMPERED
    certificate is rejected (the PKI headline: without the CA private key, no valid cert can be minted);
  * the CaTable binds a seat ONLY when the certificate is CA-signed AND a valid possession proof over
    the issued nonce proves the CERTIFIED private key — a self-signed / stolen-cert-wrong-key / stale /
    unknown credential is rejected into the SAME spectator class, so authentication stays an admission
    filter that never touches the CommandQueue;
  * REVOCATION rejects a compromised identity (and only that one); un-revoking restores it;
  * KEY ROTATION (a raised epoch floor) supersedes older certificates while a re-issued higher-epoch
    certificate with a NEW key authenticates;
  * the binding is still a function of IDENTITY (invariant to join order), never double-booked, and
    reclaim-own-seat on reconnect — layers 21/27's guarantees, now on a certificate.
"""
import sys
from pathlib import Path

from hypothesis import given, settings, strategies as st

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import ed25519_ref as ed
import cert_ref as cert
import bound_ref as b

TOKEN = st.integers(min_value=-(1 << 40), max_value=(1 << 40))
SMALLINT = st.integers(min_value=-1000, max_value=1000)
N = st.integers(min_value=1, max_value=6)
SESSION = (0x1122334455667788, 0x99AABBCCDDEEFF00)
CA_SEED = bytes([(0xC0 + i) & 0xFF for i in range(32)])
CA_PUB = ed.public_key(CA_SEED)


def _seed(base):
    return bytes([(base + i) & 0xFF for i in range(32)])


def _cert(token, seat, epoch, base):
    return cert.issue_cert(CA_SEED, token, seat, epoch, ed.public_key(_seed(base)))


def _present(table, token, seed, cert_bytes, counter):
    """Emulate one client: read the fresh nonce, sign it with `seed`, present `cert_bytes`."""
    nonce = cert.derive_nonce(SESSION[0], SESSION[1], counter)
    sig = cert.sign_challenge(seed, nonce, token)
    return table.authenticate(cert_bytes, nonce, sig)


# ---- CERT-001 codec --------------------------------------------------------------------
def test_cert_body_known_encoding_pin():
    # the SAME bytes asserted in the C++ bridge (src/net/netauthcert_test_main.cpp LEG 3).
    assert cert.cert_body(7, 1, 0, bytes([0x11] * 32)) == bytes([0x01, 0x0E, 0x02, 0x00] + [0x11] * 32)


def test_hello4_known_encoding_pin():
    assert cert.encode_hello4(bytes([0xAA, 0xBB]), bytes([0xCC])) == \
        bytes([0x04, 0x02, 0xAA, 0xBB, 0x01, 0xCC])


@given(TOKEN, st.integers(-8, 8), st.integers(-(1 << 20), 1 << 20), st.integers(0, 255))
@settings(max_examples=30, deadline=None)
def test_cert_roundtrip_and_ca_verify(token, seat, epoch, base):
    c = _cert(token, seat, epoch, base)
    cf, pos = cert.decode_cert(c)
    assert pos == len(c)
    assert cf["token"] == token and cf["seat"] == seat and cf["epoch"] == epoch
    assert cf["pubkey"] == ed.public_key(_seed(base))
    assert cert.verify_cert(CA_PUB, cf)                    # genuine cert verifies under the CA key


@given(TOKEN, st.integers(0, 255))
@settings(max_examples=30, deadline=None)
def test_hello4_roundtrip(token, base):
    c = _cert(token, 0, 0, base)
    sig = cert.sign_challenge(_seed(base), 999, token)
    dec, pos = cert.decode_hello4(cert.encode_hello4(c, sig))
    assert pos == len(cert.encode_hello4(c, sig))
    assert dec["cert"] == c and dec["sig"] == sig


def test_versions_enforced():
    # HELLO-004 is v4, CERT-001 is v1 — each decoder rejects the others / any wrong version byte.
    for bad in (0x00, 0x01, 0x02, 0x03, 0xFF):
        try:
            cert.decode_hello4(bytes([bad, 0x00])); assert False, "hello4 wrong version must raise"
        except ValueError:
            pass
    for bad in (0x00, 0x02, 0x04, 0xFF):
        try:
            cert.decode_cert(bytes([bad, 0x0E, 0x02, 0x00] + [0] * 34))
            assert False, "cert wrong version must raise"
        except ValueError:
            pass


# ---- CA verification: self-signed / tampered rejected ----------------------------------
@given(st.integers(0, 255), st.integers(-8, 8))
@settings(max_examples=30, deadline=None)
def test_self_signed_certificate_rejected(attacker_base, seat):
    # An attacker mints a certificate for token 100 with its OWN key (not the CA). It fails CA verify.
    # This is the headline: without the CA private key no valid certificate can be minted.
    n = max(seat, 0) + 1
    attacker = _seed(attacker_base)
    self_signed = cert.issue_cert(attacker, 100, max(seat, 0), 0, ed.public_key(_seed(0x10)))
    ct = cert.CaTable(n, CA_PUB)
    if ed.public_key(attacker) != CA_PUB:                  # (astronomically always true)
        assert _present(ct, 100, _seed(0x10), self_signed, 0) == cert.SPECTATOR
        assert ct.occupied() == []


@given(st.integers(0, 255), st.integers(0, 31))
@settings(max_examples=30, deadline=None)
def test_tampered_certificate_rejected(base, flip_byte):
    c = bytearray(_cert(100, 0, 0, base))
    # flip a byte inside the pubkey region (offset 4..36) — breaks the CA signature over the body.
    idx = 4 + (flip_byte % 32)
    c[idx] ^= 1
    cf, _ = cert.decode_cert(bytes(c))
    assert not cert.verify_cert(CA_PUB, cf)


# ---- the verifying CaTable -------------------------------------------------------------
@given(N, st.data())
@settings(max_examples=40, deadline=None)
def test_valid_cert_binds_designated_seat_invariant_to_order(n, data):
    # Issue a random bijection token->seat with a per-token keypair; authenticate in a SHUFFLED order.
    # A client holding the certified PRIVATE key + its CA certificate draws EXACTLY its designated
    # seat, regardless of order. The server holds ONLY the CA public key.
    seats = list(range(n))
    tokens = data.draw(st.lists(SMALLINT, min_size=n, max_size=n, unique=True))
    bases = {t: data.draw(st.integers(0, 255)) for t in tokens}
    roster = dict(zip(tokens, seats))
    certs = {t: _cert(t, roster[t], 0, bases[t]) for t in tokens}

    ct = cert.CaTable(n, CA_PUB)
    order = data.draw(st.permutations(tokens))
    for i, tok in enumerate(order):
        assert _present(ct, tok, _seed(bases[tok]), certs[tok], i) == roster[tok]
    assert sorted(ct.occupied()) == seats


@given(N, st.data())
@settings(max_examples=30, deadline=None)
def test_stolen_cert_wrong_key_rejected(n, data):
    # An attacker STEALS a victim's CA certificate (public) but lacks the certified PRIVATE key. The
    # possession proof fails -> spectator, seat left free; the genuine holder still succeeds.
    tokens = list(range(100, 100 + n))
    bases = {t: (t * 7) & 0xFF for t in tokens}
    certs = {t: _cert(t, i, 0, bases[t]) for i, t in enumerate(tokens)}
    ct = cert.CaTable(n, CA_PUB)

    tok = data.draw(st.sampled_from(tokens))
    i = tokens.index(tok)
    wrong = _seed(data.draw(st.integers(0, 255)))
    nonce = cert.derive_nonce(SESSION[0], SESSION[1], 0)
    if ed.public_key(wrong) != ed.public_key(_seed(bases[tok])):
        forged = cert.sign_challenge(wrong, nonce, tok)
        assert ct.authenticate(certs[tok], nonce, forged) == cert.SPECTATOR   # stolen cert, wrong key
        assert ct.occupied() == []
    assert _present(ct, tok, _seed(bases[tok]), certs[tok], 1) == i           # genuine holder succeeds


@given(N, st.integers(0, 255))
@settings(max_examples=30, deadline=None)
def test_replayed_proof_under_a_different_nonce_rejected(n, base):
    tok = 100
    c = _cert(tok, 0, 0, base)
    ct = cert.CaTable(n, CA_PUB)
    n1 = cert.derive_nonce(SESSION[0], SESSION[1], 1)
    n2 = cert.derive_nonce(SESSION[0], SESSION[1], 2)
    sig1 = cert.sign_challenge(_seed(base), n1, tok)
    if n1 != n2:
        assert ct.authenticate(c, n2, sig1) == cert.SPECTATOR   # stale nonce
    assert ct.authenticate(c, n1, sig1) == 0                     # valid under its own nonce


@given(N, st.data())
@settings(max_examples=30, deadline=None)
def test_revocation_targets_one_identity(n, data):
    # Revoke ONE identity: its (otherwise valid) certificate -> spectator; every other identity is
    # unaffected; un-revoking restores the seat.
    tokens = list(range(100, 100 + n))
    bases = {t: (t * 5) & 0xFF for t in tokens}
    certs = {t: _cert(t, i, 0, bases[t]) for i, t in enumerate(tokens)}
    victim = data.draw(st.sampled_from(tokens))
    ct = cert.CaTable(n, CA_PUB)
    ct.revoke(victim)
    counter = 0
    for i, tok in enumerate(tokens):
        got = _present(ct, tok, _seed(bases[tok]), certs[tok], counter); counter += 1
        assert got == (cert.SPECTATOR if tok == victim else i)
    ct.unrevoke(victim)
    ct.release(tokens.index(victim))                           # (it never took the seat, but be safe)
    assert _present(ct, victim, _seed(bases[victim]), certs[victim], counter) == tokens.index(victim)


@given(st.integers(1, 6), st.integers(0, 255), st.integers(1, 50))
@settings(max_examples=30, deadline=None)
def test_key_rotation_supersedes_old_epoch(n, base, floor):
    # Raise a token's epoch floor: its old (epoch 0) certificate is superseded -> spectator, but a
    # re-issued certificate at >= floor with a NEW key authenticates. Only the rotated identity is hit.
    tok = 300
    seat = n - 1
    old_cert = _cert(tok, seat, 0, base)
    ct = cert.CaTable(n, CA_PUB)
    ct.set_min_epoch(tok, floor)
    assert _present(ct, tok, _seed(base), old_cert, 0) == cert.SPECTATOR       # stale epoch
    new_base = (base + 0x40) & 0xFF
    new_cert = _cert(tok, seat, floor, new_base)                              # rotated (new) key
    assert _present(ct, tok, _seed(new_base), new_cert, 1) == seat


@given(N, st.integers(0, 20), st.randoms(use_true_random=False))
@settings(max_examples=15, deadline=None)
def test_no_double_booking_and_reclaim(n, n_leaves, rng):
    # A random join/leave interleaving, every join a VERIFIED CA certificate + possession proof. A
    # seat is never double-held; a second live login is a spectator; a reconnecting identity reclaims
    # ITS OWN seat.
    tokens = list(range(2000, 2000 + n))
    bases = {t: (t * 3) & 0xFF for t in tokens}
    certs = {t: _cert(t, t - 2000, 0, bases[t]) for t in tokens}
    ct = cert.CaTable(n, CA_PUB)
    live = {}
    counter = [0]

    def join(tok):
        seat_want = tok - 2000
        got = _present(ct, tok, _seed(bases[tok]), certs[tok], counter[0]); counter[0] += 1
        if seat_want in live:
            assert got == cert.SPECTATOR
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
            ct.release(seat)
            del live[seat]
    assert set(ct.occupied()) == set(live)


@given(st.integers(-1, 8), st.integers(0, 8))
def test_seat_authorizes_reused(seat, aircraft):
    # authorization is UNCHANGED from layers 18/21/26/27 — a seated client commands only its own seat,
    # a spectator commands nothing — so the certificate binding composes with it verbatim.
    assert b.seat_authorizes(seat, aircraft) == (seat >= 0 and aircraft == seat)
