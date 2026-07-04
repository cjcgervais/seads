"""BIND-001 seat handshake + join-order SeatPolicy + upstream authorization properties (layer 18).

Layer 18 settles layer 15b's positional/unauthenticated client->aircraft binding: a join-order
SeatPolicy assigns each client a seat, a BIND-001 handshake tells it its aircraft, and the server
authorizes upstream commands (a client may steer ONLY its own seat). All pure TRANSPORT (outside the
kernel / world_hash — no det_math, no seal). Byte parity with the C++ mirror (src/net/bind001,
src/net/boundserver) is pinned by the shared known-encoding vector in tools/bound_ref.py +
src/net/netbound_test_main.cpp; here we prove the reference is self-consistent and, crucially, that:

  * BIND-001 codec round-trips every seat (incl. the SPECTATOR -1 — ZigZag carries the sign) and the
    version byte is enforced;
  * the SeatPolicy is a deterministic function of the join/leave order: seats are unique while held,
    always the lowest free index, and released seats are reused — never two live clients on one seat;
  * the AUTHORIZATION filter composes: partitioning a command set by "each aircraft's commands come
    only from that aircraft's seat" accepts exactly the whole set, while any foreign-aircraft command
    is dropped — the property that makes the kernel's frames a function of the AUTHORIZED command set.
"""
import sys
from pathlib import Path

from hypothesis import given, strategies as st

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import bound_ref as b

SEAT = st.integers(min_value=-1, max_value=63)
N = st.integers(min_value=1, max_value=16)


@given(SEAT, st.integers(min_value=0, max_value=64))
def test_bind_roundtrip(seat, n_aircraft):
    dec, pos = b.decode_bind(b.encode_bind(seat, n_aircraft))
    assert pos == len(b.encode_bind(seat, n_aircraft))
    assert dec["seat"] == seat and dec["n_aircraft"] == n_aircraft


def test_bind_known_encoding_pin():
    # the SAME bytes asserted in the C++ bridge (src/net/netbound_test_main.cpp LEG 3)
    assert b.encode_bind(1, 3) == bytes([0x01, 0x02, 0x06])
    assert b.encode_bind(b.SPECTATOR, 3) == bytes([0x01, 0x01, 0x06])  # -1 -> zz 1


@given(st.binary(min_size=0, max_size=4))
def test_bind_wrong_version_rejected(tail):
    # any leading byte != 0x01 is not a BIND-001 record
    for bad in (0x00, 0x02, 0xFF):
        try:
            b.decode_bind(bytes([bad]) + tail)
            assert False, "wrong version must raise"
        except ValueError:
            pass


@given(N, st.integers(min_value=0, max_value=32))
def test_seatpolicy_fills_in_order_then_spectator(n, extra):
    p = b.SeatPolicy(n)
    seats = [p.assign() for _ in range(n)]
    assert seats == list(range(n))                 # fill 0..n-1 in order
    for _ in range(extra):
        assert p.assign() == b.SPECTATOR           # everyone after is a spectator


@given(N, st.lists(st.integers(0, 15), max_size=32), st.randoms(use_true_random=False))
def test_seatpolicy_seats_unique_and_lowest_free(n, release_seq, rng):
    # Drive a random join/leave interleaving; INVARIANTS at every step: live seats are distinct, each
    # in [0,n), and assign() always returns the lowest free seat (or SPECTATOR when full).
    p = b.SeatPolicy(n)
    live = set()

    def assign_and_check():
        free = [i for i in range(n) if i not in live]
        got = p.assign()
        if free:
            assert got == free[0]                  # lowest free
            assert got not in live                 # never a double-booking
            live.add(got)
        else:
            assert got == b.SPECTATOR

    # a shuffled sequence of joins (assign) and leaves (release a currently-live seat)
    ops = ["join"] * (n + 3) + ["leave"] * len(release_seq)
    rng.shuffle(ops)
    for op in ops:
        if op == "join":
            assign_and_check()
        elif live:
            seat = min(live)                       # release a deterministic live seat
            p.release(seat)
            live.discard(seat)
    assert set(p.occupied()) == live               # policy state matches our shadow set


@given(st.integers(-1, 8), st.integers(0, 8))
def test_seat_authorizes(seat, aircraft):
    assert b.seat_authorizes(seat, aircraft) == (seat >= 0 and aircraft == seat)


# --- authorization composes: per-seat partition accepts the whole set; foreign commands drop --------
import input001_ref as ic  # noqa: E402


CMD = st.builds(ic._cmd, apply_tick=st.integers(0, 30), aircraft=st.integers(0, 2),
                seq=st.integers(0, 5), target_phi=st.just(0.0), target_g=st.just(1.0),
                throttle=st.just(0.0), fire=st.booleans())


@given(st.lists(CMD, max_size=24))
def test_authorization_partition_accepts_exactly_the_set(cmds):
    # Model 3 seated clients, each upstreaming ONLY its own aircraft's commands. The union of what the
    # server authorizes equals the whole set — nothing legitimate is lost, nothing foreign is added.
    n = 3
    accepted = []
    for seat in range(n):
        for c in cmds:
            if b.seat_authorizes(seat, c["aircraft"]):  # client `seat` only sends its own aircraft's
                accepted.append((seat, c["apply_tick"], c["seq"], c["aircraft"]))
    # every command whose aircraft is a real seat is authorized exactly once (by that seat); an
    # out-of-seat-range aircraft (none here, aircraft in 0..2) would be authorized by nobody.
    expect = [(c["aircraft"], c["apply_tick"], c["seq"], c["aircraft"]) for c in cmds if c["aircraft"] < n]
    assert sorted(accepted) == sorted(expect)


@given(st.integers(0, 2), CMD)
def test_foreign_aircraft_is_unauthorized(seat, cmd):
    # A command whose aircraft != the sender's seat is rejected (the layer-18 boundary).
    authorized = b.seat_authorizes(seat, cmd["aircraft"])
    assert authorized == (cmd["aircraft"] == seat)
