// SEADS flight recording container (.seadsrec) — see seadsrec.h. Plain little-endian framing
// around the sealed netsnap wire codec; no det_math, no kernel (pure transport/presentation).
#include "seadsrec.h"

#include <cstring>

namespace seads {
namespace client {

namespace {

void put_u32(std::vector<uint8_t>& out, uint32_t v) {
    out.push_back(static_cast<uint8_t>(v & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
}

void put_f64(std::vector<uint8_t>& out, double v) {
    uint64_t bits;
    std::memcpy(&bits, &v, sizeof(bits));  // raw IEEE-754 bit copy (presentation, not hashed)
    for (int i = 0; i < 8; ++i) out.push_back(static_cast<uint8_t>((bits >> (8 * i)) & 0xFF));
}

bool get_u32(const uint8_t* data, size_t len, size_t& pos, uint32_t& v) {
    if (pos + 4 > len) return false;
    v = static_cast<uint32_t>(data[pos]) | (static_cast<uint32_t>(data[pos + 1]) << 8) |
        (static_cast<uint32_t>(data[pos + 2]) << 16) | (static_cast<uint32_t>(data[pos + 3]) << 24);
    pos += 4;
    return true;
}

bool get_f64(const uint8_t* data, size_t len, size_t& pos, double& v) {
    if (pos + 8 > len) return false;
    uint64_t bits = 0;
    for (int i = 0; i < 8; ++i) bits |= static_cast<uint64_t>(data[pos + i]) << (8 * i);
    pos += 8;
    std::memcpy(&v, &bits, sizeof(v));
    return true;
}

}  // namespace

void write_recording(const RecordingMeta& meta,
                     const std::vector<std::vector<uint8_t>>& frames,
                     std::vector<uint8_t>& out) {
    out.clear();
    out.insert(out.end(), SEADSREC_MAGIC, SEADSREC_MAGIC + 8);
    put_u32(out, meta.version);
    put_u32(out, meta.protocol);
    put_u32(out, meta.tick_hz);
    put_u32(out, meta.snap_hz);
    put_f64(out, meta.radius_m);
    put_u32(out, static_cast<uint32_t>(frames.size()));
    for (const auto& f : frames) {
        put_u32(out, static_cast<uint32_t>(f.size()));
        out.insert(out.end(), f.begin(), f.end());
    }
}

bool read_recording(const uint8_t* data, size_t len, Recording& out) {
    out.frames.clear();
    size_t pos = 0;
    if (len < 8 || std::memcmp(data, SEADSREC_MAGIC, 8) != 0) return false;
    pos = 8;
    uint32_t n = 0;
    if (!get_u32(data, len, pos, out.meta.version)) return false;
    if (!get_u32(data, len, pos, out.meta.protocol)) return false;
    if (!get_u32(data, len, pos, out.meta.tick_hz)) return false;
    if (!get_u32(data, len, pos, out.meta.snap_hz)) return false;
    if (!get_f64(data, len, pos, out.meta.radius_m)) return false;
    if (!get_u32(data, len, pos, n)) return false;
    out.meta.n_frames = n;
    out.frames.reserve(n);
    for (uint32_t i = 0; i < n; ++i) {
        uint32_t plen = 0;
        if (!get_u32(data, len, pos, plen)) return false;
        if (pos + plen > len) return false;
        size_t fpos = pos;
        netsnap::Snapshot snap;
        if (!netsnap::decode_snapshot(data, pos + plen, fpos, snap)) return false;
        out.frames.push_back(std::move(snap));
        pos += plen;
    }
    return true;
}

}  // namespace client
}  // namespace seads
