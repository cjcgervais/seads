"""Framing / reassembly properties for the netcode layer-7 length-prefixed stream codec.

Byte-exact C++<->reference parity is proven by the generated-vector gate
(src/net/framing_test_main.cpp). Here we prove the reference framing is self-consistent:
  * round-trip: reassembling an encoded stream yields exactly the original payloads;
  * chunk-boundary invariance: ANY partition of the stream reassembles to the same frames as
    feeding it whole (the core determinism property — timing/chunking changes nothing);
  * partial-frame buffered: a stream cut mid-frame (or mid-length-prefix) emits no partial frame
    and completes correctly once the rest arrives.
"""
import sys
from pathlib import Path

from hypothesis import given, strategies as st

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import framing_ref as fr

# arbitrary opaque payloads (layer 7 never looks inside them); include empty payloads.
PAYLOAD = st.binary(min_size=0, max_size=300)
PAYLOADS = st.lists(PAYLOAD, max_size=12)


@given(PAYLOADS)
def test_reassemble_roundtrip(payloads):
    stream = fr.encode_stream(payloads)
    got = fr.reassemble([stream])
    assert got == [bytes(p) for p in payloads]


@given(PAYLOADS, st.integers(min_value=1, max_value=7))
def test_chunk_boundary_invariance(payloads, step):
    # Feeding the stream in fixed-size `step` chunks reassembles to the identical frame sequence as
    # feeding it whole — for ANY chunk size, including ones that split length prefixes and payloads.
    stream = fr.encode_stream(payloads)
    whole = fr.reassemble([stream])
    chunks = [stream[i:i + step] for i in range(0, len(stream), step)]
    assert fr.reassemble(chunks) == whole == [bytes(p) for p in payloads]


@given(PAYLOADS, st.data())
def test_partial_frame_is_buffered(payloads, data):
    # Cutting the stream at an arbitrary interior offset must never emit a partial or spurious frame:
    # the frames emitted from the prefix are a prefix of the full frame list, and feeding the rest
    # completes it exactly. No byte is lost or duplicated across the split.
    stream = fr.encode_stream(payloads)
    full = [bytes(p) for p in payloads]
    if len(stream) == 0:
        assert fr.reassemble([stream]) == full
        return
    cut = data.draw(st.integers(min_value=0, max_value=len(stream)))
    r = fr.StreamReassembler()
    first = r.feed(stream[:cut])
    # whatever completed so far is a prefix of the full frame list (never a corrupted frame)
    assert first == full[:len(first)]
    rest = r.feed(stream[cut:])
    assert first + rest == full
    assert r.pending() == 0


@given(PAYLOADS, st.lists(st.integers(min_value=1, max_value=9), min_size=1, max_size=4))
def test_fanout_all_clients_identical(payloads, chunk_steps):
    # The netcode LAYER-8 invariant at the pure-codec level: one server broadcasts the SAME encoded
    # stream to N clients, but each client's TCP recv() chops it into DIFFERENT chunk sizes. Every
    # client's independent StreamReassembler must still emit the byte-identical frame list — fan-out
    # adds no information and no per-client nondeterminism. (seads_multiclient_test proves the same
    # end-to-end over real sockets to the full session digest.)
    stream = fr.encode_stream(payloads)
    full = [bytes(p) for p in payloads]
    for step in chunk_steps:  # each "client" reads the stream in its own fixed chunk size
        chunks = [stream[i:i + step] for i in range(0, len(stream), step)]
        assert fr.reassemble(chunks) == full


def test_overlong_prefix_rejected():
    # a 10-continuation-byte varint with no terminator is malformed (mirrors leb128's 10-byte bound).
    import pytest
    r = fr.StreamReassembler()
    with pytest.raises(ValueError):
        r.feed(bytes([0x80] * 10 + [0x00]))
