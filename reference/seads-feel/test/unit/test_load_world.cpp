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
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "config/load_world.h"

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
    CHECK(w.sky.silver > 0.0);           // Mercury silver
    CHECK(w.tone.contrast > 1.0);        // high contrast
    CHECK(w.perf.max_draw_calls >= 1);
    CHECK(w.scatter.seed >= 0);  // never a wall clock
    // The mesh knobs that moved out of render/draw.cpp constexprs into config.
    CHECK(w.planet.subdiv == 200);
    CHECK(w.planet.u_offset == Catch::Approx(0.806));
    CHECK(w.planet.dem_blur_radius == 3);
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

TEST_CASE("load_world: an out-of-range sky luminance is rejected") {
    const std::string base = slurp(SEADS_CONFIG_DIR "/world.toml");
    REQUIRE(base.find("silver              = 0.82") != std::string::npos);
    const std::string bad = replace_all(base, "silver              = 0.82",
                                        "silver              = 1.5");
    CHECK_THROWS(cfg::load_world_toml(write_temp(bad, "sky_over_1")));
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

TEST_CASE("load_world: a sub-unity furniture scale is rejected") {
    const std::string base = slurp(SEADS_CONFIG_DIR "/world.toml");
    REQUIRE(base.find("furniture_scale     = 5.0") != std::string::npos);
    const std::string bad = replace_all(base, "furniture_scale     = 5.0",
                                        "furniture_scale     = 0.5");
    CHECK_THROWS(cfg::load_world_toml(write_temp(bad, "tiny_furniture")));
}
