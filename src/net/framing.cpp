// SEADS length-prefixed stream framing codec (netcode layer 7). Mirrors tools/framing_ref.py.
#include "framing.h"

#include "geo001.h"  // leb128_encode_u64 / leb128_decode_u64 (the sealed varint — reused verbatim)

namespace seads {
namespace framing {

void encode_frame(const std::uint8_t* payload, std::size_t len, std::vector<std::uint8_t>& out) {
    geo001::leb128_encode_u64(static_cast<std::uint64_t>(len), out);
    out.insert(out.end(), payload, payload + len);
}

void encode_frame(const std::vector<std::uint8_t>& payload, std::vector<std::uint8_t>& out) {
    encode_frame(payload.data(), payload.size(), out);
}

namespace {

// Three-way length-prefix decode: mirrors geo001::leb128_decode_u64's 10-byte overlong bound but
// distinguishes "truncated" (need more bytes) from "overlong" (malformed) so the reassembler can
// wait on a split prefix without mistaking it for a bad one. Returns 0=truncated, 1=ok, 2=overlong;
// on ok, sets `value` and `next_pos`.
enum LenStatus { LEN_TRUNCATED = 0, LEN_OK = 1, LEN_OVERLONG = 2 };

LenStatus try_decode_len(const std::vector<std::uint8_t>& buf, std::size_t pos,
                         std::uint64_t& value, std::size_t& next_pos) {
    std::uint64_t result = 0;
    int shift = 0;
    for (int i = 0; i < 10; ++i) {  // ceil(64/7) = 10 — identical bound to leb128_decode_u64
        if (pos >= buf.size()) return LEN_TRUNCATED;
        std::uint8_t b = buf[pos++];
        result |= static_cast<std::uint64_t>(b & 0x7F) << shift;
        if (!(b & 0x80)) {
            value = result;
            next_pos = pos;
            return LEN_OK;
        }
        shift += 7;
    }
    return LEN_OVERLONG;
}

}  // namespace

bool StreamReassembler::feed(const std::uint8_t* data, std::size_t n,
                             std::vector<std::vector<std::uint8_t>>& out_frames) {
    carry_.insert(carry_.end(), data, data + n);
    std::size_t pos = 0;
    while (true) {
        std::uint64_t length = 0;
        std::size_t hdr_end = 0;
        LenStatus st = try_decode_len(carry_, pos, length, hdr_end);
        if (st == LEN_TRUNCATED) break;                  // partial length prefix — keep the tail
        if (st == LEN_OVERLONG) return false;            // malformed stream
        if (carry_.size() - hdr_end < length) break;     // payload not fully arrived yet
        out_frames.emplace_back(carry_.begin() + static_cast<std::ptrdiff_t>(hdr_end),
                                carry_.begin() + static_cast<std::ptrdiff_t>(hdr_end + length));
        pos = hdr_end + static_cast<std::size_t>(length);
    }
    if (pos) carry_.erase(carry_.begin(), carry_.begin() + static_cast<std::ptrdiff_t>(pos));
    return true;
}

}  // namespace framing
}  // namespace seads
