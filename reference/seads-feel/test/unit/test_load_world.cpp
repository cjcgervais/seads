// load_world (docs/world_build_plan.md §1/§5): the WORLD/ART config table loads
// strictly like the other config, degrees convert to radians at the boundary,
// and out-of-range values are rejected. Mutations are applied to the REAL
// committed table so the test can't drift from the schema (the load_scenario
// discipline).
//
// This is the config spine for the whole map build — the first scaffolding on
// sandbox/world-sudbury. It touches no sim/control state and moves no flight
// golden; it only adds a data source and its strict loader.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <glm/glm.hpp>
#include <sstream>
#include <string>

#include "config/load_world.h"
#include "render/celestial.h"  // the frozen-epoch day-baseline pin (Stage 2c)
#include "render/season.h"  // the [seasons] name<->index shared contract (W1)
#include "render/team_color.h"  // S-mapteam: the complement predicate
#include "render/weather.h"     // the [weather] header<->toml lock (Stage 3)

#ifdef NDEBUG
#error "SEADS gate requires an assert-live build (SPEC 6.1)"
#endif

namespace {

constexpr double kPi = 3.14159265358979323846;

std::string slurp(const std::string& path) {
    std::ifstream f(path);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::string write_temp(const std::string& text, const char* tag) {
    const std::filesystem::path p =
        std::filesystem::temp_directory_path() /
        (std::string("seads_world_") + tag + ".toml");
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

}  // namespace

TEST_CASE("load_world: the committed table loads and converts degrees") {
    const cfg::WorldParams w =
        cfg::load_world_toml(SEADS_CONFIG_DIR "/world.toml");
    // margin_deg = 3 -> radians at the boundary.
    CHECK(w.horizon_cull.margin_rad ==
          Catch::Approx(3.0 * kPi / 180.0).margin(1e-9));
    // Chad's rulings live in the data, not code.
    CHECK(w.scatter.furniture_scale == Catch::Approx(5.0));  // ~5x furniture
    CHECK(w.ground.procedural == true);  // procedural Sudbury
    CHECK(w.atmosphere.sky_day > 0.0);   // day-sky luminance
    CHECK(w.tone.contrast > 1.0);        // high contrast
    CHECK(w.perf.max_draw_calls >= 1);
    CHECK(w.scatter.seed >= 0);  // never a wall clock
    // The mesh knobs that moved out of render/draw.cpp constexprs into config.
    CHECK(w.planet.subdiv == 200);
    // R4d face tiling: present + in the loader's [1,4] range (the VALUE is a
    // dial — config-relative, not pinned, the AT-15 lesson).
    CHECK(w.planet.tiles >= 1);
    CHECK(w.planet.tiles <= 4);
    CHECK(w.planet.u_offset == Catch::Approx(0.806));
    CHECK(w.planet.dem_blur_radius == 3);
    CHECK(w.planet.cubemap_size ==
          4096);  // albedo cubemap face edge (texels); V2 hi-res
}

TEST_CASE("load_world: the celestial block loads (scalars + vectors)") {
    const cfg::WorldParams w =
        cfg::load_world_toml(SEADS_CONFIG_DIR "/world.toml");
    CHECK(w.celestial.tilt_deg == Catch::Approx(23.5));
    CHECK(w.celestial.day_period_s ==
          Catch::Approx(300.0));  // SOLAR day (5 min)
    CHECK(w.celestial.year_period_s ==
          Catch::Approx(3600.0));  // 12 days (1 h year)
    CHECK(w.celestial.orbit_normal.y == Catch::Approx(1.0));  // vec3 parsed
    CHECK(w.celestial.tilt_lean.x == Catch::Approx(1.0));
    CHECK(w.celestial.moon_period_s == Catch::Approx(5400.0));
    CHECK(w.celestial.moon_inclination_deg == Catch::Approx(8.0));
    CHECK(w.celestial.sun_distance_m == Catch::Approx(450000.0));
}

TEST_CASE("load_world: an out-of-range axial tilt is rejected") {
    const std::string base = slurp(SEADS_CONFIG_DIR "/world.toml");
    REQUIRE(base.find("tilt_deg                = 23.5") != std::string::npos);
    // A 95-degree tilt is not a planet — the loader caps it in [0,90).
    const std::string bad = replace_all(base, "tilt_deg                = 23.5",
                                        "tilt_deg                = 95.0");
    CHECK_THROWS(cfg::load_world_toml(write_temp(bad, "tilt_over_90")));
}

TEST_CASE("load_world: a malformed celestial vector is rejected") {
    const std::string base = slurp(SEADS_CONFIG_DIR "/world.toml");
    REQUIRE(base.find("orbit_normal            = [0.0, 1.0, 0.0]") !=
            std::string::npos);
    // A 2-element vector is not a direction — require_vec3 rejects it.
    const std::string bad =
        replace_all(base, "orbit_normal            = [0.0, 1.0, 0.0]",
                    "orbit_normal            = [0.0, 1.0]");
    CHECK_THROWS(cfg::load_world_toml(write_temp(bad, "bad_vec3")));
}

TEST_CASE("load_world: an oversized cubemap face edge is rejected") {
    const std::string base = slurp(SEADS_CONFIG_DIR "/world.toml");
    REQUIRE(base.find("cubemap_size        = 4096") != std::string::npos);
    // 8192/face would try ~1.6 GB of bake buffer — the loader caps it at 4096.
    const std::string bad = replace_all(base, "cubemap_size        = 4096",
                                        "cubemap_size        = 8192");
    CHECK_THROWS(cfg::load_world_toml(write_temp(bad, "cubemap_over_cap")));
}

TEST_CASE("load_world: a subdiv past the ushort index cap is rejected") {
    const std::string base = slurp(SEADS_CONFIG_DIR "/world.toml");
    REQUIRE(base.find("subdiv              = 200") != std::string::npos);
    // 512*512 > 65536 overflows the raylib ushort mesh index.
    const std::string bad = replace_all(base, "subdiv              = 200",
                                        "subdiv              = 512");
    CHECK_THROWS(cfg::load_world_toml(write_temp(bad, "subdiv_over_cap")));
}

TEST_CASE("load_world: a missing key throws (strict schema)") {
    const std::string base = slurp(SEADS_CONFIG_DIR "/world.toml");
    REQUIRE(base.find("furniture_scale     = 5.0") != std::string::npos);
    const std::string bad = replace_all(base, "furniture_scale     = 5.0", "");
    CHECK_THROWS(cfg::load_world_toml(write_temp(bad, "missing_key")));
}

TEST_CASE("load_world: the atmosphere block loads (Stage-2 migration)") {
    const cfg::WorldParams w =
        cfg::load_world_toml(SEADS_CONFIG_DIR "/world.toml");
    // Space-first (Chad 2026-07-08): near-black zenith + a THIN horizon band.
    CHECK(w.atmosphere.sky_space == Catch::Approx(0.02));
    CHECK(w.atmosphere.sky_band_top_rad == Catch::Approx(3.0 * kPi / 180.0));
    CHECK(w.atmosphere.sky_day == Catch::Approx(0.60));
    CHECK(w.atmosphere.sky_dusk == Catch::Approx(0.30));
    CHECK(w.atmosphere.sky_night == Catch::Approx(0.05));
    // dusk band degrees -> radians at the boundary.
    CHECK(w.atmosphere.dusk_lo_rad == Catch::Approx(-8.0 * kPi / 180.0));
    CHECK(w.atmosphere.dusk_hi_rad == Catch::Approx(6.0 * kPi / 180.0));
    CHECK(w.atmosphere.night_fill_min == Catch::Approx(0.05));
    CHECK(w.atmosphere.ground_day_gain == Catch::Approx(1.60));  // Stage 3b
    CHECK(w.atmosphere.haze_scale_m == Catch::Approx(2300.0));
    // Stage 3: the always-on haze_density_light whisper was RETIRED for the
    // weather gate — the overcast density is what the amount now scales.
    CHECK(w.atmosphere.haze_overcast_density == Catch::Approx(0.90));
    // Stage 3: atmospheric scatter (the only sky color, haze-gated).
    CHECK(w.atmosphere.scatter_strength == Catch::Approx(1.0));
    CHECK(w.atmosphere.mie_g ==
          Catch::Approx(0.66));  // Chad fly-dial: glare down
    CHECK(w.atmosphere.rayleigh_tint.x == Catch::Approx(0.45));  // blue up ~25%
    CHECK(w.atmosphere.rayleigh_tint.y == Catch::Approx(0.66));
    CHECK(w.atmosphere.rayleigh_tint.z == Catch::Approx(1.25));
    CHECK(w.atmosphere.mie_tint.x == Catch::Approx(0.80));
    CHECK(w.atmosphere.mie_tint.y == Catch::Approx(0.36));
    CHECK(w.atmosphere.mie_tint.z == Catch::Approx(0.16));
    // S-sunglare: the Mie halo coefficients (were baked GLSL constants).
    CHECK(w.atmosphere.mie_halo_gain == Catch::Approx(0.30));
    CHECK(w.atmosphere.mie_dusk_boost == Catch::Approx(1.0));
    CHECK(w.atmosphere.mie_rim_lift == Catch::Approx(0.18));
    // S-starnight: clear dome air must NOT veil the starfield (Chad's ruling).
    CHECK(w.stars.air_extinction == Catch::Approx(0.0));
}

// --- [map] + [teams]: the M-KEY tactical chart (S-mapread / S-mapteam) -----
TEST_CASE("load_world: [map] ships Chad's colour scheme and declutter ruling") {
    const cfg::WorldParams w =
        cfg::load_world_toml(SEADS_CONFIG_DIR "/world.toml");
    // S-mapteam (Chad 2026-08-09): allies BLUE, enemies SLAG ORANGE — the blue
    // channel dominates the ally, the red channel the enemy, and the enemy is
    // the slag `hot` stop itself.
    CHECK(w.teams.ally.z > w.teams.ally.x);
    CHECK(w.teams.ally.z > w.teams.ally.y);
    CHECK(w.teams.enemy.x > w.teams.enemy.y);
    CHECK(w.teams.enemy.x > w.teams.enemy.z);
    CHECK(w.teams.enemy.x == Catch::Approx(render::kSlagOrange.x));
    CHECK(w.teams.enemy.y == Catch::Approx(render::kSlagOrange.y));
    CHECK(w.teams.enemy.z == Catch::Approx(render::kSlagOrange.z));
    // "with precision the opposite ... on the color wheel" — the shipped table
    // really is the exact complement, checked here against the live config and
    // not only against the code constants (test_team_color.cpp owns those).
    CHECK(render::is_exact_complement(w.teams.enemy, w.teams.ally, 1e-9));
    // Objectives are the medical-oxygen GREEN: green channel dominant, and
    // DARKER than both team hues so it separates in lightness as well as hue.
    CHECK(w.teams.objective.y > w.teams.objective.x);
    CHECK(w.teams.objective.y > w.teams.objective.z);
    const auto luma = [](const glm::dvec3& c) {
        return 0.2126 * c.x + 0.7152 * c.y + 0.0722 * c.z;
    };
    CHECK(luma(w.teams.objective) < luma(w.teams.ally));
    CHECK(luma(w.teams.ally) < luma(w.teams.enemy));
    // The player must be the BIGGEST marker on the map (findable at a glance
    // among the allied blues, on SHAPE and SIZE rather than a private hue).
    CHECK(w.map.player_px > w.map.objective_px);
    CHECK(w.map.objective_px > w.map.marker_px);
    // Declutter ruling: trails off, zone labels off, lake labels thinned, and
    // (S-mapteam) EVERY place name off — "rEMOVE ALL THE PLACE NAMES".
    CHECK(w.map.trails_visible == false);
    CHECK(w.map.zone_labels_visible == false);
    CHECK(w.map.place_labels_visible == false);
    CHECK(w.map.lake_label_span_m > 3000.0);
    // Territory wash OFF by default — a team wash under team markers IS the
    // camouflage Chad called out.
    CHECK(w.map.territory_fill == Catch::Approx(0.0));
    // The arrow guard is ARMED (a 0 here would ship the spin back).
    CHECK(w.map.arrow_hold_sin > 0.0);
}

TEST_CASE("load_world: a chroma-tinted map basemap colour is rejected") {
    const std::string base = slurp(SEADS_CONFIG_DIR "/world.toml");
    REQUIRE(base.find("water               = [0.738, 0.740, 0.744]") !=
            std::string::npos);
    // "newspaper greyscale for the look" is a RULING, enforced executably:
    // the plate carries no chroma, so nothing on it can camouflage a marker.
    // Re-tinting the lakes blue (the pre-S-mapread look) must not load.
    const std::string bad =
        replace_all(base, "water               = [0.738, 0.740, 0.744]",
                    "water               = [0.57, 0.72, 0.82]");
    CHECK_THROWS(cfg::load_world_toml(write_temp(bad, "map_water_chroma")));
}

TEST_CASE("load_world: a near-miss ally colour is rejected") {
    const std::string base = slurp(SEADS_CONFIG_DIR "/world.toml");
    REQUIRE(base.find("ally      = [0.12, 0.57, 1.00]") != std::string::npos);
    // THE TRAP, at the config edge: (0.12, 0.55, 1.00) is the channel-reversal
    // of the slag orange. It LOOKS like the complement and is 181.36 deg away,
    // not 180 — indistinguishable on screen, so only the loader's executable
    // check can hold Chad's "with precision". A hand edit to it must not load.
    const std::string bad = replace_all(base, "ally      = [0.12, 0.57, 1.00]",
                                        "ally      = [0.12, 0.55, 1.00]");
    CHECK_THROWS(cfg::load_world_toml(write_temp(bad, "teams_ally_nearmiss")));
    // A same-hue but desaturated blue is also NOT the complement (S must
    // match).
    const std::string bad2 = replace_all(base, "ally      = [0.12, 0.57, 1.00]",
                                         "ally      = [0.45, 0.71, 1.00]");
    CHECK_THROWS(cfg::load_world_toml(write_temp(bad2, "teams_ally_desat")));
    // The shipped value itself loads (the leg is not vacuously throwing).
    CHECK_NOTHROW(cfg::load_world_toml(write_temp(base, "teams_ally_ok")));
}

TEST_CASE("load_world: a zero-size map marker is rejected") {
    const std::string base = slurp(SEADS_CONFIG_DIR "/world.toml");
    REQUIRE(base.find("marker_px           = 5.5") != std::string::npos);
    // A zero marker is an INVISIBLE marker — the silent-disarm class this
    // whole pass exists to fix, so it is rejected rather than clamped.
    const std::string bad = replace_all(base, "marker_px           = 5.5",
                                        "marker_px           = 0.0");
    CHECK_THROWS(cfg::load_world_toml(write_temp(bad, "map_marker_zero")));
}

TEST_CASE("load_world: an arrow_hold_sin of 1 is rejected") {
    const std::string base = slurp(SEADS_CONFIG_DIR "/world.toml");
    REQUIRE(base.find("arrow_hold_sin      = 0.15") != std::string::npos);
    // At exactly 1 the degeneracy guard never releases and the arrow freezes
    // at north forever — a silently-dead arrow, not a tune.
    const std::string bad = replace_all(base, "arrow_hold_sin      = 0.15",
                                        "arrow_hold_sin      = 1.0");
    CHECK_THROWS(cfg::load_world_toml(write_temp(bad, "map_arrow_hold_one")));
}

// ★ L4 (the millwright map). The view dials and the per-zoom declutter, and
// the ONE bound that is a ruling rather than a range: a declutter threshold at
// or below 1 would force that layer on at FIT and silently change the shipped
// overview, which "zoom = fit draws exactly what it draws today" forbids.
TEST_CASE("load_world: [map] ships the L4 zoom band and declutter thresholds") {
    const cfg::WorldParams w =
        cfg::load_world_toml(SEADS_CONFIG_DIR "/world.toml");
    CHECK(w.map.zoom_step > 1.0);   // the wheel is ARMED, not the kill switch
    CHECK(w.map.zoom_max > 1.0);
    // Follow must be reachable in BOTH directions: strictly inside the band.
    CHECK(w.map.follow_zoom >= 1.0);
    CHECK(w.map.follow_zoom < w.map.zoom_max);
    // Every declutter threshold is ABOVE fit, so the overview is untouched.
    CHECK(w.map.trails_zoom > 1.0);
    CHECK(w.map.minor_roads_zoom > 1.0);
    CHECK(w.map.place_labels_zoom > 1.0);
    // Names come back LATER than trails -- the reading order Chad described
    // (the trail you are on first, the name of where you are second).
    CHECK(w.map.place_labels_zoom >= w.map.trails_zoom);
}

TEST_CASE("load_world: a fit-zoom declutter threshold is rejected") {
    const std::string base = slurp(SEADS_CONFIG_DIR "/world.toml");
    REQUIRE(base.find("trails_zoom         = 4.0") != std::string::npos);
    // 1.0 means "on at fit", which silently re-clutters the overview Chad
    // signed off. Rejected at load, not clamped: a clamp would ship a chart
    // that disagrees with its own config file.
    const std::string bad = replace_all(base, "trails_zoom         = 4.0",
                                        "trails_zoom         = 1.0");
    CHECK_THROWS(cfg::load_world_toml(write_temp(bad, "map_trails_zoom_fit")));
}

TEST_CASE("load_world: a follow_zoom outside the zoom band is rejected") {
    const std::string base = slurp(SEADS_CONFIG_DIR "/world.toml");
    REQUIRE(base.find("follow_zoom         = 4.0") != std::string::npos);
    // At or past the ceiling the chart could never follow at all -- a dial
    // whose whole range is unreachable is a disarmed tripwire.
    const std::string bad = replace_all(base, "follow_zoom         = 4.0",
                                        "follow_zoom         = 64.0");
    CHECK_THROWS(cfg::load_world_toml(write_temp(bad, "map_follow_at_max")));
}

TEST_CASE("load_world: a negative Mie halo gain is rejected") {
    const std::string base = slurp(SEADS_CONFIG_DIR "/world.toml");
    REQUIRE(base.find("mie_halo_gain       = 0.30") != std::string::npos);
    // A negative gain SUBTRACTS light toward the sun — not a dimmer, a hole.
    const std::string bad = replace_all(base, "mie_halo_gain       = 0.30",
                                        "mie_halo_gain       = -0.5");
    CHECK_THROWS(cfg::load_world_toml(write_temp(bad, "mie_halo_negative")));
}

TEST_CASE("load_world: a star air_extinction outside [0,1] is rejected") {
    const std::string base = slurp(SEADS_CONFIG_DIR "/world.toml");
    REQUIRE(base.find("air_extinction     = 0.0") != std::string::npos);
    // It is a FRACTION of the air's veiling, so >1 is meaningless.
    const std::string bad = replace_all(base, "air_extinction     = 0.0",
                                        "air_extinction     = 1.5");
    CHECK_THROWS(cfg::load_world_toml(write_temp(bad, "star_air_ext_range")));
}

TEST_CASE("load_world: an out-of-range mie_g is rejected") {
    const std::string base = slurp(SEADS_CONFIG_DIR "/world.toml");
    REQUIRE(base.find("mie_g               = 0.66") != std::string::npos);
    // g >= 0.95 approaches the Henyey-Greenstein forward-peak blowup — a typo,
    // not a tune (the loader caps at 0.95).
    const std::string bad = replace_all(base, "mie_g               = 0.66",
                                        "mie_g               = 0.98");
    CHECK_THROWS(cfg::load_world_toml(write_temp(bad, "mie_g_over_cap")));
}

TEST_CASE("load_world: an over-cap scatter_strength is rejected") {
    const std::string base = slurp(SEADS_CONFIG_DIR "/world.toml");
    REQUIRE(base.find("scatter_strength    = 1.0") != std::string::npos);
    // strength scales the ambient (ex-halo) sky tint; unbounded it washes the
    // whole overcast sky past clip (AFTER-red-team P1) — cap at 4, a typo
    // guard.
    const std::string bad = replace_all(base, "scatter_strength    = 1.0",
                                        "scatter_strength    = 50.0");
    CHECK_THROWS(
        cfg::load_world_toml(write_temp(bad, "scatter_strength_over_cap")));
}

TEST_CASE("load_world: the weather block loads (Stage 3 weather variable)") {
    const cfg::WorldParams w =
        cfg::load_world_toml(SEADS_CONFIG_DIR "/world.toml");
    CHECK(w.weather.period1_s == Catch::Approx(1499.0));
    CHECK(w.weather.period2_s == Catch::Approx(547.0));
    CHECK(w.weather.period3_s == Catch::Approx(197.0));
    CHECK(w.weather.weight1 == Catch::Approx(0.5));
    CHECK(w.weather.weight2 == Catch::Approx(0.3));
    CHECK(w.weather.weight3 == Catch::Approx(0.2));
    CHECK(w.weather.phase1 == Catch::Approx(3.6));
    CHECK(w.weather.phase2 == Catch::Approx(1.7));
    CHECK(w.weather.phase3 == Catch::Approx(4.2));
    CHECK(w.weather.gate_lo == Catch::Approx(-0.05));
    CHECK(w.weather.gate_hi == Catch::Approx(1.10));
}

TEST_CASE("load_world: [weather] matches the render::WeatherParams defaults") {
    // The Fable-vetted numbers live in THREE places: the render::WeatherParams
    // header defaults, world.toml [weather], and the literal pins above. The
    // property tests (distribution / spawn-clear / slew) in test_weather.cpp
    // run on the HEADER DEFAULTS, so if the toml diverges those tests silently
    // stop covering the SHIPPED design (the config/default fork, Fable P1-2).
    // Lock the two together: a legit toml retune now ALSO fails here until the
    // header defaults are updated to match — which re-runs the property tests
    // on the new design (the intended workflow), never a silent divergence.
    const cfg::WorldParams w =
        cfg::load_world_toml(SEADS_CONFIG_DIR "/world.toml");
    const render::WeatherParams d;  // the shipped design defaults
    CHECK(w.weather.period1_s == Catch::Approx(d.period1_s));
    CHECK(w.weather.period2_s == Catch::Approx(d.period2_s));
    CHECK(w.weather.period3_s == Catch::Approx(d.period3_s));
    CHECK(w.weather.weight1 == Catch::Approx(d.weight1));
    CHECK(w.weather.weight2 == Catch::Approx(d.weight2));
    CHECK(w.weather.weight3 == Catch::Approx(d.weight3));
    CHECK(w.weather.phase1 == Catch::Approx(d.phase1));
    CHECK(w.weather.phase2 == Catch::Approx(d.phase2));
    CHECK(w.weather.phase3 == Catch::Approx(d.phase3));
    CHECK(w.weather.gate_lo == Catch::Approx(d.gate_lo));
    CHECK(w.weather.gate_hi == Catch::Approx(d.gate_hi));
}

TEST_CASE("load_world: weather weights that do not sum to 1 are rejected") {
    const std::string base = slurp(SEADS_CONFIG_DIR "/world.toml");
    REQUIRE(base.find("weight1             = 0.5") != std::string::npos);
    // The weights set the [-1,1] signal support the gate is calibrated against;
    // a different sum silently slides the whole clear/haze/overcast mix.
    const std::string bad = replace_all(base, "weight1             = 0.5",
                                        "weight1             = 0.8");
    CHECK_THROWS(cfg::load_world_toml(write_temp(bad, "weather_weight_sum")));
}

TEST_CASE("load_world: a weather gate_lo >= gate_hi is rejected") {
    const std::string base = slurp(SEADS_CONFIG_DIR "/world.toml");
    REQUIRE(base.find("gate_hi             = 1.10") != std::string::npos);
    // gate_lo >= gate_hi collapses the smoothstep gate (edge0 >= edge1). The
    // forcing value must sit below the SHIPPED gate_lo, which S-bubbleweather
    // moved to -0.05 — the old 0.05 is now a legal span and stopped forcing.
    const std::string bad = replace_all(base, "gate_hi             = 1.10",
                                        "gate_hi             = -0.10");
    CHECK_THROWS(cfg::load_world_toml(write_temp(bad, "weather_gate_span")));
}

TEST_CASE("load_world: the weather_cell block loads (W2 localized field)") {
    const cfg::WorldParams w =
        cfg::load_world_toml(SEADS_CONFIG_DIR "/world.toml");
    CHECK(w.weather_cell.cell_count == 24);
    CHECK(w.weather_cell.inner_deg == Catch::Approx(8.0));
    CHECK(w.weather_cell.outer_deg == Catch::Approx(16.0));
    CHECK(w.weather_cell.thresh_lo == Catch::Approx(0.05));
    CHECK(w.weather_cell.thresh_hi == Catch::Approx(0.65));
    CHECK(w.weather_cell.env_width == Catch::Approx(0.15));
    // S-bubbleweather: the anchored (in-bubble) lattice's own scale.
    CHECK(w.weather_cell.bubble_cell_count == 16);
    CHECK(w.weather_cell.bubble_inner_deg == Catch::Approx(2.5));
    CHECK(w.weather_cell.bubble_outer_deg == Catch::Approx(6.0));
    CHECK(w.weather_cell.bubble_fill_frac == Catch::Approx(0.80));
}

TEST_CASE(
    "load_world: [weather_cell] matches render::WeatherCellParams defaults") {
    // Same config/default fork guard as [weather] above: the weather_cell
    // property tests in test_weather.cpp run on the HEADER defaults, so lock
    // the toml to them — a toml retune fails here until the header default
    // matches, which re-runs the field tests on the new design (never a silent
    // fork).
    const cfg::WorldParams w =
        cfg::load_world_toml(SEADS_CONFIG_DIR "/world.toml");
    const render::WeatherCellParams d;  // the shipped design defaults
    CHECK(w.weather_cell.cell_count == d.cell_count);
    CHECK(w.weather_cell.inner_deg == Catch::Approx(d.inner_deg));
    CHECK(w.weather_cell.outer_deg == Catch::Approx(d.outer_deg));
    CHECK(w.weather_cell.thresh_lo == Catch::Approx(d.thresh_lo));
    CHECK(w.weather_cell.thresh_hi == Catch::Approx(d.thresh_hi));
    CHECK(w.weather_cell.env_width == Catch::Approx(d.env_width));
    CHECK(w.weather_cell.bubble_cell_count == d.bubble_cell_count);
    CHECK(w.weather_cell.bubble_inner_deg == Catch::Approx(d.bubble_inner_deg));
    CHECK(w.weather_cell.bubble_outer_deg == Catch::Approx(d.bubble_outer_deg));
    CHECK(w.weather_cell.bubble_fill_frac == Catch::Approx(d.bubble_fill_frac));
}

TEST_CASE("load_world: a weather_cell inner_deg >= outer_deg is rejected") {
    const std::string base = slurp(SEADS_CONFIG_DIR "/world.toml");
    REQUIRE(base.find("inner_deg           = 8.0") != std::string::npos);
    // inner >= outer collapses the dot-space falloff smoothstep (edge0 >=
    // edge1).
    const std::string bad = replace_all(base, "inner_deg           = 8.0",
                                        "inner_deg           = 20.0");
    CHECK_THROWS(cfg::load_world_toml(write_temp(bad, "weather_cell_cone")));
}

TEST_CASE("load_world: a weather_cell thresh_lo >= thresh_hi is rejected") {
    const std::string base = slurp(SEADS_CONFIG_DIR "/world.toml");
    REQUIRE(base.find("thresh_hi           = 0.65") != std::string::npos);
    // Non-ordered thresholds break the per-cell activation spread.
    const std::string bad = replace_all(base, "thresh_hi           = 0.65",
                                        "thresh_hi           = 0.01");
    CHECK_THROWS(cfg::load_world_toml(write_temp(bad, "weather_cell_thresh")));
}

TEST_CASE("load_world: the precip block loads (W3 snow/rain)") {
    const cfg::WorldParams w =
        cfg::load_world_toml(SEADS_CONFIG_DIR "/world.toml");
    CHECK(w.precip.enabled == true);
    CHECK(w.precip.cell_size_m == Catch::Approx(3.0));
    CHECK(w.precip.box_half_m == Catch::Approx(21.0));
    CHECK(w.precip.wrap_fade == Catch::Approx(0.12));
    CHECK(w.precip.color.x == Catch::Approx(0.90));
    CHECK(w.precip.snow_size_m == Catch::Approx(0.18));
    // AS-3: snow_rate_hz RETIRED -> snow_speed_mps. 0.60 m/s == the shipped
    // 0.20 Hz x the 3.0 m cell, so this is a rename at the ship value.
    CHECK(w.precip.snow_speed_mps == Catch::Approx(0.60));
    CHECK(w.precip.snow_opacity == Catch::Approx(0.75));
    CHECK(w.precip.rain_size_m == Catch::Approx(0.05));
    CHECK(w.precip.rain_streak_m == Catch::Approx(1.30));
    CHECK(w.precip.rain_rate_hz == Catch::Approx(1.50));
    CHECK(w.precip.rain_opacity == Catch::Approx(0.55));
    // ATMOSPHERE AS-1/AS-2/AS-3 keys (all STRICT-required, see load_world.cpp).
    CHECK(w.precip.flurry_level == Catch::Approx(0.35));
    CHECK(w.precip.flurry_thresh_lo == Catch::Approx(0.00));
    CHECK(w.precip.flurry_thresh_hi == Catch::Approx(0.65));
    CHECK(w.precip.flurry_period_scale == Catch::Approx(1.4));
    CHECK(w.precip.flurry_inner_deg == Catch::Approx(6.0));
    CHECK(w.precip.flurry_outer_deg == Catch::Approx(14.0));
    CHECK(w.precip.flurry_cell_count == Catch::Approx(5.0));
    CHECK(w.precip.flurry_gate_lo == Catch::Approx(-0.95));
    CHECK(w.precip.flurry_phase_off == Catch::Approx(2.6));
    CHECK(w.precip.snow_floor == Catch::Approx(0.0));
    // AS-5: the snow squall's own dials.
    CHECK(w.precip.squall_own_dials);
    CHECK(w.precip.squall_inner_deg == Catch::Approx(8.0));
    CHECK(w.precip.squall_outer_deg == Catch::Approx(15.0));
    CHECK(w.precip.squall_cell_count == Catch::Approx(3.0));
    CHECK(w.precip.squall_gate_lo == Catch::Approx(-0.65));
    CHECK(w.precip.squall_thresh_lo == Catch::Approx(0.0));
    CHECK(w.precip.squall_thresh_hi == Catch::Approx(0.35));
    CHECK(w.precip.squall_period_scale == Catch::Approx(1.0));
    CHECK(w.precip.squall_phase_off == Catch::Approx(0.0));
    CHECK(w.precip.rock_band_m == Catch::Approx(2.0));
    CHECK(w.precip.size_var == Catch::Approx(0.40));
    CHECK(w.precip.alpha_var == Catch::Approx(0.40));
    CHECK(w.precip.density_exp == Catch::Approx(0.60));
    CHECK(w.precip.density_soft == Catch::Approx(0.08));
    CHECK(w.precip.sway_m == Catch::Approx(0.25));
    CHECK(w.precip.veil_sway_m == Catch::Approx(0.60));
    CHECK(w.precip.veil_enabled == true);
    CHECK(w.precip.veil_cell_size_m == Catch::Approx(8.0));
    CHECK(w.precip.veil_box_half_m == Catch::Approx(70.0));
    CHECK(w.precip.veil_size_m == Catch::Approx(0.45));
    CHECK(w.precip.veil_opacity == Catch::Approx(0.35));
    CHECK(w.precip.veil_inner_fade_m == Catch::Approx(12.0));
    CHECK(w.precip.rim_dark == Catch::Approx(1.0));
}

TEST_CASE("load_world: a precip box_half_m < cell_size_m is rejected") {
    const std::string base = slurp(SEADS_CONFIG_DIR "/world.toml");
    REQUIRE(base.find("box_half_m          = 21.0") != std::string::npos);
    // A box smaller than one cell holds no lattice cell each way (degenerate).
    const std::string bad = replace_all(base, "box_half_m          = 21.0",
                                        "box_half_m          = 2.0");
    CHECK_THROWS(cfg::load_world_toml(write_temp(bad, "precip_box_lt_cell")));
}

TEST_CASE("load_world: a precip wrap_fade >= 0.5 is rejected") {
    const std::string base = slurp(SEADS_CONFIG_DIR "/world.toml");
    REQUIRE(base.find("wrap_fade           = 0.12") != std::string::npos);
    // wrap_fade >= 0.5 makes the two wrap fades overlap and null the flake.
    const std::string bad = replace_all(base, "wrap_fade           = 0.12",
                                        "wrap_fade           = 0.6");
    CHECK_THROWS(cfg::load_world_toml(write_temp(bad, "precip_wrapfade")));
}

TEST_CASE("load_world: the W4 ground snow-cover dials load + validate") {
    const cfg::WorldParams w =
        cfg::load_world_toml(SEADS_CONFIG_DIR "/world.toml");
    CHECK(w.ground.winter_snow_cover == Catch::Approx(0.85));
    CHECK(w.ground.snow_albedo == Catch::Approx(0.92));
    CHECK(w.ground.snow_slope_lo == Catch::Approx(0.55));
    // snow_slope_lo >= 1 would shed snow everywhere (the smoothstep never
    // fires) — rejected so the dial keeps a meaningful [0,1) range.
    const std::string base = slurp(SEADS_CONFIG_DIR "/world.toml");
    REQUIRE(base.find("snow_slope_lo       = 0.55") != std::string::npos);
    const std::string bad = replace_all(base, "snow_slope_lo       = 0.55",
                                        "snow_slope_lo       = 1.0");
    CHECK_THROWS(cfg::load_world_toml(write_temp(bad, "ground_snow_slope")));
}

TEST_CASE("load_world: the snow sparkle dials load + validate") {
    // Snow sparkle (Chad, twice: snow "appears to sparkle" in moon AND
    // starlight). Round-trip the committed defaults, then reject an
    // amplitude above 1 (blows out the shader's clamp budget) and an
    // exponent below 8 (a broad sheen, not sparkle -- defeats the anti-moire
    // fade).
    const cfg::WorldParams w =
        cfg::load_world_toml(SEADS_CONFIG_DIR "/world.toml");
    CHECK(w.ground.snow_sparkle == Catch::Approx(0.4));
    CHECK(w.ground.snow_sparkle_sharp == Catch::Approx(120.0));

    const std::string base = slurp(SEADS_CONFIG_DIR "/world.toml");
    REQUIRE(base.find("snow_sparkle        = 0.4") != std::string::npos);
    REQUIRE(base.find("snow_sparkle_sharp  = 120.0") != std::string::npos);

    const std::string bad_amp =
        replace_all(base, "snow_sparkle        = 0.4",
                    "snow_sparkle        = 1.5");
    CHECK_THROWS(
        cfg::load_world_toml(write_temp(bad_amp, "snow_sparkle_over_1")));

    const std::string bad_sharp =
        replace_all(base, "snow_sparkle_sharp  = 120.0",
                    "snow_sparkle_sharp  = 2.0");
    CHECK_THROWS(cfg::load_world_toml(
        write_temp(bad_sharp, "snow_sparkle_sharp_under_8")));
}

TEST_CASE("load_world: the S6 halftone/dither dials load + validate") {
    const cfg::WorldParams w =
        cfg::load_world_toml(SEADS_CONFIG_DIR "/world.toml");
    // Committed default is OFF (enabled=0) -> the post pass is bit-exact
    // identity.
    CHECK(w.halftone.enabled == false);
    CHECK(w.halftone.style == 1);
    CHECK(w.halftone.scale_px == Catch::Approx(4.0));
    CHECK(w.halftone.angle_rad == Catch::Approx(0.7853982));  // rad(45 deg)
    CHECK(w.halftone.soft == Catch::Approx(1.0));
    CHECK(w.halftone.ink == Catch::Approx(0.05));
    CHECK(w.halftone.grain_mul == Catch::Approx(0.25));
}

TEST_CASE("load_world: a halftone style outside {1,2} is rejected") {
    // style is the shader branch selector; a stray value would silently no-op
    // the mode (mode>0 with no matching branch = the dots path by fallthrough)
    // — reject.
    const std::string base = slurp(SEADS_CONFIG_DIR "/world.toml");
    REQUIRE(base.find("style               = 1") != std::string::npos);
    const std::string bad =
        replace_all(base, "style               = 1", "style               = 3");
    CHECK_THROWS(cfg::load_world_toml(write_temp(bad, "halftone_style")));
}

TEST_CASE(
    "load_world: a halftone scale_px below the NaN/subsample floor is "
    "rejected") {
    // The shader divides gl_FragCoord by scale_px (0 = NaN) and below ~2 px the
    // dots subsample and shimmer regardless of AA (Fable-BEFORE).
    const std::string base = slurp(SEADS_CONFIG_DIR "/world.toml");
    REQUIRE(base.find("scale_px            = 4.0") != std::string::npos);
    const std::string bad = replace_all(base, "scale_px            = 4.0",
                                        "scale_px            = 1.0");
    CHECK_THROWS(cfg::load_world_toml(write_temp(bad, "halftone_scale")));
}

TEST_CASE("load_world: the seasons block loads (W1 framing)") {
    const cfg::WorldParams w =
        cfg::load_world_toml(SEADS_CONFIG_DIR "/world.toml");
    // Committed default is "winter" (index 0) with equal draw weights kept for
    // whenever the table goes back to "random". RULING (Chad, 2026-08-09):
    // "lets make the default winter from now on for this game loop" — Scarce
    // Skies is a winter world, and a fixed season also makes --smoke shots show
    // the same season he flies (static_season outranks the smoke default).
    render::Season winter;
    REQUIRE(render::season_from_string("winter", winter));
    CHECK(w.seasons.static_season == static_cast<int>(winter));
    CHECK(w.seasons.weight_winter == Catch::Approx(1.0));
    CHECK(w.seasons.weight_spring == Catch::Approx(1.0));
    CHECK(w.seasons.weight_summer == Catch::Approx(1.0));
    CHECK(w.seasons.weight_autumn == Catch::Approx(1.0));
}

TEST_CASE(
    "load_world: a static_season name resolves to the render::Season index") {
    // The loader's LOCAL season_index() must agree with
    // render::season_from_string (the shared-contract lock — a fork silently
    // forces the wrong season). Mutate to a season OTHER than the shipped one,
    // so this leg keeps testing the name->index resolution rather than the
    // committed value (which moved to "winter" on Chad's 2026-08-09 ruling and
    // may move again).
    const std::string base = slurp(SEADS_CONFIG_DIR "/world.toml");
    REQUIRE(base.find("static_season       = \"winter\"") != std::string::npos);
    const std::string spring =
        replace_all(base, "static_season       = \"winter\"",
                    "static_season       = \"spring\"");
    const cfg::WorldParams w =
        cfg::load_world_toml(write_temp(spring, "season_static_spring"));
    render::Season s;
    REQUIRE(render::season_from_string("spring", s));
    CHECK(w.seasons.static_season == static_cast<int>(s));
    // And the shipped value itself resolves through the same shared contract.
    const cfg::WorldParams shipped =
        cfg::load_world_toml(SEADS_CONFIG_DIR "/world.toml");
    render::Season shipped_season;
    REQUIRE(render::season_from_string("winter", shipped_season));
    CHECK(shipped.seasons.static_season == static_cast<int>(shipped_season));
}

TEST_CASE("load_world: an unknown static_season is rejected") {
    const std::string base = slurp(SEADS_CONFIG_DIR "/world.toml");
    const std::string bad =
        replace_all(base, "static_season       = \"winter\"",
                    "static_season       = \"monsoon\"");
    CHECK_THROWS(cfg::load_world_toml(write_temp(bad, "season_bad_name")));
}

TEST_CASE("load_world: all-zero season draw weights are rejected") {
    // A zero sum is UB in the app-side std::discrete_distribution draw (P2-4).
    const std::string base = slurp(SEADS_CONFIG_DIR "/world.toml");
    REQUIRE(base.find("weight_winter       = 1.0") != std::string::npos);
    std::string bad = base;
    for (const char* k :
         {"weight_winter       = 1.0", "weight_spring       = 1.0",
          "weight_summer       = 1.0", "weight_autumn       = 1.0"}) {
        bad = replace_all(bad, k, std::string(k, 20) + "= 0.0");
    }
    CHECK_THROWS(cfg::load_world_toml(write_temp(bad, "season_zero_weights")));
}

TEST_CASE("load_world: a negative season draw weight is rejected") {
    const std::string base = slurp(SEADS_CONFIG_DIR "/world.toml");
    const std::string bad = replace_all(base, "weight_spring       = 1.0",
                                        "weight_spring       = -1.0");
    CHECK_THROWS(cfg::load_world_toml(write_temp(bad, "season_neg_weight")));
}

TEST_CASE("load_world: a non-finite season draw weight is rejected") {
    // inf/nan pass >=0 and Sum>0 but give discrete_distribution NaN probs
    // (P2-1).
    const std::string base = slurp(SEADS_CONFIG_DIR "/world.toml");
    const std::string bad = replace_all(base, "weight_summer       = 1.0",
                                        "weight_summer       = inf");
    CHECK_THROWS(cfg::load_world_toml(write_temp(bad, "season_inf_weight")));
}

TEST_CASE("load_world: an over-cap ground_day_gain is rejected") {
    const std::string base = slurp(SEADS_CONFIG_DIR "/world.toml");
    REQUIRE(base.find("ground_day_gain     = 1.60") != std::string::npos);
    // > 2 blows the albedo to white — a typo, not a tune (Stage 3b).
    const std::string bad = replace_all(base, "ground_day_gain     = 1.60",
                                        "ground_day_gain     = 3.0");
    CHECK_THROWS(cfg::load_world_toml(write_temp(bad, "ground_gain_over_cap")));
}

TEST_CASE("load_world: an out-of-range sky luminance is rejected") {
    const std::string base = slurp(SEADS_CONFIG_DIR "/world.toml");
    REQUIRE(base.find("sky_day             = 0.60") != std::string::npos);
    const std::string bad = replace_all(base, "sky_day             = 0.60",
                                        "sky_day             = 1.5");
    CHECK_THROWS(cfg::load_world_toml(write_temp(bad, "sky_over_1")));
}

TEST_CASE("load_world: an inverted tonal ladder is rejected") {
    // night <= dusk <= day is a monotone requirement; a night brighter than day
    // would invert the day/night blend. Push sky_night above sky_day.
    const std::string base = slurp(SEADS_CONFIG_DIR "/world.toml");
    REQUIRE(base.find("sky_night           = 0.05") != std::string::npos);
    const std::string bad = replace_all(base, "sky_night           = 0.05",
                                        "sky_night           = 0.95");
    CHECK_THROWS(cfg::load_world_toml(write_temp(bad, "inverted_ladder")));
}

TEST_CASE("load_world: sky_space above the night band is rejected") {
    // SPACE-FIRST: space is the darkest backdrop (space <= night <= dusk <=
    // day). A space value brighter than the night band would light the zenith
    // above the horizon at night, inverting the model. Push sky_space above
    // sky_night.
    const std::string base = slurp(SEADS_CONFIG_DIR "/world.toml");
    REQUIRE(base.find("sky_space           = 0.02") != std::string::npos);
    const std::string bad = replace_all(base, "sky_space           = 0.02",
                                        "sky_space           = 0.50");
    CHECK_THROWS(cfg::load_world_toml(write_temp(bad, "space_over_night")));
}

TEST_CASE(
    "load_world: the frozen epoch is DAYTIME over the +X spawn (Stage 2)") {
    // The Stage-2 sky is FROZEN at t_epoch = 0; the "day baseline" the probe
    // grades is only valid if the sun is actually up over the +X spawn
    // sub-point (P1-4 — else the gate silently measures dusk). Guards an epoch
    // retune.
    const cfg::WorldParams w =
        cfg::load_world_toml(SEADS_CONFIG_DIR "/world.toml");
    render::CelestialConfig ccfg;  // only the epoch + basis drive the sun dir;
    ccfg.orbit_normal = w.celestial.orbit_normal;  // other fields keep valid
    ccfg.tilt_deg = w.celestial.tilt_deg;          // make_celestial defaults
    ccfg.tilt_lean = w.celestial.tilt_lean;
    ccfg.day_period_s = w.celestial.day_period_s;
    ccfg.year_period_s = w.celestial.year_period_s;
    ccfg.epoch_day_frac = w.celestial.epoch_day_frac;
    ccfg.epoch_year_frac = w.celestial.epoch_year_frac;
    const render::CelestialParams cel = render::make_celestial(ccfg, 15000.0);
    const glm::dvec3 to_sun = render::sun_dir(cel, 0.0);  // direction TO sun
    const glm::dvec3 spawn_up{1.0, 0.0, 0.0};             // +X spawn up
    const double sun_elev =
        std::asin(glm::clamp(glm::dot(to_sun, spawn_up), -1.0, 1.0));
    REQUIRE(sun_elev > w.atmosphere.dusk_hi_rad);  // above the dusk band => day
}

TEST_CASE("load_world: a stale [sky] table is rejected (superseded schema)") {
    // require() throws only on MISSING keys, not unknown ones, so a leftover
    // [sky] table would parse silently alongside [atmosphere] — the loader must
    // reject it so the Stage-2 migration is strict (P1-2). Append a [sky]
    // table.
    const std::string base = slurp(SEADS_CONFIG_DIR "/world.toml");
    const std::string bad = base + "\n[sky]\nsilver = 0.82\n";
    CHECK_THROWS(cfg::load_world_toml(write_temp(bad, "stale_sky")));
}

TEST_CASE("load_world: a non-binary ground toggle is rejected") {
    const std::string base = slurp(SEADS_CONFIG_DIR "/world.toml");
    REQUIRE(base.find("use_procedural      = 1") != std::string::npos);
    // The A/B toggle is a hard 0/1; 2 is not a mode.
    const std::string bad =
        replace_all(base, "use_procedural      = 1", "use_procedural      = 2");
    CHECK_THROWS(cfg::load_world_toml(write_temp(bad, "ground_toggle")));
}

TEST_CASE("load_world: a gritty grain past the crisp ceiling is rejected") {
    const std::string base = slurp(SEADS_CONFIG_DIR "/world.toml");
    REQUIRE(base.find("grain               = 0.02") != std::string::npos);
    // 0.5 > the 0.25 ceiling: Chad's ruling is high-contrast, NOT grainy — the
    // loader rejects a dust storm, not just an out-of-[0,1) value.
    const std::string bad = replace_all(base, "grain               = 0.02",
                                        "grain               = 0.5");
    CHECK_THROWS(cfg::load_world_toml(write_temp(bad, "grainy")));
}

TEST_CASE("load_world: the fleet_rig mirror block loads (rig-A.2)") {
    const cfg::WorldParams w =
        cfg::load_world_toml(SEADS_CONFIG_DIR "/world.toml");
    CHECK(w.fleet_rig.reflectivity == Catch::Approx(0.45));
    CHECK(w.fleet_rig.fresnel_power == Catch::Approx(3.0));
    // S-mapteam 2026-08-09: per-plane chroma NO LONGER lives in [fleet_rig] —
    // the livery is the faction palette ([teams]), one table for map and sky.
    // Pinned here so a future re-add would have to face the fork question.
    CHECK(w.teams.ally.z == Catch::Approx(1.00));
    CHECK(w.teams.enemy.x == Catch::Approx(1.00));
    // rig-B deflection + prop knobs load.
    CHECK(w.fleet_rig.aileron_deg == Catch::Approx(18.0));
    CHECK(w.fleet_rig.elevator_deg == Catch::Approx(20.0));
    CHECK(w.fleet_rig.rudder_deg == Catch::Approx(22.0));
    CHECK(w.fleet_rig.gear_deploy_deg == Catch::Approx(85.0));
    CHECK(w.fleet_rig.prop_disc_alpha == Catch::Approx(0.30));
    CHECK(w.fleet_rig.prop_idle_alpha == Catch::Approx(0.06));
}

TEST_CASE("load_world: an out-of-range rig deflection throw is rejected") {
    const std::string base = slurp(SEADS_CONFIG_DIR "/world.toml");
    REQUIRE(base.find("elevator_deg        = 20.0") != std::string::npos);
    // A 90-degree elevator would swing the surface through the tailplane.
    const std::string bad = replace_all(base, "elevator_deg        = 20.0",
                                        "elevator_deg        = 90.0");
    CHECK_THROWS(cfg::load_world_toml(write_temp(bad, "elev_over_60")));
}

TEST_CASE("load_world: a prop idle alpha above the disc alpha is rejected") {
    const std::string base = slurp(SEADS_CONFIG_DIR "/world.toml");
    REQUIRE(base.find("prop_idle_alpha     = 0.06") != std::string::npos);
    // idle must be the FLOOR, not brighter than the full-throttle disc.
    const std::string bad = replace_all(base, "prop_idle_alpha     = 0.06",
                                        "prop_idle_alpha     = 0.9");
    CHECK_THROWS(cfg::load_world_toml(write_temp(bad, "idle_over_disc")));
}

TEST_CASE("load_world: an out-of-range plane color is rejected") {
    const std::string base = slurp(SEADS_CONFIG_DIR "/world.toml");
    REQUIRE(base.find("objective = [0.000, 0.518, 0.239]") !=
            std::string::npos);
    // A color component > 1 is not a fraction — RGB stays in [0,1]. Retargeted
    // to [teams] objective (S-mapteam): the per-plane [fleet_rig] colours are
    // gone, the faction palette IS the livery, and `objective` is the one
    // entry with no complement constraint layered on top of the range check.
    const std::string bad =
        replace_all(base, "objective = [0.000, 0.518, 0.239]",
                    "objective = [0.000, 0.518, 1.9]");
    CHECK_THROWS(cfg::load_world_toml(write_temp(bad, "color_over_1")));
}

TEST_CASE("load_world: a sub-unity furniture scale is rejected") {
    const std::string base = slurp(SEADS_CONFIG_DIR "/world.toml");
    REQUIRE(base.find("furniture_scale     = 5.0") != std::string::npos);
    const std::string bad = replace_all(base, "furniture_scale     = 5.0",
                                        "furniture_scale     = 0.5");
    CHECK_THROWS(cfg::load_world_toml(write_temp(bad, "tiny_furniture")));
}

TEST_CASE("load_world: the barren SLOPE GATE dials load (Chad's fly ruling 2026-08-11)") {
    // GATE(slope) = flat + (1-flat)*smoothstep(x_lo,x_hi,1-cos(slope)) --
    // the second factor of the barren shed law. flat barrens hold snow,
    // sloped faces are the blackest.
    const cfg::WorldParams w =
        cfg::load_world_toml(SEADS_CONFIG_DIR "/world.toml");
    CHECK(w.snowpack.barren_slope_lo_deg == Catch::Approx(8.0));
    CHECK(w.snowpack.barren_slope_hi_deg == Catch::Approx(12.0));
    CHECK(w.snowpack.barren_flat_shed_frac == Catch::Approx(0.10));
    // The degree->x helpers are the ONE home of that conversion (INV-9: the
    // shader receives their output as uniforms, never re-derives it).
    CHECK(w.snowpack.barren_slope_x_lo() ==
          Catch::Approx(1.0 - std::cos(8.0 * kPi / 180.0)));
    CHECK(w.snowpack.barren_slope_x_hi() ==
          Catch::Approx(1.0 - std::cos(12.0 * kPi / 180.0)));
}

TEST_CASE("load_world: the barren FACE-DARK dials load + validate (fly 3)") {
    // Chad's fly 3 (2026-08-11): "the really steep slopes need to be even
    // blacker so it stands out more." Render-only albedo kill past the shed
    // law's full-shed slope.
    const cfg::WorldParams w =
        cfg::load_world_toml(SEADS_CONFIG_DIR "/world.toml");
    CHECK(w.ground.barren_face_dark == Catch::Approx(0.95));
    CHECK(w.ground.barren_face_dark_hi_deg == Catch::Approx(20.0));
    CHECK(w.ground.barren_face_mottle == Catch::Approx(0.6));

    const std::string base = slurp(SEADS_CONFIG_DIR "/world.toml");
    REQUIRE(base.find("barren_face_dark        = 0.95") != std::string::npos);
    REQUIRE(base.find("barren_face_dark_hi_deg = 20.0") != std::string::npos);
    REQUIRE(base.find("barren_face_mottle      = 0.6") != std::string::npos);

    // > 1 would BRIGHTEN bright mottle patches under face-dark.
    const std::string bad_mottle =
        replace_all(base, "barren_face_mottle      = 0.6",
                    "barren_face_mottle      = 1.5");
    CHECK_THROWS(cfg::load_world_toml(
        write_temp(bad_mottle, "barren_face_mottle_over_1")));

    // > 1 flips the albedo negative at full ramp.
    const std::string bad_amp =
        replace_all(base, "barren_face_dark        = 0.95",
                    "barren_face_dark        = 1.5");
    CHECK_THROWS(
        cfg::load_world_toml(write_temp(bad_amp, "barren_face_dark_over_1")));

    // The ramp STARTS at the shed law's barren_slope_hi_deg (12): an end at
    // or below it is a step, not a ramp -- a decal edge on every face.
    const std::string bad_end =
        replace_all(base, "barren_face_dark_hi_deg = 20.0",
                    "barren_face_dark_hi_deg = 12.0");
    CHECK_THROWS(cfg::load_world_toml(
        write_temp(bad_end, "barren_face_dark_end_below_shed_hi")));
}

TEST_CASE("load_world: an out-of-range barren_flat_shed_frac is rejected") {
    const std::string base = slurp(SEADS_CONFIG_DIR "/world.toml");
    REQUIRE(base.find("barren_flat_shed_frac  = 0.10") != std::string::npos);
    // > 1 would remove MORE than the whole ambient depth from flat barrens --
    // the gate is a FRACTION of the shed, not a free multiplier.
    const std::string bad =
        replace_all(base, "barren_flat_shed_frac  = 0.10",
                    "barren_flat_shed_frac  = 1.5");
    CHECK_THROWS(
        cfg::load_world_toml(write_temp(bad, "barren_flat_shed_over_1")));
}

TEST_CASE("load_world: an inverted barren slope ramp is rejected") {
    // hi <= lo collapses the feather to a hard step -- a decal edge on every
    // hillside, exactly what LAW 2.4c's ramp discipline forbids elsewhere.
    const std::string base = slurp(SEADS_CONFIG_DIR "/world.toml");
    REQUIRE(base.find("barren_slope_lo_deg    = 8.0") != std::string::npos);
    REQUIRE(base.find("barren_slope_hi_deg    = 12.0") != std::string::npos);

    const std::string bad_inverted =
        replace_all(base, "barren_slope_hi_deg    = 12.0",
                    "barren_slope_hi_deg    = 2.0");
    CHECK_THROWS(cfg::load_world_toml(
        write_temp(bad_inverted, "barren_slope_hi_below_lo")));

    const std::string bad_over_90 =
        replace_all(base, "barren_slope_hi_deg    = 12.0",
                    "barren_slope_hi_deg    = 95.0");
    CHECK_THROWS(cfg::load_world_toml(
        write_temp(bad_over_90, "barren_slope_hi_over_90")));

    const std::string bad_zero_lo =
        replace_all(base, "barren_slope_lo_deg    = 8.0",
                    "barren_slope_lo_deg    = 0.0");
    CHECK_THROWS(
        cfg::load_world_toml(write_temp(bad_zero_lo, "barren_slope_lo_zero")));
}

// --- [plane_legibility]: RUNG E4 VISIBILITY ------------------------------
// Chad 2026-08-20: "they are hard to see especially in the white scatter
// light, their tag and color is also a bit hard to see." The gate cannot see
// the frame, so what is pinned here is the CONTRACT: the block is required,
// the shipped values are in band, the relations hold, and each guard actually
// throws on the value it names.

TEST_CASE("load_world: the plane_legibility block loads in band (rung E4)") {
    const cfg::WorldParams w =
        cfg::load_world_toml(SEADS_CONFIG_DIR "/world.toml");
    const render::LegibilityParams& lg = w.legibility;
    // CONFIG-RELATIVE, not a copy of the shipped numbers: this leg pins the
    // BANDS and the RELATIONS, so a retune Chad asks for does not fail the
    // gate but a nonsensical one does.
    CHECK(lg.aircraft_haze_frac >= 0.0);
    CHECK(lg.aircraft_haze_frac <= 1.0);
    CHECK(lg.haze_full_range_m >= 0.0);
    CHECK(lg.tag_outline_px >= 0.0);
    CHECK(lg.tag_outline_px <= 4.0);
    CHECK((lg.tag_min_px == 0.0 ||
           (lg.tag_min_px >= 8.0 && lg.tag_min_px <= 48.0)));
    CHECK(lg.enemy_tint_shift >= 0.0);
    CHECK(lg.enemy_tint_shift <= 1.0);
    CHECK(lg.glint_size_m >= 0.0);
    CHECK(lg.glint_size_m <= 3.0);
    CHECK(lg.glint_duty >= 0.0);
    CHECK(lg.glint_duty <= 1.0);
    // The silent-disarm guard: a sized bead with no range/period/duty draws
    // nothing while the config LOOKS on.
    if (lg.glint_size_m > 0.0) {
        CHECK(lg.glint_range_m > 0.0);
        CHECK(lg.glint_period_frames > 0);
        CHECK(lg.glint_duty > 0.0);
    }
    // The SHIPPED set must actually be ON — E4 exists because the off frame is
    // the one Chad could not read. (Each dial's off value is pinned as an
    // identity in test_plane_legibility; this is the "we shipped it" leg.)
    CHECK(lg.aircraft_haze_frac < 1.0);
    CHECK(lg.tag_outline_px > 0.0);
    CHECK(lg.tag_min_px > 12.0);  // above render/draw.cpp's base glyph height
    CHECK(lg.enemy_tint_shift > 0.0);
    CHECK(lg.glint_size_m > 0.0);
    // E4.2 alpha floor (Fable adjudication): bounded and SHIPPED on.
    CHECK(lg.tag_min_alpha >= 0.0);
    CHECK(lg.tag_min_alpha <= 200.0);
    CHECK(lg.tag_min_alpha > 0.0);
}

TEST_CASE("load_world: a tag_min_alpha above the base tag alpha is rejected") {
    // 200 is the base tag alpha in render/draw.cpp; a floor above it is a
    // config lie (the tag never draws brighter than its base).
    const std::string base = slurp(SEADS_CONFIG_DIR "/world.toml");
    REQUIRE(base.find("tag_min_alpha       = 140.0") != std::string::npos);
    const std::string bad = replace_all(base, "tag_min_alpha       = 140.0",
                                        "tag_min_alpha       = 255.0");
    CHECK_THROWS(cfg::load_world_toml(write_temp(bad, "tag_alpha_over")));
}

TEST_CASE("load_world: an out-of-range plane_legibility haze frac is rejected") {
    const std::string base = slurp(SEADS_CONFIG_DIR "/world.toml");
    REQUIRE(base.find("aircraft_haze_frac  = 0.35") != std::string::npos);
    // 1.4 would mean "MORE env wash than the signed frame" — the dial only
    // ever removes wash.
    const std::string bad = replace_all(base, "aircraft_haze_frac  = 0.35",
                                        "aircraft_haze_frac  = 1.4");
    CHECK_THROWS(cfg::load_world_toml(write_temp(bad, "haze_frac_over_1")));
}

TEST_CASE("load_world: an absurd plane_legibility tag halo is rejected") {
    const std::string base = slurp(SEADS_CONFIG_DIR "/world.toml");
    REQUIRE(base.find("tag_outline_px      = 1.0") != std::string::npos);
    // A halo wider than the glyph turns the tag into a black smudge.
    const std::string bad = replace_all(base, "tag_outline_px      = 1.0",
                                        "tag_outline_px      = 9.0");
    CHECK_THROWS(cfg::load_world_toml(write_temp(bad, "tag_halo_too_wide")));
}

TEST_CASE("load_world: an unreadable plane_legibility tag floor is rejected") {
    const std::string base = slurp(SEADS_CONFIG_DIR "/world.toml");
    REQUIRE(base.find("tag_min_px          = 16.0") != std::string::npos);
    // 4 px is not a size floor, it is a typo: 0 (off) or a readable glyph.
    const std::string bad = replace_all(base, "tag_min_px          = 16.0",
                                        "tag_min_px          = 4.0");
    CHECK_THROWS(cfg::load_world_toml(write_temp(bad, "tag_floor_tiny")));
}

TEST_CASE("load_world: a glint sized on with no blink is rejected") {
    // THE SILENT-DISARM LEG: size > 0 with period 0 compiles, loads, and draws
    // absolutely nothing while every dial reads "on".
    const std::string base = slurp(SEADS_CONFIG_DIR "/world.toml");
    REQUIRE(base.find("glint_period_frames = 48") != std::string::npos);
    const std::string bad = replace_all(base, "glint_period_frames = 48",
                                        "glint_period_frames = 0");
    CHECK_THROWS(cfg::load_world_toml(write_temp(bad, "glint_no_period")));
}

TEST_CASE("load_world: an oversized engagement glint is rejected") {
    const std::string base = slurp(SEADS_CONFIG_DIR "/world.toml");
    REQUIRE(base.find("glint_size_m        = 0.55") != std::string::npos);
    // The bead is a nav light, not a second aircraft (Bf 109 span ~9.9 m).
    const std::string bad = replace_all(base, "glint_size_m        = 0.55",
                                        "glint_size_m        = 7.0");
    CHECK_THROWS(cfg::load_world_toml(write_temp(bad, "glint_too_big")));
}

TEST_CASE("load_world: a missing plane_legibility key throws (strict schema)") {
    // Strict like every other block: a world.toml that predates E4 must fail
    // LOUD, not fly silently on half the dials at struct defaults.
    const std::string base = slurp(SEADS_CONFIG_DIR "/world.toml");
    REQUIRE(base.find("enemy_tint_shift    = 1.0") != std::string::npos);
    const std::string bad =
        replace_all(base, "enemy_tint_shift    = 1.0", "");
    CHECK_THROWS(cfg::load_world_toml(write_temp(bad, "legibility_missing")));
}
