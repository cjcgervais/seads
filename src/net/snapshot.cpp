// SEADS GEO-001 snapshot serialization — bit-for-bit mirror of tools/snapshot_ref.py.
#include "snapshot.h"

namespace seads {
namespace netsnap {

EntityState from_kernel(int64_t id, double lat_rad, double lon_rad, double psi_rad,
                        double alt_m, double phi_rad, double tas_mps, double gamma_rad) {
    return EntityState{id, lat_rad * RAD2DEG, lon_rad * RAD2DEG, psi_rad * RAD2DEG, alt_m,
                       phi_rad * RAD2DEG, tas_mps, gamma_rad * RAD2DEG};
}

// Wire framing: header (protocol, server_tick, n), then the GEO section n*(id, GeoPoint),
// then — iff protocol >= 2 — the KIN section n*(id, phi_q, tas_q). Both self-delimiting; the
// GEO-001 codec is reused verbatim so its byte layout / parity vectors are untouched.
void encode_snapshot(const Snapshot& s, std::vector<uint8_t>& out) {
    geo001::encode_i64(s.protocol, out);
    geo001::encode_i64(s.server_tick, out);
    geo001::encode_i64(static_cast<int64_t>(s.entities.size()), out);
    for (const auto& e : s.entities) {  // GEO section
        geo001::encode_i64(e.id, out);
        geo001::encode_point(geo001::GeoPoint{e.lat_deg, e.lon_deg, e.bearing_deg, e.alt_m},
                             out);
    }
    if (s.protocol >= 2) {              // KIN section (auxiliary, non-geographic)
        for (const auto& e : s.entities) {
            geo001::encode_i64(e.id, out);
            geo001::encode_i64(geo001::quantize(e.phi_deg, PHI_SCALE), out);
            geo001::encode_i64(geo001::quantize(e.tas_mps, SPEED_SCALE), out);
            if (s.protocol >= 3)        // KIN-002: gamma (flight-path angle)
                geo001::encode_i64(geo001::quantize(e.gamma_deg, GAMMA_SCALE), out);
        }
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
    for (int64_t i = 0; i < n; ++i) {  // GEO section
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
    if (out.protocol >= 2) {           // KIN section
        for (int64_t i = 0; i < n; ++i) {
            int64_t kid = 0, phi_q = 0, tas_q = 0;
            if (!geo001::decode_i64(data, len, pos, kid)) return false;
            if (kid != out.entities[static_cast<size_t>(i)].id) return false;
            if (!geo001::decode_i64(data, len, pos, phi_q)) return false;
            if (!geo001::decode_i64(data, len, pos, tas_q)) return false;
            out.entities[static_cast<size_t>(i)].phi_deg = geo001::dequantize(phi_q, PHI_SCALE);
            out.entities[static_cast<size_t>(i)].tas_mps = geo001::dequantize(tas_q, SPEED_SCALE);
            if (out.protocol >= 3) {    // KIN-002: gamma (flight-path angle)
                int64_t gamma_q = 0;
                if (!geo001::decode_i64(data, len, pos, gamma_q)) return false;
                out.entities[static_cast<size_t>(i)].gamma_deg =
                    geo001::dequantize(gamma_q, GAMMA_SCALE);
            }
        }
    }
    return true;
}

}  // namespace netsnap
}  // namespace seads
