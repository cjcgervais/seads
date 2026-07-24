#pragma once

#include <array>
#include <glm/glm.hpp>

// S-cues (comfort program auto/comfort-orient, REC-3 + REC-4): two PERIPHERAL,
// world-stable orientation cues, pure HUD, default OFF. They address Finding 4
// of docs/comfort_attribution.md — during a maneuver NOTHING on screen is
// world-stable, and the accumulated camera-frame roll (holonomy the mouse basis
// carries by design, SPEC §9.1) leaves the pilot with no read of "which way is
// up." These cues give that read at the SCREEN PERIPHERY, never at the reticle
// (the S-retclamp lesson: a cue where the pilot AIMS is in the loop; a cue at
// the periphery is loop-neutral).
//
// PURE and raylib-free on purpose (compiled into seads_render_core, gate-tested
// headlessly): these functions return screen-space geometry in NDC; the thin
// raylib glue in render/draw.cpp pixel-converts and strokes them, UNDER the
// reticle layer. Nothing here reads or writes the aim/camera/control path — the
// cue is a display read of local_up (Cue A) and the SHARED-extracted bank phi
// (Cue B). Constitution: every HUD "up/level/horizon" is recomputed from
// normalize(position) each call, NEVER cached, NO fixed axis (SPEC §6.1).

