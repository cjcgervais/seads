// Ballistics truth harness (implementation spec §6).
// Fires each of the four reference guns FLAT using weapon::advance at the REAL
// sim tick (dt = 1/120 s), reads off (tof, bore drop, retained speed) at 100–500
// m by interpolating across the crossing step, writes the results to
// build/ballistics_measured.json so ballistic_score.py can score the REAL model,
// and asserts realism gates on the two on-airframe guns.
//
// Firewall: weapon/ is PURE (glm + std, no raylib, no sim:: writes) — this test
// links seads_render_core and carries no raylib dependency.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <fstream>
#include <string>
#include <vector>

#include "weapon/ballistics.h"  // apply_drag + Projectile + advance

#ifdef NDEBUG
#error "SEADS gate requires an assert-live build (SPEC 6.1)"
#endif

namespace {

constexpr double kDt  = 1.0 / 120.0;   // shipped sim tick (SPEC §7)
constexpr double kG   = 9.81;           // m/s^2
constexpr double kTol_vret = 0.07;      // 7 % retained-velocity gate (on-airframe guns only)

// Description of one reference gun.
struct GunEntry {
    const char* key;   // JSON key (must match ballistic_score.py expectations)
    double v0;         // muzzle speed [m/s]
    double drag_k;     // [1/m]
    bool on_airframe;  // true -> assert vret gate
    double ref_vret500; // reference vret @ 500 m from ballistics_reference.json
};

// The four reference guns (spec §4 — values from docs/ballistics_reference.json).
const GunEntry kGuns[] = {
    // key             v0      drag_k     on_airframe  ref_vret@500
    { "MG151_20",    805.0,  0.001565,  true,        368.1 },
    { "MG17_792",    855.0,  0.000710,  true,        599.5 },
    { "MG131_13",    710.0,  0.001315,  false,       367.9 },
    { "MGFF_M_20",   700.0,  0.001565,  false,       320.1 },
};

const double kRanges[] = { 100.0, 200.0, 300.0, 400.0, 500.0 };
constexpr int kNRanges = 5;

// Fire one gun flat (+x direction, gravity = -y) using weapon::advance at kDt.
// Interpolate (tof, drop, vret) at each reference range by linear interpolation
// across the step that crosses the range.
struct ShotResult {
    double tof_s[kNRanges];
    double drop_m[kNRanges];
    double vret_mps[kNRanges];
};

ShotResult fire_flat(double v0, double drag_k) {
    ShotResult r;
    for (int i = 0; i < kNRanges; ++i) {
        r.tof_s[i] = r.drop_m[i] = r.vret_mps[i] = 0.0;
    }

    const glm::dvec3 grav{0.0, -kG, 0.0};
    weapon::Projectile p;
    p.pos = {0.0, 0.0, 0.0};
    p.vel = {v0, 0.0, 0.0};  // horizontal along +x
    p.drag_k = drag_k;
    p.active = true;

    int ri = 0;  // next range index to capture
    double x_prev = p.pos.x;
    double y_prev = p.pos.y;
    double vx_prev = p.vel.x;
    double vy_prev = p.vel.y;
    double t = 0.0;

    // March with weapon::advance (the REAL integrator, at the real tick).
    for (int step = 0; step < 1'000'000 && ri < kNRanges; ++step) {
        weapon::advance(p, kDt, grav);
        t += kDt;
        const double x_curr = p.pos.x;
        const double y_curr = p.pos.y;
        const double vx_curr = p.vel.x;
        const double vy_curr = p.vel.y;

        // Capture each range that the round crossed during this step.
        while (ri < kNRanges && x_curr >= kRanges[ri]) {
            // Linear interpolation fraction within this step.
            const double dx = x_curr - x_prev;
            const double f = (dx > 1e-15) ? (kRanges[ri] - x_prev) / dx : 0.0;
            r.tof_s[ri]    = (t - kDt) + f * kDt;
            r.drop_m[ri]   = -(y_prev + f * (y_curr - y_prev));  // bore-referenced drop (positive down)
            const double cvx = vx_prev + f * (vx_curr - vx_prev);
            const double cvy = vy_prev + f * (vy_curr - vy_prev);
            r.vret_mps[ri] = std::hypot(cvx, cvy);
            ++ri;
        }

        x_prev  = x_curr;
        y_prev  = y_curr;
        vx_prev = vx_curr;
        vy_prev = vy_curr;
    }
    return r;
}

// Write one gun's results as a JSON sub-object.
void write_gun_json(std::ofstream& f, const char* key, const ShotResult& r, bool last) {
    f << "    \"" << key << "\": {\n";
    // ranges_m
    f << "      \"ranges_m\": [";
    for (int i = 0; i < kNRanges; ++i) {
        f << kRanges[i];
        if (i < kNRanges - 1) f << ", ";
    }
    f << "],\n";
    // tof_s
    f << "      \"tof_s\": [";
    for (int i = 0; i < kNRanges; ++i) {
        f << r.tof_s[i];
        if (i < kNRanges - 1) f << ", ";
    }
    f << "],\n";
    // drop_m
    f << "      \"drop_m\": [";
    for (int i = 0; i < kNRanges; ++i) {
        f << r.drop_m[i];
        if (i < kNRanges - 1) f << ", ";
    }
    f << "],\n";
    // vret_mps
    f << "      \"vret_mps\": [";
    for (int i = 0; i < kNRanges; ++i) {
        f << r.vret_mps[i];
        if (i < kNRanges - 1) f << ", ";
    }
    f << "]\n";
    f << "    }";
    if (!last) f << ",";
    f << "\n";
}

}  // namespace

