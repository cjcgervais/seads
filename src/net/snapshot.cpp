// SEADS GEO-001 snapshot serialization — bit-for-bit mirror of tools/snapshot_ref.py.
#include "snapshot.h"

namespace seads {
namespace netsnap {

EntityState from_kernel(int64_t id, double lat_rad, double lon_rad, double psi_rad,
                        double alt_m) {
    return EntityState{id, lat_rad * RAD2DEG, lon_rad * RAD2DEG, psi_rad * RAD2DEG, alt_m};
}

// Wire framing: header (protocol, server_tick, n) then n * (id, GeoPoint).
void encode_snapshot(const Snapshot& s, std::vector<uint8_t>& out) {
    geo001::encode_i64(s.protocol, out);
    geo001::encode_i64(s.server_tick, out);
    geo001::encode_i64(static_cast<int64_t>(s.entities.size()), out);
    for (const auto& e : s.entities) {
        geo001::encode_i64(e.id, out);
        geo001::encode_point(geo001::GeoPoint{e.lat_deg, e.lon_deg, e.bearing_deg, e.alt_m},
                             out);
    }
}

bool decode_snapshot(const uint8_t* data, size_t len, size_t& pos, Snapshot& out) {
    int64_t n = 0;
    if (!geo001::decode_i64(data, len, pos, out.protocol)) return false;
    if (!geo001::decode_i64(data, len, pos, out.server_tick)) return false;
    if (!geo001::decode_i64(data, len, pos, n)) return false;
    if (n < 0) return false;
    out.entities.clear();
    out.entities.reserve(static_cast<size_t>(n));
    for (int64_t i = 0; i < n; ++i) {
        EntityState e{};
        geo001::GeoPoint pt{};
        if (!geo001::decode_i64(data, len, pos, e.id)) return false;
        if (!geo001::decode_point(data, len, pos, pt)) return false;
        e.lat_deg = pt.lat;
        e.lon_deg = pt.lon;
        e.bearing_deg = pt.bearing;
        e.alt_m = pt.alt;
        out.entities.push_back(e);
    }
    return true;
}

}  // namespace netsnap
}  // namespace seads
