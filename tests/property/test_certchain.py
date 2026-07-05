"""Certificate-CHAIN handshake properties (layer 33): HELLO-005 + verify_chain + the verifying
CaChainTable (one trusted ROOT public key, path validation up to it, with subtree revocation and
per-link rotation).

Layer 30 (test_cert.py) trusted ONE self-signed root CA and each client presented a SINGLE certificate
signed DIRECTLY by it — no delegation. Layer 33 carries a certificate CHAIN [leaf, intermediate, ...,
top] and validates the PATH: each link is signed by the NEXT one's key and the top by the trusted root.
A real PKI delegates issuance to intermediate CAs; revoking or rotating an INTERMEDIATE invalidates
every leaf beneath it. All pure TRANSPORT (outside the kernel / world_hash — no det_math, no seal).
Byte parity with the C++ mirror (src/net/{cert001,authcertchainserver}) is pinned by the shared
known-encoding + fixed-key vectors in tools/cert_ref.py + src/net/netauthcertchain_test_main.cpp; here
we prove the reference is self-consistent and, crucially, that:

  * HELLO-005 round-trips any chain + signature and enforces its (distinct) version byte;
  * verify_chain accepts a leaf<-intermediate<-root PATH and REJECTS a self-signed leaf (no path), a
    broken link (leaf not signed by the presented intermediate), and an over-depth chain;
  * the CaChainTable binds the LEAF's seat ONLY when the whole path verifies to the root AND a valid
    possession proof over the issued nonce proves the LEAF private key — every failure rejects into the
    SAME spectator class, so authentication stays an admission filter that never touches the CommandQueue;
  * INTERMEDIATE subtree revocation rejects every leaf beneath a revoked intermediate (and only those);
  * INTERMEDIATE (and leaf) key rotation via an epoch floor supersedes older chains while a re-issued
    higher-epoch chain authenticates;
  * a depth-1 root-signed leaf (the layer-30 case) is a valid degenerate chain;
  * the binding is a function of IDENTITY (invariant to join order), never double-booked, reclaim-own-seat.
"""
import sys
from pathlib import Path

from hypothesis import given, settings, strategies as st

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import ed25519_ref as ed
import cert_ref as cert
import bound_ref as b

SMALLINT = st.integers(min_value=-1000, max_value=1000)
N = st.integers(min_value=1, max_value=6)
SESSION = (0x1122334455667788, 0x99AABBCCDDEEFF00)
ROOT_SEED = bytes([(0xC0 + i) & 0xFF for i in range(32)])
ROOT_PUB = ed.public_key(ROOT_SEED)
INT1_BASE, INT2_BASE = 0xA0, 0xA1
INT1_TOKEN, INT2_TOKEN = 900, 901


def _seed(base):
    return bytes([(base + i) & 0xFF for i in range(32)])


def _intermediate(int_base, int_token, epoch=0):
    """root -> intermediate certificate (the intermediate CA's pubkey, signed by the root)."""
    return cert.issue_cert(ROOT_SEED, int_token, 0, epoch, ed.public_key(_seed(int_base)))


def _leaf(issuer_base, token, seat, epoch, client_base):
    """issuer -> leaf certificate (the client pubkey, signed by the issuer's key)."""
    return cert.issue_cert(_seed(issuer_base), token, seat, epoch, ed.public_key(_seed(client_base)))


def _chain_via(int_base, int_token, token, seat, client_base, epoch=0):
    return [_leaf(int_base, token, seat, epoch, client_base), _intermediate(int_base, int_token)]


def _present(table, chain, leaf_token, leaf_base, counter):
    nonce = cert.derive_nonce(SESSION[0], SESSION[1], counter)
    sig = cert.sign_challenge(_seed(leaf_base), nonce, leaf_token)
    return table.authenticate(chain, nonce, sig)


def _fields(chain):
    return [cert.decode_cert(c)[0] for c in chain]


# ---- HELLO-005 codec -------------------------------------------------------------------
def test_hello5_known_encoding_pin():
    # the SAME bytes asserted in the C++ bridge (src/net/netauthcertchain_test_main.cpp LEG 3).
    assert cert.encode_hello5([bytes([0xAA]), bytes([0xBB])], bytes([0xCC])) == \
        bytes([0x05, 0x02, 0x01, 0xAA, 0x01, 0xBB, 0x01, 0xCC])


@given(st.integers(1, 4), st.integers(0, 255))
@settings(max_examples=30, deadline=None)
def test_hello5_roundtrip(depth, base):
    chain = [_leaf(INT1_BASE, 100 + i, i, 0, (base + i) & 0xFF) for i in range(depth)]
    sig = cert.sign_challenge(_seed(base), 999, 100)
    enc = cert.encode_hello5(chain, sig)
    dec, pos = cert.decode_hello5(enc)
    assert pos == len(enc)
    assert dec["certs"] == chain and dec["sig"] == sig


