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
    CHECK(s.drone.speed ==
          Catch::Approx(85.0));  // bandit cruise (140->115->95->85, catchable)
    CHECK(s.drone.spawn_alt > 0.0);
    // Gunsight: hit_cone_deg -> cos at the boundary; track_range >= max_range.
    CHECK(s.gunsight.hit_cone_cos ==
          Catch::Approx(std::cos(2.0 * kPi / 180.0)).margin(1e-9));
    CHECK(s.gunsight.max_range == Catch::Approx(500.0));
    CHECK(s.gunsight.track_range >= s.gunsight.max_range);
}

TEST_CASE("scenario_sled_comfort_matches_kernel_defaults") {
    // RC (§0b): the committed [sled_comfort] table must reproduce
    // sim::SledComfort{} EXACTLY, so loading is bit-neutral until Chad turns
    // a dial — the same identity contract [damage] ships under. EXACT
    // equality on purpose: these are doubles copied through toml, not
    // computed; any drift means the toml and the kernel defaults have
    // forked, which is the silent-fork risk this leg exists to catch.
    const cfg::ScenarioParams s =
        cfg::load_scenario_toml(SEADS_CONFIG_DIR "/scenario.toml", kAp);
    const sim::SledComfort d;
    CHECK(s.sled_comfort.rolled_persist_s == d.rolled_persist_s);
    CHECK(s.sled_comfort.rolled_grace_s == d.rolled_grace_s);
    CHECK(s.sled_comfort.roll_stiff_nm == d.roll_stiff_nm);
    CHECK(s.sled_comfort.roll_ref_rad == d.roll_ref_rad);
    CHECK(s.sled_comfort.roll_release_lo_rad == d.roll_release_lo_rad);
    CHECK(s.sled_comfort.roll_release_hi_rad == d.roll_release_hi_rad);
    CHECK(s.sled_comfort.roll_damp_nms == d.roll_damp_nms);
    CHECK(s.sled_comfort.roll_ref_blend == d.roll_ref_blend);
    CHECK(s.sled_comfort.lean_bite_gain == d.lean_bite_gain);
    CHECK(s.sled_comfort.lean_sat_gain_rad == d.lean_sat_gain_rad);
    CHECK(s.sled_comfort.assist_hull_frac == d.assist_hull_frac);
    CHECK(s.sled_comfort.roll_stiff_vgain == d.roll_stiff_vgain);
    CHECK(s.sled_comfort.release_floor_frac == d.release_floor_frac);
    CHECK(s.sled_comfort.release_floor_hi_rad == d.release_floor_hi_rad);
    CHECK(s.sled_comfort.assist_v_lo_ms == d.assist_v_lo_ms);
    CHECK(s.sled_comfort.assist_v_hi_ms == d.assist_v_hi_ms);
    CHECK(s.sled_comfort.side_k == d.side_k);
    CHECK(s.sled_comfort.side_c == d.side_c);
    CHECK(s.sled_comfort.side_mu == d.side_mu);
    CHECK(s.sled_comfort.side_right_gain_nm == d.side_right_gain_nm);
    CHECK(s.sled_comfort.side_right_vmin_ms == d.side_right_vmin_ms);
    CHECK(s.sled_comfort.side_right_vref_ms == d.side_right_vref_ms);
}

TEST_CASE("scenario_sled_comfort_rejects_an_inverted_release_band") {
    // The loader's own sanity net, mutation-proven on the REAL table.
    std::string t = slurp(SEADS_CONFIG_DIR "/scenario.toml");
    const std::string k = "roll_release_hi_rad = 1.20";
    const auto pos = t.find(k);
    REQUIRE(pos != std::string::npos);
    t.replace(pos, k.size(), "roll_release_hi_rad = 0.10");
    CHECK_THROWS(cfg::load_scenario_toml(write_temp(t, "rcband"), kAp));
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
    REQUIRE(base.find("speed         = 85.0") != std::string::npos);
    // 30 m/s is below 1.5*V_stall (~68 m/s for the committed airframe).
    const std::string bad =
        replace_all(base, "speed         = 85.0", "speed         = 30.0");
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
    REQUIRE(base.find("muzzle_speed  = 805.0") != std::string::npos);
    // 400 < 2*v_redline + g*kBallisticTMax (~515 for the committed airframe).
    const std::string bad =
        replace_all(base, "muzzle_speed  = 805.0", "muzzle_speed  = 400.0");
    CHECK_THROWS(cfg::load_scenario_toml(write_temp(bad, "slowmuzzle"), kAp));
}

