#pragma once

#include <string>

// The WEATHER SEASON (docs/weather_seasons_plan.md W1) — the framing layer for
// the localized weather (W2) and precipitation (W3): Winter => snow, Spring =>
// rain, Summer/Autumn => dry. DISTINCT from the [celestial] declination season
// (the sun's axial-tilt swing over the year) — do not conflate the two.
//
// SEAM (SPEC §6/§9, greppable): the active season is CHOSEN APP-SIDE by a single
// RANDOM DRAW at spawn (app/main.cpp, an app-local mt19937 — NOT in sim/control,
// NOT a function of t_cel, NOT from a clock), with a config static override and
// an env override for building/smoke determinism. render/ only READS the season
// (like the celestial wheel); it never rolls dice. Pure std::string only — no
// raylib — so this lives in seads_render_core and the gate pins it headlessly.

namespace render {

// The four weather seasons. Fixed integer order so a config static-season index
// and the app-side weighted draw agree on the mapping.
enum class Season { Winter = 0, Spring = 1, Summer = 2, Autumn = 3 };
constexpr int kSeasonCount = 4;

// Canonical lowercase name ("winter"/"spring"/"summer"/"autumn"). Total (every
// enumerator maps); used by the config loader, the HUD tag, and the tests.
const char* season_name(Season s);

// Parse a season name (case-insensitive; leading/trailing spaces ignored).
// Returns true and sets `out` on a match; false on no match — the caller owns
// the "random" sentinel and the error policy (config throws, env falls through).
bool season_from_string(const std::string& s, Season& out);

// Resolve the active season from the app-side precedence (Fable-before P1-6):
// env override > config static > deterministic smoke/probe default (Summer) > a
// weighted RANDOM draw. `draw` is invoked ONLY on the random branch, so the
// smoke/probe path never samples the RNG (the determinism invariant), and must
// return an index in [0, kSeasonCount). Pure + header-inline (a template so the
// pure core needs no <functional>) so the precedence — which the app-binary-
// blind gate cannot otherwise reach — gets a unit test (season.cpp has no state).
template <class Draw>
inline Season resolve_season(bool have_env, Season env_season, int static_index,
                             bool smoke, Draw&& draw) {
    if (have_env) return env_season;                    // env SEADS_SEASON
    if (static_index >= 0)                              // [seasons] static_season
        return static_cast<Season>(static_index);
    if (smoke) return Season::Summer;                   // deterministic smoke/probe
    return static_cast<Season>(draw());                 // weighted random draw
}

}  // namespace render