def test_hello5_version_enforced():
    # HELLO-005 is v5; the decoder rejects HELLO-001..4 (0x01..0x04) and any wrong version byte.
    for bad in (0x00, 0x01, 0x02, 0x03, 0x04, 0xFF):
        try:
            cert.decode_hello5(bytes([bad, 0x00])); assert False, "hello5 wrong version must raise"
        except ValueError:
            pass


# ---- verify_chain: the signature path -------------------------------------------------
@given(st.integers(0, 255))
@settings(max_examples=30, deadline=None)
def test_valid_path_validates(base):
    chain = _chain_via(INT1_BASE, INT1_TOKEN, 100, 2, base)
    assert cert.verify_chain(ROOT_PUB, _fields(chain), 4)


@given(st.integers(0, 255))
@settings(max_examples=30, deadline=None)
def test_self_signed_leaf_has_no_path_to_root(base):
    # A leaf signed by its OWN key, presented ALONE, does not reach the trusted root.
    self_leaf = cert.issue_cert(_seed(base), 100, 2, 0, ed.public_key(_seed(base)))
    if ed.public_key(_seed(base)) != ROOT_PUB:                 # (astronomically always true)
        assert not cert.verify_chain(ROOT_PUB, _fields([self_leaf]), 4)


@given(st.integers(0, 255), st.integers(0, 255))
@settings(max_examples=30, deadline=None)
def test_broken_link_rejected(rogue_base, client_base):
    # A leaf signed by a DIFFERENT key than the presented intermediate breaks the path.
    rogue_leaf = _leaf(rogue_base, 100, 2, 0, client_base)
    int1 = _intermediate(INT1_BASE, INT1_TOKEN)
    if ed.public_key(_seed(rogue_base)) != ed.public_key(_seed(INT1_BASE)):
        assert not cert.verify_chain(ROOT_PUB, _fields([rogue_leaf, int1]), 4)


def test_over_depth_rejected():
    chain = _chain_via(INT1_BASE, INT1_TOKEN, 100, 2, 0x10)  # length 2
    assert cert.verify_chain(ROOT_PUB, _fields(chain), 2)
    assert not cert.verify_chain(ROOT_PUB, _fields(chain), 1)  # too deep for max_depth 1


@given(st.integers(0, 255))
@settings(max_examples=20, deadline=None)
def test_depth1_root_signed_leaf_is_a_valid_chain(base):
    # A leaf signed directly by the root (the layer-30 case) is a valid degenerate depth-1 chain.
    direct = cert.issue_cert(ROOT_SEED, 100, 2, 0, ed.public_key(_seed(base)))
    assert cert.verify_chain(ROOT_PUB, _fields([direct]), 4)


# ---- the verifying CaChainTable --------------------------------------------------------
@given(N, st.data())
@settings(max_examples=40, deadline=None)
def test_chain_binds_designated_seat_invariant_to_order(n, data):
    # A random bijection token->seat; each leaf issued by intermediate I1; authenticate in a SHUFFLED
    # order. A client holding the leaf PRIVATE key + a valid chain draws EXACTLY the seat its leaf
    # designates, regardless of order. The server trusts only the ONE root key.
    seats = list(range(n))
    tokens = data.draw(st.lists(SMALLINT.filter(lambda t: t not in (INT1_TOKEN, INT2_TOKEN)),
                                min_size=n, max_size=n, unique=True))
    bases = {t: data.draw(st.integers(0, 255)) for t in tokens}
    roster = dict(zip(tokens, seats))
    chains = {t: _chain_via(INT1_BASE, INT1_TOKEN, t, roster[t], bases[t]) for t in tokens}

    ct = cert.CaChainTable(n, ROOT_PUB)
    order = data.draw(st.permutations(tokens))
    for i, tok in enumerate(order):
        assert _present(ct, chains[tok], tok, bases[tok], i) == roster[tok]
    assert sorted(ct.occupied()) == seats


@given(N, st.integers(0, 255))
@settings(max_examples=30, deadline=None)
def test_forged_possession_rejected(n, wrong_base):
    # Right chain, WRONG signing key -> the possession proof fails -> spectator; the genuine holder seats.
    chain = _chain_via(INT1_BASE, INT1_TOKEN, 100, 0, 0x10)
    ct = cert.CaChainTable(n, ROOT_PUB)
    nonce = cert.derive_nonce(SESSION[0], SESSION[1], 0)
    if ed.public_key(_seed(wrong_base)) != ed.public_key(_seed(0x10)):
        forged = cert.sign_challenge(_seed(wrong_base), nonce, 100)
        assert ct.authenticate(chain, nonce, forged) == cert.SPECTATOR
        assert ct.occupied() == []
    assert _present(ct, chain, 100, 0x10, 1) == 0


