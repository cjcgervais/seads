// Red-team leg #8 (docs/dem_map_red_team.md): the ONLY thing in this tree that
// can detect a PARTIAL BAKE.
//
// The map ships as five PNGs, a 26 MB generated header and a .bin. Every stored
// direction is a pure function of the projection and every stored elevation is
// sampled from one specific DEM, so those artifacts are meaningful only TOGETHER
// — a lake dir from one bake against a height field from another is silently
// wrong, not obviously wrong. Nothing could see that, and it HAS happened:
// assets/projection.lock recorded bake_commit=5607671d6 while the assets came
// from 3d9ab1ee3. The commit that documents the mixed set is also the commit that
// claims "co-generated in a single bake, so the invariant holds" — asserted, not
// enforced.
//
// build_sudbury.py now mints one bake_id per RUN, stamps it into the lock and the
// header, and writes assets/bake_manifest.txt with a SHA-256 of every artifact.
// This leg re-checks all of it on every build, so a stale asset is a RED GATE
// rather than something Chad finds from the cockpit.
//
// KILLING MUTATION (run once, 2026-08-09): copy one PNG from the previous commit
// over assets/sudbury_landmask.png -> this leg fails on the landmask hash while
// the build, every other test, and the flown map all stay green.

#include <algorithm>
#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "render/sudbury_gis.gen.h"

#ifdef NDEBUG
#error "SEADS gate requires an assert-live build (SPEC 6.1)"
#endif

namespace {

// --- SHA-256 (FIPS 180-4). Self-contained: seads_tests links no crypto library,
// and the alternative — trusting file size or mtime — is exactly the kind of
// almost-check this leg exists to replace. Verified below against the two
// standard NIST vectors, so a broken digest cannot silently pass everything.
struct Sha256 {
    std::array<uint32_t, 8> h{0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
                              0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};
    std::array<uint8_t, 64> buf{};
    std::size_t buf_len = 0;
    uint64_t total = 0;

    static uint32_t ror(uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); }

    void block(const uint8_t* p) {
        static const uint32_t K[64] = {
            0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu,
            0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u,
            0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u,
            0xc19bf174u, 0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
            0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau, 0x983e5152u,
            0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
            0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu,
            0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
            0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u,
            0xd6990624u, 0xf40e3585u, 0x106aa070u, 0x19a4c116u, 0x1e376c08u,
            0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu,
            0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
            0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};
        uint32_t w[64];
        for (int i = 0; i < 16; ++i)
            w[i] = (uint32_t(p[i * 4]) << 24) | (uint32_t(p[i * 4 + 1]) << 16) |
                   (uint32_t(p[i * 4 + 2]) << 8) | uint32_t(p[i * 4 + 3]);
        for (int i = 16; i < 64; ++i) {
            const uint32_t s0 = ror(w[i - 15], 7) ^ ror(w[i - 15], 18) ^ (w[i - 15] >> 3);
            const uint32_t s1 = ror(w[i - 2], 17) ^ ror(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
        uint32_t e = h[4], f = h[5], g = h[6], hh = h[7];
        for (int i = 0; i < 64; ++i) {
            const uint32_t S1 = ror(e, 6) ^ ror(e, 11) ^ ror(e, 25);
            const uint32_t ch = (e & f) ^ (~e & g);
            const uint32_t t1 = hh + S1 + ch + K[i] + w[i];
            const uint32_t S0 = ror(a, 2) ^ ror(a, 13) ^ ror(a, 22);
            const uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            const uint32_t t2 = S0 + maj;
            hh = g; g = f; f = e; e = d + t1;
            d = c; c = b; b = a; a = t1 + t2;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d;
        h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }

    void update(const uint8_t* p, std::size_t n) {
        total += n;
        while (n > 0) {
            const std::size_t take = std::min(n, std::size_t(64) - buf_len);
            for (std::size_t i = 0; i < take; ++i) buf[buf_len + i] = p[i];
            buf_len += take; p += take; n -= take;
            if (buf_len == 64) { block(buf.data()); buf_len = 0; }
        }
    }

    std::string hex() {
        // Pad DIRECTLY into the buffer, never through update(): update() advances
        // `total`, and the length word we are about to write is computed from it.
        const uint64_t bits = total * 8;
        buf[buf_len++] = 0x80;                  // buf_len < 64 here: block() resets it
        if (buf_len > 56) {
            while (buf_len < 64) buf[buf_len++] = 0;
            block(buf.data());
            buf_len = 0;
        }
        while (buf_len < 56) buf[buf_len++] = 0;
        for (int i = 0; i < 8; ++i)
            buf[56 + std::size_t(i)] = uint8_t((bits >> (56 - i * 8)) & 0xff);
        block(buf.data());
        buf_len = 0;
        std::string out;
        char tmp[3];
        for (uint32_t v : h)
            for (int i = 3; i >= 0; --i) {
                std::snprintf(tmp, sizeof tmp, "%02x", (v >> (i * 8)) & 0xffu);
                out += tmp;
            }
        return out;
    }
};

std::string sha256_file(const std::string& path, bool* ok) {
    std::ifstream in(path, std::ios::binary);
    if (!in.good()) { *ok = false; return {}; }
    Sha256 s;
    std::vector<char> chunk(1 << 20);
    while (in) {
        in.read(chunk.data(), std::streamsize(chunk.size()));
        const std::streamsize got = in.gcount();
        if (got > 0) s.update(reinterpret_cast<const uint8_t*>(chunk.data()),
                              std::size_t(got));
    }
    *ok = true;
    return s.hex();
}

std::string trim(const std::string& s) {
    std::size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r')) ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r')) --b;
    return s.substr(a, b - a);
}

std::map<std::string, std::string> read_kv(const std::string& path, bool* ok) {
    std::map<std::string, std::string> out;
    std::ifstream in(path);
    if (!in.good()) { *ok = false; return out; }
    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        const std::size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string v = trim(line.substr(eq + 1));
        const std::size_t hash = v.find('#');   // strip the "# derived" comments
        if (hash != std::string::npos) v = trim(v.substr(0, hash));
        out[trim(line.substr(0, eq))] = v;
    }
    *ok = true;
    return out;
}

