// AT-18a — authority single-count, plant side (SPEC §7, HARNESS §5).
// Per axis: peak angular acceleration MEASURED through the real sim::step()
// (injector, full deflection from rest) vs DERIVED from the analytic
// formula c * max(q, q_att_floor) * delta_max_eff(V) / I, at two speeds —
// one BELOW the floor crossover (~24 m/s), which verifies the floor itself.
// A mismatch means q was applied twice (H1) or the params forked.
// (AT-18b — the controller-side tau_cmd -> Input -> tau round-trip — lands
// with the controller in Section 4.)

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>

#include "config/load_aircraft.h"
#include "sim/aero.h"
#include "test/harness/injector.h"

#ifdef NDEBUG
#error \
    "SEADS gate requires an assert-live build (SPEC 6.1); configure with CMAKE_BUILD_TYPE=Debug"
#endif

namespace {

const sim::AircraftParams kP =
    cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");

struct AxisCase {
    harness::Axis axis;
    const char* name;
    double c, I;
};

}  // namespace

TEST_CASE("AT-18a: measured ang-accel matches derived, per axis, two speeds") {
    const AxisCase axes[] = {
        {harness::Axis::pitch, "pitch", kP.c_pitch, kP.I_pitch},
        {harness::Axis::yaw, "yaw", kP.c_yaw, kP.I_yaw},
        {harness::Axis::roll, "roll", kP.c_roll, kP.I_roll},
    };

    // Guard the test's own premise against config drift: the low speed must
    // sit below the floor crossover, the high speed above it (and inside
    // the compression ramp so delta_max_eff < 1 is exercised too).
    const double v_low = 15.0;
    const double v_high = 150.0;
    const double crossover = std::sqrt(2.0 * kP.q_att_floor / kP.rho);
    REQUIRE(v_low < crossover);
    REQUIRE(v_high > crossover);
    REQUIRE(v_high > kP.v_full);
    REQUIRE(v_high < kP.v_redline);

    for (const auto& ax : axes) {
        CAPTURE(ax.name);
        for (double V : {v_low, v_high}) {
            CAPTURE(V);
            const double measured =
                harness::measure_ang_accel_max(kP, V, ax.axis, 2000.0);
            const double derived =
                sim::ang_accel_max_derived(ax.c, ax.I, V, 2000.0, kP);
            CHECK(measured == Catch::Approx(derived).epsilon(1e-12));
        }

        // Teeth, not tautology: at v_low the FLOOR must be doing the work —
        // the unfloored value is visibly smaller — and at v_high the
        // compression ramp must be biting (delta < 1).
        const double measured_low =
            harness::measure_ang_accel_max(kP, v_low, ax.axis, 2000.0);
        const double unfloored = ax.c * sim::q_dyn(kP.rho, v_low) *
                                 sim::delta_max_eff(v_low, kP) / ax.I;
        CHECK(measured_low > 2.0 * unfloored);

        const double measured_high =
            harness::measure_ang_accel_max(kP, v_high, ax.axis, 2000.0);
        const double unramped =
            ax.c * sim::q_eff(sim::q_dyn(kP.rho, v_high), kP) / ax.I;
        CHECK(measured_high < 0.95 * unramped);

        // MB-atm: the SAME measured==derived identity holds ABOVE the taper
        // (7 km, atm_frac ~ 0.44) — the plant's thinned torque and the
        // derived formula share sim::rho_at through sim/aero.h (H1 at
        // altitude). Teeth: the thinning is REAL (measured at 7 km visibly
        // below sea-band at the same V). Mutation: apply atm_frac in the
        // plant torque but not in ang_accel_max_derived (or vice versa) ->
        // the identity splits by 1/f ~ 2.3x.
        const double measured_hi_alt =
            harness::measure_ang_accel_max(kP, v_high, ax.axis, 7000.0);
        const double derived_hi_alt =
            sim::ang_accel_max_derived(ax.c, ax.I, v_high, 7000.0, kP);
        CHECK(measured_hi_alt == Catch::Approx(derived_hi_alt).epsilon(1e-12));
        CHECK(measured_hi_alt <
              0.6 * measured_high);  // the taper genuinely thins (~0.44)
    }
}
