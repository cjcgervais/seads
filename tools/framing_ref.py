#!/usr/bin/env python3
"""
framing_ref.py — SEADS length-prefixed stream FRAMING codec REFERENCE (netcode layer 7).

Layer 7 carries the EXISTING self-delimiting protocol-6 snapshot (tools/snapshot_ref.py) across a
REAL byte STREAM (a TCP socket) between two OS processes. A TCP stream has no message boundaries: a
single recv() may return half a frame, or two frames plus a sliver of a third. So we wrap each
snapshot payload in a strictly-OUTER length prefix and reassemble frames from arbitrary chunks.

Wire format
-----------
    stream = concat over frames of ( LEB128(len(payload)) || payload )

where `payload` is a whole encode_snapshot() frame (already self-delimiting; layer 7 never looks
inside it). The length prefix is an UNSIGNED LEB128 varint (lengths are non-negative), reusing the
SEALED geo001_ref.leb128_encode_u64 / leb128_decode_u64 so C++ ≡ Python parity is free (no new
codec). This is a NO-SEAL change: the kernel, det_math, rails, goldens, and the protocol-6 snapshot
bytes are all untouched — layer 7 is a pure OUTER envelope + OS sockets + a determinism bridge.

StreamReassembler — a PURE function of the byte stream
------------------------------------------------------
feed(chunk) depends only on (carry_buffer, chunk). INVARIANT: for any partition C1..Cn of a stream
S, reassemble(C1..Cn) == reassemble(S as one chunk) == the frame sequence F1..Fk. Timing / chunk
sizes / coalescing change nothing — the emitted frames are byte-identical. This is what lets the
socket path reproduce the in-process session digest to the bit.

The one fragile spot: the LEB128 LENGTH PREFIX itself can split across chunks (a length needs 1..10
bytes). The reassembler buffers a partial PREFIX, not just a partial payload:
  * truncated prefix (ran out of bytes mid-varint)  -> buffer and wait for more,
  * overlong prefix (>10 bytes, no terminator)      -> ERROR (mirrors leb128_decode_u64 bit-for-bit),
and it never emits a frame until all `len` payload bytes are present, retaining the partial tail.

Usage:  python tools/framing_ref.py        # internal self-test
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import geo001_ref as g


def encode_frame(payload):
    """Wrap one payload (bytes) as LEB128(len) || payload. Length is a non-negative count, so a
    plain UNSIGNED LEB128 varint (NOT ZigZag) — matches the C++ mirror src/net/framing."""
    return g.leb128_encode_u64(len(payload)) + bytes(payload)


def encode_stream(payloads):
    """Concatenate encode_frame over an iterable of payloads -> the full stream bytes."""
    out = bytearray()
    for p in payloads:
        out += encode_frame(p)
    return bytes(out)


# ---- length-prefix decode with a three-way status (the fragile spot) -------------------
# Mirrors geo001_ref.leb128_decode_u64's 10-byte overlong limit exactly, but distinguishes
# "truncated" (buffer more) from "overlong" (hard error) so the streaming reassembler can wait
# on a split prefix without mistaking it for a malformed one.
_TRUNCATED = "truncated"
_OVERLONG = "overlong"


def _try_decode_len(buf, pos):
    """Try to read an unsigned LEB128 length from buf starting at pos.
    Returns ("ok", value, next_pos) | ("truncated",) | ("overlong",)."""
    result = 0
    shift = 0
    for i in range(10):  # ceil(64/7) = 10 bytes max — identical bound to leb128_decode_u64
        if pos >= len(buf):
            return (_TRUNCATED,)             # ran out mid-varint: wait for more bytes
        b = buf[pos]
        pos += 1
        result |= (b & 0x7F) << shift
        if not (b & 0x80):
            return ("ok", result & g._MASK64, pos)
        shift += 7
    return (_OVERLONG,)                      # 10 continuation bytes, still no terminator: malformed


class StreamReassembler:
    """Reassembles length-prefixed frames from arbitrary stream chunks. A PURE function of the
    concatenated bytes: feed(chunk) returns the list of frames (payloads, bytes) that became
    complete with this chunk, and retains any partial prefix/payload tail for the next call."""

    def __init__(self):
        self._carry = bytearray()

    def feed(self, chunk):
        """Append chunk; return [payload, ...] for every frame now complete. Raises ValueError on
        an overlong length prefix (a malformed stream)."""
        self._carry += bytes(chunk)
        frames = []
        pos = 0
        while True:
            status = _try_decode_len(self._carry, pos)
            if status[0] == _TRUNCATED:
                break                        # partial length prefix — keep the tail, wait
            if status[0] == _OVERLONG:
                raise ValueError("framing: overlong length prefix (>10 bytes)")
            _, length, hdr_end = status
            if len(self._carry) - hdr_end < length:
                break                        # length known but payload not fully arrived yet
            frames.append(bytes(self._carry[hdr_end:hdr_end + length]))
            pos = hdr_end + length
        if pos:
            del self._carry[:pos]            # drop fully-consumed frames, retain the partial tail
        return frames

    def pending(self):
        """Bytes buffered but not yet a complete frame (a partial prefix and/or payload)."""
        return len(self._carry)


def reassemble(chunks):
    """Feed each chunk in order through one reassembler; return the concatenated frame list."""
    r = StreamReassembler()
    out = []
    for c in chunks:
        out.extend(r.feed(c))
    return out


# ---- self-test -------------------------------------------------------------------------
def _chunkings(stream, prefix_ends):
    """Several fixed partitions of `stream` that stress every split point:
    whole / 1-byte / every-3-bytes / on-each-prefix-boundary / one-byte-into-each-prefix /
    a giant chunk plus a trailing sliver."""
    n = len(stream)
    parts = []
    parts.append([stream])                                          # whole
    parts.append([stream[i:i + 1] for i in range(n)])               # 1 byte at a time
    parts.append([stream[i:i + 3] for i in range(0, n, 3)])         # every 3 bytes
    # split on each frame boundary (prefix_ends holds cumulative frame-start offsets)
    cuts = sorted(set([0] + prefix_ends + [n]))
    parts.append([stream[cuts[i]:cuts[i + 1]] for i in range(len(cuts) - 1)])
    # one byte into every frame start (forces a length prefix to straddle a chunk edge)
    cuts2 = sorted(set([0] + [min(p + 1, n) for p in prefix_ends] + [n]))
    parts.append([stream[cuts2[i]:cuts2[i + 1]] for i in range(len(cuts2) - 1)])
    if n > 4:
        parts.append([stream[:n - 3], stream[n - 3:]])              # giant chunk + trailing sliver
    return parts


def _selftest():
    import snapshot_ref as s
    fails = 0

    # payload batches: empty, one small, several, and one LARGE (multi-byte LEB128 length).
    small = s.encode_snapshot(s.Snapshot(1, [s.from_kernel(1, 0.0, 0.0, 0.7853981633974483, 1000.0,
                                                            0.0, 250.0, 0.0, 150.0, 0.0, 340.0)]))
    big_ents = [s.from_kernel(i, (i * 7) % 90, (i * 13) % 180 - 90, (i * 11) % 360,
                              (i * 137) % 8000, (i * 17) % 80 - 40, (i * 29) % 320,
                              (i * 19) % 60 - 30, 100.0 + i, float(i % 5), 200.0 + 3 * i,
                              float((i % 4) - 1))
                for i in range(1, 9)]
    big_projs = [s.proj_from_kernel(j, 0.01 * j, 0.02 * j, 1.0, 1000.0 + j, 12.0, 200 - j, j % 8)
                 for j in range(60)]                                 # forces len >= 128 (2-byte prefix)
    big = s.encode_snapshot(s.Snapshot(100000, big_ents, big_projs))

    batches = [
        [],                          # empty stream
        [small],                     # one small frame
        [small, small, small],       # several small frames
        [big],                       # one large frame (multi-byte length prefix)
        [small, big, small, big],    # mixed
    ]

    for bi, payloads in enumerate(batches):
        stream = encode_stream(payloads)
        # frame-start offsets, for the boundary chunkings
        offs, pos = [], 0
        for p in payloads:
            offs.append(pos)
            pos += len(g.leb128_encode_u64(len(p))) + len(p)

        # every chunking yields byte-identical frames == the original payloads
        for ci, parts in enumerate(_chunkings(stream, offs)):
            assert b"".join(parts) == stream, "chunking must partition the stream"
            got = reassemble(parts)
            if got != [bytes(p) for p in payloads]:
                print(f"FAIL batch {bi} chunking {ci}: {len(got)} frames != {len(payloads)}")
                fails += 1

        # each reassembled frame decodes back to a valid snapshot (payloads untouched)
        got = reassemble([stream])
        for p_out, p_in in zip(got, payloads):
            dec, dpos = s.decode_snapshot(p_out)
            if dpos != len(p_out) or dec.protocol != s.SNAPSHOT_PROTOCOL:
                print(f"FAIL batch {bi}: reassembled payload is not a clean protocol-6 frame")
                fails += 1

    # partial-frame-buffered: feed a stream truncated MID-PAYLOAD -> zero frames, tail retained.
    stream = encode_stream([big])
    r = StreamReassembler()
    got = r.feed(stream[:len(stream) - 5])
    if got or r.pending() != len(stream) - 5:
        print("FAIL partial payload should buffer with no frame emitted"); fails += 1
    got = r.feed(stream[len(stream) - 5:])
    if got != [big] or r.pending() != 0:
        print("FAIL completing the payload should emit exactly the buffered frame"); fails += 1

    # partial-PREFIX-buffered: a large payload's length needs 2 bytes; feed only the first.
    stream = encode_stream([big])
    r = StreamReassembler()
    got = r.feed(stream[:1])             # one byte of a multi-byte length prefix (0x80-set)
    if got or r.pending() != 1:
        print("FAIL partial length prefix should buffer with no frame emitted"); fails += 1
    got = r.feed(stream[1:])
    if got != [big]:
        print("FAIL completing a split length prefix should emit the frame"); fails += 1

    # overlong length prefix -> hard error (10 continuation bytes, no terminator).
    r = StreamReassembler()
    try:
        r.feed(bytes([0x80] * 10 + [0x00]))
        print("FAIL overlong prefix should raise"); fails += 1
    except ValueError:
        pass

    # trailing bytes AFTER complete frames stay buffered (not misread as a frame).
    stream = encode_stream([small, small])
    r = StreamReassembler()
    got = r.feed(stream + b"\x80")        # a dangling continuation byte after two whole frames
    if got != [small, small] or r.pending() != 1:
        print("FAIL trailing partial prefix should buffer after complete frames"); fails += 1

    if fails == 0:
        print("RESULT: FRAMING REFERENCE SELFTEST PASS "
              f"({len(batches)} batches; chunk-boundary invariance + split prefix/payload + overlong)")
        return 0
    print(f"RESULT: FRAMING REFERENCE SELFTEST FAIL ({fails})")
    return 1


if __name__ == "__main__":
    sys.exit(_selftest())
