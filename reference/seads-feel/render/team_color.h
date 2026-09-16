#pragma once

// TEAM COLOUR — the ONE source of the two faction hues (S-mapteam, Chad
// 2026-08-09):
//
//   "Make the enemies the slag orange color we use across this codebase and
//    make the allies switch back to an opposite blue functionally and with
//    precision the opposite of blue on the color wheel of the orange slag
//    color and verify this is right. Then go and make sure all allies planes
//    and their name tag are colored blue in map and in game. And the [enemy]
//    planes the same slag orange code for the in game planes, map icons and
//    their enemy name tags"
//
// WHY THIS FILE EXISTS (CLAUDE.md's H1 single-source-or-fork rule). The enemy
// hue is the SLAG ORANGE — literally the `hot` stop of heatColor() in
// render/slag.cpp, the codebase's one sanctioned warm-chroma exception. Before
// this header that value lived only inside the lava GLSL string, so painting a
// second surface with "the slag orange" meant retyping the literal — a fork
// waiting to drift. Now the lava shader and the team palette read the SAME
// constant: render/slag.cpp injects kSlagOrange into its fragment source as
// `#define SLAG_HOT`, and every faction-coloured pixel (map markers, map
// airspace tags, the 3D aircraft livery, the in-game ALLY/BANDIT tags, Chad's
// own callsign tag) reads team_colors().
//
// THE ALLY HUE IS DERIVED, NOT PICKED. Chad asked for "with precision the
// opposite ... on the color wheel", i.e. the exact 180 deg HSV complement at
// the SAME saturation and value:
//
//   slag orange (1.00, 0.55, 0.12) -> HSV (29.3181818 deg, 0.88, 1.00)
//   + 180 deg                      -> HSV (209.3181818 deg, 0.88, 1.00)
//                                  -> RGB (0.12, 0.57, 1.00)   EXACTLY
//
// (The arithmetic closes exactly: C = V*S = 0.88, X = C*(1 - |209.318/60 mod 2
// - 1|) = 0.88 * 0.5113636... = 0.45, m = V - C = 0.12, sector 3 => (0, X, C)
// + m = (0.12, 0.57, 1.00). No rounding is hidden in the constant.)
//
// ⚠ THE TRAP, recorded so nobody "fixes" it: the eye-plausible
// channel-reversal (0.12, 0.55, 1.00) is NOT the complement — its hue is
// 210.68 deg, i.e. 181.36 deg away. The 1.36 deg error is invisible on screen
// and would quietly make "precisely opposite" a lie. The invariant is
// therefore EXECUTABLE, not a comment: test/unit/test_team_color.cpp converts
// both colours to HSV and requires hue separation == 180 deg (1e-9), equal S
// and equal V; config/load_world.cpp runs the SAME predicate over the loaded
// [teams] table, so a hand-edited config that breaks the complement fails
// loud at startup instead of shipping.
//
// COLOUR-BLIND NOTE: orange/blue is the safest common pairing (it survives
// deuteranopia and protanopia, unlike the green/red it replaces). The map's
// shape language (ally circle / enemy square / objective pump glyph / player
// arrow) is kept anyway — hue is never the only cue.
//
// Raylib-free (glm + std only) so config/, tests and the harness may include
// it; header-only so it needs no TU of its own.

#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>

