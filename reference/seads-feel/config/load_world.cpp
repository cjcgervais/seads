#include "config/load_world.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <toml++/toml.hpp>

#include "render/pump_frame.h"  // kDeepPumpShellM: the [pump_frame] clearance bound
#include "world/snowhill_geo.gen.h"  // SF1: the baked anchor frame (lock-checked in buildings.cpp)
#include "render/team_color.h"  // is_exact_complement: the [teams] ruling, executable

// Envelope guard (Fable P1, iter-7): estimate dragged TOF for the guard check.
// DELIBERATELY avoids std::expm1 so it does not add a second instance of that
// token (single-source rule: expm1/log1p each appear exactly once in the whole
// tree, in render/gunsight.h). Uses std::exp directly: tof =
// (exp(k*d)-1)/(k*a).
namespace {
// Upper-bound TOF for a round at launch speed a with drag k over distance d
// [s]. k=0: d/a. Uses std::exp (not expm1) to satisfy the single-source grep
// rule.
inline double cfg_tof_bound(double d, double a, double k) {
    if (k <= 0.0 || a <= 0.0) return (a > 0.0) ? d / a : 0.0;
    return (std::exp(k * d) - 1.0) / (k * a);
}
// Maximum flight time (must match render::kBallisticTMax = 10.0 s).
constexpr double kCfgMaxFlightTime = 10.0;
}  // namespace

