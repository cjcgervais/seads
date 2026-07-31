// Section-4b controller golden (HARNESS §3 T2): the scripted closed-loop
// instructor flight reproduces its committed checkpoints. This is the "still
// flies the same" tripwire for the cascade — it moves iff the controller, the
// plant, config/aircraft.toml, or config/controller.toml move. A move is a
// HALT: confirm intent, re-record via `seads_harness ctrl_golden`, explain in
// the commit. The flight runs through the REAL spherical sim::step() (HARNESS
// §3) so the correct constant corrective rotation is graded correctly.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <glm/glm.hpp>
#include <iterator>

#include "config/load_aircraft.h"
#include "config/load_controller.h"
#include "control/controller.h"
#include "sim/world.h"
#include "test/golden/controller_golden.h"
#include "test/harness/instructor.h"

#ifdef NDEBUG
#error \
    "SEADS gate requires an assert-live build (SPEC 6.1); configure with CMAKE_BUILD_TYPE=Debug"
#endif

TEST_CASE(
    "controller golden: scripted instructor flight reproduces checkpoints") {
    const sim::AircraftParams p =
        cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");
    const control::ControllerParams cp =
        cfg::load_controller_toml(SEADS_CONFIG_DIR "/controller.toml", p);

    double thr = 0.0;
    const sim::SimState s0 = harness::ctrl_golden_start(p, &thr);
    harness::ClosedLoop cl(s0, glm::dvec3{0.0, 0.0, -1.0});
    cl.aim_nose();

    int next = 0;
    for (int tick = 1; tick <= harness::kCtrlGoldenTicks; ++tick) {
        harness::ctrl_golden_step(cl, tick, thr, p, cp);
        if (tick % harness::kCtrlGoldenCheckpointEvery != 0) continue;

        REQUIRE(next < static_cast<int>(std::size(golden::kCtrlFlight)));
        const golden::CtrlCheckpoint& g = golden::kCtrlFlight[next++];
        REQUIRE(g.tick == tick);
        CAPTURE(tick);

        const sim::SimState& s = cl.state;
        // Same runtime, same flags: bit-identical up to compiler/library
        // micro-drift. Position tolerance is looser (km-scale magnitudes at
        // R = 15 km); rates/quat pinned tight.
        CHECK(s.position.x == Catch::Approx(g.px).margin(1e-5));
        CHECK(s.position.y == Catch::Approx(g.py).margin(1e-5));
        CHECK(s.position.z == Catch::Approx(g.pz).margin(1e-5));
        CHECK(s.velocity.x == Catch::Approx(g.vx).margin(1e-7));
        CHECK(s.velocity.y == Catch::Approx(g.vy).margin(1e-7));
        CHECK(s.velocity.z == Catch::Approx(g.vz).margin(1e-7));
        CHECK(s.orientation.w == Catch::Approx(g.qw).margin(1e-9));
        CHECK(s.orientation.x == Catch::Approx(g.qx).margin(1e-9));
        CHECK(s.orientation.y == Catch::Approx(g.qy).margin(1e-9));
        CHECK(s.orientation.z == Catch::Approx(g.qz).margin(1e-9));
        CHECK(s.angular_vel.x == Catch::Approx(g.wx).margin(1e-9));
        CHECK(s.angular_vel.y == Catch::Approx(g.wy).margin(1e-9));
        CHECK(s.angular_vel.z == Catch::Approx(g.wz).margin(1e-9));
        // The controller's own state is pinned too — a cascade change that
        // left the trajectory ~unchanged but altered the integrator or the
        // captured bank still moves the golden.
        CHECK(cl.internal.integ.x == Catch::Approx(g.integ_x).margin(1e-9));
        CHECK(cl.internal.integ.y == Catch::Approx(g.integ_y).margin(1e-9));
        CHECK(cl.internal.integ.z == Catch::Approx(g.integ_z).margin(1e-9));
        CHECK(cl.internal.held_bank == Catch::Approx(g.held_bank).margin(1e-9));
        CHECK((cl.internal.regime == control::Regime::FINE ? 0 : 1) ==
              g.regime);
    }
    REQUIRE(next == 4);
    // The scripted flight must also stay airborne and finite end-to-end.
    CHECK(sim::altitude(cl.state.position, p) > 0.0);
    CHECK(std::isfinite(glm::length(cl.state.velocity)));
}
