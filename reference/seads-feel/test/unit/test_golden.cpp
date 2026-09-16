// Section-2 golden trajectory (HARNESS §3 T2): fixed start + fixed input
// script -> 1200 ticks through the full plant -> committed checkpoints.
// This is the "still computes the same flight" tripwire — it moves iff the
// plant physics, the integrator, or config/aircraft.toml move. A move is
// a HALT, not a nuisance: confirm intent, re-record via `seads_harness
// golden`, explain in the commit.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <glm/glm.hpp>
#include <iterator>

#include "config/load_aircraft.h"
#include "sim/step.h"
#include "sim/world.h"
#include "test/golden/golden_flight.h"
#include "test/harness/scenarios.h"

#ifdef NDEBUG
#error \
    "SEADS gate requires an assert-live build (SPEC 6.1); configure with CMAKE_BUILD_TYPE=Debug"
#endif

TEST_CASE(
    "golden flight: 10 s scripted raw-mode flight reproduces checkpoints") {
    const sim::AircraftParams p =
        cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");

    sim::SimState s = harness::golden_start(p);
    int next_checkpoint = 0;

    for (int tick = 1; tick <= harness::kGoldenTicks; ++tick) {
        s = sim::step(s, harness::golden_input(tick - 1), p, nullptr, p.sim_dt);

        if (tick % harness::kGoldenCheckpointEvery != 0) continue;
        REQUIRE(next_checkpoint < static_cast<int>(std::size(golden::kFlight)));
        const golden::Checkpoint& g = golden::kFlight[next_checkpoint++];
        REQUIRE(g.tick == tick);
        CAPTURE(tick);

        // Same runtime, same flags: these should be bit-identical; the
        // margins only absorb legitimate compiler/library micro-drift.
        CHECK(s.position.x == Catch::Approx(g.px).margin(1e-6));
        CHECK(s.position.y == Catch::Approx(g.py).margin(1e-6));
        CHECK(s.position.z == Catch::Approx(g.pz).margin(1e-6));
        CHECK(s.velocity.x == Catch::Approx(g.vx).margin(1e-9));
        CHECK(s.velocity.y == Catch::Approx(g.vy).margin(1e-9));
        CHECK(s.velocity.z == Catch::Approx(g.vz).margin(1e-9));
        CHECK(s.orientation.w == Catch::Approx(g.qw).margin(1e-12));
        CHECK(s.orientation.x == Catch::Approx(g.qx).margin(1e-12));
        CHECK(s.orientation.y == Catch::Approx(g.qy).margin(1e-12));
        CHECK(s.orientation.z == Catch::Approx(g.qz).margin(1e-12));
        CHECK(s.angular_vel.x == Catch::Approx(g.wx).margin(1e-12));
        CHECK(s.angular_vel.y == Catch::Approx(g.wy).margin(1e-12));
        CHECK(s.angular_vel.z == Catch::Approx(g.wz).margin(1e-12));
    }

    REQUIRE(next_checkpoint == 4);
    // The scripted flight must also stay airborne and finite end-to-end —
    // a golden of a crashed state would be a very convincing wrong answer.
    CHECK(sim::altitude(s.position, p) > 0.0);
}
