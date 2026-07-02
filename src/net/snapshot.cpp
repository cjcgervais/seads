// SEADS GEO-001 snapshot serialization — bit-for-bit mirror of tools/snapshot_ref.py.
#include "snapshot.h"

namespace seads {
namespace netsnap {

EntityState from_kernel(int64_t id, double lat_rad, double lon_rad, double psi_rad,
                        double alt_m, double phi_rad, double tas_mps, double gamma_rad,
                        double hp, double fire_cd, double ammo, double last_hit_by,
                        double engine_hp, double wing_hp, double tail_hp, double kills) {
    return EntityState{id, lat_rad * RAD2DEG, lon_rad * RAD2DEG, psi_rad * RAD2DEG, alt_m,
                       phi_rad * RAD2DEG, tas_mps, gamma_rad * RAD2DEG, hp, fire_cd, ammo,
                       last_hit_by, engine_hp, wing_hp, tail_hp, kills};
}

ProjectileState proj_from_kernel(int64_t id, double lat_rad, double lon_rad, double psi_rad,
                                 double alt_m, double damage, int64_t ttl, int64_t owner) {
    return ProjectileState{id, lat_rad * RAD2DEG, lon_rad * RAD2DEG, psi_rad * RAD2DEG, alt_m,
                           damage, ttl, owner};
}

// Wire framing: header (protocol, server_tick, n), then the GEO section n*(id, GeoPoint),
// then — iff protocol >= 2 — the KIN section n*(id, phi_q, tas_q[, gamma_q]), then — iff
// protocol >= 4 — the WEAPON section: n*(id, hp_q, fire_cd_q[, ammo_q][, last_hit_by_q]
// [, engine_hp_q, wing_hp_q, tail_hp_q, kills_q]) (ammo_q iff protocol >= 5; last_hit_by_q iff
// protocol >= 6; the region pools + kills iff protocol >= 7),
// a projectile count m, and m*(pid, GeoPoint, damage_q, ttl, owner). All self-delimiting; the GEO-001 codec is reused
// verbatim so its byte layout / parity vectors are untouched.
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
    if (s.protocol >= 4) {             // WEAPON section (WEAPON-001, gunnery state)
        for (const auto& e : s.entities) {  // per-aircraft hp + fire-rate cooldown
            geo001::encode_i64(e.id, out);
            geo001::encode_i64(geo001::quantize(e.hp, HP_SCALE), out);
            geo001::encode_i64(geo001::quantize(e.fire_cd, FIRECD_SCALE), out);
            if (s.protocol >= 5)  // G4 ammo: magazine rounds remaining (v1.14r0)
                geo001::encode_i64(geo001::quantize(e.ammo, AMMO_SCALE), out);
            if (s.protocol >= 6)  // attacker attribution: last_hit_by (v1.17r0)
                geo001::encode_i64(geo001::quantize(e.last_hit_by, LASTHITBY_SCALE), out);
            if (s.protocol >= 7) {  // region pools + kill tally (v1.19r0)
                geo001::encode_i64(geo001::quantize(e.engine_hp, ENGINEHP_SCALE), out);
                geo001::encode_i64(geo001::quantize(e.wing_hp, WINGHP_SCALE), out);
                geo001::encode_i64(geo001::quantize(e.tail_hp, TAILHP_SCALE), out);
                geo001::encode_i64(geo001::quantize(e.kills, KILLS_SCALE), out);
            }
        }
        geo001::encode_i64(static_cast<int64_t>(s.projectiles.size()), out);  // live rounds
        for (const auto& p : s.projectiles) {
            geo001::encode_i64(p.id, out);
            geo001::encode_point(geo001::GeoPoint{p.lat_deg, p.lon_deg, p.bearing_deg, p.alt_m},
                                 out);
            geo001::encode_i64(geo001::quantize(p.damage, DAMAGE_SCALE), out);
            geo001::encode_i64(p.ttl, out);     // exact integer counter (kernel u32)
            geo001::encode_i64(p.owner, out);   // exact integer index  (kernel u32)
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
    out.projectiles.clear();
    if (out.protocol >= 4) {           // WEAPON section (WEAPON-001)
        for (int64_t i = 0; i < n; ++i) {  // per-aircraft hp + fire_cd
            int64_t wid = 0, hp_q = 0, firecd_q = 0;
            if (!geo001::decode_i64(data, len, pos, wid)) return false;
            if (wid != out.entities[static_cast<size_t>(i)].id) return false;
            if (!geo001::decode_i64(data, len, pos, hp_q)) return false;
            if (!geo001::decode_i64(data, len, pos, firecd_q)) return false;
            out.entities[static_cast<size_t>(i)].hp = geo001::dequantize(hp_q, HP_SCALE);
            out.entities[static_cast<size_t>(i)].fire_cd =
                geo001::dequantize(firecd_q, FIRECD_SCALE);
            if (out.protocol >= 5) {  // G4 ammo: magazine rounds remaining (v1.14r0)
                int64_t ammo_q = 0;
                if (!geo001::decode_i64(data, len, pos, ammo_q)) return false;
                out.entities[static_cast<size_t>(i)].ammo = geo001::dequantize(ammo_q, AMMO_SCALE);
            }
            if (out.protocol >= 6) {  // attacker attribution: last_hit_by (v1.17r0)
                int64_t lhb_q = 0;
                if (!geo001::decode_i64(data, len, pos, lhb_q)) return false;
                out.entities[static_cast<size_t>(i)].last_hit_by =
                    geo001::dequantize(lhb_q, LASTHITBY_SCALE);
            }
            if (out.protocol >= 7) {  // region pools + kill tally (v1.19r0)
                int64_t ehp_q = 0, whp_q = 0, thp_q = 0, kls_q = 0;
                if (!geo001::decode_i64(data, len, pos, ehp_q)) return false;
                if (!geo001::decode_i64(data, len, pos, whp_q)) return false;
                if (!geo001::decode_i64(data, len, pos, thp_q)) return false;
                if (!geo001::decode_i64(data, len, pos, kls_q)) return false;
                out.entities[static_cast<size_t>(i)].engine_hp =
                    geo001::dequantize(ehp_q, ENGINEHP_SCALE);
                out.entities[static_cast<size_t>(i)].wing_hp =
                    geo001::dequantize(whp_q, WINGHP_SCALE);
                out.entities[static_cast<size_t>(i)].tail_hp =
                    geo001::dequantize(thp_q, TAILHP_SCALE);
                out.entities[static_cast<size_t>(i)].kills =
                    geo001::dequantize(kls_q, KILLS_SCALE);
            }
        }
        int64_t m = 0;
        if (!geo001::decode_i64(data, len, pos, m)) return false;
        if (m < 0) return false;
        out.projectiles.reserve(static_cast<size_t>(m));
        for (int64_t i = 0; i < m; ++i) {
            ProjectileState p{};
            geo001::GeoPoint pt{};
            int64_t dmg_q = 0;
            if (!geo001::decode_i64(data, len, pos, p.id)) return false;
            if (!geo001::decode_point(data, len, pos, pt)) return false;
            if (!geo001::decode_i64(data, len, pos, dmg_q)) return false;
            if (!geo001::decode_i64(data, len, pos, p.ttl)) return false;
            if (!geo001::decode_i64(data, len, pos, p.owner)) return false;
            p.lat_deg = pt.lat;
            p.lon_deg = pt.lon;
            p.bearing_deg = pt.bearing;
            p.alt_m = pt.alt;
            p.damage = geo001::dequantize(dmg_q, DAMAGE_SCALE);
            out.projectiles.push_back(p);
        }
    }
    return true;
}

}  // namespace netsnap
}  // namespace seads
