// The fixed-timestep accumulator (SPEC §10, AT-9's mechanism): whole ticks
// only, remainder conserved, alpha in [0, 1), frame-dt clamp. The plant
// never sees frame time BECAUSE these properties hold — this is the
// first-principles tripwire under the eventual AT-9 end-to-end run.

#include <catch2/catch_test_macros.hpp>
#include <cmath>

#include "app/loop.h"

namespace {
constexpr double kSimDt = 1.0 / 120.0;
}

TEST_CASE("accumulator: remainder conservation across arbitrary frame dts",
          "[app][loop]") {
    // Awkward, non-multiple frame times (fixed table, no RNG).
    const double frame_dts[] = {0.0131, 1.0 / 30.0,  0.0009, 0.0517,
                                0.0083, 1.0 / 240.0, 0.0201, 0.0166,
                                0.0333, 0.00001,     0.0755, 0.0124};
    app::Accumulator acc(kSimDt);
    double fed = 0.0;
    long total_ticks = 0;
    for (int rep = 0; rep < 500; ++rep) {
        for (double dt : frame_dts) {
            const int ticks = acc.advance(dt);
            REQUIRE(ticks >= 0);
            total_ticks += ticks;
            fed += dt;
            // Alpha strictly in [0, 1): a full tick never lingers.
            REQUIRE(acc.alpha() >= 0.0);
            REQUIRE(acc.alpha() < 1.0);
            // Conservation: ticks stepped + remainder == wall time fed
            // (nothing lost, nothing invented).
            const double replayed = total_ticks * kSimDt + acc.alpha() * kSimDt;
            REQUIRE(std::abs(replayed - fed) < 1e-9 * (1.0 + fed));
        }
    }
}

TEST_CASE("accumulator: tick count independent of frame chunking (AT-9 core)",
          "[app][loop]") {
    // The same wall-time span delivered as 30 fps vs 240 fps frames must
    // produce the same number of sim ticks (+/- the sub-tick remainder,
    // which conservation above pins). Exact-multiple dts keep it exact.
    const double span = 4.0;
    app::Accumulator slow(kSimDt);
    app::Accumulator fast(kSimDt);
    long slow_ticks = 0;
    long fast_ticks = 0;
    for (int i = 0; i < 120; ++i) slow_ticks += slow.advance(span / 120.0);
    for (int i = 0; i < 960; ++i) fast_ticks += fast.advance(span / 960.0);
    REQUIRE(std::abs(slow_ticks - fast_ticks) <= 1);
    REQUIRE(std::abs(static_cast<double>(slow_ticks) * kSimDt - span) < kSimDt);
}

TEST_CASE("accumulator: spiral-of-death clamp drops excess wall time",
          "[app][loop]") {
    app::Accumulator acc(kSimDt, 0.25);
    // A 10 s hitch contributes at most max_frame_dt of sim time.
    const int ticks = acc.advance(10.0);
    REQUIRE(ticks == static_cast<int>(std::floor(0.25 / kSimDt)));
    // Negative frame dt (clock weirdness) contributes nothing.
    app::Accumulator acc2(kSimDt, 0.25);
    REQUIRE(acc2.advance(-1.0) == 0);
    REQUIRE(acc2.alpha() == 0.0);
}
