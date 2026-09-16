// S-pumpcube — the neon team wire cube that frames a pump (Chad 2026-08-09):
//
//   "the green pumps inside the black stope need a wire cube that is neon slag
//    and opposing blue of the same team color codes to frame in that pump
//    otherwise they dont apper claimed by either side in the black stope"
//
// The DRAW itself is unpinnable here — no ctest runs seads.exe, so raylib
// geometry is certified by screenshot (docs/map_shots/pump_*.png), never by the
// gate. What IS pinnable is the pure decision layer the draw reads, and that is
// where the two rulings live that a future edit could silently break:
//
//   1. SCOPE: deep-only by default (Chad's literal ask), one dial to extend it
//      to every pump. A regression here changes the SURFACE pump look, which he
//      did not ask for.
//   2. THE NEUTRAL RULE: an unowned or DEAD pump must NOT inherit a team hue.
//      This is the whole point of the mechanism — a frame is an ownership
//      claim, so a stale one is worse than none.
//
// Plus the size invariant the loader enforces (the frame must clear the body),
// which is only meaningful because kDeepPumpShellM is single-sourced: draw.cpp
// draws the sphere from the same constant.

#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "config/load_world.h"
#include "render/pump_frame.h"
#include "render/team_color.h"

#ifdef NDEBUG
#error "SEADS gate requires an assert-live build (SPEC 6.1)"
#endif

