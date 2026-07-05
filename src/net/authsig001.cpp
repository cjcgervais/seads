// SEADS layer-27 asymmetric-signature handshake record (netcode layer 27). See authsig001.h.
#include "authsig001.h"

#include "ed25519.h"
#include "geo001.h"

namespace seads {
namespace authsig001 {

namespace {
// A u64 as 8 little-endian bytes (mirror of siphash_ref.u64_le / authmac001's put_u64_le).
inline void put_u64_le(std::uint64_t x, std::uint8_t out[8]) {
    for (int i = 0; i < 8; ++i) out[i] = static_cast<std::uint8_t>((x >> (8 * i)) & 0xFF);
}
// The 16-byte challenge message: nonce_le || token_le.
inline void challenge_message(std::uint64_t nonce, std::int64_t token, std::uint8_t msg[16]) {
    put_u64_le(nonce, msg);
    put_u64_le(static_cast<std::uint64_t>(token), msg + 8);
}
}  // namespace

void encode_hello3(const Hello3Info& h, std::vector<std::uint8_t>& out) {
    out.push_back(HELLO3_VERSION);
    geo001::encode_i64(h.token, out);
    geo001::leb128_encode_u64(h.sig.size(), out);
    out.insert(out.end(), h.sig.begin(), h.sig.end());
}

bool decode_hello3(const std::uint8_t* data, std::size_t len, std::size_t& pos, Hello3Info& out) {
    if (pos >= len || data[pos] != HELLO3_VERSION) return false;
    ++pos;
    if (!geo001::decode_i64(data, len, pos, out.token)) return false;
    std::uint64_t siglen = 0;
    if (!geo001::leb128_decode_u64(data, len, pos, siglen)) return false;
    if (pos + siglen > len) return false;  // truncated signature
    out.sig.assign(data + pos, data + pos + static_cast<std::size_t>(siglen));
    pos += static_cast<std::size_t>(siglen);
    return true;
}

void sign_challenge(const std::uint8_t seed[32], std::uint64_t nonce, std::int64_t token,
                    std::uint8_t sig[64]) {
    std::uint8_t msg[16];
    challenge_message(nonce, token, msg);
    ed25519::sign(seed, msg, sizeof(msg), sig);
}

bool verify_challenge(const std::uint8_t pk[32], std::uint64_t nonce, std::int64_t token,
                      const std::uint8_t* sig, std::size_t siglen) {
    if (siglen != 64) return false;  // an Ed25519 signature is exactly 64 bytes
    std::uint8_t msg[16];
    challenge_message(nonce, token, msg);
    return ed25519::verify(pk, msg, sizeof(msg), sig);
}

}  // namespace authsig001
}  // namespace seads