// The [combat] bandit-AI block (bandit_combat_plan.md): the committed table
// loads with the expected values, degrees convert, and pursuit knobs land in
// DroneParams.
TEST_CASE("load_scenario: the [combat] section loads and converts degrees") {
    const cfg::ScenarioParams s =
        cfg::load_scenario_toml(SEADS_CONFIG_DIR "/scenario.toml", kAp);
    CHECK(s.combat.difficulty == 4);
    CHECK(s.combat.player_hp == Catch::Approx(100.0));
    CHECK(s.combat.bandit_gun.muzzle_speed == Catch::Approx(600.0));
    CHECK(s.combat.bandit_convergence == Catch::Approx(250.0));
    // Pursuit knobs route into DroneParams; deg -> rad / cos at the boundary.
    CHECK(s.drone.engage_range == Catch::Approx(6000.0));
    CHECK(s.drone.disengage_range > s.drone.engage_range);
    CHECK(s.drone.pursue_max_bank ==
          Catch::Approx(55.0 * kPi / 180.0).margin(1e-9));
    // game-AI-R4 (Chad's order: "ai that can kill me"): 5 -> 14 deg — the
    // sniper cone was unholdable vs a maneuvering human, so bandits merged
    // without ever firing.
    CHECK(s.drone.fire_cone_cos ==
          Catch::Approx(std::cos(14.0 * kPi / 180.0)).margin(1e-9));
    CHECK(s.drone.pursue_lead_speed == Catch::Approx(600.0));
    CHECK(s.drone.pursue_bank_gain == Catch::Approx(0.45));
    // game-AI-R5 tactical-spacing pins (red-team P2-3: the struct defaults
    // keep the pre-R5 values for the fixture-calibrated bfm unit tests, so
    // the SHIPPED table is pinned here to gate the fork).
    // E1 retune (rung E1.3): frustration 8 -> 12 so Offensive survives the
    // longer merges the raised chase ceiling now produces.
    CHECK(s.drone.bfm.frustration_s == Catch::Approx(12.0));
    CHECK(s.drone.bfm.extend_min_s == Catch::Approx(8.0));
    CHECK(s.drone.bfm.extend_max_s == Catch::Approx(16.0));
}

// difficulty is the int master knob, validated to [1, 5] — a 6 fails LOUD at
// load.
TEST_CASE("load_scenario: an out-of-range difficulty is rejected") {
    const std::string base = slurp(SEADS_CONFIG_DIR "/scenario.toml");
    REQUIRE(base.find("difficulty = 4") != std::string::npos);
    const std::string bad =
        replace_all(base, "difficulty = 4", "difficulty = 6");
    CHECK_THROWS(cfg::load_scenario_toml(write_temp(bad, "diff6"), kAp));
}

// The hysteresis gap is enforced: disengage_range must exceed engage_range.
TEST_CASE("load_scenario: disengage_range <= engage_range is rejected") {
    const std::string base = slurp(SEADS_CONFIG_DIR "/scenario.toml");
    REQUIRE(base.find("disengage_range = 7000.0") != std::string::npos);
    // Put disengage below engage (6000) -> the hysteresis-gap check throws.
    const std::string bad = replace_all(base, "disengage_range = 7000.0",
                                        "disengage_range = 2000.0");
    CHECK_THROWS(cfg::load_scenario_toml(write_temp(bad, "nogap"), kAp));
}

// Pursuit bank past the 55 deg attacker ceiling is rejected (above it the
// no-G-protection autopilot risks a stall/departure).
TEST_CASE(
    "load_scenario: a pursue_max_bank past the attacker ceiling is rejected") {
    const std::string base = slurp(SEADS_CONFIG_DIR "/scenario.toml");
    REQUIRE(base.find("pursue_max_bank_deg  = 55.0") != std::string::npos);
    const std::string bad = replace_all(base, "pursue_max_bank_deg  = 55.0",
                                        "pursue_max_bank_deg  = 70.0");
    CHECK_THROWS(cfg::load_scenario_toml(write_temp(bad, "steepbank"), kAp));
}

