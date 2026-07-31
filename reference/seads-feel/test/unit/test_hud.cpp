// Section 5 — HUD readouts in the CORRECT FRAME (SPEC §12; HARNESS §1 row 5).
// The gate's red-team focus for this section: G-load and AoA must be
// velocity-relative and local_up-aware, never a fixed-axis or attitude proxy.
// render::flight_readout is the exact thing draw_frame prints; these pin it
// against hand math and — the load-bearing case — separate it from a flat
// instrument that a level test could never catch (CLAUDE.md S3).

#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <glm/gtc/quaternion.hpp>
#include <string>

#include "config/load_aircraft.h"
#include "render/readout.h"
#include "test/harness/injector.h"

#ifdef NDEBUG
#error \
    "SEADS gate requires an assert-live build (SPEC 6.1); configure with CMAKE_BUILD_TYPE=Debug"
#endif

namespace {

const sim::AircraftParams kP =
    cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");

constexpr double kPi = 3.14159265358979323846;
inline double rad(double deg) { return deg * kPi / 180.0; }

}  // namespace

TEST_CASE("hud: level flight reads ~0 AoA, ~0 bank, correct speed/altitude") {
    const double V = 140.0, alt = 2000.0;
    const sim::SimState s =
        harness::level_state(kP, V, alt, {1.0, 0.0, 0.0}, {0.0, 0.0, -1.0});
    const render::FlightReadout r = render::flight_readout(s, kP);
    CHECK(r.speed == Catch::Approx(V));
    CHECK(r.altitude == Catch::Approx(alt));
    CHECK(r.aoa == Catch::Approx(0.0).margin(1e-9));  // velocity along the nose
    CHECK(r.bank == Catch::Approx(0.0).margin(1e-9));         // wings level
    CHECK(r.load_factor == Catch::Approx(0.0).margin(1e-9));  // Cl(0) = 0
}

TEST_CASE("hud: banked-with-AoA reads the local_up-aware bank and true n") {
    // 60-deg bank, 8-deg AoA at a SKEW point so local_up is no world axis.
    const double V = 160.0, bank = rad(60.0), alpha = rad(8.0);
    const sim::SimState s = harness::flight_state(
        kP, V, 2500.0, {3.0, -2.0, 5.0}, {1.0, 1.0, -0.3}, bank, 0.0, alpha);
    const render::FlightReadout r = render::flight_readout(s, kP);

    CHECK(r.aoa == Catch::Approx(alpha).margin(1e-9));  // velocity-relative
    CHECK(r.bank == Catch::Approx(bank).margin(1e-9));  // == local_up-aware phi
    CHECK(r.load_factor ==
          Catch::Approx(sim::load_factor(alpha, V, 2000.0, 0.0, kP))
              .epsilon(1e-12));

    // The flat-instrument mutation (S3): a bank measured against a FIXED world
    // up instead of local_up. At this skew point it is wildly wrong — the HUD's
    // value must be the local_up one, far from the fixed-axis proxy.
    const glm::dvec3 body_right = s.orientation * glm::dvec3{1.0, 0.0, 0.0};
    const double flat_bank = -std::asin(
        std::clamp(glm::dot(body_right, glm::dvec3{0.0, 1.0, 0.0}), -1.0, 1.0));
    CHECK(std::abs(r.bank - flat_bank) > rad(20.0));
}

TEST_CASE("hud: AoA is velocity-relative, NOT attitude pitch") {
    // Climbing with velocity exactly along the nose: the nose sits 30 deg above
    // the local horizon (attitude pitch = 30 deg) but AoA = 0. A body/attitude
    // proxy would read ~30 deg; the correct velocity-relative readout is ~0.
    const glm::dvec3 up{1.0, 0.0, 0.0};        // over +X pole
    const glm::dvec3 tangent{0.0, 0.0, -1.0};  // a horizon dir
    const double climb = rad(30.0);
    const glm::dvec3 nose =
        glm::normalize(std::cos(climb) * tangent + std::sin(climb) * up);
    const glm::dvec3 wing = glm::normalize(glm::cross(nose, up));  // body right
    const glm::dvec3 body_up = glm::normalize(glm::cross(wing, nose));

    sim::SimState s;
    s.position = up * (kP.R + 2000.0);
    s.orientation =
        glm::normalize(glm::quat_cast(glm::dmat3{wing, body_up, -nose}));
    const double V = 150.0;
    s.velocity = V * nose;  // velocity ALONG the nose -> AoA 0, climbing 30 deg
    s.last_vhat = nose;

    const render::FlightReadout r = render::flight_readout(s, kP);
    REQUIRE(glm::dot(nose, up) > 0.4);  // fixture really is climbing steeply
    CHECK(r.aoa == Catch::Approx(0.0).margin(1e-9));  // NOT the 30-deg attitude
}

