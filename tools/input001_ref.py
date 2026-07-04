#!/usr/bin/env python3
"""
input001_ref.py — SEADS canonical INPUT-001 UPSTREAM command wire REFERENCE (ATM-Sphere).

INPUT-001 is a SEALED RAIL (config/rails/atm.json -> rails.wire.command, seal v1.26r0). It is the
FIRST client->server wire: a client encodes a tick-stamped flight Command and sends it UP to the
authoritative server, which feeds it into the sealed kernel step (netcode layer 15b). Every wire
before it (GEO-001 / KIN-002 / WEAPON-001) is DOWNSTREAM state; this one is INPUT.

    format         = "INPUT-001"
    phi_scale      = 1000000   (1e6)   target_phi (bank, radians)   -> i64
    targetg_scale  = 1000000   (1e6)   target_g  (load factor)      -> i64
    throttle_scale = 1000000   (1e6)   throttle  ([0,1])            -> i64
    encoding       = "ZigZag+LEB128"

Field order (contract): apply_tick, aircraft, seq, target_phi, target_g, throttle, fire.
apply_tick/aircraft/seq/fire are exact integers (fire is 0/1); the three continuous fields quantize.
The codec REUSES the sealed GEO-001 pipeline (geo001_ref.{quantize,encode_i64,decode_i64}) so C++<->
Python byte-parity is inherited for free — INPUT-001 adds NO new primitive, just a field order.

DETERMINISM (see cmdqueue.h / ADR-Step-Net-Layer15b): the wire is LOSSY by quantization exactly like
the downstream wires. The guarantee is NOT that the wire is lossless but that the authoritative
kernel's output is a pure function of the DECODED, canonically-ordered command SET. `CommandQueue`
below is the reference ordering contract (drop + hold-last): a command whose apply_tick is below the
floor (already stepped past) is dropped STALE; an out-of-range aircraft is dropped OUT_OF_RANGE; the
(apply_tick, aircraft) winner is MAXIMAL under a total order on the wire-relevant fields, so it is a
pure function of the SET, never of submit order.

Usage:  python tools/input001_ref.py            # runs an internal self-test
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import geo001_ref as g

# ---- sealed INPUT-001 scales (must match config/rails/atm.json rails.wire.command) -----
PHI_SCALE      = 1_000_000   # 1e6  target_phi, radians
TARGETG_SCALE  = 1_000_000   # 1e6  target_g, load factor
THROTTLE_SCALE = 1_000_000   # 1e6  throttle, [0,1]

CMD_FIELDS = ("apply_tick", "aircraft", "seq", "target_phi", "target_g", "throttle", "fire")


def encode_command(apply_tick, aircraft, seq, target_phi, target_g, throttle, fire):
    """One INPUT-001 command -> wire bytes (field order is the wire contract)."""
    out = bytearray()
    out += g.encode_i64(int(apply_tick))
    out += g.encode_i64(int(aircraft))
    out += g.encode_i64(int(seq))
    out += g.encode_i64(g.quantize(target_phi, PHI_SCALE))
    out += g.encode_i64(g.quantize(target_g, TARGETG_SCALE))
    out += g.encode_i64(g.quantize(throttle, THROTTLE_SCALE))
    out += g.encode_i64(1 if fire else 0)
    return bytes(out)


def decode_command(data, pos=0):
    """wire bytes -> ({apply_tick,aircraft,seq,target_phi,target_g,throttle,fire}, next_pos)."""
    apply_tick, pos = g.decode_i64(data, pos)
    aircraft, pos = g.decode_i64(data, pos)
    seq, pos = g.decode_i64(data, pos)
    phi_q, pos = g.decode_i64(data, pos)
    g_q, pos = g.decode_i64(data, pos)
    thr_q, pos = g.decode_i64(data, pos)
    fire_i, pos = g.decode_i64(data, pos)
    return {
        "apply_tick": apply_tick, "aircraft": aircraft, "seq": seq,
        "target_phi": g.dequantize(phi_q, PHI_SCALE),
        "target_g": g.dequantize(g_q, TARGETG_SCALE),
        "throttle": g.dequantize(thr_q, THROTTLE_SCALE),
        "fire": bool(fire_i),
    }, pos


# ---- the ordering contract (mirror of src/net/cmdqueue.cpp) -----------------------------
def _wire_key(c):
    """Total order over the wire-relevant fields; the (tick,aircraft) winner is the max under it."""
    return (c["seq"],
            g.quantize(c["target_phi"], PHI_SCALE),
            g.quantize(c["target_g"], TARGETG_SCALE),
            g.quantize(c["throttle"], THROTTLE_SCALE),
            1 if c["fire"] else 0)


class CommandQueue:
    """Canonical tick-stamped command queue: drop (stale/oob) + hold-last, arrival-order-independent."""

    ACCEPTED, STALE, OUT_OF_RANGE = "ACCEPTED", "STALE", "OUT_OF_RANGE"

    def __init__(self, n_aircraft):
        self.n = n_aircraft
        self.floor = 0
        self.pend = {}  # (apply_tick, aircraft) -> command dict (the winner)

    def submit(self, c):
        if c["aircraft"] < 0 or c["aircraft"] >= self.n:
            return self.OUT_OF_RANGE
        if c["apply_tick"] < self.floor:
            return self.STALE
        key = (c["apply_tick"], c["aircraft"])
        cur = self.pend.get(key)
        if cur is None or _wire_key(cur) < _wire_key(c):
            self.pend[key] = c
        return self.ACCEPTED

    def peek(self, tick, aircraft):
        return self.pend.get((tick, aircraft))

    def consume(self, tick):
        for k in [k for k in self.pend if k[0] == tick]:
            del self.pend[k]
        if self.floor < tick + 1:
            self.floor = tick + 1


def _cmd(apply_tick, aircraft, seq, target_phi=0.0, target_g=1.0, throttle=0.0, fire=False):
    return {"apply_tick": apply_tick, "aircraft": aircraft, "seq": seq, "target_phi": target_phi,
            "target_g": target_g, "throttle": throttle, "fire": fire}


# ---- self-test -------------------------------------------------------------------------
def _selftest():
    fails = 0

    # Known encoding (the cross-impl pin — the SAME bytes are asserted in the C++ bridge test).
    # apply_tick=5, aircraft=1, seq=2, phi=0.5, g=1.5, throttle=0.75, fire=1.
    #   5 -> zz 10 -> 0x0a ; 1 -> 2 -> 0x02 ; 2 -> 4 -> 0x04 ;
    #   q(0.5,1e6)=500000 -> zz 1000000 -> LEB128 0xc0 0x84 0x3d
    #   q(1.5,1e6)=1500000 -> zz 3000000 -> LEB128 0xc0 0x8d 0xb7 0x01
    #   q(0.75,1e6)=750000 -> zz 1500000 -> LEB128 0xe0 0xc6 0x5b
    #   fire 1 -> zz 2 -> 0x02
    known = encode_command(5, 1, 2, 0.5, 1.5, 0.75, True)
    expect = bytes([0x0a, 0x02, 0x04,
                    0xc0, 0x84, 0x3d,
                    0xc0, 0x8d, 0xb7, 0x01,
                    0xe0, 0xc6, 0x5b,
                    0x02])
    if known != expect:
        print(f"FAIL known encoding: {known.hex()} != {expect.hex()}"); fails += 1

    # Round-trip: dyadic (grid-exact) values survive UNCHANGED; arbitrary values within one quantum.
    for c in [_cmd(0, 0, 0), _cmd(5, 1, 2, 0.5, 1.5, 0.75, True),
              _cmd(-3, 2, 7, -0.5, 2.0, 0.25, False), _cmd(1000, 7, 99, 0.0, 1.0, 1.0, True)]:
        enc = encode_command(**c)
        dec, pos = decode_command(enc)
        if pos != len(enc):
            print(f"FAIL length {c}"); fails += 1
        for k in ("apply_tick", "aircraft", "seq", "fire"):
            if dec[k] != c[k]:
                print(f"FAIL exact field {k}: {dec[k]} != {c[k]}"); fails += 1
        # dyadic values here all land exactly on the 1e6 grid => lossless
        for k, scale in (("target_phi", PHI_SCALE), ("target_g", TARGETG_SCALE),
                         ("throttle", THROTTLE_SCALE)):
            if dec[k] != c[k]:
                print(f"FAIL grid-exact {k}: {dec[k]} != {c[k]}"); fails += 1

    # CommandQueue: order independence — two permutations of the same set select the same winners.
    cmds = [_cmd(3, 0, 1, throttle=0.25), _cmd(3, 0, 5, throttle=1.0), _cmd(3, 1, 0, target_g=2.0),
            _cmd(5, 0, 0, fire=True)]
    q1, q2 = CommandQueue(2), CommandQueue(2)
    for c in cmds:
        q1.submit(c)
    for c in reversed(cmds):
        q2.submit(c)
    for key in [(3, 0), (3, 1), (5, 0)]:
        if q1.peek(*key) != q2.peek(*key):
            print(f"FAIL queue order independence at {key}"); fails += 1
    if q1.peek(3, 0)["seq"] != 5:  # max-seq winner
        print("FAIL max-seq winner"); fails += 1

    # DROP policy: stale (below floor) + out-of-range.
    q = CommandQueue(2)
    if q.submit(_cmd(5, 0, 0)) != CommandQueue.ACCEPTED:
        print("FAIL submit accepted"); fails += 1
    for t in range(6):
        q.consume(t)  # floor -> 6
    if q.submit(_cmd(5, 0, 1)) != CommandQueue.STALE or q.floor != 6:
        print("FAIL stale drop"); fails += 1
    if q.submit(_cmd(10, 2, 0)) != CommandQueue.OUT_OF_RANGE:
        print("FAIL oob drop"); fails += 1

    if fails == 0:
        print("RESULT: INPUT-001 REFERENCE SELFTEST PASS")
        return 0
    print(f"RESULT: INPUT-001 REFERENCE SELFTEST FAIL ({fails})")
    return 1


if __name__ == "__main__":
    sys.exit(_selftest())
