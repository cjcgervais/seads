#pragma once

// PUMP OWNERSHIP FRAME — the neon team wire cube around a pump (S-pumpcube,
// Chad 2026-08-09, verbatim):
//
//   "the green pumps inside the black stope need a wire cube that is neon slag
//    and opposing blue of the same team color codes to frame in that pump
//    otherwise they dont apper claimed by either side in the black stope"
//
// THE DEFECT. S-mapteam moved pump OWNERSHIP off the body and onto the beacon
// (map: an owner ring; world: the tall beacon column + lamp). That works on the
// SURFACE, where the beacon column is 450 m of team colour against the sky. The
// two DEEP pumps have no beacon at all — the arena is a sealed, unlit rock
// chamber, so a deep pump is a green sphere in the black and nothing on it says
// whose it is. Chad is reporting exactly that.
//
// THE FIX, and why "neon" is load-bearing. The frame is a WIRE CUBE around the
// pump body in the owner's team colour, drawn ADDITIVELY with depth-WRITE off:
// self-lit, so it needs no sun and no lamp (a lit/shaded wireframe is invisible
// down there — that IS the defect being reported), and additive-over-black can
// only ever brighten, so it cannot be swallowed by the chamber. Thickness comes
// from `layers` nested shells `layer_step_m` apart, NOT from a line width:
// GL core profile clamps glLineWidth to 1, so a width-based "neon" silently
// ships as a hairline on exactly the hardware this runs on. Nested shells bloom
// additively up close and collapse to one bright line at range — which is the
// behaviour wanted at both ends.
//
// WHY A CUBE AND NOT A RING. Chad said cube, and a cube is the right answer for
// a volume you fly THROUGH: 12 edges means at least three of them are
// silhouetted from any approach, including from INSIDE the frame, where a
// coplanar ring would vanish edge-on.
//
// NO Z-FIGHTING BY CONSTRUCTION. `half_extent_m` is required (by the loader) to
// clear kDeepPumpShellM, so no cube edge is ever coincident with the pump body;
// depth-WRITE is off so the nested shells cannot fight each other; and the draw
// sits in the additive pass AFTER every opaque depth-writer, so the stope walls
// still occlude it correctly (depth-TEST stays on).
//
// SCOPE + THE NEUTRAL RULE (both decided here, not inherited):
//  * `deep_only` (default 1) honours the literal ask — stope pumps only, the
//    surface pump look is untouched. `deep_only = 0` frames every pump.
//  * A pump whose faction is neither of the two factions, AND a DEAD pump, draw
//    the frame in `neutral_color` (a dim grey), never in a team hue. Today
//    combat::make_pumps only ever emits faction 0/1 so the neutral arm is
//    defensive — but a destroyed pump is genuinely unowned, and painting its
//    frame in the last owner's colour would be a stale-ownership lie of exactly
//    the kind Chad is complaining about.
//
// Raylib-free (glm + std only) so config/ and the tests may include it;
// header-only, same shape as render/team_color.h.

#include <glm/glm.hpp>

namespace render {

// The deep-pump beacon-shell radius (m) — the outer sphere render/draw.cpp
// draws for a stope pump. It lives HERE because the frame's size rule is
// "must clear the body", and the loader enforces that: two copies of this
// number is the H1 fork CLAUDE.md forbids.
inline constexpr double kDeepPumpShellM = 26.0;

struct PumpFrameStyle {
    bool enabled = true;    // master switch (0 = nothing drawn, no state churn)
    bool deep_only = true;  // Chad's literal ask; 0 = frame surface pumps too
    double half_extent_m = 40.0;  // cube half-edge (m); > kDeepPumpShellM
    int layers = 3;               // nested wire shells (the neon thickness)
    double layer_step_m = 0.8;    // radial spacing between shells (m)
    double brightness = 1.0;      // additive strength of the inner shell
    glm::dvec3 neutral_color{0.35, 0.35, 0.38};  // unowned / dead frame
};

// Set ONCE at startup from config/world.toml [pump_frame]. Header-only single
// instance (see team_color.h) so no TU can read a second table. Tests/harness
// never call the setter and see the defaults above.
inline PumpFrameStyle& mutable_pump_frame_style() {
    static PumpFrameStyle s{};
    return s;
}
inline void set_pump_frame_style(const PumpFrameStyle& s) {
    mutable_pump_frame_style() = s;
}
inline const PumpFrameStyle& pump_frame_style() {
    return mutable_pump_frame_style();
}

// Does pump (surface = `surface`) get a frame under this style? Pure, so the
// scope ruling is testable without a window.
inline bool pump_frame_applies(const PumpFrameStyle& s, bool surface) {
    return s.enabled && (!s.deep_only || !surface);
}

// The frame colour for a pump. `faction` is the pump's own faction; `alive`
// false or an out-of-range faction => the neutral grey (never a stale team
// hue). Pure.
//
// ★★★ ABSOLUTE BY FACTION (Chad's ruling 2026-09-10: "Sudbury always has to be
// the orange team"). This used to read `faction == player_faction ? ally :
// enemy` -- the frame said "mine / theirs", so a Sudbury player saw the Valley
// pumps in orange. VALLEY is the ally blue and SUDBURY the slag orange
// whichever side you fly for. `player_faction` is KEPT IN THE SIGNATURE and is
// deliberately unused: every caller already computes it, and dropping the
// parameter would silently re-order the two colour arguments at the call site
// -- the exact class of edit this codebase keeps getting bitten by. The
// shipped Valley player sees no change at all: for him the two predicates are
// the same predicate.
inline glm::dvec3 pump_frame_color(const PumpFrameStyle& s, int faction,
                                   int /*player_faction*/, bool alive,
                                   const glm::dvec3& ally,
                                   const glm::dvec3& enemy) {
    if (!alive || faction < 0 || faction > 1) return s.neutral_color;
    return (faction == 1) ? enemy : ally;  // 1 == SUDBURY == orange, always
}

}  // namespace render
