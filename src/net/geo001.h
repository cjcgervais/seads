// SEADS GEO-001 wire codec (ATM-Sphere). Mirrors tools/geo001_ref.py BIT-FOR-BIT.
//
// GEO-001 is a SEALED RAIL (config/rails/atm.json -> world.wire): quantize geographic
// values to fixed-point i64 (lat/lon x1e7, bearing x1e6, alt x1e3), then ZigZag + LEB128.
// This is the wire/transport layer — it lives OUTSIDE the kernel and the world_hash.
// It does NOT use det_math (no transcendentals here) and never feeds bits back into the
// sim: the wire is lossy by quantization. Field order (lat,lon,bearing,alt) is contract.
#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace seads {
namespace geo001 {

// Sealed GEO-001 scales (must match config/rails/atm.json world.wire).
constexpr int64_t LATLON_SCALE  = 10000000;  // 1e7
constexpr int64_t BEARING_SCALE = 1000000;   // 1e6
constexpr int64_t ALT_SCALE     = 1000;      // 1e3

// Fixed-point quantization, round half AWAY from zero (matches geo001_ref.quantize).
int64_t quantize(double value, int64_t scale);
double  dequantize(int64_t q, int64_t scale);

// ZigZag signed<->unsigned interleave (protobuf sint64 mapping).
uint64_t zigzag_encode(int64_t n);
int64_t  zigzag_decode(uint64_t z);

// LEB128 unsigned varint. encode appends to out; decode advances pos, returns false on
// a truncated or overlong (>10 byte) varint.
void leb128_encode_u64(uint64_t u, std::vector<uint8_t>& out);
bool leb128_decode_u64(const uint8_t* data, size_t len, size_t& pos, uint64_t& out);

// Field codec: i64 fixed-point <-> wire bytes (ZigZag then LEB128).
void encode_i64(int64_t value, std::vector<uint8_t>& out);
bool decode_i64(const uint8_t* data, size_t len, size_t& pos, int64_t& out);

// GeoPoint record in canonical field order: lat, lon, bearing, alt.
struct GeoPoint {
    double lat;      // degrees
    double lon;      // degrees
    double bearing;  // degrees
    double alt;      // metres (MSL)
};

void encode_point(const GeoPoint& p, std::vector<uint8_t>& out);
bool decode_point(const uint8_t* data, size_t len, size_t& pos, GeoPoint& out);

}  // namespace geo001
}  // namespace seads
