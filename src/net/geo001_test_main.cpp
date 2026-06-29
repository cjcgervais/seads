// SEADS GEO-001 codec parity test. Asserts C++ geo001 == Python reference vectors:
// byte-identical wire on encode, exact round-trip on decode. Exit 0 PASS, 1 FAIL.
#include "geo001.h"
#include "geo001_vectors.h"

#include <cstdio>
#include <cstring>
#include <vector>

using namespace seads;

int main() {
    int fails = 0;

    // --- i64 field codec: encode byte-parity + decode round-trip ---
    for (int i = 0; i < geo001_vec::I64_VECTOR_COUNT; ++i) {
        const auto& v = geo001_vec::I64_VECTORS[i];
        std::vector<uint8_t> wire;
        geo001::encode_i64(v.value, wire);
        bool match = (static_cast<int>(wire.size()) == v.wire_len) &&
                     (std::memcmp(wire.data(), v.wire, v.wire_len) == 0);
        if (!match) {
            ++fails;
            std::printf("FAIL i64 encode %lld: got %zu bytes, expected %d\n",
                        v.value, wire.size(), v.wire_len);
        }
        size_t pos = 0;
        int64_t got = 0;
        bool ok = geo001::decode_i64(v.wire, v.wire_len, pos, got);
        if (!ok || got != v.value || pos != static_cast<size_t>(v.wire_len)) {
            ++fails;
            std::printf("FAIL i64 decode %lld: ok=%d got=%lld pos=%zu\n",
                        v.value, ok, static_cast<long long>(got), pos);
        }
    }

    // --- GeoPoint record: concatenated wire byte-parity + field round-trip ---
    for (int i = 0; i < geo001_vec::POINT_VECTOR_COUNT; ++i) {
        const auto& v = geo001_vec::POINT_VECTORS[i];
        // build the point from quantized i64 fields via dequantize, then re-encode.
        geo001::GeoPoint p{
            geo001::dequantize(v.lat, geo001::LATLON_SCALE),
            geo001::dequantize(v.lon, geo001::LATLON_SCALE),
            geo001::dequantize(v.bearing, geo001::BEARING_SCALE),
            geo001::dequantize(v.alt, geo001::ALT_SCALE)};
        std::vector<uint8_t> wire;
        geo001::encode_point(p, wire);
        bool match = (static_cast<int>(wire.size()) == v.wire_len) &&
                     (std::memcmp(wire.data(), v.wire, v.wire_len) == 0);
        if (!match) {
            ++fails;
            std::printf("FAIL point encode #%d: got %zu bytes, expected %d\n",
                        i, wire.size(), v.wire_len);
        }
        // decode the sealed wire back to the four quantized i64 fields.
        size_t pos = 0;
        int64_t lat, lon, bear, alt;
        bool ok = geo001::decode_i64(v.wire, v.wire_len, pos, lat)
               && geo001::decode_i64(v.wire, v.wire_len, pos, lon)
               && geo001::decode_i64(v.wire, v.wire_len, pos, bear)
               && geo001::decode_i64(v.wire, v.wire_len, pos, alt);
        if (!ok || lat != v.lat || lon != v.lon || bear != v.bearing || alt != v.alt
                || pos != static_cast<size_t>(v.wire_len)) {
            ++fails;
            std::printf("FAIL point decode #%d\n", i);
        }
    }

    // --- quantize: round half away from zero, exact vs reference ---
    for (int i = 0; i < geo001_vec::QUANT_VECTOR_COUNT; ++i) {
        const auto& v = geo001_vec::QUANT_VECTORS[i];
        int64_t got = geo001::quantize(v.value, v.scale);
        if (got != v.expected) {
            ++fails;
            std::printf("FAIL quantize %.17g * %lld: got %lld expected %lld\n",
                        v.value, v.scale, static_cast<long long>(got),
                        static_cast<long long>(v.expected));
        }
    }

    if (fails == 0) {
        std::printf("PASS: GEO-001 codec byte-exact vs reference (%d i64, %d point, %d quant)\n",
                    geo001_vec::I64_VECTOR_COUNT, geo001_vec::POINT_VECTOR_COUNT,
                    geo001_vec::QUANT_VECTOR_COUNT);
        return 0;
    }
    std::printf("RESULT: GEO-001 codec FAIL (%d mismatches)\n", fails);
    return 1;
}
