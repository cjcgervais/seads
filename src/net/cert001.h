// SEADS layer-30 certificate PKI codec: CERT-001 (a CA-signed certificate) + HELLO-004 (present the
// certificate + sign the challenge) + issue/verify (ATM-Sphere netcode layer 30). Mirrors
// tools/cert_ref.py BIT-FOR-BIT.
//
// Layer 27 (authsig001.h) bound each seat to a per-token PUBLIC key ENROLLED on the server. Real
// public-key identity, but the roster is FIXED — no enrollment without a server change, no
// revocation, no key rotation. Layer 30 moves trust to a CERTIFICATE AUTHORITY: the server trusts
// ONE CA public key, and each client carries a CA-signed certificate binding its identity.
//
//   CERT-001 body = [cert_version:1 byte = 0x01] [ZigZag+LEB128 token] [ZigZag+LEB128 seat]
//                   [ZigZag+LEB128 epoch] [32 raw client-pubkey bytes]
//   certificate   = cert_body [LEB128 ca_siglen = 64] [64 raw CA signature bytes]
//       ca_sig = Ed25519_sign(ca_seed, cert_body) — the CA vouches for (token, seat, epoch, pubkey).
//       token/seat/epoch ride the sealed GEO-001 codec; the pubkey + CA signature are raw ⇒ no new
//       codec primitive, no det_math.
//
//   HELLO-004 = [version:1 byte = 0x04] [LEB128 certlen] [cert bytes]
//               [LEB128 siglen = 64] [64 raw challenge-signature bytes]
//       Client -> server, its challenge response: present the certificate AND prove possession of
//       the certified private key by signing (nonce_le || token_le) — layer 27's proof reused. The
//       CHALLENGE (CHALLENGE-001 + derive_nonce) is UNCHANGED. Version 0x04 is distinct from
//       HELLO-001/002/003 (0x01/0x02/0x03), all frozen.
//
// TRANSPORT METADATA — like BIND-001 / HELLO-003 / the framing envelope, NOT a sealed rails.wire
// block: it carries no sim state, feeds no hash, uses no det_math. No seal (rides v1.26r0).
#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace seads {
namespace cert001 {

constexpr std::uint8_t CERT_VERSION = 0x01;    // CERT-001 certificate body version
constexpr std::uint8_t HELLO4_VERSION = 0x04;  // HELLO-004 handshake record version

// A decoded certificate: the CA-vouched identity binding + the CA signature. `body` is the exact
// byte slice the CA signed (so verify hashes the literal prefix, never a re-encoding).
struct CertInfo {
    std::int64_t token = 0;
    std::int64_t seat = 0;
    std::int64_t epoch = 0;
    std::uint8_t pubkey[32] = {0};       // the certified client public key
    std::vector<std::uint8_t> ca_sig;    // CA signature over `body` (64 bytes when genuine)
    std::vector<std::uint8_t> body;      // the signed prefix bytes (version+token+seat+epoch+pubkey)
};

// A decoded HELLO-004: the presented certificate blob + the challenge signature (both opaque here).
struct Hello4Info {
    std::vector<std::uint8_t> cert;      // the full CERT-001 certificate bytes
    std::vector<std::uint8_t> sig;       // Ed25519_sign(client_seed, nonce||token) — 64 bytes
};

// Build the canonical certificate body the CA signs (version + token + seat + epoch + 32-byte pubkey).
void encode_cert_body(std::int64_t token, std::int64_t seat, std::int64_t epoch,
                      const std::uint8_t pubkey[32], std::vector<std::uint8_t>& out);
// CA side: sign a fresh certificate with the 32-byte CA seed. Returns body + LEB siglen + CA sig.
void issue_cert(const std::uint8_t ca_seed[32], std::int64_t token, std::int64_t seat,
                std::int64_t epoch, const std::uint8_t pubkey[32], std::vector<std::uint8_t>& out);
bool decode_cert(const std::uint8_t* data, std::size_t len, std::size_t& pos, CertInfo& out);
// True iff the certificate's CA signature verifies under `ca_pubkey` over its body.
bool verify_cert(const std::uint8_t ca_pubkey[32], const CertInfo& c);

void encode_hello4(const Hello4Info& h, std::vector<std::uint8_t>& out);
bool decode_hello4(const std::uint8_t* data, std::size_t len, std::size_t& pos, Hello4Info& out);

}  // namespace cert001
}  // namespace seads
