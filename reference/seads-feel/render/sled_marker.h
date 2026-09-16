#pragma once

// THE MACHINE MARKER — R4a, Chad 2026-08-25:
//
//   "a marker marks the snowmachine that is easy to see and the sudburian can
//    run back to his snowmachine however far he was thrown verses where the
//    snowmachine stopped"
//
// When he is thrown, he and the machine come to rest in DIFFERENT places, and
// the run back is the felt cost of the throw (`docs/R4A_THROW_RULING_20260825.md`
// §2.4). That run is only playable if the machine announces itself from wherever
// he landed — hence a beacon, not a map pin.
//
// ★★ THE LOOK, RULED BY CHAD 2026-08-25 (verbatim):
//
//   "LETS GO WITH BLUE LIGHT FROM 10M UP SHINING DOWN ON SLED, NO BLINKING OR
//    ANYTHING, THE SLED WONT GO THAT FAR AWAY, CONE SHAPE"
//
// So: a BLUE CONE of light, apex 10 m above the machine, opening DOWNWARD onto
// it. Steady — no blink, no pulse, no animation of any kind, which also means
// no frame counter and no clock in here.
//
// Two earlier shapes were rejected on his eye and are recorded so they are not
// quietly re-imported:
//   1. The conquest pump's tall tapered COLUMN + lamp. From the chase camera a
//      36 m column at 0.45 m radius fills a third of the screen — a wall.
//   2. A bare floating LAMP with no column (his first ruling, superseded the
//      same session by this one once he saw the column go).
// This third shape reads as ILLUMINATION rather than an obstruction, which is
// why it survives where the column did not.
//
// ★★ REAL GEOMETRY ONLY. `DrawBillboard` renders NOTHING in this raylib build
// — a documented house trap that has bitten the wingtip smoke and the drone
// glint. The beacon is DrawCylinderEx + DrawSphere for that reason. Do not
// "simplify" it to a billboard.
//
// ⚠ THIS IS NOT YET WIRED TO A TRIGGER. `rider_attached` does not exist (R4a
// Phase 1 item 3) and the ON-FOOT state is R5+, "scoped, not yet specified"
// (`docs/SUDBURIAN_LADDER.md:1815`). Until the release latch lands, the beacon
// is driven by a debug toggle so Chad can rule on the LOOK. The trigger it
// wants is `!rider_attached`.

#include <glm/glm.hpp>

namespace render {

// ★ 10 m, CHAD'S NUMBER — and it SUPERSEDES a derivation, deliberately.
// The first build derived 36 m from the canopy the beacon would have to clear
// (`tree_height_m` 16.0 x `[trees] max_scale` 1.5 = 24 m of tallest tree).
// Chad ruled 10 m with the reason attached: "THE SLED WONT GO THAT FAR AWAY."
// He is right that the derivation answered the wrong question — the marker is
// read from tens of metres on foot, not across the canopy from the air, so
// clearing the treetops was never its job. HIS WORDS WIN over a measurement
// (CLAUDE.md: "If a measurement contradicts his words, HIS WORDS WIN"). The
// canopy figure is kept only so the superseded reasoning stays legible.
inline constexpr double kTallestTreeM = 16.0 * 1.5;  // 24 m — SUPERSEDED basis
inline constexpr double kMarkerHeightM = 10.0;       // Chad, 2026-08-25

// The cone's APEX is the light source and its BASE pools on the machine. The
// base radius is taken from the machine's own footprint rather than picked: the
// chassis is 1.70 m long (`render/draw.cpp`, the placeholder hull box), so a
// 1.2 m radius pool covers it end to end with a little spill onto the snow
// around it — which is what "shining down ON sled" looks like. The apex is a
// near-point; not exactly zero, because a true zero-radius cone end renders as
// a degenerate sliver in this engine.
inline constexpr double kMarkerApexRadiusM = 0.06;
inline constexpr double kMarkerBaseRadiusM = 1.2;

struct MarkerGeom {
    glm::dvec3 base{0.0};  // the pool, ON the machine — the cone's WIDE end
    glm::dvec3 apex{0.0};  // the source, 10 m up LOCAL UP — the NARROW end
    double apex_radius_m = 0.0;
    double base_radius_m = 0.0;
    bool visible = false;
};

// The cone rises along LOCAL UP from the machine, which on a sphere is
// normalize(position) -- never a fixed world axis (the sphere invariant: no
// cached up, no global axis, recomputed every tick). A fixed axis would tilt
// the light off the machine everywhere except one point on the globe. At the
// origin the direction is undefined; report invisible rather than emit a NaN
// position that would poison the draw.
inline MarkerGeom marker_geom(const glm::dvec3& machine_pos, bool show,
                              double height_m = kMarkerHeightM) {
    MarkerGeom g;
    const double r = glm::length(machine_pos);
    if (!show || !(r > 0.0) || !(height_m > 0.0)) return g;
    const glm::dvec3 up = machine_pos / r;
    g.base = machine_pos;
    g.apex = machine_pos + up * height_m;
    g.apex_radius_m = kMarkerApexRadiusM;
    g.base_radius_m = kMarkerBaseRadiusM;
    g.visible = true;
    return g;
}

}  // namespace render