// ===========================================================================
// TRUTH HARNESS: fire all four guns, write ballistics_measured.json, assert
// realism gates on the two on-airframe weapons.
// The fixture-no-op trap is dodged by the two sanity asserts at the end:
//   REQUIRE drop@500 > 1 m  — a vacuum mutant (no drag) loses most of the drop
//     since it travels faster and flight time is lower; still > 1 m from gravity.
//   REQUIRE vret@500 < 0.85*v0 — a vacuum mutant keeps ~v0 and fails this.
// ===========================================================================
TEST_CASE("ballistic truth harness: fire flat, write JSON, assert realism gates") {
    const int nGuns = static_cast<int>(sizeof(kGuns) / sizeof(kGuns[0]));

    // Fire all guns.
    std::vector<ShotResult> results(nGuns);
    for (int g = 0; g < nGuns; ++g) {
        results[g] = fire_flat(kGuns[g].v0, kGuns[g].drag_k);
    }

    // Write build/ballistics_measured.json.
    // SEADS_CONFIG_DIR is defined by CMake as the config/ dir; build output
    // goes to the SAME directory (the harness writes telemetry there too).
    // We write into the build dir which is CMAKE_BINARY_DIR; the scorer
    // expects "build/ballistics_measured.json" relative to the project root.
    // Use a path relative to SEADS_CONFIG_DIR/../build/.
    const std::string out_path =
        std::string(SEADS_CONFIG_DIR) + "/../build/ballistics_measured.json";
    {
        std::ofstream f(out_path);
        REQUIRE(f.is_open());
        f << "{\n  \"guns\": {\n";
        for (int g = 0; g < nGuns; ++g) {
            write_gun_json(f, kGuns[g].key, results[g], g == nGuns - 1);
        }
        f << "  }\n}\n";
    }

    // Realism gates on the TWO on-airframe guns (MG151_20, MG17_792).
    // vret@500 within 7% of the reference JSON value.
    for (int g = 0; g < nGuns; ++g) {
        if (!kGuns[g].on_airframe) continue;
        const double vret500 = results[g].vret_mps[4];  // index 4 = 500 m
        const double ref     = kGuns[g].ref_vret500;
        const double rel     = std::abs(vret500 - ref) / ref;
        INFO("Gun " << kGuns[g].key << " vret@500: sim=" << vret500
                    << " ref=" << ref << " rel_err=" << rel);
        REQUIRE(rel <= kTol_vret);
    }

    // Anti-vacuum-regression: for each on-airframe gun:
    //   drop@500 > 1 m (gravity acting — not a no-op bullet)
    //   vret@500 < 0.85*v0 (drag is acting — not a vacuum run)
    for (int g = 0; g < nGuns; ++g) {
        if (!kGuns[g].on_airframe) continue;
        const double drop500  = results[g].drop_m[4];
        const double vret500  = results[g].vret_mps[4];
        INFO("Gun " << kGuns[g].key << " drop@500=" << drop500
                    << " vret@500=" << vret500 << " v0=" << kGuns[g].v0);
        REQUIRE(drop500 > 1.0);                          // gravity is non-trivial
        REQUIRE(vret500 < 0.85 * kGuns[g].v0);          // drag is non-trivial
    }
}
