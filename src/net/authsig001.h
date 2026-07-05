// SEADS layer-27 asymmetric-signature handshake record: HELLO-003 codec + the Ed25519 sign/verify
// over the server challenge (ATM-Sphere netcode layer 27). Mirror tools/authsig_ref.py BIT-FOR-BIT.
//
// The CHALLENGE is UNCHANGED from layer 26 — CHALLENGE-001 (authmac001.h), a fresh per-connection
// nonce derived by SipHash(session_key, accept_counter) — so only the *proof* becomes asymmetric.
// HELLO-003 replaces layer 26's symmetric-MAC HELLO-002 with an Ed25519 SIGNATURE:
//
//   HELLO-003 = [version:1 byte = 0x03] [ZigZag+LEB128 token]
//               [LEB128 siglen] [siglen raw signature bytes]
//       Client -> server, its response to the challenge. sig = Ed25519_sign(private_seed[token],
//       nonce_le || token_le) proves the client holds the PRIVATE key whose PUBLIC key is enrolled
//       for that token, and binds the proof to BOTH the challenge (freshness) and the identity.
//       token + siglen ride the sealed GEO-001 codec; the signature bytes are raw (opaque) ⇒ no new
//       codec, no det_math. Version 0x03 is distinct from HELLO-001 (0x01, layer 21) and HELLO-002
//       (0x02, layer 26), both frozen.
//
// TRANSPORT METADATA — like BIND-001 / HELLO-002 / the framing envelope, NOT a sealed rails.wire
// block: it carries no sim state, feeds no hash, uses no det_math. No seal (rides v1.26r0).
#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace seads {
namespace authsig001 {

constexpr std::uint8_t HELLO3_VERSION = 0x03;  // HELLO-003 record version

// A client identity claim + Ed25519 signature proof.
struct Hello3Info {
    std::int64_t token = 0;           // public identity
    std::vector<std::uint8_t> sig;    // Ed25519_sign(seed, nonce||token) — 64 bytes when genuine
};

void encode_hello3(const Hello3Info& h, std::vector<std::uint8_t>& out);
bool decode_hello3(const std::uint8_t* data, std::size_t len, std::size_t& pos, Hello3Info& out);

// The credential proof, shared by the client and the server/ref. The signed/verified message is
// nonce_le || token_le (16 bytes) — the same shape layer 26 MAC'd (freshness + identity).
//   sign_challenge   fills sig[64] with Ed25519_sign(seed, nonce||token).
//   verify_challenge returns true iff sig[0..siglen) verifies under pk (false unless siglen==64).
void sign_challenge(const std::uint8_t seed[32], std::uint64_t nonce, std::int64_t token,
                    std::uint8_t sig[64]);
bool verify_challenge(const std::uint8_t pk[32], std::uint64_t nonce, std::int64_t token,
                      const std::uint8_t* sig, std::size_t siglen);

}  // namespace authsig001
}  // namespace seads