namespace cfg {

namespace {

constexpr double kPi = 3.14159265358979323846;
double rad(double deg) { return deg * kPi / 180.0; }

double require(const toml::table& root, const char* section, const char* key) {
    const toml::node* node =
        root.at_path(std::string(section) + "." + key).node();
    if (node == nullptr || !node->is_number()) {
        throw std::runtime_error(std::string("world.toml: missing or "
                                             "non-numeric key [") +
                                 section + "] " + key);
    }
    return node->value_or(0.0);
}

// ★ W3: the ONLY boolean [snowpack] key. Mirrors config/load_game.cpp's
// require_bool() (same strict-or-throw shape as require()/require_vec3()
// above, just for toml::node::is_boolean() instead of is_number()).
bool require_bool(const toml::table& root, const char* section,
                  const char* key) {
    const toml::node* node =
        root.at_path(std::string(section) + "." + key).node();
    if (node == nullptr || !node->is_boolean()) {
        throw std::runtime_error(std::string("world.toml: missing or "
                                             "non-boolean key [") +
                                 section + "] " + key);
    }
    return node->value_or(false);
}

void check(bool ok, const char* what) {
    if (!ok) {
        throw std::runtime_error(std::string("world.toml: invalid value: ") +
                                 what);
    }
}

// Parse a 3-element numeric array [x, y, z] (the celestial vectors). Throws on
// a missing key, wrong length, or a non-numeric element — strict like
// require().
glm::dvec3 require_vec3(const toml::table& root, const char* section,
                        const char* key) {
    const toml::node* node =
        root.at_path(std::string(section) + "." + key).node();
    const toml::array* arr = node != nullptr ? node->as_array() : nullptr;
    if (arr == nullptr || arr->size() != 3) {
        throw std::runtime_error(std::string("world.toml: [") + section + "] " +
                                 key + " must be a 3-element array");
    }
    glm::dvec3 v;
    for (int i = 0; i < 3; ++i) {
        const toml::node& e = *arr->get(i);
        if (!e.is_number())
            throw std::runtime_error(std::string("world.toml: [") + section +
                                     "] " + key + " has a non-numeric element");
        v[i] = e.value_or(0.0);
    }
    return v;
}

// Parse a string key strictly (throws on a missing / non-string key), like
// require(). Used by [seasons] static_season.
std::string require_str(const toml::table& root, const char* section,
                        const char* key) {
    const toml::node* node =
        root.at_path(std::string(section) + "." + key).node();
    if (node == nullptr || !node->is_string()) {
        throw std::runtime_error(std::string("world.toml: missing or "
                                             "non-string key [") +
                                 section + "] " + key);
    }
    return node->value_or(std::string{});
}

// Resolve a season name to its index (Winter=0..Autumn=3), kept LOCAL so config
// does not depend on render/ (mirrors render::season_from_string, the header
// lock test pins them equal). Returns -1 on no match.
int season_index(const std::string& s) {
    static const char* kNames[4] = {"winter", "spring", "summer", "autumn"};
    for (int i = 0; i < 4; ++i)
        if (s == kNames[i]) return i;
    return -1;
}

}  // namespace

WorldParams load_world_toml(const std::string& path) {
    toml::table root;
    try {
        root = toml::parse_file(path);
    } catch (const toml::parse_error& e) {
        throw std::runtime_error(std::string("world.toml parse error: ") +
                                 e.what());
    }

    WorldParams w;

    // [sky] was superseded by [atmosphere] in Stage 2. require() throws only on
    // MISSING keys, not unknown ones, so a stale [sky] table would parse
    // silently — reject it explicitly so the migration is strict (P1-2).
    check(!root.contains("sky"),
          "[sky] superseded by [atmosphere] (Stage 2 migration)");
    auto& atm = w.atmosphere;
    atm.sky_space = require(root, "atmosphere", "sky_space");
    atm.sky_band_top_rad = rad(require(root, "atmosphere", "sky_band_top_deg"));
    atm.sky_day = require(root, "atmosphere", "sky_day");
    atm.sky_dusk = require(root, "atmosphere", "sky_dusk");
    atm.sky_night = require(root, "atmosphere", "sky_night");
    atm.dusk_lo_rad = rad(require(root, "atmosphere", "dusk_lo_deg"));
    atm.dusk_hi_rad = rad(require(root, "atmosphere", "dusk_hi_deg"));
    atm.night_fill_min = require(root, "atmosphere", "night_fill_min");
    atm.ground_day_gain = require(root, "atmosphere", "ground_day_gain");
    atm.horizon_definition = require(root, "atmosphere", "horizon_definition");
    atm.dither = require(root, "atmosphere", "dither");
    atm.haze_overcast_density =
        require(root, "atmosphere", "haze_overcast_density");
    atm.haze_scale_m = require(root, "atmosphere", "haze_scale_m");
    // Atmospheric scatter (Stage 3): the only sky color, gated by the haze
    // amount. Tints are RGB vectors (require_vec3, strict like the celestial
    // vectors).
    atm.scatter_strength = require(root, "atmosphere", "scatter_strength");
    atm.mie_g = require(root, "atmosphere", "mie_g");
    atm.rayleigh_tint = require_vec3(root, "atmosphere", "rayleigh_tint");
    atm.mie_tint = require_vec3(root, "atmosphere", "mie_tint");
    // S-sunglare: the Mie forward-halo coefficients (were baked in the GLSL).
    atm.mie_halo_gain = require(root, "atmosphere", "mie_halo_gain");
    atm.mie_dusk_boost = require(root, "atmosphere", "mie_dusk_boost");
    atm.mie_rim_lift = require(root, "atmosphere", "mie_rim_lift");
    // S-airdome (spec §2): the ALWAYS-ON AirField render tuning.
    atm.air_haze_density = require(root, "atmosphere", "air_haze_density");
    atm.air_weather_gain = require(root, "atmosphere", "air_weather_gain");
    atm.tau_scale_m = require(root, "atmosphere", "tau_scale_m");
    atm.march_max_m = require(root, "atmosphere", "march_max_m");
    atm.march_steps_sky =
        static_cast<int>(require(root, "atmosphere", "march_steps_sky"));
    atm.march_steps_ground =
        static_cast<int>(require(root, "atmosphere", "march_steps_ground"));
    atm.aerial_gain = require(root, "atmosphere", "aerial_gain");
    atm.mie_tint_day = require_vec3(root, "atmosphere", "mie_tint_day");

    // [weather] — the Stage-3 weather variable (pure t_cel -> haze[0,1]). Gate
    // and periods; the structural math lives in render::weather_haze.
    auto& wx = w.weather;
    wx.period1_s = require(root, "weather", "period1_s");
    wx.period2_s = require(root, "weather", "period2_s");
    wx.period3_s = require(root, "weather", "period3_s");
    wx.weight1 = require(root, "weather", "weight1");
    wx.weight2 = require(root, "weather", "weight2");
    wx.weight3 = require(root, "weather", "weight3");
    wx.phase1 = require(root, "weather", "phase1");
    wx.phase2 = require(root, "weather", "phase2");
    wx.phase3 = require(root, "weather", "phase3");
    wx.gate_lo = require(root, "weather", "gate_lo");
    wx.gate_hi = require(root, "weather", "gate_hi");

    // [weather_cell] — the LOCALIZED WEATHER FIELD (W2). The [weather] scalar
    // is the storm budget; these place the microsystem cells on the sphere. The
    // structural math lives in render::weather_cell.
    auto& wcx = w.weather_cell;
    wcx.cell_count =
        static_cast<int>(require(root, "weather_cell", "cell_count"));
    wcx.inner_deg = require(root, "weather_cell", "inner_deg");
    wcx.outer_deg = require(root, "weather_cell", "outer_deg");
    wcx.thresh_lo = require(root, "weather_cell", "thresh_lo");
    wcx.thresh_hi = require(root, "weather_cell", "thresh_hi");
    wcx.env_width = require(root, "weather_cell", "env_width");
    // S-bubbleweather: the anchored (in-bubble) lattice's scale.
    wcx.bubble_cell_count =
        static_cast<int>(require(root, "weather_cell", "bubble_cell_count"));
    wcx.bubble_inner_deg = require(root, "weather_cell", "bubble_inner_deg");
    wcx.bubble_outer_deg = require(root, "weather_cell", "bubble_outer_deg");
    wcx.bubble_fill_frac = require(root, "weather_cell", "bubble_fill_frac");

    // [seasons] — W1 weather-season framing. static_season is "random" (draw at
    // spawn) or a season name (winter/spring/summer/autumn) forcing that season
    // for building. The four weights govern the random draw (each >= 0, SUM >
    // 0).
    auto& sx = w.seasons;
    const std::string ss = require_str(root, "seasons", "static_season");
    if (ss == "random") {
        sx.static_season = -1;
    } else {
        sx.static_season = season_index(ss);
        check(sx.static_season >= 0,
              "[seasons] static_season must be "
              "random|winter|spring|summer|autumn");
    }
    sx.weight_winter = require(root, "seasons", "weight_winter");
    sx.weight_spring = require(root, "seasons", "weight_spring");
    sx.weight_summer = require(root, "seasons", "weight_summer");
    sx.weight_autumn = require(root, "seasons", "weight_autumn");
    check(std::isfinite(sx.weight_winter) && std::isfinite(sx.weight_spring) &&
              std::isfinite(sx.weight_summer) &&
              std::isfinite(sx.weight_autumn),
          "[seasons] draw weights must be finite");  // inf/nan -> NaN probs
                                                     // (P2-1)
    check(sx.weight_winter >= 0.0 && sx.weight_spring >= 0.0 &&
              sx.weight_summer >= 0.0 && sx.weight_autumn >= 0.0,
          "[seasons] draw weights must be >= 0");
    check(sx.weight_winter + sx.weight_spring + sx.weight_summer +
                  sx.weight_autumn >
              0.0,
          "[seasons] draw weights must sum to > 0");

    // [precip] — W3 precipitation. Season+weather-gated snow/rain; the
    // structural math lives in render::precip_sample. RAW metres/Hz/[0,1] at
    // the boundary.
    auto& px = w.precip;
    px.enabled = require(root, "precip", "enabled") != 0.0;
    px.cell_size_m = require(root, "precip", "cell_size_m");
    px.box_half_m = require(root, "precip", "box_half_m");
    px.wrap_fade = require(root, "precip", "wrap_fade");
    px.color = require_vec3(root, "precip", "color");
    px.snow_size_m = require(root, "precip", "snow_size_m");
    px.snow_rate_hz = require(root, "precip", "snow_rate_hz");
    px.snow_opacity = require(root, "precip", "snow_opacity");
    px.rain_size_m = require(root, "precip", "rain_size_m");
    px.rain_streak_m = require(root, "precip", "rain_streak_m");
    px.rain_rate_hz = require(root, "precip", "rain_rate_hz");
    px.rain_opacity = require(root, "precip", "rain_opacity");

    w.water.reflectivity = require(root, "water", "reflectivity");
    w.water.sparkle_sharpness = require(root, "water", "sparkle_sharpness");
    w.water.min_lake_m = require(root, "water", "min_lake_m");
    w.water.surface_lift_m = require(root, "water", "surface_lift_m");
    w.water.star_reflect = require(root, "water", "star_reflect");
    w.water.star_density = require(root, "water", "star_density");

    w.ground.relief_scale_m = require(root, "ground", "relief_scale_m");
    w.ground.field_scale_m = require(root, "ground", "field_scale_m");
    w.ground.winter_snow_cover = require(root, "ground", "winter_snow_cover");
    w.ground.snow_albedo = require(root, "ground", "snow_albedo");
    w.ground.snow_slope_lo = require(root, "ground", "snow_slope_lo");
    w.ground.ice_albedo = require(root, "ground", "ice_albedo");
    w.ground.ice_reflect_frac = require(root, "ground", "ice_reflect_frac");
    w.ground.ice_glint_frac = require(root, "ground", "ice_glint_frac");
    w.ground.night_glow = require_vec3(root, "ground", "night_glow");
    w.ground.ice_night_reflect_frac =
        require(root, "ground", "ice_night_reflect_frac");
    w.ground.snow_sparkle = require(root, "ground", "snow_sparkle");
    w.ground.snow_sparkle_sharp =
        require(root, "ground", "snow_sparkle_sharp");
    w.ground.barren_face_dark = require(root, "ground", "barren_face_dark");
    w.ground.barren_face_dark_hi_deg =
        require(root, "ground", "barren_face_dark_hi_deg");
    w.ground.barren_face_mottle =
        require(root, "ground", "barren_face_mottle");
    const double procedural = require(root, "ground", "use_procedural");

    // ★ R5 row 9 — [shadows]. Strict like every other block.
    w.shadows.enabled = require_bool(root, "shadows", "enabled");
    w.shadows.strength = require(root, "shadows", "strength");
    w.shadows.tint = require_vec3(root, "shadows", "tint");
    w.shadows.underground_margin_m =
        require(root, "shadows", "underground_margin_m");

    // ★ [snowpack] -- WINTER_LAW §2.2. Strict like every other block: every key
    // present or it throws. The gains were MEASURED against the shipped DEM
    // (offline_tool/measure_snowpack.py), not guessed.
    {
        world::SnowParams& s = w.snowpack;
        s.base_m = require(root, "snowpack", "base_m");
        s.slope_shed = require(root, "snowpack", "slope_shed");
        s.slope_full_deg = require(root, "snowpack", "slope_full_deg");
        s.curv_gain = require(root, "snowpack", "curv_gain");
        s.curv_probe_m = require(root, "snowpack", "curv_probe_m");
        s.elev_gain_per_km = require(root, "snowpack", "elev_gain_per_km");
        s.elev_ref_m = require(root, "snowpack", "elev_ref_m");
        s.aspect_lee = require(root, "snowpack", "aspect_lee");
        s.wind_dir = require_vec3(root, "snowpack", "wind_dir");
        s.drain_gain = require(root, "snowpack", "drain_gain");
        s.drain_slope_eps = require(root, "snowpack", "drain_slope_eps");
        s.depth_max_m = require(root, "snowpack", "depth_max_m");
        s.k_barren = require(root, "snowpack", "k_barren");
        s.barren_shed_lo = require(root, "snowpack", "barren_shed_lo");
        s.barren_shed_hi = require(root, "snowpack", "barren_shed_hi");
        s.barren_slope_lo_deg = require(root, "snowpack", "barren_slope_lo_deg");
        s.barren_slope_hi_deg = require(root, "snowpack", "barren_slope_hi_deg");
        s.barren_flat_shed_frac =
            require(root, "snowpack", "barren_flat_shed_frac");
        s.ice_snow_m = require(root, "snowpack", "ice_snow_m");
        s.water_class_frac = require(root, "snowpack", "water_class_frac");
        // ★★ S3 / §6b.2: THE ICE THE SLED DRIVES ON IS THE MIRROR MESH. It is
        // NOT a [snowpack] key -- it is [water] surface_lift_m, read a second
        // time into the field that decides the drive surface, so ONE number
        // moves both the mesh and the ground under the machine. Typing it twice
        // is how the sled ends up riding inside the ice it can see.
        s.ice_lift_m = w.water.surface_lift_m;
        s.trail_pack_m = require(root, "snowpack", "trail_pack_m");
        s.corridor_search_m = require(root, "snowpack", "corridor_search_m");
        s.corridor_edge_m = require(root, "snowpack", "corridor_edge_m");
        s.road_bare_m = require(root, "snowpack", "road_bare_m");
        s.class_blend_m = require(root, "snowpack", "class_blend_m");
        s.corner_blend_m = require(root, "snowpack", "corner_blend_m");
        s.bank_height_m = require(root, "snowpack", "bank_height_m");
        s.bank_rise_m = require(root, "snowpack", "bank_rise_m");
        s.bank_fall_m = require(root, "snowpack", "bank_fall_m");
        s.bank_max_grade = require(root, "snowpack", "bank_max_grade");
        s.bank_gap_m = require(root, "snowpack", "bank_gap_m");
        s.barren_class_frac = require(root, "snowpack", "barren_class_frac");
        s.bare_rock_depth_m = require(root, "snowpack", "bare_rock_depth_m");
        // ★ W3 (§PHASE W3, RULED-GI-1): Chad's A/B toggle -- drive the drawn
        // mesh facet instead of the analytic radius base. Default false; the
        // app layer supplies the facet_radius_fn injection separately (world/
        // stays render-free by law, P2-7), this key only gates whether it is
        // read.
        s.hf_faceted_ground = require_bool(root, "snowpack", "hf_faceted_ground");
    }

    // ★ SF1 [snowhill] -- WINTER_LAW §3.6b, the St. Charles snow mountain.
    // The anchor frame is the GENERATED one (offline_tool/sf1_snowhill_place.py
    // -> world/snowhill_geo.gen.h) -- baked through the ONE locked projection,
    // hash-checked against the GIS bake in render/buildings.cpp. The frame is
    // still DATA in the params so probes and tests inject synthetic anchors.
    {
        world::SnowhillParams& sh = w.snowhill;
        sh.enabled = require(root, "snowhill", "enabled") != 0.0;
        sh.up = glm::dvec3(world::kSnowhillDir[0], world::kSnowhillDir[1],
                           world::kSnowhillDir[2]);
        sh.east = glm::dvec3(world::kSnowhillEast[0], world::kSnowhillEast[1],
                             world::kSnowhillEast[2]);
        sh.north = glm::dvec3(world::kSnowhillNorth[0],
                              world::kSnowhillNorth[1],
                              world::kSnowhillNorth[2]);
        sh.r_cut_m = require(root, "snowhill", "r_cut_m");
        sh.feather_m = require(root, "snowhill", "feather_m");
        sh.class_min_m = require(root, "snowhill", "class_min_m");
        sh.pack_cap_m = require(root, "snowhill", "pack_cap_m");
        // peakN = [dx_m, dy_m, h_m, sx_m, sy_m, rot_deg] -- strict 6-arrays.
        const char* peak_keys[world::SnowhillParams::kPeaks] = {"peak1",
                                                               "peak2",
                                                               "peak3"};
        for (int i = 0; i < world::SnowhillParams::kPeaks; ++i) {
            const toml::node* node =
                root.at_path(std::string("snowhill.") + peak_keys[i]).node();
            const toml::array* arr =
                node != nullptr ? node->as_array() : nullptr;
            if (arr == nullptr || arr->size() != 6)
                throw std::runtime_error(
                    std::string("world.toml: [snowhill] ") + peak_keys[i] +
                    " must be a 6-element array [dx, dy, h, sx, sy, rot_deg]");
            double v[6];
            for (int k = 0; k < 6; ++k) {
                const toml::node& e = *arr->get(k);
                if (!e.is_number())
                    throw std::runtime_error(
                        std::string("world.toml: [snowhill] ") + peak_keys[i] +
                        " has a non-numeric element");
                v[k] = e.value_or(0.0);
            }
            sh.peaks[i] = world::SnowhillPeak{v[0], v[1], v[2],
                                              v[3], v[4], v[5]};
        }
    }

    // ★ SF2-BANKS [bank_mesh] -- WINTER_LAW §3.6c. Strict like every block.
    {
        auto& b = w.bank_mesh;
        b.enabled = require(root, "bank_mesh", "enabled") != 0.0;
        b.station_m = require(root, "bank_mesh", "station_m");
        b.skirt_m = require(root, "bank_mesh", "skirt_m");
        b.skirt_bury_m = require(root, "bank_mesh", "skirt_bury_m");
        b.speckle_density = require(root, "bank_mesh", "speckle_density");
        b.speckle_dark = require(root, "bank_mesh", "speckle_dark");
        b.crest_smudge = require(root, "bank_mesh", "crest_smudge");
        b.min_amp_m = require(root, "bank_mesh", "min_amp_m");
        b.skirt_rings =
            static_cast<int>(require(root, "bank_mesh", "skirt_rings"));
        b.chord_tol_m = require(root, "bank_mesh", "chord_tol_m");
        b.junction_station_m =
            require(root, "bank_mesh", "junction_station_m");
        // ★ ROAD-REPAIR ONAPING RUNG 2 -- the drawn apron.
        b.apron_m = require(root, "bank_mesh", "apron_m");
        b.apron_tol_m = require(root, "bank_mesh", "apron_tol_m");
        b.apron_min_drop_m = require(root, "bank_mesh", "apron_min_drop_m");
    }

    // ★ SC1 [cold] -- WINTER_LAW §3.7. Strict like every other block. Mirrors
    // [snowpack]'s loading shape exactly (world::ColdParams IS the block).
    {
        world::ColdParams& c = w.cold;
        c.enabled = require(root, "cold", "enabled") != 0.0;
        c.t_ref_c = require(root, "cold", "t_ref_c");
        c.t_day_max_c = require(root, "cold", "t_day_max_c");
        c.t_night_c = require(root, "cold", "t_night_c");
        c.t_snap_c = require(root, "cold", "t_snap_c");
        c.snap_center = require(root, "cold", "snap_center");
        c.snap_width = require(root, "cold", "snap_width");
        c.cold_gain = require(root, "cold", "cold_gain");
        c.h_slope_per_c = require(root, "cold", "h_slope_per_c");
        c.h_max = require(root, "cold", "h_max");
        c.warm_drag_gain = require(root, "cold", "warm_drag_gain");
    }

    w.planet.subdiv = static_cast<int>(require(root, "planet", "subdiv"));
    w.planet.tiles = static_cast<int>(require(root, "planet", "tiles"));
    w.planet.u_offset = require(root, "planet", "u_offset");
    w.planet.dem_blur_radius =
        static_cast<int>(require(root, "planet", "dem_blur_radius"));
    w.planet.cubemap_size =
        static_cast<int>(require(root, "planet", "cubemap_size"));

    // [trees] — S2 boreal scatter fly-dials (stereoscope-sudbury).
    w.trees.enabled = require(root, "trees", "enabled") != 0.0;
    w.trees.density_gain = require(root, "trees", "density_gain");
    w.trees.min_scale = require(root, "trees", "min_scale");
    w.trees.max_scale = require(root, "trees", "max_scale");
    w.trees.height_m = require(root, "trees", "height_m");
    w.trees.base_width_m = require(root, "trees", "base_width_m");
    w.trees.render_range_m = require(root, "trees", "render_range_m");
    w.trees.ambient = require(root, "trees", "ambient");
    w.trees.diffuse = require(root, "trees", "diffuse");
    w.trees.fade_frac = require(root, "trees", "fade_frac");
    w.trees.corridor_margin_m = require(root, "trees", "corridor_margin_m");
    w.trees.cells_per_face =
        static_cast<int>(require(root, "trees", "cells_per_face"));
    w.trees.chunk_cells =
        static_cast<int>(require(root, "trees", "chunk_cells"));

    // [ribbons] — S3 draped linework (roads / snowmobile trails). Look dials
    // only (geometry is baked); colors RGB [0,1], roads mono, trail the
    // light-green accent.
    w.ribbons.enabled = require(root, "ribbons", "enabled") != 0.0;
    w.ribbons.lift_m = require(root, "ribbons", "lift_m");
    w.ribbons.max_seg_m = require(root, "ribbons", "max_seg_m");
    w.ribbons.max_tr_m = require(root, "ribbons", "max_tr_m");
    w.ribbons.road_bed = require_vec3(root, "ribbons", "road_bed");
    w.ribbons.road_line = require_vec3(root, "ribbons", "road_line");
    w.ribbons.road_center_frac = require(root, "ribbons", "road_center_frac");
    w.ribbons.road_dash_m = require(root, "ribbons", "road_dash_m");
    w.ribbons.road_gap_m = require(root, "ribbons", "road_gap_m");
    w.ribbons.road_mottle = require(root, "ribbons", "road_mottle");
    w.ribbons.road_mottle_frac = require(root, "ribbons", "road_mottle_frac");
    w.ribbons.road_fade = require(root, "ribbons", "road_fade");
    w.ribbons.trail_color = require_vec3(root, "ribbons", "trail_color");
    w.ribbons.trail_mottle = require(root, "ribbons", "trail_mottle");
    w.ribbons.trail_winter_color =
        require_vec3(root, "ribbons", "trail_winter_color");
    w.ribbons.trail_corduroy = require(root, "ribbons", "trail_corduroy");
    w.ribbons.trail_corduroy_m = require(root, "ribbons", "trail_corduroy_m");

    // ★★ W1.2 / §CORRECTIONS 2: the road-deck registration lift. Mirrors the
    // ice_lift_m single-source PATTERN ([snowpack] above, right after
    // [water]), but NOT its application site: deck_lift_m enters
    // world::SnowpackField's GEOMETRY term only (corridor_eval /
    // drive_radius_at), never the reported depth (depth_base_at/depth_at/
    // sample_at), because unlike LakeIce the sinkable corridor classes
    // (TrailMain/TrailTributary/Road) have a real sinkage kernel underneath
    // that reported depth would retune. Assigned HERE, after both [ribbons]
    // and [bank_mesh] have loaded (the [snowpack] block above runs BEFORE
    // either, so setting it there would read their still-default values).
    w.snowpack.deck_lift_m = w.ribbons.lift_m;
    // ★ W1.3: the skirt run the lift ramps out over, beyond the drawn bank
    // ring set on a plowed road. Single-sourced from [bank_mesh] skirt_m so
    // the drawn strip's outer burial ramp and the driven lift's fade-out
    // share one number (render::BankBuildParams::skirt_m mirrors the SAME
    // config key -- see render/bank_mesh.h).
    w.snowpack.deck_skirt_m = w.bank_mesh.skirt_m;

    // [buildings] — S4 massing. Look dials only (footprints/heights/roofs are
    // baked).
    w.buildings.enabled = require(root, "buildings", "enabled") != 0.0;
    w.buildings.wall_val = require(root, "buildings", "wall_val");
    w.buildings.roof_val = require(root, "buildings", "roof_val");
    w.buildings.ambient = require(root, "buildings", "ambient");
    w.buildings.diffuse = require(root, "buildings", "diffuse");
    w.buildings.height_scale = require(root, "buildings", "height_scale");
    w.buildings.window_color = require_vec3(root, "buildings", "window_color");
    w.buildings.window_bright = require(root, "buildings", "window_bright");
    w.buildings.window_lit_frac = require(root, "buildings", "window_lit_frac");

    // [streetlamps] — night point lights along the town streets (baked
    // positions).
    w.streetlamps.enabled = require(root, "streetlamps", "enabled") != 0.0;
    w.streetlamps.color = require_vec3(root, "streetlamps", "color");
    w.streetlamps.brightness = require(root, "streetlamps", "brightness");
    w.streetlamps.core_bright = require(root, "streetlamps", "core_bright");
    w.streetlamps.core_sharp = require(root, "streetlamps", "core_sharp");
    w.streetlamps.size_m = require(root, "streetlamps", "size_m");
    w.streetlamps.min_px = require(root, "streetlamps", "min_px");
    // core_sharp is a gaussian exponent: 0/negative flattens the "bulb" into a
    // full disc (the silent-disarm class), so require it strictly > 0;
    // brightnesses >= 0.
    check(w.streetlamps.core_sharp > 0.0,
          "streetlamps core_sharp > 0 (gaussian tightness)");
    check(w.streetlamps.core_bright >= 0.0, "streetlamps core_bright >= 0");
    check(w.streetlamps.brightness >= 0.0, "streetlamps brightness >= 0");

    // [smoke] — CC1 Superstack plume (Living Copper Cliff). Mono billboard
    // puffs.
    w.smoke.enabled = require(root, "smoke", "enabled") != 0.0;
    w.smoke.puffs = require(root, "smoke", "puffs");
    w.smoke.color = require_vec3(root, "smoke", "color");
    w.smoke.rise_m = require(root, "smoke", "rise_m");
    w.smoke.drift_m = require(root, "smoke", "drift_m");
    w.smoke.r0_m = require(root, "smoke", "r0_m");
    w.smoke.r1_m = require(root, "smoke", "r1_m");
    w.smoke.opacity = require(root, "smoke", "opacity");
    w.smoke.jitter_m = require(root, "smoke", "jitter_m");
    w.smoke.wind = require_vec3(root, "smoke", "wind");
    w.smoke.rate_hz = require(root, "smoke", "rate_hz");
    check(w.smoke.puffs >= 1.0, "smoke puffs >= 1");
    check(w.smoke.r1_m >= w.smoke.r0_m,
          "smoke r1_m >= r0_m (puffs expand rising)");
    check(w.smoke.rate_hz > 0.0, "smoke rate_hz > 0 (plume cycle rate)");

    // [train] — CC2 slag-pot trains (Living Copper Cliff). Mono steel loco +
    // pots.
    w.train.enabled = require(root, "train", "enabled") != 0.0;
    w.train.pots = require(root, "train", "pots");
    w.train.car_gap_m = require(root, "train", "car_gap_m");
    w.train.tip_span_m = require(root, "train", "tip_span_m");
    w.train.lift_m = require(root, "train", "lift_m");
    w.train.color = require_vec3(root, "train", "color");
    w.train.ambient = require(root, "train", "ambient");
    w.train.diffuse = require(root, "train", "diffuse");
    w.train.rate_hz = require(root, "train", "rate_hz");
    check(w.train.pots >= 0.0, "train pots >= 0");
    check(w.train.car_gap_m > 0.0, "train car_gap_m > 0 (car spacing)");
    check(w.train.tip_span_m > 0.0, "train tip_span_m > 0 (pot tip zone)");
    check(w.train.rate_hz > 0.0, "train rate_hz > 0 (loop cycle rate)");

    // [slag] — CC3 slag pour (Living Copper Cliff). Mound + pot mono; lava =
    // chroma.
    w.slag.enabled = require(root, "slag", "enabled") != 0.0;
    w.slag.mound_radius_m = require(root, "slag", "mound_radius_m");
    w.slag.mound_top_r_m = require(root, "slag", "mound_top_r_m");
    w.slag.mound_height_m = require(root, "slag", "mound_height_m");
    w.slag.ridge_length_m = require(root, "slag", "ridge_length_m");
    w.slag.crest_width_m = require(root, "slag", "crest_width_m");
    w.slag.face_angle_deg = require(root, "slag", "face_angle_deg");
    w.slag.benches = require(root, "slag", "benches");
    w.slag.mound_color = require_vec3(root, "slag", "mound_color");
    w.slag.ambient = require(root, "slag", "ambient");
    w.slag.diffuse = require(root, "slag", "diffuse");
    w.slag.rivers = require(root, "slag", "rivers");
    w.slag.river_halfwidth_m = require(root, "slag", "river_halfwidth_m");
    w.slag.glow = require(root, "slag", "glow");
    w.slag.night_boost = require(root, "slag", "night_boost");
    w.slag.pour_rate_hz = require(root, "slag", "pour_rate_hz");
    w.slag.lift_m = require(root, "slag", "lift_m");
    check(w.slag.rivers >= 1.0, "slag rivers >= 1");
    check(w.slag.mound_radius_m > w.slag.mound_top_r_m,
          "slag mound_radius_m > mound_top_r_m (a heap, not a cylinder)");
    check(w.slag.pour_rate_hz > 0.0, "slag pour_rate_hz > 0 (pour cycle rate)");
    check(w.slag.ridge_length_m > w.slag.crest_width_m,
          "slag ridge_length_m > crest_width_m (a ridge, not a pad)");
    check(w.slag.crest_width_m > 0.0, "slag crest_width_m > 0");
    check(w.slag.face_angle_deg > 5.0 && w.slag.face_angle_deg < 80.0,
          "slag face_angle_deg in (5,80) (a slag pour face)");
    check(w.slag.benches >= 1.0 && w.slag.benches <= 5.0,
          "slag benches in [1,5] (1 = legacy planar face)");

    w.fleet_rig.reflectivity = require(root, "fleet_rig", "reflectivity");
    w.fleet_rig.fresnel_power = require(root, "fleet_rig", "fresnel_power");
    w.fleet_rig.aileron_deg = require(root, "fleet_rig", "aileron_deg");
    w.fleet_rig.elevator_deg = require(root, "fleet_rig", "elevator_deg");
    w.fleet_rig.rudder_deg = require(root, "fleet_rig", "rudder_deg");
    w.fleet_rig.gear_deploy_deg = require(root, "fleet_rig", "gear_deploy_deg");
    w.fleet_rig.prop_disc_alpha = require(root, "fleet_rig", "prop_disc_alpha");
    w.fleet_rig.prop_idle_alpha = require(root, "fleet_rig", "prop_idle_alpha");

    // [plane_legibility] — RUNG E4 VISIBILITY. Strict like every other block:
    // the keys are REQUIRED, so a config that predates E4 fails loud instead of
    // silently flying with half the dials at struct defaults.
    {
        auto& lg = w.legibility;
        lg.aircraft_haze_frac =
            require(root, "plane_legibility", "aircraft_haze_frac");
        lg.haze_full_range_m =
            require(root, "plane_legibility", "haze_full_range_m");
        lg.tag_outline_px = require(root, "plane_legibility", "tag_outline_px");
        lg.tag_min_px = require(root, "plane_legibility", "tag_min_px");
        lg.enemy_tint_shift =
            require(root, "plane_legibility", "enemy_tint_shift");
        lg.tag_min_alpha = require(root, "plane_legibility", "tag_min_alpha");
        lg.glint_size_m = require(root, "plane_legibility", "glint_size_m");
        lg.glint_range_m = require(root, "plane_legibility", "glint_range_m");
        lg.glint_min_mrad =
            require(root, "plane_legibility", "glint_min_mrad");
        lg.glint_period_frames = static_cast<int>(std::lround(
            require(root, "plane_legibility", "glint_period_frames")));
        lg.glint_duty = require(root, "plane_legibility", "glint_duty");
    }

    // Gun battery (rig-D guns leg). SCAFFOLD data — loaded + validated now,
    // consumed when firing is wired (weapon/ballistics.*).
    w.guns.convergence_range_m = require(root, "guns", "convergence_range_m");
    w.guns.cannon_speed_mps = require(root, "guns", "cannon_speed_mps");
    w.guns.mg_speed_mps = require(root, "guns", "mg_speed_mps");
    w.guns.cannon_rof_hz = require(root, "guns", "cannon_rof_hz");
    w.guns.mg_rof_hz = require(root, "guns", "mg_rof_hz");
    w.guns.cannon_drag_k = require(root, "guns", "cannon_drag_k_per_m");
    w.guns.flak_damage = require(root, "guns", "flak_damage");
    w.guns.flak_reload_s = require(root, "guns", "flak_reload_s");
    w.guns.flak_selfdestruct_s = require(root, "guns", "flak_selfdestruct_s");
    w.guns.flak_prox_radius_m = require(root, "guns", "flak_prox_radius_m");
    // FLAK TRACER LOOK (Stage B): cosmetic, kept off the signed aircraft keys.
    w.guns.flak_tracer_rgb =
        glm::dvec3(require(root, "guns", "flak_tracer_r"),
                   require(root, "guns", "flak_tracer_g"),
                   require(root, "guns", "flak_tracer_b"));
    w.guns.flak_tracer_base = require(root, "guns", "flak_tracer_base");
    w.guns.flak_tracer_len_mult =
        require(root, "guns", "flak_tracer_len_mult");
    w.guns.flak_tracer_lum_mult =
        require(root, "guns", "flak_tracer_lum_mult");
    w.guns.mg_drag_k = require(root, "guns", "mg_drag_k_per_m");
    w.guns.tracer_lifetime_s = require(root, "guns", "tracer_lifetime_s");
    w.guns.tracer_len_m = require(root, "guns", "tracer_len_m");
    w.guns.tracer_cannon_rgb =
        glm::dvec3(require(root, "guns", "tracer_cannon_r"),
                   require(root, "guns", "tracer_cannon_g"),
                   require(root, "guns", "tracer_cannon_b"));
    w.guns.tracer_mg_rgb = glm::dvec3(require(root, "guns", "tracer_mg_r"),
                                      require(root, "guns", "tracer_mg_g"),
                                      require(root, "guns", "tracer_mg_b"));
    // Tracer comet look dials (iter-7 Task E).
    w.guns.tracer_cannon_base = require(root, "guns", "tracer_cannon_base");
    w.guns.tracer_mg_base = require(root, "guns", "tracer_mg_base");
    w.guns.tracer_r_core_frac = require(root, "guns", "tracer_r_core_frac");
    w.guns.tracer_r_core_min = require(root, "guns", "tracer_r_core_min");
    w.guns.tracer_r_glow_mult = require(root, "guns", "tracer_r_glow_mult");
    w.guns.tracer_glow_lum = require(root, "guns", "tracer_glow_lum");
    w.guns.tracer_glow_alpha = require(root, "guns", "tracer_glow_alpha");
    w.guns.tracer_core_lum = require(root, "guns", "tracer_core_lum");
    w.guns.tracer_core_alpha = require(root, "guns", "tracer_core_alpha");
    w.guns.tracer_head_r_mult = require(root, "guns", "tracer_head_r_mult");
    w.guns.tracer_head_lum = require(root, "guns", "tracer_head_lum");
    w.guns.tracer_head_alpha = require(root, "guns", "tracer_head_alpha");
    w.guns.tracer_slag_dark = require(root, "guns", "tracer_slag_dark");
    w.guns.tracer_slag_period_m = require(root, "guns", "tracer_slag_period_m");
    w.guns.tracer_glow_len_mult = require(root, "guns", "tracer_glow_len_mult");
    w.guns.muzzle_hub = require_vec3(root, "guns", "muzzle_hub");
    w.guns.muzzle_cowl_l = require_vec3(root, "guns", "muzzle_cowl_l");
    w.guns.muzzle_cowl_r = require_vec3(root, "guns", "muzzle_cowl_r");
    w.guns.muzzle_wing_l = require_vec3(root, "guns", "muzzle_wing_l");
    w.guns.muzzle_wing_r = require_vec3(root, "guns", "muzzle_wing_r");
    w.guns.cannon_damage = require(root, "guns", "cannon_damage");
    w.guns.mg_damage = require(root, "guns", "mg_damage");
    // The hub muzzle sits ON the boresight ~2.9 m ahead of the origin; a
    // convergence range below it (or near 0) makes its fire dir a zero-vector /
    // fires it backward. Floor it well clear (Fable P1-3).
    check(w.guns.convergence_range_m >= 50.0, "guns.convergence_range_m >= 50");
    check(w.guns.cannon_speed_mps > 0.0 && w.guns.mg_speed_mps > 0.0,
          "guns muzzle speeds > 0");
    check(w.guns.cannon_rof_hz > 0.0 && w.guns.mg_rof_hz > 0.0,
          "guns rates of fire > 0");
    // Drag constants: non-negative (0 = vacuum, a valid setting). Matching the
    // convergence_range floor style (Fable P1-3).
    check(w.guns.cannon_drag_k >= 0.0, "guns.cannon_drag_k_per_m >= 0");
    check(w.guns.mg_drag_k >= 0.0, "guns.mg_drag_k_per_m >= 0");
    check(w.guns.cannon_damage > w.guns.mg_damage && w.guns.mg_damage > 0.0,
          "guns cannon_damage > mg_damage > 0");
    // P1 envelope guard (Fable P1, iter-7): the harmonized TOF to
    // convergence_range must be ≤ the projectile max flight time
    // (kCfgMaxFlightTime = 10 s) for BOTH gun classes. A convergence range so
    // large (e.g. 2000 m) that the round expires before reaching it makes
    // harmonization_rise return an absurd Δ. The TOF at launch speed v is
    // dragged_tof(conv, v, k); we check ≤ 9 s (a 10% margin below the cap) to
    // leave headroom for the lag correction.
    {
        const double conv = w.guns.convergence_range_m;
        const double tof_cannon =
            cfg_tof_bound(conv, w.guns.cannon_speed_mps, w.guns.cannon_drag_k);
        const double tof_mg =
            cfg_tof_bound(conv, w.guns.mg_speed_mps, w.guns.mg_drag_k);
        const double tof_limit = kCfgMaxFlightTime * 0.90;  // 9 s (10% margin)
        if (tof_cannon > tof_limit || tof_mg > tof_limit) {
            throw std::runtime_error(
                std::string("world.toml: guns.convergence_range_m=") +
                std::to_string(conv) +
                " makes harmonized TOF exceed the projectile flight time "
                "(canon=" +
                std::to_string(tof_cannon) +
                " s, mg=" + std::to_string(tof_mg) +
                " s; limit=" + std::to_string(tof_limit) +
                " s). Reduce convergence_range_m or verify muzzle speeds / "
                "drag.");
        }
    }

    w.tone.contrast = require(root, "tone", "contrast");
    w.tone.lift = require(root, "tone", "lift");
    w.tone.grain = require(root, "tone", "grain");
    // S1 stereoscope post dials (the ONE tone owner; display-space).
    w.tone.split_shadow = require_vec3(root, "tone", "split_shadow");
    w.tone.split_hi = require_vec3(root, "tone", "split_hi");
    w.tone.split_hi_night = require_vec3(root, "tone", "split_hi_night");
    w.tone.sat_c0 = require(root, "tone", "sat_c0");
    w.tone.sat_c1 = require(root, "tone", "sat_c1");
    w.tone.sat_dark = require(root, "tone", "sat_dark");
    w.tone.halation = require(root, "tone", "halation");
    w.tone.halation_threshold = require(root, "tone", "halation_threshold");
    w.tone.halation_tint = require_vec3(root, "tone", "halation_tint");
    w.tone.vignette = require(root, "tone", "vignette");
    w.tone.fxaa = require(root, "tone", "fxaa");

    // [dof] — S6 far-field-only depth of field (stereoscope-sudbury). Far-only;
    // near/mid stays crisp. View-space metres / screen px.
    w.dof.enabled = require(root, "dof", "enabled") != 0.0;
    w.dof.focus_start_m = require(root, "dof", "focus_start_m");
    w.dof.focus_end_m = require(root, "dof", "focus_end_m");
    w.dof.radius_px = require(root, "dof", "radius_px");
    w.dof.sky_coc = require(root, "dof", "sky_coc");

    // [halftone] — S6 "printed card" halftone/dither MODE
    // (stereoscope-sudbury). OFF by default (enabled=0 -> the post pass is
    // bit-exact identity). angle in DEGREES at the config edge (converted to
    // rad); mode 2 ignores it.
    w.halftone.enabled = require(root, "halftone", "enabled") != 0.0;
    w.halftone.style = static_cast<int>(require(root, "halftone", "style"));
    w.halftone.scale_px = require(root, "halftone", "scale_px");
    w.halftone.angle_rad = rad(require(root, "halftone", "angle_deg"));
    w.halftone.soft = require(root, "halftone", "soft");
    w.halftone.ink = require(root, "halftone", "ink");
    w.halftone.grain_mul = require(root, "halftone", "grain_mul");

    w.horizon_cull.margin_rad =
        rad(require(root, "horizon_cull", "margin_deg"));

    const double seed = require(root, "scatter", "seed");
    w.scatter.seed = static_cast<int>(seed);
    w.scatter.furniture_scale = require(root, "scatter", "furniture_scale");

    w.perf.frame_budget_ms = require(root, "perf", "frame_budget_ms");
    const double max_draw = require(root, "perf", "max_draw_calls");
    w.perf.max_draw_calls = static_cast<int>(max_draw);

    auto& cel = w.celestial;
    cel.orbit_normal = require_vec3(root, "celestial", "orbit_normal");
    cel.tilt_deg = require(root, "celestial", "tilt_deg");
    cel.tilt_lean = require_vec3(root, "celestial", "tilt_lean");
    cel.day_period_s = require(root, "celestial", "day_period_s");
    cel.year_period_s = require(root, "celestial", "year_period_s");
    cel.epoch_day_frac = require(root, "celestial", "epoch_day_frac");
    cel.epoch_year_frac = require(root, "celestial", "epoch_year_frac");
    cel.epoch_moon_frac = require(root, "celestial", "epoch_moon_frac");
    cel.sun_angular_diameter_deg =
        require(root, "celestial", "sun_angular_diameter_deg");
    cel.sun_distance_m = require(root, "celestial", "sun_distance_m");
    cel.sun_intensity = require(root, "celestial", "sun_intensity");
    cel.sun_glare_deg = require(root, "celestial", "sun_glare_deg");
    cel.moon_angular_diameter_deg =
        require(root, "celestial", "moon_angular_diameter_deg");
    cel.moon_period_s = require(root, "celestial", "moon_period_s");
    cel.moon_inclination_deg =
        require(root, "celestial", "moon_inclination_deg");
    cel.moon_intensity = require(root, "celestial", "moon_intensity");
    cel.star_brightness = require(root, "celestial", "star_brightness");

    auto& st = w.stars;
    st.mag_limit = require(root, "stars", "mag_limit");
    st.mag_ref = require(root, "stars", "mag_ref");
    st.size_px = require(root, "stars", "size_px");
    st.glare_suppress_deg = require(root, "stars", "glare_suppress_deg");
    st.wash_lum = require(root, "stars", "wash_lum");
    // S-starnight: how much the ALWAYS-ON dome air veils stars (Chad's ruling
    // ships this at 0 — only OVERCAST hides the starfield).
    st.air_extinction = require(root, "stars", "air_extinction");
    st.r_star_m = require(root, "stars", "r_star_m");

    auto& mn = w.moon;
    mn.disc_intensity = require(root, "moon", "disc_intensity");
    mn.ground_gain = require(root, "moon", "ground_gain");
    mn.fill_lo = require(root, "moon", "fill_lo");
    mn.sparkle_sharpness = require(root, "moon", "sparkle_sharpness");

    auto& au = w.aurora;
    au.enabled = require(root, "aurora", "enabled") != 0.0;
    au.intensity = require(root, "aurora", "intensity");
    au.oval_center_rad = rad(require(root, "aurora", "oval_center_deg"));
    au.oval_width_rad = rad(require(root, "aurora", "oval_width_deg"));
    au.curtain_scale = require(root, "aurora", "curtain_scale");
    au.height_m = require(root, "aurora", "height_m");
    au.anim_rate = require(root, "aurora", "anim_rate");
    au.ground_glow = require(root, "aurora", "ground_glow");
    au.haze_suppress = require(root, "aurora", "haze_suppress");
    au.tint_low = require_vec3(root, "aurora", "tint_low");
    au.tint_high = require_vec3(root, "aurora", "tint_high");

    // [map] — the M-KEY tactical chart (S-mapread, Chad 2026-08-09).
    auto& mp = w.map;
    mp.paper = require_vec3(root, "map", "paper");
    mp.ink = require_vec3(root, "map", "ink");
    mp.water = require_vec3(root, "map", "water");
    mp.basemap_ink = require(root, "map", "basemap_ink");
    mp.lake_label_span_m = require(root, "map", "lake_label_span_m");
    mp.trails_visible = require(root, "map", "trails_visible") != 0.0;
    mp.minor_roads_visible = require(root, "map", "minor_roads_visible") != 0.0;
    mp.rivers_visible = require(root, "map", "rivers_visible") != 0.0;
    mp.zone_labels_visible = require(root, "map", "zone_labels_visible") != 0.0;
    mp.place_labels_visible =
        require(root, "map", "place_labels_visible") != 0.0;
    mp.neutral_color = require_vec3(root, "map", "neutral_color");
    mp.outline_color = require_vec3(root, "map", "outline_color");
    mp.marker_px = require(root, "map", "marker_px");
    mp.objective_px = require(root, "map", "objective_px");
    mp.player_px = require(root, "map", "player_px");
    mp.outline_px = require(root, "map", "outline_px");
    mp.territory_fill = require(root, "map", "territory_fill");
    mp.territory_outline_px = require(root, "map", "territory_outline_px");
    mp.arrow_hold_sin = require(root, "map", "arrow_hold_sin");
    // L4 -- the view + the per-zoom declutter.
    mp.zoom_step = require(root, "map", "zoom_step");
    mp.zoom_max = require(root, "map", "zoom_max");
    mp.follow_zoom = require(root, "map", "follow_zoom");
    mp.trails_zoom = require(root, "map", "trails_zoom");
    mp.minor_roads_zoom = require(root, "map", "minor_roads_zoom");
    mp.place_labels_zoom = require(root, "map", "place_labels_zoom");

    // [teams] — the ONE faction palette (S-mapteam, Chad 2026-08-09). Read
    // AFTER [map] because the map layer no longer owns a team hue at all.
    w.teams.ally = require_vec3(root, "teams", "ally");
    w.teams.enemy = require_vec3(root, "teams", "enemy");
    w.teams.objective = require_vec3(root, "teams", "objective");

    // [pump_frame] — the neon team wire cube (S-pumpcube, Chad 2026-08-09).
    // Geometry + strength only; the HUES come from [teams] above.
    auto& pf = w.pump_frame;
    pf.enabled = require(root, "pump_frame", "enabled") != 0.0;
    pf.deep_only = require(root, "pump_frame", "deep_only") != 0.0;
    pf.half_extent_m = require(root, "pump_frame", "half_extent_m");
    pf.layers =
        static_cast<int>(std::lround(require(root, "pump_frame", "layers")));
    pf.layer_step_m = require(root, "pump_frame", "layer_step_m");
    pf.brightness = require(root, "pump_frame", "brightness");
    pf.neutral_color = require_vec3(root, "pump_frame", "neutral_color");

    // ---- sanity ---------------------------------------------------------
    // Luminance/strength fractions are physical fractions of the value range.
    check(atm.sky_day >= 0.0 && atm.sky_day <= 1.0,
          "atmosphere sky_day in [0,1]");
    check(atm.sky_dusk >= 0.0 && atm.sky_dusk <= 1.0,
          "atmosphere sky_dusk in [0,1]");
    check(atm.sky_night >= 0.0 && atm.sky_night <= 1.0,
          "atmosphere sky_night in [0,1]");
    check(atm.sky_space >= 0.0 && atm.sky_space <= 1.0,
          "atmosphere sky_space in [0,1]");
    // Monotone tonal ladder space <= night <= dusk <= day. space is the darkest
    // backdrop (overhead), so a space brighter than the night band would light
    // the zenith above the horizon at night — the space-first model inverted.
    check(atm.sky_space <= atm.sky_night && atm.sky_night <= atm.sky_dusk &&
              atm.sky_dusk <= atm.sky_day,
          "atmosphere sky_space <= sky_night <= sky_dusk <= sky_day");
    // The atmosphere band must have positive angular thickness (a thin horizon
    // rim); a non-positive top collapses the whole sky to space.
    check(atm.sky_band_top_rad > 0.0, "atmosphere sky_band_top_deg > 0");
    check(atm.dusk_lo_rad < atm.dusk_hi_rad,
          "atmosphere dusk_lo_deg < dusk_hi_deg");
    check(atm.night_fill_min >= 0.0 && atm.night_fill_min <= 1.0,
          "atmosphere night_fill_min in [0,1]");
    // Ground diffuse gain: >= 0; <= 2 is a loose sanity cap (a gain that far
    // overdrives the albedo blows the B&W terrain to white — not a hard law,
    // but a value beyond it is a typo, not a tune).
    check(atm.ground_day_gain >= 0.0 && atm.ground_day_gain <= 2.0,
          "atmosphere ground_day_gain in [0,2]");
    check(atm.horizon_definition >= 0.0 && atm.horizon_definition <= 1.0,
          "atmosphere horizon_definition in [0,1]");
    check(atm.dither >= 0.0 && atm.dither <= 1.0, "atmosphere dither in [0,1]");
    check(atm.haze_overcast_density >= 0.0 && atm.haze_overcast_density <= 1.0,
          "atmosphere haze_overcast_density in [0,1]");
    check(atm.haze_scale_m > 0.0, "atmosphere haze_scale_m > 0");
    // Scatter: strength in [0, 4] (0 = off; the UPPER cap matters — the ambient
    // (ex-halo) sky tint scales with tint*strength, so an unbounded strength
    // lets a legal config wash the WHOLE overcast sky past clip and drown
    // silhouettes, AFTER-red-team P1; 4 is a loose typo cap, the default 1.0
    // sits well inside). mie_g strictly < 1 (the Henyey-Greenstein forward peak
    // (1-g^2)/(1-g)^3 diverges as g->1); cap 0.95 so a fat-fingered g can't
    // blow the halo to a NaN-adjacent spike. Tint components in [0, 2] (a tint
    // past 2 is a typo, not a tune — the defaults max at 1.0; the product
    // tint*strength is what clips).
    check(atm.scatter_strength >= 0.0 && atm.scatter_strength <= 4.0,
          "atmosphere scatter_strength in [0,4]");
    check(atm.mie_g >= 0.0 && atm.mie_g < 0.95,
          "atmosphere mie_g in [0, 0.95)");
    for (int i = 0; i < 3; ++i) {
        check(atm.rayleigh_tint[i] >= 0.0 && atm.rayleigh_tint[i] <= 2.0,
              "atmosphere rayleigh_tint components in [0,2]");
        check(atm.mie_tint[i] >= 0.0 && atm.mie_tint[i] <= 2.0,
              "atmosphere mie_tint components in [0,2]");
        check(atm.mie_tint_day[i] >= 0.0 && atm.mie_tint_day[i] <= 4.0,
              "atmosphere mie_tint_day components in [0,4]");
    }
    // S-sunglare: halo coefficients non-negative and bounded. The caps are
    // generous (these are look dials) but a negative gain would SUBTRACT light
    // toward the sun, which is not a dimmer — it is a black hole in the sky.
    check(atm.mie_halo_gain >= 0.0 && atm.mie_halo_gain <= 4.0,
          "atmosphere mie_halo_gain in [0,4]");
    check(atm.mie_dusk_boost >= 0.0 && atm.mie_dusk_boost <= 8.0,
          "atmosphere mie_dusk_boost in [0,8]");
    check(atm.mie_rim_lift >= 0.0 && atm.mie_rim_lift <= 2.0,
          "atmosphere mie_rim_lift in [0,2]");
    // S-starnight: an air-extinction fraction, so strictly [0,1].
    check(w.stars.air_extinction >= 0.0 && w.stars.air_extinction <= 1.0,
          "stars air_extinction in [0,1]");
    // S-airdome (spec §2, house style: steps in [2,32], tau/march > 0, gains
    // >= 0, tints in [0,4]).
    check(atm.air_haze_density >= 0.0, "atmosphere air_haze_density >= 0");
    check(atm.air_weather_gain >= 0.0, "atmosphere air_weather_gain >= 0");
    check(atm.tau_scale_m > 0.0, "atmosphere tau_scale_m > 0");
    check(atm.march_max_m > 0.0, "atmosphere march_max_m > 0");
    check(atm.march_steps_sky >= 2 && atm.march_steps_sky <= 32,
          "atmosphere march_steps_sky in [2,32]");
    check(atm.march_steps_ground >= 2 && atm.march_steps_ground <= 32,
          "atmosphere march_steps_ground in [2,32]");
    check(atm.aerial_gain >= 0.0, "atmosphere aerial_gain >= 0");

    // [weather] ranges. Periods > 0 (a zero period is a divide-by-zero in the
    // sine). Weights: non-negative and SUM to 1 (they set the [-1,1] support
    // the gate is calibrated against — a different sum silently moves the whole
    // distribution). Gate: lo < hi (a non-positive span collapses the gate) and
    // lo strictly inside the support so a clear plateau exists (lo >= 1 =>
    // always clear; lo <= -1 => the gate never zeros => haze never fully
    // clears).
    check(wx.period1_s > 0.0 && wx.period2_s > 0.0 && wx.period3_s > 0.0,
          "weather periods > 0");
    check(wx.weight1 >= 0.0 && wx.weight2 >= 0.0 && wx.weight3 >= 0.0,
          "weather weights >= 0");
    check(std::abs((wx.weight1 + wx.weight2 + wx.weight3) - 1.0) < 1e-9,
          "weather weights sum to 1 (they set the signal support)");
    check(wx.gate_lo < wx.gate_hi, "weather gate_lo < gate_hi");
    check(wx.gate_lo > -1.0 && wx.gate_lo < 1.0,
          "weather gate_lo in (-1,1) (a clear plateau must exist)");

    // [weather_cell] ranges. cell_count >= 1 (0 => an empty field, no weather
    // ever). inner < outer (a non-positive falloff span collapses the dot-space
    // smoothstep). Thresholds strictly inside (0,1) and lo < hi (thresh at 0
    // would activate a cell whenever budget > 0, defeating "rare"; at 1 it
    // never activates). env_width > 0 (a zero span is a hard step, not the C1
    // fade-in).
    check(wcx.cell_count >= 1, "weather_cell cell_count >= 1");
    check(wcx.inner_deg >= 0.0 && wcx.inner_deg < wcx.outer_deg,
          "weather_cell 0 <= inner_deg < outer_deg");
    check(wcx.outer_deg <= 180.0, "weather_cell outer_deg <= 180");
    check(wcx.thresh_lo > 0.0 && wcx.thresh_lo < wcx.thresh_hi &&
              wcx.thresh_hi < 1.0,
          "weather_cell thresholds: 0 < thresh_lo < thresh_hi < 1");
    check(wcx.env_width > 0.0, "weather_cell env_width > 0 (C1 fade-in)");
    // S-bubbleweather: same shape of bounds for the anchored lattice.
    // fill_frac in (0,1] — 0 would collapse every cell in a dome onto its
    // centre; >1 would push squalls out past the dome rim into the vacuum the
    // air gate then zeroes, wasting the budget the anchoring exists to save.
    check(wcx.bubble_cell_count >= 1, "weather_cell bubble_cell_count >= 1");
    check(wcx.bubble_inner_deg >= 0.0 &&
              wcx.bubble_inner_deg < wcx.bubble_outer_deg,
          "weather_cell 0 <= bubble_inner_deg < bubble_outer_deg");
    check(wcx.bubble_outer_deg <= 180.0,
          "weather_cell bubble_outer_deg <= 180");
    check(wcx.bubble_fill_frac > 0.0 && wcx.bubble_fill_frac <= 1.0,
          "weather_cell bubble_fill_frac in (0,1]");

    // [precip] ranges. cell_size/box_half > 0 (a zero grid/box degenerates the
    // lattice); box_half >= cell_size so the box holds at least one cell each
    // way. wrap_fade in [0,0.5) (>=0.5 the two wrap fades overlap and null the
    // flake). sizes > 0; rates >= 0 (0 = a static, non-falling field, still
    // valid); opacities in [0,1].
    check(px.cell_size_m > 0.0, "precip cell_size_m > 0");
    check(px.box_half_m >= px.cell_size_m,
          "precip box_half_m >= cell_size_m (>= one cell each way)");
    check(px.wrap_fade >= 0.0 && px.wrap_fade < 0.5,
          "precip wrap_fade in [0, 0.5)");
    check(
        px.snow_size_m > 0.0 && px.rain_size_m > 0.0 && px.rain_streak_m > 0.0,
        "precip flake/streak sizes > 0");
    check(px.snow_rate_hz >= 0.0 && px.rain_rate_hz >= 0.0,
          "precip fall rates >= 0");
    check(px.snow_opacity >= 0.0 && px.snow_opacity <= 1.0 &&
              px.rain_opacity >= 0.0 && px.rain_opacity <= 1.0,
          "precip opacities in [0,1]");

    check(w.water.reflectivity >= 0.0 && w.water.reflectivity <= 1.0,
          "water reflectivity in [0,1]");
    check(w.water.sparkle_sharpness > 0.0, "water sparkle_sharpness > 0");
    // A lake below the landmask resolution floor (~46 m/texel at 2048) renders
    // as a gray smudge, not a silver beacon (Fable water trap 3).
    check(w.water.min_lake_m > 0.0, "water min_lake_m > 0");
    check(w.water.surface_lift_m > 0.0, "water surface_lift_m > 0");
    check(w.water.star_reflect >= 0.0 && w.water.star_reflect <= 1.0,
          "water star_reflect in [0,1]");
    check(w.water.star_density > 0.0, "water star_density > 0");

    // [ribbons] S3: a positive lift (a ribbon at/below the terrain z-fights)
    // and a positive dash period per kind (period=dash+gap feeds
    // fract(s/period)).
    check(w.ribbons.lift_m > 0.0, "ribbons lift_m > 0");
    check(w.ribbons.max_seg_m >= 0.0, "ribbons max_seg_m >= 0 (0 = identity)");
    check(w.ribbons.max_tr_m >= 0.0, "ribbons max_tr_m >= 0 (0 = identity)");
    check(w.ribbons.road_center_frac > 0.0 && w.ribbons.road_center_frac <= 1.0,
          "ribbons road_center_frac in (0,1]");
    check(w.ribbons.road_dash_m + w.ribbons.road_gap_m > 0.0,
          "ribbons road dash+gap > 0");
    check(
        w.ribbons.road_mottle_frac >= 0.0 && w.ribbons.road_mottle_frac <= 1.0,
        "ribbons road_mottle_frac in [0,1]");
    check(w.ribbons.trail_mottle >= 0.0 && w.ribbons.trail_mottle <= 1.0,
          "ribbons trail_mottle in [0,1]");
    check(w.ribbons.trail_corduroy >= 0.0 && w.ribbons.trail_corduroy <= 1.0,
          "ribbons trail_corduroy in [0,1]");
    check(w.ribbons.trail_corduroy_m > 0.0,
          "ribbons trail_corduroy_m must be > 0");
    // S1 / RT-1: the groomed trail must stay BELOW the wild snowpack value.
    // A trail brighter than the snow around it is LESS legible than the clay
    // it replaced -- a regression that looks like the fix. Packed snow really
    // is darker and bluer than fresh cover, so physics and legibility agree.
    check(w.ribbons.trail_winter_color.b >= w.ribbons.trail_winter_color.r,
          "ribbons trail_winter_color must be blue-dominant (packed snow)");
    check(w.ribbons.trail_winter_color.g < w.ground.snow_albedo,
          "ribbons trail_winter_color must be dimmer than ground snow_albedo "
          "(a white trail on white snow is invisible)");

    check(w.ground.relief_scale_m > 0.0, "ground relief_scale_m > 0");
    check(w.ground.field_scale_m > 0.0, "ground field_scale_m > 0");
    // W4 winter snow-cover: amount + target albedo in [0,1]; slope threshold in
    // [0,1) (a dot below which slopes shed snow; >=1 would shed everywhere).
    check(
        w.ground.winter_snow_cover >= 0.0 && w.ground.winter_snow_cover <= 1.0,
        "ground winter_snow_cover in [0,1]");
    check(w.ground.snow_albedo >= 0.0 && w.ground.snow_albedo <= 1.0,
          "ground snow_albedo in [0,1]");
    check(w.ground.snow_slope_lo >= 0.0 && w.ground.snow_slope_lo < 1.0,
          "ground snow_slope_lo in [0,1)");
    check(w.ground.ice_albedo >= 0.0 && w.ground.ice_albedo <= 1.0,
          "ground ice_albedo in [0,1]");
    check(w.ground.ice_reflect_frac >= 0.0 && w.ground.ice_reflect_frac <= 1.0,
          "ground ice_reflect_frac in [0,1]");
    check(w.ground.ice_glint_frac >= 0.0 && w.ground.ice_glint_frac <= 1.0,
          "ground ice_glint_frac in [0,1]");
    // S1b: the night lift must EXIST (snow that goes black reads as melted --
    // the bug) but must not exceed the day range (a glowing snowfield).
    check(w.ground.night_glow.r > 0.0 && w.ground.night_glow.g > 0.0 &&
              w.ground.night_glow.b > 0.0,
          "ground night_glow channels must be > 0 -- zero makes winter nights "
          "read as summer (snow goes dark and looks melted)");
    // ★ COOL, and enforced. Two independent reasons point the same way:
    // (1) moon/starlight really is cooler than sunlight, so snow under a moon
    //     reads blue-white -- this is the physically right colour;
    // (2) the post split-tone deliberately WARMS highlights
    //     (split_hi = [1.07,1.01,0.90], the flown silver look), so a NEUTRAL
    //     night light comes out CREAM on bright snow. Measured at R/B = 1.13
    //     before this, against Chad's "snow needs to stay WHITE".
    // The post pass is a flown, game-wide art choice and is NOT touched; the
    // night light is pre-compensated instead.
    check(w.ground.night_glow.b > w.ground.night_glow.r,
          "ground night_glow must be COOL (b > r) -- a neutral or warm night "
          "light reads as CREAM snow once the post split-tone warms highlights");
    check(w.ground.ice_night_reflect_frac >= 0.0 &&
              w.ground.ice_night_reflect_frac <= 1.0,
          "ground ice_night_reflect_frac in [0,1]");
    // Snow sparkle: amplitude above 1 blows out the clamp budget in the
    // shader term (snowSparkle is clamped [0,1] before the uSnowSparkle
    // multiply, so anything above 1 only overdrives with no visible effect --
    // reject it here instead of shipping a dead dial). A low exponent is a
    // broad sheen, not sparkle, and defeats the anti-moire fade (the
    // fwidth-based LOD term needs a steep enough falloff to die before it
    // aliases at range).
    check(w.ground.snow_sparkle >= 0.0 && w.ground.snow_sparkle <= 1.0,
          "ground snow_sparkle in [0,1] -- amplitude above 1 blows out the "
          "clamp budget");
    check(w.ground.snow_sparkle_sharp >= 8.0,
          "ground snow_sparkle_sharp >= 8 -- a low exponent is a broad sheen, "
          "not sparkle, and defeats the anti-moire fade");
    // ★ FACE-DARK (Chad's fly 3, 2026-08-11): steepest barren faces darken
    // the albedo past full shed. The ramp STARTS at the shed law's
    // barren_slope_hi_deg, so its end must lie beyond it or the two curves
    // overlap into a step -- a decal edge on every barren face.
    check(w.ground.barren_face_dark >= 0.0 && w.ground.barren_face_dark <= 1.0,
          "ground barren_face_dark in [0,1] -- it multiplies the albedo");
    check(w.ground.barren_face_dark_hi_deg > w.snowpack.barren_slope_hi_deg &&
              w.ground.barren_face_dark_hi_deg <= 90.0,
          "ground barren_face_dark_hi_deg must lie in "
          "(snowpack barren_slope_hi_deg, 90] -- the face-dark ramp starts "
          "where the shed saturates, so an earlier end is a step, not a "
          "ramp");
    check(w.ground.barren_face_mottle >= 0.0 &&
              w.ground.barren_face_mottle <= 1.0,
          "ground barren_face_mottle in [0,1] -- above 1 the bright mottle "
          "patches would BRIGHTEN under face-dark instead of resisting it");
    // S1 / §6b.2: ice MUST stay darker than land snow or the shoreline
    // disappears -- and the shoreline is what tells a sled it is over water.
    check(w.ground.ice_albedo < w.ground.snow_albedo,
          "ground ice_albedo must be < snow_albedo (the shore must read)");
    // ★ R5 row 9 — [shadows]. strength is a fraction of the direct-sun term
    // (the shader clamps its product with the SEADS_SHADOWS dial to [0,1]);
    // the tint is the packed-snow ruling AGAIN (ribbons.h:37-47 / S1_SPEC
    // RT-1): darkened snow is darker AND bluer, so a neutral-or-warm shadow
    // tint is rejected executably, like night_glow above. It must also
    // genuinely darken — a tint at 1.0 is a dead dial.
    check(w.shadows.strength >= 0.0 && w.shadows.strength <= 1.0,
          "shadows strength in [0,1] -- it is the occluded fraction of the "
          "direct-sun term");
    check(w.shadows.tint.r >= 0.0 && w.shadows.tint.g >= 0.0 &&
              w.shadows.tint.b >= 0.0 && w.shadows.tint.r < 1.0 &&
              w.shadows.tint.g < 1.0 && w.shadows.tint.b < 1.0,
          "shadows tint components in [0,1) -- it multiplies the direct sun "
          "inside the shadow and must darken");
    check(w.shadows.tint.b > w.shadows.tint.r,
          "shadows tint must be COOL (b > r) -- a snow shadow is lit by blue "
          "sky (the ribbons.h packed-snow ruling; never neutral grey)");
    check(w.shadows.underground_margin_m > 0.0,
          "shadows underground_margin_m > 0 -- the fence-5 altitude gate "
          "needs slack for suspension sink, and 0 would flicker the sled's "
          "own shadow at every contact");
    // ★ [snowpack] -- WINTER_LAW §2.2/§2.4c. These are not range hygiene; each
    // one is a law that a retune could otherwise break silently.
    {
        const world::SnowParams& s = w.snowpack;
        check(s.base_m >= 0.0 && s.base_m <= 5.0, "snowpack base_m in [0,5]");
        check(s.slope_shed >= 0.0 && s.slope_shed <= 1.0,
              "snowpack slope_shed in [0,1]");
        check(s.slope_full_deg > 0.0 && s.slope_full_deg <= 90.0,
              "snowpack slope_full_deg in (0,90]");
        check(s.curv_probe_m > 0.0, "snowpack curv_probe_m > 0");
        check(s.drain_slope_eps > 0.0,
              "snowpack drain_slope_eps > 0 -- it is what keeps the drainage "
              "quotient finite on flat ground");
        check(s.depth_max_m > 0.0, "snowpack depth_max_m > 0");
        check(glm::length(s.wind_dir) > 1e-6,
              "snowpack wind_dir must be a non-zero direction");
        // §6c.1: the black-rock shed is a FRACTION of our depth. Above 1 it
        // would drive depth negative and invert the exposure the whole
        // black-rock layer keys off.
        check(s.k_barren >= 0.0 && s.k_barren <= 1.0,
              "snowpack k_barren in [0,1] (it sheds a FRACTION of depth)");
        // ★ The shed curve must stay ORDERED and inside the field's range. An
        // inverted pair (hi <= lo) would make the response a hard step at `lo`
        // -- a decal edge on the exact layer whose whole design is a continuous
        // feather -- and thresholds outside [0,1] would either shed nothing or
        // shed everywhere, both of which read as "the fix did nothing".
        check(s.barren_shed_lo >= 0.0 && s.barren_shed_lo < s.barren_shed_hi &&
                  s.barren_shed_hi <= 1.0,
              "snowpack barren_shed_lo < barren_shed_hi, both in [0,1]");
        // ★ THE SLOPE GATE (Chad's fly ruling 2026-08-11). Same reasoning as
        // the shed curve just above: an inverted or collapsed ramp makes the
        // gate a step, which is a decal edge on every hillside instead of the
        // continuous feather the whole layer exists to produce.
        check(s.barren_flat_shed_frac >= 0.0 && s.barren_flat_shed_frac <= 1.0,
              "snowpack barren_flat_shed_frac in [0,1] -- it is the gate's "
              "floor on dead-flat ground");
        check(s.barren_slope_lo_deg > 0.0 &&
                  s.barren_slope_lo_deg < s.barren_slope_hi_deg &&
                  s.barren_slope_hi_deg <= 90.0,
              "snowpack barren_slope_lo_deg < barren_slope_hi_deg, both in "
              "(0,90] -- an inverted or collapsed ramp makes the gate a step, "
              "which is a decal edge on every hillside");
        // ★ INV-8: the class threshold reads a FRACTION. Pinned strictly inside
        // (0,1) so nobody can restore the `mask > 0` bug by writing 0.0 here --
        // that would classify the entire one-texel shore feather as lake ice.
        check(s.water_class_frac > 0.0 && s.water_class_frac < 1.0,
              "snowpack water_class_frac strictly in (0,1) -- INV-8: the "
              "landmask is FRACTIONAL, and 0 restores the half-water shore band");
        check(s.corridor_search_m > 0.0, "snowpack corridor_search_m > 0");
        // ★★ S3: the drive surface over water and the visible ice mesh are ONE
        // number. Asserted at load rather than trusted, because the failure is
        // silent -- the machine simply rides below the ice, everywhere, and no
        // seam test can see a constant offset.
        check(s.ice_lift_m == w.water.surface_lift_m,
              "snowpack ice_lift_m must BE [water] surface_lift_m -- the sled "
              "drives on the lake MIRROR MESH, not on the flattened lakebed");
        // ★ W1.2: same single-source discipline as ice_lift_m just above --
        // the road-deck registration lift must BE [ribbons] lift_m, so the
        // drawn drape and the driven corridor radius register at one number.
        check(s.deck_lift_m == w.ribbons.lift_m,
              "snowpack deck_lift_m must BE [ribbons] lift_m -- the sled's "
              "corridor GEOMETRY term and the drawn ribbon drape must "
              "register, or the sled rides above/below what it sees");
        // ★ W1.3: the skirt run must BE [bank_mesh] skirt_m, so the driven
        // lift's fade-out matches the drawn bank strip's outer burial ramp.
        check(s.deck_skirt_m == w.bank_mesh.skirt_m,
              "snowpack deck_skirt_m must BE [bank_mesh] skirt_m -- the "
              "driven lift's skirt fade must match the drawn bank strip's");
        check(s.deck_lift_m >= 0.0, "snowpack deck_lift_m >= 0");
        check(s.deck_skirt_m > 0.0,
              "snowpack deck_skirt_m > 0 -- a zero-width skirt is a STEP in "
              "the driven geometry (same §2.4c rule as corridor_edge_m)");
        // ★ §2.4c: a depth STEP stops a sled dead or breaks the contact solver.
        // A zero-width feather or a zero-width bank face IS that step.
        check(s.corridor_edge_m > 0.0,
              "snowpack corridor_edge_m > 0 -- a zero-width feather is a STEP "
              "at the corridor edge, and a step stops the machine dead");
        check(s.bank_rise_m > 0.0 && s.bank_fall_m > 0.0,
              "snowpack bank_rise_m / bank_fall_m > 0 -- the bank must be a "
              "rideable RAMP, never a step (§2.4c)");
        check(s.class_blend_m >= 0.0, "snowpack class_blend_m >= 0");
        check(s.corner_blend_m >= 0.0, "snowpack corner_blend_m >= 0");
        // The blend band may never reach the corridor search
        // radius: past that the query has no candidates left to
        // blend with and the band would be a lie.
        check(s.corner_blend_m < s.corridor_search_m,
              "snowpack corner_blend_m < corridor_search_m");
        check(s.bank_height_m >= 0.0, "snowpack bank_height_m >= 0");
        check(s.bank_max_grade > 0.0, "snowpack bank_max_grade > 0");
        check(s.bank_gap_m > 0.0,
              "snowpack bank_gap_m > 0 -- the banks are BROKEN at junctions, "
              "so a road crossing is a decision and not a wall");
        // The ramp bound itself, checked at LOAD rather than only in a test:
        // a smoothstep's peak gradient is exactly 1.5*height/width, so this is
        // an exact statement about the shipped profile, not a sample of it.
        const double rise_grade = 1.5 * s.bank_height_m / s.bank_rise_m;
        const double fall_grade = 1.5 * s.bank_height_m / s.bank_fall_m;
        check(rise_grade <= s.bank_max_grade && fall_grade <= s.bank_max_grade,
              "snowpack bank faces exceed bank_max_grade -- widen bank_rise_m/"
              "bank_fall_m or lower bank_height_m (§2.4c: RAMP, not step)");
        check(s.trail_pack_m >= 0.0 && s.road_bare_m >= 0.0,
              "snowpack trail_pack_m / road_bare_m >= 0");
        check(s.barren_class_frac > 0.0 && s.barren_class_frac < 1.0,
              "snowpack barren_class_frac strictly in (0,1)");
        check(s.bare_rock_depth_m >= 0.0, "snowpack bare_rock_depth_m >= 0");
    }
    // ★ SF1 [snowhill] guards. The peaks must stay a RIDEABLE mountain: a
    // sigma under 2 m at ~6 m of height is a wall, not a snowform (the exact
    // grade bound is asserted by test_snowhill's sampled leg; these are the
    // load-time sanity rails in the [snowpack] style).
    {
        const world::SnowhillParams& sh = w.snowhill;
        check(sh.r_cut_m > 0.0 && sh.feather_m > 0.0 &&
                  sh.feather_m < sh.r_cut_m,
              "snowhill 0 < feather_m < r_cut_m -- the window must reach "
              "exactly zero inside the footprint");
        check(sh.class_min_m > 0.0 && sh.pack_cap_m > 0.0,
              "snowhill class_min_m / pack_cap_m > 0");
        for (const world::SnowhillPeak& pk : sh.peaks) {
            check(pk.h_m > 0.0 && pk.h_m <= 8.0,
                  "snowhill peak h_m in (0, 8] -- Chad's 20 ft mountain, not "
                  "a cliff");
            check(pk.sx_m >= 2.0 && pk.sy_m >= 2.0,
                  "snowhill peak sigmas >= 2 m -- a narrower gaussian at this "
                  "height is a wall, not a rideable snowform");
        }
    }
    // ★ SF2-BANKS rails.
    {
        const auto& b = w.bank_mesh;
        check(b.station_m > 0.0, "bank_mesh station_m > 0");
        check(b.skirt_m > 0.0 && b.skirt_bury_m > 0.0,
              "bank_mesh skirt_m / skirt_bury_m > 0 -- the skirt is a BURIAL "
              "ramp, not a floating terrace edge (SF2 red-team F3)");
        check(b.speckle_density >= 0.0 && b.speckle_density <= 1.0 &&
                  b.speckle_dark >= 0.0 && b.speckle_dark <= 1.0,
              "bank_mesh speckle dials in [0,1]");
        check(b.crest_smudge >= 0.0, "bank_mesh crest_smudge >= 0");
        check(b.min_amp_m >= 0.0,
              "bank_mesh min_amp_m >= 0 -- the clear-the-intersections "
              "threshold (0 disables the skip)");
        // ★ ROAD-REPAIR. skirt_rings 1 is the identity (the single skirt
        // quad the mesh shipped with); the upper bound is the u16 index type,
        // which the builder also clamps stations against.
        check(b.skirt_rings >= 1 && b.skirt_rings <= 8,
              "bank_mesh skirt_rings in [1,8] -- 1 is the pre-repair single "
              "skirt quad (the crease), 8 is the u16 vertex budget");
        // ★ ROAD-REPAIR. 0.0 is the identity (uniform station_m everywhere);
        // anything else must be SHORTER than station_m or the name is a lie,
        // and it must stay above the bake's own resolution -- a station length
        // finer than the drawn ribbon's vertex spacing buys interpolation, not
        // information.
        check(b.junction_station_m >= 0.0 &&
                  (b.junction_station_m == 0.0 ||
                   (b.junction_station_m < b.station_m &&
                    b.junction_station_m >= 1.0)),
              "bank_mesh junction_station_m == 0 (off) or in [1, station_m)");
        check(b.chord_tol_m >= 0.0,
              "bank_mesh chord_tol_m >= 0 -- the cross-section chord "
              "tolerance in metres (0 disables the ring refinement)");
        // ★ ROAD-REPAIR ONAPING RUNG 2. 0.0 is the identity (no apron strip);
        // the ceiling is the [snowpack] corridor's own reach -- an apron wider
        // than 4x the burial skirt has stopped being a road edge and started
        // being a second terrain, and the vertex budget (+72 % already owed on
        // the bank mesh) says the same thing.
        check(b.apron_m >= 0.0 && b.apron_m <= 4.0 * b.skirt_m,
              "bank_mesh apron_m == 0 (off) or in (0, 4*skirt_m]");
        check(b.apron_tol_m > 0.0,
              "bank_mesh apron_tol_m > 0 -- the drawn-vs-driven agreement "
              "tolerance the apron stops at, in metres");
        check(b.apron_min_drop_m >= 0.0,
              "bank_mesh apron_min_drop_m >= 0 -- the terrain fall across the "
              "burial skirt a station needs before it gets an apron at all");
    }
    // ★ SC1 [cold] -- WINTER_LAW §3.7. Exactly the rejections spec'd (SC1_COLD_
    // SPEC.md §1): each one guards a signed ruling from a silent retune.
    {
        const world::ColdParams& c = w.cold;
        // The night snap must be COLDER than (or equal to) the night baseline
        // -- spec's literal rejection is "t_snap_c > t_night_c".
        check(!(c.t_snap_c > c.t_night_c),
              "cold t_snap_c must be <= t_night_c -- the snap is the COLDEST "
              "point of the cycle, not a warm spike");
        check(c.h_max >= 1.0 && c.h_max <= 1.40,
              "cold h_max in [1.0, 1.40] -- the p99 drainage line un-bogs past "
              "~1.45 (measured cliff), deleting signed bogging behaviour");
        check(c.cold_gain >= 0.0, "cold cold_gain >= 0");
        // ★ "AFTER midnight" is the ruling (WINTER_LAW §3.7's "extra cold in
        // the midnight"): snap_center <= 0.5 would put the coldest point
        // before or at solar midnight, contradicting it.
        check(c.snap_center > 0.5,
              "cold snap_center must be > 0.5 -- the cold snap is ruled to "
              "sit AFTER solar midnight");
        check(c.warm_drag_gain >= 0.0, "cold warm_drag_gain >= 0");
        // Not in the spec's literal rejection list, but the same class of
        // hygiene as [snowpack]'s corridor_edge_m/bank_gap_m > 0 checks: a
        // zero-or-negative width would divide by zero / invert the gate in
        // world::air_temp_c's snap-distance smoothstep.
        check(c.snap_width > 0.0,
              "cold snap_width > 0 -- it is the denominator of the snap gate");
        check(c.h_slope_per_c >= 0.0, "cold h_slope_per_c >= 0");
    }
    // subdiv: the raylib mesh index is unsigned short, so <=256 verts/edge
    // (256*256 < 65536); >=2 to be a quad. dem_blur_radius: non-negative.
    check(w.planet.subdiv >= 2 && w.planet.subdiv <= 256,
          "planet subdiv in [2,256] (ushort index cap)");
    // tiles: R4d face tiling — each face is tiles^2 sub-meshes, each under the
    // ushort cap; 4 (96 meshes, ~6M tris) is the sane ceiling.
    check(w.planet.tiles >= 1 && w.planet.tiles <= 4, "planet tiles in [1,4]");
    check(w.planet.dem_blur_radius >= 0, "planet dem_blur_radius >= 0");
    // Cubemap face edge: >0 to upload, and a soft ceiling so a fat-fingered
    // value can't try to allocate a multi-GB bake (6 * size^2 * 3 bytes).
    check(w.planet.cubemap_size >= 1 && w.planet.cubemap_size <= 4096,
          "planet cubemap_size in [1,4096]");
    // The A/B toggle is a hard 0/1 (Fable: a placeholder source tunes behind
    // it).
    check(procedural == 0.0 || procedural == 1.0,
          "ground use_procedural is 0 or 1");
    w.ground.procedural = (procedural != 0.0);

    // Fleet Rig mirror knobs. Colors are RGB fractions [0,1] (the only
    // saturated entries in the render config).
    check(w.fleet_rig.reflectivity >= 0.0 && w.fleet_rig.reflectivity <= 1.0,
          "fleet_rig reflectivity in [0,1]");
    check(w.fleet_rig.fresnel_power > 0.0, "fleet_rig fresnel_power > 0");
    const auto rgb_in01 = [](const glm::dvec3& c) {
        return c.x >= 0.0 && c.x <= 1.0 && c.y >= 0.0 && c.y <= 1.0 &&
               c.z >= 0.0 && c.z <= 1.0;
    };
    // rig-B deflection throws: positive degrees, and a sane ceiling so a
    // fat-fingered value can't swing a surface through the airframe.
    check(w.fleet_rig.aileron_deg > 0.0 && w.fleet_rig.aileron_deg <= 60.0,
          "fleet_rig aileron_deg in (0,60]");
    check(w.fleet_rig.elevator_deg > 0.0 && w.fleet_rig.elevator_deg <= 60.0,
          "fleet_rig elevator_deg in (0,60]");
    check(w.fleet_rig.rudder_deg > 0.0 && w.fleet_rig.rudder_deg <= 60.0,
          "fleet_rig rudder_deg in (0,60]");
    check(w.fleet_rig.gear_deploy_deg > 0.0 &&
              w.fleet_rig.gear_deploy_deg <= 120.0,
          "fleet_rig gear_deploy_deg in (0,120]");
    check(w.fleet_rig.prop_disc_alpha >= 0.0 &&
              w.fleet_rig.prop_disc_alpha <= 1.0,
          "fleet_rig prop_disc_alpha in [0,1]");
    check(w.fleet_rig.prop_idle_alpha >= 0.0 &&
              w.fleet_rig.prop_idle_alpha <= w.fleet_rig.prop_disc_alpha,
          "fleet_rig prop_idle_alpha in [0, prop_disc_alpha]");

    // [plane_legibility] (rung E4). CONFIG-RELATIVE where a relation exists,
    // not just absolute ranges — the load_scenario discipline.
    {
        const render::LegibilityParams& lg = w.legibility;
        // 1.0 = today (full mirror env wash); 0 would strip the mirror finish
        // off the drones entirely, which is a legitimate extreme, so [0,1].
        check(lg.aircraft_haze_frac >= 0.0 && lg.aircraft_haze_frac <= 1.0,
              "plane_legibility aircraft_haze_frac in [0,1] (1 = today)");
        // The ramp distance must be non-negative; 0 means "apply everywhere,
        // no ramp" and is the documented degenerate case, not an error.
        check(lg.haze_full_range_m >= 0.0,
              "plane_legibility haze_full_range_m >= 0");
        // A halo wider than a glyph turns the tag into a black smudge; the
        // shipped tag font is 12 px, so cap the ring at 4 px.
        check(lg.tag_outline_px >= 0.0 && lg.tag_outline_px <= 4.0,
              "plane_legibility tag_outline_px in [0,4]");
        // 0 = off; anything positive must still be a readable-and-not-absurd
        // glyph height.
        check(lg.tag_min_px == 0.0 ||
                  (lg.tag_min_px >= 8.0 && lg.tag_min_px <= 48.0),
              "plane_legibility tag_min_px is 0 (off) or in [8,48] px");
        check(lg.enemy_tint_shift >= 0.0 && lg.enemy_tint_shift <= 1.0,
              "plane_legibility enemy_tint_shift in [0,1] (0 = today)");
        check(lg.tag_min_alpha >= 0.0 && lg.tag_min_alpha <= 200.0,
              "plane_legibility tag_min_alpha in [0,200] (0 = today's fade; "
              "200 = the base tag alpha, a floor above it is meaningless)");
        // The glint is a NAV-LIGHT BEAD, not a second aircraft: bound it well
        // under the ~9.9 m Bf 109 span so it can never read as geometry.
        check(lg.glint_size_m >= 0.0 && lg.glint_size_m <= 3.0,
              "plane_legibility glint_size_m in [0,3] m (0 = off)");
        check(lg.glint_range_m >= 0.0,
              "plane_legibility glint_range_m >= 0");
        // The angular floor: 5 mrad at 4 km would be a 20 m beacon.
        check(lg.glint_min_mrad >= 0.0 && lg.glint_min_mrad <= 5.0,
              "plane_legibility glint_min_mrad in [0,5]");
        // A period under ~10 frames strobes; 600 frames is a 10 s blink at 60.
        check(lg.glint_period_frames == 0 ||
                  (lg.glint_period_frames >= 10 &&
                   lg.glint_period_frames <= 600),
              "plane_legibility glint_period_frames is 0 (off) or in [10,600]");
        check(lg.glint_duty >= 0.0 && lg.glint_duty <= 1.0,
              "plane_legibility glint_duty in [0,1]");
        // CONFIG-RELATIVE: a lit glint with no range and no period is a dial
        // set that LOOKS on and silently draws nothing (the silent-disarm
        // class). If the bead has a size, the other two must be live.
        check(lg.glint_size_m == 0.0 ||
                  (lg.glint_range_m > 0.0 && lg.glint_period_frames > 0 &&
                   lg.glint_duty > 0.0),
              "plane_legibility glint_size_m > 0 requires glint_range_m, "
              "glint_period_frames and glint_duty all > 0");
    }

    // contrast > 0; upper cap 8 — past ~30 the sigmoid's 1e-6 epsilon dominates
    // the denominator and collapses mid-grey toward 0 (a typo like 35 would
    // pass a bare >0 check; the config-relative-bounds lesson). Default 1.35.
    check(w.tone.contrast > 0.0 && w.tone.contrast <= 8.0,
          "tone contrast in (0,8]");
    check(w.tone.lift >= 0.0 && w.tone.lift < 1.0, "tone lift in [0,1)");
    // Grain is a whisper by ruling (high contrast, NOT grainy) — reject a value
    // that would crush legibility, not just an out-of-range one.
    check(
        w.tone.grain >= 0.0 && w.tone.grain <= 0.25,
        "tone grain in [0, 0.25] (minimal — the ruling is crisp, not gritty)");
    // S1 post dials. The split-tone gate is an ORDERED smoothstep: c0 < c1 (a
    // collapsed edge makes GLSL smoothstep undefined — the silent-disarm
    // class). sat_dark > 0 is the near-black divide floor (a 0 blows up C/mx at
    // black).
    check(w.tone.sat_c0 >= 0.0 && w.tone.sat_c0 < w.tone.sat_c1,
          "tone sat_c0 in [0, sat_c1)");
    check(w.tone.sat_c1 <= 1.0, "tone sat_c1 <= 1");
    check(w.tone.sat_dark > 0.0 && w.tone.sat_dark <= 1.0,
          "tone sat_dark in (0,1] (near-black chroma floor)");
    check(w.tone.halation >= 0.0 && w.tone.halation <= 1.0,
          "tone halation in [0,1]");
    check(w.tone.halation_threshold >= 0.0 && w.tone.halation_threshold <= 1.0,
          "tone halation_threshold in [0,1]");
    check(w.tone.vignette >= 0.0 && w.tone.vignette <= 1.0,
          "tone vignette in [0,1]");
    check(w.tone.fxaa >= 0.0 && w.tone.fxaa <= 1.0, "tone fxaa in [0,1]");
    // Tints are luminance MULTIPLIERS near 1 (a silver print). [0,2] is a loose
    // typo cap — a tint past 2 washes the whole mono scene, not a tune.
    const auto tint_in02 = [](const glm::dvec3& c) {
        return c.x >= 0.0 && c.x <= 2.0 && c.y >= 0.0 && c.y <= 2.0 &&
               c.z >= 0.0 && c.z <= 2.0;
    };
    check(tint_in02(w.tone.split_shadow), "tone split_shadow RGB in [0,2]");
    check(tint_in02(w.tone.split_hi), "tone split_hi RGB in [0,2]");
    check(tint_in02(w.tone.halation_tint), "tone halation_tint RGB in [0,2]");

    // [dof] S6 far-field-only DoF. The CoC is smoothstep(focus_start,
    // focus_end): a collapsed/inverted edge makes GLSL smoothstep undefined
    // (the silent-disarm class), so require the ordered pair strictly. radius 0
    // = off (valid). Bounds are loose typo caps (metres within the 60 km far
    // clip, px within a sane disc).
    check(w.dof.focus_start_m >= 0.0 && w.dof.focus_start_m < w.dof.focus_end_m,
          "dof focus_start_m in [0, focus_end_m)");
    check(w.dof.focus_end_m <= 60000.0, "dof focus_end_m <= far clip (60 km)");
    check(w.dof.radius_px >= 0.0 && w.dof.radius_px <= 16.0,
          "dof radius_px in [0,16] (0=off; a subtle far-field disc, not a "
          "smear)");
    check(w.dof.sky_coc >= 0.0 && w.dof.sky_coc <= 1.0,
          "dof sky_coc in [0,1] (far-sky CoC cap)");

    // [halftone] S6 print MODE. style is the shader branch selector (1 dots / 2
    // dither) — reject anything else (a stray 3 would silently no-op the mode).
    // scale_px floored >= 2: the shader divides by it (0 = NaN) and below ~2 px
    // the dots subsample and shimmer regardless of AA (Fable-BEFORE #4).
    check(w.halftone.style == 1 || w.halftone.style == 2,
          "halftone style is 1 (dots) or 2 (dither)");
    check(w.halftone.scale_px >= 2.0 && w.halftone.scale_px <= 64.0,
          "halftone scale_px in [2,64] (>=2 = NaN/subsample guard)");
    check(w.halftone.soft >= 0.0 && w.halftone.soft <= 4.0,
          "halftone soft in [0,4] (mode1 px AA mult; mode2 tonal crossfade)");
    check(w.halftone.ink >= 0.0 && w.halftone.ink <= 1.0,
          "halftone ink in [0,1] (ink darkness level)");
    check(w.halftone.grain_mul >= 0.0 && w.halftone.grain_mul <= 1.0,
          "halftone grain_mul in [0,1] (grain scale on the mono print)");

    check(w.horizon_cull.margin_rad >= 0.0, "horizon_cull margin_deg >= 0");

    check(w.scatter.seed >= 0,
          "scatter seed >= 0 (deterministic; never a clock)");
    // Furniture is scaled UP for read-from-altitude legibility (ruling: ~5x,
    // generalized). A <1 furniture scale is never intended.
    check(w.scatter.furniture_scale >= 1.0, "scatter furniture_scale >= 1");

    check(w.perf.frame_budget_ms > 0.0, "perf frame_budget_ms > 0");
    check(w.perf.max_draw_calls >= 1, "perf max_draw_calls >= 1");

    // Celestial ranges. The STRUCTURAL asserts (year an integer multiple of the
    // solar day; tilt_lean not parallel to the orbit normal) live in
    // render::make_celestial, called once at app startup — kept there so config
    // stays render-independent. The sun_distance >= 30*R check needs R (the
    // sim's), so the app asserts it after loading both configs.
    check(glm::length(cel.orbit_normal) > 1e-9,
          "celestial orbit_normal nonzero");
    check(glm::length(cel.tilt_lean) > 1e-9, "celestial tilt_lean nonzero");
    check(cel.tilt_deg >= 0.0 && cel.tilt_deg < 90.0,
          "celestial tilt_deg in [0,90)");
    check(cel.day_period_s > 0.0, "celestial day_period_s > 0");
    check(cel.year_period_s > 0.0, "celestial year_period_s > 0");
    check(cel.epoch_day_frac >= 0.0 && cel.epoch_day_frac < 1.0,
          "celestial epoch_day_frac in [0,1)");
    check(cel.epoch_year_frac >= 0.0 && cel.epoch_year_frac < 1.0,
          "celestial epoch_year_frac in [0,1)");
    check(cel.epoch_moon_frac >= 0.0 && cel.epoch_moon_frac < 1.0,
          "celestial epoch_moon_frac in [0,1)");
    check(cel.sun_angular_diameter_deg > 0.0,
          "celestial sun_angular_diameter_deg > 0");
    check(cel.sun_distance_m > 0.0, "celestial sun_distance_m > 0");
    check(cel.sun_intensity >= 0.0, "celestial sun_intensity >= 0");
    check(cel.sun_glare_deg >= 0.0, "celestial sun_glare_deg >= 0");
    check(cel.moon_angular_diameter_deg > 0.0,
          "celestial moon_angular_diameter_deg > 0");
    check(cel.moon_period_s > 0.0, "celestial moon_period_s > 0");
    check(cel.moon_inclination_deg >= 0.0 && cel.moon_inclination_deg < 90.0,
          "celestial moon_inclination_deg in [0,90)");
    check(cel.moon_intensity >= 0.0, "celestial moon_intensity >= 0");
    check(cel.star_brightness >= 0.0, "celestial star_brightness >= 0");

    // Star render knobs. mag_limit only CULLS the compiled catalog (baked to
    // mag 5.5), so a higher value just keeps all — a loose typo cap. size_px
    // and r_star_m must be positive (a zero collapses the billboard); wash_lum
    // is a luminance fraction; glare_suppress non-negative. The r_star_m <
    // far-plane clip bound is asserted in render::load_stars (the far plane is
    // a render constant, kept out of config).
    check(st.mag_limit >= -2.0 && st.mag_limit <= 8.0,
          "stars mag_limit in [-2,8]");
    check(st.mag_ref >= -2.0 && st.mag_ref <= 8.0, "stars mag_ref in [-2,8]");
    check(st.size_px > 0.0, "stars size_px > 0");
    // Strictly positive: a 0 collapses the shader's smoothstep to edge0==edge1
    // (GLSL-undefined -> stars silently ALL off / a NaN), the silent-disarm
    // class (Fable red-team P2-1). A tiny positive is effectively "off".
    check(st.glare_suppress_deg > 0.0, "stars glare_suppress_deg > 0");
    check(st.wash_lum > 0.0 && st.wash_lum <= 1.0, "stars wash_lum in (0,1]");
    check(st.r_star_m > 0.0, "stars r_star_m > 0");

    // Moon look knobs. disc_intensity [0,4] (a loose typo cap; the LDR disc
    // saturates ~1). ground_gain [0,2] (mirrors ground_day_gain; >2 blows the
    // night side to white). fill_lo in [0,1) — it's the low edge of a
    // smoothstep(fill_lo, 1, phase), so >= 1 collapses the knee (no fill ever).
    // sparkle_sharpness > 0 (a 0 exponent makes pow() == 1 everywhere).
    check(mn.disc_intensity >= 0.0 && mn.disc_intensity <= 4.0,
          "moon disc_intensity in [0,4]");
    check(mn.ground_gain >= 0.0 && mn.ground_gain <= 2.0,
          "moon ground_gain in [0,2]");
    check(mn.fill_lo >= 0.0 && mn.fill_lo < 1.0, "moon fill_lo in [0,1)");
    check(mn.sparkle_sharpness > 0.0, "moon sparkle_sharpness > 0");

    // Aurora ranges. intensity >= 0. The oval colatitude in (0, pi) and its
    // half-width > 0 (a 0 collapses the gaussian ring's max() to 1e-4 -> a
    // spike). height_m > 0 AND is asserted > max flyable altitude at the app
    // boundary (needs R; the eye MUST stay inside the shell, Fable-after P1-1)
    // — kept here as a positive check, tightened app-side. anim_rate >= 0;
    // glow/suppress in [0,1]; tints RGB in [0,2] (a splash of color, not
    // unbounded). intensity: >=0, loose typo cap 2 (the curtain adds up to
    // intensity*1.4*tint; the plane is drawn after the sky so it can't be
    // drowned, but keep a ceiling — Fable red-team P2). curtain_scale >= 1: the
    // shader rounds it to an INTEGER harmonic (az-continuity), and a 0 would
    // null the fundamental ribbon.
    check(au.intensity >= 0.0 && au.intensity <= 2.0,
          "aurora intensity in [0,2]");
    check(au.oval_center_rad > 0.0 && au.oval_center_rad < 3.14159265,
          "aurora oval_center_deg in (0,180)");
    check(au.oval_width_rad > 0.0, "aurora oval_width_deg > 0");
    check(au.curtain_scale >= 1.0, "aurora curtain_scale >= 1");
    check(au.height_m > 0.0, "aurora height_m > 0");
    check(au.anim_rate >= 0.0, "aurora anim_rate >= 0");
    check(au.ground_glow >= 0.0 && au.ground_glow <= 1.0,
          "aurora ground_glow in [0,1]");
    check(au.haze_suppress >= 0.0 && au.haze_suppress <= 1.0,
          "aurora haze_suppress in [0,1]");
    for (int i = 0; i < 3; ++i) {
        check(au.tint_low[i] >= 0.0 && au.tint_low[i] <= 2.0,
              "aurora tint_low components in [0,2]");
        check(au.tint_high[i] >= 0.0 && au.tint_high[i] <= 2.0,
              "aurora tint_high components in [0,2]");
    }

    // [map] — S-mapread. Colours strictly in [0,1] (raylib bytes, no HDR here);
    // pixel sizes strictly > 0 (a zero marker is an INVISIBLE marker, the
    // silent-disarm class this whole pass exists to fix — so it is rejected,
    // not clamped); fractions in [0,1].
    {
        check(rgb_in01(mp.paper), "map paper RGB in [0,1]");
        check(rgb_in01(mp.ink), "map ink RGB in [0,1]");
        check(rgb_in01(mp.water), "map water RGB in [0,1]");
        check(rgb_in01(mp.neutral_color), "map neutral_color RGB in [0,1]");
        check(rgb_in01(mp.outline_color), "map outline_color RGB in [0,1]");
    }
    // [teams] — the faction palette. Range first, then the RULING: Chad asked
    // for the ally hue to be "with precision the opposite" of the slag orange
    // on the colour wheel, so the 180 deg HSV complement is enforced HERE, at
    // the config edge, using the SAME predicate the unit test uses
    // (render::is_exact_complement). A hand-edited [teams] that breaks the
    // complement fails loud at startup instead of shipping a near-miss nobody
    // can see (the plausible channel-reversal (0.12,0.55,1.00) is 181.36 deg
    // away, not 180 — that is the trap this check exists for). The tolerance
    // is loose enough for TOML's decimal round-trip, tight enough to reject
    // that 1.36 deg miss by three orders of magnitude.
    {
        check(rgb_in01(w.teams.ally), "teams ally RGB in [0,1]");
        check(rgb_in01(w.teams.enemy), "teams enemy RGB in [0,1]");
        check(rgb_in01(w.teams.objective), "teams objective RGB in [0,1]");
        check(render::is_exact_complement(w.teams.enemy, w.teams.ally, 1e-6),
              "teams ally must be the EXACT 180 deg HSV complement of "
              "teams enemy (same saturation, same value)");
        // NOTE: `teams.enemy` is ALSO the slag `hot` stop — render/slag.cpp
        // injects team_colors().enemy into the lava shader, so retuning this
        // one entry moves the molten slag and the enemy faction together, by
        // construction. That is the ruling ("the slag orange color we use
        // across this codebase"), not an accident.
    }
    check(mp.basemap_ink >= 0.0 && mp.basemap_ink <= 1.0,
          "map basemap_ink in [0,1]");
    check(mp.lake_label_span_m >= 0.0, "map lake_label_span_m >= 0");
    check(mp.marker_px > 0.0, "map marker_px > 0");
    check(mp.objective_px > 0.0, "map objective_px > 0");
    check(mp.player_px > 0.0, "map player_px > 0");
    check(mp.outline_px > 0.0, "map outline_px > 0");
    check(mp.territory_outline_px > 0.0, "map territory_outline_px > 0");
    check(mp.territory_fill >= 0.0 && mp.territory_fill <= 1.0,
          "map territory_fill in [0,1]");
    // arrow_hold_sin is a SINE threshold on the nose's tangential fraction, so
    // it lives in [0,1). At exactly 1 the guard would never release and the
    // arrow would freeze at north forever — reject it rather than ship a
    // silently-dead arrow.
    check(mp.arrow_hold_sin >= 0.0 && mp.arrow_hold_sin < 1.0,
          "map arrow_hold_sin in [0,1)");
    // L4 -- THE VIEW. zoom is a multiple of FIT and the floor is 1 by
    // construction, so every bound below is stated against that 1. zoom_step
    // <= 1 is the wheel's kill-switch arm (a step of exactly 1 would multiply
    // by 1 forever, a step below 1 would invert the wheel), so it is ALLOWED
    // and read as OFF -- but it must be positive and finite.
    check(mp.zoom_step > 0.0, "map zoom_step > 0 (<= 1 = wheel OFF)");
    check(mp.zoom_max > 1.0, "map zoom_max > 1 (1 = fit, the floor)");
    // The follow threshold has to be INSIDE the band or it is unreachable in
    // one direction: at or above zoom_max the view could never follow, below 1
    // it would follow at fit -- which is the one zoom that must keep drawing
    // today's chart.
    check(mp.follow_zoom >= 1.0 && mp.follow_zoom < mp.zoom_max,
          "map follow_zoom in [1, zoom_max)");
    // The declutter thresholds ADD (on = visible || zoom >= threshold), so a
    // threshold at or below 1 would force that layer on at FIT and silently
    // change the shipped overview. Reject it: "zoom = fit draws exactly what
    // it draws today" is a ruling, not a default.
    check(mp.trails_zoom > 1.0, "map trails_zoom > 1 (fit must not declutter)");
    check(mp.minor_roads_zoom > 1.0,
          "map minor_roads_zoom > 1 (fit must not declutter)");
    check(mp.place_labels_zoom > 1.0,
          "map place_labels_zoom > 1 (fit must not declutter)");
    // The BASEMAP IS GREYSCALE BY RULING (Chad: "newspaper greyscale for the
    // look but objectives and playermarkers properly color coded"). Enforce it
    // executably — a chroma cast smuggled into the plate is exactly the
    // "conflicting or camouflaging colors" defect, and a comment cannot stop
    // it. Tolerance is one 8-bit step, so a legal near-neutral paper stock
    // passes but a tint does not.
    const auto is_grey = [](const glm::dvec3& c) {
        const double lo = std::min({c.x, c.y, c.z});
        const double hi = std::max({c.x, c.y, c.z});
        return (hi - lo) <= (1.0 / 255.0) * 4.0;
    };
    check(is_grey(mp.paper), "map paper must be neutral grey (basemap rule)");
    check(is_grey(mp.ink), "map ink must be neutral grey (basemap rule)");
    check(is_grey(mp.water), "map water must be neutral grey (basemap rule)");

    // [pump_frame] — the frame must CLEAR the pump body it frames, or its edges
    // z-fight the beacon shell (the defect the additive/no-depth-write draw
    // exists to avoid). render::kDeepPumpShellM is the ONE copy of the shell
    // radius — draw.cpp draws the sphere from it too, so this bound can never
    // drift from the geometry it guards.
    check(pf.half_extent_m > render::kDeepPumpShellM,
          "pump_frame half_extent_m must exceed the deep-pump shell radius "
          "(render::kDeepPumpShellM = 26 m) or the frame z-fights the body");
    check(pf.half_extent_m <= 500.0, "pump_frame half_extent_m <= 500");
    check(pf.layers >= 1 && pf.layers <= 8, "pump_frame layers in [1,8]");
    check(pf.layer_step_m >= 0.0 && pf.layer_step_m <= 10.0,
          "pump_frame layer_step_m in [0,10]");
    // The nested shells must stay OUTSIDE the body too, not just the inner one.
    check(pf.half_extent_m -
                  static_cast<double>(pf.layers - 1) * pf.layer_step_m >
              render::kDeepPumpShellM,
          "pump_frame innermost shell (half_extent_m - (layers-1)*"
          "layer_step_m) must still clear render::kDeepPumpShellM");
    check(pf.brightness > 0.0 && pf.brightness <= 4.0,
          "pump_frame brightness in (0,4]");
    check(rgb_in01(pf.neutral_color), "pump_frame neutral_color RGB in [0,1]");

    return w;
}

}  // namespace cfg