// The [maverick] block (drone/maverick.h): the committed table loads with the
// tunnel-run dials, and the airframe-relative envelope guard rejects a bore
// speed past redline (the docile-envelope discipline extended to the bore).
TEST_CASE("load_scenario: the [maverick] section loads the tunnel-run dials") {
    const cfg::ScenarioParams s =
        cfg::load_scenario_toml(SEADS_CONFIG_DIR "/scenario.toml", kAp);
    CHECK(s.drone.maverick.enabled);
    CHECK(s.drone.maverick.tunnel_speed_max == Catch::Approx(135.0));
    // FIX-F4 (2026-08-07): 2.0 -> 1.0, the probe-measured bore-wall fix.
    CHECK(s.drone.maverick.centerline_gain == Catch::Approx(1.0));
    CHECK(s.drone.maverick.exit_frac == Catch::Approx(0.95));
    // The curvature-ff bore-autopilot dials (v5 2026-07-24).
    CHECK(s.drone.maverick.gamma_lookahead_m == Catch::Approx(300.0));
    CHECK(s.drone.maverick.slope_ff_gain == Catch::Approx(1.0));
    CHECK(s.drone.maverick.track_gain == Catch::Approx(0.0045));
    CHECK(s.drone.maverick.climb_throttle_gain == Catch::Approx(1.3));
    CHECK(s.drone.maverick.climb_speed_min == Catch::Approx(118.0));
    // deg -> rad at the boundary.
    CHECK(s.drone.maverick.run_gamma_cap ==
          Catch::Approx(44.0 * kPi / 180.0).margin(1e-9));
    CHECK(s.drone.maverick.tunnel_speed_max <= kAp.v_redline);
}

TEST_CASE("load_scenario: a tunnel_speed_max past redline is rejected") {
    const std::string base = slurp(SEADS_CONFIG_DIR "/scenario.toml");
    REQUIRE(base.find("tunnel_speed_max    = 135.0") != std::string::npos);
    // 300 > v_redline (245): the [maverick] envelope guard throws.
    const std::string bad = replace_all(base, "tunnel_speed_max    = 135.0",
                                        "tunnel_speed_max    = 300.0");
    CHECK_THROWS(cfg::load_scenario_toml(write_temp(bad, "mav_fast"), kAp));
}

TEST_CASE("load_scenario: a maverick exit_frac out of range is rejected") {
    const std::string base = slurp(SEADS_CONFIG_DIR "/scenario.toml");
    REQUIRE(base.find("exit_frac = 0.95") != std::string::npos);
    const std::string bad =
        replace_all(base, "exit_frac = 0.95", "exit_frac = 0.3");
    CHECK_THROWS(cfg::load_scenario_toml(write_temp(bad, "mav_exit"), kAp));
}

// The [damage] component-damage block (damage_model_plan.md): the committed
// table loads with the Fable-tuned defaults, which must MATCH
// combat::DamageParams{} so the config-driven path is bit-identical to the
// previously-hardcoded defaults.
TEST_CASE("load_scenario: the [damage] section loads the tuned defaults") {
    const cfg::ScenarioParams s =
        cfg::load_scenario_toml(SEADS_CONFIG_DIR "/scenario.toml", kAp);
    const combat::DamageParams d{};  // the hardcoded defaults the toml mirrors
    CHECK(s.damage.wing_cl_loss == Catch::Approx(d.wing_cl_loss));
    CHECK(s.damage.wing_roll_loss == Catch::Approx(d.wing_roll_loss));
    CHECK(s.damage.struct_auth_loss == Catch::Approx(d.struct_auth_loss));
    CHECK(s.damage.pilot_gain_floor == Catch::Approx(d.pilot_gain_floor));
    CHECK(s.damage.cl_floor == Catch::Approx(d.cl_floor));
    CHECK(s.damage.c_roll_floor == Catch::Approx(d.c_roll_floor));
    CHECK(s.damage.cd0_cap_mult == Catch::Approx(d.cd0_cap_mult));
    CHECK(s.damage.component_hp == Catch::Approx(d.component_hp));
    CHECK(s.damage.route_wing_frac == Catch::Approx(d.route_wing_frac));
    CHECK(s.damage.route_center_pilot == Catch::Approx(d.route_center_pilot));
}

// FIX-6 (BFM loader coverage). The committed table loads with bfm disabled by
// default and its degree dials convert at the boundary, matching the
// [maverick]/[damage] precedent above.
TEST_CASE("load_scenario: the [combat] bfm block loads and converts degrees") {
    const cfg::ScenarioParams s =
        cfg::load_scenario_toml(SEADS_CONFIG_DIR "/scenario.toml", kAp);
    CHECK(s.drone.bfm.enabled);  // ships ARMED (the R5 fly-1 lesson)
    CHECK(s.drone.bfm.lag_off_hi ==
          Catch::Approx(60.0 * kPi / 180.0).margin(1e-9));
    CHECK(s.drone.bfm.yoyo_gamma ==
          Catch::Approx(30.0 * kPi / 180.0).margin(1e-9));
}