namespace render {

// ---------------------------------------------------------------------------
// The canonical constants.
// ---------------------------------------------------------------------------

// SLAG ORANGE = the `hot` stop of render/slag.cpp's heatColor(). Changing this
// changes the molten slag AND the enemy faction, by design — they are the same
// colour by Chad's ruling.
inline constexpr glm::dvec3 kSlagOrange{1.00, 0.55, 0.12};

// The exact 180 deg HSV complement of kSlagOrange at equal S and V. Derived,
// never eyeballed — see the banner and test_team_color.cpp.
inline constexpr glm::dvec3 kComplementBlue{0.12, 0.57, 1.00};

// MEDICAL-OXYGEN GREEN — the objective (pump) hue. Chad: "make the pump
// symbols green the same green used for oxygen bottles in an ambulance".
//
// SOURCE: in North America the colour marking of medical gas containers and
// their pipe/label markers is CGA Pamphlet C-9 (adopted by NFPA 99C), which
// assigns GREEN to oxygen — that is the green on an ambulance O2 cylinder.
// C-9 names the colour but does not publish a Pantone; the de-facto ink used
// by medical-gas marker printers is the ANSI Z535.1 safety green, PANTONE
// 348 C = #00843D. That is this value: (0, 132, 61)/255 = (0.000, 0.518,
// 0.239). Documented, not invented.
//
// It is also the right choice for the plate: luma ~0.39 sits well BELOW both
// team hues (ally blue ~0.51, slag orange ~0.61), so the objective separates
// from both teams in LIGHTNESS as well as hue, and reads dark against the 0.90
// newsprint paper.
inline constexpr glm::dvec3 kOxygenGreen{0.000, 0.518, 0.239};

// ---------------------------------------------------------------------------
// HSV — the executable form of "opposite on the colour wheel".
// ---------------------------------------------------------------------------

struct Hsv {
    double h = 0.0;  // hue in DEGREES [0,360)
    double s = 0.0;  // saturation [0,1]
    double v = 0.0;  // value [0,1]
};

inline Hsv rgb_to_hsv(const glm::dvec3& c) {
    const double mx = std::max(c.x, std::max(c.y, c.z));
    const double mn = std::min(c.x, std::min(c.y, c.z));
    const double d = mx - mn;
    Hsv o;
    o.v = mx;
    o.s = (mx > 0.0) ? d / mx : 0.0;
    if (d <= 0.0) return o;  // grey: hue undefined, reported as 0
    if (mx == c.x)
        o.h = 60.0 * std::fmod((c.y - c.z) / d + 6.0, 6.0);
    else if (mx == c.y)
        o.h = 60.0 * ((c.z - c.x) / d + 2.0);
    else
        o.h = 60.0 * ((c.x - c.y) / d + 4.0);
    return o;
}

// Shortest angular separation of two hues, in degrees, in [0,180].
inline double hue_separation_deg(double a_deg, double b_deg) {
    double d = std::fmod(std::fabs(a_deg - b_deg), 360.0);
    return d > 180.0 ? 360.0 - d : d;
}

// The complement predicate: exactly opposite hue, same saturation, same value.
// `tol` is in degrees for the hue and in absolute units for S/V.
inline bool is_exact_complement(const glm::dvec3& a, const glm::dvec3& b,
                                double tol = 1e-9) {
    const Hsv ha = rgb_to_hsv(a), hb = rgb_to_hsv(b);
    return std::fabs(hue_separation_deg(ha.h, hb.h) - 180.0) <= tol &&
           std::fabs(ha.s - hb.s) <= tol && std::fabs(ha.v - hb.v) <= tol;
}

// The inverse of rgb_to_hsv. Standard sector reconstruction; exact for the
// values rgb_to_hsv produces (round-trip within 1e-12), so the E4.2 tint below
// can be authored in HSV — the space Chad's ask ("deeper saturated red") is
// actually stated in — instead of as an eyeballed RGB triple.
inline glm::dvec3 hsv_to_rgb(const Hsv& h) {
    const double s = std::min(1.0, std::max(0.0, h.s));
    const double v = std::min(1.0, std::max(0.0, h.v));
    const double hh = std::fmod(std::fmod(h.h, 360.0) + 360.0, 360.0) / 60.0;
    const double c = v * s;
    const double x = c * (1.0 - std::fabs(std::fmod(hh, 2.0) - 1.0));
    const double m = v - c;
    double r = 0.0, g = 0.0, b = 0.0;
    const int sector = static_cast<int>(hh);  // 0..5 (hh < 6 by construction)
    switch (sector) {
        case 0: r = c; g = x; break;
        case 1: r = x; g = c; break;
        case 2: g = c; b = x; break;
        case 3: g = x; b = c; break;
        case 4: r = x; b = c; break;
        default: r = c; b = x; break;
    }
    return glm::dvec3{r + m, g + m, b + m};
}

// ---------------------------------------------------------------------------
// E4.2 — THE ENEMY LEGIBILITY TINT (rung E4, Chad 2026-08-20: "they are hard
// to see especially in the white scatter light, their tag and color is also a
// bit hard to see").
// ---------------------------------------------------------------------------
//
// THE PROBLEM, STATED IN NUMBERS. The slag orange is HSV(29.3 deg, 0.88, 1.00)
// — V = 1.00, i.e. its brightest channel is FULL WHITE. Against the winter
// world's blown-out white scatter sky that buys almost no luminance separation:
// the plane reads as a warm smudge in warm haze. Rotating the hue toward RED
// and easing V off the ceiling restores luminance contrast against white while
// RAISING saturation, so the S1 split-tone chroma gate (uSatC0/uSatC1,
// render/post_glsl.cpp) still passes it as colour in an otherwise mono frame.
//
// ONE AUTHORITY, NOT A SECOND LITERAL. This is a DERIVED accent computed from
// team_colors().enemy — the ruled base hue is still the single source. Two
// consumers deliberately keep reading team_colors().enemy UNSHIFTED:
//   * render/slag.cpp — the lava IS the slag colour by Chad's 2026-08-09
//     ruling; retinting the pour to fix an aircraft is the tail wagging the dog.
//   * the tactical map — 0.90 newsprint has the OPPOSITE contrast problem from
//     a white sky, and the map plate was signed as it stands.
// The shift is applied only where an aircraft is painted against the sky: the
// 3D livery (app/main.cpp rig_bandit_color) and its in-game BANDIT tag
// (render/draw.cpp), both through THIS function, never by hand.
//
// COLOUR-BLIND NOTE (extending the banner above): red/blue is still a safe
// pair. Under protanopia/deuteranopia the red side darkens rather than
// converging on the blue, so hue AND lightness both still separate the teams;
// the map's shape language is untouched either way.
inline constexpr double kEnemyLegibleHueDeg = 8.0;  // deep red (slag = 29.32)
inline constexpr double kEnemyLegibleSat = 0.95;    // slag S = 0.88
inline constexpr double kEnemyLegibleVal = 0.98;    // slag V = 1.00

// Blend `base` toward the legibility target in HSV by `shift` in [0,1].
// shift <= 0 returns `base` BIT-IDENTICALLY (early return, no round-trip) —
// that is the knob-off contract for [plane_legibility] enemy_tint_shift, and
// test_plane_legibility pins it. shift = 1 is the full deep red.
inline glm::dvec3 enemy_legibility_tint(const glm::dvec3& base, double shift) {
    if (!(shift > 0.0)) return base;  // NaN-safe: also returns base
    const double t = std::min(1.0, shift);
    const Hsv b = rgb_to_hsv(base);
    // Hue interpolates the SHORT way round so a base already past 180 deg can
    // never take the long way through green.
    double dh = kEnemyLegibleHueDeg - b.h;
    if (dh > 180.0) dh -= 360.0;
    if (dh < -180.0) dh += 360.0;
    Hsv o;
    o.h = b.h + dh * t;
    o.s = b.s + (kEnemyLegibleSat - b.s) * t;
    o.v = b.v + (kEnemyLegibleVal - b.v) * t;
    return hsv_to_rgb(o);
}

// ---------------------------------------------------------------------------
// The live palette (set once at startup from config/world.toml [teams]).
// ---------------------------------------------------------------------------

struct TeamColors {
    glm::dvec3 ally{kComplementBlue};    // Chad's side, whichever faction it is
    glm::dvec3 enemy{kSlagOrange};       // the other side
    glm::dvec3 objective{kOxygenGreen};  // pumps (neither team by design)
};

// Header-only single instance: `inline` gives one object across every TU that
// includes this, so the map layer, the world draw and the slag shader can
// never read two different tables. Tests/harness never call the setter, so
// they see the derived defaults above.
inline TeamColors& mutable_team_colors() {
    static TeamColors t{};
    return t;
}
inline void set_team_colors(const TeamColors& t) { mutable_team_colors() = t; }
inline const TeamColors& team_colors() { return mutable_team_colors(); }

}  // namespace render
