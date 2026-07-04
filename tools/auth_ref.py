#!/usr/bin/env python3
"""
auth_ref.py — SEADS HELLO-001 identity handshake + credential-based seat binding REFERENCE
(netcode layer 21).

Layer 18 (bound_ref.py) settled the client->aircraft binding to a JOIN-ORDER SeatPolicy: the first
client to connect got seat 0, the next seat 1, and so on. That is "one client, one aircraft", but it
is POSITIONAL and UNAUTHENTICATED — a reconnecting client got a *different* seat depending on who
else was connected, and nothing tied a socket to an identity. Layer 21 settles the last honest-scope
caveat every bidirectional ADR flagged: the binding becomes IDENTITY-BASED.

Two pieces, both pure TRANSPORT (outside the kernel / world_hash — no det_math, no seal, rides
v1.26r0):

  * HELLO-001 — the client's one-time identity claim, sent UP as its FIRST framing frame (the mirror
    of the server's downstream BIND-001 reply). Modelled on BIND-001 / the layer-7 framing envelope
    (transport metadata, NOT a sealed rails.wire block): it carries no sim state and feeds no hash.
    Byte parity with the C++ mirror (src/net/hello001) is still pinned — the codebase pins every wire
    — reusing the sealed GEO-001 ZigZag+LEB128 i64 codec, so no new primitive, no det_math.

        HELLO-001 = [version:1 byte = 0x01] [ZigZag+LEB128 token]

    `token` is an opaque integer credential identifying the client. (A production system would carry
    a MAC/signature the server verifies against a secret; that is orthogonal crypto — this layer is
    about the BINDING mechanics, so the "credential" is abstracted to an i64 the server looks up in a
    pre-shared roster. The determinism story does not depend on the credential's cryptographic
    strength, only on the roster being a fixed function of identity.)

  * CredentialTable — the identity->seat binding. enroll(token, seat) registers a pre-shared roster
    (token maps to a DESIGNATED seat in [0, n_aircraft)); authenticate(token) returns:
        - the token's designated seat, when the token is enrolled AND that seat is currently free;
        - SPECTATOR (-1), when the token is unknown (no credential) OR the token's seat is already
          held by a live session (a double-login — the identity is valid but already seated).
    release(seat) frees a seat on leave, so a reconnecting identity reclaims ITS seat.

The headline vs layer 18: a client's seat is a function of its IDENTITY, INVARIANT TO JOIN ORDER.
Token T always maps to roster[T] whether its client connects first, last, or in the middle; an
unknown token gets NO aircraft (spectator, receive-only); the same token cannot seat two live
clients at once. The upstream AUTHORIZATION predicate is UNCHANGED from layer 18 (bound_ref /
seat_authorizes): a seated client may command ONLY its own seat, a spectator commands nothing. So
the layer-18 determinism claim composes verbatim — the authoritative kernel's frames stay a pure
function of the AUTHORIZED, canonically-ordered command SET; authentication only decides WHICH seat
(if any) each identity holds, it never touches the CommandQueue's ordering.

Usage:  python tools/auth_ref.py        # internal self-test
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import geo001_ref as g
import bound_ref as bnd  # reuse SPECTATOR + seat_authorizes (the layer-18 authorization predicate)

VERSION = 0x01
SPECTATOR = bnd.SPECTATOR  # -1


# ---- HELLO-001 codec (mirror of src/net/hello001.cpp) ----------------------------------
def encode_hello(token):
    """One HELLO-001 identity claim -> wire bytes (field order is the wire contract)."""
    out = bytearray([VERSION])
    out += g.encode_i64(int(token))
    return bytes(out)


def decode_hello(data, pos=0):
    """wire bytes -> ({token}, next_pos). Raises ValueError on a wrong/absent version byte."""
    if pos >= len(data) or data[pos] != VERSION:
        raise ValueError("hello001: bad or missing version byte")
    pos += 1
    token, pos = g.decode_i64(data, pos)
    return {"token": token}, pos


# ---- credential-based seat binding (mirror of src/net/authserver.cpp) -------------------
class CredentialTable:
    """Identity->seat binding. A pre-shared roster maps each known token to a DESIGNATED seat;
    authenticate(token) hands back that seat if it is currently free, else SPECTATOR (unknown token
    or double-login). release(seat) frees it. A deterministic function of the roster + the
    join/leave order (only double-login contention depends on order; the seat for a given token is
    fixed by the roster)."""

    def __init__(self, n_aircraft):
        self.n = n_aircraft
        self._roster = {}                       # token -> designated seat
        self._occupied = [False] * n_aircraft

    def enroll(self, token, seat):
        """Register a credential: `token` is designated `seat` (must be in range)."""
        if not (0 <= seat < self.n):
            raise ValueError("enroll: seat out of range")
        self._roster[int(token)] = int(seat)

    def authenticate(self, token):
        """Resolve a presented token to a seat, occupying it. Unknown token or an already-held seat
        (double-login) -> SPECTATOR."""
        seat = self._roster.get(int(token))
        if seat is None:                        # unknown identity: no aircraft
            return SPECTATOR
        if self._occupied[seat]:                # valid identity, but its seat is taken (double-login)
            return SPECTATOR
        self._occupied[seat] = True
        return seat

    def release(self, seat):
        if 0 <= seat < self.n:
            self._occupied[seat] = False

    def occupied(self):
        return [i for i in range(self.n) if self._occupied[i]]


# ---- self-test -------------------------------------------------------------------------
def _selftest():
    fails = 0

    # HELLO-001 known encoding (the cross-impl pin — the SAME bytes are asserted in the C++ bridge).
    #   version 0x01 ; token=7 -> zz 14 -> 0x0E
    known = encode_hello(7)
    expect = bytes([0x01, 0x0E])
    if known != expect:
        print(f"FAIL known encoding: {known.hex()} != {expect.hex()}"); fails += 1

    # A negative token round-trips (ZigZag carries the sign, as on BIND-001's seat).
    if encode_hello(-1) != bytes([0x01, 0x01]):
        print(f"FAIL negative token encoding: {encode_hello(-1).hex()}"); fails += 1

    # Round-trip a range of tokens exactly.
    for token in (-5, -1, 0, 1, 7, 300, 1 << 40):
        dec, pos = decode_hello(encode_hello(token))
        if pos != len(encode_hello(token)) or dec["token"] != token:
            print(f"FAIL round-trip token={token}: {dec}"); fails += 1

    # Wrong version byte -> hard error.
    try:
        decode_hello(bytes([0x02, 0x0E]))
        print("FAIL wrong version should raise"); fails += 1
    except ValueError:
        pass

    # CredentialTable: identity determines the seat, INVARIANT to enrollment/join order.
    #   Deliberately map tokens so token order != seat order (100->2, 200->0, 300->1).
    ct = CredentialTable(3)
    ct.enroll(100, 2)
    ct.enroll(200, 0)
    ct.enroll(300, 1)
    # authenticate in a DIFFERENT order than enrollment; each still gets ITS roster seat.
    if ct.authenticate(300) != 1 or ct.authenticate(100) != 2 or ct.authenticate(200) != 0:
        print("FAIL identity->seat not invariant to join order"); fails += 1
    if sorted(ct.occupied()) != [0, 1, 2]:
        print(f"FAIL occupancy after all seated: {ct.occupied()}"); fails += 1

    # Unknown token -> spectator (no aircraft).
    if ct.authenticate(999) != SPECTATOR:
        print("FAIL unknown token should be a spectator"); fails += 1

    # Double-login: token 100's seat (2) is held -> a second presenter of 100 is a spectator.
    if ct.authenticate(100) != SPECTATOR:
        print("FAIL double-login should be rejected to spectator"); fails += 1

    # Release token 100's seat -> the SAME identity reclaims the SAME seat (2), not the lowest free.
    ct.release(2)
    if ct.authenticate(100) != 2:
        print("FAIL a reconnecting identity should reclaim its own seat"); fails += 1

    # A spectator (from an unknown token) authorizes NOTHING; a seated client only its own seat.
    if bnd.seat_authorizes(SPECTATOR, 0) or not bnd.seat_authorizes(2, 2) or bnd.seat_authorizes(2, 0):
        print("FAIL authorization predicate under identity binding"); fails += 1

    if fails == 0:
        print("RESULT: HELLO-001 / CredentialTable REFERENCE SELFTEST PASS")
        return 0
    print(f"RESULT: HELLO-001 / CredentialTable REFERENCE SELFTEST FAIL ({fails})")
    return 1


if __name__ == "__main__":
    sys.exit(_selftest())