// attack_release_frac must be > 1 (the hysteresis gap) — exactly 1.0 is
// rejected, not merely a value below it.
TEST_CASE("load_scenario: a bfm attack_release_frac of exactly 1 is rejected") {
    const std::string base = slurp(SEADS_CONFIG_DIR "/scenario.toml");
    REQUIRE(base.find("bfm_attack_release_frac = 1.4") != std::string::npos);
    const std::string bad = replace_all(base, "bfm_attack_release_frac = 1.4",
                                        "bfm_attack_release_frac = 1.0");
    CHECK_THROWS(cfg::load_scenario_toml(write_temp(bad, "bfm_release1"), kAp));
}

// yoyo_exit_closure_mps must stay <= yoyo_closure_mps (40) so a yo-yo can
// always end before its own entry predicate re-arms.
TEST_CASE(
    "load_scenario: a bfm yoyo_exit_closure_mps above yoyo_closure_mps is "
    "rejected") {
    const std::string base = slurp(SEADS_CONFIG_DIR "/scenario.toml");
    REQUIRE(base.find("bfm_yoyo_exit_closure_mps = 10.0") != std::string::npos);
    const std::string bad =
        replace_all(base, "bfm_yoyo_exit_closure_mps = 10.0",
                    "bfm_yoyo_exit_closure_mps = 999.0");
    CHECK_THROWS(cfg::load_scenario_toml(write_temp(bad, "bfm_exit999"), kAp));
}

// bfm_enabled is a strict bool key — a string value must be rejected, not
// silently coerced.
TEST_CASE("load_scenario: a non-boolean bfm_enabled is rejected") {
    const std::string base = slurp(SEADS_CONFIG_DIR "/scenario.toml");
    REQUIRE(base.find("bfm_enabled = true") != std::string::npos);
    const std::string bad =
        replace_all(base, "bfm_enabled = true", "bfm_enabled = \"yes\"");
    CHECK_THROWS(
        cfg::load_scenario_toml(write_temp(bad, "bfm_enabled_str"), kAp));
}

// FIX-B loader rejection leg: the commanded yoyo climb must not exceed the
// pursuit gamma ceiling (pursue_max_gamma_deg = 30) — 31 > 30 is rejected.
TEST_CASE(
    "load_scenario: a bfm yoyo_gamma_deg past pursue_max_gamma_deg is "
    "rejected") {
    const std::string base = slurp(SEADS_CONFIG_DIR "/scenario.toml");
    REQUIRE(base.find("bfm_yoyo_gamma_deg        = 30.0") != std::string::npos);
    const std::string bad =
        replace_all(base, "bfm_yoyo_gamma_deg        = 30.0",
                    "bfm_yoyo_gamma_deg        = 31.0");
    CHECK_THROWS(
        cfg::load_scenario_toml(write_temp(bad, "bfm_yoyo_gamma31"), kAp));
}

// FIX-E (loader-check coverage). The five checks below were either newly
// added this round or previously landed with no rejection leg exercising
// them (a check nothing ever calls is unverified dead weight).

// reenter_energy_m must stay a SHALLOWER deficit than extend_energy_m (FIX-7)
// — a residual deficit at least as deep as the one that triggered Extend
// would let a bandit "recover" straight back into the same losing fight.
TEST_CASE(
    "load_scenario: a bfm reenter_energy_m >= extend_energy_m is rejected") {
    const std::string base = slurp(SEADS_CONFIG_DIR "/scenario.toml");
    REQUIRE(base.find("bfm_reenter_energy_m = 3000.0") != std::string::npos);
    // FIX-F1 fold: the violating value is a huge constant, not a copy of the
    // shipped extend_energy_m literal — the old "600.0" was welded to the old
    // extend dial and silently stopped violating when extend moved to 3000
    // (the config-relative-bounds trap, loader-test flavor). 1e9 out-violates
    // any sane future extend retune.
    const std::string bad = replace_all(base, "bfm_reenter_energy_m = 3000.0",
                                        "bfm_reenter_energy_m = 1000000000.0");
    CHECK_THROWS(
        cfg::load_scenario_toml(write_temp(bad, "bfm_reenter_ge"), kAp));
}

