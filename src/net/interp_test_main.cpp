// SEADS remote interpolation parity test (netcode layer 4a). Builds each buffer from
// interp_vectors.h, samples at the given render tick, and asserts the interpolated entities are
// BIT-IDENTICAL to the Python reference (tools/interp_ref.py). Pure IEEE +,-,*,/ => exact
// equality (not a tolerance), proven across the toolchain matrix in CI. Exit 0 PASS, 1 FAIL.
#include "interp.h"
#include "interp_vectors.h"
#include "snapshot.h"

#include <cstdio>
#include <vector>

using namespace seads;

int main() {
    int fails = 0;

    for (int ci = 0; ci < interp_vec::CASE_COUNT; ++ci) {
        const auto& c = interp_vec::CASES[ci];

        interp::SnapshotBuffer buf;
        for (int fi = 0; fi < c.n_frames; ++fi) {
            const auto& f = c.frames[fi];
            netsnap::Snapshot snap;
            snap.server_tick = f.server_tick;
            for (int e = 0; e < f.n; ++e) {
                const auto& ie = f.ents[e];
                snap.entities.push_back(
                    netsnap::EntityState{ie.id, ie.lat_deg, ie.lon_deg, ie.bearing_deg, ie.alt_m});
            }
            buf.add(snap);
        }

        std::vector<netsnap::EntityState> got = buf.sample(c.render_tick);

        bool ok = (static_cast<int>(got.size()) == c.n_expect);
        for (int e = 0; ok && e < c.n_expect; ++e) {
            const auto& a = got[static_cast<std::size_t>(e)];
            const auto& b = c.expect[e];
            // bit-exact: these are pure IEEE ops, so equality must be exact, not approximate.
            if (a.id != b.id || a.lat_deg != b.lat_deg || a.lon_deg != b.lon_deg ||
                a.bearing_deg != b.bearing_deg || a.alt_m != b.alt_m) {
                ok = false;
            }
        }
        if (!ok) {
            ++fails;
            std::printf("FAIL case #%d (%s): %zu entities, expected %d\n",
                        ci, c.label, got.size(), c.n_expect);
            for (int e = 0; e < static_cast<int>(got.size()) && e < c.n_expect; ++e) {
                const auto& a = got[static_cast<std::size_t>(e)];
                const auto& b = c.expect[e];
                std::printf("  id %lld: got (%.17g,%.17g,%.17g,%.17g) "
                            "exp (%.17g,%.17g,%.17g,%.17g)\n",
                            (long long)a.id, a.lat_deg, a.lon_deg, a.bearing_deg, a.alt_m,
                            b.lat_deg, b.lon_deg, b.bearing_deg, b.alt_m);
            }
        }
    }

    if (fails == 0) {
        std::printf("PASS: remote interpolation bit-exact vs reference (%d cases)\n",
                    interp_vec::CASE_COUNT);
        return 0;
    }
    std::printf("RESULT: remote interpolation FAIL (%d mismatches)\n", fails);
    return 1;
}
