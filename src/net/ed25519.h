// SEADS Ed25519 signature primitive (ATM-Sphere netcode layer 27). Mirrors tools/ed25519_ref.py
// (the RFC 8032 reference algorithm) in OUTPUT bytes.
//
// Ed25519 (RFC 8032) is the modern standard PUBLIC-KEY signature. Layer 27 uses it to turn layer
// 21's abstracted i64 "token" into an ASYMMETRIC identity: a client holds a 32-byte PRIVATE seed;
// the server holds ONLY the derived 32-byte PUBLIC key. The client proves possession by SIGNING a
// fresh server challenge; the server VERIFIES with the public key alone. Headline over layer 26's
// symmetric SipHash MAC: the server holds no secret capable of signing, so a full roster leak
// (every enrolled public key) cannot impersonate any client.
//
// This C++ side is a transcription of TweetNaCl's known-correct, minimal, PORTABLE Ed25519 — a
// fixed 16-limb (gf[16]) bignum, so it needs no __int128 and is byte-identical across MSVC / Clang
// / GCC on x64 / AArch64 (100% integer arithmetic; no float, no libm, no det_math — TRANSPORT
// outside the world_hash). Its hash H is our sha512.h. Ed25519 signing is DETERMINISTIC, so a
// given (seed, message) yields ONE canonical 64-byte signature identical to the RFC 8032 Python
// reference (pinned in both, and confirmed against the `cryptography` library). No seal (rides
// v1.26r0).
#pragma once
#include <cstddef>
#include <cstdint>

namespace seads {
namespace ed25519 {

// Derive the 32-byte Ed25519 public key from a 32-byte private seed.
void public_key(const std::uint8_t seed[32], std::uint8_t pk[32]);

// Deterministically sign msg[0..len) with the 32-byte seed -> a 64-byte detached signature.
void sign(const std::uint8_t seed[32], const std::uint8_t* msg, std::size_t len,
          std::uint8_t sig[64]);

// Verify: true iff sig[0..64) is a valid Ed25519 signature on msg[0..len) under public key pk.
bool verify(const std::uint8_t pk[32], const std::uint8_t* msg, std::size_t len,
            const std::uint8_t sig[64]);

}  // namespace ed25519
}  // namespace seads
