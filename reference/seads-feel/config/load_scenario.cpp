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
                                  const sim::AircraftParams& ap,
                                  double cannon_drag_k) {
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
    s.drone.hp = require(root, "drone", "hp");
    s.drone.hit_radius_m = require(root, "drone", "hit_radius_m");

    // ---- [combat] bandit return fire + pursuit AI (bandit_combat_plan.md) ---
    // Difficulty + player stakes + bandit gun -> s.combat (CombatSetup); the
    // pursuit-flight knobs -> s.drone (they steer the drone autopilot). Angles
    // convert deg->rad / deg->cos at the boundary. All required (strict schema).
    s.combat.difficulty =
        static_cast<int>(require(root, "combat", "difficulty"));
    s.combat.player_hp = require(root, "combat", "player_hp");
    s.combat.player_hit_radius_m =
        require(root, "combat", "player_hit_radius_m");
    s.combat.respawn_invuln_time =
        require(root, "combat", "respawn_invuln_time");
    s.combat.bandit_convergence = require(root, "combat", "bandit_convergence");
    s.combat.bandit_gun.muzzle_body = glm::dvec3{0.0};  // single forward gun @ CG
    s.combat.bandit_gun.kind = weapon::Round::Cannon20mm;
    s.combat.bandit_gun.muzzle_speed =
        require(root, "combat", "bandit_muzzle_speed");
    s.combat.bandit_gun.drag_k = require(root, "combat", "bandit_drag_k");
    // rof_hz + damage are set from difficulty_params at fire time — not here.

    s.drone.engage_range = require(root, "combat", "engage_range");
    s.drone.disengage_range = require(root, "combat", "disengage_range");
    s.drone.pursue_k_az = require(root, "combat", "pursue_k_az");
    s.drone.pursue_k_el = require(root, "combat", "pursue_k_el");
    s.drone.pursue_max_bank = rad(require(root, "combat", "pursue_max_bank_deg"));
    s.drone.pursue_max_gamma =
        rad(require(root, "combat", "pursue_max_gamma_deg"));
    s.drone.pursue_bank_gain = require(root, "combat", "pursue_bank_gain");
    s.drone.pursue_pitch_gain = require(root, "combat", "pursue_pitch_gain");
    s.drone.pursue_track_pitch_gain =  // E7.4
        require(root, "combat", "pursue_track_pitch_gain");
    s.drone.pursue_pull = rad(require(root, "combat", "pursue_pull_deg"));
    s.drone.pursue_lead_speed = require(root, "combat", "pursue_lead_speed");
    s.drone.pursue_lead_max_s = require(root, "combat", "pursue_lead_max_s");
    s.drone.fire_cone_cos =
        std::cos(rad(require(root, "combat", "fire_cone_deg")));
    s.drone.fire_range_min = require(root, "combat", "fire_range_min");
    s.drone.fire_range_max = require(root, "combat", "fire_range_max");
    s.drone.pursue_speed_bump = require(root, "combat", "pursue_speed_bump");
    s.drone.pursue_climb_throttle_gain =
        require(root, "combat", "pursue_climb_throttle_gain");

    // ---- RUNG E1 (docs/ENEMY_AI_E1_E2_SPEC.md) ------------------------------
    // The kill-chain dials that live on DroneParams (the bfm_* ones load with
    // the rest of the BFM block below). Each has an OFF value that reproduces
    // the pre-E1 tick bit-for-bit; the airframe cross-checks are in the
    // [combat] sanity block.
    s.drone.snapshot_range_m = require(root, "combat", "snapshot_range_m");
    s.drone.throttle_ff = require(root, "combat", "throttle_ff");
    s.drone.bank_slew_dps = require(root, "combat", "bank_slew_dps");

    // ---- BFM rung R1 (docs/bandit_bfm_r1_spec.md; drone/bfm.h) --------------
    // The dwell-latched dogfight state machine. enabled is a bool (require()
    // handles only numbers); the rest convert deg->rad at the boundary (the
    // maverick precedent above).
    {
        const toml::node* en = root.at_path("combat.bfm_enabled").node();
        if (en == nullptr || !en->is_boolean()) {
            throw std::runtime_error(
                "scenario.toml: missing or non-boolean key [combat] bfm_enabled");
        }
        s.drone.bfm.enabled = en->value_or(false);
    }
    s.drone.bfm.attack_range_m = require(root, "combat", "bfm_attack_range_m");
    s.drone.bfm.attack_release_frac =
        require(root, "combat", "bfm_attack_release_frac");
    s.drone.bfm.lag_dist_m = require(root, "combat", "bfm_lag_dist_m");
    s.drone.bfm.lag_off_lo = rad(require(root, "combat", "bfm_lag_off_lo_deg"));
    s.drone.bfm.lag_off_hi = rad(require(root, "combat", "bfm_lag_off_hi_deg"));
    s.drone.bfm.yoyo_closure_mps =
        require(root, "combat", "bfm_yoyo_closure_mps");
    s.drone.bfm.yoyo_angle = rad(require(root, "combat", "bfm_yoyo_angle_deg"));
    s.drone.bfm.yoyo_arm_s = require(root, "combat", "bfm_yoyo_arm_s");
    s.drone.bfm.yoyo_gamma = rad(require(root, "combat", "bfm_yoyo_gamma_deg"));
    s.drone.bfm.yoyo_bank_frac = require(root, "combat", "bfm_yoyo_bank_frac");
    s.drone.bfm.yoyo_time_s = require(root, "combat", "bfm_yoyo_time_s");
    s.drone.bfm.yoyo_exit_closure_mps =
        require(root, "combat", "bfm_yoyo_exit_closure_mps");
    s.drone.bfm.extend_energy_m = require(root, "combat", "bfm_extend_energy_m");
    s.drone.bfm.extend_gamma =
        rad(require(root, "combat", "bfm_extend_gamma_deg"));
    s.drone.bfm.extend_min_s = require(root, "combat", "bfm_extend_min_s");
    s.drone.bfm.extend_max_s = require(root, "combat", "bfm_extend_max_s");
    s.drone.bfm.reenter_energy_m =
        require(root, "combat", "bfm_reenter_energy_m");
    s.drone.bfm.frustration_s = require(root, "combat", "bfm_frustration_s");
    s.drone.bfm.intercept_speed_bump =
        require(root, "combat", "bfm_intercept_speed_bump");
    s.drone.bfm.min_dwell_s = require(root, "combat", "bfm_min_dwell_s");
    // FIX-D (cooldown duty-cycle dial): the guns-hot spell length between
    // yo-yos, independent of min_dwell_s.
    s.drone.bfm.yoyo_cooldown_s = require(root, "combat", "bfm_yoyo_cooldown_s");
    // FIX-A (air-abort hysteresis band): the ramp-value ARM/ABORT thresholds.
    s.drone.bfm.climb_abort_t = require(root, "combat", "bfm_climb_abort_t");
    // RUNG E1: the closure / energy-bail / grace dials (all OFF-valued at 0).
    s.drone.bfm.intercept_speed_mps =
        require(root, "combat", "bfm_intercept_speed_mps");
    // ★★★ RUNG S3-GUNS — the un-fade of the chase ceiling, at the INTERCEPT
    // consumer only (Extend keeps the faded ceiling: Chad's ruled asymmetry,
    // pursuit unlimited / evade contained). A REQUIRED key — the header default
    // is `false` and the header is never the shipped table (the law, paid for
    // eight times). Ships TRUE.
    {
        const auto* n =
            root["combat"]["bfm_intercept_chase_unfaded"].as_boolean();
        if (n == nullptr)
            throw std::runtime_error(
                "scenario.toml: missing or non-boolean key [combat] "
                "bfm_intercept_chase_unfaded");
        s.drone.bfm.intercept_chase_unfaded = n->value_or(false);
    }
    s.drone.bfm.reenter_closure_mps =
        require(root, "combat", "bfm_reenter_closure_mps");
    s.drone.bfm.mode_blend_s = require(root, "combat", "bfm_mode_blend_s");

    // ---- RUNG E2 — THE BLACK STOPE OFFENSIVE --------------------------------
    // The strike divert's envelope was welded into drone::StrikeOrder's struct
    // defaults where no config could reach it (measured: gamma_cap 35 deg
    // against a ~42 deg required descent, inside a 90 s budget). Every key
    // here has an OFF value reproducing the pre-E2 tick bit-for-bit; the
    // airframe cross-checks are in the [combat] sanity block below.
    {
        const auto req_bool = [&](const char* key) {
            const std::string path = std::string("combat.") + key;
            const toml::node* n = root.at_path(path).node();
            if (n == nullptr || !n->is_boolean()) {
                throw std::runtime_error(
                    "scenario.toml: missing or non-boolean key [combat] " +
                    std::string(key));
            }
            return n->value_or(false);
        };
        s.drone.strike_on_order = req_bool("strike_on_order");
        s.drone.strike_defer_exit = req_bool("strike_defer_exit");
        s.drone.strike_strafe = req_bool("strike_strafe");
    }
    s.drone.strike_stagger_s = require(root, "combat", "strike_stagger_s");
    s.drone.strike_engage_m = require(root, "combat", "strike_engage_m");
    s.drone.strike_k_az = require(root, "combat", "strike_k_az");
    s.drone.strike_k_el = require(root, "combat", "strike_k_el");
    s.drone.strike_bank_cap =
        rad(require(root, "combat", "strike_bank_cap_deg"));
    s.drone.strike_gamma_cap =
        rad(require(root, "combat", "strike_gamma_cap_deg"));
    s.drone.strike_bail_s = require(root, "combat", "strike_bail_s");
    s.drone.strike_station_range_m =
        require(root, "combat", "strike_station_range_m");
    s.drone.strike_station_cos =
        require(root, "combat", "strike_station_cos");
    // ---- RUNG E18 — THE DIVE THAT ARRIVES ----------------------------------
    // 0.0 / false = OFF = the pre-E18 divert, bit-for-bit.
    s.drone.strike_attack_alt_m =
        require(root, "combat", "strike_attack_alt_m");
    {
        const auto* n = root["combat"]["strike_glide_limit"].as_boolean();
        if (n == nullptr)
            throw std::runtime_error(
                "scenario.toml: missing or non-boolean key [combat] "
                "strike_glide_limit");
        s.drone.strike_glide_limit = n->value_or(false);
    }
    s.drone.arena_fight_range_m =
        require(root, "combat", "arena_fight_range_m");
    s.drone.arena_fight_s = require(root, "combat", "arena_fight_s");
    s.drone.arena_gamma_cap =
        rad(require(root, "combat", "arena_gamma_cap_deg"));
    s.drone.arena_guard_margin_m =
        require(root, "combat", "arena_guard_margin_m");
    s.drone.arena_guard_release_m =
        require(root, "combat", "arena_guard_release_m");

    // ---- RUNG E3 — THE FELT PASS -------------------------------------------
    s.drone.transit_fight_yield_m =
        require(root, "combat", "transit_fight_yield_m");
    s.drone.strike_concurrent_max = static_cast<int>(
        std::llround(require(root, "combat", "strike_concurrent_max")));

    // ---- RUNG E6 — THE RELENTLESS PASS -------------------------------------
    {
        const auto req_bool = [&](const char* key) {
            const std::string path = std::string("combat.") + key;
            const toml::node* n = root.at_path(path).node();
            if (n == nullptr || !n->is_boolean()) {
                throw std::runtime_error(
                    "scenario.toml: missing or non-boolean key [combat] " +
                    std::string(key));
            }
            return n->value_or(false);
        };
        s.drone.extend_leash = req_bool("extend_leash");
        s.drone.raid_fight_in_place = req_bool("raid_fight_in_place");
        s.drone.slash_doctrine = req_bool("slash_doctrine");  // E7.2
    }
    s.drone.aggression_range_m =
        require(root, "combat", "aggression_range_m");

    // ---- RUNG E17 — THE ATTACK PATTERN -------------------------------------
    // Chad's ruling 2026-08-24: "keep killing the pump, not shoot it and fly
    // away." Both 0 = OFF = the pre-E17 point chase, bit-for-bit.
    s.drone.raid_attack_alt_m =
        require(root, "combat", "raid_attack_alt_m");
    s.drone.raid_reattack_m = require(root, "combat", "raid_reattack_m");

    // ---- S2-TUNNEL — THE RAID ROUTER ---------------------------------------
    // Chad's ruling 2026-08-26: raids on the surface pump go by the TUNNEL;
    // a few designated runs may still cross the deck. See the [combat] block
    // in scenario.toml for the whole measurement.
    {
        const toml::node* n =
            root.at_path("combat.raid_route_via_tunnel").node();
        if (n == nullptr || !n->is_boolean()) {
            throw std::runtime_error(
                "scenario.toml: missing or non-boolean key [combat] "
                "raid_route_via_tunnel");
        }
        s.drone.raid_route_via_tunnel = n->value_or(false);
    }
    s.drone.raid_deck_run_slots = static_cast<int>(
        std::llround(require(root, "combat", "raid_deck_run_slots")));
    s.drone.raid_route_gap_max_m =
        require(root, "combat", "raid_route_gap_max_m");
    s.drone.raid_route_stagger_s =
        require(root, "combat", "raid_route_stagger_s");
    s.drone.raid_speed_target = require(root, "combat", "raid_speed_target");

    // ---- ★★★ D2 — the reposition (stage 0) + the return (stage 1) ----------
    // All REQUIRED (the house rule: the loader IS the shipped table — a struct
    // default that is not required is a second table waiting to disagree).
    s.drone.regroup_frac_arm = require(root, "combat", "regroup_frac_arm");
    s.drone.regroup_frac_lip = require(root, "combat", "regroup_frac_lip");
    s.drone.regroup_frac_release =
        require(root, "combat", "regroup_frac_release");
    s.drone.regroup_pull_frac = require(root, "combat", "regroup_pull_frac");
    s.drone.regroup_agl_m = require(root, "combat", "regroup_agl_m");
    s.drone.regroup_max_s = require(root, "combat", "regroup_max_s");
    s.drone.regroup_attack_window_s =
        require(root, "combat", "regroup_attack_window_s");
    {
        const auto req_bool = [&](const char* key) {
            const std::string path = std::string("combat.") + key;
            const toml::node* n = root.at_path(path).node();
            if (n == nullptr || !n->is_boolean()) {
                throw std::runtime_error(
                    "scenario.toml: missing or non-boolean key [combat] " +
                    std::string(key));
            }
            return n->value_or(false);
        };
        s.drone.regroup_require_pump_attack =
            req_bool("regroup_require_pump_attack");
        s.drone.regroup_return_deck_run = req_bool("regroup_return_deck_run");
    }

    // ---- ★★★ D3 — the parabolic dive (stages 2-5). All REQUIRED, same house
    // rule: the loader IS the shipped table. The gamma is authored in DEGREES
    // at the TOML boundary and stored in radians (the [combat] *_deg
    // precedent).
    s.drone.raid_ballistic_climb_agl_m =
        require(root, "combat", "raid_ballistic_climb_agl_m");
    s.drone.raid_ballistic_climb_max_s =
        require(root, "combat", "raid_ballistic_climb_max_s");
    s.drone.raid_ballistic_dive_gamma =
        rad(require(root, "combat", "raid_ballistic_dive_gamma_deg"));
    s.drone.raid_ballistic_dive_speed =
        require(root, "combat", "raid_ballistic_dive_speed");
    s.drone.raid_ballistic_align_min =
        require(root, "combat", "raid_ballistic_align_min");
    s.drone.raid_ballistic_run_agl_m =
        require(root, "combat", "raid_ballistic_run_agl_m");

    // ---- RUNG E7 — THE KILLERS ---------------------------------------------
    // E7.1 the tiered ENGAGED bank caps (degrees at the TOML boundary, radians
    // in DroneParams — the [combat] *_deg precedent).
    s.drone.ace_bank_cap = rad(require(root, "combat", "ace_bank_cap_deg"));
    s.drone.mid_bank_cap = rad(require(root, "combat", "mid_bank_cap_deg"));
    s.drone.ace_aggression_min =
        require(root, "combat", "ace_aggression_min");
    s.drone.mid_aggression_min =
        require(root, "combat", "mid_aggression_min");
    s.drone.ace_bank_track_lo =
        rad(require(root, "combat", "ace_bank_track_lo_deg"));
    s.drone.ace_bank_track_hi =
        rad(require(root, "combat", "ace_bank_track_hi_deg"));
    s.drone.ace_bank_agl_lo_m =  // E10
        require(root, "combat", "ace_bank_agl_lo_m");
    s.drone.ace_bank_agl_hi_m =
        require(root, "combat", "ace_bank_agl_hi_m");
    // E7.2 the slasher doctrine's geometry.
    s.drone.bfm.perch_height_m = require(root, "combat", "bfm_perch_height_m");
    s.drone.bfm.perch_release_frac =
        require(root, "combat", "bfm_perch_release_frac");
    s.drone.bfm.perch_lag_m = require(root, "combat", "bfm_perch_lag_m");
    s.drone.bfm.fight_speed_mps =
        require(root, "combat", "bfm_fight_speed_mps");  // E8.2
    s.drone.bfm.perch_max_s = require(root, "combat", "bfm_perch_max_s");
    s.drone.bfm.slash_min_s = require(root, "combat", "bfm_slash_min_s");
    s.drone.bfm.slash_time_s = require(root, "combat", "bfm_slash_time_s");
    s.drone.bfm.slash_break_m = require(root, "combat", "bfm_slash_break_m");
    // E7.3 the defensive break.
    s.drone.bfm.defensive_range_m =
        require(root, "combat", "bfm_defensive_range_m");
    s.drone.bfm.defensive_cone_cos =
        std::cos(rad(require(root, "combat", "bfm_defensive_cone_deg")));
    s.drone.bfm.defensive_release_cos =
        std::cos(rad(require(root, "combat", "bfm_defensive_release_deg")));
    s.drone.bfm.defensive_arm_s =
        require(root, "combat", "bfm_defensive_arm_s");
    s.drone.bfm.defensive_memory_s =
        require(root, "combat", "bfm_defensive_memory_s");
    s.drone.bfm.defensive_max_s =
        require(root, "combat", "bfm_defensive_max_s");
    s.drone.bfm.defensive_cooldown_s =
        require(root, "combat", "bfm_defensive_cooldown_s");

    // AI self-preservation ("MAKE THEM GOOD", 2026-07-26): hard-deck/terrain
    // avoidance + the flyable-air climb ceiling.
    s.drone.avoid_agl_enter_m = require(root, "combat", "avoid_agl_enter_m");
    s.drone.avoid_agl_release_m =
        require(root, "combat", "avoid_agl_release_m");
    s.drone.avoid_air_dive_agl_m =  // E11
        require(root, "combat", "avoid_air_dive_agl_m");
    s.drone.avoid_lookahead_s = require(root, "combat", "avoid_lookahead_s");
    s.drone.avoid_gamma = rad(require(root, "combat", "avoid_gamma_deg"));
    s.drone.avoid_bank_cap = rad(require(root, "combat", "avoid_bank_cap_deg"));
    s.drone.avoid_air_frac_full =
        require(root, "combat", "avoid_air_frac_full");
    s.drone.avoid_air_frac_hard =
        require(root, "combat", "avoid_air_frac_hard");
    // ★★★ RUNG S1-DECK — the deck band, the scope probe, the track law and the
    // forward eyes. All REQUIRED (the house rule: the loader IS the shipped
    // table -- a struct default that is not required is a second table waiting
    // to disagree with this one).
    s.drone.deck_avoid_agl_enter_m =
        require(root, "combat", "deck_avoid_agl_enter_m");
    s.drone.deck_avoid_agl_release_m =
        require(root, "combat", "deck_avoid_agl_release_m");
    s.drone.deck_scope_hyst_frac =
        require(root, "combat", "deck_scope_hyst_frac");
    s.drone.deck_track_agl_m = require(root, "combat", "deck_track_agl_m");
    s.drone.deck_track_gain = require(root, "combat", "deck_track_gain");
    s.drone.deck_track_dive_cap =
        rad(require(root, "combat", "deck_track_dive_cap_deg"));
    s.drone.deck_lookahead_s = require(root, "combat", "deck_lookahead_s");
    // RUNG DF-1: the engaged deck-scope climb cap. NEGATIVE = OFF (bit-
    // identical), so the range check below admits the off-value deliberately.
    s.drone.deck_fight_climb_cap =
        rad(require(root, "combat", "deck_fight_climb_cap_deg"));
    s.drone.avoid_pull_net_g = require(root, "combat", "avoid_pull_net_g");

    // ---- ENV-2: the air-envelope governor -------------------------------
    // `require`, never a default: a hand-maintained constant that describes the
    // shipped table stops describing it the moment the table moves, and this
    // ladder has paid for that law seven times.
    // ⚠ env_shrink_radius_frac MIRRORS game.toml's [conquest] shrink_radius_frac.
    // The plateau's whole justification is "reach as far in as one shrink can
    // move the edge", so if that dial moves and this one does not, the buffer
    // silently stops insuring the thing it exists to insure. The E2 arm pins
    // them equal — it is a mirror, and a mirror needs a gate.
    s.drone.env_edge_alt_m = require(root, "combat", "env_edge_alt_m");
    s.drone.env_slope = require(root, "combat", "env_slope");
    s.drone.env_plateau_m = require(root, "combat", "env_plateau_m");
    s.drone.env_desc_cap = rad(require(root, "combat", "env_desc_cap_deg"));
    s.drone.env_desc_fade_band_m =
        require(root, "combat", "env_desc_fade_band_m");
    s.drone.env_fade_band_m = require(root, "combat", "env_fade_band_m");
    s.drone.env_shrink_radius_frac =
        require(root, "combat", "env_shrink_radius_frac");

    // ---- ENV-3: the defence scramble --------------------------------------
    s.drone.defend_sprint_speed =
        require(root, "combat", "defend_sprint_speed");
    s.drone.defend_release_m = require(root, "combat", "defend_release_m");
    s.drone.defend_dive_gamma =
        rad(require(root, "combat", "defend_dive_gamma_deg"));
    s.drone.defend_dive_close_gamma =
        rad(require(root, "combat", "defend_dive_close_gamma_deg"));
    s.drone.defend_dive_close_range_m =
        require(root, "combat", "defend_dive_close_range_m");
    // Chad's 2026-08-30 doctrine: the parabola ARMS once and EXITS on a gate.
    s.drone.defend_dive_arm_alt_m =
        require(root, "combat", "defend_dive_arm_alt_m");
    s.drone.defend_dive_exit_speed =
        require(root, "combat", "defend_dive_exit_speed");

    // ---- ENV-3.4: the perch -----------------------------------------------
    s.drone.guard_alt_lo_m = require(root, "combat", "guard_alt_lo_m");
    s.drone.guard_alt_hi_m = require(root, "combat", "guard_alt_hi_m");
    s.drone.guard_margin_m = require(root, "combat", "guard_margin_m");
    s.drone.guard_climb_gain = require(root, "combat", "guard_climb_gain");
    s.drone.guard_gamma_cap =
        rad(require(root, "combat", "guard_gamma_cap_deg"));

    // ---- [damage] component damage feel dials (docs/damage_model_plan.md) -----
    // Pure feel/controllability knobs for the damage->flight transforms; every
    // one is IDENTITY at zero damage (combat::damage_zero short-circuits), so the
    // committed values reproduce combat::DamageParams{} and the gate stays
    // bit-identical. Exposed here so the hurt-plane feel tunes without a recompile.
    s.damage.wing_cl_loss = require(root, "damage", "wing_cl_loss");
    s.damage.wing_roll_loss = require(root, "damage", "wing_roll_loss");
    s.damage.wing_drag_gain = require(root, "damage", "wing_drag_gain");
    s.damage.struct_auth_loss = require(root, "damage", "struct_auth_loss");
    s.damage.struct_drag_gain = require(root, "damage", "struct_drag_gain");
    s.damage.roll_bias_gain = require(root, "damage", "roll_bias_gain");
    s.damage.roll_bias_n_ref = require(root, "damage", "roll_bias_n_ref");
    s.damage.pilot_gain_floor = require(root, "damage", "pilot_gain_floor");
    s.damage.cl_floor = require(root, "damage", "cl_floor");
    s.damage.c_pitchyaw_floor = require(root, "damage", "c_pitchyaw_floor");
    s.damage.c_roll_floor = require(root, "damage", "c_roll_floor");
    s.damage.cd0_cap_mult = require(root, "damage", "cd0_cap_mult");
    s.damage.component_hp = require(root, "damage", "component_hp");
    s.damage.route_wing_frac = require(root, "damage", "route_wing_frac");
    s.damage.route_nose_frac = require(root, "damage", "route_nose_frac");
    s.damage.route_tail_frac = require(root, "damage", "route_tail_frac");
    s.damage.route_center_pilot = require(root, "damage", "route_center_pilot");
    s.damage.ground_wing_ref_ms = require(root, "damage", "ground_wing_ref_ms");
    s.damage.ground_wing_rate = require(root, "damage", "ground_wing_rate");

    // ---- [sled_comfort] RC roll-comfort dials (sim/sled.h SledComfort) -----
    // §0b ruling (ROLL_COMFORT_HANDOFF.md): every mechanism a named dial,
    // tunable without a recompile, each with an OFF value. The committed toml
    // reproduces sim::SledComfort{} exactly (bit-neutral load, asserted by
    // scenario_sled_comfort_matches_kernel_defaults); strict like every other
    // section — a missing key throws.
    s.sled_comfort.rolled_persist_s =
        require(root, "sled_comfort", "rolled_persist_s");
    s.sled_comfort.rolled_grace_s =
        require(root, "sled_comfort", "rolled_grace_s");
    s.sled_comfort.roll_stiff_nm =
        require(root, "sled_comfort", "roll_stiff_nm");
    s.sled_comfort.roll_ref_rad = require(root, "sled_comfort", "roll_ref_rad");
    s.sled_comfort.roll_release_lo_rad =
        require(root, "sled_comfort", "roll_release_lo_rad");
    s.sled_comfort.roll_release_hi_rad =
        require(root, "sled_comfort", "roll_release_hi_rad");
    s.sled_comfort.roll_damp_nms =
        require(root, "sled_comfort", "roll_damp_nms");
    s.sled_comfort.roll_ref_blend =
        require(root, "sled_comfort", "roll_ref_blend");
    s.sled_comfort.lean_bite_gain =
        require(root, "sled_comfort", "lean_bite_gain");
    s.sled_comfort.lean_sat_gain_rad =
        require(root, "sled_comfort", "lean_sat_gain_rad");
    // R4a seated self-right (Chad 2026-08-26). `require` on purpose: a missing
    // key is a loud failure, never a silent fall back to the 0.0 default that
    // would leave the mechanic quietly off in the game while the tests pass.
    s.sled_comfort.right_assist_nm =
        require(root, "sled_comfort", "right_assist_nm");
    s.sled_comfort.right_assist_max_ms =
        require(root, "sled_comfort", "right_assist_max_ms");
    s.sled_comfort.right_assist_rearm_frac =
        require(root, "sled_comfort", "right_assist_rearm_frac");
    s.sled_comfort.right_assist_min_tilt_rad =
        require(root, "sled_comfort", "right_assist_min_tilt_rad");
    s.sled_comfort.right_charge_push_s =
        require(root, "sled_comfort", "right_charge_push_s");
    s.sled_comfort.right_charge_rest_s =
        require(root, "sled_comfort", "right_charge_rest_s");
    s.sled_comfort.right_dir_eps =
        require(root, "sled_comfort", "right_dir_eps");
    s.sled_comfort.right_seed_frac =
        require(root, "sled_comfort", "right_seed_frac");
    s.sled_comfort.right_tilt_lo_rad =
        require(root, "sled_comfort", "right_tilt_lo_rad");
    s.sled_comfort.right_tilt_hi_rad =
        require(root, "sled_comfort", "right_tilt_hi_rad");
    s.sled_comfort.right_speed_lp_s =
        require(root, "sled_comfort", "right_speed_lp_s");
    s.sled_comfort.right_pump_omega_eps =
        require(root, "sled_comfort", "right_pump_omega_eps");
    s.sled_comfort.right_stand_shift_frac =
        require(root, "sled_comfort", "right_stand_shift_frac");
    s.sled_comfort.assist_hull_frac =
        require(root, "sled_comfort", "assist_hull_frac");
    s.sled_comfort.roll_stiff_vgain =
        require(root, "sled_comfort", "roll_stiff_vgain");
    s.sled_comfort.release_floor_frac =
        require(root, "sled_comfort", "release_floor_frac");
    s.sled_comfort.release_floor_hi_rad =
        require(root, "sled_comfort", "release_floor_hi_rad");
    s.sled_comfort.assist_v_lo_ms =
        require(root, "sled_comfort", "assist_v_lo_ms");
    s.sled_comfort.assist_v_hi_ms =
        require(root, "sled_comfort", "assist_v_hi_ms");
    s.sled_comfort.side_k = require(root, "sled_comfort", "side_k");
    s.sled_comfort.side_c = require(root, "sled_comfort", "side_c");
    s.sled_comfort.side_mu = require(root, "sled_comfort", "side_mu");
    s.sled_comfort.side_right_gain_nm =
        require(root, "sled_comfort", "side_right_gain_nm");
    s.sled_comfort.side_right_vmin_ms =
        require(root, "sled_comfort", "side_right_vmin_ms");
    s.sled_comfort.side_right_vref_ms =
        require(root, "sled_comfort", "side_right_vref_ms");
    check(s.sled_comfort.rolled_persist_s >= 0.0,
          "sled_comfort.rolled_persist_s must be >= 0");
    check(s.sled_comfort.rolled_grace_s >= 0.0,
          "sled_comfort.rolled_grace_s must be >= 0");
    check(s.sled_comfort.roll_stiff_nm >= 0.0,
          "sled_comfort.roll_stiff_nm must be >= 0");
    check(s.sled_comfort.roll_ref_rad > 0.0,
          "sled_comfort.roll_ref_rad must be > 0");
    check(s.sled_comfort.roll_release_lo_rad >= 0.0,
          "sled_comfort.roll_release_lo_rad must be >= 0");
    check(s.sled_comfort.roll_release_hi_rad >
              s.sled_comfort.roll_release_lo_rad,
          "sled_comfort.roll_release_hi_rad must exceed roll_release_lo_rad");
    check(s.sled_comfort.roll_damp_nms >= 0.0,
          "sled_comfort.roll_damp_nms must be >= 0");
    check(s.sled_comfort.roll_ref_blend >= 0.0 &&
              s.sled_comfort.roll_ref_blend <= 1.0,
          "sled_comfort.roll_ref_blend must be in [0, 1]");
    check(s.sled_comfort.lean_bite_gain >= 0.0,
          "sled_comfort.lean_bite_gain must be >= 0");
    check(s.sled_comfort.lean_sat_gain_rad >= 0.0,
          "sled_comfort.lean_sat_gain_rad must be >= 0");
    check(s.sled_comfort.assist_hull_frac >= 0.0,
          "sled_comfort.assist_hull_frac must be >= 0");
    check(s.sled_comfort.roll_stiff_vgain >= 0.0,
          "sled_comfort.roll_stiff_vgain must be >= 0");
    check(s.sled_comfort.release_floor_frac >= 0.0 &&
              s.sled_comfort.release_floor_frac <= 1.0,
          "sled_comfort.release_floor_frac must be in [0, 1]");
    check(s.sled_comfort.release_floor_hi_rad <= 1.5533,
          "sled_comfort.release_floor_hi_rad must stay under 89 deg -- a "
          "downed machine gets no push (RC-1)");
    check(s.sled_comfort.assist_v_hi_ms > s.sled_comfort.assist_v_lo_ms,
          "sled_comfort.assist_v_hi_ms must exceed assist_v_lo_ms");
    check(s.sled_comfort.assist_v_lo_ms >= 0.0,
          "sled_comfort.assist_v_lo_ms must be >= 0");
    check(s.sled_comfort.side_k >= 0.0, "sled_comfort.side_k must be >= 0");
    check(s.sled_comfort.side_c >= 0.0, "sled_comfort.side_c must be >= 0");
    check(s.sled_comfort.side_mu >= 0.0, "sled_comfort.side_mu must be >= 0");
    check(s.sled_comfort.side_right_gain_nm >= 0.0,
          "sled_comfort.side_right_gain_nm must be >= 0");
    // vmin >= 0 is the ANTI-MAGNETISM clause at the boundary (red-team P2-10):
    // a negative vmin gives wv > 0 at zero slide — parked machines would
    // self-right, the governor-in-a-costume failure the consult names.
    check(s.sled_comfort.side_right_vmin_ms >= 0.0,
          "sled_comfort.side_right_vmin_ms must be >= 0");
    check(s.sled_comfort.side_right_vref_ms >
              s.sled_comfort.side_right_vmin_ms,
          "sled_comfort.side_right_vref_ms must exceed side_right_vmin_ms");

    // ---- [maverick] THE MAVERICK SQUADRON (drone/maverick.h) ----------------
    // The tunnel-run adversary dials. `enabled` is a bool (require() handles
    // only numbers); the rest convert deg->rad at the boundary. Validated
    // against the airframe below (the docile-envelope discipline, extended to
    // the bore-speed ceiling).
    {
        const toml::node* en = root.at_path("maverick.enabled").node();
        if (en == nullptr || !en->is_boolean()) {
            throw std::runtime_error(
                "scenario.toml: missing or non-boolean key [maverick] enabled");
        }
        s.drone.maverick.enabled = en->value_or(false);
    }
    s.drone.maverick.base_speed = require(root, "maverick", "base_speed");
    s.drone.maverick.tunnel_speed_max =
        require(root, "maverick", "tunnel_speed_max");
    s.drone.maverick.transit_speed_scale =
        require(root, "maverick", "transit_speed_scale");
    s.drone.maverick.lookahead_m = require(root, "maverick", "lookahead_m");
    s.drone.maverick.dive_lookahead_m =
        require(root, "maverick", "dive_lookahead_m");
    s.drone.maverick.centerline_gain =
        require(root, "maverick", "centerline_gain");
    s.drone.maverick.gamma_lookahead_m =
        require(root, "maverick", "gamma_lookahead_m");
    s.drone.maverick.slope_ff_gain =
        require(root, "maverick", "slope_ff_gain");
    s.drone.maverick.track_gain = require(root, "maverick", "track_gain");
    s.drone.maverick.climb_throttle_gain =
        require(root, "maverick", "climb_throttle_gain");
    s.drone.maverick.climb_speed_min =
        require(root, "maverick", "climb_speed_min");
    s.drone.maverick.wall_margin_m = require(root, "maverick", "wall_margin_m");
    s.drone.maverick.wall_margin_release_m =
        require(root, "maverick", "wall_margin_release_m");
    s.drone.maverick.wall_margin_floor_m =
        require(root, "maverick", "wall_margin_floor_m");
    s.drone.maverick.dive_gamma_cap =
        rad(require(root, "maverick", "dive_gamma_cap_deg"));
    s.drone.maverick.run_gamma_cap =
        rad(require(root, "maverick", "run_gamma_cap_deg"));
    s.drone.maverick.transit_gamma_cap =
        rad(require(root, "maverick", "transit_gamma_cap_deg"));
    s.drone.maverick.bank_cap = rad(require(root, "maverick", "bank_cap_deg"));
    // ---- RUNG E17 — the arena spiral fix + the RUN's own bail --------------
    // All three are 0-OFF and reproduce the pre-E17 machine bit-for-bit.
    s.drone.maverick.run_recover_alt_m =
        require(root, "maverick", "run_recover_alt_m");
    s.drone.maverick.run_recover_bank_cap =
        rad(require(root, "maverick", "run_recover_bank_cap_deg"));
    s.drone.maverick.run_stall_s = require(root, "maverick", "run_stall_s");
    s.drone.maverick.run_stall_arc_m =
        require(root, "maverick", "run_stall_arc_m");
    s.drone.maverick.bank_p = require(root, "maverick", "bank_p");
    s.drone.maverick.pitch_p = require(root, "maverick", "pitch_p");
    s.drone.maverick.k_az = require(root, "maverick", "k_az");
    s.drone.maverick.k_el = require(root, "maverick", "k_el");
    s.drone.maverick.exit_frac = require(root, "maverick", "exit_frac");
    s.drone.maverick.period_scale = require(root, "maverick", "period_scale");
    s.drone.maverick.engage_aggression_min =
        require(root, "maverick", "engage_aggression_min");
    // RUNG E2.1 FIX: the final-leg grace on the TRANSIT budget (0 = off,
    // bit-identical to the pre-fix machine).
    s.drone.maverick.transit_fix_grace_s =
        require(root, "maverick", "transit_fix_grace_s");
    s.drone.maverick.transit_reach_s_per_km =
        require(root, "maverick", "transit_reach_s_per_km");

    s.gunsight.muzzle_speed = require(root, "gunsight", "muzzle_speed");
    s.gunsight.hit_cone_cos =
        std::cos(rad(require(root, "gunsight", "hit_cone_deg")));
    s.gunsight.max_range = require(root, "gunsight", "max_range");
    s.gunsight.track_range = require(root, "gunsight", "track_range");
    s.gunsight.track_cone_cos =
        std::cos(rad(require(root, "gunsight", "track_cone_deg")));
    // FIX 5 (Fable: single-source drag k). drag_k is NOT read from
    // scenario.toml; it is derived from the world.toml [guns] cannon_drag_k
    // passed by the caller.  This eliminates the silent-fork risk of two
    // independent copies drifting.  See load_scenario.h for the contract.
    s.gunsight.drag_k = cannon_drag_k;
    // FIX 2 (Fable P1): thread the sim tick so the pipper's discretization-lag
    // correction matches weapon::advance exactly — single source from aircraft.toml.
    s.gunsight.fire_dt = ap.sim_dt;

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
    check(s.drone.hp > 0.0, "drone hp > 0");
    check(s.drone.hit_radius_m > 0.0, "drone hit_radius_m > 0 (collision radius)");

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
    // R6 note (Fable-BEFORE §2, the structural straggler): this stays on the
    // ALTITUDE rho_at — a config-load feasibility check has NO world position
    // (drones are placed later, at runtime). It assumes drones spawn inside
    // breathable air (their town bubble); a drone spawned into a vacuum gap is
    // a scenario-authoring error, not a stall this guard should model.
    const double v_stall =
        std::sqrt(2.0 * ap.mass * ap.g /
                  (sim::rho_at(s.drone.spawn_alt, ap) * ap.S * ap.Cl_max));
    check(s.drone.speed >= 1.5 * v_stall,
          "drone speed >= 1.5 * V_stall (docile envelope, above stall)");
    check(s.drone.throttle >= 0.4 && s.drone.throttle <= 1.0,
          "drone throttle in [0.4, 1] (level flight thrust-feasible)");

    check(s.gunsight.muzzle_speed > 0.0, "gunsight muzzle_speed > 0");
    // FIX 4 (Fable P2): stale tripwire rationale updated.
    // The old comment claimed "f strictly decreasing → single root" via the
    // vacuum dragged_range being monotone.  Under drag the RANGE ENVELOPE
    // saturates (a round asymptotically slows; it never reverses, but the
    // far-time domain is useless for in-range intercepts).  The world-frame
    // solver caps its search at 1.1*max_range dragged TOF (render/gunsight.cpp)
    // so it never chases the saturated far envelope.  The tripwire here is
    // therefore a CONSERVATIVE guard: it ensures muzzle_speed exceeds the max
    // closing speed (head-on at redline), which keeps the initial t=|p|/m
    // estimate sane and the iteration convergent for any in-range target.
    // 805 m/s >> 588 m/s bound — assert still holds; reason is now correct.
    check(s.gunsight.muzzle_speed >=
              2.0 * ap.v_redline + ap.g * render::kBallisticTMax,
          "gunsight muzzle_speed >= 2*v_redline + g*kBallisticTMax (retune "
          "tripwire: ensures muzzle_speed covers max closing speed so the "
          "world-frame iteration starts from a sane estimate; the far-envelope "
          "saturation is handled by the 1.1*max_range cap in lead_solution)");
    // drag_k is derived from cannon_drag_k (world config), not read from toml.
    check(s.gunsight.drag_k >= 0.0,
          "gunsight drag_k >= 0 (derived from world.guns.cannon_drag_k)");
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

    // ---- [combat] sanity (bandit_combat_plan.md §6) --------------------------
    // difficulty is the int master knob; the mapping guarantees max_engaged >= 0
    // and rof > 0, so only the level range needs guarding.
    check(s.combat.difficulty >= 1 && s.combat.difficulty <= 5,
          "combat difficulty in [1, 5]");
    check(s.combat.player_hp > 0.0, "combat player_hp > 0");
    check(s.combat.player_hit_radius_m > 0.0,
          "combat player_hit_radius_m > 0");
    check(s.combat.respawn_invuln_time >= 0.0,
          "combat respawn_invuln_time >= 0");
    check(s.combat.bandit_gun.muzzle_speed > 0.0,
          "combat bandit_muzzle_speed > 0");
    check(s.combat.bandit_gun.drag_k >= 0.0, "combat bandit_drag_k >= 0");
    // Convergence >= the 50 m harmonisation floor (the same floor the player
    // battery uses; below it harmonization_rise is ill-posed).
    check(s.combat.bandit_convergence >= 50.0,
          "combat bandit_convergence >= 50 m");
    // Engage band: disengage strictly beyond engage (the hysteresis gap).
    check(s.drone.engage_range > 0.0, "combat engage_range > 0");
    check(s.drone.disengage_range > s.drone.engage_range,
          "combat disengage_range > engage_range (hysteresis gap)");
    check(s.drone.pursue_k_az > 0.0, "combat pursue_k_az > 0");
    check(s.drone.pursue_k_el > 0.0, "combat pursue_k_el > 0");
    // Pursuit bank capped at ~65 deg (55->65, furball repro fix, 2026-07-26:
    // more roll authority to cut the corner on a long-range intercept instead
    // of settling into a matched-rotation orbit around a maneuvering player).
    // A full-power attacker runs faster (higher q) than the docile patrol
    // test, so it can hold a steeper bank than the 45 deg patrol ceiling
    // without departing (at 95 m/s: 55 deg -> n=1.74, V_stall ~90 < 95, so
    // V_stall_1g ~= 90/sqrt(1.74) ~= 68 m/s). This 55 deg ceiling is a
    // documented STALL-SAFETY guard: the drone autopilot has no G-protection
    // besides the soft AoA limiter (aoa_protect, always on in pursuit), and
    // above ~55 deg at pursuit q it risks departure. REVERTED to 55 (was
    // briefly raised to 65 during the 2026-07-26 furball retune; more bank
    // does not even help pointing at high V — turn rate = g*tan(bank)/V is
    // V-dominated — so the pointing fix lives in speed/gain tuning within
    // this ceiling, not a wider bank envelope).
    check(s.drone.pursue_max_bank >= 0.0 &&
              s.drone.pursue_max_bank <= 55.0 * kPi / 180.0,
          "combat pursue_max_bank_deg in [0, 55] (attacker envelope, stall-safe)");
    check(s.drone.pursue_max_gamma >= 0.0 &&
              s.drone.pursue_max_gamma <= 60.0 * kPi / 180.0,
          "combat pursue_max_gamma_deg in [0, 60]");
    check(s.drone.pursue_bank_gain > 0.0, "combat pursue_bank_gain > 0");
    check(s.drone.pursue_pitch_gain > 0.0, "combat pursue_pitch_gain > 0");
    // E7.4: the ENGAGED-TRACKING pitch gain. 0 = off (the errand gain is used
    // everywhere). It may not EXCEED pursue_pitch_gain: this dial exists to
    // repeal a measured over-gain in the tracking task, and a value above the
    // errand gain would be a second, unruled way to raise combat pitch
    // authority rather than the one-way relief it is documented as.
    check(s.drone.pursue_track_pitch_gain >= 0.0 &&
              s.drone.pursue_track_pitch_gain <= s.drone.pursue_pitch_gain,
          "combat pursue_track_pitch_gain in [0, pursue_pitch_gain] "
          "(0 = off = the errand gain at every seam)");
    check(s.drone.pursue_pull >= 0.0, "combat pursue_pull_deg >= 0");
    check(s.drone.pursue_lead_speed >= 0.0, "combat pursue_lead_speed >= 0");
    check(s.drone.pursue_lead_max_s > 0.0, "combat pursue_lead_max_s > 0");
    // fire_cone_deg in (0, 90): cos in (0, 1).
    check(s.drone.fire_cone_cos > 0.0 && s.drone.fire_cone_cos < 1.0,
          "combat fire_cone_deg in (0, 90)");
    check(s.drone.fire_range_min >= 0.0, "combat fire_range_min >= 0");
    check(s.drone.fire_range_max > s.drone.fire_range_min,
          "combat fire_range_max > fire_range_min");
    check(s.drone.pursue_speed_bump >= 0.0, "combat pursue_speed_bump >= 0");
    check(s.drone.pursue_climb_throttle_gain >= 0.0,
          "combat pursue_climb_throttle_gain >= 0");

    // ---- RUNG E1 sanity (docs/ENEMY_AI_E1_E2_SPEC.md) -----------------------
    // Same discipline as the maverick speed/gamma guards below: config-
    // RELATIVE to the airframe, never bare numbers calibrated to today's
    // table. Every dial's OFF value (0) is legal.
    check(s.drone.snapshot_range_m >= 0.0, "combat snapshot_range_m >= 0");
    // The snapshot band is a STRUCTURAL bound on where a round may be spawned
    // (the anti-lobbing guarantee), so it must sit inside the engagement it is
    // taken within and above the spawn-on-top floor.
    check(s.drone.snapshot_range_m == 0.0 ||
              s.drone.snapshot_range_m > s.drone.fire_range_min,
          "combat snapshot_range_m > fire_range_min (or 0 = off)");
    check(s.drone.snapshot_range_m <= s.drone.engage_range,
          "combat snapshot_range_m <= engage_range (no lobbing beyond the "
          "engagement the foe assignment actually granted)");
    check(s.drone.throttle_ff >= 0.0, "combat throttle_ff >= 0");
    // The deficit feedforward only ADDS throttle into a [0, dp.throttle]
    // clamp, so it cannot destabilise the speed loop; bound it anyway so a
    // typo cannot turn the autothrottle into a bang-bang switch on a 1 m/s
    // ripple (one full throttle range per m/s of error).
    check(s.drone.throttle_ff <= 1.0,
          "combat throttle_ff <= 1 (throttle per m/s of deficit)");
    check(s.drone.bank_slew_dps >= 0.0, "combat bank_slew_dps >= 0");
    // A slew slower than the roll loop can actually track is a lie; a slew
    // that cannot cross the pursuit bank envelope inside a second would make
    // the bank cap unreachable in a real reversal. Config-relative: the cap
    // must be crossable in <= 1 s.
    check(s.drone.bank_slew_dps == 0.0 ||
              s.drone.bank_slew_dps >= s.drone.pursue_max_bank * 180.0 / kPi,
          "combat bank_slew_dps >= pursue_max_bank_deg (the commanded bank "
          "envelope must stay crossable within a second) or 0 = off");

    // ---- RUNG E7 sanity (docs/ENEMY_AI_E1_E2_SPEC.md) -----------------------
    //
    // E7.1 THE NAMED REPEAL, and its safety case. pursue_max_bank keeps its
    // ruled [0, 55] bound above — this dial does not widen it, it adds a
    // SEPARATE, ENGAGED-ONLY cap that only drone::engaged_bank_cap can hand
    // out. The bound here is the airframe's, not the ruling's.
    // ★★★ THE BOUND IS RIGHT AND ITS OLD REASON WAS WRONG (rung E10, Chad
    // 2026-08-23 "make them good"). It read 80 with the rationale "80 deg is
    // n = 5.8, past the airframe's usable load factor". The airframe's usable
    // load factor is `n_max` = 32 (controller.toml) and the engaged path flies
    // with aoa_protect ON, so 5.8 was never the limit. The rung went looking to
    // RAISE this bound and the PLANT refused (spec §E10.4, saturated turn,
    // ACHIEVED bank as a fraction of the command, deadband off):
    //     cap    30 deg   50 deg   70 deg   90 deg
    //     72      101%     105%     105%     105%
    //     80      101%     103%     103%     102%
    //     83      102%      54%      50%      46%   <- 103-120 deg of swing
    // Between 80 and 83 the aeroplane stops FLYING the bank it is handed: it
    // mushes to 38-45 deg and the flight path thrashes. 80 is the measured edge
    // of the envelope, so the number stays and the reason is now a measurement.
    check(s.drone.ace_bank_cap >= 0.0 &&
              s.drone.ace_bank_cap <= 80.0 * kPi / 180.0,
          "combat ace_bank_cap_deg in [0, 80] (0 = off; past 80 the plant "
          "mushes the commanded bank -- measured, spec E10.4)");
    check(s.drone.ace_bank_cap == 0.0 ||
              s.drone.ace_bank_cap >= s.drone.pursue_max_bank,
          "combat ace_bank_cap_deg >= pursue_max_bank_deg (the tier may only "
          "RAISE the ruled cap; lowering it is what pursue_max_bank_deg is "
          "for) or 0 = off");
    check(s.drone.mid_bank_cap >= 0.0 &&
              (s.drone.ace_bank_cap == 0.0 ||
               s.drone.mid_bank_cap <= s.drone.ace_bank_cap),
          "combat mid_bank_cap_deg in [0, ace_bank_cap_deg] (the middle tier "
          "never out-turns the aces)");
    check(s.drone.mid_bank_cap == 0.0 ||
              s.drone.mid_bank_cap >= s.drone.pursue_max_bank,
          "combat mid_bank_cap_deg >= pursue_max_bank_deg or 0 = off");
    // ---- RUNG E10 -- THE ALTITUDE FADE ON THE ENGAGED CAP ----------------
    // hi <= lo is the ruled OFF form (no fade), so the ORDERING is deliberately
    // not checked -- only that neither is negative, and that a fade which IS
    // armed starts no lower than the forced pull-up's own release altitude.
    // Below that the terrain-avoid branch owns the aeroplane anyway, so a fade
    // that reached further down would be describing a regime it does not
    // control.
    check(s.drone.ace_bank_agl_lo_m >= 0.0 && s.drone.ace_bank_agl_hi_m >= 0.0,
          "combat ace_bank_agl_lo_m / ace_bank_agl_hi_m >= 0 (hi <= lo = off)");
    check(s.drone.ace_bank_agl_hi_m <= s.drone.ace_bank_agl_lo_m ||
              s.drone.ace_bank_agl_lo_m >= s.drone.avoid_agl_release_m,
          "combat ace_bank_agl_lo_m >= avoid_agl_release_m when the fade is "
          "armed (below the pull-up's release the avoid branch owns the "
          "aeroplane, rung E10)");
    check(s.drone.ace_aggression_min > 0.0 &&
              s.drone.ace_aggression_min <= 1.0,
          "combat ace_aggression_min in (0, 1]");
    check(s.drone.mid_aggression_min > 0.0 &&
              s.drone.mid_aggression_min <= s.drone.ace_aggression_min,
          "combat mid_aggression_min in (0, ace_aggression_min]");
    // The tracking deadband. lo >= hi is the legal OFF form (a FLAT tier cap),
    // so the ordering is not checked -- only the envelope.
    check(s.drone.ace_bank_track_lo >= 0.0 &&
              s.drone.ace_bank_track_lo <= kPi,
          "combat ace_bank_track_lo_deg in [0, 180]");
    check(s.drone.ace_bank_track_hi >= 0.0 &&
              s.drone.ace_bank_track_hi <= kPi,
          "combat ace_bank_track_hi_deg in [0, 180] (<= lo = a FLAT tier cap)");
    // AGAINST THE AIRFRAME, config-relative (the v_stall precedent above, never
    // a bare number): a sustained bank phi needs n = 1/cos(phi), and holding n
    // needs V >= V_stall * sqrt(n). The speed an ACE actually fights at nose-on
    // is its cruise plus the full pursuit bump (the corner-speed law fades that
    // bump off-nose, and off-nose is exactly where the AoA limiter is supposed
    // to mush the turn — that is measured in test_enemy_ai_e7, not asserted
    // here). The check is that the raised cap is flyable where it matters.
    if (s.drone.ace_bank_cap > 0.0) {
        const double n_ace = 1.0 / std::max(1e-9, std::cos(s.drone.ace_bank_cap));
        check(s.drone.speed + s.drone.pursue_speed_bump >=
                  v_stall * std::sqrt(n_ace),
              "combat ace_bank_cap_deg: the tier's nose-on fight speed "
              "(speed + pursue_speed_bump) must clear V_stall * sqrt(n) for "
              "the raised bank's load factor");
    }
    // The E1.4 slew must still be able to cross the RAISED envelope inside a
    // second, or the ace's new bank authority is a number it never reaches.
    check(s.drone.bank_slew_dps == 0.0 ||
              s.drone.bank_slew_dps >= s.drone.ace_bank_cap * 180.0 / kPi,
          "combat bank_slew_dps >= ace_bank_cap_deg (the RAISED engaged bank "
          "envelope must stay crossable within a second)");

    // E7.2 the slasher doctrine.
    check(s.drone.bfm.perch_height_m >= 0.0,
          "combat bfm_perch_height_m >= 0 (0 = the doctrine is unavailable)");
    check(s.drone.bfm.perch_release_frac > 0.0 &&
              s.drone.bfm.perch_release_frac < 1.0,
          "combat bfm_perch_release_frac in (0, 1) (the Perch<->Slash "
          "hysteresis gap)");
    check(s.drone.bfm.perch_max_s > 0.0, "combat bfm_perch_max_s > 0");
    check(s.drone.bfm.slash_min_s > 0.0, "combat bfm_slash_min_s > 0");
    check(s.drone.bfm.slash_time_s > s.drone.bfm.slash_min_s,
          "combat bfm_slash_time_s > bfm_slash_min_s (the pass ceiling must "
          "sit above its own commit floor)");
    check(s.drone.bfm.slash_break_m >= 0.0, "combat bfm_slash_break_m >= 0");
    check(s.drone.bfm.slash_break_m < s.drone.bfm.attack_range_m,
          "combat bfm_slash_break_m < bfm_attack_range_m (a break-off range "
          "beyond the merge would end every pass on its first tick)");
    // A slasher's whole doctrine is that it never sustained-turns, so its guns
    // are only ever hot in Slash: if the pass cannot outlive the anti-chatter
    // dwell the doctrine has repealed the guns.
    check(s.drone.bfm.perch_height_m == 0.0 ||
              s.drone.bfm.slash_min_s >= s.drone.bfm.min_dwell_s,
          "combat bfm_slash_min_s >= bfm_min_dwell_s (the guns-hot pass is "
          "the doctrine's only firing window)");
    // ---- RUNG E8.1 — THE DIVE-ENVELOPE TRIPWIRE ---------------------------
    // ★ THIS IS THE CHECK WHOSE ABSENCE LET AN UN-DIVEABLE PERCH SHIP GREEN.
    // The slasher dives from perch_height_m above a point perch_lag_m behind
    // him; that dive needs atan(height/lag) of nose-down and the steering law
    // clamps it at pursue_max_gamma. At the pre-E8 dials (700 m over the 220 m
    // TRACKING lag point) the dive needed 72.6 deg against a 30 deg clamp, so
    // the pass could never point at the target and probe P-S measured a whole
    // pass at a best cone cos of -0.106 (96 deg off) and ZERO rounds. Every
    // E7.2 test passed: they asserted the guns_hot MODE FLAG.
    //
    // The margin (not merely "<= max_gamma") is deliberate: a dive sitting ON
    // the clamp tracks nothing -- the law needs authority left over to correct
    // onto the lead point as it closes. A future perch retune that breaks the
    // envelope now fails at CONFIG TIME with the geometry in the message.
    {
        const double perch_lag = s.drone.bfm.perch_lag_m > 0.0
                                     ? s.drone.bfm.perch_lag_m
                                     : s.drone.bfm.lag_dist_m;
        check(s.drone.bfm.perch_lag_m >= 0.0,
              "combat bfm_perch_lag_m >= 0 (0 = fall back to bfm_lag_dist_m)");
        constexpr double kDiveMargin = 0.80;  // of the pursuit dive envelope
        // ★★ GATED ON THE DOCTRINE (rung E9, from the E8 red team's P0-4, and
        // reproduced before it was believed). As shipped, this tripwire
        // REJECTED ITS OWN RUNG'S DOCUMENTED WALK-BACK: `bfm_perch_lag_m = 0`
        // is billed everywhere (the handoff twice, the spec, the dial comment)
        // as the one-line off value, and 0 falls back to `lag_dist_m` = the
        // 220 m TRACKING point -- which is EXACTLY the pre-E8 geometry this
        // check exists to reject. The two are incompatible by construction, so
        // the walk-back took the game and the whole test binary down at config
        // load with a std::runtime_error. Nothing caught it because the only
        // leg that uses the off value sets it PROGRAMMATICALLY, bypassing the
        // loader -- a test that never runs the path that ships.
        // The check keeps all of its force where it can matter: a perch that
        // cannot be dived is only a defect if a slasher will fly it.
        if (s.drone.slash_doctrine && s.drone.bfm.perch_height_m > 0.0 &&
            perch_lag > 0.0) {
            const double need = std::atan2(s.drone.bfm.perch_height_m, perch_lag);
            check(need <= kDiveMargin * s.drone.pursue_max_gamma,
                  "combat bfm_perch_lag_m: the slash dive must fit inside the "
                  "pursuit envelope -- atan(bfm_perch_height_m / perch_lag) "
                  "must be <= 0.8 * pursue_max_gamma_deg, else the slasher "
                  "cannot point at the target it perched over (rung E8.1)");
        }
    }

    // ---- RUNG E8.2 — the engaged fight speed ------------------------------
    check(s.drone.bfm.fight_speed_mps >= 0.0,
          "combat bfm_fight_speed_mps >= 0 (0 = the patrol cruise, off)");
    // A fight speed BELOW the patrol cruise is a no-op by construction
    // (max(dl.speed, this)) but it is always a config mistake -- fail loud
    // rather than silently ship a dial that does nothing.
    check(s.drone.bfm.fight_speed_mps == 0.0 ||
              s.drone.bfm.fight_speed_mps > s.drone.speed,
          "combat bfm_fight_speed_mps > combat speed (a fight speed below the "
          "patrol cruise is a silent no-op)");
    // It is a BASE the align^2-faded bumps sit on top of, so the ceiling is
    // the airframe's, not the dial's. Bounded like intercept_speed_mps.
    check(s.drone.bfm.fight_speed_mps <= 1.25 * ap.v_redline,
          "combat bfm_fight_speed_mps <= 1.25 * v_redline");

    // E7.3 the defensive break.
    check(s.drone.bfm.defensive_range_m >= 0.0,
          "combat bfm_defensive_range_m >= 0 (0 = off)");
    // A defender must not break at a threat further away than the fight it is
    // in — that would turn every distant merge into a guns-cold break.
    check(s.drone.bfm.defensive_range_m <= s.drone.bfm.attack_range_m,
          "combat bfm_defensive_range_m <= bfm_attack_range_m (a break is a "
          "response to a gun threat, not to a distant contact)");
    check(s.drone.bfm.defensive_cone_cos > 0.0 &&
              s.drone.bfm.defensive_cone_cos < 1.0,
          "combat bfm_defensive_cone_deg in (0, 90)");
    check(s.drone.bfm.defensive_release_cos <
              s.drone.bfm.defensive_cone_cos,
          "combat bfm_defensive_release_deg > bfm_defensive_cone_deg (the "
          "break releases at a WIDER angle than it arms — the hysteresis gap)");
    check(s.drone.bfm.defensive_arm_s > 0.0,
          "combat bfm_defensive_arm_s > 0 (a predictor with no debounce is a "
          "coin flip)");
    check(s.drone.bfm.defensive_max_s > 0.0, "combat bfm_defensive_max_s > 0");
    check(s.drone.bfm.defensive_memory_s > 0.0,
          "combat bfm_defensive_memory_s > 0 (how long a hit keeps a pilot "
          "under fire; the break needs a REAL round, not just a nose)");
    check(s.drone.bfm.defensive_cooldown_s > 0.0,
          "combat bfm_defensive_cooldown_s > 0 (the guaranteed guns-live "
          "spell between breaks)");

    // ---- BFM rung R1 sanity (docs/bandit_bfm_r1_spec.md) --------------------
    check(s.drone.bfm.attack_range_m > 0.0, "combat bfm_attack_range_m > 0");
    // attack_release_frac MUST be > 1: a single threshold would chatter every
    // time the range rippled across it (the hysteresis gap).
    check(s.drone.bfm.attack_release_frac > 1.0,
          "combat bfm_attack_release_frac > 1 (hysteresis gap)");
    check(s.drone.bfm.lag_dist_m >= 0.0, "combat bfm_lag_dist_m >= 0");
    check(s.drone.bfm.lag_off_hi > s.drone.bfm.lag_off_lo,
          "combat bfm_lag_off_hi_deg > bfm_lag_off_lo_deg");
    check(s.drone.bfm.lag_off_lo >= 0.0 &&
              s.drone.bfm.lag_off_hi <= 90.0 * kPi / 180.0,
          "combat bfm_lag_off_lo/hi_deg in [0, 90]");
    check(s.drone.bfm.yoyo_closure_mps >= 0.0,
          "combat bfm_yoyo_closure_mps >= 0");
    check(s.drone.bfm.yoyo_angle > 0.0 &&
              s.drone.bfm.yoyo_angle <= 90.0 * kPi / 180.0,
          "combat bfm_yoyo_angle_deg in (0, 90]");
    check(s.drone.bfm.yoyo_arm_s > 0.0, "combat bfm_yoyo_arm_s > 0");
    check(s.drone.bfm.yoyo_gamma > 0.0 &&
              s.drone.bfm.yoyo_gamma <= 80.0 * kPi / 180.0,
          "combat bfm_yoyo_gamma_deg in (0, 80]");
    // FIX-3 (yoyo gamma escapes the envelope): the commanded yoyo climb must
    // not exceed the pursuit gamma ceiling — bfm.h clamps it defensively at
    // the call site too, but a config-time reject is a friendlier failure
    // than a silently-clamped value drifting from what the table claims.
    check(s.drone.bfm.yoyo_gamma <= s.drone.pursue_max_gamma,
          "combat bfm_yoyo_gamma_deg <= pursue_max_gamma_deg");
    check(s.drone.bfm.yoyo_bank_frac > 0.0 && s.drone.bfm.yoyo_bank_frac <= 1.0,
          "combat bfm_yoyo_bank_frac in (0, 1]");
    check(s.drone.bfm.yoyo_time_s > 0.0, "combat bfm_yoyo_time_s > 0");
    check(s.drone.bfm.yoyo_exit_closure_mps >= 0.0,
          "combat bfm_yoyo_exit_closure_mps >= 0");
    // A yo-yo must always be able to end before its own entry predicate can
    // re-arm — else it could exit-then-immediately-re-enter with the closure
    // never having dropped (an extra guard beyond the base spec; the R1
    // reference implementation's own note flags this as a real gap).
    check(s.drone.bfm.yoyo_exit_closure_mps <= s.drone.bfm.yoyo_closure_mps,
          "combat bfm_yoyo_exit_closure_mps <= bfm_yoyo_closure_mps");
    check(s.drone.bfm.extend_energy_m > 0.0, "combat bfm_extend_energy_m > 0");
    check(s.drone.bfm.extend_gamma < 0.0 &&
              s.drone.bfm.extend_gamma >= -45.0 * kPi / 180.0,
          "combat bfm_extend_gamma_deg in [-45, 0)");
    check(s.drone.bfm.extend_min_s > 0.0, "combat bfm_extend_min_s > 0");
    check(s.drone.bfm.extend_max_s > s.drone.bfm.extend_min_s,
          "combat bfm_extend_max_s > bfm_extend_min_s");
    check(s.drone.bfm.reenter_energy_m >= 0.0,
          "combat bfm_reenter_energy_m >= 0");
    // FIX-7 (reenter_energy_m recovery semantics): the residual deficit that
    // counts as "recovered" must be a SHALLOWER deficit than the one that
    // triggered the extend in the first place — else a bandit could recover
    // to the exact deficit that just sent it fleeing and immediately
    // re-engage into the same losing fight.
    check(s.drone.bfm.reenter_energy_m < s.drone.bfm.extend_energy_m,
          "combat bfm_reenter_energy_m < bfm_extend_energy_m");
    check(s.drone.bfm.frustration_s > 0.0, "combat bfm_frustration_s > 0");
    check(s.drone.bfm.intercept_speed_bump >= 0.0,
          "combat bfm_intercept_speed_bump >= 0");
    check(s.drone.bfm.min_dwell_s > 0.0, "combat bfm_min_dwell_s > 0");
    // FIX-10 (config-relative disarms). The arm counter must be able to reach
    // its threshold strictly before yoyo_time_s would otherwise time the
    // machine out on its own were it (wrongly) started counting from mode
    // entry — i.e. the arm window must fit inside a single Offensive dwell in
    // the intended sense: arm_s < yoyo_time_s keeps the two clocks from being
    // configured backwards. attack_range_m must stay inside fire_range_max —
    // otherwise Offensive's merge range never overlaps the range band pursue()
    // requires to fire, and guns never go hot at all.
    check(s.drone.bfm.yoyo_arm_s < s.drone.bfm.yoyo_time_s,
          "combat bfm_yoyo_arm_s < bfm_yoyo_time_s");
    check(s.drone.bfm.attack_range_m > s.drone.fire_range_max,
          "combat bfm_attack_range_m > fire_range_max (else guns never go hot "
          "in range)");
    // FIX-E (loader-check coverage): yoyo_time_s must exceed min_dwell_s —
    // else may_exit (gated on min_dwell_s) never even opens before the
    // yoyo_time_s timeout would have already fired, masking the timeout
    // behind the dwell floor (a mode-length inversion).
    check(s.drone.bfm.yoyo_time_s > s.drone.bfm.min_dwell_s,
          "combat bfm_yoyo_time_s > bfm_min_dwell_s");
    // FIX-D: the cooldown duty-cycle dial must be a real positive spell.
    check(s.drone.bfm.yoyo_cooldown_s > 0.0, "combat bfm_yoyo_cooldown_s > 0");
    // FIX-A / round-4: the abort floor sits strictly INSIDE the ramp — the
    // arm boundary is structural (atm_frac >= avoid_air_frac_full, i.e.
    // climb_t == 1.0 exactly), so abort_t < 1 guarantees a real hysteresis
    // gap and abort_t at/above the ramp top would re-create the measured
    // 1-tick flap loop.
    check(s.drone.bfm.climb_abort_t >= 0.0 &&
              s.drone.bfm.climb_abort_t < 1.0,
          "combat bfm_climb_abort_t in [0, 1)");

    // ---- RUNG E1 BFM sanity -------------------------------------------------
    // intercept_speed_mps is a COMMAND ceiling, not an achievable speed: the
    // airframe's own T=D and the [compression] min_frac deflection floor cap
    // what it can actually fly. It is therefore deliberately allowed ABOVE
    // ap.v_redline — that is Chad's ruled repeal of the pursue_speed_bump
    // "a straight-line run still escapes" property (spec P1-3), not an
    // oversight — but it must stay above stall (a sub-stall chase command is a
    // stall order for the no-G-protection autopilot) and inside a band the
    // compression law still flies. 0 = off (the legacy align-dependent
    // ceiling), which is why this is not a plain range check.
    check(s.drone.bfm.intercept_speed_mps >= 0.0,
          "combat bfm_intercept_speed_mps >= 0 (0 = legacy ceiling)");
    check(s.drone.bfm.intercept_speed_mps == 0.0 ||
              s.drone.bfm.intercept_speed_mps >= 1.5 * v_stall,
          "combat bfm_intercept_speed_mps >= 1.5 * V_stall (above stall for "
          "the no-protection autopilot) or 0 = off");
    check(s.drone.bfm.intercept_speed_mps <= 1.25 * ap.v_redline,
          "combat bfm_intercept_speed_mps <= 1.25 * v_redline (a command the "
          "airframe can still fly inside the compression band)");
    // The chase ceiling must actually EXCEED the in-fight pursuit ceiling, or
    // it is a dead dial that silently reads as shipped (the max() in bfm.h
    // would return the legacy value at every alignment).
    check(s.drone.bfm.intercept_speed_mps == 0.0 ||
              s.drone.bfm.intercept_speed_mps >
                  s.drone.speed + s.drone.pursue_speed_bump,
          "combat bfm_intercept_speed_mps > speed + pursue_speed_bump (else "
          "the raised chase ceiling is dead)");
    check(s.drone.bfm.reenter_closure_mps >= 0.0,
          "combat bfm_reenter_closure_mps >= 0 (0 = off)");
    check(s.drone.bfm.mode_blend_s >= 0.0, "combat bfm_mode_blend_s >= 0");
    // The blend deliberately MAY outlast min_dwell_s (E1.2 cuts that floor to
    // 0.5 s and the blend is what covers the felt side of the faster machine —
    // a re-entered blend simply re-latches from the already-blended command,
    // so chaining stays continuous). It must still be short against the
    // shortest committed maneuver, or a yo-yo would be all transient.
    check(s.drone.bfm.mode_blend_s <= s.drone.bfm.yoyo_time_s,
          "combat bfm_mode_blend_s <= bfm_yoyo_time_s (a blend must be short "
          "against the shortest committed maneuver it can span)");

    // ---- RUNG E2 strike / chamber sanity, cross-checked against the airframe
    // and the maverick envelope (the load_scenario AI-dial precedent above:
    // never a bare number calibrated to today's table). The divert steers the
    // SAME no-protection PD autopilot the bore run does, so its caps must sit
    // inside the angles that autopilot is already RULED to fly.
    check(s.drone.strike_stagger_s >= 0.0, "combat strike_stagger_s >= 0");
    check(s.drone.strike_engage_m > 0.0, "combat strike_engage_m > 0");
    check(s.drone.strike_station_range_m > 0.0,
          "combat strike_station_range_m > 0");
    check(s.drone.strike_engage_m > s.drone.strike_station_range_m,
          "combat strike_engage_m > strike_station_range_m (the divert must "
          "arm outside the on-station envelope it flies to)");
    check(s.drone.strike_station_cos >= -1.0 &&
              s.drone.strike_station_cos <= 1.0,
          "combat strike_station_cos in [-1, 1]");
    check(s.drone.strike_k_az > 0.0 && s.drone.strike_k_el > 0.0,
          "combat strike_k_az / strike_k_el > 0");
    check(s.drone.strike_bail_s > 0.0, "combat strike_bail_s > 0");
    check(s.drone.strike_bank_cap > 0.0 &&
              s.drone.strike_bank_cap <= s.drone.maverick.bank_cap,
          "combat strike_bank_cap_deg in (0, maverick bank_cap] (the divert "
          "banks no harder than the bore run it interrupts)");
    // THE 50-DEG BOUND IS DELIBERATE AND BOUNDED BY CONFIG, not by taste: the
    // required descent from the entry mouth to a deep pump on the FAR side of
    // the arena is ~42 deg, so the cap must exceed the bore run's own ascent
    // cap (run_gamma_cap, the steep-slope precedent) — and it must not exceed
    // the pit plunge (dive_gamma_cap), the steepest angle this autopilot is
    // ruled to fly anywhere. The floor/ceiling safety case at that angle is
    // written in drone/drone.h at the arena-shell wall guard.
    check(s.drone.strike_gamma_cap >= s.drone.maverick.run_gamma_cap,
          "combat strike_gamma_cap_deg >= maverick run_gamma_cap_deg (else "
          "the divert cannot out-descend the bore it flies in)");
    check(s.drone.strike_gamma_cap <= s.drone.maverick.dive_gamma_cap,
          "combat strike_gamma_cap_deg <= maverick dive_gamma_cap_deg (the "
          "steepest angle this no-protection autopilot is ruled to fly)");
    // E2.4 the chamber fight. 0 = the whole block off, so these are
    // conditional, not plain range checks.
    check(s.drone.arena_fight_range_m >= 0.0,
          "combat arena_fight_range_m >= 0 (0 = off)");
    check(s.drone.arena_fight_range_m <= s.drone.disengage_range,
          "combat arena_fight_range_m <= disengage_range (a foe further out "
          "than the assignment band can never be this drone's foe)");
    check(s.drone.arena_fight_s >= 0.0, "combat arena_fight_s >= 0");
    check(s.drone.arena_fight_range_m == 0.0 || s.drone.arena_fight_s > 0.0,
          "combat arena_fight_s > 0 when arena_fight_range_m > 0 (a zero "
          "budget interrupt would flap the run every tick)");
    // The chamber clamp must be TIGHTER than the free-air pursuit envelope or
    // it is a dead dial (the arena's vertical semi-axis is well under its
    // horizontal one — a 30-deg sustained climb runs out of chamber long
    // before it runs out of authority).
    check(s.drone.arena_gamma_cap > 0.0 &&
              s.drone.arena_gamma_cap <= s.drone.pursue_max_gamma,
          "combat arena_gamma_cap_deg in (0, pursue_max_gamma_deg]");
    check(s.drone.arena_guard_margin_m >= 0.0,
          "combat arena_guard_margin_m >= 0 (0 = off)");
    check(s.drone.arena_guard_release_m >= 0.0,
          "combat arena_guard_release_m >= 0");
    check(s.drone.arena_guard_margin_m == 0.0 ||
              s.drone.arena_guard_release_m > s.drone.arena_guard_margin_m,
          "combat arena_guard_release_m > arena_guard_margin_m (the house "
          "anti-chatter hysteresis gap, the bore wall guard's own shape)");
    // THE MARGIN MUST FUND THE PULL-OUT, and the arithmetic is config-relative
    // end to end (never a number calibrated to today's arena table). The guard
    // steers the shell's INWARD NORMAL, so the manoeuvre it has to complete is
    // a pitch pull out of a strike_gamma_cap descent: r*(1 - cos gamma), with
    // r = v^2 / (g*sqrt(n^2 - 1)) at the bank cap the divert flies.
    //
    // v is the airframe REDLINE, not the maverick's commanded bore speed: a
    // 50-deg dive is flown by gravity with the autothrottle already at zero
    // (measured ~180 m/s against a 135 m/s command), so the commanded speed is
    // not an upper bound on anything and v_redline is.
    {
        const double v = ap.v_redline;
        const double n = 1.0 / std::max(0.2, std::cos(s.drone.strike_bank_cap));
        const double a_lat = ap.g * std::sqrt(std::max(0.0, n * n - 1.0));
        const double r_turn = a_lat > 1e-6 ? v * v / a_lat : 0.0;
        const double pullout =
            r_turn * (1.0 - std::cos(s.drone.strike_gamma_cap));
        check(s.drone.arena_guard_margin_m == 0.0 ||
                  s.drone.arena_guard_margin_m >= pullout,
              "combat arena_guard_margin_m >= the pull-out height from a "
              "strike_gamma_cap descent at v_redline (else the guard engages "
              "too late to lift the divert off the shell)");
    }

    // ---- RUNG E3 — THE FELT PASS -------------------------------------------
    // Same loader discipline as the E2 keys: every bound is config-relative to
    // a mechanism that already exists, never a number calibrated to taste.
    check(s.drone.transit_fight_yield_m >= 0.0,
          "combat transit_fight_yield_m >= 0 (0 = off)");
    check(s.drone.transit_fight_yield_m <= s.drone.disengage_range,
          "combat transit_fight_yield_m <= disengage_range (a foe further out "
          "than the assignment band can never be this drone's foe, so a wider "
          "yield radius would be a dead dial)");
    // THE LOOP-CLOSURE INVARIANT, and the reason this bound is a check and not
    // a comment: the yield hands the pilot back through the give-up path with
    // a RELOADED countdown, and what stops that countdown from immediately
    // relaunching the transit into the very same foe is the MERGE HOLD, which
    // is keyed to raid_fight_yield_m. If the yield radius were the wider of
    // the two, a foe in the gap between them would abort every relaunch
    // forever — a mode flap at the boundary, the failure the hysteretic
    // patterns elsewhere in this file exist to forbid. Yield <= merge hold
    // makes "yielding implies held" structurally true.
    check(s.drone.transit_fight_yield_m <= s.drone.raid_fight_yield_m,
          "combat transit_fight_yield_m <= raid_fight_yield_m (a yield must "
          "imply the merge hold that freezes the reloaded countdown, or the "
          "relaunch would abort into the same foe every time)");
    check(s.drone.strike_concurrent_max >= 0,
          "combat strike_concurrent_max >= 0 (0 = uncapped)");
    // S2-TUNNEL — the router's four dials. Every bound is a real failure mode,
    // not decoration: a negative slot count would silently make EVERY raider a
    // deck runner (the router off, with the gate reading ON); a gap tolerance
    // past the world's own circumference would make the direct route
    // unconditionally legal, likewise a silent OFF.
    check(s.drone.raid_deck_run_slots >= 0 &&
              s.drone.raid_deck_run_slots <= 16,
          "combat raid_deck_run_slots in [0, 16] (0 = strictly tunnel; 16 is a "
          "sanity ceiling above any fleet this loader has ever seen -- config/ "
          "does not include combat/, so kNumMavericks is not readable here)");
    check(s.drone.raid_route_gap_max_m >= 0.0 &&
              s.drone.raid_route_gap_max_m <= 50000.0,
          "combat raid_route_gap_max_m in [0, 50000] m");
    check(s.drone.raid_route_stagger_s >= 0.0 &&
              s.drone.raid_route_stagger_s <= 120.0,
          "combat raid_route_stagger_s in [0, 120] s");
    check(s.drone.raid_speed_target >= 0.0 &&
              s.drone.raid_speed_target <= 400.0,
          "combat raid_speed_target in [0, 400] m/s (0 = OFF, inherit the "
          "pursuit bump)");

    // ---- ★★★ D2 — the reposition's relationship checks ---------------------
    // Every one of these is a way the feature silently becomes a no-op or a
    // new crash source, made structurally unrepresentable at LOAD instead of
    // discovered in a tape (the deck-band gate-leg pattern).
    check(s.drone.regroup_frac_arm >= 0.0 && s.drone.regroup_frac_arm <= 2.0,
          "combat regroup_frac_arm in [0, 2] (0 = THE WHOLE-FEATURE "
          "WALK-BACK: an explicit dead switch, not an empty interval)");
    check(s.drone.regroup_frac_arm == 0.0 ||
              s.drone.regroup_frac_lip > s.drone.regroup_frac_arm,
          "combat regroup_frac_lip > regroup_frac_arm (the arm band must be "
          "non-empty)");
    check(s.drone.regroup_frac_arm == 0.0 ||
              (s.drone.regroup_frac_release > 0.0 &&
               s.drone.regroup_frac_release < s.drone.regroup_frac_arm),
          "combat regroup_frac_release in (0, regroup_frac_arm) (the "
          "anti-chatter gap; == arm is a single-threshold trigger)");
    check(s.drone.regroup_frac_arm == 0.0 ||
              (s.drone.regroup_pull_frac > 0.0 &&
               s.drone.regroup_pull_frac < s.drone.regroup_frac_release),
          "combat regroup_pull_frac in (0, regroup_frac_release) -- the aim "
          "point must sit STRICTLY INSIDE the release, or an episode can "
          "never complete and every one of them runs to the timeout");
    check(s.drone.regroup_frac_arm == 0.0 ||
              s.drone.regroup_agl_m > s.drone.avoid_agl_release_m,
          "combat regroup_agl_m > avoid_agl_release_m -- the reposition must "
          "clear the in-dome terrain-avoid band, or the manoeuvre that exists "
          "to reduce crashes flies into the pull-up latch");
    check(s.drone.regroup_max_s > 0.0 && s.drone.regroup_max_s <= 600.0,
          "combat regroup_max_s in (0, 600] s (the episode timeout; 0 would "
          "release every episode on its first tick)");
    check(s.drone.regroup_attack_window_s >= 0.0 &&
              s.drone.regroup_attack_window_s <= 120.0,
          "combat regroup_attack_window_s in [0, 120] s");
    check(s.drone.regroup_frac_arm == 0.0 ||
              !s.drone.regroup_require_pump_attack ||
              s.drone.regroup_attack_window_s > 0.0,
          "combat regroup_attack_window_s > 0 when "
          "regroup_require_pump_attack is on -- a zero window makes CHAD'S "
          "OWN TRIGGER structurally unreachable while the key reads ON");

    // ---- ★★★ RUNG D3 — THE PARABOLIC DIVE ---------------------------------
    // Every bound below is DERIVED from a dial that already ships, never a
    // literal that describes one (the law, paid for ten times). ⚠ The
    // pull-out inequality also needs game.toml's deck_soft_m, and this loader
    // never sees game.toml -- so that half lives in the both-configs GATE leg
    // beside S1-DECK's, exactly where the deck release check lives.
    const double d3 = s.drone.raid_ballistic_climb_agl_m;
    check(d3 >= 0.0, "combat raid_ballistic_climb_agl_m >= 0 (0 = THE "
                     "WHOLE-FEATURE DEAD SWITCH -- it also disarms the "
                     "stage-1 return, so 0 is the pre-D3 machine)");
    check(d3 == 0.0 || d3 > s.drone.avoid_agl_release_m,
          "combat raid_ballistic_climb_agl_m > avoid_agl_release_m -- the "
          "climb must top out ABOVE the in-dome pull-up band it would "
          "otherwise be fighting the whole way (the regroup_agl_m precedent)");
    check(s.drone.raid_ballistic_climb_max_s > 0.0 &&
              s.drone.raid_ballistic_climb_max_s <= 600.0,
          "combat raid_ballistic_climb_max_s in (0, 600] s -- it ADVANCES to "
          "the dive, it never aborts, so 0 would dive on the arming tick");
    check(s.drone.raid_ballistic_dive_gamma > 0.0 &&
              s.drone.raid_ballistic_dive_gamma < 0.5 * kPi,
          "combat raid_ballistic_dive_gamma_deg in (0, 90)");
    check(d3 == 0.0 || s.drone.raid_ballistic_dive_speed > 0.0,
          "combat raid_ballistic_dive_speed > 0 when the ballistic run is "
          "armed (0 would command a stationary sprint)");
    check(s.drone.raid_ballistic_align_min >= -1.0 &&
              s.drone.raid_ballistic_align_min <= 1.0,
          "combat raid_ballistic_align_min in [-1, 1] (it is a cosine)");
    // ★ THE HANDOFF FLOOR, DERIVED. The terrain latch arms on
    // eff_agl = agl - avoid_lookahead_s * sink < deck_avoid_agl_enter_m, and
    // the dive's own sink is dive_speed * sin(dive_gamma). Handing the
    // elevation channel back BELOW that point puts the handoff inside a
    // latched 26 deg panic climb with the guns muted, and the runner
    // porpoises across the deck HIGHER and more muted than it flies today.
    const double d3_sink = s.drone.raid_ballistic_dive_speed *
                           std::sin(s.drone.raid_ballistic_dive_gamma);
    const double d3_latch_agl =
        s.drone.deck_avoid_agl_enter_m + s.drone.avoid_lookahead_s * d3_sink;
    check(d3 == 0.0 || s.drone.deck_avoid_agl_enter_m <= 0.0 ||
              s.drone.raid_ballistic_run_agl_m > d3_latch_agl,
          "combat raid_ballistic_run_agl_m > deck_avoid_agl_enter_m + "
          "avoid_lookahead_s * dive_speed * sin(dive_gamma) -- the DIVE must "
          "hand the elevation channel back to the track law BEFORE the "
          "terrain latch arms, or the arrival is a muted porpoise");
    check(d3 == 0.0 || d3 > s.drone.raid_ballistic_run_agl_m,
          "combat raid_ballistic_climb_agl_m > raid_ballistic_run_agl_m -- "
          "the dive must have somewhere to fall from");
    // ★ AND IT MUST CLEAR THE IN-DOME BAND TOO. The dive STARTS inside the
    // runner's own dome (that is where the climb happens), so it descends
    // through the 250/400 in-bubble pull-up band on the way to the deck.
    // Ending the dive at or below that release hands the elevation channel
    // over mid-latch. Same clearance regroup_agl_m takes, same reason.
    check(d3 == 0.0 ||
              s.drone.raid_ballistic_run_agl_m > s.drone.avoid_agl_release_m,
          "combat raid_ballistic_run_agl_m > avoid_agl_release_m -- the dive "
          "begins INSIDE the runner's own dome and must not end inside the "
          "in-bubble pull-up band it descends through");

    // ---- RUNG E6 — THE RELENTLESS PASS -------------------------------------
    // Same discipline: every bound is config-relative to a mechanism that
    // already exists, never a number calibrated to taste.
    check(s.drone.aggression_range_m >= 0.0,
          "combat aggression_range_m >= 0 (0 = the posture is never armed)");
    // RUNG E17. Both dials are 0-OFF. The reattack turn divides by
    // raid_reattack_m, and its release threshold is half of it, so a positive
    // value must be a real distance; the attack altitude must clear the pump.
    // RUNG E18. The aim point must clear the pump without leaving the arena's
    // vertical clearance, and it must stay well inside the on-station envelope
    // or the striker would be steered out of the window that credits damage.
    check(s.drone.strike_attack_alt_m >= 0.0,
          "combat strike_attack_alt_m >= 0 (0 = aim AT the pump, the pre-E18 "
          "point chase)");
    check(s.drone.strike_attack_alt_m < s.drone.strike_station_range_m,
          "combat strike_attack_alt_m < strike_station_range_m (an aim point "
          "outside the on-station envelope steers the striker out of the "
          "window that credits the damage)");
    check(s.drone.raid_attack_alt_m >= 0.0,
          "combat raid_attack_alt_m >= 0 (0 = aim AT the pump, the pre-E17 "
          "point chase)");
    check(s.drone.raid_reattack_m >= 0.0,
          "combat raid_reattack_m >= 0 (0 = no reattack turn, the pre-E17 "
          "point chase)");
    check(s.drone.raid_reattack_m == 0.0 || s.drone.raid_reattack_m >= 500.0,
          "combat raid_reattack_m == 0 or >= 500 m (the latch releases at HALF "
          "this range -- a smaller value puts arm and release inside one "
          "turn radius and the vertical channel strobes)");
    // The posture is a RAID-PROXIMITY test, so a radius wider than the raid
    // errand's own reach would arm it from anywhere on the map and turn a
    // posture into a permanent retune of the BFM machine.
    check(s.drone.aggression_range_m <= s.drone.engage_range,
          "combat aggression_range_m <= engage_range (a posture armed beyond "
          "the assignment band is a permanent Extend repeal, not a posture)");
    // E6.5's guns-in-the-raid clause is the E1.2 SNAPSHOT discipline. If the
    // snapshot band is not armed it silently degrades to the narrower in-band
    // `fire` gate, which is honest but not what the ruling asks for — say so.
    check(!s.drone.raid_fight_in_place || s.drone.snapshot_range_m > 0.0,
          "combat raid_fight_in_place requires snapshot_range_m > 0 (the "
          "fight-in-place guns ARE the E1.2 snapshot gate)");
    // Fighting in place while the merge-yield radius is zero would be a dial
    // with nothing to override — fight_hot could never be true.
    check(!s.drone.raid_fight_in_place || s.drone.raid_fight_yield_m > 0.0,
          "combat raid_fight_in_place requires raid_fight_yield_m > 0 (it "
          "overrides exactly that yield; with no yield it is a dead dial)");

    // E6.2's flee leash only exists inside the BFM machine's Extend mode.
    check(!s.drone.extend_leash || s.drone.bfm.enabled,
          "combat extend_leash requires bfm_enabled (Extend is a BFM mode)");

    // AI self-preservation ("MAKE THEM GOOD"): the hard-deck hysteresis band
    // (release strictly above enter, the anti-chatter gap) and the flyable-
    // air ceiling band (full-climb frac strictly above the hard-zero frac,
    // both in [0, 1] since atm_frac_at is a fraction).
    check(s.drone.avoid_agl_enter_m > 0.0, "combat avoid_agl_enter_m > 0");
    check(s.drone.avoid_agl_release_m > s.drone.avoid_agl_enter_m,
          "combat avoid_agl_release_m > avoid_agl_enter_m (hysteresis gap)");
    // RUNG E11: 0 = off; armed it must sit ABOVE the pull-up's release, since
    // that is the altitude it fades the air-seek dive out AT.
    check(s.drone.avoid_air_dive_agl_m >= 0.0,
          "combat avoid_air_dive_agl_m >= 0 (0 = off)");
    check(s.drone.avoid_air_dive_agl_m == 0.0 ||
              s.drone.avoid_air_dive_agl_m > s.drone.avoid_agl_release_m,
          "combat avoid_air_dive_agl_m > avoid_agl_release_m when armed "
          "(rung E11: the seek fades to zero AT the release altitude)");
    check(s.drone.avoid_lookahead_s >= 0.0, "combat avoid_lookahead_s >= 0");
    check(s.drone.avoid_gamma > 0.0 &&
              s.drone.avoid_gamma <= 60.0 * kPi / 180.0,
          "combat avoid_gamma_deg in (0, 60]");
    check(s.drone.avoid_bank_cap >= 0.0 &&
              s.drone.avoid_bank_cap <= 65.0 * kPi / 180.0,
          "combat avoid_bank_cap_deg in [0, 65]");
    check(s.drone.avoid_air_frac_hard >= 0.0 &&
              s.drone.avoid_air_frac_hard < 1.0,
          "combat avoid_air_frac_hard in [0, 1)");
    check(s.drone.avoid_air_frac_full > s.drone.avoid_air_frac_hard &&
              s.drone.avoid_air_frac_full <= 1.0,
          "combat avoid_air_frac_full in (avoid_air_frac_hard, 1]");
    // ★★★ RUNG S1-DECK. Every clause is the disjoint-band defect, refused.
    check(s.drone.deck_avoid_agl_enter_m >= 0.0,
          "combat deck_avoid_agl_enter_m >= 0 (0 = off)");
    check(s.drone.deck_avoid_agl_enter_m == 0.0 ||
              s.drone.deck_avoid_agl_release_m >
                  s.drone.deck_avoid_agl_enter_m,
          "combat deck_avoid_agl_release_m > deck_avoid_agl_enter_m when the "
          "deck band is armed (hysteresis gap)");
    // The hold altitude must live INSIDE the band it is held by: below the
    // release (or the track law fights the latch) and above the enter (or it
    // holds an altitude that arms the pull-up every tick). This pair IS the
    // disjointness, in the small.
    check(s.drone.deck_avoid_agl_enter_m == 0.0 ||
              (s.drone.deck_track_agl_m > s.drone.deck_avoid_agl_enter_m &&
               s.drone.deck_track_agl_m <= s.drone.deck_avoid_agl_release_m),
          "combat deck_track_agl_m in (deck_avoid_agl_enter_m, "
          "deck_avoid_agl_release_m]");
    check(s.drone.deck_scope_hyst_frac >= 0.0 &&
              s.drone.deck_scope_hyst_frac < s.drone.avoid_air_frac_full,
          "combat deck_scope_hyst_frac in [0, avoid_air_frac_full)");
    check(s.drone.deck_track_gain >= 0.0, "combat deck_track_gain >= 0");
    check(s.drone.deck_track_dive_cap >= 0.0 &&
              s.drone.deck_track_dive_cap <= 60.0 * kPi / 180.0,
          "combat deck_track_dive_cap_deg in [0, 60]");
    check(s.drone.deck_lookahead_s >= 0.0, "combat deck_lookahead_s >= 0");
    // RUNG DF-1: any negative value is the OFF sentinel; a live cap is a climb
    // angle, so it can never be steeper than the 60 deg this file admits
    // elsewhere for an elevation cap.
    check(s.drone.deck_fight_climb_cap <= 60.0 * kPi / 180.0,
          "combat deck_fight_climb_cap_deg <= 60 (negative = off)");
    check(s.drone.avoid_pull_net_g >= 0.0,
          "combat avoid_pull_net_g >= 0 (0 = the forward arming is off)");

    // ---- [damage] sanity (damage_model_plan.md) ------------------------------
    // The floors and divisors are load-bearing invariants: the transforms in
    // combat/damage.h are constructed so the WORST legal airframe is degraded-
    // but-flyable — never zero-authority, never a divide-by-zero. Guard the two
    // divisors (component_hp, roll_bias_n_ref), the four controllability floors
    // in (0, 1], the drag cap >= 1, the severity losses in [0, 1], and the
    // routing extents so a region can never be starved or unreachable.
    check(s.damage.component_hp > 0.0,
          "damage component_hp > 0 (KE-per-component divisor)");
    check(s.damage.roll_bias_n_ref > 0.0,
          "damage roll_bias_n_ref > 0 (load-factor divisor)");
    check(s.damage.pilot_gain_floor > 0.0 && s.damage.pilot_gain_floor <= 1.0,
          "damage pilot_gain_floor in (0, 1] (keeps the pilot in some control)");
    check(s.damage.cl_floor > 0.0 && s.damage.cl_floor <= 1.0,
          "damage cl_floor in (0, 1]");
    check(s.damage.c_pitchyaw_floor > 0.0 && s.damage.c_pitchyaw_floor <= 1.0,
          "damage c_pitchyaw_floor in (0, 1]");
    check(s.damage.c_roll_floor > 0.0 && s.damage.c_roll_floor <= 1.0,
          "damage c_roll_floor in (0, 1]");
    check(s.damage.cd0_cap_mult >= 1.0,
          "damage cd0_cap_mult >= 1 (drag only ever rises)");
    check(s.damage.wing_cl_loss >= 0.0 && s.damage.wing_cl_loss <= 1.0,
          "damage wing_cl_loss in [0, 1]");
    check(s.damage.wing_roll_loss >= 0.0 && s.damage.wing_roll_loss <= 1.0,
          "damage wing_roll_loss in [0, 1]");
    check(s.damage.struct_auth_loss >= 0.0 && s.damage.struct_auth_loss <= 1.0,
          "damage struct_auth_loss in [0, 1]");
    check(s.damage.wing_drag_gain >= 0.0, "damage wing_drag_gain >= 0");
    check(s.damage.struct_drag_gain >= 0.0, "damage struct_drag_gain >= 0");
    check(s.damage.roll_bias_gain >= 0.0, "damage roll_bias_gain >= 0");
    check(s.damage.route_wing_frac > 0.0 && s.damage.route_wing_frac < 1.0,
          "damage route_wing_frac in (0, 1)");
    check(s.damage.route_nose_frac >= 0.0 && s.damage.route_nose_frac < 1.0,
          "damage route_nose_frac in [0, 1)");
    check(s.damage.route_tail_frac >= 0.0 && s.damage.route_tail_frac < 1.0,
          "damage route_tail_frac in [0, 1)");
    check(s.damage.route_center_pilot >= 0.0 && s.damage.route_center_pilot <= 1.0,
          "damage route_center_pilot in [0, 1]");
    check(s.damage.ground_wing_ref_ms > 0.0,
          "damage ground_wing_ref_ms > 0");
    check(s.damage.ground_wing_rate >= 0.0 && s.damage.ground_wing_rate <= 50.0,
          "damage ground_wing_rate in [0, 50]");

    // ---- [maverick] envelope guards, cross-checked against the airframe ------
    // The maverick brain steers the SAME no-protection PD autopilot (with the
    // soft AoA limiter always on in the bore), so its speeds must sit in the
    // controllable band: above stall (else a hard pull spirals), below redline
    // (else the compression floor + energy make the bore untrackable). Config-
    // relative to aircraft.toml (v_stall above, v_redline) — never bare numbers
    // calibrated to today's table (the AT-15 trap). The trait speed_scale caps
    // at ~1.55 and the daredevil hot-up at +30%, but every in-BORE speed is
    // clamped to tunnel_speed_max, so bounding that + base_speed bounds the set.
    const maverick::MaverickParams& mv = s.drone.maverick;
    // The widest patrol/transit cruise a trait can command (speed_scale <= 1.55).
    constexpr double kMaxSpeedScale = 1.55;
    const double max_cruise =
        mv.base_speed * kMaxSpeedScale * std::max(1.0, mv.transit_speed_scale);
    check(mv.base_speed > 0.0, "maverick base_speed > 0");
    check(mv.base_speed >= 1.3 * v_stall,
          "maverick base_speed >= 1.3 * V_stall (above stall for the "
          "no-protection autopilot)");
    check(max_cruise <= ap.v_redline,
          "maverick base_speed * max trait scale <= v_redline (controllable)");
    check(mv.tunnel_speed_max >= 1.3 * v_stall &&
              mv.tunnel_speed_max <= ap.v_redline,
          "maverick tunnel_speed_max in [1.3*V_stall, v_redline]");
    check(mv.transit_speed_scale > 0.0, "maverick transit_speed_scale > 0");
    check(mv.lookahead_m > 0.0, "maverick lookahead_m > 0");
    check(mv.dive_lookahead_m > 0.0, "maverick dive_lookahead_m > 0");
    check(mv.centerline_gain >= 0.0, "maverick centerline_gain >= 0");
    // Path-slope + throttle feedforward (the curvature-ff rung).
    check(mv.gamma_lookahead_m > 0.0, "maverick gamma_lookahead_m > 0");
    check(mv.slope_ff_gain >= 0.0, "maverick slope_ff_gain >= 0");
    check(mv.track_gain >= 0.0, "maverick track_gain >= 0");
    check(mv.climb_throttle_gain >= 0.0, "maverick climb_throttle_gain >= 0");
    // The climb-speed floor the run bleeds toward must stay in the controllable
    // band (above stall for the no-protection autopilot, below the bore ceiling)
    // — config-relative to the airframe v_stall (never a welded number).
    check(mv.climb_speed_min >= 1.3 * v_stall,
          "maverick climb_speed_min >= 1.3 * V_stall (sustainable climb, above "
          "stall for the no-protection autopilot)");
    check(mv.climb_speed_min <= mv.tunnel_speed_max,
          "maverick climb_speed_min <= tunnel_speed_max");
    check(mv.exit_frac > 0.5 && mv.exit_frac <= 1.0,
          "maverick exit_frac in (0.5, 1.0]");
    // Wall margins: floor > 0, a proper hysteresis gap, and the engage margin
    // below the smaller tube semi-axis (default 90 m) so a centreline safety
    // tube exists — a margin wider than the bore can never be satisfied.
    check(mv.wall_margin_floor_m > 0.0, "maverick wall_margin_floor_m > 0");
    check(mv.wall_margin_m > mv.wall_margin_floor_m,
          "maverick wall_margin_m > wall_margin_floor_m");
    check(mv.wall_margin_release_m > mv.wall_margin_m,
          "maverick wall_margin_release_m > wall_margin_m (hysteresis gap)");
    check(mv.wall_margin_m <= 80.0,
          "maverick wall_margin_m <= 80 m (below the bore semi-axis)");
    // Caps within the autopilot's controllable authority (aoa_protect keeps any
    // of these stall-safe; the ceilings mirror the drone pursuit envelope).
    check(mv.dive_gamma_cap > 0.0 && mv.dive_gamma_cap <= 65.0 * kPi / 180.0,
          "maverick dive_gamma_cap_deg in (0, 65]");
    check(mv.run_gamma_cap > 0.0 && mv.run_gamma_cap <= 45.0 * kPi / 180.0,
          "maverick run_gamma_cap_deg in (0, 45]");
    // RUNG E17. The recovery cap only ever RELAXES bank authority toward the
    // vertical — a "recovery" cap above the normal cap would be a bank REPEAL
    // wearing a safety name, which is the failure this rung exists to fix.
    check(mv.run_recover_alt_m >= 0.0,
          "maverick run_recover_alt_m >= 0 (0 = OFF, the pre-E17 law)");
    check(mv.run_recover_alt_m == 0.0 ||
              mv.run_recover_bank_cap <= mv.bank_cap,
          "maverick run_recover_bank_cap_deg <= bank_cap_deg (the recovery cap "
          "must REDUCE bank so the lift goes to the vertical -- a larger value "
          "is a bank repeal wearing a safety name)");
    check(mv.run_recover_bank_cap > 0.0,
          "maverick run_recover_bank_cap_deg > 0 (a zero cap cannot track the "
          "bore axis at all)");
    check(mv.run_stall_s >= 0.0,
          "maverick run_stall_s >= 0 (0 = OFF, RUN keeps its pre-E17 "
          "no-timeout behavior)");
    check(mv.run_stall_arc_m > 0.0,
          "maverick run_stall_arc_m > 0 (the progress a live run must make)");
    check(mv.transit_gamma_cap > 0.0 &&
              mv.transit_gamma_cap <= 45.0 * kPi / 180.0,
          "maverick transit_gamma_cap_deg in (0, 45]");
    check(mv.bank_cap > 0.0 && mv.bank_cap <= 70.0 * kPi / 180.0,
          "maverick bank_cap_deg in (0, 70]");
    check(mv.bank_p > 0.0, "maverick bank_p > 0");
    check(mv.pitch_p > 0.0, "maverick pitch_p > 0");
    check(mv.k_az > 0.0, "maverick k_az > 0");
    check(mv.k_el > 0.0, "maverick k_el > 0");
    check(mv.period_scale > 0.0, "maverick period_scale > 0");
    check(mv.engage_aggression_min >= 0.0 && mv.engage_aggression_min <= 1.0,
          "maverick engage_aggression_min in [0, 1]");
    // RUNG E2.1 FIX — the final-leg grace, bounded config-RELATIVE (never a
    // RUNG E15: Stage A's per-km allowance. Floor 0 = off (the flat timeout,
    // bit-identical). Ceiling is generous but finite -- the guarantee the
    // timeout exists for is that the budget is BOUNDED, and the distance it
    // scales is bounded by the planet, so any finite rate is safe. 60 s/km is
    // ~6x the measured 11.0 and exists only to catch a fat-fingered dial.
    check(s.drone.maverick.transit_reach_s_per_km >= 0.0 &&
              s.drone.maverick.transit_reach_s_per_km <= 60.0,
          "maverick transit_reach_s_per_km in [0, 60] (0 = the flat "
          "transit_timeout_s budget)");
    // welded second). Floor 0 = off. Ceiling = transit_timeout_s: the grace may
    // at most DOUBLE the transit budget, so "a livelocked pattern is
    // unrepresentable" (the reason the timeout exists) survives verbatim. The
    // grace is granted ONCE per transit, at the FIX capture, so the worst case
    // really is transit_timeout_s + transit_fix_grace_s.
    check(mv.transit_fix_grace_s >= 0.0 &&
              mv.transit_fix_grace_s <= mv.transit_timeout_s,
          "maverick transit_fix_grace_s in [0, transit_timeout_s] (the grace "
          "may at most double the bounded TRANSIT budget)");
    // The grace only pays for itself if the final leg is actually flyable in
    // it: the leg is transit_fix_back_m long, flown at climb_speed_min.
    check(mv.transit_fix_grace_s == 0.0 ||
              mv.transit_fix_grace_s * mv.climb_speed_min >=
                  mv.transit_fix_back_m,
          "maverick transit_fix_grace_s * climb_speed_min >= "
          "transit_fix_back_m (a grace too short to fly the final leg buys "
          "nothing)");

    return s;
}

}  // namespace cfg