// yoyo_arm_s must stay strictly below yoyo_time_s (FIX-10) — else the arm
// window can never fit inside a single climb.
TEST_CASE("load_scenario: a bfm yoyo_arm_s >= yoyo_time_s is rejected") {
    const std::string base = slurp(SEADS_CONFIG_DIR "/scenario.toml");
    REQUIRE(base.find("bfm_yoyo_arm_s            = 0.5   # [s] both must hold "
                      "this long to arm (> 0)") != std::string::npos);
    const std::string bad = replace_all(
        base,
        "bfm_yoyo_arm_s            = 0.5   # [s] both must hold this long "
        "to arm (> 0)",
        "bfm_yoyo_arm_s            = 4.0   # [s] both must hold this long "
        "to arm (> 0)");
    CHECK_THROWS(
        cfg::load_scenario_toml(write_temp(bad, "bfm_arm_ge_time"), kAp));
}

// attack_range_m must stay strictly above fire_range_max (FIX-10) — else
// Offensive's merge range never overlaps the range band pursue() requires to
// fire, and guns never go hot at all.
TEST_CASE("load_scenario: a bfm attack_range_m <= fire_range_max is rejected") {
    const std::string base = slurp(SEADS_CONFIG_DIR "/scenario.toml");
    REQUIRE(base.find("bfm_attack_range_m      = 2200.0") != std::string::npos);
    const std::string bad =
        replace_all(base, "bfm_attack_range_m      = 2200.0",
                    "bfm_attack_range_m      = 500.0");
    CHECK_THROWS(
        cfg::load_scenario_toml(write_temp(bad, "bfm_attack_le_fire"), kAp));
}

// FIX-E's new check: yoyo_time_s must exceed min_dwell_s, else may_exit
// (gated on min_dwell_s) masks the yoyo_time_s timeout behind the dwell
// floor (a mode-length inversion).
TEST_CASE("load_scenario: a bfm yoyo_time_s <= min_dwell_s is rejected") {
    const std::string base = slurp(SEADS_CONFIG_DIR "/scenario.toml");
    REQUIRE(base.find("bfm_yoyo_time_s           = 3.5   # [s] hard ceiling on "
                      "the climb (> min_dwell_s)") != std::string::npos);
    const std::string bad = replace_all(
        base,
        "bfm_yoyo_time_s           = 3.5   # [s] hard ceiling on the climb "
        "(> min_dwell_s)",
        // 0.3 sits below the CURRENT min_dwell_s (0.5) while staying above
        // yoyo_arm_s (0.25 rule not in play: arm 0.5 -- so this mutant may
        // trip the arm<time check first; either throw satisfies the leg).
        // The old 1.0 was welded to min_dwell 2.0 and silently stopped
        // violating at the E1 retune (the config-relative-bounds trap).
        "bfm_yoyo_time_s           = 0.3   # [s] hard ceiling on the climb "
        "(> min_dwell_s)");
    CHECK_THROWS(
        cfg::load_scenario_toml(write_temp(bad, "bfm_time_le_dwell"), kAp));
}

// FIX-A / round-4 dial validation: the abort floor must sit strictly INSIDE
// the ramp (abort_t < 1) — the arm boundary is structural at climb_t == 1.0
// exactly, so abort_t at/above 1 collapses the hysteresis gap and re-creates
// the measured 1-tick flap loop.
TEST_CASE("load_scenario: a bfm climb_abort_t at or above 1 is rejected") {
    const std::string base = slurp(SEADS_CONFIG_DIR "/scenario.toml");
    REQUIRE(base.find("bfm_climb_abort_t = 0.25") != std::string::npos);
    const std::string bad = replace_all(base, "bfm_climb_abort_t = 0.25",
                                        "bfm_climb_abort_t = 1.0");
    CHECK_THROWS(
        cfg::load_scenario_toml(write_temp(bad, "bfm_abort_ge_one"), kAp));
}

// FIX-D's dial validation: the yo-yo cooldown (the guns-hot spell between
// yo-yos) must be a real positive duration.
TEST_CASE("load_scenario: a zero bfm_yoyo_cooldown_s is rejected") {
    const std::string base = slurp(SEADS_CONFIG_DIR "/scenario.toml");
    const std::string anchor = "bfm_yoyo_cooldown_s";
    REQUIRE(base.find(anchor) != std::string::npos);
    const std::size_t pos = base.find(anchor);
    const std::size_t eol = base.find('\n', pos);
    std::string bad = base;
    bad.replace(pos, eol - pos, "bfm_yoyo_cooldown_s = 0.0");
    CHECK_THROWS(
        cfg::load_scenario_toml(write_temp(bad, "bfm_cooldown_zero"), kAp));
}

