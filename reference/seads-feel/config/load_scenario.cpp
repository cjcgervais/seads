#include "config/load_scenario.h"

#include <cmath>
#include <stdexcept>
#include <string>
#include <toml++/toml.hpp>

#include "sim/aero.h"  // sim::rho_at (MB-atm drone stall guard)

namespace cfg {

namespace {

constexpr double kPi = 3.14159265358979323846;
double rad(double deg) { return deg * kPi / 180.0; }

double require(const toml::table& root, const char* section, const char* key) {
    const toml::node* node =
        root.at_path(std::string(section) + "." + key).node();
    if (node == nullptr || !node->is_number()) {
        throw std::runtime_error(std::string("scenario.toml: missing or "
                                             "non-numeric key [") +
                                 section + "] " + key);
    }
    return node->value_or(0.0);
}

void check(bool ok, const char* what) {
    if (!ok) {
        throw std::runtime_error(std::string("scenario.toml: invalid value: ") +
                                 what);
    }
}

}  // namespace

ScenarioParams load_scenario_toml(const std::string& path,
                                  const sim::AircraftParams& ap) {
    toml::table root;
    try {
        root = toml::parse_file(path);
    } catch (const toml::parse_error& e) {
        throw std::runtime_error(std::string("scenario.toml parse error: ") +
                                 e.what());
    }

    ScenarioParams s;

    s.drone.count = static_cast<int>(require(root, "drone", "count"));
    s.drone.size = require(root, "drone", "size");
    s.drone.straight_time = require(root, "drone", "straight_time");
    s.drone.turn_time = require(root, "drone", "turn_time");
    s.drone.turn_bank = rad(require(root, "drone", "turn_bank_deg"));
    s.drone.throttle = require(root, "drone", "throttle");
    s.drone.speed = require(root, "drone", "speed");
    s.drone.spawn_ahead = require(root, "drone", "spawn_ahead");
    s.drone.spawn_side = require(root, "drone", "spawn_side");
    s.drone.spawn_alt = require(root, "drone", "spawn_alt");
    s.drone.spread_m = require(root, "drone", "spread_m");

    s.gunsight.muzzle_speed = require(root, "gunsight", "muzzle_speed");
    s.gunsight.hit_cone_cos =
        std::cos(rad(require(root, "gunsight", "hit_cone_deg")));
    s.gunsight.max_range = require(root, "gunsight", "max_range");
    s.gunsight.track_range = require(root, "gunsight", "track_range");
    s.gunsight.track_cone_cos =
        std::cos(rad(require(root, "gunsight", "track_cone_deg")));

    // ---- sanity ---------------------------------------------------------
    check(s.drone.count >= 1 && s.drone.count <= 200,
          "drone count in [1, 200]");
    check(s.drone.size > 0.0, "drone size > 0");
    check(s.drone.straight_time > 0.0, "drone straight_time > 0");
    check(s.drone.turn_time >= 0.0, "drone turn_time >= 0 (0 = never turn)");
    // turn_bank: the held bank during a turn leg. Capped at 45 deg — the drone
    // autopilot has NO AoA/G protection (drone.h), so it must stay in the
    // docile envelope where a level turn is sustainable (n = 1/cos(45) = 1.41,
    // well within Cl_max) and stability is verified (test_drone). A steeper
    // "gentle turn" would spiral/descend uncontrolled. 0 = never bank
    // (straight).
    check(s.drone.turn_bank >= 0.0 && s.drone.turn_bank <= 45.0 * kPi / 180.0,
          "drone turn_bank_deg in [0, 45] (autopilot docile envelope)");
    check(s.drone.throttle >= 0.0 && s.drone.throttle <= 1.0,
          "drone throttle in [0, 1]");
    check(s.drone.spawn_alt > 0.0, "drone spawn_alt > 0 (airborne)");
    check(s.drone.spread_m > 0.0, "drone spread_m > 0");

    // ---- docile-envelope guards, cross-checked against the airframe --------
    // The autopilot has NO stall/G protection and no speed awareness (drone.h),
    // so the envelope is a joint function of bank, SPEED, and throttle — not
    // bank alone (Fable P1-1). Floor the other two axes:
    //   speed: 1.5 * V_stall (level, n=1) so a 45 deg turn (n=1.41) stays clear
    //     of the stall the pitch loop would otherwise chase into a spiral.
    //   throttle: >= 0.4 so level flight is not thrust-infeasible (a perpetual
    //     sink -> crash carousel). Not exact trim (speed-dependent), a floor.
    // MB-atm: stall speed at the drones' OWN spawn altitude (a sea-level
    // v_stall silently disarms this guard if spawn_alt retunes above the
    // taper, where stall rises 1/sqrt(atm_frac) and the unprotected PD
    // autopilot would chase into the spiral this check exists to prevent —
    // the S8-drone "calibrated to today's table" class, recurring inside
    // the very check that lesson added).
    const double v_stall =
        std::sqrt(2.0 * ap.mass * ap.g /
                  (sim::rho_at(s.drone.spawn_alt, ap) * ap.S * ap.Cl_max));
    check(s.drone.speed >= 1.5 * v_stall,
          "drone speed >= 1.5 * V_stall (docile envelope, above stall)");
    check(s.drone.throttle >= 0.4 && s.drone.throttle <= 1.0,
          "drone throttle in [0.4, 1] (level flight thrust-feasible)");

    check(s.gunsight.muzzle_speed > 0.0, "gunsight muzzle_speed > 0");
    // The ballistic bracket+bisect is robust (single root, no scan-cell skip,
    // TTI < kBallisticTMax) ONLY while muzzle_speed exceeds the max closing
    // speed (two planes head-on at redline) plus the gravity drift over the cap
    // — else f(t) goes non-monotone and a valid shot can hide or time out
    // (Fable P1-3). Pin it cross-file so a slow-tracer retune fails LOUD.
    check(s.gunsight.muzzle_speed >=
              2.0 * ap.v_redline + ap.g * render::kBallisticTMax,
          "gunsight muzzle_speed >= 2*v_redline + g*kBallisticTMax (ballistic "
          "solve stays monotone within the time cap)");
    check(s.gunsight.max_range > 0.0, "gunsight max_range > 0");
    check(s.gunsight.track_range >= s.gunsight.max_range,
          "gunsight track_range >= max_range");
    // cone half-angle in (0, 90): cos in (0, 1). Below the -0.08 latch band so
    // the widened latched cone stays a valid cos.
    check(s.gunsight.track_cone_cos > 0.1 && s.gunsight.track_cone_cos < 1.0,
          "gunsight track_cone_deg in (0, ~84)");
    // hit_cone_deg in (0, 90): cos in (0, 1). A degenerate cone (<=0 or >=90
    // deg) is not a gunsight.
    check(s.gunsight.hit_cone_cos > 0.0 && s.gunsight.hit_cone_cos < 1.0,
          "gunsight hit_cone_deg in (0, 90)");

    return s;
}

}  // namespace cfg
