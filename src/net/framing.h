// SEADS length-prefixed stream FRAMING codec (netcode layer 7). Mirrors tools/framing_ref.py
// BIT-FOR-BIT.
//
// Layer 7 carries the EXISTING self-delimiting protocol-6 snapshot (netsnap::encode_snapshot)
// across a real byte STREAM (a TCP socket) between two OS processes. A TCP stream has no message
// boundaries — one recv() may return half a frame, or several frames plus a sliver — so each
// payload is wrapped in a strictly-OUTER length prefix and frames are reassembled from arbitrary
// chunks:
//
//     stream = concat over frames of ( LEB128(len(payload)) || payload )
//
// The length prefix is an UNSIGNED LEB128 varint (lengths are non-negative), reusing the SEALED
// geo001::leb128_encode_u64 / leb128_decode_u64 so C++ ≡ Python parity is free. layer 7 never looks
// inside the payload. NO-SEAL: kernel/det_math/rails/goldens/protocol-6 bytes are all untouched —
// this is a pure OUTER envelope + OS sockets + a determinism bridge.
//
// StreamReassembler is a PURE function of the byte stream: feed(chunk) depends only on
// (carry_buffer, chunk). For any partition of a stream S, the emitted frame sequence is identical
// to feeding S whole. The one fragile spot — the length prefix itself can split across chunks — is
// handled by buffering a partial PREFIX (truncated -> wait; overlong >10 bytes -> error), never
// emitting a frame until all len payload bytes are present.
#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace seads {
namespace framing {

// Wrap one payload as LEB128(len) || payload, appending to out.
void encode_frame(const std::uint8_t* payload, std::size_t len, std::vector<std::uint8_t>& out);
void encode_frame(const std::vector<std::uint8_t>& payload, std::vector<std::uint8_t>& out);

// Reassembles length-prefixed frames from arbitrary stream chunks. A PURE function of the
// concatenated bytes: feed() appends the frames that became complete to `out_frames` and retains
// any partial prefix/payload tail. Returns false iff the stream is malformed (overlong prefix).
class StreamReassembler {
public:
    // Append `n` bytes; push every now-complete frame (payload bytes) onto out_frames. Returns
    // false on an overlong length prefix (a malformed stream); true otherwise.
    bool feed(const std::uint8_t* data, std::size_t n,
              std::vector<std::vector<std::uint8_t>>& out_frames);

    // Bytes buffered but not yet a complete frame (a partial prefix and/or payload).
    std::size_t pending() const { return carry_.size(); }

private:
    std::vector<std::uint8_t> carry_;
};

}  // namespace framing
}  // namespace seads