// RUNG E2.1 FIX — the TRANSIT final-leg grace is bounded on BOTH sides, and
// both bounds are CONFIG-RELATIVE (never a welded second): above
// transit_timeout_s it would more than double the budget the anti-livelock
// give-up exists to enforce; below transit_fix_back_m / climb_speed_min it is
// too short to fly the final leg it is granted for, so it buys nothing while
// still loosening the bound. The shipped value is checked to sit inside both.
TEST_CASE("load_scenario: an out-of-band maverick transit_fix_grace_s is rejected") {
    const std::string base = slurp(SEADS_CONFIG_DIR "/scenario.toml");
    const std::string anchor = "transit_fix_grace_s";
    REQUIRE(base.find(anchor) != std::string::npos);
    const std::size_t pos = base.find(anchor);
    const std::size_t eol = base.find('\n', pos);
    auto with = [&](const char* line) {
        std::string bad = base;
        bad.replace(pos, eol - pos, line);
        return bad;
    };
    // Above the budget it may extend (transit_timeout_s is 150 s).
    CHECK_THROWS(cfg::load_scenario_toml(
        write_temp(with("transit_fix_grace_s = 1000.0"), "mav_grace_hi"), kAp));
    // Positive but far too short to fly transit_fix_back_m at climb_speed_min.
    CHECK_THROWS(cfg::load_scenario_toml(
        write_temp(with("transit_fix_grace_s = 1.0"), "mav_grace_lo"), kAp));
    // Negative is not "off".
    CHECK_THROWS(cfg::load_scenario_toml(
        write_temp(with("transit_fix_grace_s = -1.0"), "mav_grace_neg"), kAp));
    // 0 IS off, and must still load (the knob-off arm).
    CHECK_NOTHROW(cfg::load_scenario_toml(
        write_temp(with("transit_fix_grace_s = 0.0"), "mav_grace_off"), kAp));
}

// component_hp is a DIVISOR (ke / component_hp) — a zero would divide by zero
// and instakill every hit. It must be strictly positive.
TEST_CASE("load_scenario: a zero damage component_hp is rejected") {
    const std::string base = slurp(SEADS_CONFIG_DIR "/scenario.toml");
    REQUIRE(base.find("component_hp = 60.0") != std::string::npos);
    const std::string bad =
        replace_all(base, "component_hp = 60.0", "component_hp = 0.0");
    CHECK_THROWS(cfg::load_scenario_toml(write_temp(bad, "dmg_hp0"), kAp));
}

// A controllability floor of 0 would let a component's authority reach zero —
// the transforms rely on the floor staying in (0, 1] to keep the plane flyable.
TEST_CASE("load_scenario: a zero controllability floor is rejected") {
    const std::string base = slurp(SEADS_CONFIG_DIR "/scenario.toml");
    REQUIRE(base.find("c_roll_floor      = 0.25") != std::string::npos);
    const std::string bad = replace_all(base, "c_roll_floor      = 0.25",
                                        "c_roll_floor      = 0.0");
    CHECK_THROWS(cfg::load_scenario_toml(write_temp(bad, "dmg_floor0"), kAp));
}

// cd0_cap_mult caps how much drag can rise; below 1 it would REDUCE base drag
// on damage (nonsense) — must be >= 1.
TEST_CASE("load_scenario: a sub-unity cd0_cap_mult is rejected") {
    const std::string base = slurp(SEADS_CONFIG_DIR "/scenario.toml");
    REQUIRE(base.find("cd0_cap_mult      = 2.0") != std::string::npos);
    const std::string bad =
        replace_all(base, "cd0_cap_mult      = 2.0", "cd0_cap_mult      = 0.5");
    CHECK_THROWS(cfg::load_scenario_toml(write_temp(bad, "dmg_cap"), kAp));
}

// ---- RUNG E3 — THE FELT PASS ----------------------------------------------
// The two new [combat] keys, pinned at their SHIPPED values. The cap is the
// only INTEGER-valued dial in this section, so this leg is also what proves
// the loader reads it as a count and not as 0.
TEST_CASE("load_scenario: the [combat] E3 felt-pass dials load") {
    const cfg::ScenarioParams s =
        cfg::load_scenario_toml(SEADS_CONFIG_DIR "/scenario.toml", kAp);
    CHECK(s.drone.transit_fight_yield_m == Catch::Approx(2500.0));
    CHECK(s.drone.strike_concurrent_max == 1);
    // The shipped yield radius IS the merge-hold radius — one felt scale for
    // "a live merge" (the invariant the rejection leg below defends).
    CHECK(s.drone.transit_fight_yield_m ==
          Catch::Approx(s.drone.raid_fight_yield_m));
}

