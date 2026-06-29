// SEADS GEO-001 snapshot parity test. Asserts C++ netsnap == Python reference vectors:
// byte-identical wire on encode, exact round-trip on decode, bit-exact rad->quantize
// conversion. Exit 0 PASS, 1 FAIL.
#include "snapshot.h"
#include "snapshot_vectors.h"

#include <cstdio>
#include <cstring>
#include <vector>

using namespace seads;

int main() {
    int fails = 0;

    // --- snapshot framing: encode byte-parity + decode round-trip ---
    for (int i = 0; i < snap_vec::SNAP_VECTOR_COUNT; ++i) {
        const auto& v = snap_vec::SNAP_VECTORS[i];

        netsnap::Snapshot snap;
        snap.protocol = v.protocol;
        snap.server_tick = v.server_tick;
        for (int e = 0; e < v.n_entities; ++e) {
            const auto& se = v.entities[e];
            snap.entities.push_back(
                netsnap::EntityState{se.id, se.lat_deg, se.lon_deg, se.bearing_deg, se.alt_m});
        }

        std::vector<uint8_t> wire;
        netsnap::encode_snapshot(snap, wire);
        bool match = (static_cast<int>(wire.size()) == v.wire_len) &&
                     (std::memcmp(wire.data(), v.wire, v.wire_len) == 0);
        if (!match) {
            ++fails;
            std::printf("FAIL snapshot encode #%d: got %zu bytes, expected %d\n",
                        i, wire.size(), v.wire_len);
        }

        size_t pos = 0;
        netsnap::Snapshot dec;
        bool ok = netsnap::decode_snapshot(v.wire, v.wire_len, pos, dec);
        bool same = ok && pos == static_cast<size_t>(v.wire_len)
                 && dec.protocol == v.protocol && dec.server_tick == v.server_tick
                 && static_cast<int>(dec.entities.size()) == v.n_entities;
        for (int e = 0; same && e < v.n_entities; ++e) {
            const auto& a = v.entities[e];
            const auto& b = dec.entities[e];
            // wire is lossy by quantization: compare within one quantum, ids exactly.
            if (b.id != a.id ||
                geo001::quantize(b.lat_deg, geo001::LATLON_SCALE)
                    != geo001::quantize(a.lat_deg, geo001::LATLON_SCALE) ||
                geo001::quantize(b.lon_deg, geo001::LATLON_SCALE)
                    != geo001::quantize(a.lon_deg, geo001::LATLON_SCALE) ||
                geo001::quantize(b.bearing_deg, geo001::BEARING_SCALE)
                    != geo001::quantize(a.bearing_deg, geo001::BEARING_SCALE) ||
                geo001::quantize(b.alt_m, geo001::ALT_SCALE)
                    != geo001::quantize(a.alt_m, geo001::ALT_SCALE)) {
                same = false;
            }
        }
        if (!same) {
            ++fails;
            std::printf("FAIL snapshot decode #%d\n", i);
        }
    }

    // --- from_kernel conversion: rad -> degrees -> quantize, bit-exact vs reference ---
    for (int i = 0; i < snap_vec::CONV_VECTOR_COUNT; ++i) {
        const auto& v = snap_vec::CONV_VECTORS[i];
        netsnap::EntityState e = netsnap::from_kernel(0, v.lat_rad, v.lon_rad, v.psi_rad, 0.0);
        int64_t latq = geo001::quantize(e.lat_deg, geo001::LATLON_SCALE);
        int64_t lonq = geo001::quantize(e.lon_deg, geo001::LATLON_SCALE);
        int64_t bearq = geo001::quantize(e.bearing_deg, geo001::BEARING_SCALE);
        if (latq != v.lat_q || lonq != v.lon_q || bearq != v.bear_q) {
            ++fails;
            std::printf("FAIL conv #%d: lat %lld/%lld lon %lld/%lld bear %lld/%lld\n", i,
                        (long long)latq, (long long)v.lat_q, (long long)lonq,
                        (long long)v.lon_q, (long long)bearq, (long long)v.bear_q);
        }
    }

    if (fails == 0) {
        std::printf("PASS: GEO-001 snapshot byte-exact vs reference (%d snapshots, %d conv)\n",
                    snap_vec::SNAP_VECTOR_COUNT, snap_vec::CONV_VECTOR_COUNT);
        return 0;
    }
    std::printf("RESULT: GEO-001 snapshot FAIL (%d mismatches)\n", fails);
    return 1;
}