TEST_CASE("hud: pitch_attitude is ATTITUDE - holds the climb angle, not AoA") {
    // The attitude-vs-velocity separation, BOTH ways (MB HUD, Chad's "a read
    // that maintains its attitude" ask). Leg 1: climbing 30 deg with velocity
    // exactly along the nose — AoA reads 0, pitch_attitude reads the 30-deg
    // climb. Built at a GENERIC point (S8-drone P2: no world axis aligned
    // with local_up, so a fixed-world-up pitch mutant separates here).
    const glm::dvec3 up = glm::normalize(glm::dvec3{1.0, 1.0, 1.0});
    const glm::dvec3 t_raw = glm::dvec3{0.0, 0.0, -1.0} -
                             glm::dot(glm::dvec3{0.0, 0.0, -1.0}, up) * up;
    const glm::dvec3 tangent = glm::normalize(t_raw);
    const double climb = rad(30.0);
    const glm::dvec3 nose =
        glm::normalize(std::cos(climb) * tangent + std::sin(climb) * up);
    const glm::dvec3 wing = glm::normalize(glm::cross(nose, up));
    const glm::dvec3 body_up = glm::normalize(glm::cross(wing, nose));

    sim::SimState s;
    s.position = up * (kP.R + 2000.0);
    s.orientation =
        glm::normalize(glm::quat_cast(glm::dmat3{wing, body_up, -nose}));
    s.velocity = 150.0 * nose;  // velocity ALONG the nose
    s.last_vhat = nose;

    const render::FlightReadout r = render::flight_readout(s, kP);
    REQUIRE(glm::dot(nose, up) > 0.4);  // fixture really climbs steeply
    CHECK(r.aoa == Catch::Approx(0.0).margin(1e-9));
    // Kills pitch := alpha (0) and pitch := dot(nose, fixed world up).
    CHECK(r.pitch_attitude == Catch::Approx(climb).margin(1e-9));

    // Leg 2 (the vacuity companion — with velocity along the nose, attitude
    // and flight path COINCIDE, so a nose->vhat slip reading gamma passes
    // leg 1): level ATTITUDE with 8 deg AoA — velocity descends 8 deg below
    // the level nose, gamma ~ -8. pitch_attitude must read the NOSE: 0.
    const sim::SimState s2 =
        harness::flight_state(kP, 150.0, 2500.0, {1.0, 1.0, 1.0},
                              {0.0, 0.0, -1.0}, 0.0, 0.0, rad(8.0));
    const render::FlightReadout r2 = render::flight_readout(s2, kP);
    REQUIRE(r2.aoa == Catch::Approx(rad(8.0)).margin(1e-9));  // premise
    // Kills pitch := gamma (~ -8 deg) and pitch := alpha (+8 deg).
    CHECK(r2.pitch_attitude == Catch::Approx(0.0).margin(1e-9));
}

TEST_CASE("hud: bank_full unfolds past 90 deg, BOTH signs") {
    // phi = -asin(...) FOLDS at 90 (bank 100 reads phi 80, decreasing); the
    // roll-arc pointer needs the true +/-180 read. Sampled BOTH signs — an
    // abs(sin phi) mutant reads +100 at the +100 fixture and only the -100
    // sample kills it (the S4a upper-half-plane trap).
    const double V = 150.0;
    for (const double sgn : {+1.0, -1.0}) {
        const sim::SimState s = harness::flight_state(
            kP, V, 2500.0, {3.0, -2.0, 5.0}, {1.0, 1.0, -0.3}, sgn * rad(100.0),
            0.0, rad(4.0));
        const render::FlightReadout r = render::flight_readout(s, kP);
        // Premise: the fixture really crossed the fold (folded phi = 80).
        REQUIRE(r.bank == Catch::Approx(sgn * rad(80.0)).margin(1e-9));
        // Kills bank_full := phi (80), a clamped-cosPhiTheta (90), and
        // swapped atan2 args (~ -10 via atan2(cos, sin) sign structure).
        CHECK(r.bank_full == Catch::Approx(sgn * rad(100.0)).margin(1e-9));
    }
    // Inverted: atan2(+/-0, -1) is +/-pi — assert the magnitude explicitly
    // (never abs-fold the expectation into the mechanism's own failure mode).
    const sim::SimState si =
        harness::flight_state(kP, V, 3000.0, {1.0, 0.0, 0.0}, {0.0, 0.0, -1.0},
                              rad(180.0), 0.0, rad(5.0));
    const render::FlightReadout ri = render::flight_readout(si, kP);
    CHECK(std::isfinite(ri.bank_full));
    CHECK(std::abs(ri.bank_full) == Catch::Approx(kPi).margin(1e-6));
}