// THE LOOP-CLOSURE INVARIANT. A yield radius WIDER than raid_fight_yield_m
// would let a foe sit in the gap between them: wide enough to abort every
// relaunched transit, too far to freeze the countdown that relaunches it — a
// mode flap at the boundary. Rejected at load.
TEST_CASE(
    "load_scenario: a transit_fight_yield_m past raid_fight_yield_m is "
    "rejected") {
    const std::string base = slurp(SEADS_CONFIG_DIR "/scenario.toml");
    REQUIRE(base.find("transit_fight_yield_m = 2500.0") != std::string::npos);
    // A huge constant, not a copy of today's raid_fight_yield_m literal (the
    // config-relative-bounds trap): this out-violates any future retune.
    const std::string bad = replace_all(base, "transit_fight_yield_m = 2500.0",
                                        "transit_fight_yield_m = 1000000.0");
    CHECK_THROWS(cfg::load_scenario_toml(write_temp(bad, "e3_yield_wide"), kAp));
}

// The cap is a COUNT: 0 means uncapped, negative is meaningless.
TEST_CASE("load_scenario: a negative strike_concurrent_max is rejected") {
    const std::string base = slurp(SEADS_CONFIG_DIR "/scenario.toml");
    REQUIRE(base.find("strike_concurrent_max = 1") != std::string::npos);
    const std::string bad = replace_all(base, "strike_concurrent_max = 1",
                                        "strike_concurrent_max = -3");
    CHECK_THROWS(cfg::load_scenario_toml(write_temp(bad, "e3_cap_neg"), kAp));
}

// ===========================================================================
// RUNG E8.1's DIVE-ENVELOPE TRIPWIRE — BOTH DIRECTIONS.
//
// ★★ THE HALF THAT WAS BROKEN ON MAIN (found by the E8 red team, reproduced
// before it was believed, fixed in rung E9). E8 shipped `bfm_perch_lag_m = 0`
// as its documented ONE-LINE WALK-BACK -- the handoff says so twice, the spec
// says so, the dial comment says so. But 0 falls back to `bfm_lag_dist_m` (the
// 220 m TRACKING point), which is EXACTLY the pre-E8 geometry the tripwire was
// written to reject, so the walk-back threw at config load and took the game
// and the whole test binary with it. The rung's off-switch was a brick.
//
// NOTHING CAUGHT IT because the only leg exercising the off value sets it
// PROGRAMMATICALLY -- a test that never runs the path that ships. THAT is why
// this leg loads a real TOML through the real loader, and why it asserts BOTH
// directions: an off-switch that does not switch off, and a tripwire that does
// not trip, are the same class of defect.
// ===========================================================================
TEST_CASE("load_scenario: the slash dive tripwire lets its own walk-back load") {
    const std::string base = slurp(SEADS_CONFIG_DIR "/scenario.toml");
    REQUIRE(base.find("bfm_perch_lag_m       = 1700.0") != std::string::npos);
    REQUIRE(base.find("slash_doctrine        = false") != std::string::npos);

    // (1) THE WALK-BACK LOADS, with the doctrine in its shipped (off) state.
    const std::string off =
        replace_all(base, "bfm_perch_lag_m       = 1700.0",
                    "bfm_perch_lag_m       = 0.0");
    CHECK_NOTHROW(cfg::load_scenario_toml(write_temp(off, "e81_walkback"), kAp));

    // (2) NON-VACUITY, and the check keeps all of its force where it matters:
    // the SAME un-diveable geometry with the doctrine ARMED still throws,
    // because that is the configuration a slasher would actually fly.
    const std::string armed =
        replace_all(off, "slash_doctrine        = false",
                    "slash_doctrine        = true");
    CHECK_THROWS(cfg::load_scenario_toml(write_temp(armed, "e81_armed0"), kAp));

    // (3) And an armed doctrine with a perch that is un-diveable for the OTHER
    // reason (a tall perch over a real standoff) throws too -- so (2) is not
    // passing on the fallback path alone.
    const std::string tall =
        replace_all(replace_all(base, "slash_doctrine        = false",
                                "slash_doctrine        = true"),
                    "bfm_perch_lag_m       = 1700.0",
                    "bfm_perch_lag_m       = 300.0");
    CHECK_THROWS(cfg::load_scenario_toml(write_temp(tall, "e81_tall"), kAp));
}

