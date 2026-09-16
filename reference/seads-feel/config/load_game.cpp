#include "config/load_game.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <toml++/toml.hpp>

namespace cfg {

namespace {

constexpr double kPi = 3.14159265358979323846;
double rad(double deg) { return deg * kPi / 180.0; }

double require(const toml::table& root, const char* section, const char* key) {
    const toml::node* node =
        root.at_path(std::string(section) + "." + key).node();
    if (node == nullptr || !node->is_number()) {
        throw std::runtime_error(
            std::string("game.toml: missing or non-numeric key [") + section +
            "] " + key);
    }
    return node->value_or(0.0);
}

bool require_bool(const toml::table& root, const char* section,
                  const char* key) {
    const toml::node* node =
        root.at_path(std::string(section) + "." + key).node();
    if (node == nullptr || !node->is_boolean()) {
        throw std::runtime_error(
            std::string("game.toml: missing or non-boolean key [") + section +
            "] " + key);
    }
    return node->value_or(false);
}

std::string require_string(const toml::table& root, const char* section,
                           const char* key) {
    const toml::node* node =
        root.at_path(std::string(section) + "." + key).node();
    if (node == nullptr || !node->is_string()) {
        throw std::runtime_error(
            std::string("game.toml: missing or non-string key [") + section +
            "] " + key);
    }
    return node->value_or(std::string());
}

void check(bool ok, const char* what) {
    if (!ok) {
        throw std::runtime_error(std::string("game.toml: invalid value: ") +
                                 what);
    }
}

}  // namespace

GameParams load_game_toml(const std::string& path,
                          const sim::AircraftParams& ap) {
    toml::table root;
    try {
        root = toml::parse_file(path);
    } catch (const toml::parse_error& e) {
        throw std::runtime_error(std::string("game.toml parse error: ") +
                                 e.what());
    }

    GameParams g;

    g.ground.enabled = require_bool(root, "ground", "enabled");
    g.ground.slope_limit_rad = rad(require(root, "ground", "slope_limit_deg"));
    g.ground.friction = require(root, "ground", "friction");
    g.ground.max_sink_ms = require(root, "ground", "max_sink_ms");
    g.ground.normal_probe_m = require(root, "ground", "normal_probe_m");
    g.ground.contact_height_m = require(root, "ground", "contact_height_m");
    g.ground.brake_friction = require(root, "ground", "brake_friction");
    g.ground.roll_align_rate_rad =
        rad(require(root, "ground", "roll_align_rate_deg_s"));
    g.ground.ground_ang_damp = require(root, "ground", "ground_ang_damp");
    g.ground.wing_halfspan_m = require(root, "ground", "wing_halfspan_m");
    g.ground.ground_loop_lat_g = require(root, "ground", "ground_loop_lat_g");
    g.ground.brake_pitch_rate_rad =
        rad(require(root, "ground", "brake_pitch_rate_deg_s"));
    g.ground.prop_strike_pitch_rad =
        rad(require(root, "ground", "prop_strike_pitch_deg"));
    g.ground.noseover_full_speed_ms =
        require(root, "ground", "noseover_full_speed_ms");
    g.ground.deep_penetration_m = require(root, "ground", "deep_penetration_m");

    g.buildings.collide = require_bool(root, "buildings", "collide");
    g.buildings.inflate_m = require(root, "buildings", "inflate_m");
    g.buildings.base_margin_m = require(root, "buildings", "base_margin_m");

    g.fx.touchdown_ref_speed_ms = require(root, "fx", "touchdown_ref_speed_ms");
    g.fx.touchdown_intensity = require(root, "fx", "touchdown_intensity");
    g.fx.rolling_min_speed_ms = require(root, "fx", "rolling_min_speed_ms");

    g.input.thumb_up_a =
        static_cast<int>(require(root, "input", "thumb_up_button_a"));
    g.input.thumb_up_b =
        static_cast<int>(require(root, "input", "thumb_up_button_b"));
    g.input.thumb_down_a =
        static_cast<int>(require(root, "input", "thumb_down_button_a"));
    g.input.thumb_down_b =
        static_cast<int>(require(root, "input", "thumb_down_button_b"));

    g.atmosphere.enabled = require_bool(root, "atmosphere", "enabled");
    g.atmosphere.deck_agl_m = require(root, "atmosphere", "deck_agl_m");
    g.atmosphere.deck_soft_m = require(root, "atmosphere", "deck_soft_m");
    g.atmosphere.deck_terrain_relative =
        require_bool(root, "atmosphere", "deck_terrain_relative");
    g.atmosphere.bubble_radius_m =
        require(root, "atmosphere", "bubble_radius_m");
    g.atmosphere.bubble_ceiling_m =
        require(root, "atmosphere", "bubble_ceiling_m");
    g.atmosphere.bubble_edge_soft_m =
        require(root, "atmosphere", "bubble_edge_soft_m");
    g.atmosphere.bubble_ceil_soft_m =
        require(root, "atmosphere", "bubble_ceil_soft_m");
    g.atmosphere.bubble_dome_exponent =
        require(root, "atmosphere", "bubble_dome_exponent");
    g.atmosphere.bubble_ceiling_volume_preserve =
        require_bool(root, "atmosphere", "bubble_ceiling_volume_preserve");
    // S-domeround §1.3: derive H = bubble_ceiling_m / I(n) at LOAD time (n
    // fixed per config, never per-frame) via std::lgamma, NOT a hard-coded
    // table -- it must track a retuned bubble_dome_exponent. I(n) =
    // Gamma(1+1/n)*Gamma(1+2/n)/Gamma(1+3/n); volume_preserve=false => H is
    // the literal ceiling (I == 1). Growth later scales this derived H by
    // the SAME ceiling_scale as bubble_ceiling_m (world/faction_bubbles.h),
    // and clamps to the SAME 20000 m bound (mirrored, not shared -- the
    // world/faction_bubbles.h precedent for this exact loader-band mirror).
    {
        double i_n = 1.0;
        if (g.atmosphere.bubble_ceiling_volume_preserve) {
            const double n = g.atmosphere.bubble_dome_exponent;
            i_n = std::exp(std::lgamma(1.0 + 1.0 / n) +
                           std::lgamma(1.0 + 2.0 / n) -
                           std::lgamma(1.0 + 3.0 / n));
        }
        constexpr double kFactionBubbleCeilingMaxM =
            20000.0;  // mirrors
                      // world::kFactionBubbleCeilingMaxM
                      // (world/faction_bubbles.h) -- no live GameParams handle
                      // at this call site, same precedent.
        g.atmosphere.bubble_dome_h_m = std::min(
            g.atmosphere.bubble_ceiling_m / i_n, kFactionBubbleCeilingMaxM);
    }

    g.gravity.enabled = require_bool(root, "gravity", "enabled");
    g.gravity.h_g0_m = require(root, "gravity", "h_g0_m");
    g.gravity.sigma_g_m = require(root, "gravity", "sigma_g_m");

    g.tunnel.enabled = require_bool(root, "tunnel", "enabled");
    g.tunnel.tube_width_m = require(root, "tunnel", "tube_width_m");
    g.tunnel.tube_height_m = require(root, "tunnel", "tube_height_m");
    g.tunnel.depth_m = require(root, "tunnel", "depth_m");
    g.tunnel.soft_m = require(root, "tunnel", "soft_m");
    g.tunnel.ramp_frac = require(root, "tunnel", "ramp_frac");
    g.tunnel.spacing_m = require(root, "tunnel", "spacing_m");
    g.tunnel.floor_height_m = require(root, "tunnel", "floor_height_m");
    g.tunnel.arena_a_m = require(root, "tunnel", "arena_a_m");
    g.tunnel.arena_c_m = require(root, "tunnel", "arena_c_m");
    g.tunnel.arena_depth_m = require(root, "tunnel", "arena_depth_m");
    g.tunnel.cavern_core_m = require(root, "tunnel", "cavern_core_m");
    g.tunnel.breach_margin_m = require(root, "tunnel", "breach_margin_m");
    g.tunnel.chamber_long_m = require(root, "tunnel", "chamber_long_m");
    g.tunnel.chamber_lat_m = require(root, "tunnel", "chamber_lat_m");
    g.tunnel.chamber_vert_m = require(root, "tunnel", "chamber_vert_m");
    g.tunnel.chamber_breach_offset_m =
        require(root, "tunnel", "chamber_breach_offset_m");
    g.tunnel.connector_radius_m = require(root, "tunnel", "connector_radius_m");
    g.tunnel.chambers_on = require_bool(root, "tunnel", "chambers_on");
    g.tunnel.bowl_radius_m = require(root, "tunnel", "bowl_radius_m");
    g.tunnel.bowl_depth_m = require(root, "tunnel", "bowl_depth_m");
    g.tunnel.mouth_sink_m = require(root, "tunnel", "mouth_sink_m");
    g.tunnel.min_cover_m = require(root, "tunnel", "min_cover_m");
    g.tunnel.trench_len_m = require(root, "tunnel", "trench_len_m");
    g.tunnel.trench_rim_m = require(root, "tunnel", "trench_rim_m");
    g.tunnel.headframe_h_m = require(root, "tunnel", "headframe_h_m");
    g.tunnel.headframe_on = require_bool(root, "tunnel", "headframe_on");

    g.conquest.enabled = require_bool(root, "conquest", "enabled");
    g.conquest.growth_radius_frac =
        require(root, "conquest", "growth_radius_frac");
    g.conquest.growth_ceiling_frac =
        require(root, "conquest", "growth_ceiling_frac");
    g.conquest.player_faction =
        require_string(root, "conquest", "player_faction");
    // 2026-07-25 fly-2 rulings C/D: pump destruction shrinks the VICTIM's
    // dome (growth of the destroyer already existed above), and a pump's HP
    // is DERIVED from the live gun DPS * pump_kill_seconds (never welded).
    g.conquest.shrink_radius_frac =
        require(root, "conquest", "shrink_radius_frac");
    g.conquest.shrink_ceiling_frac =
        require(root, "conquest", "shrink_ceiling_frac");
    g.conquest.pump_kill_seconds =
        require(root, "conquest", "pump_kill_seconds");
    g.conquest.raid_dps_frac = require(root, "conquest", "raid_dps_frac");
    g.conquest.all_vs_player = require_bool(root, "conquest", "all_vs_player");
    g.conquest.match_countdown_s =
        require(root, "conquest", "match_countdown_s");
    // FIX-F3 (docs/ai_phase2_fix_spec.md): the raid-pause dial pair, split
    // from the foe-assignment engage/disengage ranges.
    g.conquest.raid_pause_engage_m =
        require(root, "conquest", "raid_pause_engage_m");
    g.conquest.raid_pause_disengage_m =
        require(root, "conquest", "raid_pause_disengage_m");
    // RUNG E5 REINFORCEMENT WAVES (Chad, 2026-08-20).
    g.conquest.reinforce_delay_s =
        require(root, "conquest", "reinforce_delay_s");
    g.conquest.reinforce_restores_roster =
        require_bool(root, "conquest", "reinforce_restores_roster");
    // ---- RUNG E6 — THE RELENTLESS PASS -------------------------------------
    g.conquest.reinforce_pool_n = static_cast<int>(
        std::llround(require(root, "conquest", "reinforce_pool_n")));
    g.conquest.leash_min_radius_scale =
        require(root, "conquest", "leash_min_radius_scale");
    g.conquest.raid_no_pause =
        require_bool(root, "conquest", "raid_no_pause");
    g.conquest.raid_backfill =
        require_bool(root, "conquest", "raid_backfill");

    // ---- L1 THE MILLWRIGHT LOOP -----------------------------------------
    g.interact.sled_reach_m = require(root, "interact", "sled_reach_m");
    g.interact.aircraft_reach_m = require(root, "interact", "aircraft_reach_m");
    g.interact.gun_reach_m = require(root, "interact", "gun_reach_m");
    g.interact.dismount_side_m = require(root, "interact", "dismount_side_m");
    g.interact.dismount_stop_ms = require(root, "interact", "dismount_stop_ms");
    g.repair.reach_m = require(root, "repair", "reach_m");
    g.repair.full_s = require(root, "repair", "full_s");
    g.repair.repair_points =
        static_cast<int>(require(root, "repair", "repair_points"));
    g.repair.engine_reach_m = require(root, "repair", "engine_reach_m");
    g.repair.engine_full_s = require(root, "repair", "engine_full_s");
    g.spawn.sled_dist_m = require(root, "spawn", "sled_dist_m");
    g.spawn.aircraft_beside_m = require(root, "spawn", "aircraft_beside_m");

    // ---- sanity ---------------------------------------------------------
    // slope limit in (0, 90) deg: cos stays in (0, 1) so the acceptance dots
    // are meaningful; 0 or 90 is a degenerate landing rule, not a tune.
    check(g.ground.slope_limit_rad > 0.0 &&
              g.ground.slope_limit_rad < 90.0 * kPi / 180.0,
          "ground slope_limit_deg in (0, 90)");
    check(g.ground.friction >= 0.0 && g.ground.friction <= 2.0,
          "ground friction in [0, 2] (fraction of g)");
    // Cross-airframe guard (the load_scenario docile-envelope pattern, derived
    // from the config the mechanism reads — never a calibrated constant): a
    // full-throttle roll must ACCELERATE or takeoff is unreachable.
    check(g.ground.friction < ap.T_max / (ap.mass * ap.g),
          "ground friction < T_max/(m*g) (a full-throttle takeoff roll must "
          "accelerate)");
    check(g.ground.max_sink_ms >= 0.0, "ground max_sink_ms >= 0");
    check(g.ground.normal_probe_m > 0.0, "ground normal_probe_m > 0");
    // Brake: non-negative + the friction-class ceiling. DELIBERATELY NOT in
    // the accelerate cross-check above — a full brake-hold at full throttle
    // pinning the plane IS the runup (Fable design review (a)).
    check(g.ground.brake_friction >= 0.0 && g.ground.brake_friction <= 2.0,
          "ground brake_friction in [0, 2] (fraction of g)");
    check(g.ground.roll_align_rate_rad >= 0.0,
          "ground roll_align_rate_deg_s >= 0");
    check(g.ground.ground_ang_damp >= 0.0 && g.ground.ground_ang_damp <= 20.0,
          "ground ground_ang_damp in [0, 20] (1/s)");
    check(g.ground.wing_halfspan_m >= 0.0 && g.ground.wing_halfspan_m <= 30.0,
          "ground wing_halfspan_m in [0, 30]");
    check(
        g.ground.ground_loop_lat_g >= 0.0 && g.ground.ground_loop_lat_g <= 5.0,
        "ground ground_loop_lat_g in [0, 5]");
    check(g.ground.brake_pitch_rate_rad >= 0.0,
          "ground brake_pitch_rate_deg_s >= 0");
    check(g.ground.prop_strike_pitch_rad >= 0.0 &&
              g.ground.prop_strike_pitch_rad < 1.55,
          "ground prop_strike_pitch_deg in [0, 89]");
    check(g.ground.noseover_full_speed_ms >= 0.0 &&
              g.ground.noseover_full_speed_ms <= 100.0,
          "ground noseover_full_speed_ms in [0, 100] (0 = unscaled dip; a "
          "huge value would silently delete the nose-over consequence)");
    check(g.buildings.inflate_m >= 0.0 && g.buildings.inflate_m <= 50.0,
          "buildings inflate_m in [0, 50]");
    check(g.buildings.base_margin_m >= 0.0 && g.buildings.base_margin_m <= 50.0,
          "buildings base_margin_m in [0, 50]");
    // Non-negative and airframe-plausible (a 20 m stilt is a typo, not a tune).
    check(g.ground.contact_height_m >= 0.0 && g.ground.contact_height_m <= 10.0,
          "ground contact_height_m in [0, 10]");
    // T3 deep-penetration wall-strike floor. Two-sided, DERIVED from the config
    // the mechanism reads (the AT-15/ground-friction cross-check pattern, never
    // a calibrated constant): the LOWER bound keeps it UNREACHABLE by a
    // legitimate landing — max legit penetration is one tick of sink
    // (max_sink_ms * sim_dt, sub-metre) plus the contact_height offset and the
    // bilinear kink, so > contact_height_m + 5 has clear headroom. The UPPER
    // bound keeps it BELOW the tunnel depth so a wall graze (~depth_m deep,
    // ~2000 m) always exceeds it and fires — a value near/above depth would
    // silently let the deep graze teleport-land again. 500 m is comfortably
    // between (contact_height ~2.45, depth ~2000).
    check(g.ground.deep_penetration_m > g.ground.contact_height_m + 5.0 &&
              g.ground.deep_penetration_m <= 500.0,
          "ground deep_penetration_m in (contact_height_m + 5, 500] (a wall-"
          "strike floor: unreachable by a legit landing, well below tunnel "
          "depth so a wall graze always fires it)");
    check(g.fx.touchdown_ref_speed_ms > 0.0, "fx touchdown_ref_speed_ms > 0");
    check(g.fx.touchdown_intensity >= 0.0 && g.fx.touchdown_intensity <= 2.0,
          "fx touchdown_intensity in [0, 2] (the render caps effect at 2 — "
          "a wider band would be a silent dead zone)");
    check(g.fx.rolling_min_speed_ms >= 0.0 && g.fx.rolling_min_speed_ms <= 50.0,
          "fx rolling_min_speed_ms in [0, 50]");
    const auto mb_ok = [](int c) { return c >= -1 && c <= 7; };
    check(mb_ok(g.input.thumb_up_a) && mb_ok(g.input.thumb_up_b) &&
              mb_ok(g.input.thumb_down_a) && mb_ok(g.input.thumb_down_b),
          "input thumb_*_button_* in [-1, 7] (raylib mouse-button codes; "
          "-1 = unbound)");
    // R6 [atmosphere] bands (Fable-BEFORE §3: enabled must not ship a live
    // EMPTY field — a plane spawns into vacuum). The single test bubble is
    // built unconditionally centered on spawn, so "enabled => >= 1 bubble" is
    // structural; here we just keep every dimension physical.
    check(g.atmosphere.deck_agl_m > 0.0 && g.atmosphere.deck_agl_m <= 2000.0,
          "atmosphere deck_agl_m in (0, 2000] (the go-anywhere breathable "
          "floor above the surface)");
    check(g.atmosphere.deck_soft_m > 0.0 && g.atmosphere.deck_soft_m <= 5000.0,
          "atmosphere deck_soft_m in (0, 5000]");
    // Bubble radius floor keeps a flyable interior; ceiling floor keeps the
    // dome above the deck so there is breathable air to fly in.
    check(g.atmosphere.bubble_radius_m >= 1000.0 &&
              g.atmosphere.bubble_radius_m <= 40000.0,
          "atmosphere bubble_radius_m in [1000, 40000]");
    check(g.atmosphere.bubble_ceiling_m > g.atmosphere.deck_agl_m &&
              g.atmosphere.bubble_ceiling_m <= 20000.0,
          "atmosphere bubble_ceiling_m in (deck_agl_m, 20000]");
    // Edge softness FLOOR (Fable-BEFORE §5): a sub-km edge at cruise is a
    // <1 s density cliff that reads as a SCRIPTED wall — the honest ρ-gradient
    // wall needs km-scale softness. 0 would re-arm exactly that.
    check(g.atmosphere.bubble_edge_soft_m >= 200.0 &&
              g.atmosphere.bubble_edge_soft_m <= 10000.0,
          "atmosphere bubble_edge_soft_m in [200, 10000] (km-scale => a felt, "
          "not scripted, wall)");
    check(g.atmosphere.bubble_ceil_soft_m >= 100.0 &&
              g.atmosphere.bubble_ceil_soft_m <= 10000.0,
          "atmosphere bubble_ceil_soft_m in [100, 10000]");
    // S-domeround: below ~1.5 the superellipse goes diamond/pinched (the
    // corner reappears, inverted); above 32 pow() precision degrades and it
    // reads as a cylinder anyway (spec §3).
    check(g.atmosphere.bubble_dome_exponent >= 1.5 &&
              g.atmosphere.bubble_dome_exponent <= 32.0,
          "atmosphere bubble_dome_exponent in [1.5, 32]");
    // R5 [gravity] bands. The h_g0_m floor keeps FULL g over all terrain:
    // the baked Sudbury DEM tops out ~550 m and the grounded regime
    // (sim/ground.h) reads the constant p.g — at any legal h_g0_m that read
    // is EXACT (g_at == p.g below the taper), never an approximation. A
    // taper reaching into the terrain would fork ground vs flight gravity.
    check(g.gravity.h_g0_m >= 1000.0 && g.gravity.h_g0_m <= 20000.0,
          "gravity h_g0_m in [1000, 20000] (floor > max terrain ~550 m: the "
          "grounded regime's constant-g reads must stay exact)");
    // Sigma floor: a redline plane covers ~2 m per tick — sigma below ~50 m
    // would let it jump the whole taper band in a tick or two, degrading the
    // escape threshold the climb AT pins (numerics stay stable even at 1,
    // but the DESIGN band must be resolved by >= ~25 ticks).
    check(g.gravity.sigma_g_m >= 50.0 && g.gravity.sigma_g_m <= 10000.0,
          "gravity sigma_g_m in [50, 10000] (band must span >= ~25 ticks at "
          "redline, not be jumpable in one)");
    // T1 [tunnel] bands. All lengths strictly positive (a zero radius/semi-axis
    // is a degenerate, un-flyable volume, not a tune). ramp_frac in (0, 0.5]
    // (the two end ramps must not overlap into a mouth with residual depth).
    // spacing in [10, 1000] (below 10 m the spine explodes; above 1 km the
    // tube's capsule chords cut corners). soft_m < tube_radius_m (the atm blend
    // band must fit inside the tube — a wider band would blend air outside the
    // solid wall). The egg long-axis total (fat+thin) canon range [600, 800] is
    // a FEEL dial, deliberately NOT enforced (Chad may push it either way).
    check(g.tunnel.tube_width_m > 0.0, "tunnel tube_width_m > 0");
    check(g.tunnel.tube_height_m > 0.0, "tunnel tube_height_m > 0");
    check(g.tunnel.depth_m > 0.0, "tunnel depth_m > 0");
    check(g.tunnel.soft_m > 0.0, "tunnel soft_m > 0");
    check(
        g.tunnel.soft_m <
            std::min(g.tunnel.tube_width_m, g.tunnel.tube_height_m),
        "tunnel soft_m < min(tube_width_m, tube_height_m) (the atm blend band "
        "fits inside the bore)");
    check(g.tunnel.ramp_frac > 0.0 && g.tunnel.ramp_frac <= 0.5,
          "tunnel ramp_frac in (0, 0.5]");
    check(g.tunnel.spacing_m >= 10.0 && g.tunnel.spacing_m <= 1000.0,
          "tunnel spacing_m in [10, 1000]");
    // T5a floor: >= 0 (0 = OFF) and strictly below the bore CENTER (one
    // tube_height above the bore's radial bottom) so the flat floor keeps the
    // crown clearance and never rises past the bore center — a floor at/above
    // tube_height would leave under half the bore open. Canon 20 m << 90.
    check(g.tunnel.floor_height_m >= 0.0 &&
              g.tunnel.floor_height_m < g.tunnel.tube_height_m,
          "tunnel floor_height_m in [0, tube_height_m) (0 = off; the floor "
          "stays below the bore center, keeping crown clearance)");
    check(g.tunnel.chamber_long_m > 0.0, "tunnel chamber_long_m > 0");
    check(g.tunnel.chamber_lat_m > 0.0, "tunnel chamber_lat_m > 0");
    check(g.tunnel.chamber_vert_m > 0.0, "tunnel chamber_vert_m > 0");
    check(g.tunnel.chamber_breach_offset_m > 0.0,
          "tunnel chamber_breach_offset_m > 0");
    check(g.tunnel.connector_radius_m > 0.0, "tunnel connector_radius_m > 0");
    // T11/T12 — THE SEALED-CORE ARENA. The arena ellipsoid must be physical:
    //  - the arena oblate (arena_a >= arena_c) — the escape ruling (no more
    //    "dive a little, climb ages");
    //  - a real vertical extent (arena_c >= 1000);
    //  - a buried inner-core safety floor big enough to floor |position| clear
    //    of the r->0 singularity (local_up / the S-ffrad curvature ff both
    //    divide by |p|), with real rock between the arena floor and the core
    //    (T12 sealed the floor: this is now the SOLID cover Chad asked for, not
    //    air for a shaft);
    //  - the bore breach plateau (arena apex - breach_margin) at least
    //    tube_height + 100 below the apex so the bore breaks fully through.
    // The bare-R arena CENTER radius (terrain >= R, so this is conservative):
    // r_c = R - arena_depth - arena_c.
    const double arena_rc_bare =
        ap.R - g.tunnel.arena_depth_m - g.tunnel.arena_c_m;
    check(g.tunnel.arena_a_m >= g.tunnel.arena_c_m,
          "tunnel arena_a_m >= arena_c_m (the arena is oblate — the escape "
          "ruling)");
    check(g.tunnel.arena_c_m >= 1000.0,
          "tunnel arena_c_m >= 1000 (a real vertical dogfight extent)");
    check(g.tunnel.cavern_core_m >= 1500.0,
          "tunnel cavern_core_m >= 1500 (a solid core big enough to floor "
          "|position| clear of the r->0 singularity)");
    check(arena_rc_bare - g.tunnel.arena_c_m >= g.tunnel.cavern_core_m + 1500.0,
          "tunnel (arena floor) - cavern_core_m >= 1500 (real ROCK between the "
          "sealed arena floor and the buried core — T12 covers the core)");
    check(
        g.tunnel.breach_margin_m >= g.tunnel.tube_height_m + 100.0,
        "tunnel breach_margin_m >= tube_height_m + 100 (the bore breaks fully "
        "through the arena apex into the arena)");
    // THE SHOULDER TRIPWIRE (P1-3): the SHALLOWEST point of the oblate arena is
    // the SHOULDER, not the apex. Closed form: max over theta of rho(theta)^2 =
    // (r_c + c*cos)^2 + a^2*sin^2, with the interior max at
    // cos* = c*r_c/(a^2 - c^2) (when in [-1,1]; else the endpoint theta=pi/2 or
    // 0 wins). Require rho_max <= R - min_cover - 100 (bare-R conservative
    // since terrain >= R): the arena shoulder keeps min_cover of rock below
    // terrain.
    {
        const double c = g.tunnel.arena_c_m, a = g.tunnel.arena_a_m;
        const double rc = arena_rc_bare;
        double rho2;
        const double denom = a * a - c * c;
        double cs = denom != 0.0 ? c * rc / denom : 2.0;
        if (cs >= -1.0 && cs <= 1.0) {
            rho2 = (rc + c * cs) * (rc + c * cs) + a * a * (1.0 - cs * cs);
        } else {
            // Endpoints: theta=0 -> (rc+c)^2 ; theta=pi/2 -> rc^2 + a^2.
            const double e0 = (rc + c) * (rc + c);
            const double e1 = rc * rc + a * a;
            rho2 = std::max(e0, e1);
        }
        const double rho_max = std::sqrt(rho2);
        check(
            rho_max <= ap.R - g.tunnel.min_cover_m - 100.0,
            "tunnel arena shoulder rho_max <= R - min_cover - 100 (the oblate "
            "arena's shallowest point keeps min_cover of rock below terrain — "
            "the SHOULDER, not the apex, is the constraint)");
    }
    // The Murray bowl: an open pit wider than the tube, shallower than the
    // deepest depth, both positive.
    check(g.tunnel.bowl_radius_m > 0.0, "tunnel bowl_radius_m > 0");
    check(g.tunnel.bowl_depth_m > 0.0, "tunnel bowl_depth_m > 0");
    check(g.tunnel.bowl_radius_m > g.tunnel.tube_width_m,
          "tunnel bowl_radius_m > tube_width_m (the pit is wider than the "
          "bore)");
    check(g.tunnel.bowl_depth_m < g.tunnel.depth_m,
          "tunnel bowl_depth_m < depth_m (the pit floor is above the deep)");
    // T6c/T6d: the Errington pit sink + the monotone-descent cover. mouth_sink
    // in (0, depth_m) (a recess, above the deep); min_cover in (0, depth_m)
    // (rock above the crown, less than the deepest dip).
    check(
        g.tunnel.mouth_sink_m > 0.0 && g.tunnel.mouth_sink_m < g.tunnel.depth_m,
        "tunnel mouth_sink_m in (0, depth_m) (the home pit is a recess above "
        "the deep)");
    check(g.tunnel.min_cover_m > 0.0 && g.tunnel.min_cover_m < g.tunnel.depth_m,
          "tunnel min_cover_m in (0, depth_m) (rock above the bore crown, less "
          "than the deepest dip)");
    // T13 entry trench: non-negative reach (0 == OFF); a live trench needs a
    // real rim wider than the bore so the approach cut is a genuine open
    // crater.
    check(g.tunnel.trench_len_m >= 0.0, "tunnel trench_len_m >= 0 (0 = OFF)");
    check(g.tunnel.trench_len_m == 0.0 ||
              g.tunnel.trench_rim_m > g.tunnel.tube_width_m,
          "tunnel trench_rim_m > tube_width_m when the trench is on (the "
          "approach cut is wider than the bore)");
    // T13 (B1): the headframe head-house height must be positive when on (a
    // 0-height headframe is degenerate); off is fine at any value.
    check(!g.tunnel.headframe_on || g.tunnel.headframe_h_m > 0.0,
          "tunnel headframe_h_m > 0 when headframe_on (a 0-height pit-head is "
          "degenerate)");
    // [conquest] — the faction-bubble growth rung (world/faction_bubbles.h).
    // Fractions in [0, 1] (a growth > 100% per single pump kill is a typo, not
    // a tune — the clamp in build_faction_bubbles is the LAST-resort backstop,
    // not the intended dial range). player_faction is exactly one of the two
    // named factions — never a silent third state.
    check(g.conquest.growth_radius_frac >= 0.0 &&
              g.conquest.growth_radius_frac <= 1.0,
          "conquest growth_radius_frac in [0, 1]");
    check(g.conquest.growth_ceiling_frac >= 0.0 &&
              g.conquest.growth_ceiling_frac <= 1.0,
          "conquest growth_ceiling_frac in [0, 1]");
    check(g.conquest.player_faction == "valley" ||
              g.conquest.player_faction == "sudbury",
          "conquest player_faction is \"valley\" or \"sudbury\"");
    // SUDDEN-DEATH CLOCK: a positive, finite match length. 0 or negative would
    // arm a clock that expires on its own arming tick (an instant loss the
    // moment a bubble pops); an absurd value silently disables the mechanism.
    check(g.conquest.match_countdown_s > 0.0 &&
              g.conquest.match_countdown_s <= 7200.0,
          "conquest match_countdown_s in (0, 7200] seconds");
    check(g.conquest.shrink_radius_frac >= 0.0 &&
              g.conquest.shrink_radius_frac <= 1.0,
          "conquest shrink_radius_frac in [0, 1]");
    check(g.conquest.shrink_ceiling_frac >= 0.0 &&
              g.conquest.shrink_ceiling_frac <= 1.0,
          "conquest shrink_ceiling_frac in [0, 1]");
    check(g.conquest.pump_kill_seconds > 0.0 &&
              g.conquest.pump_kill_seconds <= 120.0,
          "conquest pump_kill_seconds in (0, 120]");
    check(g.conquest.raid_dps_frac >= 0.0 && g.conquest.raid_dps_frac <= 1.0,
          "conquest raid_dps_frac in [0, 1]");
    // FIX-F3: the house hysteresis rule (load_scenario.cpp's engage/disengage
    // pattern) -- disengage strictly beyond engage, both positive, so the
    // raid-pause latch has a real gap and can never arm at/below zero range.
    check(g.conquest.raid_pause_engage_m > 0.0,
          "conquest raid_pause_engage_m > 0");
    check(g.conquest.raid_pause_disengage_m > g.conquest.raid_pause_engage_m,
          "conquest raid_pause_disengage_m > raid_pause_engage_m (hysteresis "
          "gap)");
    // RUNG E5: the wave delay is EARNED breathing room. Negative is a config
    // bug, not an off-value (0 is the off-value); above the match clock a wave
    // could never arrive inside a match, which is a dead dial dressed as a
    // live one.
    // ---- RUNG E6 — THE RELENTLESS PASS -------------------------------------
    // The pool is a COUNT of revives per faction; negative is the structural
    // "infinite" off-value (rung E5), so the only illegal shape is a value
    // below -1 pretending to be a different kind of infinity.
    check(g.conquest.reinforce_pool_n >= -1,
          "conquest reinforce_pool_n >= -1 (-1 = infinite waves, 0 = none)");
    // The leash floor is a radius_scale fraction. Above 1.0 it would treat a
    // FULL-STRENGTH dome as crushed and leash the whole map to one faction.
    check(g.conquest.leash_min_radius_scale >= 0.0 &&
              g.conquest.leash_min_radius_scale <= 1.0,
          "conquest leash_min_radius_scale in [0, 1] (0 = off; a value > 1 "
          "would call a full-strength dome crushed)");
    check(g.conquest.reinforce_delay_s >= 0.0,
          "conquest reinforce_delay_s >= 0 (0 = off, permanent death)");
    check(g.conquest.reinforce_delay_s <= g.conquest.match_countdown_s,
          "conquest reinforce_delay_s <= match_countdown_s (a wave must be "
          "able to arrive inside a match)");
    // ---- L1 THE MILLWRIGHT LOOP -----------------------------------------
    // A reach is a real arm's length at a real machine. Zero would make a
    // diegetic key unpressable (you can never stand at EXACTLY the CG), and a
    // reach wider than the 5 m gap the mount seed leaves between the machine
    // and the aeroplane would let one key mean two things at one spot -- which
    // is the ambiguity the position gate exists to remove.
    check(g.interact.sled_reach_m > 0.0 && g.interact.sled_reach_m <= 5.0,
          "interact sled_reach_m in (0, 5] m");
    check(g.interact.aircraft_reach_m > 0.0 &&
              g.interact.aircraft_reach_m <= 5.0,
          "interact aircraft_reach_m in (0, 5] m");
    check(g.interact.gun_reach_m > 0.0 && g.interact.gun_reach_m <= 30.0,
          "interact gun_reach_m in (0, 30] m");
    // He steps off BESIDE the machine: inside its own half-width he would be
    // standing in it, and past a stride he would teleport rather than step.
    check(g.interact.dismount_side_m >= 0.4 &&
              g.interact.dismount_side_m <= 3.0,
          "interact dismount_side_m in [0.4, 3] m (inside the machine, or a "
          "teleport)");
    // The stop gate is a real speed. 0 would make dismount impossible (a
    // parked machine still creeps at 1e-9 m/s on a sphere); above a walking
    // pace it stops being "stopped".
    check(g.interact.dismount_stop_ms > 0.0 &&
              g.interact.dismount_stop_ms <= 3.0,
          "interact dismount_stop_ms in (0, 3] m/s");
    // The pump reach must clear the pump's own body (radius_m 25 is the KILL
    // radius, not the model) and must not span from one pump to another.
    check(g.repair.reach_m > 0.0 && g.repair.reach_m <= 50.0,
          "repair reach_m in (0, 50] m");
    // A non-positive full_s is a divide-by-zero dressed as "instant repair";
    // above ten minutes the fix outlasts the whole sudden-death clock it is
    // supposed to hold, which makes the mechanic pointless rather than hard.
    check(g.repair.full_s > 0.0 && g.repair.full_s <= 600.0,
          "repair full_s in (0, 600] s");
    // Points are a reward, never a tax.
    check(g.repair.repair_points >= 0 && g.repair.repair_points <= 10000,
          "repair repair_points in [0, 10000]");
    // ★ L10 -- THE ENGINE REACH MUST CLEAR THE AEROPLANE AND MUST NOT SWALLOW
    // A PUMP. Below the boarding reach the U prompt would light in places the
    // J prompt does not, which reads as the aeroplane having two different
    // sizes; a reach wider than the pump's would make "walk to the plane" and
    // "walk to the pump" the same walk whenever he lands beside one.
    check(g.repair.engine_reach_m >= g.interact.aircraft_reach_m &&
              g.repair.engine_reach_m <= g.repair.reach_m,
          "repair engine_reach_m in [interact aircraft_reach_m, repair reach_m] m");
    // Same divide-by-zero and same ceiling argument as full_s above: an
    // instant engine fix is not a repair loop, and one that outlasts a match
    // is not a mechanic.
    check(g.repair.engine_full_s > 0.0 && g.repair.engine_full_s <= 600.0,
          "repair engine_full_s in (0, 600] s");
    // ★ L3 -- THE SPAWN DISTANCE IS A RIDE, NOT A WALK AND NOT A FLIGHT. Below
    // the repair reach the machine would be spawned ON the pump (and the whole
    // "drive up, get off, fix it" beat vanishes); past a few km he is outside
    // his own dome and the spawn has dropped him in the vacuum gap.
    check(g.spawn.sled_dist_m > g.repair.reach_m &&
              g.spawn.sled_dist_m <= 5000.0,
          "spawn sled_dist_m in (repair reach_m, 5000] m");
    // The aeroplane parks BESIDE the machine: inside the machine's own reach it
    // would be seeded through it, and past the sled reach the two J-key sites
    // would still be unambiguous but he would have to hunt for his aircraft.
    check(g.spawn.aircraft_beside_m > g.interact.sled_reach_m &&
              g.spawn.aircraft_beside_m <= 50.0,
          "spawn aircraft_beside_m in (interact sled_reach_m, 50] m");
    return g;
}

}  // namespace cfg