TEST_CASE("hud: flap_mode_label maps the commanded detents") {
    CHECK(std::string(render::flap_mode_label(0)) == "CLEAN");
    CHECK(std::string(render::flap_mode_label(1)) == "COMBAT");
    CHECK(std::string(render::flap_mode_label(2)) == "LANDING");
    CHECK(std::string(render::flap_mode_label(-1)) == "CLEAN");  // fallback
    CHECK(std::string(render::flap_mode_label(7)) == "CLEAN");
}

TEST_CASE("hud: inverted flight reads signed n and stays finite") {
    // 180-deg bank (rolled inverted), small positive AoA: n is signed by the
    // (still positive) Cl here; the readout must not NaN or clamp |Cl|.
    const double V = 150.0, alpha = rad(5.0);
    const sim::SimState s =
        harness::flight_state(kP, V, 3000.0, {1.0, 0.0, 0.0}, {0.0, 0.0, -1.0},
                              rad(180.0), 0.0, alpha);
    const render::FlightReadout r = render::flight_readout(s, kP);
    CHECK(std::isfinite(r.load_factor));
    CHECK(std::isfinite(r.bank));
    CHECK(r.aoa == Catch::Approx(alpha).margin(1e-9));
    CHECK(r.load_factor ==
          Catch::Approx(sim::load_factor(alpha, V, 2000.0, 0.0, kP)));
}

// ===========================================================================
// MB-7c pure mappings (read-only energy legibility; the S8 gunsight firewall
// — these feed ONLY the render layer). Pinned here beside the readout they
// consume.
// ===========================================================================
#include "render/vortex.h"
#include "render/wind_audio.h"

TEST_CASE("mb7c: wind level is monotone in speed, hushed by thin air") {
    const render::WindLevel quiet = render::wind_level(20.0, 1.0);
    CHECK(quiet.volume == 0.0);  // below the quiet floor: silent
    double prev_v = -1.0, prev_p = 0.0;
    for (double V = 40.0; V <= 260.0; V += 20.0) {
        const render::WindLevel w = render::wind_level(V, 1.0);
        CHECK(w.volume >= prev_v);
        CHECK(w.pitch >= prev_p);
        CHECK(w.volume <= 1.0);
        prev_v = w.volume;
        prev_p = w.pitch;
    }
    // Thin air hushes but never fully mutes a fast dive (the ceiling stays
    // audible): at atm 0.5 the volume sits strictly between the floor-scaled
    // and full-density values.
    const render::WindLevel full = render::wind_level(200.0, 1.0);
    const render::WindLevel thin = render::wind_level(200.0, 0.5);
    CHECK(thin.volume < full.volume);
    CHECK(thin.volume > render::kWindAtmFloor * full.volume - 1e-12);
    CHECK(thin.pitch == full.pitch);  // pitch tracks speed, not density
}

TEST_CASE("mb7c: vortex strength gates on the SHARED AoA margin and true n") {
    const double aoa_max = rad(20.0);
    // Calm cruise: nothing.
    CHECK(render::vortex_strength(rad(4.0), aoa_max, 1.0, 150.0) == 0.0);
    // Near the protection limit: lit, ramping to 1 at the limit.
    CHECK(render::vortex_strength(rad(19.9), aoa_max, 1.0, 150.0) > 0.9);
    CHECK(render::vortex_strength(aoa_max, aoa_max, 1.0, 150.0) == 1.0);
    // Negative AoA (inverted pull) lights it symmetrically.
    CHECK(render::vortex_strength(-aoa_max, aoa_max, 1.0, 150.0) == 1.0);
    // High G alone lights it even at modest AoA fraction.
    CHECK(render::vortex_strength(rad(4.0), aoa_max, 7.5, 200.0) == 1.0);
    // No q, no vortices (the v->0 apex is not a smoke machine).
    CHECK(render::vortex_strength(aoa_max, aoa_max, 9.0, 20.0) == 0.0);
    // Trails: spawn, age, expire; reset clears.
    render::VortexTrails t;
    sim::SimState s = harness::level_state(kP, 150.0, 2000.0, {1.0, 0.0, 0.0},
                                           {0.0, 0.0, -1.0});
    render::vortex_update(t, s, 1.0, 0.016);
    CHECK(t.left.size() == 1);
    CHECK(t.right.size() == 1);
    // The tips straddle the fuselage symmetrically about the body-X axis.
    const glm::dvec3 body_right = s.orientation * glm::dvec3{1.0, 0.0, 0.0};
    const double xl = glm::dot(t.left[0].pos - s.position, body_right);
    const double xr = glm::dot(t.right[0].pos - s.position, body_right);
    CHECK(xl == Catch::Approx(-render::kVortexTipRight));
    CHECK(xr == Catch::Approx(+render::kVortexTipRight));
    render::vortex_update(t, s, 0.0, render::kVortexLife + 0.1);  // all expire
    CHECK(t.left.empty());
    CHECK(t.right.empty());
    render::vortex_update(t, s, 0.5, 0.016);
    render::vortex_reset(t);
    CHECK(t.left.empty());
}
