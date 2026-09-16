#pragma once

// ★★★ TEAM KIT — THE PLAYER'S SIDE, AND THE FOUR SURFACES THAT SAY SO
// (Chad 2026-09-10, verbatim):
//
//   "for game loop we need to allow player to choose 'central city' or
//    'valley' spawn for the teams. Make the color codes opposite for central
//    city: so plane would be the coded query-able slag orange, the scarf will
//    be orange, smoke blue for plane, helmet color blue for central city."
//
// WHY THIS FILE EXISTS. Before it, the player's four identity colours were
// FOUR SEPARATE AUTHORITIES that happened to agree:
//
//   plane   app/main.cpp          info.rig_player_color = team_colors().ally
//   scarf   the GLB asset         material `sudburian_scarf_blue`
//   smoke   render/draw.cpp       three hand-typed glm::vec3 literals
//   helmet  the GLB asset         material `helmet_orange`
//
// Flipping a side by editing four places is a fork waiting to happen: the day
// the plane turns orange and the smoke does not, nothing fails, it just looks
// wrong. So the four are ONE TABLE here, keyed on the side, and every draw
// site reads the table instead of its own literal.
//
// ★ THE HUES ARE NOT NEW. This header invents no colour. The two identity hues
// are render/team_color.h's kSlagOrange and its exact 180 deg HSV complement
// kComplementBlue, taken through the live palette (render::team_colors()) so
// the kit, the map markers, the name tags and the lava pour can never read two
// different tables. Central City is the Valley kit with ally and enemy
// exchanged — which is exactly what "make the color codes opposite" means.
//
// ★ THE SMOKE IS A THREE-STOP RAMP, NOT ONE COLOUR, so "smoke blue" cannot be
// a single constant: the trail reads as a pour because a fresh stop, an aged
// stop and a bright slag fleck differ in S and V. Central City therefore takes
// the SAME three stops rotated 180 deg in hue at UNCHANGED S and V — the same
// derivation kComplementBlue itself uses. The ramp survives; only the hue
// flips. Valley's three stops are the shipped literals, moved here byte for
// byte and pinned by test/unit/test_team_kit.cpp.
//
// ⚠ VALLEY IS BIT-IDENTICAL TO WHAT SHIPPED. That is the contract, and it is
// executable, not a promise: the test compares every Valley entry against the
// named constants the four old authorities used, and config/world.toml's
// [teams] table already carries those same numbers (test_load_world pins it).
//
// Raylib-free (glm + std only), header-only, exactly like team_color.h — so
// config/, the tests and the harness may include it.

#include <cmath>

#include "render/team_color.h"