namespace render {

// ---- Cue A: GHOST HORIZON (the ADI level-line, REC-3) ----------------------
//
// The LOCAL LEVEL PLANE is the plane through the eye perpendicular to local_up.
// Its projection into a pinhole view is a straight NDC line (a plane through
// the projection center images to a line). NOTE this is the LEVEL plane, NOT
// the visible limb: on R=15 km the limb dips acos(R/(R+h)) below level, but the
// recovery cue the pilot needs is "which way is up" — i.e. LEVEL — so we draw
// the level plane and label it as such. The line rotates with camera roll (it
// stays perpendicular to the projected local_up), which is exactly the roll
// read the pilot lacks after an immelmann.
//
// Output: up to 2 NDC line segments (the line, clipped to the [-1,1]^2 NDC box,
// with a CENTER EXCLUSION GAP of radius `center_gap_frac` cut out of the
// middle — nothing new lives where the pilot aims). 0 segments when the level
// line does not cross the view (nose straight up/down: the level plane edge-on,
// or the whole visible line inside the center gap), handled with NO NaN.

struct NdcSegment {
    glm::dvec2 a{0.0};
    glm::dvec2 b{0.0};
    // SKY-SIDE TICK (the inverted-ambiguity fix): a short stub from `tick_root`
    // (the segment's inner endpoint, toward screen center) extending along the
    // projected +local_up direction in DISPLAY NDC — i.e. toward the SKY. The
    // bare level line is identical upright vs inverted; this tick points
    // UP-screen when upright and DOWN-screen when inverted, so the pilot reads
    // which way is up. Same alpha as the line; caller strokes tick_root->tick.
    glm::dvec2 tick_root{0.0};
    glm::dvec2 tick{0.0};
};

struct GhostHorizonResult {
    int count = 0;  // 0, 1, or 2 valid segments
    std::array<NdcSegment, 2> segs{};
};

// cam_fwd/cam_up = the render camera basis (cam_up need not be orthonormal to
// cam_fwd; it is re-orthogonalized here exactly as project_dir does). local_up
// = normalize(position), passed in fresh by the caller (never cached). fovy_rad
// + aspect define the frustum; shift_ndc is the SPEC §9.2 vertical lens shift
// applied to the NDC-y (the whole overlay slides with the scene — matched to
// draw.cpp's to_pxf which does y' = y - shift). center_gap_frac in [0,1) is the
// radius (in NDC, isotropic) of the center exclusion disk. Degenerate cases
// (level plane edge-on, cam_up || cam_fwd) return count = 0, never NaN.
GhostHorizonResult ghost_horizon_segments(const glm::dvec3& cam_fwd,
                                          const glm::dvec3& cam_up,
                                          const glm::dvec3& local_up,
                                          double fovy_rad, double aspect,
                                          double shift_ndc,
                                          double center_gap_frac);

// GENERALIZED ghost-plane geometry (the single implementation the horizon and
// the pitch ladder both delegate to — NO copy-paste of the projection). It
// images the PLANE dot(d, local_up) = sin(elev_rad): elev_rad = 0 is the LEVEL
// plane (recovers ghost_horizon_segments bit-for-bit); ±elev_rad are the
// pitch-ladder rungs (the plane offset — the ghost-horizon linear math treats
// the elevation cone as its offset plane). `reach_frac <= 0` = the full-width
// line (the horizon); a positive value clamps each emitted segment to that NDC
// radius from screen center so ladder rungs read SHORT. Same sky-side tick and
// center-gap convention as the horizon. NaN-free at zenith/nadir; a rung whose
// plane never crosses the (clipped) view returns count = 0.
GhostHorizonResult ghost_plane_segments(const glm::dvec3& cam_fwd,
                                        const glm::dvec3& cam_up,
                                        const glm::dvec3& local_up,
                                        double fovy_rad, double aspect,
                                        double shift_ndc,
                                        double center_gap_frac, double elev_rad,
                                        double reach_frac);

// GHOST PITCH LADDER (the angle indicator): a SHORT rung at elevation
// `elev_rad` above/below the level plane, in the SAME NdcSegment shape as the
// ghost horizon. The caller draws rungs at ±15/±30/±45° in the ember color,
// dimmer than the main line, each with the sky-side tick. Delegates to
// ghost_plane_segments with a fixed middle-of-view reach clamp. Gated by the
// SAME cue_horizon_alpha dial (no new dial — the ladder is part of the horizon
// cue). A rung whose cone is out of view returns 0 segments; zenith/nadir-safe.
//
// The rung reach clamp [NDC radius from center]. In the header so the reach
// tests read the SAME constant the mechanism uses (single source — a test
// pinning a copied 0.45 would silently disarm on a retune). MUST stay above
// cue_horizon_gap_frac: a gap >= the reach annihilates every rung (gap cut
// leaves only pieces the reach clip then removes) while the horizon survives
// — a silent-disappearance the gate cannot see.
inline constexpr double kRungReachFrac = 0.45;
GhostHorizonResult ghost_ladder(const glm::dvec3& cam_fwd,
                                const glm::dvec3& cam_up,
                                const glm::dvec3& local_up, double fovy_rad,
                                double aspect, double shift_ndc,
                                double center_gap_frac, double elev_rad);

// ---- Cue B: BANK ARC (REC-4, Falcon 4.0 convention) ------------------------
//
// A small arc at the TOP screen edge with fixed ticks at 0, +/-10, +/-20,
// +/-30,
// +/-45, +/-60 deg and a moving pointer showing aircraft bank phi. phi comes
// from the SHARED extraction (control::extract -> Extracted.phi, the single
// source, SPEC §7): + = right wing down. Falcon/ADI convention: the pointer
// moves OPPOSITE the roll (a "sky pointer" — bank right, wing down on the
// right, pointer swings LEFT along the arc toward the +30/+45 side on the left
// of the scale) so the sky-half of the arc stays "up." Concretely we map bank
// phi to a pointer ANGLE about the arc center measured from straight-up, with
// pointer deflecting to the pilot's-left (screen -x) for +phi (right wing
// down).
//
// The geometry is returned in a NORMALIZED arc frame (unit radius, centered at
// the arc pivot, angle measured CCW from straight up in standard screen coords
// where +x is right and +y is DOWN). The caller places/scales it at the top of
// the screen. This keeps the pure function free of pixel/layout choices.

struct BankTick {
    double bank_deg = 0.0;  // the tick's bank value
    double angle = 0.0;     // arc angle [rad], 0 = straight up, +CCW
    glm::dvec2 dir{0.0};  // unit direction from pivot (screen coords, +y DOWN)
    bool major = false;   // 0/30/45/60 drawn longer (caller styles)
};

struct BankArcGeometry {
    double pointer_angle = 0.0;   // pointer arc angle [rad], 0 = up, +CCW
    glm::dvec2 pointer_dir{0.0};  // unit pointer direction (screen coords)
    int tick_count = 0;
    std::array<BankTick, 11> ticks{};  // 0,+/-10,+/-20,+/-30,+/-45,+/-60
};

// phi = bank [rad] from control::extract (+ = right wing down). arc_span_deg =
// the half-angular extent of the arc scale in DEGREES of bank mapped onto
// `arc_sweep_rad` of physical arc (so bank +/-arc_span_deg spans the visible
// arc). A pointer beyond the scale clamps to the arc end (no wrap, no NaN).
BankArcGeometry bank_arc_geometry(double phi, double arc_span_deg,
                                  double arc_sweep_rad);

// ---- REC-6: STYLIZED COCKPIT FRAME (the STEADY-STATE REST FRAME) -----------
//
// A subtle, SCREEN-ANCHORED (static — never moves with the scene) stylized
// canopy interior at the screen periphery. It is the REST FRAME from
// docs/comfort_research.md §3.3: a stationary peripheral visual reference that
// suppresses VECTION (the sense of self-motion the moving scene induces) by
// matching the pilot's expected "I always see the inside of my cockpit"
// self-model. Because it is static and screen-locked it does NOT move with the
// camera/aim — it is pure cosmetic overlay, off every aim/camera/control path.
//
// Geometry (all NDC, all PERIPHERAL — nothing within 0.55 NDC radius of screen
// center, nothing in the top-center 30% width where the bank arc lives):
//   (a) four CORNER STRUT lines angling inward from each screen corner, each
//       DOUBLED (two parallel thin lines 0.008 NDC apart) for a machined look;
//   (b) a shallow LOWER COWLING ARC across the bottom (a polyline, apex at
//       bottom-center rising slightly) suggesting the dash;
//   (c) two short UPPER CANOPY-BOW diagonals near the top corners.
// The caller strokes the whole set in a dark steel-blue with the ember accent
// on the INNER line of each doubled pair (the rim-light look). Screen-anchored,
// so `aspect` only shapes the strut reach; the segments stay inside [-1,1]^2 at
// any aspect. PURE / deterministic (two calls are bit-equal).

struct CockpitSegment {
    glm::dvec2 a{0.0};   // NDC endpoint
    glm::dvec2 b{0.0};   // NDC endpoint
    bool inner = false;  // the INNER line of a doubled pair (ember accent)
};

struct CockpitFrame {
    // 4 corners * 2 (doubled) = 8 struts; 8 cowling-arc chords; 2 canopy bows.
    static constexpr int kMax = 18;
    int count = 0;
    std::array<CockpitSegment, kMax> segs{};
};

CockpitFrame cockpit_frame(double aspect);

// ---- S-carets: SCREEN-EDGE THREAT INDICATORS (REC-2) ----------------------
//
// For every drone/bandit NOT visible in the current view frustum, draw a small
// caret at the screen edge pointing along the direction you would turn to face
// it. Standard off-screen-indicator math (Envato Tuts+ / Elite / War Thunder):
// project the world direction into the screen basis; if it lands inside the
// frustum (with margin) => on_screen, NO caret. Otherwise place the caret on
// the screen-edge rectangle (inset by `edge_margin_ndc`) along the ray from
// screen center through the direction's screen-space AZIMUTH.
//
// PURE / raylib-free like the cues above: returns NDC geometry; the raylib glue
// in draw.cpp pixel-converts and strokes a small triangle. Uses the SHARED
// render::camera_screen_basis (single source — never a re-derived basis, the
// projection-basis-fork lesson). Nothing here touches aim/camera/control.
//
// Azimuth continuity (P0-1): the caret ray is computed in the SAME DISPLAY NDC
// frame the on_screen inset box and to_pxf use, so a threat sliding from just-
// visible to just-off-screen does NOT teleport across the lens shift. The ray
// direction is (xr/tan_x, yu/tan_y - z*shift_ndc) with z = dot(d, fwd): this is
// the display azimuth for a front dir (z>0), is CONTINUOUS through z=0, and for
// a behind dir (z<0) is the screen-plane projection (no z-division => no
// behind-flip). EXACTLY-behind / vanishing-azimuth (magnitude below a ~1e-3
// deadband) => documented BOTTOM-CENTER fallback (edge point straight down,
// angle pointing down), NEVER a NaN.

struct EdgeCaret {
    glm::dvec2 edge{0.0};  // caret position, DISPLAY NDC (on the inset rect)
    double angle = 0.0;    // pointing angle [rad] in DISPLAY NDC, atan2(y, x)
                           // of the ray center->edge (+y is UP in NDC), i.e.
                           // the direction to TURN to face the threat
    bool on_screen =
        false;            // true => inside the frustum (with margin), no caret
    bool behind = false;  // true => the direction is behind the camera
};

// dir_world = the world-space unit direction from the eye to the threat.
// cam_fwd/cam_up = the render camera basis. fovy_rad + aspect define the
// frustum; shift_ndc is the SPEC §9.2 vertical lens shift (the caret rides the
// scene, matching draw.cpp's to_pxf). edge_margin_ndc in [0, 1) insets the
// screen-edge rectangle the caret sits on (a POSITIVE margin pulls the caret
// inward from the very edge). on_screen = true when the projection lands inside
// [-1+margin, 1-margin]^2 AND in front => the caller draws NO caret. Degenerate
// (zero-length dir or forward) and exactly-behind cases are finite, never NaN.
EdgeCaret edge_caret(const glm::dvec3& dir_world, const glm::dvec3& cam_fwd,
                     const glm::dvec3& cam_up, double fovy_rad, double aspect,
                     double shift_ndc, double edge_margin_ndc);

// P1-3 HYSTERESIS overload — every instrument-selection gate is hysteretic
// (SPEC/constitution; the gunsight target-pick precedent). A drone dwelling at
// the frustum edge with the lagged-camera ripple would STROBE the on_screen
// boundary; the caret would flicker in and out. This variant applies a
// caller-threaded hysteresis band so the caret APPEARS when the projected point
// leaves the inset box, and only DISAPPEARS once it comes back inside by an
// extra `hyst_band_ndc` on each side.
//
// The function stays PURE (render/ holds no frame state): the caller passes
// `prev_visible` (the caret's shown-state from last frame, keyed by fleet slot)
// and reads out.on_screen == false to mean "draw the caret". Geometry (edge,
// angle, behind) is identical to the base overload; only the on_screen decision
// gains the band. A behind-camera dir is always off-screen (visible), band or
// not. hyst_band_ndc <= 0 reduces to the base overload bit-for-bit.
EdgeCaret edge_caret(const glm::dvec3& dir_world, const glm::dvec3& cam_fwd,
                     const glm::dvec3& cam_up, double fovy_rad, double aspect,
                     double shift_ndc, double edge_margin_ndc,
                     bool prev_visible, double hyst_band_ndc);

}  // namespace render
