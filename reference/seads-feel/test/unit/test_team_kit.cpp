// ★★★ THE TEAM KIT (render/team_kit.h) — Chad 2026-09-10, verbatim:
//
//   "for game loop we need to allow player to choose 'central city' or
//    'valley' spawn for the teams. Make the color codes opposite for central
//    city: so plane would be the coded query-able slag orange, the scarf will
//    be orange, smoke blue for plane, helmet color blue for central city."
//
// TWO THINGS ARE PINNED HERE, AND THEY PULL IN OPPOSITE DIRECTIONS ON PURPOSE.
//
//  (1) VALLEY IS EXACTLY WHAT SHIPPED. Before the kit existed, the player's
//      four identity colours had four separate authorities: the livery read
//      render::team_colors().ally, the scarf and the helmet band came out of
//      the GLB materials `sudburian_scarf_blue` and `helmet_orange`, and the
//      wingtip trail was three hand-typed literals in render/draw.cpp. Every
//      one of those values is re-asserted below against the NAMED constant it
//      was, so "the default side is unchanged" is measured, not promised.
//
//  (2) CENTRAL CITY IS THE INVERSE, NOT A NEW PALETTE. Its plane and scarf are
//      render::kSlagOrange -- the same symbol the lava pour and the enemy
//      faction read, which is what "the coded query-able slag orange" means --
//      and its helmet is kComplementBlue. Its smoke is the Valley ramp rotated
//      180 deg in hue at UNCHANGED saturation and value, the same derivation
//      that produced kComplementBlue from kSlagOrange.
//
// ⚠ The equality checks are EXACT (==), not Approx. A kit entry that is a
// hand-copied near-miss of a named constant is the exact defect this file
// exists to catch, and Approx would let 0.12 vs 0.1200001 through.

#include <catch2/catch_test_macros.hpp>
#include <cmath>

#include "render/team_color.h"
#include "render/team_kit.h"