// ===========================================================================
// ★★★ RUNG D2 — THE REPOSITION'S RELATIONSHIP CHECKS, BOTH DIRECTIONS.
//
// Every one of these is a way the feature silently becomes a no-op or a NEW
// crash source. ★ AND THE WALK-BACK IS RUN THROUGH THE REAL LOADER (the E8.1
// lesson above: an off-switch that only ever gets set programmatically is an
// off-switch nobody has proved loads).

TEST_CASE("load_scenario: D2 regroup_agl_m must clear the in-dome avoid band") {
    const std::string base = slurp(SEADS_CONFIG_DIR "/scenario.toml");
    REQUIRE(base.find("regroup_agl_m        = 500.0") != std::string::npos);
    // 300 sits INSIDE the shipped 250/400 terrain-avoid band, so the manoeuvre
    // that exists to reduce crashes would fly straight into the pull-up latch.
    const std::string bad = replace_all(base, "regroup_agl_m        = 500.0",
                                        "regroup_agl_m        = 300.0");
    CHECK_THROWS(cfg::load_scenario_toml(write_temp(bad, "d2_agl_low"), kAp));
}

TEST_CASE("load_scenario: D2 pull_frac at or past the release is rejected") {
    const std::string base = slurp(SEADS_CONFIG_DIR "/scenario.toml");
    REQUIRE(base.find("regroup_pull_frac    = 0.40") != std::string::npos);
    // == release: the aim point sits exactly ON the release, so an episode can
    // never get inside it and every one runs out the clock.
    const std::string bad = replace_all(base, "regroup_pull_frac    = 0.40",
                                        "regroup_pull_frac    = 0.45");
    CHECK_THROWS(cfg::load_scenario_toml(write_temp(bad, "d2_pull_eq"), kAp));
}

TEST_CASE("load_scenario: D2 a non-hysteretic regroup trigger is rejected") {
    const std::string base = slurp(SEADS_CONFIG_DIR "/scenario.toml");
    REQUIRE(base.find("regroup_frac_release = 0.45") != std::string::npos);
    // release == arm is a SINGLE-THRESHOLD trigger: it chatters at tick rate
    // on a dwelling boundary frac. The house rule is that every latch in this
    // codebase is hysteretic, and here it is structurally unrepresentable.
    const std::string bad = replace_all(base, "regroup_frac_release = 0.45",
                                        "regroup_frac_release = 0.667");
    CHECK_THROWS(cfg::load_scenario_toml(write_temp(bad, "d2_no_hyst"), kAp));
}

TEST_CASE("load_scenario: D2 an unreachable trigger is rejected") {
    const std::string base = slurp(SEADS_CONFIG_DIR "/scenario.toml");
    REQUIRE(base.find("regroup_attack_window_s = 10.0") != std::string::npos);
    // A zero window with require_pump_attack ON makes CHAD'S OWN TRIGGER
    // structurally unreachable while the key still reads ON -- the exact shape
    // of a secret nerf, and the loader refuses it.
    const std::string bad = replace_all(base, "regroup_attack_window_s = 10.0",
                                        "regroup_attack_window_s = 0.0");
    CHECK_THROWS(cfg::load_scenario_toml(write_temp(bad, "d2_no_window"), kAp));
}

// ⚠ NO COMMA IN THIS NAME (house rule) -- see test_drone.cpp's D2 note.
TEST_CASE("load_scenario: D2's own walk-back LOADS through the real loader") {
    const std::string base = slurp(SEADS_CONFIG_DIR "/scenario.toml");
    REQUIRE(base.find("regroup_frac_arm     = 0.667") != std::string::npos);
    const std::string off = replace_all(base, "regroup_frac_arm     = 0.667",
                                        "regroup_frac_arm     = 0.0");
    const cfg::ScenarioParams s =
        cfg::load_scenario_toml(write_temp(off, "d2_walkback"), kAp);
    CHECK(s.drone.regroup_frac_arm == 0.0);
    // ★ AND THE PAIR-CHECKS MUST NOT FIRE ON THE OFF VALUE. This is exactly
    // the E8.1 brick: a walk-back that throws at config load is not a
    // walk-back, it is a broken build.
    CHECK(s.drone.regroup_frac_lip > 0.0);
}
