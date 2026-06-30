// SEADS WEAPON-001 snapshot-section parity test. Asserts C++ netsnap == Python reference
// vectors for the gunnery state (seal v1.12r0, snapshot protocol 4): byte-identical wire on
// encode of a full GEO+KIN+WEAPON frame, and exact round-trip on decode — per-aircraft
// hp/fire_cd within one quantum, every live projectile within a quantum (position/damage) with
// ttl/owner EXACT. Exit 0 PASS, 1 FAIL.
#include "snapshot.h"
#include "weapon_vectors.h"

#include <cstdio>
#include <cstring>
#include <vector>

using namespace seads;

int main() {
    int fails = 0;
    int n_proj_total = 0;

    for (int i = 0; i < weap_vec::WEAPON_VECTOR_COUNT; ++i) {
        const auto& v = weap_vec::WEAPON_VECTORS[i];

        netsnap::Snapshot snap;
        snap.protocol = v.protocol;
        snap.server_tick = v.server_tick;
        for (int e = 0; e < v.n_entities; ++e) {
            const auto& se = v.entities[e];
            snap.entities.push_back(netsnap::EntityState{
                se.id, se.lat_deg, se.lon_deg, se.bearing_deg, se.alt_m, se.phi_deg, se.tas_mps,
                se.gamma_deg, se.hp, se.fire_cd});
        }
        for (int p = 0; p < v.n_projectiles; ++p) {
            const auto& sp = v.projectiles[p];
            snap.projectiles.push_back(netsnap::ProjectileState{
                sp.id, sp.lat_deg, sp.lon_deg, sp.bearing_deg, sp.alt_m, sp.damage, sp.ttl,
                sp.owner});
        }
        n_proj_total += v.n_projectiles;

        // --- encode byte-parity ---
        std::vector<uint8_t> wire;
        netsnap::encode_snapshot(snap, wire);
        if (static_cast<int>(wire.size()) != v.wire_len ||
            std::memcmp(wire.data(), v.wire, v.wire_len) != 0) {
            ++fails;
            std::printf("FAIL weapon encode #%d: got %zu bytes, expected %d\n",
                        i, wire.size(), v.wire_len);
        }

        // --- decode round-trip ---
        size_t pos = 0;
        netsnap::Snapshot dec;
        bool ok = netsnap::decode_snapshot(v.wire, v.wire_len, pos, dec);
        bool same = ok && pos == static_cast<size_t>(v.wire_len)
                 && dec.protocol == v.protocol && dec.server_tick == v.server_tick
                 && static_cast<int>(dec.entities.size()) == v.n_entities
                 && static_cast<int>(dec.projectiles.size()) == v.n_projectiles;
        for (int e = 0; same && e < v.n_entities; ++e) {
            const auto& a = v.entities[e];
            const auto& b = dec.entities[e];
            // wire is lossy by quantization: compare each field within its quantum, ids exactly.
            if (b.id != a.id ||
                geo001::quantize(b.hp, netsnap::HP_SCALE)
                    != geo001::quantize(a.hp, netsnap::HP_SCALE) ||
                geo001::quantize(b.fire_cd, netsnap::FIRECD_SCALE)
                    != geo001::quantize(a.fire_cd, netsnap::FIRECD_SCALE)) {
                same = false;
            }
        }
        for (int p = 0; same && p < v.n_projectiles; ++p) {
            const auto& a = v.projectiles[p];
            const auto& b = dec.projectiles[p];
            if (b.id != a.id ||
                geo001::quantize(b.lat_deg, geo001::LATLON_SCALE)
                    != geo001::quantize(a.lat_deg, geo001::LATLON_SCALE) ||
                geo001::quantize(b.lon_deg, geo001::LATLON_SCALE)
                    != geo001::quantize(a.lon_deg, geo001::LATLON_SCALE) ||
                geo001::quantize(b.bearing_deg, geo001::BEARING_SCALE)
                    != geo001::quantize(a.bearing_deg, geo001::BEARING_SCALE) ||
                geo001::quantize(b.alt_m, geo001::ALT_SCALE)
                    != geo001::quantize(a.alt_m, geo001::ALT_SCALE) ||
                geo001::quantize(b.damage, netsnap::DAMAGE_SCALE)
                    != geo001::quantize(a.damage, netsnap::DAMAGE_SCALE) ||
                b.ttl != a.ttl || b.owner != a.owner) {  // ttl/owner carried EXACTLY
                same = false;
            }
        }
        if (!same) {
            ++fails;
            std::printf("FAIL weapon decode #%d\n", i);
        }
    }

    if (fails == 0) {
        std::printf("PASS: WEAPON-001 byte-exact vs reference (%d snapshots, %d projectiles)\n",
                    weap_vec::WEAPON_VECTOR_COUNT, n_proj_total);
        return 0;
    }
    std::printf("RESULT: WEAPON-001 snapshot FAIL (%d mismatches)\n", fails);
    return 1;
}
