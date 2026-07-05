// SEADS layer-26 strong-credential handshake records (netcode layer 26). See authmac001.h.
#include "authmac001.h"

#include "geo001.h"
#include "siphash.h"

namespace seads {
namespace authmac001 {

namespace {
// A u64 as 8 little-endian bytes appended to a SipHash message buffer (mirror of siphash_ref.u64_le).
inline void put_u64_le(std::uint64_t x, std::uint8_t out[8]) {
    for (int i = 0; i < 8; ++i) out[i] = static_cast<std::uint8_t>((x >> (8 * i)) & 0xFF);
}
}  // namespace

void encode_challenge(const ChallengeInfo& c, std::vector<std::uint8_t>& out) {
    out.push_back(CHALLENGE_VERSION);
    geo001::encode_i64(static_cast<std::int64_t>(c.nonce), out);  // bit pattern via ZigZag+LEB128
}

bool decode_challenge(const std::uint8_t* data, std::size_t len, std::size_t& pos,
                      ChallengeInfo& out) {
    if (pos >= len || data[pos] != CHALLENGE_VERSION) return false;
    ++pos;
    std::int64_t n = 0;
    if (!geo001::decode_i64(data, len, pos, n)) return false;
    out.nonce = static_cast<std::uint64_t>(n);
    return true;
}

void encode_hello2(const Hello2Info& h, std::vector<std::uint8_t>& out) {
    out.push_back(HELLO2_VERSION);
    geo001::encode_i64(h.token, out);
    geo001::encode_i64(static_cast<std::int64_t>(h.mac), out);  // bit pattern via ZigZag+LEB128
}

bool decode_hello2(const std::uint8_t* data, std::size_t len, std::size_t& pos, Hello2Info& out) {
    if (pos >= len || data[pos] != HELLO2_VERSION) return false;
    ++pos;
    if (!geo001::decode_i64(data, len, pos, out.token)) return false;
    std::int64_t m = 0;
    if (!geo001::decode_i64(data, len, pos, m)) return false;
    out.mac = static_cast<std::uint64_t>(m);
    return true;
}

std::uint64_t compute_mac(std::uint64_t k0, std::uint64_t k1, std::uint64_t nonce,
                          std::int64_t token) {
    std::uint8_t msg[16];
    put_u64_le(nonce, msg);
    put_u64_le(static_cast<std::uint64_t>(token), msg + 8);
    return siphash::siphash24(k0, k1, msg, sizeof(msg));
}

std::uint64_t derive_nonce(std::uint64_t session_k0, std::uint64_t session_k1,
                           std::uint64_t counter) {
    std::uint8_t msg[8];
    put_u64_le(counter, msg);
    return siphash::siphash24(session_k0, session_k1, msg, sizeof(msg));
}

}  // namespace authmac001
}  // namespace seads