namespace render {

// ---------------------------------------------------------------------------
// The two sides.
// ---------------------------------------------------------------------------
//
// VALLEY (Onaping/Levack) is combat::CQ_VALLEY == world::VALLEY == 0 and is
// the shipped default — `config/game.toml [conquest] player_faction`. CENTRAL
// CITY is the other side of that same 1:1 map, combat::CQ_SUDBURY == 1: the
// city in the middle of the basin, the side the slag pours on. The int values
// are deliberately the faction ints so app/main.cpp can hand one straight to
// `cq.state.player_faction` without a second lookup table to keep in step.
enum PlayerTeam { kTeamValley = 0, kTeamCentralCity = 1 };

// ---------------------------------------------------------------------------
// Valley's wingtip-smoke ramp — the three literals from render/draw.cpp's
// airshow trail (Chad 2026-08-06, "like bright hot slag"), moved here
// UNCHANGED so the draw has no colour of its own left to drift.
// ---------------------------------------------------------------------------
inline constexpr glm::dvec3 kSmokeFreshWarm{1.00, 0.45, 0.05};  // neon orange
inline constexpr glm::dvec3 kSmokeAgedWarm{1.00, 0.18, 0.02};   // hot red-orange
inline constexpr glm::dvec3 kSmokeSlagWarm{1.00, 0.72, 0.18};   // white-hot slag

// Rotate a colour 180 deg on the wheel at UNCHANGED saturation and value —
// the derivation that produced kComplementBlue from kSlagOrange, applied to an
// arbitrary stop. Round-trips through render/team_color.h's own HSV pair, so
// there is one conversion in the codebase, not two.
inline glm::dvec3 opposite_hue(const glm::dvec3& c) {
    Hsv h = rgb_to_hsv(c);
    h.h = std::fmod(h.h + 180.0, 360.0);
    return hsv_to_rgb(h);
}

// ---------------------------------------------------------------------------
// ★★★ FACTION COLOUR — ABSOLUTE, NOT PLAYER-RELATIVE.
// Chad's RULING, 2026-09-10, verbatim: "Sudbury always has to be the orange
// team." He flew the first cut of this rung, picked Central City, and reported:
// "I picked central city but my spawn was over the valley and the valley was
// then orange."
// ---------------------------------------------------------------------------
//
// THE DEFECT HE HIT, NAMED. render::TeamColors has two SLOTS, `ally` and
// `enemy`, and their banner says so outright -- "Chad's side, whichever faction
// it is". Every world surface that codes a side resolved it as
// `is_ours ? ally : enemy`, keyed on ConquestState::player_faction:
// render/map_screen.cpp's team_col lambda (whose own comment read "Team colour
// is PLAYER-RELATIVE, not VALLEY/SUDBURY-absolute"), the pump owner beacon and
// the flak-gun beacon in render/draw.cpp, render/pump_frame.h's frame colour.
// So the moment the spawn menu moved him to Sudbury, the Valley became "the
// other side" and turned orange -- exactly as he saw it. The bug was not in
// the new menu; the menu was the first thing that ever exercised the other
// branch of a decade-old relative mapping.
//
// THE LAW NOW. The hue belongs to the GROUND, not to the viewer:
//
//     VALLEY  (faction 0) is ALWAYS the ally blue.
//     SUDBURY (faction 1) is ALWAYS the slag orange.
//
// ⚠ AND IT IS BIT-IDENTICAL TO WHAT SHIPPED, because the shipped player IS
// Valley (config/game.toml [conquest] player_faction = "valley"): for him
// `is_ours ? ally : enemy` and `faction == SUDBURY ? enemy : ally` return the
// same colour for every faction, every time. Nothing moved for the default
// side; the other side simply stopped lying.
//
// ⚠ WHAT THIS IS *NOT*. It does not touch HOSTILITY. A furball can make a
// teammate shoot at you (DroneState::friendly_side), and the E4.2 legibility
// tint exists to make whoever is shooting at you visible against a white sky --
// at the shipped enemy_tint_shift = 1.0 the bandit livery is a DEEP RED that
// was never either faction hue. Colour of the ground = faction; the red of a
// hostile aircraft and the ALLY/BANDIT tag WORD = hostility. Those are two
// different questions and they keep two different cues.
//
// An out-of-range faction returns the neutral-safe ally blue rather than
// asserting: this is a draw path, and a bad index must not be a crash.
inline glm::dvec3 faction_color(int faction,
                                const TeamColors& p = team_colors()) {
    return faction == kTeamCentralCity ? p.enemy : p.ally;
}

// ---------------------------------------------------------------------------
// ★★★ A MAP CONTACT — SHAPE IS PLAYER-RELATIVE, COLOUR IS ABSOLUTE.
// ---------------------------------------------------------------------------
//
// Chad flew the first absolute pass and reported: "map and ally markers are
// still wrong color, other than that it looked right." The pump rings and the
// gun rings had been routed through the absolute resolver, but the CONTACT
// loop in render/map_screen.cpp carried its OWN second copy of the old law --
// `fac == player_fac ? c_ally : c_enemy` -- so every aircraft dot on the chart
// was still painted relative to the viewer.
//
// It lives HERE, pure, instead of as a lambda inside a raylib draw function,
// for the reason render/pump_frame.h's colour function does: a decision that
// cannot be tested is a decision that drifts back. `seads_tests` bans raylib
// across its include closure, so the ONLY way to pin a marker rule is to put
// the rule somewhere raylib cannot reach.
//
// The two questions a contact answers are deliberately kept apart:
//   SHAPE  "is he mine?"      -> player-relative, and rightly so. Circle for a
//                               friendly faction, square for the other -- the
//                               map's shape language, unchanged since S-mapteam.
//   COLOUR "whose ground?"    -> ABSOLUTE. Valley blue, Sudbury orange.
// An out-of-range faction (-1: a maverick with no side, the non-conquest pass)
// is NEUTRAL -- grey circle, and `color` must not be used.
struct ContactStyle {
    bool square = false;   // false = circle (a friendly faction), true = square
    bool neutral = false;  // true = unfactioned: grey, `color` is meaningless
    glm::dvec3 color{kComplementBlue};
};

inline ContactStyle contact_style(int faction, int player_faction,
                                  const TeamColors& p = team_colors()) {
    ContactStyle st;
    if (faction < 0 || faction > 1) {
        st.neutral = true;
        return st;
    }
    st.square = (faction != player_faction);
    st.color = faction_color(faction, p);
    return st;
}

// ---------------------------------------------------------------------------
// The kit: the four surfaces that carry the player's side.
// ---------------------------------------------------------------------------
struct TeamKit {
    glm::dvec3 plane{kComplementBlue};   // the aircraft livery (rig_player_color)
    glm::dvec3 scarf{kComplementBlue};   // the Sudburian's scarf prim
    glm::dvec3 helmet{kSlagOrange};      // the helmet's team band (helmet_orange)
    glm::dvec3 smoke_fresh{kSmokeFreshWarm};  // wingtip trail, stop 1 of 3
    glm::dvec3 smoke_aged{kSmokeAgedWarm};    // stop 2
    glm::dvec3 smoke_slag{kSmokeSlagWarm};    // stop 3 (the bright fleck)
};

// Build the kit for a side from the LIVE palette (defaulted, so a test or the
// harness gets the derived constants without touching global state).
//
//   VALLEY        plane/scarf = ally blue,   helmet = slag orange, smoke warm
//   CENTRAL CITY  plane/scarf = slag orange, helmet = ally blue,   smoke cool
//
// Note the helmet is the ODD ONE OUT on both sides, and always has been: the
// shipped Valley rider flies a blue plane in a blue scarf under an ORANGE
// helmet band, so the man is legible against his own machine. Chad's Central
// City list keeps that structure and inverts it whole.
inline TeamKit team_kit(int team, const TeamColors& p = team_colors()) {
    TeamKit k;
    if (team == kTeamCentralCity) {
        k.plane = p.enemy;   // the coded, query-able SLAG ORANGE
        k.scarf = p.enemy;
        k.helmet = p.ally;   // blue
        k.smoke_fresh = opposite_hue(kSmokeFreshWarm);
        k.smoke_aged = opposite_hue(kSmokeAgedWarm);
        k.smoke_slag = opposite_hue(kSmokeSlagWarm);
        return k;
    }
    k.plane = p.ally;
    k.scarf = p.ally;
    k.helmet = p.enemy;
    k.smoke_fresh = kSmokeFreshWarm;
    k.smoke_aged = kSmokeAgedWarm;
    k.smoke_slag = kSmokeSlagWarm;
    return k;
}

// ---------------------------------------------------------------------------
// The session's choice. Set ONCE from the spawn menu's side stage
// (app/spawn_menu.h -> app/main.cpp) and read by every draw site.
// ---------------------------------------------------------------------------
//
// Header-only single instance, the team_colors() pattern: one object across
// every TU, so the livery, the scarf, the helmet and the trail cannot end up
// on different sides. Defaults to VALLEY — the shipped behaviour — so a
// headless run, a golden and every test that never calls the setter see
// exactly what they saw before this file existed.
inline int& mutable_player_team() {
    static int t = kTeamValley;
    return t;
}
inline void set_player_team(int t) { mutable_player_team() = t; }
inline int player_team() { return mutable_player_team(); }
inline TeamKit player_kit() { return team_kit(player_team()); }

}  // namespace render
