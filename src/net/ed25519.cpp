// SEADS Ed25519 signature primitive (netcode layer 27). See ed25519.h.
//
// A faithful transcription of TweetNaCl's Ed25519 (Bernstein/van Gastel/Janssen/Lange/
// Schwabe/Smetsers, public domain) — the minimal, portable, known-correct reference. Field
// elements are gf = i64[16] (16-bit limbs held in 64-bit words), so no __int128 is needed and the
// arithmetic is byte-identical on every toolchain/architecture. The only substitution: the hash H
// is our sha512::hash (a standard SHA-512, exactly what RFC 8032 / TweetNaCl's crypto_hash use).
// All 100% integer; no float, no libm, no det_math. Correctness is anchored by the RFC 8032
// output pins in ed25519_ref.py (confirmed against the `cryptography` library) — reproduced
// byte-for-byte here via the netauthsig bridge / property tests.
#include "ed25519.h"

#include <cstring>
#include <vector>

#include "sha512.h"

namespace seads {
namespace ed25519 {

namespace {

using u8 = std::uint8_t;
using i64 = std::int64_t;
using gf = i64[16];

const gf gf0 = {0};
const gf gf1 = {1};
const gf D = {0x78a3, 0x1359, 0x4dca, 0x75eb, 0xd8ab, 0x4141, 0x0a4d, 0x0070,
              0xe898, 0x7779, 0x4079, 0x8cc7, 0xfe73, 0x2b6f, 0x6cee, 0x5203};
const gf D2 = {0xf159, 0x26b2, 0x9b94, 0xebd6, 0xb156, 0x8283, 0x149a, 0x00e0,
               0xd130, 0xeef3, 0x80f2, 0x198e, 0xfce7, 0x56df, 0xd9dc, 0x2406};
const gf X = {0xd51a, 0x8f25, 0x2d60, 0xc956, 0xa7b2, 0x9525, 0xc760, 0x692c,
              0xdc5c, 0xfdd6, 0xe231, 0xc0a4, 0x53fe, 0xcd6e, 0x36d3, 0x2169};
const gf Y = {0x6658, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666,
              0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666};
const gf I = {0xa0b0, 0x4a0e, 0x1b27, 0xc4ee, 0xe478, 0xad2f, 0x1806, 0x2f43,
              0xd7a7, 0x3dfb, 0x0099, 0x2b4d, 0xdf0b, 0x4fc1, 0x2480, 0x2b83};

// group order L = 2^252 + 27742317777372353535851937790883648493 (little-endian bytes).
const i64 L[32] = {0xed, 0xd3, 0xf5, 0x5c, 0x1a, 0x63, 0x12, 0x58, 0xd6, 0x9c, 0xf7,
                   0xa2, 0xde, 0xf9, 0xde, 0x14, 0,    0,    0,    0,    0,    0,
                   0,    0,    0,    0,    0,    0,    0,    0,    0,    0x10};

void set25519(gf r, const gf a) {
    for (int i = 0; i < 16; ++i) r[i] = a[i];
}

void car25519(gf o) {
    for (int i = 0; i < 16; ++i) {
        o[i] += (1LL << 16);
        i64 c = o[i] >> 16;
        o[(i + 1) * (i < 15)] += c - 1 + 37 * (c - 1) * (i == 15);
        o[i] -= c << 16;
    }
}

void sel25519(gf p, gf q, int b) {
    i64 c = ~(static_cast<i64>(b) - 1);
    for (int i = 0; i < 16; ++i) {
        i64 t = c & (p[i] ^ q[i]);
        p[i] ^= t;
        q[i] ^= t;
    }
}

void pack25519(u8* o, const gf n) {
    gf m, t;
    for (int i = 0; i < 16; ++i) t[i] = n[i];
    car25519(t);
    car25519(t);
    car25519(t);
    for (int j = 0; j < 2; ++j) {
        m[0] = t[0] - 0xffed;
        int i;
        for (i = 1; i < 15; ++i) {
            m[i] = t[i] - 0xffff - ((m[i - 1] >> 16) & 1);
            m[i - 1] &= 0xffff;
        }
        m[15] = t[15] - 0x7fff - ((m[14] >> 16) & 1);
        int b = (m[15] >> 16) & 1;
        m[14] &= 0xffff;
        sel25519(t, m, 1 - b);
    }
    for (int i = 0; i < 16; ++i) {
        o[2 * i] = static_cast<u8>(t[i] & 0xff);
        o[2 * i + 1] = static_cast<u8>(t[i] >> 8);
    }
}

// constant-time 32-byte compare: true iff equal.
bool eq32(const u8* x, const u8* y) {
    unsigned d = 0;
    for (int i = 0; i < 32; ++i) d |= static_cast<unsigned>(x[i] ^ y[i]);
    return d == 0;
}

// returns true iff a != b (mirrors TweetNaCl neq25519's "packed inequality").
bool neq25519(const gf a, const gf b) {
    u8 c[32], d[32];
    pack25519(c, a);
    pack25519(d, b);
    return !eq32(c, d);
}

u8 par25519(const gf a) {
    u8 d[32];
    pack25519(d, a);
    return d[0] & 1;
}

void unpack25519(gf o, const u8* n) {
    for (int i = 0; i < 16; ++i) o[i] = n[2 * i] + (static_cast<i64>(n[2 * i + 1]) << 8);
    o[15] &= 0x7fff;
}

void A(gf o, const gf a, const gf b) {
    for (int i = 0; i < 16; ++i) o[i] = a[i] + b[i];
}
void Z(gf o, const gf a, const gf b) {
    for (int i = 0; i < 16; ++i) o[i] = a[i] - b[i];
}

void M(gf o, const gf a, const gf b) {
    i64 t[31];
    for (int i = 0; i < 31; ++i) t[i] = 0;
    for (int i = 0; i < 16; ++i)
        for (int j = 0; j < 16; ++j) t[i + j] += a[i] * b[j];
    for (int i = 0; i < 15; ++i) t[i] += 38 * t[i + 16];
    for (int i = 0; i < 16; ++i) o[i] = t[i];
    car25519(o);
    car25519(o);
}

void S(gf o, const gf a) { M(o, a, a); }

void inv25519(gf o, const gf i_in) {
    gf c;
    for (int a = 0; a < 16; ++a) c[a] = i_in[a];
    for (int a = 253; a >= 0; --a) {
        S(c, c);
        if (a != 2 && a != 4) M(c, c, i_in);
    }
    for (int a = 0; a < 16; ++a) o[a] = c[a];
}

void pow2523(gf o, const gf i_in) {
    gf c;
    for (int a = 0; a < 16; ++a) c[a] = i_in[a];
    for (int a = 250; a >= 0; --a) {
        S(c, c);
        if (a != 1) M(c, c, i_in);
    }
    for (int a = 0; a < 16; ++a) o[a] = c[a];
}

// Extended-coordinate Edwards point: p[4] = (X, Y, Z, T).
void add(gf p[4], gf q[4]) {
    gf a, b, c, d, t, e, f, g, h;
    Z(a, p[1], p[0]);
    Z(t, q[1], q[0]);
    M(a, a, t);
    A(b, p[0], p[1]);
    A(t, q[0], q[1]);
    M(b, b, t);
    M(c, p[3], q[3]);
    M(c, c, D2);
    M(d, p[2], q[2]);
    A(d, d, d);
    Z(e, b, a);
    Z(f, d, c);
    A(g, d, c);
    A(h, b, a);
    M(p[0], e, f);
    M(p[1], h, g);
    M(p[2], g, f);
    M(p[3], e, h);
}

void cswap(gf p[4], gf q[4], u8 b) {
    for (int i = 0; i < 4; ++i) sel25519(p[i], q[i], b);
}

void pack(u8* r, gf p[4]) {
    gf tx, ty, zi;
    inv25519(zi, p[2]);
    M(tx, p[0], zi);
    M(ty, p[1], zi);
    pack25519(r, ty);
    r[31] ^= par25519(tx) << 7;
}

void scalarmult(gf p[4], gf q[4], const u8* s) {
    set25519(p[0], gf0);
    set25519(p[1], gf1);
    set25519(p[2], gf1);
    set25519(p[3], gf0);
    for (int i = 255; i >= 0; --i) {
        u8 b = (s[i / 8] >> (i & 7)) & 1;
        cswap(p, q, b);
        add(q, p);
        add(p, p);
        cswap(p, q, b);
    }
}

void scalarbase(gf p[4], const u8* s) {
    gf q[4];
    set25519(q[0], X);
    set25519(q[1], Y);
    set25519(q[2], gf1);
    M(q[3], X, Y);
    scalarmult(p, q, s);
}

void modL(u8* r, i64 x[64]) {
    i64 carry;
    int i, j;
    for (i = 63; i >= 32; --i) {
        carry = 0;
        for (j = i - 32; j < i - 12; ++j) {
            x[j] += carry - 16 * x[i] * L[j - (i - 32)];
            carry = (x[j] + 128) >> 8;
            x[j] -= carry << 8;
        }
        x[j] += carry;
        x[i] = 0;
    }
    carry = 0;
    for (j = 0; j < 32; ++j) {
        x[j] += carry - (x[31] >> 4) * L[j];
        carry = x[j] >> 8;
        x[j] &= 255;
    }
    for (j = 0; j < 32; ++j) x[j] -= carry * L[j];
    for (i = 0; i < 32; ++i) {
        x[i + 1] += x[i] >> 8;
        r[i] = static_cast<u8>(x[i] & 255);
    }
}

void reduce(u8* r) {
    i64 x[64];
    for (int i = 0; i < 64; ++i) x[i] = static_cast<i64>(static_cast<u8>(r[i]));
    for (int i = 0; i < 64; ++i) r[i] = 0;
    modL(r, x);
}

// Decode a public key to the NEGATED point -A (TweetNaCl's unpackneg); returns false on failure.
bool unpackneg(gf r[4], const u8 p[32]) {
    gf t, chk, num, den, den2, den4, den6;
    set25519(r[2], gf1);
    unpack25519(r[1], p);
    S(num, r[1]);
    M(den, num, D);
    Z(num, num, r[2]);
    A(den, r[2], den);
    S(den2, den);
    S(den4, den2);
    M(den6, den4, den2);
    M(t, den6, num);
    M(t, t, den);
    pow2523(t, t);
    M(t, t, num);
    M(t, t, den);
    M(t, t, den);
    M(r[0], t, den);
    S(chk, r[0]);
    M(chk, chk, den);
    if (neq25519(chk, num)) M(r[0], r[0], I);
    S(chk, r[0]);
    M(chk, chk, den);
    if (neq25519(chk, num)) return false;
    if (par25519(r[0]) == (p[31] >> 7)) Z(r[0], gf0, r[0]);
    M(r[3], r[0], r[1]);
    return true;
}

}  // namespace

void public_key(const std::uint8_t seed[32], std::uint8_t pk[32]) {
    u8 d[64];
    sha512::hash(seed, 32, d);
    d[0] &= 248;
    d[31] &= 127;
    d[31] |= 64;
    gf p[4];
    scalarbase(p, d);
    pack(pk, p);
}

void sign(const std::uint8_t seed[32], const std::uint8_t* msg, std::size_t len,
          std::uint8_t sig[64]) {
    u8 d[64], h[64], r[64], pk[32];
    sha512::hash(seed, 32, d);
    d[0] &= 248;
    d[31] &= 127;
    d[31] |= 64;

    gf p[4];
    scalarbase(p, d);
    pack(pk, p);

    // sm = [ R(32) | prefix/pk(32) | msg ]. Build the hashed regions exactly as TweetNaCl does.
    std::vector<u8> sm(len + 64, 0);
    if (len) std::memcpy(sm.data() + 64, msg, len);
    std::memcpy(sm.data() + 32, d + 32, 32);  // prefix = upper half of the key hash

    sha512::hash(sm.data() + 32, len + 32, r);  // r = H(prefix || msg)
    reduce(r);
    scalarbase(p, r);
    pack(sm.data(), p);  // R -> sm[0..32]

    std::memcpy(sm.data() + 32, pk, 32);                    // sm[32..64] = pk
    sha512::hash(sm.data(), len + 64, h);                   // h = H(R || pk || msg)
    reduce(h);

    i64 x[64];
    for (int i = 0; i < 64; ++i) x[i] = 0;
    for (int i = 0; i < 32; ++i) x[i] = static_cast<i64>(static_cast<u8>(r[i]));
    for (int i = 0; i < 32; ++i)
        for (int j = 0; j < 32; ++j) x[i + j] += static_cast<i64>(h[i]) * static_cast<i64>(d[j]);

    u8 s_out[32];
    modL(s_out, x);  // S = (r + h*a) mod L

    for (int i = 0; i < 32; ++i) sig[i] = sm[i];       // R
    for (int i = 0; i < 32; ++i) sig[32 + i] = s_out[i];  // S
}

bool verify(const std::uint8_t pk[32], const std::uint8_t* msg, std::size_t len,
            const std::uint8_t sig[64]) {
    gf p[4], q[4];
    if (!unpackneg(q, pk)) return false;  // q = -A

    // h = H(R || pk || msg), R = sig[0..32]
    std::vector<u8> tmp(len + 64, 0);
    std::memcpy(tmp.data(), sig, 32);
    std::memcpy(tmp.data() + 32, pk, 32);
    if (len) std::memcpy(tmp.data() + 64, msg, len);
    u8 h[64];
    sha512::hash(tmp.data(), len + 64, h);
    reduce(h);

    scalarmult(p, q, h);   // p = [h](-A) = -[h]A
    scalarbase(q, sig + 32);  // q = [S]B
    add(p, q);             // p = [S]B - [h]A  (== R iff valid)
    u8 t[32];
    pack(t, p);
    return eq32(sig, t);   // R == [S]B - [h]A ?
}

}  // namespace ed25519
}  // namespace seads
