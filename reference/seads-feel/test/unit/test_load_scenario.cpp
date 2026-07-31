// load_scenario (SPEC §0 S8-drone): the scenario table (target drone + Stage-2
// gunsight) loads strictly like the other config, degrees convert to radians at
// the boundary, and out-of-range values are rejected. Mutations are applied to
// the REAL committed table so the test can't drift from the schema.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "config/load_aircraft.h"
#include "config/load_scenario.h"

#ifdef NDEBUG
#error "SEADS gate requires an assert-live build (SPEC 6.1)"
#endif

namespace {

const sim::AircraftParams kAp =
    cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");

std::string slurp(const std::string& path) {
    std::ifstream f(path);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::string write_temp(const std::string& text, const char* tag) {
    const std::filesystem::path p =
        std::filesystem::temp_directory_path() /
        (std::string("seads_scen_") + tag + ".toml");
    std::ofstream(p) << text;
    return p.string();
}

std::string replace_all(std::string s, const std::string& from,
                        const std::string& to) {
    for (size_t i = s.find(from); i != std::string::npos;
         i = s.find(from, i + to.size())) {
        s.replace(i, from.size(), to);
    }
    return s;
}

constexpr double kPi = 3.14159265358979323846;

}  // namespace

TEST_CASE("load_scenario: the committed table loads and converts degrees") {
    const cfg::ScenarioParams s =
        cfg::load_scenario_toml(SEADS_CONFIG_DIR "/scenario.toml", kAp);
    // turn_bank_deg = 25 -> radians at the boundary.
    CHECK(s.drone.turn_bank == Catch::Approx(25.0 * kPi / 180.0).margin(1e-9));
    CHECK(s.drone.throttle == Catch::Approx(1.0));
    CHECK(s.drone.speed == Catch::Approx(140.0));
    CHECK(s.drone.spawn_alt > 0.0);
    // Gunsight: hit_cone_deg -> cos at the boundary; track_range >= max_range.
    CHECK(s.gunsight.hit_cone_cos ==
          Catch::Approx(std::cos(2.0 * kPi / 180.0)).margin(1e-9));
    CHECK(s.gunsight.max_range == Catch::Approx(500.0));
    CHECK(s.gunsight.track_range >= s.gunsight.max_range);
}

TEST_CASE("load_scenario: missing key throws") {
    const std::string base = slurp(SEADS_CONFIG_DIR "/scenario.toml");
    REQUIRE(base.find("turn_bank_deg = 25.0") != std::string::npos);
    // Drop the key entirely -> require() must throw (strict schema).
    const std::string bad = replace_all(base, "turn_bank_deg = 25.0", "");
    const std::string path = write_temp(bad, "missing_key");
    CHECK_THROWS(cfg::load_scenario_toml(path, kAp));
}

TEST_CASE(
    "load_scenario: a turn_bank past the 45 deg docile ceiling is rejected") {
    const std::string base = slurp(SEADS_CONFIG_DIR "/scenario.toml");
    REQUIRE(base.find("turn_bank_deg = 25.0") != std::string::npos);
    // 60 deg > the 45 deg cap: the no-protection autopilot's docile envelope
    // (red-team P1) — the loader rejects it, not just >= 90.
    const std::string bad =
        replace_all(base, "turn_bank_deg = 25.0", "turn_bank_deg = 60.0");
    const std::string path = write_temp(bad, "turn_bank_over_cap");
    CHECK_THROWS(cfg::load_scenario_toml(path, kAp));
}

TEST_CASE("load_scenario: a throttle above 1 is rejected") {
    const std::string base = slurp(SEADS_CONFIG_DIR "/scenario.toml");
    REQUIRE(base.find("throttle      = 1.0") != std::string::npos);
    const std::string bad =
        replace_all(base, "throttle      = 1.0", "throttle      = 1.5");
    const std::string path = write_temp(bad, "throttle_over_1");
    CHECK_THROWS(cfg::load_scenario_toml(path, kAp));
}

// The docile-envelope floors (Fable P1-1) — the guard must cover speed and
// throttle, not just bank.
TEST_CASE("load_scenario: a sub-stall drone speed is rejected") {
    const std::string base = slurp(SEADS_CONFIG_DIR "/scenario.toml");
    REQUIRE(base.find("speed         = 140.0") != std::string::npos);
    // 30 m/s is below 1.5*V_stall (~68 m/s for the committed airframe).
    const std::string bad =
        replace_all(base, "speed         = 140.0", "speed         = 30.0");
    CHECK_THROWS(cfg::load_scenario_toml(write_temp(bad, "substall"), kAp));
}

TEST_CASE("load_scenario: a thrust-infeasible drone throttle is rejected") {
    const std::string base = slurp(SEADS_CONFIG_DIR "/scenario.toml");
    REQUIRE(base.find("throttle      = 1.0") != std::string::npos);
    // 0.2 is below the 0.4 sink-avoidance floor.
    const std::string bad =
        replace_all(base, "throttle      = 1.0", "throttle      = 0.2");
    CHECK_THROWS(cfg::load_scenario_toml(write_temp(bad, "lowthr"), kAp));
}

// The ballistic-solver retune tripwire (Fable P1-3): a muzzle too slow to keep
// f(t) monotone within the time cap must fail LOUD at load, not silently
// mis-solve in flight.
TEST_CASE("load_scenario: a too-slow muzzle_speed is rejected") {
    const std::string base = slurp(SEADS_CONFIG_DIR "/scenario.toml");
    REQUIRE(base.find("muzzle_speed  = 850.0") != std::string::npos);
    // 400 < 2*v_redline + g*kBallisticTMax (~515 for the committed airframe).
    const std::string bad =
        replace_all(base, "muzzle_speed  = 850.0", "muzzle_speed  = 400.0");
    CHECK_THROWS(cfg::load_scenario_toml(write_temp(bad, "slowmuzzle"), kAp));
}
