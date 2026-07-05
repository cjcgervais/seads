// SEADS SipHash-2-4 keyed-MAC primitive (netcode layer 26). See siphash.h.
//
// A direct transcription of the SipHash-2-4 reference (c=2 compression rounds, d=4 finalization
// rounds), byte-for-byte equivalent to tools/siphash_ref.py. All arithmetic is std::uint64_t,
// which wraps mod 2^64 by definition ⇒ identical on every toolchain/architecture. No FMA, no
// float, no libm — this is transport, not kernel math.
#include "siphash.h"

namespace seads {
namespace siphash {

namespace {

inline std::uint64_t rotl(std::uint64_t x, int b) {
    return (x << b) | (x >> (64 - b));
}

inline void sipround(std::uint64_t& v0, std::uint64_t& v1, std::uint64_t& v2, std::uint64_t& v3) {
    v0 += v1; v1 = rotl(v1, 13); v1 ^= v0; v0 = rotl(v0, 32);
    v2 += v3; v3 = rotl(v3, 16); v3 ^= v2;
    v0 += v3; v3 = rotl(v3, 21); v3 ^= v0;
    v2 += v1; v1 = rotl(v1, 17); v1 ^= v2; v2 = rotl(v2, 32);
}

}  // namespace

std::uint64_t siphash24(std::uint64_t k0, std::uint64_t k1, const std::uint8_t* data,
                        std::size_t len) {
    std::uint64_t v0 = 0x736F6D6570736575ULL ^ k0;
    std::uint64_t v1 = 0x646F72616E646F6DULL ^ k1;
    std::uint64_t v2 = 0x6C7967656E657261ULL ^ k0;
    std::uint64_t v3 = 0x7465646279746573ULL ^ k1;

    const std::size_t end = len - (len % 8);
    for (std::size_t off = 0; off < end; off += 8) {
        std::uint64_t m = 0;
        for (int i = 0; i < 8; ++i)
            m |= static_cast<std::uint64_t>(data[off + i]) << (8 * i);  // little-endian
        v3 ^= m;
        sipround(v0, v1, v2, v3);
        sipround(v0, v1, v2, v3);
        v0 ^= m;
    }

    // last block: the remaining <8 bytes in the low bytes, with (len & 0xff) in the top byte.
    std::uint64_t b = static_cast<std::uint64_t>(len & 0xFF) << 56;
    for (std::size_t i = end; i < len; ++i)
        b |= static_cast<std::uint64_t>(data[i]) << (8 * (i - end));
    v3 ^= b;
    sipround(v0, v1, v2, v3);
    sipround(v0, v1, v2, v3);
    v0 ^= b;

    v2 ^= 0xFF;
    sipround(v0, v1, v2, v3);
    sipround(v0, v1, v2, v3);
    sipround(v0, v1, v2, v3);
    sipround(v0, v1, v2, v3);
    return v0 ^ v1 ^ v2 ^ v3;
}

}  // namespace siphash
}  // namespace seads
