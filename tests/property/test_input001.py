"""INPUT-001 upstream command wire + CommandQueue ordering-contract properties (netcode layer 15b).

Byte-exact C++<->reference parity is inherited from the sealed GEO-001 pipeline the codec reuses (and
pinned by the shared known-encoding vector in tools/input001_ref.py + src/net/netinput_test_main.cpp).
Here we prove the reference is self-consistent and, crucially, that the ORDERING CONTRACT holds:

  * codec round-trip: decode(encode(c)) recovers the integer fields exactly and the continuous fields
    within one quantum; DYADIC (grid-exact) values round-trip losslessly (the property the socket
    bridge relies on to be byte-identical to the phase-schedule server);
  * queue ORDER INVARIANCE: submitting a command SET in ANY permutation selects the SAME (tick,
    aircraft) winners — the canonicalisation that makes the authoritative kernel's output blind to
    upstream arrival order (the layer-15b determinism claim);
  * DROP + hold-last: a command below the floor (apply_tick already stepped past) is dropped STALE; an
    out-of-range aircraft is dropped OUT_OF_RANGE; the winner is the max under the wire-field order.
"""
import sys
from pathlib import Path

from hypothesis import given, strategies as st

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import input001_ref as ic

TICK = st.integers(min_value=-1000, max_value=100000)
IDX = st.integers(min_value=0, max_value=7)
SEQ = st.integers(min_value=-1000, max_value=1000)
# arbitrary continuous values (rounded within a quantum) ...
CONT = st.floats(min_value=-3.0, max_value=3.0, allow_nan=False, allow_infinity=False)
# ... and DYADIC values that land exactly on the 1e6 grid (lossless round-trip)
DYADIC = st.sampled_from([0.0, 0.5, -0.5, 0.25, -0.25, 0.75, -0.75, 1.0, 1.5, 2.0])
FIRE = st.booleans()

_QUANTUM = 1.0 / ic.PHI_SCALE


@given(TICK, IDX, SEQ, CONT, CONT, CONT, FIRE)
def test_codec_roundtrip_within_quantum(tick, ac, seq, phi, tg, thr, fire):
    enc = ic.encode_command(tick, ac, seq, phi, tg, thr, fire)
    dec, pos = ic.decode_command(enc)
    assert pos == len(enc)
    assert (dec["apply_tick"], dec["aircraft"], dec["seq"], dec["fire"]) == (tick, ac, seq, fire)
    assert abs(dec["target_phi"] - phi) <= _QUANTUM
    assert abs(dec["target_g"] - tg) <= _QUANTUM
    assert abs(dec["throttle"] - thr) <= _QUANTUM


@given(TICK, IDX, SEQ, DYADIC, DYADIC, DYADIC, FIRE)
def test_dyadic_values_are_lossless(tick, ac, seq, phi, tg, thr, fire):
    # The socket bridge is byte-identical to build_server_frames ONLY because dyadic command values
    # survive the lossy wire UNCHANGED. Pin that: decode(encode(dyadic)) == dyadic exactly.
    dec, _ = ic.decode_command(ic.encode_command(tick, ac, seq, phi, tg, thr, fire))
    assert dec["target_phi"] == phi and dec["target_g"] == tg and dec["throttle"] == thr


CMD = st.builds(ic._cmd, apply_tick=st.integers(0, 50), aircraft=st.integers(0, 2),
                seq=st.integers(-5, 5), target_phi=DYADIC, target_g=DYADIC, throttle=DYADIC,
                fire=FIRE)


@given(st.lists(CMD, max_size=20), st.randoms(use_true_random=False))
def test_queue_order_invariance(cmds, rng):
    # Submit the SAME set in the given order and in a shuffled order; every (tick, aircraft) winner
    # must be identical — the kernel's command source is a pure function of the SET, not the order.
    shuffled = list(cmds)
    rng.shuffle(shuffled)
    qa, qb = ic.CommandQueue(3), ic.CommandQueue(3)
    for c in cmds:
        qa.submit(c)
    for c in shuffled:
        qb.submit(c)
    assert qa.pend.keys() == qb.pend.keys()
    for key in qa.pend:
        assert ic._wire_key(qa.pend[key]) == ic._wire_key(qb.pend[key])


@given(st.lists(CMD, min_size=1, max_size=12))
def test_max_seq_winner(cmds):
    # For any (tick, aircraft) key, the stored winner is the max under the wire-field order (seq-first).
    q = ic.CommandQueue(3)
    for c in cmds:
        q.submit(c)
    for (tick, ac), win in q.pend.items():
        peers = [c for c in cmds if c["apply_tick"] == tick and c["aircraft"] == ac]
        assert ic._wire_key(win) == max(ic._wire_key(c) for c in peers)


@given(st.integers(0, 50), IDX, SEQ)
def test_stale_below_floor_is_dropped(tick, ac, seq):
    q = ic.CommandQueue(8)
    for t in range(tick + 1):
        q.consume(t)  # floor -> tick+1
    assert q.floor == tick + 1
    # a command for any already-stepped tick is STALE and leaves the queue untouched
    before = dict(q.pend)
    assert q.submit(ic._cmd(tick, ac, seq)) == ic.CommandQueue.STALE
    assert q.pend == before


@given(st.integers(3, 20), st.integers(0, 50), SEQ)
def test_out_of_range_aircraft_is_dropped(n_plus, tick, seq):
    q = ic.CommandQueue(3)
    assert q.submit(ic._cmd(tick, n_plus, seq)) == ic.CommandQueue.OUT_OF_RANGE  # idx >= 3
    assert q.submit(ic._cmd(tick, -1, seq)) == ic.CommandQueue.OUT_OF_RANGE
    assert len(q.pend) == 0