@given(N, st.data())
@settings(max_examples=30, deadline=None)
def test_intermediate_subtree_revocation(n, data):
    # Two intermediates: I1 issues some leaves, I2 issues others. Revoking I2 rejects EVERY leaf beneath
    # it (subtree revocation — the delegation headline); leaves under I1 are unaffected; un-revoke restores.
    if n < 2:
        n = 2
    seats = list(range(n))
    tokens = list(range(100, 100 + n))
    bases = {t: (t * 7) & 0xFF for t in tokens}
    # split the roster across the two intermediates.
    under = {t: (INT1_BASE if i % 2 == 0 else INT2_BASE) for i, t in enumerate(tokens)}
    utok = {INT1_BASE: INT1_TOKEN, INT2_BASE: INT2_TOKEN}
    chains = {t: _chain_via(under[t], utok[under[t]], t, seats[i], bases[t])
              for i, t in enumerate(tokens)}

    ct = cert.CaChainTable(n, ROOT_PUB)
    ct.revoke(INT2_TOKEN)
    counter = 0
    for i, t in enumerate(tokens):
        got = _present(ct, chains[t], t, bases[t], counter); counter += 1
        if under[t] == INT2_BASE:
            assert got == cert.SPECTATOR                        # its whole subtree is revoked
        else:
            assert got == seats[i]                              # I1 subtree unaffected
    ct.unrevoke(INT2_TOKEN)
    # a previously-revoked I2 leaf now authenticates (into a still-free seat).
    for i, t in enumerate(tokens):
        if under[t] == INT2_BASE:
            assert _present(ct, chains[t], t, bases[t], counter) == seats[i]; counter += 1
            break


@given(st.integers(2, 6), st.integers(1, 50))
@settings(max_examples=30, deadline=None)
def test_intermediate_rotation_supersedes_old_chain(n, floor):
    # Raise intermediate I2's epoch floor: an old (epoch-0) intermediate cert is superseded -> spectator,
    # but a re-issued epoch>=floor intermediate + leaf authenticates.
    seat = n - 1
    old_chain = _chain_via(INT2_BASE, INT2_TOKEN, 300, seat, 0x30)
    ct = cert.CaChainTable(n, ROOT_PUB)
    ct.set_min_epoch(INT2_TOKEN, floor)
    assert _present(ct, old_chain, 300, 0x30, 0) == cert.SPECTATOR
    new_chain = [_leaf(INT2_BASE, 300, seat, floor, 0x30), _intermediate(INT2_BASE, INT2_TOKEN, floor)]
    assert _present(ct, new_chain, 300, 0x30, 1) == seat


@given(st.integers(1, 6), st.integers(0, 255), st.integers(1, 50))
@settings(max_examples=30, deadline=None)
def test_leaf_rotation_supersedes_old_leaf(n, base, floor):
    # Rotation also works at the LEAF: raise a leaf token's floor; its epoch-0 chain -> spectator, a
    # re-issued epoch>=floor leaf under the same intermediate authenticates.
    seat = n - 1
    old_chain = _chain_via(INT1_BASE, INT1_TOKEN, 300, seat, base)
    ct = cert.CaChainTable(n, ROOT_PUB)
    ct.set_min_epoch(300, floor)
    assert _present(ct, old_chain, 300, base, 0) == cert.SPECTATOR
    new_base = (base + 0x40) & 0xFF
    new_chain = _chain_via(INT1_BASE, INT1_TOKEN, 300, seat, new_base, epoch=floor)
    assert _present(ct, new_chain, 300, new_base, 1) == seat


@given(N, st.integers(0, 20), st.randoms(use_true_random=False))
@settings(max_examples=15, deadline=None)
def test_no_double_booking_and_reclaim(n, n_leaves, rng):
    # A random join/leave interleaving, every join a VERIFIED chain + possession proof. A seat is never
    # double-held; a second live login is a spectator; a reconnecting identity reclaims ITS OWN seat.
    tokens = list(range(2000, 2000 + n))
    bases = {t: (t * 3) & 0xFF for t in tokens}
    chains = {t: _chain_via(INT1_BASE, INT1_TOKEN, t, t - 2000, bases[t]) for t in tokens}
    ct = cert.CaChainTable(n, ROOT_PUB)
    live = {}
    counter = [0]

    def join(tok):
        seat_want = tok - 2000
        got = _present(ct, chains[tok], tok, bases[tok], counter[0]); counter[0] += 1
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
    # authorization is UNCHANGED from layers 18/21/26/27/30 — the chain binding composes with it verbatim.
    assert b.seat_authorizes(seat, aircraft) == (seat >= 0 and aircraft == seat)
