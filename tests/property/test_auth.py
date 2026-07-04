"""HELLO-001 identity handshake + credential-based (identity->seat) binding properties (layer 21).

Layer 21 settles the last honest-scope caveat every bidirectional ADR flagged: layer 18's
client->aircraft binding was POSITIONAL (join-order SeatPolicy) and UNAUTHENTICATED. Now a client
presents a HELLO-001 credential and the server looks it up in a pre-shared CredentialTable to a
DESIGNATED seat — invariant to join order — or seats it as a spectator when the identity is unknown.
All pure TRANSPORT (outside the kernel / world_hash — no det_math, no seal). Byte parity with the C++
mirror (src/net/hello001, src/net/authserver) is pinned by the shared known-encoding vector in
tools/auth_ref.py + src/net/netauth_test_main.cpp; here we prove the reference is self-consistent and,
crucially, that:

  * HELLO-001 round-trips every token (incl. negatives — ZigZag carries the sign) and the version byte
    is enforced;
  * the CredentialTable binds seat by IDENTITY, invariant to join order: a token always resolves to its
    ROSTER seat (never a different one because of who else connected); an unknown token gets no seat; a
    seat is never double-booked (double-login -> spectator); a released identity reclaims ITS OWN seat;
  * the AUTHORIZATION filter (reused from layer 18) composes with authentication: a seated client
    steers only its own seat, a spectator steers nothing — the property that keeps the kernel's frames
    a function of the AUTHORIZED command set.
"""
import sys
from pathlib import Path

from hypothesis import given, strategies as st

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import auth_ref as a
import bound_ref as b

TOKEN = st.integers(min_value=-(1 << 40), max_value=(1 << 40))
N = st.integers(min_value=1, max_value=16)


@given(TOKEN)
def test_hello_roundtrip(token):
    dec, pos = a.decode_hello(a.encode_hello(token))
    assert pos == len(a.encode_hello(token))
    assert dec["token"] == token


def test_hello_known_encoding_pin():
    # the SAME bytes asserted in the C++ bridge (src/net/netauth_test_main.cpp LEG 3)
    assert a.encode_hello(7) == bytes([0x01, 0x0E])   # token 7 -> zz 14 -> 0x0E
    assert a.encode_hello(-1) == bytes([0x01, 0x01])  # -1 -> zz 1


@given(st.binary(min_size=0, max_size=4))
def test_hello_wrong_version_rejected(tail):
    for bad in (0x00, 0x02, 0xFF):
        try:
            a.decode_hello(bytes([bad]) + tail)
            assert False, "wrong version must raise"
        except ValueError:
            pass


@given(N, st.data())
def test_credential_seat_is_a_function_of_identity(n, data):
    # Enroll a random bijection token->seat, then authenticate the tokens in a SHUFFLED order: each
    # token must resolve to EXACTLY its enrolled seat, regardless of authentication order. This is the
    # headline vs the layer-18 join-order policy (where the seat depended on who connected first).
    seats = list(range(n))
    tokens = data.draw(st.lists(st.integers(-1000, 1000), min_size=n, max_size=n, unique=True))
    roster = dict(zip(tokens, seats))  # token -> seat (a bijection over [0,n))

    ct = a.CredentialTable(n)
    for tok, seat in roster.items():
        ct.enroll(tok, seat)

    order = data.draw(st.permutations(tokens))
    for tok in order:
        assert ct.authenticate(tok) == roster[tok]     # its designated seat, no matter the order
    assert sorted(ct.occupied()) == seats              # every seat taken exactly once


@given(N, st.integers(min_value=0, max_value=32))
def test_unknown_token_is_a_spectator(n, extra):
    ct = a.CredentialTable(n)
    for seat in range(n):
        ct.enroll(1000 + seat, seat)                   # tokens 1000..1000+n-1
    # any token NOT in the roster gets no aircraft
    for k in range(extra):
        assert ct.authenticate(90000 + k) == a.SPECTATOR


@given(N, st.lists(st.integers(0, 15), max_size=32), st.randoms(use_true_random=False))
def test_credential_no_double_booking_and_reclaim(n, release_seq, rng):
    # A random join(authenticate)/leave(release) interleaving over a fixed roster. INVARIANTS: a seat is
    # never held by two live sessions; a token's second authenticate while its seat is held is rejected
    # (spectator); after release the SAME identity reclaims ITS OWN seat (not the lowest free).
    tokens = list(range(2000, 2000 + n))               # token 2000+i -> seat i
    ct = a.CredentialTable(n)
    for i, tok in enumerate(tokens):
        ct.enroll(tok, i)
    live = {}                                          # seat -> token currently seated

    def join(tok):
        seat_want = tok - 2000
        got = ct.authenticate(tok)
        if seat_want in live:                          # its seat is held -> double-login rejected
            assert got == a.SPECTATOR
        else:
            assert got == seat_want                    # always ITS OWN seat, never another
            live[seat_want] = tok

    ops = [("join", t) for t in tokens] * 2 + [("leave", None)] * len(release_seq)
    rng.shuffle(ops)
    for kind, tok in ops:
        if kind == "join":
            join(tok)
        elif live:
            seat = min(live)                           # release a deterministic live seat
            ct.release(seat)
            del live[seat]
    assert set(ct.occupied()) == set(live)             # policy state matches our shadow set


@given(st.integers(-1, 8), st.integers(0, 8))
def test_seat_authorizes_reused(seat, aircraft):
    # authorization is UNCHANGED from layer 18 — a seated client commands only its own seat, a
    # spectator commands nothing — so authentication composes with it verbatim.
    assert b.seat_authorizes(seat, aircraft) == (seat >= 0 and aircraft == seat)


# --- authentication + authorization compose: only bound identities drive their own seat -------------
import input001_ref as ic  # noqa: E402


CMD = st.builds(ic._cmd, apply_tick=st.integers(0, 30), aircraft=st.integers(0, 2),
                seq=st.integers(0, 5), target_phi=st.just(0.0), target_g=st.just(1.0),
                throttle=st.just(0.0), fire=st.booleans())


@given(st.lists(CMD, max_size=24))
def test_authenticated_partition_accepts_exactly_the_set(cmds):
    # Model 3 identities enrolled to seats 0,1,2, each upstreaming ONLY its own aircraft's commands, plus
    # an unknown-token spectator upstreaming EVERYTHING. The union the server authorizes equals exactly
    # the whole seated set; the spectator adds nothing.
    n = 3
    ct = a.CredentialTable(n)
    seat_of = {}
    for seat in range(n):
        ct.enroll(500 + seat, seat)
        seat_of[500 + seat] = ct.authenticate(500 + seat)
    spectator_seat = ct.authenticate(77777)            # unknown -> spectator
    assert spectator_seat == a.SPECTATOR

    accepted = []
    # each seated identity sends its own aircraft's commands (authorized); the spectator sends all.
    for tok, seat in seat_of.items():
        for c in cmds:
            if b.seat_authorizes(seat, c["aircraft"]):
                accepted.append((seat, c["apply_tick"], c["seq"], c["aircraft"]))
    for c in cmds:                                     # spectator: nothing authorized
        assert not b.seat_authorizes(spectator_seat, c["aircraft"])

    expect = [(c["aircraft"], c["apply_tick"], c["seq"], c["aircraft"]) for c in cmds if c["aircraft"] < n]
    assert sorted(accepted) == sorted(expect)
