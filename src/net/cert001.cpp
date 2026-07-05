// SEADS layer-30 certificate PKI codec (netcode layer 30). See cert001.h.
#include "cert001.h"

#include "ed25519.h"
#include "geo001.h"

namespace seads {
namespace cert001 {

void encode_cert_body(std::int64_t token, std::int64_t seat, std::int64_t epoch,
                      const std::uint8_t pubkey[32], std::vector<std::uint8_t>& out) {
    out.push_back(CERT_VERSION);
    geo001::encode_i64(token, out);
    geo001::encode_i64(seat, out);
    geo001::encode_i64(epoch, out);
    out.insert(out.end(), pubkey, pubkey + 32);
}

void issue_cert(const std::uint8_t ca_seed[32], std::int64_t token, std::int64_t seat,
                std::int64_t epoch, const std::uint8_t pubkey[32], std::vector<std::uint8_t>& out) {
    std::vector<std::uint8_t> body;
    encode_cert_body(token, seat, epoch, pubkey, body);
    std::uint8_t sig[64];
    ed25519::sign(ca_seed, body.data(), body.size(), sig);
    out.insert(out.end(), body.begin(), body.end());
    geo001::leb128_encode_u64(64, out);
    out.insert(out.end(), sig, sig + 64);
}

bool decode_cert(const std::uint8_t* data, std::size_t len, std::size_t& pos, CertInfo& out) {
    std::size_t start = pos;
    if (pos >= len || data[pos] != CERT_VERSION) return false;
    ++pos;
    if (!geo001::decode_i64(data, len, pos, out.token)) return false;
    if (!geo001::decode_i64(data, len, pos, out.seat)) return false;
    if (!geo001::decode_i64(data, len, pos, out.epoch)) return false;
    if (pos + 32 > len) return false;  // truncated pubkey
    for (int i = 0; i < 32; ++i) out.pubkey[i] = data[pos + static_cast<std::size_t>(i)];
    pos += 32;
    out.body.assign(data + start, data + pos);  // the exact bytes the CA signed
    std::uint64_t siglen = 0;
    if (!geo001::leb128_decode_u64(data, len, pos, siglen)) return false;
    if (pos + siglen > len) return false;  // truncated CA signature
    out.ca_sig.assign(data + pos, data + pos + static_cast<std::size_t>(siglen));
    pos += static_cast<std::size_t>(siglen);
    return true;
}

bool verify_cert(const std::uint8_t ca_pubkey[32], const CertInfo& c) {
    if (c.ca_sig.size() != 64) return false;  // an Ed25519 signature is exactly 64 bytes
    return ed25519::verify(ca_pubkey, c.body.data(), c.body.size(), c.ca_sig.data());
}

void encode_hello4(const Hello4Info& h, std::vector<std::uint8_t>& out) {
    out.push_back(HELLO4_VERSION);
    geo001::leb128_encode_u64(h.cert.size(), out);
    out.insert(out.end(), h.cert.begin(), h.cert.end());
    geo001::leb128_encode_u64(h.sig.size(), out);
    out.insert(out.end(), h.sig.begin(), h.sig.end());
}

bool decode_hello4(const std::uint8_t* data, std::size_t len, std::size_t& pos, Hello4Info& out) {
    if (pos >= len || data[pos] != HELLO4_VERSION) return false;
    ++pos;
    std::uint64_t certlen = 0;
    if (!geo001::leb128_decode_u64(data, len, pos, certlen)) return false;
    if (pos + certlen > len) return false;  // truncated certificate
    out.cert.assign(data + pos, data + pos + static_cast<std::size_t>(certlen));
    pos += static_cast<std::size_t>(certlen);
    std::uint64_t siglen = 0;
    if (!geo001::leb128_decode_u64(data, len, pos, siglen)) return false;
    if (pos + siglen > len) return false;  // truncated challenge signature
    out.sig.assign(data + pos, data + pos + static_cast<std::size_t>(siglen));
    pos += static_cast<std::size_t>(siglen);
    return true;
}

// ---- layer 33: HELLO-005 (certificate chain) + path validation ---------------------------------
void encode_hello5(const Hello5Info& h, std::vector<std::uint8_t>& out) {
    out.push_back(HELLO5_VERSION);
    geo001::leb128_encode_u64(h.certs.size(), out);
    for (const auto& c : h.certs) {
        geo001::leb128_encode_u64(c.size(), out);
        out.insert(out.end(), c.begin(), c.end());
    }
    geo001::leb128_encode_u64(h.sig.size(), out);
    out.insert(out.end(), h.sig.begin(), h.sig.end());
}

bool decode_hello5(const std::uint8_t* data, std::size_t len, std::size_t& pos, Hello5Info& out) {
    if (pos >= len || data[pos] != HELLO5_VERSION) return false;
    ++pos;
    std::uint64_t n = 0;
    if (!geo001::leb128_decode_u64(data, len, pos, n)) return false;
    out.certs.clear();
    for (std::uint64_t i = 0; i < n; ++i) {
        std::uint64_t certlen = 0;
        if (!geo001::leb128_decode_u64(data, len, pos, certlen)) return false;
        if (pos + certlen > len) return false;  // truncated certificate in the chain
        out.certs.emplace_back(data + pos, data + pos + static_cast<std::size_t>(certlen));
        pos += static_cast<std::size_t>(certlen);
    }
    std::uint64_t siglen = 0;
    if (!geo001::leb128_decode_u64(data, len, pos, siglen)) return false;
    if (pos + siglen > len) return false;  // truncated challenge signature
    out.sig.assign(data + pos, data + pos + static_cast<std::size_t>(siglen));
    pos += static_cast<std::size_t>(siglen);
    return true;
}

bool verify_chain(const std::uint8_t root_pubkey[32], const std::vector<CertInfo>& chain,
                  std::size_t max_depth) {
    if (chain.empty() || chain.size() > max_depth) return false;
    for (std::size_t i = 0; i < chain.size(); ++i) {
        // Each link is signed by the NEXT link's key; the top link is signed by the trusted root.
        const std::uint8_t* issuer = (i + 1 < chain.size()) ? chain[i + 1].pubkey : root_pubkey;
        if (!verify_cert(issuer, chain[i])) return false;
    }
    return true;
}

}  // namespace cert001
}  // namespace seads
