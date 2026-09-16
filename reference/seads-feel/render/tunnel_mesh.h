#pragma once

#include <algorithm>
#include <glm/geometric.hpp>
#include <glm/vec3.hpp>
#include <vector>

#include "world/tunnel_net.h"

// T2 GREYBOX GEOMETRY (docs/tunnel_staging.md rung T2). The PURE generator for
// the Errington-tunnel interior: it turns the T1 query volume
// (world::TunnelNet) into vertex/index arrays a caller uploads. The whole point
// of routing through the net is SINGLE-SOURCE fidelity — the visible wall IS
// the T1/T3 query surface, so a pilot can never see a wall the collision
// doesn't have (or die at a wall he can't see). Every ring/pocket vertex sits
// on (or just inside) the net's signed_distance == 0 surface; the test pins it.
//
// PURE: glm + std only, ZERO raylib (lives in seads_render_core so the gate can
// pin it headlessly). The app-side upload/draw is render/tunnel.cpp (the seads
// target). world::HeightField is the OPTIONAL collar rim source (may be null =>
// the bare sphere at TunnelParams::sphere_R for the mouth funnels).

namespace world {
struct HeightField;
}  // namespace world

namespace render {

// The KIND of a greybox piece — drives its base draw value AND its depth cue
// (T4b, the lit Black Stope). Superset of the old is_collar bool: the egg +
// chambers are LIT (they draw brighter than the void-black tube wall AND their
// per-vertex cue is baked at 1.0 so the deep depth-floor never darkens them —
// "the energy fight has a lit chamber. The black stope." — Chad 2026-07-18).
enum class PieceKind {
    kWall,     // tube + connectors: void-black deep, depth-cued (kWallVal)
    kChamber,  // the two side chambers: lit (kChamberVal, cue baked 1.0)
    kCollar,   // the mouth skirts / bowl wall: solid daylight earth (kSkirtVal)
    kFloor,    // T5a: the flat spine-tube floor strip (kFloorVal; reads bright
               // via the local-up floor key — it IS the floor)
    kCavern,   // T10/T11: the arena ceiling (mantle underside) — lit by the
               // radial core-key + radius strata (crust/mantle bands). T12: the
               // emissive core (kCore) is GONE; the arena is lit by the key +
               // embers alone.
    kTrench,   // T16 DEFECT-2 — the Errington entry-trench open-cut walls: a
               // terrain-hugging rim + vertical wall + floor disk draping each
               // approach-trench step so the ramp no longer sees through the
               // backface-invisible cut to space. Daylight earth (kSkirtVal),
               // EXTERIOR. VISUAL-ONLY (collision is the trench Bowl SDF). Its
               // OWN kind (not kCollar) so the "two mouth collars" structural
               // pins stay exact; shaded/exterior-gated like a collar.
    kPortal,   // T16 ENTRANCE-ART (Chad 2026-07-21, "delete the sinking
               // headframe; a proper tunnel PORTAL FRAME and then a tunnel"):
               // the framed adit at the Errington bore mouth — thick posts +
               // lintel beam framing the bore, angled wing walls, and a
               // protruding canopy hood over the approach. Bright concrete
               // (kPortalVal), DAYLIGHT ambient (kExteriorAmbient), EXTERIOR.
               // VISUAL-ONLY — no SDF, planes fly through it (collision is the
               // terrain/pit/tube). Frame geometry sits OUTSIDE the bore
               // ellipse so it never narrows the flyable opening.
};

// A depth-cue-carrying greybox mesh piece. `cue` is one scalar per vertex in
// [0,1] (a brightness the FS multiplies): ~1 near the mouths, fading toward the
// kDepthFloor deep underground so the tube reads dark-into-black. Indices are
// 16-bit — every piece is well under 65535 verts (asserted at generation).
// `kind` selects the draw value (see PieceKind); `is_collar` is kept as a
// convenience mirror of (kind == kCollar) for the T3-era callers/tests.
struct TunnelPiece {
    std::vector<float> positions;         // 3 floats/vertex (world-absolute)
    std::vector<float> cue;               // 1 float/vertex, [0,1]
    std::vector<unsigned short> indices;  // triangle list
    PieceKind kind = PieceKind::kWall;    // draw-value / cue selector (T4b)
    bool is_collar = false;               // == (kind == kCollar); T3 mirror
    // T23: vertex count BEFORE the bore/cut trims ran (the ring-structured
    // roster the count pins assert against). Both trims append subdivided
    // border LEAVES after this mark, always in unshared vertex TRIPLES, so
    // (cue.size() - trim_base_verts) % 3 == 0. 0 == the piece receives no
    // leaf-appending trim.
    std::size_t trim_base_verts = 0;
};

// A gaslamp (T4b): a warm point-glow along the tube / around the Black Stope
// rim / in the chambers. Placement is PURE (from the T1 net); the app uploads
// these into the additive lamp pass (render/tunnel.cpp), reusing the proven
// world-sudbury street-lamp glow tech. `pos` is a world-absolute metre point
// STRICTLY inside the net (net.signed_distance(pos) < -2), so a lamp is never
// buried in rock (invisible) or floating in open sky (misplaced).
struct TunnelLamp {
    glm::dvec3 pos;          // world-absolute [m], strictly inside the net
    float intensity = 1.0f;  // relative brightness (egg-rim lamps are brighter)
};

struct TunnelMeshData {
    std::vector<TunnelPiece> pieces;
};

// ---------------------------------------------------------------------------
// T16 VISIBILITY CULLING (docs/tunnel_staging.md T16). The tunnel greybox was
// drawn in full every frame — the two-sided 336x168 arena + the whole 12 km
// tube + hundreds of additive lamp glows — regardless of the camera, at Ramsey
// Lake as much as at a mouth. The pieces split cleanly into two visibility
// classes, gated per frame from the player position (deterministic, no frustum
// extraction, no per-frame SDF eval).
//
// A piece is EXTERIOR (visible from the open sky) when its kind sits at the
// surface: the mouth skirts / open-pit + bowl walls (kCollar) and the Errington
// portal frame (kPortal). Everything else is INTERIOR (the tube walls
// kWall, the flat floor strip kFloor, the side chambers kChamber, and the deep
// arena + its breach collars kCavern) — only visible from underground or while
// diving into a mouth.
inline bool piece_is_exterior(PieceKind kind) {
    return kind == PieceKind::kCollar || kind == PieceKind::kPortal ||
           kind == PieceKind::kTrench;
}

// EXTERIOR pieces draw out to this generous range (they are landmarks seen from
// the air); INTERIOR pieces draw when the player is underground OR within this
// range of either mouth (so the corridor is present as you dive in).
inline constexpr double kTunnelExteriorDrawDist_m = 20000.0;  // 20 km
inline constexpr double kTunnelInteriorMouthDist_m = 2500.0;  // 2.5 km

struct TunnelDrawGate {
    bool exterior = true;  // draw the surface pieces (within draw distance)
    bool interior =
        true;  // draw the tube/arena pieces (underground / at mouth)
};

// The pure per-frame draw decision (T16). `net_center`/`net_radius` are the
// TunnelNet broad-phase bound sphere (enclosing every piece); `mouth_e`/
// `mouth_m` the two surface bore mouths (spine front/back); `underground` is
// the player-inside-the-net boolean plumbed from the app (already known — never
// a second SDF eval here). EXTERIOR is on within the draw distance of the net;
// INTERIOR is on when underground or near a mouth. Testable in isolation.
inline TunnelDrawGate tunnel_draw_gate(
    const glm::dvec3& eye, const glm::dvec3& net_center, double net_radius,
    const glm::dvec3& mouth_e, const glm::dvec3& mouth_m, bool underground) {
    TunnelDrawGate g;
    const double to_net = glm::length(eye - net_center) - net_radius;
    g.exterior = to_net < kTunnelExteriorDrawDist_m;
    const double dm =
        std::min(glm::length(eye - mouth_e), glm::length(eye - mouth_m));
    g.interior = underground || dm < kTunnelInteriorMouthDist_m;
    return g;
}

// Render-look constants for the greybox (S9-zoom pattern: render tunables are
// CODE CONSTANTS, not config — only geometry-coupled values derive from the
// net). Exposed so the test and the app-side shader share ONE definition.
inline constexpr int kTubeSegments =
    24;                                 // ring verts along the tube/connectors
inline constexpr int kPocketLong = 48;  // ellipsoid longitude divisions (T9:
                                        // the 2.2 km GRAND egg needs finer
                                        // facets so the cavern reads round, not
                                        // as a D20 — ~4.6k verts, under the u16
                                        // index cap asserted in upload_piece)
inline constexpr int kPocketLat = 24;   // ellipsoid latitude divisions (T9)
inline constexpr int kCollarSegments = 24;  // mouth-collar annulus verts
// T5d: the terrain-hugging collar/bowl-rim band uses MORE azimuth segments than
// the tube (2x) so the azimuthal chords also stay under the fine terrain at the
// larger collar rim radius (measured: at 24 segs the Murray 631 m rim's
// azimuthal mid-chord poked +0.14 m above the facets; at 48 it clears). Must be
// an integer multiple of kTubeSegments so the innermost band ring reduces 2:1
// onto the EXACT tube-end seam ring (kTubeSegments verts) with no crack.
inline constexpr int kCollarBandSegments = 2 * kTubeSegments;  // 48
inline constexpr double kMouthCutFactor =
    2.0;  // CUT RADIUS factor (cut_radius = kMouthCutFactor * tube_radius).
          // The COLLAR outer radius is LARGER — see collar_reach() below.
inline constexpr float kWallVal = 0.11f;     // deepest wall brightness ceiling
inline constexpr float kSkirtVal = 0.58f;    // mouth-collar / open-pit wall:
                                             // DAYLIGHT ROCK (T16 de-black —
                                             // read as sunlit terrain, not a
                                             // black cave; the FS exterior path
                                             // uses this as the base directly,
                                             // no underground strata override)
inline constexpr float kEggVal = 0.32f;      // Black Stope egg wall (lit, T4b)
inline constexpr float kChamberVal = 0.22f;  // side-chamber wall (lit, T4b)
inline constexpr float kFloorVal = 0.16f;    // T5a flat floor strip base value
                                             // (depth-cued; the local-up floor
                                             // key brightens it — it faces up)
inline constexpr float kPortalVal =
    0.82f;  // T16 Errington portal frame — bright CONCRETE (the framed adit
            // reads pale against the pit rock; daylight exterior shading).
            // T24: 0.75 -> 0.82, one notch BRIGHTER than the 0.58 rock stack
            // behind it (Chad's right-flank "hard to tell" pointer — the
            // sleeve went a notch darker, the wing a notch brighter, so the
            // stacked pale layers separate in value and parallax reads).
inline constexpr float kDepthFloor = 0.30f;  // cue floor deep underground [0,1]
inline constexpr double kCueFadeDepth_m =
    900.0;  // depth below local surface over which the cue fades to the floor

// The minimum terrain cell arc length at the face equator: the cubesphere face
// is pi/2 of arc divided into tiles*(subdiv-1) cells. Cells near the face edges
// are up to 2x larger (the gnomonic tangent warp, sec²(q·pi/4)=2 at q=±1). ONE
// definition — both collar_reach() (the outer-radius margin) AND the
// terrain-hugging collar band (its radial ring spacing, T5d) derive from THIS,
// so a subdiv/tiles retune moves both automatically — never a welded constant.
inline double terrain_cell_arc(int subdiv, int tiles, double base_R) {
    const double pi = 3.14159265358979323846;
    return (pi * 0.5 * base_R) / (static_cast<double>(tiles) * (subdiv - 1));
}

// Derive the collar outer radius from the terrain grid geometry.
//   cut_radius_m  — the mouth hole radius (kMouthCutFactor * tube_radius)
//   subdiv, tiles — the cubesphere mesh knobs from [planet] (world.toml)
//   base_R        — the sphere radius [m]
//
// The terrain cut leaves a ragged edge up to one cell DIAGONAL beyond the
// cut radius, so the collar must reach past it by a safety margin (20 m).
// Derived from the same table the terrain builds from so a subdiv/tiles
// retune moves the collar automatically — never a welded constant.
// The collar reach from the terrain CELL ARC directly (the single expression;
// the subdiv/tiles overload routes through this). Both the mouth collars and
// the T16 trench-step walls derive their outer rim from THIS, so a subdiv/tiles
// retune moves every covering rim together.
inline double collar_reach_from_cell(double cut_radius_m, double cell_arc_m) {
    // The collar must cover the worst-case ragged edge, so we use 2×cell_diag
    // as a conservative upper bound (cell_diag = sqrt(2)*cell_arc).
    const double cell_diag = 1.41421356237 * cell_arc_m;
    return cut_radius_m + 2.0 * cell_diag + 20.0;
}
inline double collar_reach(double cut_radius_m, int subdiv, int tiles,
                           double base_R) {
    return collar_reach_from_cell(cut_radius_m,
                                  terrain_cell_arc(subdiv, tiles, base_R));
}

inline constexpr int kBowlRings =
    6;  // cone rings down a SMOOTH funnel wall (the Errington entry pit)
// T16 ENTRANCE-ART — THE MURRAY OPEN-PIT BENCHES (Chad 2026-07-21: "a proper
// OPEN-PIT mine look ... a dry open pit that actually reads as one from the
// air"). The Murray bowl wall is reshaped from a smooth funnel into stepped
// terraces: kBowlBenches benches, each a near-vertical FACE + a flat BERM, so
// from above it reads as a real terraced open pit. The staircase keeps the
// SAME outer rim radius and the SAME tube-end (floor mouth) seam as the smooth
// cone, and every bench radius stays AT-OR-OUTSIDE the smooth-funnel radius at
// that depth (benches carve OUTWARD into the rock, never inward into the open
// flight volume) — so collision (the bowl SDF cylinder, radius bowl_r) is never
// protruded and no death surface reads on open air. A benched wall emits
// 2*kBowlBenches + 1 rings (rim + a face + a berm per bench, the last berm ==
// the tube end ring).
inline constexpr int kBowlBenches =
    world::TunnelNet::kMurrayBenches;  // terraces down the Murray open pit —
                                       // T18: CANON IN WORLD (the collision
                                       // cone's floor edge and this staircase
                                       // derive from the same count)

// T11/T12 — THE SEALED-CORE ARENA tessellation. The arena is an inward-viewed
// oblate ellipsoid (arena_a x arena_c). T12: the floor is SEALED (no shaft, no
// visible core sphere). The grid must be fine enough that a breach HOLE
// (angular radius breach_hole_arc/R) spans SEVERAL facets — else no facet is
// skipped and the arena wall reads solid over the bore. At 336x168 the facet
// arc resolves the canon ~275 m hole radius, so the opening reads through. u16
// index cap: (kCavernLong+1)*(kCavernLat+1) verts must stay < 65535 —
// 337*169 = 56953 clears.
inline constexpr int kCavernLong = 336;  // cavern sphere longitude divisions
inline constexpr int kCavernLat = 168;   // cavern sphere latitude divisions
// The breach HOLE angular radius: a cavern facet whose direction lies within
// kBreachHoleFactor * tube_width of a breach axis (arc metres) is SKIPPED so
// the bore opening reads through the ceiling. Single-sourced (the
// errington_pit_rim pattern) so the visible hole tracks the bore width.
inline constexpr double kBreachHoleFactor = 2.5;
inline double breach_hole_arc(double tube_width_m) {
    return kBreachHoleFactor * tube_width_m;
}

// T22 — THE THROAT RAMP FACE dimensions (fly-16 "hit something invisible...
// right down the pipe"): the drawn face on the Murray throat's collision
// ramp. Exported so the bowl-piece structural pins DERIVE the vertex delta
// (rows * across) instead of welding a count.
inline constexpr int kThroatRampRows = 9;    // stations along the throat
inline constexpr int kThroatRampAcross = 7;  // verts per row

// T14 — THE TRUE BREACH LOCATOR (single source; supersedes T10.1's
// breach_axes). ROOT CAUSE of the round-10 blind spots (docs/
// tunnel_scout_2026-07-20.md): T13-A1 shrank the arena (7350 -> 4200) but the
// old locator still picked "the spine node nearest arena_apex_r BY RADIUS" — a
// depth rule — so the holes/beacons landed 748-897 m from where the bore
// actually pierces the shrunk ellipsoid (275 m holes, NO overlap: real openings
// opaque, beacon rings around death-holes over solid rock).
//
// A Breach is the EXACT crossing of the bore centreline with the arena
// ellipsoid wall, found by bisecting world::ellipsoid_sd (the very function the
// SDF flies — single source) along the spine segment that crosses it.
// `into` is the unit bore tangent INTO the arena there; `node_in` the first
// spine node strictly inside the arena on that leg (build_tube suppresses the
// inclusive [node_in_E, node_in_M] index range — no tube wall across open
// arena air, and no wall-less tube outside it). `stretch` (>= 1) is the
// oblique-crossing elongation: the bore meets the dome at angle theta to the
// wall normal, so the true opening is an ellipse elongated 1/cos(theta) along
// the bore tangent — a round hole would clip it. Capped at kBreachMaxStretch
// (a grazing crossing must not cut the whole dome).
//
// EVERY breach consumer routes through breach_points + breach_hole_hit: the
// arena facet cut, the ember skip, the beacon rings, the tube suppression, the
// manifold verifier's "designed opening" allowance. ONE definition so the hole
// the ceiling cuts, the embers avoid, and the beacons ring can never disagree
// — and all of them sit where the bore ACTUALLY crosses. Returns [Errington,
// Murray]; EMPTY if the arena is off or the bore never enters it.
struct Breach {
    glm::dvec3 pos{0.0};   // exact wall crossing of the bore centreline [world]
    glm::dvec3 into{0.0};  // unit bore tangent INTO the arena at the crossing
    std::size_t node_in = 0;  // first spine node inside the arena on this leg
    double stretch = 1.0;     // oblique elongation along `into` (>= 1, capped)
    // T14b (fly-11: "I can see through the ground to the surface" west of the
    // Murray bore) — the cut hole's semi-axes in arc metres, baked at locator
    // time so every consumer (facet cut, ember skip, beacon ring, tests) reads
    // ONE pair. minor = the canon round hole (breach_hole_arc). major WAS
    // stretch*hole_arc (~707 m at ship) — 2.9x the physical opening, leaving
    // ~460 m of see-through-but-solid gash at each elongated end (terrain
    // backfaces = the surface reads through). Now the PHYSICAL oblique extent
    // + the same absolute margin the round hole has always carried:
    //   major = max(minor, stretch*tube_height + (hole_arc - tube_width))
    // (~399 m at ship; degrades exactly to the round hole as stretch -> 1).
    double hole_minor_m = 0.0;  // cut semi-minor [arc m]
    double hole_major_m = 0.0;  // cut semi-major along `into`'s tangential dir
};
inline constexpr double kBreachMaxStretch = 4.0;
// T15 — the breach-collar outer rim reaches this far past the cut ellipse [m]
// (covers the ragged any-corner facet edge, ~2 facet diagonals at the crossing
// latitude), so the annulus always overlaps the kept facets and no sliver of
// rock void survives at the rim.
inline constexpr double kBreachCollarPad_m = 160.0;
// T26 — the coarse 24-vertex breach-collar rings tore azimuthally where the
// OBLIQUE bore grazes the arena wall (it drops a long thin tongue of arena
// facets on the trailing side that the compact mouth-ring cross-section
// under-covered). AZIMUTHALLY SUBDIVIDE the collar rings by this factor (the
// exact 24-vert tube seam ring is preserved and fanned onto the dense ring, so
// the tube<->collar no-crack seam is untouched); and lay the outer margin
// annulus as this many concentric on-wall bands so the widened annulus is
// finely triangulated (voids impossible, over-cover allowed).
inline constexpr int kBreachCollarAzSub = 4;    // 24 -> 96 azimuth verts
inline constexpr int kBreachCollarBands = 3;    // radial bands in the margin
// The margin annulus reaches this far past the landed rim [m] (T26: raised from
// the pad above so the collar over-reaches the oblique grazing tongue on the
// trailing side, where the arena drops facets well beyond the compact bore).
inline constexpr double kBreachCollarReach_m = 320.0;
// T16 DEFECT-1 — RENDER CUT FROM COLLISION TRUTH. The arena wall drops a facet
// when the swept bore actually pierces the shell at a corner
// (tube_signed_distance < this small positive band), so the visible hole is the
// TRUE collision opening — never an independently-computed angular ellipse that
// can leave a rendered wall over open bore air (the fly-through wall). The tiny
// positive band makes the hole a hair larger than the opening so no kept facet
// straddles the boundary within the render-vs-collision tolerance.
inline constexpr double kBreachRenderCut_m = 2.0;
// Only arena wall vertices within this world radius of a breach crossing pay
// the tube-SDF scan; the far ceiling costs one length check (the bore opening
// is a local ~500 m feature on a 4200 m room).
inline constexpr double kArenaBreachTestRadius_m = 2500.0;
// Azimuth divisions of the on-wall breach collar annulus (finer than the tube
// so the sealed rim reads round from inside the arena).
inline constexpr int kBreachCollarSegments = 48;
std::vector<Breach> breach_points(const world::TunnelNet& net);

// ---------------------------------------------------------------------------
// T17 — THE CUT-SWALLOWED TRIM (fly-13, Chad: "both entrances are occluded I
// could not fly into either"). Two same-class regressions shipped in T16:
//  * ERRINGTON: the three approach-trench steps are heavily OVERLAPPING open
//    cuts (150 m rims, centres 100 m apart, abutting the entry pit), and each
//    T16 drape emits a FULL 360-degree wall + full floor disk — so every
//    step's drape renders straight across its neighbours' (and the pit's)
//    open volume. The whole approach roofed over into a pale slab; no
//    entrance read at all.
//  * MURRAY: the bore leaves the pit floor INCLINED, so the tube's crown near
//    the mouth sits ABOVE the bowl floor — inside the bowl's open collision
//    CYLINDER (T6-P0). Rendered: a solid black dome plugging the floor mouth.
//    (Pre-T16 the pit rendered near-black, so the black-on-black plug was
//    invisible; the T16 daylight benches exposed it — it is plainly visible,
//    unremarked, in the committed murray_bowl_t16.png certification smoke.)
// ONE fix, collision truth (the T16 DEFECT-1 principle extended to the
// surface cuts): a greybox facet of the TUBE / FLOOR / TRENCH pieces is
// DROPPED when any of its vertices is swallowed by an open-cut Bowl volume
// (world::bowl_sd < -kCutSwallowEps_m for the Murray bowl, the Errington pit,
// or any trench step) — rendered rock strictly inside a volume collision says
// is open air is a lie the pilot cannot fly. The collar/bowl-funnel pieces
// are deliberately NOT trimmed: the decorative funnel/bench walls live inside
// the T6-P0 collision cylinder BY DESIGN (render-only terraces carved into
// the cut), and the portal is a visual frame standing in the open pit.
// kCutSwallowEps_m clears the tube's own azimuth chord sag (~0.94 m at 24
// segments) and every drape's on-boundary seat with margin, so a facet ON its
// own cut boundary is never dropped.
inline constexpr double kCutSwallowEps_m = 5.0;

// The shared hole predicate: is world direction `wdir` (unit, from the planet
// centre) inside any breach's cut? Each breach cuts an ELLIPSE in angle space
// centred on its own axis (normalize(pos)): semi-axes = the breach's OWN
// hole_minor_m/hole_major_m (baked by breach_points — see above) at the
// breach's own radius. `pad_m` widens both semi-axes (the manifold verifier's
// rim allowance); 0 for the exact cut. Single predicate for the facet cut, the
// ember skip, and every test oracle.
bool breach_hole_hit(const std::vector<Breach>& breaches,
                     const glm::dvec3& wdir, double pad_m = 0.0);

// ---------------------------------------------------------------------------
// T16 ENTRANCE-ART — THE ERRINGTON PORTAL FRAME (docs/tunnel_scout_2026-07-20;
// Chad's 2026-07-21 ruling: DELETE the T13 sinking headframe, "replace with a
// proper tunnel PORTAL FRAME (framed adit) and then a tunnel — and NOT all
// black"). A framed adit at the sunk bore mouth on the Errington pit floor:
// two thick POSTS + a LINTEL beam framing the elliptical bore opening, angled
// WING WALLS flaring outward from the posts, and a short protruding CANOPY hood
// over the approach flight path. Built in the bore's own (horiz, vert, tangent)
// frame at net.spine.front() so it frames the exact hole the pilot flies into.
//
// VISUAL-ONLY — NO SDF. Planes fly THROUGH it (collision is the terrain/pit/
// tube volume, unchanged). Every member is a CLOSED manifold box. ALL frame
// geometry sits OUTSIDE the bore ellipse (semi rw x rh) with clear margin
// (kPortalClearance) so the frame never narrows the flyable opening. Gated on
// net.headframe_on (the surface-structure flag, repurposed from the deleted
// headframe — collision/config plumbing untouched). PieceKind::kPortal,
// EXTERIOR, bright concrete (kPortalVal) with daylight ambient.
//
// GEOMETRY DIALS (fractions of the bore semi-axes so the frame scales with the
// bore and reads from the air):
inline constexpr double kPortalClearance =
    0.15;  // frame inset past the bore ellipse (posts stand this * rw beyond
           // the horizontal semi; lintel this * rh above the vertical semi)
inline constexpr double kPortalPostThick =
    0.13;  // post half-thickness as a fraction of the horizontal bore semi
inline constexpr double kPortalLintelThick =
    0.16;  // lintel half-height as a fraction of the vertical bore semi
inline constexpr double kPortalRiseFrac =
    0.20;  // post foot drops this * rh below the bore bottom (planted on floor)
inline constexpr double kPortalWingSpread =
    0.9;  // wing-wall outward flare as a fraction of the horizontal bore semi
inline constexpr double kPortalCanopyReach =
    0.45;  // canopy hood reach OUT over the approach as a fraction of rw.
           // T20 (fly-16 "errington is impregnable!"): at 1.3 the hood was a
           // ~143 m slab that ROOFED the arch from trench-line height — on
           // the shallow approach the entrance read as a sealed building. A
           // short brow keeps the adit read; posts + lintel stay the frame.
inline constexpr int kPortalLintelBeacons =
    3;  // aviation-beacon lamps across the lintel (reads at dusk)

// ---------------------------------------------------------------------------
// T19 ENTRANCE-ART — THE ERRINGTON 1931 RUINS + THE MURRAY OPENING BLEND
// (fly-15, Chad: "we just need to do a cosmetic cleanup. Make it look like it
// was in 1930 ruins. The murray bowl is good just needs a clean up. Make the
// opening blend in to the scene, now it looks out of place."). Two render-only
// deliverables, no SDF, no collision:
//
//  A. THE 1931 GHOST-RUIN ENSEMBLE — a leaning timber gallows headframe (one
//     leg snapped short, fallen members half-sunk at its base), a roofless
//     rockhouse/mill shell (staggered irregular wall tops), and an ore-car
//     trestle running to a waste-rock dump fan, clustered on the Errington pit
//     FLANKS (never straddling the bore). Built from closed manifold boxes,
//     FOLDED INTO the kPortal piece (same net.headframe_on gate, still the
//     LAST piece) so the portal roster + structural pins stay put. Placement
//     derives from the pit frame (net.pit + net.pit_up_tangent) — no magic
//     world coords. The Treadwell Yukon camp died in the 1931 metal-price
//     crash (docs/errington_entrance_art_brief.md): silvered timber ghost-ruin.
//  B. THE MURRAY OPENING BLEND — the pale open-pit surround butts bright
//     against the black bore slot. Scale the mouth pieces' per-vertex CUE DOWN
//     toward rock-dark near the bore mouth (darken_cue_near) so the opening
//     fades into the slot; and raise the mouth-piece border subdivision depth
//     (kMouthSplitDepth) so the pale cut-border flap leaves shrink again.

// A: the ruin draw value — silvered timber / rusted iron, DARKER than the pale
// concrete kPortalVal. LEAST-INVASIVE mechanism (no new uniform, no FS change):
// the ruins share the kPortal piece, and the FS draws each kPortal vertex at
// base(kPortalVal) * lit * cue, so a ruin vertex baked at cue = kRuinVal /
// kPortalVal reads at kRuinVal while the pale frame stays at kPortalVal (cue
// 1).
inline constexpr float kRuinVal = 0.42f;
// Ruin placement, as fractions of the pit rim radius (net.pit.bowl_r) so the
// cluster scales with the pit; the ensemble sits on ONE lateral flank + the
// approach side, clear of the bore and the dive-in corridor.
inline constexpr double kRuinHeadframeFlankFrac = 1.30;  // headframe centre lat
inline constexpr double kRuinRockhouseFlankFrac = 1.60;  // rockhouse centre lat
inline constexpr double kRuinDumpFlankFrac = 2.05;       // dump-fan centre lat
inline constexpr double kRuinLeanDeg = 3.0;     // headframe leg lean [deg]
inline constexpr double kRuinFootSink_m = 0.8;  // every footing sunk this deep
// The ensemble's worst-case LATERAL reach from the pit axis (fraction of the
// rim radius; the far dump-fan mound corner) — the test's portal-bounds pin
// derives its ruin term from this.
inline constexpr double kRuinReachFrac = 2.75;

// B: the mouth-opening darkening blend. cue *= mix(kMouthBlendFloor, 1, t) with
// t = clamp(dist(vert, bore-mouth centre) / R_blend). Murray gets the wider
// radius (the open pit is bigger); Errington the tighter adit radius.
inline constexpr float kMouthBlendFloor = 0.35f;      // darkest at the mouth
inline constexpr double kMouthBlendMurrayFrac = 2.2;  // R_blend = *tube_width
inline constexpr double kMouthBlendErringtonFrac =
    1.6;  // R_blend = *tube_width
// The mouth-piece border subdivision depth (kSplitDepth 3 -> 4 for the two
// mouth collars only; the trench/tube/floor keep 3). Passed per-call to
// drop_cut_swallowed_tris so the pale cut-border flaps halve again.
inline constexpr int kMouthSplitDepth = 4;

// T28 — THE ERRINGTON PIT-SLEEVE BROW TRIM (fly round-18 wrap). The pit cut
// sleeve (build_pit_sleeve) occludes the LOW grazing see-through rays that
// thread under the cone terminus (the T26 census sealer, near the floor). Its
// shallow upper wall band seals nothing — on the flat-top quadrant the funnel
// cone does not reach, it stands proud inside the crater and reads as "mesh
// strung across the opening" (the T18/T26 arch/lintel class). Drop the sleeve
// facets whose shallowest vertex is within this FRACTION of the pit depth of
// the surface. The sleeve wall rings sit at depths {~0 (tucked), 0.5*depth,
// depth}; any value in (0, 0.5) drops exactly the top->mid upper-wall band (the
// only part visible above the funnel cone) and leaves the mid->foot lower wall
// + floor disk — the actual see-through seal — untouched, so the T26 census
// stays 0 (verified: 0.55 which also eats mid->foot re-opens 4 rays; <=0.45
// keeps them). 0 => keep the whole sleeve (the pre-T28 look / mutation lever).
inline constexpr double kSleeveBrowDepthFrac = 0.4;

// Build the greybox from the T1 net. `ground` (may be null) is the collar rim
// surface source; when null the collar drapes on the bare sphere at
// net-implied R (recovered from the spine mouth radius).
// `errington_collar_outer_m` is the outer radius of the ERRINGTON mouth-collar
// funnel [m]; use collar_reach() to derive it from the terrain grid. A value
// <= 0 falls back to the legacy kMouthCutFactor * tube_radius.
// `murray_collar_outer_m` is the outer radius of the MURRAY BOWL WALL [m] — the
// terrain rim of the open pit; use collar_reach(bowl_radius, ...). The Murray
// collar becomes a multi-ring BOWL WALL (T4a) from the terrain rim down to the
// tube end ring at the bowl floor. A value <= 0 falls back to the bowl radius
// stored in the net.
// `terrain_cell_arc_m` (T5d) is the terrain grid cell size [m] — the RADIAL
// ring spacing for the TERRAIN-HUGGING collar/bowl-rim band, so the coarse
// collar can no longer chord above the fine terrain (the portal strobe). Pass
// terrain_cell_arc(subdiv, tiles, R); a value <= 0 falls back to a single
// terrain-overlap band (the pre-T5d 2-ring behavior — the null-ground / test
// path with no grid info).
// `cut_swallow_trim` (T17) applies the cut-swallowed trim above. TRUE is the
// shipped behavior; false is the TEST-ONLY mutation lever for the
// entrance-open kill tests (the load-bearing arm — the T16 leak audit's
// drop_trench discipline). The app never passes false.
// `tube_trim_split_depth` (T23) is the subdivision depth of the tube/throat
// bore-swallow trim on the two mouth pieces (mixed facets split, only
// fully-swallowed leaves drop — voids impossible, over-cover allowed). The
// default is the shipped depth; 0 is the TEST-ONLY mutation lever that
// reproduces the legacy whole-facet drop (the see-through sliver at the
// Murray pit-floor/slot junction). The app never passes it.
TunnelMeshData build_tunnel_mesh(const world::TunnelNet& net,
                                 const world::HeightField* ground,
                                 double errington_collar_outer_m = 0.0,
                                 double murray_collar_outer_m = 0.0,
                                 double terrain_cell_arc_m = 0.0,
                                 bool cut_swallow_trim = true,
                                 int tube_trim_split_depth = kMouthSplitDepth);

// ---------------------------------------------------------------------------
// GASLAMPS (T4b + T5b, docs/tunnel_staging.md rung T4/T5; Chad: "line the sides
// of the bottom ramp with lights as well as the arc of the tunnel"). PURE
// placement from the T1 net — the app uploads the result into the additive lamp
// pass. Families:
//   * FLOOR-EDGE PAIRS (T5b) — runway-edge lamps lining BOTH sides of the spine
//     tube where the floor meets the wall, every kFloorLampSpacing along the
//     whole spine (incl. the bottom ramp Chad named), a couple metres above the
//     floor + inset. With the T5a floor ON they sit at the floor chord edges;
//     with the floor OFF they fall back to ~±60° low wall positions so the
//     runway read survives.
//   * CROWN/ARC LINE (T5b) — a single line of lamps along the TOP of the arch
//     (the tube crown), every kCrownLampSpacing, inset from the ceiling.
//   * CHAMBER lamps — kChamberLampCount per side chamber + kConnectorLampCount
//     per connector.
// T10: the buried egg + its bright lamp RINGS are GONE — the CORE lights the
// cavern (an emissive mesh + glow billboard, render/tunnel.cpp), not a lamp
// ring. Nodes inside the chambers/bowl are SKIPPED (those volumes own their own
// light). Every returned lamp satisfies net.signed_distance(pos) <
// kLampInsideMargin.
//   * T10.1 CAVERN EMBER FIELD — kCavernEmberCount dim embers on the ceiling
//     sphere (Fibonacci lattice, breach holes skipped); intensity
//     kCavernEmberIntensity (its own render tier). NOT inside-net-gated (the
//     ceiling sphere IS the net boundary — an ember there reads sd ~ 0, not <
//     -2 — so the strict-inside gate would drop them all; they are placed ON
//     the ceiling by construction, breach-skip is the only cull).
//   * T10.1 BREACH BEACON RINGS — kBreachBeaconCount bright lamps around each
//     breach lip (intensity kBreachBeaconIntensity, the bright tier). Same
//     on-the-ceiling placement (not inside-net-gated).
inline constexpr double kFloorLampSpacing = 60.0;   // floor-edge pair spacing
inline constexpr double kCrownLampSpacing = 120.0;  // crown line spacing
inline constexpr double kLampInset = 6.0;       // lamp inset off the wall [m]
inline constexpr double kFloorLampRaise = 3.0;  // floor-edge lamps above floor
inline constexpr float kTubeLampIntensity = 1.0f;
inline constexpr float kChamberLampIntensity = 1.4f;
inline constexpr int kChamberLampCount = 4;        // lamps per side chamber
inline constexpr int kConnectorLampCount = 2;      // lamps per connector
inline constexpr double kLampInsideMargin = -2.0;  // strictly-inside pin [m]

// T10.1 — CAVERN READABILITY (docs/tunnel_staging.md T10.1, driver's screenshot
// verdict: the far shell reads pure black + the ceiling is a featureless wash +
// the breaches are unfindable dark blobs). The repo's flown lesson — "to see a
// big space you need POINT LIGHTS, not wall shading" (the world-sudbury
// street-lamps-from-altitude precedent) — applied to the km-scale cavern.
//
//  * EMBER FIELD — kCavernEmberCount dim warm ember lamps scattered
//    DETERMINISTICALLY (a Fibonacci-sphere lattice, no wall-clock/random)
//    across the arena ceiling surface. Skips embers inside a breach
//    hole (single-sourced with the ceiling mesh's breach_hole predicate). They
//    give the black shell depth/parallax cues (points scattered in the dark)
//    and the ceiling a textured field instead of a uniform wash. Their
//    intensity is kCavernEmberIntensity so the app draws them in a dedicated
//    large-world-size ember tier (km-scale readability — a 8 m tube-lamp glow
//    is invisible at 5-15 km).
//  * BREACH BEACON RINGS — kBreachBeaconCount BRIGHT lamps ringed around each
//    breach hole lip (radius = the breach hole arc + a margin, on the cavern
//    sphere, centered on each breach axis). From across the cavern each exit
//    reads as a RING OF LIGHT (the "two ways in two ways out" cue). Their
//    intensity is kBreachBeaconIntensity => the bright tier.
inline constexpr int kCavernEmberCount = 150;  // ember lamps on the arena
                                               // ceiling (T11: density-parity
                                               // said 78; readability wins)
// T12 — FLOOR EMBERS: with the core sealed away (no upward glow), the arena
// FLOOR half self-shades to a black void (the sealed floor faces away from the
// origin). A floor-ward view needs a reference. A separate deterministic ember
// lattice on the FLOOR hemisphere gives the sealed floor depth/parallax the way
// the ceiling embers do for the mantle underside. Own count (the floor is
// smaller-read than the ceiling the pilot fights toward).
inline constexpr int kFloorEmberCount = 100;   // ember lamps on the arena floor
inline constexpr int kBreachBeaconCount = 12;  // beacons per breach ring
inline constexpr double kBreachBeaconArcMargin_m =
    140.0;  // beacon ring radius = breach_hole_arc + this [m, arc]
// T12 — MOUTH BEACON RINGS (Chad's round-9 fly: "The entrance to errington mine
// is not very visible, the black hole is but the tunnel shaft is still hard to
// find"). A bright ring of lamps around the ACTUAL bore opening at each mouth
// face (inside the entry pit / bowl), so the shaft reads as a ring of light
// inside the dark crater from approach distance. Same bright-tier tech as the
// breach beacons. Both mouths (Murray untested but symmetric — the same fix).
inline constexpr int kMouthBeaconCount = 12;  // beacons ringing each bore mouth
inline constexpr double kMouthBeaconFrac =
    0.8;  // ring radius as a FRACTION of the bore semi-axes (well inside the
          // wall so the whole ring clears both the Errington pit and the Murray
          // bowl-floor geometry — a fixed metre inset left the Murray ring
          // partly outside the net)
inline constexpr float kCavernEmberIntensity =
    0.5f;  // ember tier selector (< kBrightLampCut => dim family; own tier by
           // exact value)
inline constexpr float kBreachBeaconIntensity =
    3.0f;  // beacon tier selector (>= kBrightLampCut => bright family)
// T14b — THE MURRAY BOWL FUNNEL (fly-11: "a blind entrance ... its black and I
// can only see one light going down, caused me to crash"). The Murray pit had
// NO lamps of its own — only the bore mouth ring 300 m down at the floor, so
// from approach the pit is a black hole with a single glow. Two beacon rings
// light the funnel: one around the bowl RIM at the surface (the pit outline
// reads from approach distance AND on the climb out — the "exiting is a little
// blind" cue), one at MID-DEPTH on the bowl wall (the descent has a depth
// rung between rim and mouth ring). Own bright-family tier (2.5 >= the 2.0
// bright cut => 48 m glow) so tests/draw can tell them from breach/mouth
// beacons by exact value. Placed just inside the open cut (inset off the rim
// and wall). bowl_depth <= 0 => structurally absent (bit-identical).
inline constexpr int kBowlRimBeaconCount = 12;    // rim ring lamps (surface)
inline constexpr int kBowlMidBeaconCount = 8;     // mid-depth wall ring lamps
inline constexpr double kBowlBeaconInset = 15.0;  // inset into the cut [m]
inline constexpr float kBowlBeaconIntensity = 2.5f;  // own bright-family tier
// T16 ENTRANCE-ART — MURRAY EXIT THROAT RINGS (round-12 finding: "leaving the
// Murray tunnel into the open pit is still a BLIND flight — nothing marks the
// final tube stretch from inside"). The T14b bowl beacons all sit up in the
// pit; nothing lights the last stretch of TUBE before the Murray floor mouth.
// Three rings of throat beacons inside the final ~kMurrayThroatSpan of bore
// (placed by spine arc-length from the Murray mouth) give the pilot a ladder of
// rings leading to the exit. Own bright-family tier value (distinct from the
// bowl / breach / mouth beacons so tests isolate them by intensity), drawn in
// the bright tier (>= kBrightLampCut).
inline constexpr int kMurrayThroatRings = 3;    // rings of throat beacons
inline constexpr int kMurrayThroatPerRing = 8;  // beacons per throat ring
inline constexpr double kMurrayThroatSpacing_m =
    80.0;  // arc spacing from the Murray mouth (rings at ~80/160/240 m)
inline constexpr float kMurrayThroatIntensity =
    2.7f;  // own bright-family tier (>= 2.0 bright cut; != 2.5 bowl / 3.0
           // beacon)
// Down-angle for the floor-edge fallback when the floor is OFF (~60° below
// horizontal on each wall) so the runway-edge read survives without a floor.
inline constexpr double kFloorEdgeDownDeg = 60.0;

std::vector<TunnelLamp> place_tunnel_lamps(const world::TunnelNet& net);

}  // namespace render
