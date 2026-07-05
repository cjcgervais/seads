// SEADS SHA-512 hash primitive (netcode layer 27). See sha512.h.
//
// A direct transcription of FIPS 180-4, byte-for-byte equivalent to tools/sha512_ref.py. All
// arithmetic is std::uint64_t, which wraps mod 2^64 by definition => identical on every
// toolchain/architecture. No FMA, no float, no libm — this is transport, not kernel math.
#include "sha512.h"

namespace seads {
namespace sha512 {

namespace {

// First 64 bits of the fractional parts of the cube roots of the first 80 primes.
const std::uint64_t K[80] = {
    0x428A2F98D728AE22ULL, 0x7137449123EF65CDULL, 0xB5C0FBCFEC4D3B2FULL, 0xE9B5DBA58189DBBCULL,
    0x3956C25BF348B538ULL, 0x59F111F1B605D019ULL, 0x923F82A4AF194F9BULL, 0xAB1C5ED5DA6D8118ULL,
    0xD807AA98A3030242ULL, 0x12835B0145706FBEULL, 0x243185BE4EE4B28CULL, 0x550C7DC3D5FFB4E2ULL,
    0x72BE5D74F27B896FULL, 0x80DEB1FE3B1696B1ULL, 0x9BDC06A725C71235ULL, 0xC19BF174CF692694ULL,
    0xE49B69C19EF14AD2ULL, 0xEFBE4786384F25E3ULL, 0x0FC19DC68B8CD5B5ULL, 0x240CA1CC77AC9C65ULL,
    0x2DE92C6F592B0275ULL, 0x4A7484AA6EA6E483ULL, 0x5CB0A9DCBD41FBD4ULL, 0x76F988DA831153B5ULL,
    0x983E5152EE66DFABULL, 0xA831C66D2DB43210ULL, 0xB00327C898FB213FULL, 0xBF597FC7BEEF0EE4ULL,
    0xC6E00BF33DA88FC2ULL, 0xD5A79147930AA725ULL, 0x06CA6351E003826FULL, 0x142929670A0E6E70ULL,
    0x27B70A8546D22FFCULL, 0x2E1B21385C26C926ULL, 0x4D2C6DFC5AC42AEDULL, 0x53380D139D95B3DFULL,
    0x650A73548BAF63DEULL, 0x766A0ABB3C77B2A8ULL, 0x81C2C92E47EDAEE6ULL, 0x92722C851482353BULL,
    0xA2BFE8A14CF10364ULL, 0xA81A664BBC423001ULL, 0xC24B8B70D0F89791ULL, 0xC76C51A30654BE30ULL,
    0xD192E819D6EF5218ULL, 0xD69906245565A910ULL, 0xF40E35855771202AULL, 0x106AA07032BBD1B8ULL,
    0x19A4C116B8D2D0C8ULL, 0x1E376C085141AB53ULL, 0x2748774CDF8EEB99ULL, 0x34B0BCB5E19B48A8ULL,
    0x391C0CB3C5C95A63ULL, 0x4ED8AA4AE3418ACBULL, 0x5B9CCA4F7763E373ULL, 0x682E6FF3D6B2B8A3ULL,
    0x748F82EE5DEFB2FCULL, 0x78A5636F43172F60ULL, 0x84C87814A1F0AB72ULL, 0x8CC702081A6439ECULL,
    0x90BEFFFA23631E28ULL, 0xA4506CEBDE82BDE9ULL, 0xBEF9A3F7B2C67915ULL, 0xC67178F2E372532BULL,
    0xCA273ECEEA26619CULL, 0xD186B8C721C0C207ULL, 0xEADA7DD6CDE0EB1EULL, 0xF57D4F7FEE6ED178ULL,
    0x06F067AA72176FBAULL, 0x0A637DC5A2C898A6ULL, 0x113F9804BEF90DAEULL, 0x1B710B35131C471BULL,
    0x28DB77F523047D84ULL, 0x32CAAB7B40C72493ULL, 0x3C9EBE0A15C9BEBCULL, 0x431D67C49C100D4CULL,
    0x4CC5D4BECB3E42B6ULL, 0x597F299CFC657E2AULL, 0x5FCB6FAB3AD6FAECULL, 0x6C44198C4A475817ULL,
};

inline std::uint64_t rotr(std::uint64_t x, int n) {
    return (x >> n) | (x << (64 - n));
}

inline std::uint64_t be64(const std::uint8_t* p) {
    std::uint64_t v = 0;
    for (int i = 0; i < 8; ++i) v = (v << 8) | p[i];
    return v;
}

}  // namespace

void hash(const std::uint8_t* data, std::size_t len, std::uint8_t out[64]) {
    std::uint64_t h[8] = {
        0x6A09E667F3BCC908ULL, 0xBB67AE8584CAA73BULL, 0x3C6EF372FE94F82BULL, 0xA54FF53A5F1D36F1ULL,
        0x510E527FADE682D1ULL, 0x9B05688C2B3E6C1FULL, 0x1F83D9ABFB41BD6BULL, 0x5BE0CD19137E2179ULL,
    };

    // Process every full 128-byte block that lies entirely within the message, then a padded tail.
    // Padding: 0x80, zeros to 112 mod 128, then the 128-bit big-endian bit length. len < 2^61 so
    // the high 64 bits of the bit length are zero.
    auto process = [&](const std::uint8_t* blk) {
        std::uint64_t w[80];
        for (int i = 0; i < 16; ++i) w[i] = be64(blk + i * 8);
        for (int i = 16; i < 80; ++i) {
            std::uint64_t s0 = rotr(w[i - 15], 1) ^ rotr(w[i - 15], 8) ^ (w[i - 15] >> 7);
            std::uint64_t s1 = rotr(w[i - 2], 19) ^ rotr(w[i - 2], 61) ^ (w[i - 2] >> 6);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        std::uint64_t a = h[0], b = h[1], c = h[2], d = h[3];
        std::uint64_t e = h[4], f = h[5], g = h[6], hh = h[7];
        for (int i = 0; i < 80; ++i) {
            std::uint64_t S1 = rotr(e, 14) ^ rotr(e, 18) ^ rotr(e, 41);
            std::uint64_t ch = (e & f) ^ (~e & g);
            std::uint64_t t1 = hh + S1 + ch + K[i] + w[i];
            std::uint64_t S0 = rotr(a, 28) ^ rotr(a, 34) ^ rotr(a, 39);
            std::uint64_t maj = (a & b) ^ (a & c) ^ (b & c);
            std::uint64_t t2 = S0 + maj;
            hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d;
        h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    };

    std::size_t full = len - (len % 128);
    for (std::size_t off = 0; off < full; off += 128) process(data + off);

    // Assemble the final 1 or 2 padded blocks from the <128-byte remainder.
    std::uint8_t tail[256];
    std::size_t rem = len - full;
    for (std::size_t i = 0; i < rem; ++i) tail[i] = data[full + i];
    std::size_t tlen = (rem < 112) ? 128 : 256;
    tail[rem] = 0x80;
    for (std::size_t i = rem + 1; i < tlen; ++i) tail[i] = 0x00;
    std::uint64_t bits = static_cast<std::uint64_t>(len) * 8ULL;  // len < 2^61 => high 64 bits zero
    for (int i = 0; i < 8; ++i)
        tail[tlen - 1 - i] = static_cast<std::uint8_t>((bits >> (8 * i)) & 0xFF);
    // (bytes tlen-16 .. tlen-9 stay zero: the high 64 bits of the 128-bit length)
    for (std::size_t off = 0; off < tlen; off += 128) process(tail + off);

    for (int i = 0; i < 8; ++i)
        for (int j = 0; j < 8; ++j)
            out[i * 8 + j] = static_cast<std::uint8_t>((h[i] >> (56 - 8 * j)) & 0xFF);
}

}  // namespace sha512
}  // namespace seads
