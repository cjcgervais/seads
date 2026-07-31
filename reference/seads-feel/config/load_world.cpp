#include "config/load_world.h"

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
        throw std::runtime_error(std::string("world.toml: missing or "
                                             "non-numeric key [") +
                                 section + "] " + key);
    }
    return node->value_or(0.0);
}

void check(bool ok, const char* what) {
    if (!ok) {
        throw std::runtime_error(std::string("world.toml: invalid value: ") +
                                 what);
    }
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

    w.sky.silver = require(root, "sky", "silver");
    w.sky.gradient = require(root, "sky", "gradient");

    w.water.reflectivity = require(root, "water", "reflectivity");
    w.water.sparkle_sharpness = require(root, "water", "sparkle_sharpness");
    w.water.min_lake_m = require(root, "water", "min_lake_m");

    w.ground.relief_scale_m = require(root, "ground", "relief_scale_m");
    w.ground.field_scale_m = require(root, "ground", "field_scale_m");
    const double procedural = require(root, "ground", "use_procedural");

    w.planet.subdiv = static_cast<int>(require(root, "planet", "subdiv"));
    w.planet.u_offset = require(root, "planet", "u_offset");
    w.planet.dem_blur_radius =
        static_cast<int>(require(root, "planet", "dem_blur_radius"));

    w.tone.contrast = require(root, "tone", "contrast");
    w.tone.lift = require(root, "tone", "lift");
    w.tone.grain = require(root, "tone", "grain");

    w.horizon_cull.margin_rad =
        rad(require(root, "horizon_cull", "margin_deg"));

    const double seed = require(root, "scatter", "seed");
    w.scatter.seed = static_cast<int>(seed);
    w.scatter.furniture_scale = require(root, "scatter", "furniture_scale");

    w.perf.frame_budget_ms = require(root, "perf", "frame_budget_ms");
    const double max_draw = require(root, "perf", "max_draw_calls");
    w.perf.max_draw_calls = static_cast<int>(max_draw);

    // ---- sanity ---------------------------------------------------------
    // Luminance/strength fractions are physical fractions of the value range.
    check(w.sky.silver >= 0.0 && w.sky.silver <= 1.0, "sky silver in [0,1]");
    check(w.sky.gradient >= 0.0 && w.sky.gradient <= 1.0,
          "sky gradient in [0,1]");

    check(w.water.reflectivity >= 0.0 && w.water.reflectivity <= 1.0,
          "water reflectivity in [0,1]");
    check(w.water.sparkle_sharpness > 0.0, "water sparkle_sharpness > 0");
    // A lake below the landmask resolution floor (~46 m/texel at 2048) renders
    // as a gray smudge, not a silver beacon (Fable water trap 3).
    check(w.water.min_lake_m > 0.0, "water min_lake_m > 0");

    check(w.ground.relief_scale_m > 0.0, "ground relief_scale_m > 0");
    check(w.ground.field_scale_m > 0.0, "ground field_scale_m > 0");
    // subdiv: the raylib mesh index is unsigned short, so <=256 verts/edge
    // (256*256 < 65536); >=2 to be a quad. dem_blur_radius: non-negative.
    check(w.planet.subdiv >= 2 && w.planet.subdiv <= 256,
          "planet subdiv in [2,256] (ushort index cap)");
    check(w.planet.dem_blur_radius >= 0, "planet dem_blur_radius >= 0");
    // The A/B toggle is a hard 0/1 (Fable: a placeholder source tunes behind
    // it).
    check(procedural == 0.0 || procedural == 1.0,
          "ground use_procedural is 0 or 1");
    w.ground.procedural = (procedural != 0.0);

    check(w.tone.contrast > 0.0, "tone contrast > 0");
    check(w.tone.lift >= 0.0 && w.tone.lift < 1.0, "tone lift in [0,1)");
    // Grain is a whisper by ruling (high contrast, NOT grainy) — reject a value
    // that would crush legibility, not just an out-of-range one.
    check(
        w.tone.grain >= 0.0 && w.tone.grain <= 0.25,
        "tone grain in [0, 0.25] (minimal — the ruling is crisp, not gritty)");

    check(w.horizon_cull.margin_rad >= 0.0, "horizon_cull margin_deg >= 0");

    check(w.scatter.seed >= 0,
          "scatter seed >= 0 (deterministic; never a clock)");
    // Furniture is scaled UP for read-from-altitude legibility (ruling: ~5x,
    // generalized). A <1 furniture scale is never intended.
    check(w.scatter.furniture_scale >= 1.0, "scatter furniture_scale >= 1");

    check(w.perf.frame_budget_ms > 0.0, "perf frame_budget_ms > 0");
    check(w.perf.max_draw_calls >= 1, "perf max_draw_calls >= 1");

    return w;
}

}  // namespace cfg
