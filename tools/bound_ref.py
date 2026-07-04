#!/usr/bin/env python3
"""
bound_ref.py — SEADS BIND-001 seat handshake + join-order SeatPolicy REFERENCE (netcode layer 18).

Layer 18 settles layer 15b's positional/unauthenticated client->aircraft binding. Two pieces, both
pure TRANSPORT (outside the kernel / world_hash — no det_math, no seal, rides v1.26r0):

  * BIND-001 — the server's one-time seat-assignment reply to a joining client. Modelled on the
    layer-7 framing envelope (transport metadata, NOT a sealed rails.wire block): it carries no sim
    state and feeds no hash. Byte parity with the C++ mirror (src/net/bind001) is still pinned — the
    codebase pins every wire — reusing the sealed GEO-001 ZigZag+LEB128 i64 codec, so no new primitive.

        BIND-001 = [version:1 byte = 0x01] [ZigZag+LEB128 seat] [ZigZag+LEB128 n_aircraft]

    `seat` = the assigned aircraft index, or SPECTATOR (-1) for a receive-only client; ZigZag carries
    the sign (as last_hit_by does on the WEAPON-001 wire).

  * SeatPolicy — the join-order seat assignment. assign() hands each joiner the LOWEST free seat in
    [0, n_aircraft); when all seats are taken further joiners are SPECTATORS (-1). release() frees a
    seat on leave, so the lowest free seat is reused. Pure integer bookkeeping; a deterministic
    function of the join/leave event order (a transport fact, not a sim one).

The layer-18 determinism claim (see boundserver.h) is that the authoritative kernel's frames stay a
pure function of the AUTHORIZED, canonically-ordered command SET: a client may only command its own
seat (the server drops a foreign-aircraft command, unchanged from never arriving — the same class as
the CommandQueue's OUT_OF_RANGE reject). BIND-001 and SeatPolicy are the bookkeeping around that
filter, not the filter's correctness.

Usage:  python tools/bound_ref.py        # internal self-test
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import geo001_ref as g

VERSION = 0x01
SPECTATOR = -1


# ---- BIND-001 codec (mirror of src/net/bind001.cpp) ------------------------------------
def encode_bind(seat, n_aircraft):
    """One BIND-001 record -> wire bytes (field order is the wire contract)."""
    out = bytearray([VERSION])
    out += g.encode_i64(int(seat))
    out += g.encode_i64(int(n_aircraft))
    return bytes(out)


def decode_bind(data, pos=0):
    """wire bytes -> ({seat, n_aircraft}, next_pos). Raises ValueError on a wrong/absent version."""
    if pos >= len(data) or data[pos] != VERSION:
        raise ValueError("bind001: bad or missing version byte")
    pos += 1
    seat, pos = g.decode_i64(data, pos)
    n_aircraft, pos = g.decode_i64(data, pos)
    return {"seat": seat, "n_aircraft": n_aircraft}, pos


# ---- join-order SeatPolicy (mirror of src/net/boundserver.cpp) --------------------------
class SeatPolicy:
    """Lowest-free-seat assignment with reuse. assign() -> lowest free index in [0,n) or SPECTATOR;
    release(seat) frees it. A deterministic function of the join/leave order."""

    def __init__(self, n_aircraft):
        self.n = n_aircraft
        self._occupied = [False] * n_aircraft

    def assign(self):
        for i in range(self.n):
            if not self._occupied[i]:
                self._occupied[i] = True
                return i
        return SPECTATOR

    def release(self, seat):
        if 0 <= seat < self.n:
            self._occupied[seat] = False

    def occupied(self):
        return [i for i in range(self.n) if self._occupied[i]]


def seat_authorizes(seat, aircraft):
    """The upstream authorization predicate: a seated client may command ONLY its own aircraft; a
    spectator (seat -1) may command nothing. Mirrors boundserver.cpp's check."""
    return seat >= 0 and aircraft == seat


# ---- self-test -------------------------------------------------------------------------
def _selftest():
    fails = 0

    # Known encoding (the cross-impl pin — the SAME bytes are asserted in the C++ bridge test).
    #   version 0x01 ; seat=1 -> zz 2 -> 0x02 ; n_aircraft=3 -> zz 6 -> 0x06
    known = encode_bind(1, 3)
    expect = bytes([0x01, 0x02, 0x06])
    if known != expect:
        print(f"FAIL known encoding: {known.hex()} != {expect.hex()}"); fails += 1

    # Spectator seat -1 -> zz 1 -> 0x01 (the sign survives ZigZag).
    spec = encode_bind(SPECTATOR, 3)
    if spec != bytes([0x01, 0x01, 0x06]):
        print(f"FAIL spectator encoding: {spec.hex()}"); fails += 1

    # Round-trip a range of seats (incl. spectator) exactly.
    for seat in (-1, 0, 1, 2, 7, 63):
        dec, pos = decode_bind(encode_bind(seat, 8))
        if pos != len(encode_bind(seat, 8)) or dec["seat"] != seat or dec["n_aircraft"] != 8:
            print(f"FAIL round-trip seat={seat}: {dec}"); fails += 1

    # Wrong version byte -> hard error.
    try:
        decode_bind(bytes([0x02, 0x00, 0x00]))
        print("FAIL wrong version should raise"); fails += 1
    except ValueError:
        pass

    # SeatPolicy: three joiners fill 0,1,2; the fourth is a spectator.
    p = SeatPolicy(3)
    seats = [p.assign() for _ in range(4)]
    if seats != [0, 1, 2, SPECTATOR]:
        print(f"FAIL join-order seats: {seats}"); fails += 1

    # release the middle seat -> the next joiner reuses the LOWEST free seat (1).
    p.release(1)
    if p.assign() != 1:
        print("FAIL seat reuse should hand back the lowest free seat"); fails += 1
    if p.assign() != SPECTATOR:            # full again
        print("FAIL should be full after reuse"); fails += 1

    # Authorization predicate.
    if not seat_authorizes(0, 0) or seat_authorizes(0, 1) or seat_authorizes(SPECTATOR, 0):
        print("FAIL seat_authorizes"); fails += 1

    if fails == 0:
        print("RESULT: BIND-001 / SeatPolicy REFERENCE SELFTEST PASS")
        return 0
    print(f"RESULT: BIND-001 / SeatPolicy REFERENCE SELFTEST FAIL ({fails})")
    return 1


if __name__ == "__main__":
    sys.exit(_selftest())
