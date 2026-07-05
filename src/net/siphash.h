// SEADS SipHash-2-4 keyed-MAC primitive (ATM-Sphere netcode layer 26). Mirrors
// tools/siphash_ref.py BIT-FOR-BIT.
//
// SipHash-2-4 (Aumasson & Bernstein, 2012) is a real, standard keyed pseudo-random function
// used everywhere as a short-input MAC. Netcode layer 26 uses it to turn layer 21's abstracted
// i64 "token" (HELLO-001, a public username-like identifier) into a credential the server can
// VERIFY: the client proves possession of a pre-shared 128-bit secret by MAC'ing a server-issued
// challenge nonce, and the server recomputes the MAC and compares (constant-time). A forger who
// knows only the public token cannot produce the MAC.
//
// It is reproducible cross-toolchain — the whole SEADS promise — because it is 100% 64-bit
// UNSIGNED integer arithmetic: add (mod 2^64, which std::uint64_t does by definition), xor, and
// rotate. NO floating point, NO libm, NO det_math (there are no transcendentals here, and this is
// TRANSPORT outside the kernel/world_hash — it feeds no bits back into the sim). C++ and Python
// produce byte-identical output. Pinned to the OFFICIAL SipHash-2-4 reference test vector.
#pragma once
#include <cstddef>
#include <cstdint>

namespace seads {
namespace siphash {

// SipHash-2-4 keyed MAC over `data[0..len)` with the 128-bit key (k0, k1). Returns a u64.
std::uint64_t siphash24(std::uint64_t k0, std::uint64_t k1, const std::uint8_t* data,
                        std::size_t len);

}  // namespace siphash
}  // namespace seads