const std::string kRoot = std::string(SEADS_ASSET_DIR) + "/..";

}  // namespace

TEST_CASE("bake-manifest: SHA-256 self-check against the NIST vectors") {
    // A digest that is subtly wrong would agree with itself and pass every file
    // below while detecting nothing.
    Sha256 a;
    REQUIRE(a.hex() ==
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    Sha256 b;
    const std::string msg = "abc";
    b.update(reinterpret_cast<const uint8_t*>(msg.data()), msg.size());
    REQUIRE(b.hex() ==
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    Sha256 c;
    const std::string long_msg =
        "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    c.update(reinterpret_cast<const uint8_t*>(long_msg.data()), long_msg.size());
    REQUIRE(c.hex() ==
            "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
}

TEST_CASE("bake-manifest: every shipped asset came from ONE bake run") {
    bool ok = false;
    const auto man = read_kv(std::string(SEADS_ASSET_DIR) + "/bake_manifest.txt", &ok);
    INFO("assets/bake_manifest.txt is missing — re-run offline_tool/build_sudbury.py");
    REQUIRE(ok);
    REQUIRE(man.count("bake_id") == 1);

    // Every artifact the manifest names must exist and hash exactly.
    int checked = 0;
    for (const auto& [key, want] : man) {
        if (key == "bake_id" || key == "remap") continue;
        bool fok = false;
        const std::string got = sha256_file(kRoot + "/" + key, &fok);
        INFO("artifact " << key);
        REQUIRE(fok);                       // named in the manifest but not on disk
        REQUIRE(got == want);               // stale, hand-edited, or from another bake
        ++checked;
    }
    // A manifest listing nothing would pass the loop vacuously.
    REQUIRE(checked >= 6);
}

TEST_CASE("bake-manifest: lock, header and manifest agree on the bake_id") {
    bool ok = false;
    const auto man = read_kv(std::string(SEADS_ASSET_DIR) + "/bake_manifest.txt", &ok);
    REQUIRE(ok);
    const auto lock = read_kv(std::string(SEADS_ASSET_DIR) + "/projection.lock", &ok);
    REQUIRE(ok);
    REQUIRE(lock.count("bake_id") == 1);
    REQUIRE(lock.at("bake_id") == man.at("bake_id"));

    // The header's own stamp, read from the file rather than from a constant, so a
    // header regenerated without the PNGs is caught even though it compiles.
    std::ifstream hdr(kRoot + "/render/sudbury_gis.gen.h");
    REQUIRE(hdr.good());
    std::string line, stamp;
    for (int i = 0; i < 40 && std::getline(hdr, line); ++i) {
        const std::string t = trim(line);
        if (t.rfind("// bake_id=", 0) == 0) { stamp = t.substr(11); break; }
    }
    INFO("render/sudbury_gis.gen.h carries no bake_id stamp — re-bake");
    REQUIRE(!stamp.empty());
    REQUIRE(stamp == man.at("bake_id"));
}

TEST_CASE("bake-manifest: the theatrical remap range is PINNED, not drifting") {
    // Red team P1-7 / correction #5: emin/emax were the 0.5/99.5 percentiles of the
    // WHOLE DISK, so the CORE's gain was a function of the far field and moved the
    // ground under Copper Cliff and both pump anchors whenever the wilderness did.
    bool ok = false;
    const auto pin = read_kv(std::string(SEADS_ASSET_DIR) + "/remap_range.lock", &ok);
    INFO("assets/remap_range.lock is missing — the remap range is unpinned again");
    REQUIRE(ok);
    REQUIRE(pin.count("emin") == 1);
    REQUIRE(pin.count("emax") == 1);
    const double emin = std::stod(pin.at("emin"));
    const double emax = std::stod(pin.at("emax"));
    REQUIRE(emax - emin > 1.0);

    // The assets on disk must have been baked at the CURRENT pin.
    const auto man = read_kv(std::string(SEADS_ASSET_DIR) + "/bake_manifest.txt", &ok);
    REQUIRE(ok);
    REQUIRE(man.count("remap") == 1);
    const std::string& r = man.at("remap");
    const std::size_t comma = r.find(',');
    REQUIRE(comma != std::string::npos);
    INFO("the shipped assets were baked at a different remap range than the pin");
    REQUIRE(std::abs(std::stod(r.substr(0, comma)) - emin) < 1e-3);
    REQUIRE(std::abs(std::stod(r.substr(comma + 1)) - emax) < 1e-3);
}
