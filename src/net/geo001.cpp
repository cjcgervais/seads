// SEADS GEO-001 wire codec — bit-for-bit mirror of tools/geo001_ref.py.
#include "geo001.h"

#include <cmath>

namespace seads {
namespace geo001 {

// Round half away from zero on IEEE doubles: floor(x+0.5) for x>=0, ceil(x-0.5) for x<0.
// Identical to geo001_ref.quantize (CPython floats are C doubles, round-to-nearest-even);
// the only float op is value*scale, so the result is cross-impl reproducible.
int64_t quantize(double value, int64_t scale) {
    double s = value * static_cast<double>(scale);
    double q = (s >= 0.0) ? std::floor(s + 0.5) : std::ceil(s - 0.5);
    return static_cast<int64_t>(q);
}

double dequantize(int64_t q, int64_t scale) {
    return static_cast<double>(q) / static_cast<double>(scale);
}

uint64_t zigzag_encode(int64_t n) {
    // (n << 1) ^ (n >> 63): the right shift is arithmetic on the signed value, so it
    // yields 0 or all-ones — the standard branch-free ZigZag.
    uint64_t un = static_cast<uint64_t>(n);
    return (un << 1) ^ static_cast<uint64_t>(n >> 63);
}

int64_t zigzag_decode(uint64_t z) {
    // (z >> 1) ^ -(z & 1): the -(z&1) produces 0 or all-ones to flip the bits back.
    return static_cast<int64_t>((z >> 1) ^ (~(z & 1) + 1));
}

void leb128_encode_u64(uint64_t u, std::vector<uint8_t>& out) {
    while (true) {
        uint8_t b = static_cast<uint8_t>(u & 0x7F);
        u >>= 7;
        if (u) {
            out.push_back(b | 0x80);
        } else {
            out.push_back(b);
            return;
        }
    }
}

bool leb128_decode_u64(const uint8_t* data, size_t len, size_t& pos, uint64_t& out) {
    uint64_t result = 0;
    int shift = 0;
    for (int i = 0; i < 10; ++i) {  // ceil(64/7) = 10 bytes max
        if (pos >= len) return false;          // truncated
        uint8_t b = data[pos++];
        result |= static_cast<uint64_t>(b & 0x7F) << shift;
        if (!(b & 0x80)) {
            out = result;
            return true;
        }
        shift += 7;
    }
    return false;  // overlong (>10 bytes)
}

void encode_i64(int64_t value, std::vector<uint8_t>& out) {
    leb128_encode_u64(zigzag_encode(value), out);
}

bool decode_i64(const uint8_t* data, size_t len, size_t& pos, int64_t& out) {
    uint64_t u;
    if (!leb128_decode_u64(data, len, pos, u)) return false;
    out = zigzag_decode(u);
    return true;
}

void encode_point(const GeoPoint& p, std::vector<uint8_t>& out) {
    encode_i64(quantize(p.lat, LATLON_SCALE), out);
    encode_i64(quantize(p.lon, LATLON_SCALE), out);
    encode_i64(quantize(p.bearing, BEARING_SCALE), out);
    encode_i64(quantize(p.alt, ALT_SCALE), out);
}

bool decode_point(const uint8_t* data, size_t len, size_t& pos, GeoPoint& out) {
    int64_t lat, lon, bearing, alt;
    if (!decode_i64(data, len, pos, lat)) return false;
    if (!decode_i64(data, len, pos, lon)) return false;
    if (!decode_i64(data, len, pos, bearing)) return false;
    if (!decode_i64(data, len, pos, alt)) return false;
    out.lat = dequantize(lat, LATLON_SCALE);
    out.lon = dequantize(lon, LATLON_SCALE);
    out.bearing = dequantize(bearing, BEARING_SCALE);
    out.alt = dequantize(alt, ALT_SCALE);
    return true;
}

}  // namespace geo001
}  // namespace seads
