// SEADS layer-26 strong-credential handshake records: CHALLENGE-001 + HELLO-002 codecs
// (ATM-Sphere netcode layer 26). Mirror tools/authmac_ref.py BIT-FOR-BIT.
//
// Layer 21 (hello001.h) bound a client's seat to an abstracted i64 `token` the server merely
// LOOKED UP — a public username with no proof of possession. Layer 26 turns it into a keyed MAC
// over a server-issued challenge that the server VERIFIES against a pre-shared secret:
//
//   CHALLENGE-001 = [version:1 byte = 0x01] [ZigZag+LEB128 nonce]
//       Server -> client, the FIRST downstream framing frame (before BIND). The server derives
//       nonce = SipHash(session_key, accept_counter); a fresh nonce per connection defeats replay.
//
//   HELLO-002     = [version:1 byte = 0x02] [ZigZag+LEB128 token] [ZigZag+LEB128 mac]
//       Client -> server, its response. mac = SipHash(secret[token], nonce_le || token_le) proves
//       possession of the token's 128-bit secret and binds the proof to the challenge + identity.
//       Version 0x02 keeps it distinct from layer 21's token-only HELLO-001 (version 0x01), which
//       stays byte-for-byte frozen. The mac u64 rides the sealed GEO-001 i64 codec as its bit
//       pattern.
//
// Both are TRANSPORT METADATA — like BIND-001 / the layer-7 framing envelope, NOT a sealed
// rails.wire block: they carry no sim state, feed no hash, use no det_math (the integers reuse
// geo001::{encode_i64,decode_i64}; the MAC is siphash.h, pure u64 ops). No seal (rides v1.26r0).
#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace seads {
namespace authmac001 {

constexpr std::uint8_t CHALLENGE_VERSION = 0x01;  // CHALLENGE-001 record version
constexpr std::uint8_t HELLO2_VERSION = 0x02;     // HELLO-002 record version

// A server challenge (nonce carried as its i64 bit pattern through the sealed codec).
struct ChallengeInfo {
    std::uint64_t nonce = 0;
};

// A client identity claim + keyed-MAC proof.
struct Hello2Info {
    std::int64_t token = 0;   // public identity
    std::uint64_t mac = 0;    // SipHash(secret, nonce||token)
};

void encode_challenge(const ChallengeInfo& c, std::vector<std::uint8_t>& out);
bool decode_challenge(const std::uint8_t* data, std::size_t len, std::size_t& pos,
                      ChallengeInfo& out);

void encode_hello2(const Hello2Info& h, std::vector<std::uint8_t>& out);
bool decode_hello2(const std::uint8_t* data, std::size_t len, std::size_t& pos, Hello2Info& out);

// The credential proof and the challenge nonce, shared by the server and any client/ref:
//   compute_mac  = SipHash(secret, nonce_le || token_le)   — binds freshness + identity
//   derive_nonce = SipHash(session_key, counter_le)        — a fresh per-connection challenge
std::uint64_t compute_mac(std::uint64_t k0, std::uint64_t k1, std::uint64_t nonce,
                          std::int64_t token);
std::uint64_t derive_nonce(std::uint64_t session_k0, std::uint64_t session_k1,
                           std::uint64_t counter);

}  // namespace authmac001
}  // namespace seads
