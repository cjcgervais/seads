// The WEATHER SEASON framing (docs/weather_seasons_plan.md W1). Pins the pure
// enum<->name mapping that the config loader, the app-side draw, and the HUD tag
// all share (a fork here = a static override or a random draw landing on the
// wrong season). No sim/control state; moves no flight golden.

#include <catch2/catch_test_macros.hpp>
#include <string>

#include "render/season.h"

#ifdef NDEBUG
#error "SEADS gate requires an assert-live build (SPEC 6.1)"
#endif

using render::Season;

TEST_CASE("season: names round-trip through parse (every enumerator)") {
    for (int i = 0; i < render::kSeasonCount; ++i) {
        const Season s = static_cast<Season>(i);
        Season back;
        REQUIRE(render::season_from_string(render::season_name(s), back));
        CHECK(back == s);
    }
}

TEST_CASE("season: the canonical names + integer order are the shared contract") {
    // The loader's local season_index() and the app-side weighted draw both
    // assume THIS order — Winter=0..Autumn=3 — so pin it explicitly.
    CHECK(std::string(render::season_name(Season::Winter)) == "winter");
    CHECK(std::string(render::season_name(Season::Spring)) == "spring");
    CHECK(std::string(render::season_name(Season::Summer)) == "summer");
    CHECK(std::string(render::season_name(Season::Autumn)) == "autumn");
    CHECK(static_cast<int>(Season::Winter) == 0);
    CHECK(static_cast<int>(Season::Autumn) == 3);
}

TEST_CASE("season: parse is case-insensitive and space-tolerant") {
    Season s;
    REQUIRE(render::season_from_string("WINTER", s));
    CHECK(s == Season::Winter);
    REQUIRE(render::season_from_string("  Spring ", s));
    CHECK(s == Season::Spring);
    REQUIRE(render::season_from_string("SuMmEr", s));
    CHECK(s == Season::Summer);
}

TEST_CASE("season: resolve_season honors the env>static>smoke>random precedence") {
    // The one W1 mechanism the app-binary-blind gate can't reach otherwise
    // (Fable-after P2-3). draw() returns Autumn(3); a call means the random
    // branch was taken. draw_called tracks that the RNG is sampled ONLY there.
    bool drawn = false;
    auto draw = [&] {
        drawn = true;
        return 3;  // Autumn
    };
    // env override wins over everything (static set, smoke true).
    drawn = false;
    CHECK(render::resolve_season(true, Season::Spring, 0, true, draw) ==
          Season::Spring);
    CHECK_FALSE(drawn);
    // no env: static wins over smoke + random.
    drawn = false;
    CHECK(render::resolve_season(false, Season::Spring, 2 /*Summer*/, true,
                                 draw) == Season::Summer);
    CHECK_FALSE(drawn);
    // no env, no static (-1): smoke default is Summer, RNG NOT sampled.
    drawn = false;
    CHECK(render::resolve_season(false, Season::Winter, -1, true, draw) ==
          Season::Summer);
    CHECK_FALSE(drawn);
    // no env, no static, not smoke: the weighted random draw is taken.
    drawn = false;
    CHECK(render::resolve_season(false, Season::Winter, -1, false, draw) ==
          Season::Autumn);
    CHECK(drawn);
}

TEST_CASE("season: junk and the 'random' sentinel do NOT parse to a season") {
    Season s = Season::Autumn;  // sentinel; must be left untouched on a miss
    CHECK_FALSE(render::season_from_string("random", s));
    CHECK_FALSE(render::season_from_string("", s));
    CHECK_FALSE(render::season_from_string("wintr", s));
    CHECK_FALSE(render::season_from_string("fall", s));  // we use 'autumn'
    CHECK(s == Season::Autumn);
}