namespace {

std::string slurp(const std::string& path) {
    std::ifstream f(path);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::string write_temp(const std::string& text, const char* tag) {
    const std::filesystem::path p =
        std::filesystem::temp_directory_path() /
        (std::string("seads_pumpframe_") + tag + ".toml");
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

constexpr int kValley = 0, kSudbury = 1;

}  // namespace

TEST_CASE("pump_frame: scope is deep-only by default, one dial extends it") {
    render::PumpFrameStyle s{};  // shipped defaults
    REQUIRE(s.enabled);
    REQUIRE(s.deep_only);

    // Chad's literal ask: the STOPE pumps. The surface pump look is untouched.
    CHECK(render::pump_frame_applies(s, /*surface=*/false));
    CHECK_FALSE(render::pump_frame_applies(s, /*surface=*/true));

    // The dial that extends it everywhere, if he later wants it.
    s.deep_only = false;
    CHECK(render::pump_frame_applies(s, /*surface=*/false));
    CHECK(render::pump_frame_applies(s, /*surface=*/true));

    // The master switch beats the scope dial in BOTH arms — a "disabled" that
    // still drew on one path would be the silent-disarm class inverted.
    s.enabled = false;
    CHECK_FALSE(render::pump_frame_applies(s, /*surface=*/false));
    CHECK_FALSE(render::pump_frame_applies(s, /*surface=*/true));
    s.deep_only = true;
    CHECK_FALSE(render::pump_frame_applies(s, /*surface=*/false));
}

// ★★★ THIS CASE WAS INVERTED BY A RULING, NOT BY A BUG FIX -- and the history
// is kept here on purpose, because the pendulum has now swung both ways and the
// next agent deserves to know that.
//
// It used to be named "the frame is player-relative, never faction-absolute",
// and its own comment recorded that the ORIGINAL 3D pump tint had been
// faction-absolute and was changed to player-relative so an allied pump would
// not "read the wrong way round for one of the two sides".
//
// Chad flew a Sudbury player for the first time on 2026-09-10 -- the spawn
// menu's new side stage is what finally made that reachable -- and reported:
// "I picked central city but my spawn was over the valley and the valley was
// then orange." His RULING, verbatim: "Sudbury always has to be the orange
// team." So the hue belongs to the GROUND. The earlier reasoning is not wrong
// about what it observed; it answered "which pump is mine" with COLOUR, and
// the map already answers that with SHAPE and with its FIX/ATTACK labels,
// which is where a player-relative question belongs.
//
// ⚠ The shipped Valley player sees no change: for player_faction = VALLEY the
// two laws agree on every faction. Both halves are asserted below.
TEST_CASE("pump_frame: the frame is faction-ABSOLUTE (Chad 2026-09-10)") {
    const render::PumpFrameStyle s{};
    const glm::dvec3 ally = render::kComplementBlue;
    const glm::dvec3 enemy = render::kSlagOrange;

    // Flying VALLEY -- unchanged, bit for bit, from what shipped.
    CHECK(render::pump_frame_color(s, kValley, kValley, true, ally, enemy) ==
          ally);
    CHECK(render::pump_frame_color(s, kSudbury, kValley, true, ally, enemy) ==
          enemy);
    // Flying SUDBURY -- these two are the INVERSION. The Valley pump stays
    // blue and the Sudbury pump stays orange, because a pump does not change
    // colour according to who is looking at it.
    CHECK(render::pump_frame_color(s, kValley, kSudbury, true, ally, enemy) ==
          ally);
    CHECK(render::pump_frame_color(s, kSudbury, kSudbury, true, ally, enemy) ==
          enemy);
    // THE INVARIANT, stated once rather than as four cases: the player
    // argument cannot change the answer.
    for (int pf : {kValley, kSudbury}) {
        CHECK(render::pump_frame_color(s, kValley, pf, true, ally, enemy) ==
              render::pump_frame_color(s, kValley, kValley, true, ally,
                                       enemy));
        CHECK(render::pump_frame_color(s, kSudbury, pf, true, ally, enemy) ==
              render::pump_frame_color(s, kSudbury, kValley, true, ally,
                                       enemy));
    }
}

TEST_CASE("pump_frame: a dead or unowned pump never inherits a team hue") {
    const render::PumpFrameStyle s{};
    const glm::dvec3 ally = render::kComplementBlue;
    const glm::dvec3 enemy = render::kSlagOrange;

    // DEAD. The frame is an ownership CLAIM; a destroyed pump has no owner, so
    // keeping the last owner's colour would be exactly the stale-ownership lie
    // this mechanism exists to remove.
    CHECK(render::pump_frame_color(s, kValley, kValley, /*alive=*/false, ally,
                                   enemy) == s.neutral_color);
    CHECK(render::pump_frame_color(s, kSudbury, kValley, /*alive=*/false, ally,
                                   enemy) == s.neutral_color);

    // OUT-OF-RANGE faction. combat::make_pumps only ever emits 0/1 today, so
    // this arm is defensive — but a future neutral/capturable pump must fall to
    // grey by construction rather than index a team colour.
    CHECK(render::pump_frame_color(s, -1, kValley, true, ally, enemy) ==
          s.neutral_color);
    CHECK(render::pump_frame_color(s, 2, kValley, true, ally, enemy) ==
          s.neutral_color);

    // And the neutral grey is genuinely neither team (not a near-miss of one).
    CHECK(s.neutral_color != ally);
    CHECK(s.neutral_color != enemy);
}

TEST_CASE("load_world: [pump_frame] loads and the shipped table is legal") {
    const cfg::WorldParams w =
        cfg::load_world_toml(SEADS_CONFIG_DIR "/world.toml");
    CHECK(w.pump_frame.enabled);
    CHECK(w.pump_frame.deep_only);
    CHECK(w.pump_frame.layers >= 1);
    // The invariant the whole no-z-fight argument rests on: EVERY shell,
    // not just the outermost, stands clear of the pump body.
    CHECK(w.pump_frame.half_extent_m -
              static_cast<double>(w.pump_frame.layers - 1) *
                  w.pump_frame.layer_step_m >
          render::kDeepPumpShellM);
}

TEST_CASE("load_world: [pump_frame] rejects a frame inside the pump body") {
    const std::string base = slurp(SEADS_CONFIG_DIR "/world.toml");
    REQUIRE(base.find("half_extent_m = 40.0") != std::string::npos);

    // A cube smaller than the beacon shell would put its edges INSIDE the
    // sphere — coincident-depth wire on a curved surface, the z-fight the
    // mechanism's construction claims is impossible. Rejected loud.
    const std::string bad =
        replace_all(base, "half_extent_m = 40.0", "half_extent_m = 20.0");
    CHECK_THROWS(cfg::load_world_toml(write_temp(bad, "inside")));

    // The subtler one: the OUTER shell clears the body but the innermost does
    // not. A bound written only against half_extent_m would pass this.
    const std::string bad2 = replace_all(
        replace_all(
            replace_all(base, "half_extent_m = 40.0", "half_extent_m = 27.0"),
            "layers        = 3", "layers        = 3"),
        "layer_step_m  = 0.4", "layer_step_m  = 1.0");
    CHECK_THROWS(cfg::load_world_toml(write_temp(bad2, "inner")));

    // Sanity: the shipped table itself is NOT throwing (so the legs above are
    // not vacuously green on some unrelated parse error).
    CHECK_NOTHROW(cfg::load_world_toml(write_temp(base, "shipped")));
}