namespace {

// The three wingtip-smoke stops as render/draw.cpp carried them before the
// kit. Typed here INDEPENDENTLY of the header so the test is a second witness
// to the values, not an echo of them.
constexpr glm::dvec3 kShippedSmokeFresh{1.00, 0.45, 0.05};
constexpr glm::dvec3 kShippedSmokeAged{1.00, 0.18, 0.02};
constexpr glm::dvec3 kShippedSmokeSlag{1.00, 0.72, 0.18};

bool exactly(const glm::dvec3& a, const glm::dvec3& b) {
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

}  // namespace

TEST_CASE("TK1 VALLEY is the shipped set, byte for byte", "[team_kit]") {
    const render::TeamKit k = render::team_kit(render::kTeamValley);
    // The livery: render::team_colors().ally, which is kComplementBlue by
    // default and is pinned to the same numbers in config/world.toml [teams]
    // (test_load_world).
    CHECK(exactly(k.plane, render::kComplementBlue));
    // The scarf: the GLB material `sudburian_scarf_blue`, written by
    // patch_scarf.py FROM kComplementBlue (render/sled_model.cpp's banner).
    CHECK(exactly(k.scarf, render::kComplementBlue));
    // The helmet band: the GLB material `helmet_orange`, pinned to kSlagOrange
    // by test_rider_pose's H8 case.
    CHECK(exactly(k.helmet, render::kSlagOrange));
    // The trail: render/draw.cpp's three literals.
    CHECK(exactly(k.smoke_fresh, kShippedSmokeFresh));
    CHECK(exactly(k.smoke_aged, kShippedSmokeAged));
    CHECK(exactly(k.smoke_slag, kShippedSmokeSlag));
}

TEST_CASE("TK2 VALLEY is the default: no setter call, shipped colours",
          "[team_kit]") {
    // Nothing in this binary calls set_player_team(), so every test, golden
    // and headless run sees the side the game shipped with.
    CHECK(render::player_team() == render::kTeamValley);
    CHECK(exactly(render::player_kit().plane, render::kComplementBlue));
    CHECK(exactly(render::player_kit().helmet, render::kSlagOrange));
}

TEST_CASE("TK3 CENTRAL CITY: plane and scarf ARE the named slag orange",
          "[team_kit]") {
    const render::TeamKit k = render::team_kit(render::kTeamCentralCity);
    // "plane would be the coded query-able slag orange" -- the SAME symbol the
    // lava pour injects into its shader and the enemy faction wears, not a
    // copied triple that happens to match today.
    CHECK(exactly(k.plane, render::kSlagOrange));
    CHECK(exactly(k.scarf, render::kSlagOrange));
    // "helmet color blue for central city"
    CHECK(exactly(k.helmet, render::kComplementBlue));
    // And it really is the constant, not a look-alike: the pair must still
    // satisfy the executable complement law (exact 180 deg, equal S, equal V).
    CHECK(render::is_exact_complement(k.plane, k.helmet));
}

TEST_CASE("TK4 CENTRAL CITY is the INVERSE of VALLEY, surface for surface",
          "[team_kit]") {
    const render::TeamKit v = render::team_kit(render::kTeamValley);
    const render::TeamKit c = render::team_kit(render::kTeamCentralCity);
    CHECK(exactly(c.plane, v.helmet));   // orange where Valley is blue
    CHECK(exactly(c.scarf, v.helmet));
    CHECK(exactly(c.helmet, v.plane));   // blue where Valley is orange
    // No surface keeps its Valley colour: the coding is opposite, not partial.
    CHECK_FALSE(exactly(c.plane, v.plane));
    CHECK_FALSE(exactly(c.scarf, v.scarf));
    CHECK_FALSE(exactly(c.helmet, v.helmet));
    CHECK_FALSE(exactly(c.smoke_fresh, v.smoke_fresh));
}

TEST_CASE("TK5 CENTRAL CITY smoke is BLUE: the warm ramp, hue-flipped",
          "[team_kit]") {
    const render::TeamKit v = render::team_kit(render::kTeamValley);
    const render::TeamKit c = render::team_kit(render::kTeamCentralCity);
    const glm::dvec3 vs[3] = {v.smoke_fresh, v.smoke_aged, v.smoke_slag};
    const glm::dvec3 cs[3] = {c.smoke_fresh, c.smoke_aged, c.smoke_slag};
    for (int i = 0; i < 3; ++i) {
        const render::Hsv a = render::rgb_to_hsv(vs[i]);
        const render::Hsv b = render::rgb_to_hsv(cs[i]);
        // Exactly opposite on the wheel...
        CHECK(std::fabs(render::hue_separation_deg(a.h, b.h) - 180.0) < 1e-9);
        // ...at the SAME saturation and value, so the trail keeps the tonal
        // ramp that makes it read as a pour instead of a flat blue tube.
        CHECK(std::fabs(a.s - b.s) < 1e-12);
        CHECK(std::fabs(a.v - b.v) < 1e-12);
        // And it is BLUE, not merely "not orange": every stop lands in the
        // cyan-to-blue arc, and blue is the dominant channel.
        CHECK(b.h > 180.0);
        CHECK(b.h < 260.0);
        CHECK(cs[i].z > cs[i].x);
        CHECK(cs[i].z > cs[i].y);
    }
    // The warm side is the mirror statement: every Valley stop is red-dominant.
    for (int i = 0; i < 3; ++i) {
        CHECK(vs[i].x > vs[i].y);
        CHECK(vs[i].x > vs[i].z);
    }
}

TEST_CASE("TK6 the kit follows the LIVE palette, so it cannot fork from it",
          "[team_kit]") {
    // A config that moved [teams] would move the map markers and the name
    // tags; the kit must move with them rather than keeping a private copy of
    // the hues. Passed explicitly here -- the global palette is never mutated
    // by a test, so no other case can be disturbed.
    render::TeamColors p;
    p.ally = glm::dvec3{0.20, 0.40, 0.90};
    p.enemy = glm::dvec3{0.90, 0.50, 0.10};
    const render::TeamKit v = render::team_kit(render::kTeamValley, p);
    const render::TeamKit c = render::team_kit(render::kTeamCentralCity, p);
    CHECK(exactly(v.plane, p.ally));
    CHECK(exactly(v.helmet, p.enemy));
    CHECK(exactly(c.plane, p.enemy));
    CHECK(exactly(c.helmet, p.ally));
}

TEST_CASE("TK7 the session's side is a single object, set once", "[team_kit]") {
    // The setter exists so app/main.cpp can answer the spawn menu's side stage
    // once and have every draw site agree. Restored to VALLEY at the end so
    // the default this file's other cases assert is not disturbed.
    REQUIRE(render::player_team() == render::kTeamValley);
    render::set_player_team(render::kTeamCentralCity);
    CHECK(render::player_team() == render::kTeamCentralCity);
    CHECK(exactly(render::player_kit().plane, render::kSlagOrange));
    CHECK(exactly(render::player_kit().scarf, render::kSlagOrange));
    CHECK(exactly(render::player_kit().helmet, render::kComplementBlue));
    CHECK(render::player_kit().smoke_fresh.z >
          render::player_kit().smoke_fresh.x);
    render::set_player_team(render::kTeamValley);
    REQUIRE(render::player_team() == render::kTeamValley);
}

TEST_CASE("TK8 the side int IS the faction int (one mapping, not two)",
          "[team_kit]") {
    // app/main.cpp hands the spawn-menu row straight to both
    // render::set_player_team() and combat::ConquestState::player_faction.
    // combat::CQ_VALLEY == 0 and CQ_SUDBURY == 1 (combat/conquest.h), and
    // world::VALLEY/SUDBURY are the same pair by construction. This pins the
    // third member of that identity so the menu row cannot drift off it.
    static_assert(render::kTeamValley == 0, "VALLEY is faction 0");
    static_assert(render::kTeamCentralCity == 1, "CENTRAL CITY is faction 1");
    CHECK(render::kTeamValley == 0);
    CHECK(render::kTeamCentralCity == 1);
}

// ---------------------------------------------------------------------------
// ★★★ THE RULING (Chad 2026-09-10, verbatim): "Sudbury always has to be the
// orange team." He flew the first cut, picked Central City, and found the
// VALLEY painted orange under him -- because every world surface resolved its
// hue as `is_ours ? ally : enemy` against ConquestState::player_faction.
// These cases pin the hue to the GROUND, under BOTH player sides.
// ---------------------------------------------------------------------------

TEST_CASE("TK9 faction colour is ABSOLUTE: Valley blue, Sudbury orange",
          "[team_kit]") {
    CHECK(exactly(render::faction_color(render::kTeamValley),
                  render::kComplementBlue));
    CHECK(exactly(render::faction_color(render::kTeamCentralCity),
                  render::kSlagOrange));
}

TEST_CASE("TK10 the ground's hue does not move when the PLAYER's side does",
          "[team_kit]") {
    // THE DEFECT, EXECUTABLE. Under the old player-relative law these two
    // passes disagreed: choosing Central City made faction 0 read "enemy" and
    // turned the Valley orange. faction_color() takes no player argument at
    // all, so the passes cannot diverge -- but assert it over BOTH player
    // sides anyway, because the invariance IS the point of the case, and a
    // future signature that re-admits a player term must fail right here.
    for (int player : {render::kTeamValley, render::kTeamCentralCity}) {
        render::set_player_team(player);
        CHECK(exactly(render::faction_color(0), render::kComplementBlue));
        CHECK(exactly(render::faction_color(1), render::kSlagOrange));
    }
    render::set_player_team(render::kTeamValley);
    REQUIRE(render::player_team() == render::kTeamValley);
}

TEST_CASE("TK11 the SHIPPED Valley player sees exactly what he always saw",
          "[team_kit]") {
    // The OLD expression, re-typed here as an independent witness: every side
    // coding site read `faction == player_faction ? ally : enemy`. For the
    // shipped player_faction = VALLEY it must agree with the new absolute law
    // on EVERY faction -- that is what "bit-identical for the default side"
    // means, and it is the whole safety argument for this change.
    const render::TeamColors& p = render::team_colors();
    const int shipped_player = render::kTeamValley;
    for (int fac : {0, 1}) {
        const glm::dvec3 was = (fac == shipped_player) ? p.ally : p.enemy;
        CHECK(exactly(render::faction_color(fac), was));
    }
    // And for a SUDBURY player the two laws deliberately DISAGREE on both
    // factions -- that disagreement IS the defect Chad reported.
    const int cc_player = render::kTeamCentralCity;
    for (int fac : {0, 1}) {
        const glm::dvec3 old_law = (fac == cc_player) ? p.ally : p.enemy;
        CHECK_FALSE(exactly(render::faction_color(fac), old_law));
    }
}

TEST_CASE("TK12 the player's KIT is NOT re-interpreted by the absolute law",
          "[team_kit]") {
    // Chad specified the player's OWN four colours separately, and they stand:
    // a Central City player still flies a slag-orange plane with an orange
    // scarf, blue smoke and a blue helmet. Note his plane and his faction's
    // hue agree on both sides -- which is WHY a friendly maverick (drawn in
    // fp.player_color) needed no edit of its own.
    const render::TeamKit v = render::team_kit(render::kTeamValley);
    const render::TeamKit c = render::team_kit(render::kTeamCentralCity);
    CHECK(exactly(v.plane, render::faction_color(render::kTeamValley)));
    CHECK(exactly(c.plane, render::faction_color(render::kTeamCentralCity)));
    // The helmet stays the CONTRAST colour on both sides, by his words.
    CHECK(exactly(v.helmet, render::faction_color(render::kTeamCentralCity)));
    CHECK(exactly(c.helmet, render::faction_color(render::kTeamValley)));
}

// ---------------------------------------------------------------------------
// ★★★ THE SECOND REPORT (Chad, after flying the first absolute pass): "map and
// ally markers are still wrong color, other than that it looked right." The
// pump and gun rings had been routed; the CONTACT loop in map_screen.cpp and
// the friendly half of the in-world tag carried their OWN copies of the old
// player-relative law. These cases walk both decisions for a Valley drone and
// a Sudbury drone under BOTH player sides.
// ---------------------------------------------------------------------------

TEST_CASE("TK13 a map dot's COLOUR is its own faction's, under either player",
          "[team_kit]") {
    for (int player : {render::kTeamValley, render::kTeamCentralCity}) {
        // A VALLEY contact is blue whoever is reading the chart...
        const render::ContactStyle v = render::contact_style(0, player);
        CHECK_FALSE(v.neutral);
        CHECK(exactly(v.color, render::kComplementBlue));
        CHECK(exactly(v.color, render::faction_color(render::kTeamValley)));
        // ...and a SUDBURY contact is the slag orange.
        const render::ContactStyle s = render::contact_style(1, player);
        CHECK_FALSE(s.neutral);
        CHECK(exactly(s.color, render::kSlagOrange));
        CHECK(exactly(s.color,
                      render::faction_color(render::kTeamCentralCity)));
    }
    // Stated as the invariance it is: the player argument cannot move the ink.
    for (int fac : {0, 1})
        CHECK(exactly(render::contact_style(fac, 0).color,
                      render::contact_style(fac, 1).color));
}

TEST_CASE("TK14 a map dot's SHAPE stays player-relative", "[team_kit]") {
    // The shape language is how the chart still answers "is he mine" now that
    // hue cannot: circle = my side, square = the other side. This half MUST
    // depend on the player, and would be the defect if it ever stopped.
    CHECK_FALSE(render::contact_style(0, 0).square);  // Valley drone, Valley me
    CHECK(render::contact_style(1, 0).square);        // Sudbury drone, Valley me
    CHECK(render::contact_style(0, 1).square);        // Valley drone, CC me
    CHECK_FALSE(render::contact_style(1, 1).square);  // Sudbury drone, CC me
    // An unfactioned maverick is grey on either side, and its colour is not to
    // be read (the non-conquest pass never populates drones_faction).
    for (int player : {0, 1}) {
        CHECK(render::contact_style(-1, player).neutral);
        CHECK(render::contact_style(7, player).neutral);
    }
}

TEST_CASE("TK15 the in-world ALLY marker follows the drone's faction",
          "[team_kit]") {
    // render/draw.cpp's friendly tag paints with
    // render::faction_color(info.drones_faction[li]) -- so a friendly of the
    // SUDBURY side is orange, which is exactly what Chad found wrong. Walked
    // here for both drones under both player sides, the same shape as TK13.
    for (int player : {render::kTeamValley, render::kTeamCentralCity}) {
        render::set_player_team(player);
        CHECK(exactly(render::faction_color(0), render::kComplementBlue));
        CHECK(exactly(render::faction_color(1), render::kSlagOrange));
        // And the marker of a friendly agrees with the map dot of the same
        // aircraft -- one colour per side, on the chart and out the canopy.
        for (int fac : {0, 1})
            CHECK(exactly(render::faction_color(fac),
                          render::contact_style(fac, player).color));
    }
    render::set_player_team(render::kTeamValley);
    REQUIRE(render::player_team() == render::kTeamValley);
}

TEST_CASE("TK16 the SHIPPED Valley player's chart is unchanged", "[team_kit]") {
    // The old contact expression, re-typed as an independent witness:
    //     fac == player_fac ? c_ally : c_enemy
    // For the shipped player it must agree with the new style on both
    // factions -- shape AND colour -- so nothing moved for him.
    const render::TeamColors& p = render::team_colors();
    const int shipped = render::kTeamValley;
    for (int fac : {0, 1}) {
        const glm::dvec3 was = (fac == shipped) ? p.ally : p.enemy;
        const render::ContactStyle now = render::contact_style(fac, shipped);
        CHECK(exactly(now.color, was));
        CHECK(now.square == (fac != shipped));
    }
}
