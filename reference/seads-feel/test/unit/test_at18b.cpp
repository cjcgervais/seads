// AT-18b — authority single-count, CONTROLLER side (SPEC §7/§9.4, HARNESS §5).
// The tau_cmd -> Input -> tau round-trip: for a grid of (tau_cmd, V) below
// saturation, the Input the controller emits (control::plant_invert), fed
// back through the sim's exact authority torque, reproduces tau_cmd. This
// catches a SECOND q/delta division in control/ that AT-18a (which reads
// omega only, from the plant side) is structurally blind to. Above
// saturation the controller clamps ONLY to +/-1 (SPEC §9.4).

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>

#include "config/load_aircraft.h"
#include "control/controller.h"
#include "sim/aero.h"

#ifdef NDEBUG
#error \
    "SEADS gate requires an assert-live build (SPEC 6.1); configure with CMAKE_BUILD_TYPE=Debug"
#endif

namespace {

const sim::AircraftParams kP =
    cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");

// The sim's authority torque (step.cpp, the c*q_eff*delta*Input term — no
// damping, which is zero at the omega the round-trip assumes). Written here
// from the SAME sim/aero.h pieces the plant uses, so "reproduces tau_cmd" is
// a real single-count check, not a tautology against plant_invert's own math.
double authority_torque(double input, double c_axis, double V, double alt,
                        const sim::AircraftParams& p) {
    return c_axis * sim::q_eff(sim::q_dyn(sim::rho_at(alt, p), V), p) *
           sim::delta_max_eff(V, p) * input;
}

double tau_max(double c_axis, double V, const sim::AircraftParams& p) {
    return c_axis * sim::q_eff(sim::q_dyn(p.rho, V), p) *
           sim::delta_max_eff(V, p);
}

}  // namespace

TEST_CASE("AT-18b: tau_cmd -> Input -> tau round-trip below saturation") {
    struct AxisCase {
        const char* name;
        double c;
    };
    const AxisCase axes[] = {
        {"pitch", kP.c_pitch}, {"yaw", kP.c_yaw}, {"roll", kP.c_roll}};

    // Two speeds, one BELOW the floor crossover so the round-trip exercises
    // q_att_floor (where a naive double-division would still cancel but a
    // forked floor would not), one in the compression ramp (delta < 1).
    const double crossover = std::sqrt(2.0 * kP.q_att_floor / kP.rho);
    const double v_low = 15.0;
    const double v_high = 150.0;
    REQUIRE(v_low < crossover);
    REQUIRE(v_high > kP.v_full);
    REQUIRE(v_high < kP.v_redline);

    for (const auto& ax : axes) {
        CAPTURE(ax.name);
        for (double V : {v_low, v_high}) {
            CAPTURE(V);
            const double tmax = tau_max(ax.c, V, kP);
            // Below saturation: several fractions of full authority, both
            // signs.
            for (double frac : {-0.9, -0.5, -0.1, 0.1, 0.5, 0.9}) {
                const double tau_cmd = frac * tmax;
                const double input =
                    control::plant_invert(tau_cmd, ax.c, V, 0.0, kP);
                REQUIRE(std::abs(input) < 1.0);  // genuinely unsaturated
                CHECK(authority_torque(input, ax.c, V, 0.0, kP) ==
                      Catch::Approx(tau_cmd).epsilon(1e-12));
            }
        }

        // MB-atm: the SAME round-trip at 7 km (atm_frac ~ 0.44) — the
        // inversion must divide by the plant's THINNED authority (H1 at
        // altitude on the CONTROLLER side; AT-18a's 7 km leg owns the plant
        // side and is blind to this one). Mutation: plant_invert reads p.rho
        // instead of rho_at -> the emitted Input is ~2.3x too small and the
        // round-trip torque misses tau_cmd by atm_frac.
        {
            const double alt = 7000.0;
            const double tmax7 =
                tau_max(ax.c, v_high, kP) * sim::atm_frac(alt, kP);
            const double tau_cmd = 0.5 * tmax7;
            const double input =
                control::plant_invert(tau_cmd, ax.c, v_high, alt, kP);
            REQUIRE(std::abs(input) < 1.0);
            CHECK(authority_torque(input, ax.c, v_high, alt, kP) ==
                  Catch::Approx(tau_cmd).epsilon(1e-12));
            // Teeth: at altitude the same tau needs MORE deflection.
            CHECK(std::abs(input) > 1.5 * std::abs(control::plant_invert(
                                              tau_cmd, ax.c, v_high, 0.0, kP)));
        }
    }
}

TEST_CASE("AT-18b: above saturation the controller clamps only to +/-1") {
    // 3x full authority both directions -> Input pegs at the sign, exactly
    // +/-1 (no controller-side compression, SPEC §9.4).
    const double V = 150.0;
    const double tmax = tau_max(kP.c_pitch, V, kP);
    CHECK(control::plant_invert(3.0 * tmax, kP.c_pitch, V, 0.0, kP) ==
          Catch::Approx(1.0));
    CHECK(control::plant_invert(-3.0 * tmax, kP.c_pitch, V, 0.0, kP) ==
          Catch::Approx(-1.0));

    // ...and at exactly full authority, Input == the sign, magnitude 1.
    CHECK(control::plant_invert(tmax, kP.c_pitch, V, 0.0, kP) ==
          Catch::Approx(1.0));
}
