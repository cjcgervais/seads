// SEADS SHA-512 hash primitive (ATM-Sphere netcode layer 27). Mirrors tools/sha512_ref.py
// BIT-FOR-BIT.
//
// SHA-512 (FIPS 180-4) is the collision-resistant hash Ed25519 (RFC 8032) is built on — the
// private scalar, the deterministic per-message nonce, and the challenge scalar are all SHA-512
// outputs. Layer 27 turns layer 21's abstracted i64 "token" into an ASYMMETRIC public-key
// identity (Ed25519), which needs this hash; it lives here as a standalone, separately-pinnable
// primitive (as siphash was for the layer-26 MAC).
//
// Reproducible cross-toolchain — the whole SEADS promise — because it is 100% 64-bit UNSIGNED
// integer arithmetic (add mod 2^64, xor, and, not, shift/rotate). NO floating point, NO libm, NO
// det_math (there are no transcendentals; this is TRANSPORT outside the kernel/world_hash). C++
// and Python produce byte-identical output. Pinned to the OFFICIAL NIST vector SHA-512("abc").
#pragma once
#include <cstddef>
#include <cstdint>

namespace seads {
namespace sha512 {

// SHA-512 of data[0..len) -> the 64-byte digest in `out`.
void hash(const std::uint8_t* data, std::size_t len, std::uint8_t out[64]);

}  // namespace sha512
}  // namespace seads
