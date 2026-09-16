#pragma once

// M-KEY BUBBLE MAP — pure projection math for the full-screen top-down
// faction map overlay (Chad's ask, 2026-07-25). Kept ENTIRELY free of raylib
// so it can link into seads_tests without pulling in the GPU — the actual
// screen-space draw calls (DrawTriangleFan/DrawLineStrip/DrawText) live in
// render/draw.cpp's 2D HUD stage, which #includes this header for the math.
// Pure DISPLAY: reads baked world constants + live Bubble fields, writes
// nothing, feeds nothing back into mouse->aim/control (the S-reticle/RA9
// discipline — same firewall class as every other HUD read in draw.h).
//
// PROJECTION: azimuthal-equidistant (aeqd) about a FIXED map center — the
// midpoint of world::kValleyCenterDir / kSudburyCenterDir (normalized), so
// both faction ellipses sit roughly centered on the map with the vacuum gap
// between them. For unit dir u: theta = acos(dot(c,u)), t = normalize(u -
// c*dot(c,u)), X = R*theta*dot(t,east), Y = R*theta*dot(t,north). North-up on
// screen (higher Y = further north); X east = right. The caller (draw.cpp)
// flips Y for raylib's y-down pixel space.
//
// NORTH REFERENCE: this codebase's one global convention (offline_tool/
// sudbury_geo.py's SudburyFrame: REF_N=(0,1,0), REF_E=(1,0,0) in the render
// sphere's own fixed basis — the SAME axes world/tunnel_geo.h's baked tunnel
// mouths and world/faction_bubbles.h's major_axis anchors were derived
// against, verified there to reproduce the tunnel mouths bit-for-bit). At an
// arbitrary map center this is realized as the tangent-plane projection of
// those two FIXED axes: north = normalize(REF_N - c*dot(REF_N,c)), east =
// normalize(cross(north, c)) (the standard local-up/north/east handedness:
// at the equator-analog point up=(1,0,0), north=(0,0,1), cross(north,up) =
// (0,1,0) = east — verified against two baked landmarks in
// test_bubble_map.cpp: Capreol reads NORTH-EAST of Chelmsford, Whitefish
// reads SOUTH-WEST of the Sudbury town center).
//
// ELLIPSE BOUNDARY: mirrors sim/aero.h's atm_frac_at ellipse branch (the
// r_eff(theta) = a*b / sqrt((b*cos_th)^2 + (a*sin_th)^2) polar-form radius,
// theta = bearing of the query tangent direction relative to major_axis)
// EXACTLY — same a/b/cos_th/sin_th roles, same variable names — so the drawn
// outline is the identical curve atm_frac_at treats as the bubble's edge, not
// a re-derived approximation. Sampling walks bearing linearly (not the
// membership test), so it is a distinct code path from sim/aero.h's — this is
// the accepted class of render-side geometry duplication (H1 binds sim-
// semantic PLANT math; a display-only boundary walk of an already-baked
// ellipse is not a second plant).

#include <cmath>
#include <vector>

#include <glm/glm.hpp>

#include "world/faction_bubbles.h"

namespace render {

// The map's fixed top-down projection frame.
struct MapProj {
    glm::dvec3 center{0.0, 1.0, 0.0};
    glm::dvec3 east{1.0, 0.0, 0.0};
    glm::dvec3 north{0.0, 0.0, 1.0};
    double R = 15000.0;
};

// Builds the tangent-plane (east, north) basis at `center_in` (need not be
// pre-normalized) by projecting the codebase's two FIXED global reference
// axes (REF_N/REF_E, see the header banner) into the tangent plane. Guards
// the degenerate case where `center` coincides with REF_N (a pole of the
// reference frame) by seeding off REF_E instead — never hit in practice
// (the Sudbury map center is nowhere near that reference pole), but keeps
// the function total.
inline MapProj make_map_proj(const glm::dvec3& center_in, double R) {
    const glm::dvec3 c = glm::normalize(center_in);
    const glm::dvec3 ref_n{0.0, 1.0, 0.0};
    const glm::dvec3 ref_e{1.0, 0.0, 0.0};

    glm::dvec3 n = ref_n - c * glm::dot(ref_n, c);
    const double n_len = glm::length(n);

    glm::dvec3 north, east;
    if (n_len > 1e-9) {
        north = n / n_len;
        east = glm::normalize(glm::cross(north, c));
    } else {
        glm::dvec3 e0 = ref_e - c * glm::dot(ref_e, c);
        east = glm::normalize(e0);
        north = glm::normalize(glm::cross(c, east));
    }

    MapProj mp;
    mp.center = c;
    mp.north = north;
    mp.east = east;
    mp.R = R;
    return mp;
}

// The map center used by the M-key overlay: the midpoint of the two faction
// bubble centers (normalized) — puts both ellipses + the vacuum gap roughly
// centered on screen.
inline glm::dvec3 default_map_center() {
    return glm::normalize(world::kValleyCenterDir + world::kSudburyCenterDir);
}

// aeqd projection of a world unit direction into map-plane meters
// (X east-positive, Y north-positive). Returns (0,0) for u == center (the
// tangent direction is undefined there; every other direction is well-formed
// since theta < pi almost everywhere relevant on this small a/R region).
inline glm::dvec2 project_to_map(const MapProj& mp, const glm::dvec3& u_in) {
    const glm::dvec3 u = glm::normalize(u_in);
    const double c = glm::clamp(glm::dot(mp.center, u), -1.0, 1.0);
    const double theta = std::acos(c);
    glm::dvec3 t = u - mp.center * c;
    const double t_len = glm::length(t);
    if (t_len < 1e-12) {
        return glm::dvec2(0.0, 0.0);
    }
    t /= t_len;
    const double x = mp.R * theta * glm::dot(t, mp.east);
    const double y = mp.R * theta * glm::dot(t, mp.north);
    return glm::dvec2(x, y);
}

// ★★★ L4 — THE WAY BACK. The exact inverse of project_to_map: map-plane
// metres -> the world unit direction they came from. It is a real inverse and
// not an approximation -- the forward map writes |(x,y)| = R*theta and a unit
// tangent t, so theta = |(x,y)|/R and t = (x*east + y*north)/|(x,y)| recover
// both, and the round trip closes to floating-point noise at every zoom (that
// is the leg test_map_screen.cpp walks at three of them).
//
// ⚠ THE ORIGIN IS NOT INVERTIBLE AND SAYS SO. project_to_map collapses the map
// centre to (0,0) and every direction whose tangent part vanishes with it, so
// (0,0) maps back to mp.center and nothing else can be claimed. Same guard,
// same epsilon, same silent-zero honesty as the forward function.
inline glm::dvec3 unproject_from_map(const MapProj& mp, const glm::dvec2& p) {
    const double len = std::sqrt(p.x * p.x + p.y * p.y);
    if (len < 1e-12) return glm::normalize(mp.center);
    const double theta = len / mp.R;
    const glm::dvec3 t = mp.east * (p.x / len) + mp.north * (p.y / len);
    return glm::normalize(glm::normalize(mp.center) * std::cos(theta) +
                          t * std::sin(theta));
}

// ★★★ L4 — THE VIEW (Chad, 2026-09-01: "zoomable via mousewheel, better local
// scale ... pointer good at both large and small scales").
//
// ★ `zoom` IS A MULTIPLE OF FIT, NOT A px/m SCALE, and that is the whole
// design. The map's fit scale is a function of the WINDOW and of the two live
// faction ellipses -- it changes when the window is resized and when a bubble
// shrinks -- so a view that stored px/m would silently mean something
// different after either. Storing the multiple means zoom == 1 is "the whole
// picture" forever, the clamp is a pair of constants ([1, 64]) rather than
// something the app has to recompute from the renderer's geometry, and the
// app never needs to know the fit scale at all. It is the same reasoning the
// dolly uses for its geometric distance mapping: store the ratio, not the
// metres.
struct MapView {
    double zoom = 1.0;              // multiples of the fit scale; 1 = fit
    glm::dvec2 centre_m{0.0, 0.0};  // map-plane metres at the screen centre
    bool follow_player = false;     // derived from zoom, held so it persists
};

// One wheel event. `notches` is +1 per notch UP (zoom IN). The clamp is
// applied whether or not the wheel moved, so a re-clamp after a config change
// or a window resize costs nothing and cannot leave a stale value outside the
// band. step <= 1 or a non-finite zoom collapses to the floor rather than
// running away.
inline double map_zoom_step(double zoom, int notches, double step, double zmin,
                            double zmax) {
    if (!(zoom > 0.0)) zoom = zmin;
    if (notches != 0 && step > 1.0)
        zoom *= std::pow(step, static_cast<double>(notches));
    return std::min(zmax, std::max(zmin, zoom));
}

// ★ THE SCREEN MAPPING, AND IT LIVES HERE SO THE TEST TESTS THE SHIPPED ONE.
// `render/map_screen.cpp` cannot be executed by any ctest (it is in the
// `seads` target and nothing runs the exe), so if the map's pan/zoom
// arithmetic lived inside its draw lambda the only test possible would be a
// TRANSCRIBED SECOND COPY of it -- a test that passes when the code and the
// copy agree about the wrong answer. The lambda calls these instead.
//
// `cx`/`cy` are the screen pixel the view is centred on; `centre_m` is the
// map-plane point that sits there. Screen Y is DOWN, map Y is NORTH, hence the
// one sign flip -- the only asymmetry between the pair.
struct MapScreen {
    double scale_px_per_m = 1.0;
    double cx = 0.0;
    double cy = 0.0;
    glm::dvec2 centre_m{0.0, 0.0};
};

inline glm::dvec2 map_plane_to_screen(const MapScreen& s,
                                      const glm::dvec2& p_m) {
    return glm::dvec2(s.cx + (p_m.x - s.centre_m.x) * s.scale_px_per_m,
                      s.cy - (p_m.y - s.centre_m.y) * s.scale_px_per_m);
}

// ★★★ L5 -- THE TRUE-POSITION TRAP, AS ARITHMETIC (and therefore as
// something a gate can fail on).
//
// A flak gun stands ~80 m from the pump it defends. At chart scale that is
// about TWO PIXELS, i.e. the gun's marker lands INSIDE the pump's own owner
// rings and neither symbol is readable. Chart practice is to displace the
// marker outward until it clears the assembly it belongs to; what must stay
// honest is the BEARING, and the standoff distance is symbolic -- the same
// licence every road-atlas icon takes.
//
//   pump_px     the pump glyph's centre, screen px
//   gun_px      where the gun ACTUALLY projects, screen px
//   bearing_px  the projection of a point stepped along the world tangent
//               pump -> gun. It is a projected WORLD step and not the ~2 px
//               screen delta, because at that separation the delta is
//               quantization noise and the marker would point anywhere.
//   need_px     the clearance the pump assembly wants
//
// Returns `gun_px` unchanged when the gun already clears -- a gun far from any
// pump is drawn where it is.
inline glm::dvec2 map_displace_from(const glm::dvec2& pump_px,
                                    const glm::dvec2& gun_px,
                                    const glm::dvec2& bearing_px,
                                    double need_px) {
    const glm::dvec2 d = gun_px - pump_px;
    if (glm::dot(d, d) >= need_px * need_px) return gun_px;
    glm::dvec2 b = bearing_px - pump_px;
    const double bl = std::sqrt(glm::dot(b, b));
    // Degenerate (the stepped point projected back onto the pump itself):
    // up-right, and DETERMINISTIC. A marker that picks a different direction
    // each frame is worse than one standing in a symbolic place.
    static const double kInvSqrt2 = 0.70710678118654752;
    b = bl > 1.0e-3 ? b / bl : glm::dvec2(kInvSqrt2, -kInvSqrt2);
    return pump_px + b * need_px;
}

inline glm::dvec2 map_screen_to_plane(const MapScreen& s,
                                      const glm::dvec2& px) {
    if (!(s.scale_px_per_m > 0.0)) return s.centre_m;
    return glm::dvec2(s.centre_m.x + (px.x - s.cx) / s.scale_px_per_m,
                      s.centre_m.y - (px.y - s.cy) / s.scale_px_per_m);
}

// ⚠ THE THRESHOLD IS STRICT, AND THE PLAN SAYS SO: §4.4 reads "at zoom > 4x
// fit the view follows the player" while the DECLUTTER row beside it reads
// ">= 4x". The asymmetry is transcribed, not invented -- with a 1.25 step off
// 1.0 no reachable zoom is ever exactly 4, so the two spellings can never
// disagree on a real wheel; naming it here is cheaper than a reader deciding
// one of them is a typo.
inline bool map_view_follows(double zoom, double follow_zoom) {
    return zoom > follow_zoom;
}

// ★ RANGE AND BEARING FROM A BODY, in the codebase's ONE north convention.
// It reuses make_map_proj at `from` rather than re-deriving a local frame,
// so "north" on the objective label and "north" on the chart can never fork
// -- the same reason the ellipse sampler mirrors atm_frac_at instead of
// approximating it. Bearing is degrees clockwise from north in [0, 360);
// distance is the great-circle arc, which is the distance a man WALKS, not
// the chord.
struct MapRangeBearing {
    double dist_m = 0.0;
    double bearing_deg = 0.0;
};

inline MapRangeBearing map_range_bearing(const glm::dvec3& from_in,
                                         const glm::dvec3& to_in, double R) {
    MapRangeBearing rb;
    const double fl = glm::length(from_in), tl0 = glm::length(to_in);
    if (fl < 1e-12 || tl0 < 1e-12) return rb;
    const MapProj lp = make_map_proj(from_in, R);
    const glm::dvec3 u = glm::normalize(to_in);
    const double c = glm::clamp(glm::dot(lp.center, u), -1.0, 1.0);
    rb.dist_m = R * std::acos(c);
    glm::dvec3 t = u - lp.center * c;
    const double tl = glm::length(t);
    if (tl < 1e-12) return rb;
    t /= tl;
    double b = std::atan2(glm::dot(t, lp.east), glm::dot(t, lp.north)) *
               (180.0 / M_PI);
    // ⚠ THE HALF-OPEN INTERVAL IS THE POINT, AND ONE `+= 360` DOES NOT GIVE
    // IT. A bearing a hair BELOW north comes out of atan2 as -1e-14, and
    // -1e-14 + 360 rounds to exactly 360.0 -- a value that prints as "360" on
    // an objective label and fails every [0,360) claim made about it. Snap the
    // top of the range back to the bottom, where it belongs.
    b = std::fmod(b, 360.0);
    if (b < 0.0) b += 360.0;
    if (b >= 360.0) b = 0.0;
    rb.bearing_deg = b;
    return rb;
}

// A great-circle point `dist_m` from `center_in` along bearing `theta_rad`
// (0 = along major/reference axis given, increasing toward the paired
// tangent axis) — the inverse of the polar description used by the ellipse
// boundary sampler and the round-trip test ("a point 10 km east of center").
inline glm::dvec3 point_at_bearing(const glm::dvec3& center_in,
                                   const glm::dvec3& axis_a_in,
                                   double dist_m, double theta_rad, double R) {
    const glm::dvec3 c = glm::normalize(center_in);
    glm::dvec3 a = axis_a_in - c * glm::dot(axis_a_in, c);
    a = glm::normalize(a);
    const glm::dvec3 b = glm::normalize(glm::cross(c, a));
    const glm::dvec3 t = a * std::cos(theta_rad) + b * std::sin(theta_rad);
    const double arc = dist_m / R;
    return glm::normalize(c * std::cos(arc) + t * std::sin(arc));
}

// Faction ellipse parameters (mirrors sim::AtmosphereField::Bubble's ellipse
// fields exactly — same names, same units).
struct EllipseParams {
    glm::dvec3 center_dir{0.0, 1.0, 0.0};
    glm::dvec3 major_axis{1.0, 0.0, 0.0};  // unit tangent seed; re-orthogonalized
    double major_radius_m = 0.0;           // semi-major `a`
    double minor_radius_m = 0.0;           // semi-minor `b` (0 => circular, uses a)
};

// Samples `n_points` world-space directions around the ellipse boundary
// (bearing walked linearly 0..2pi relative to major_axis), using the EXACT
// r_eff(theta) polar-radius formula sim/aero.h's atm_frac_at applies to test
// membership (see the header banner). Returns an empty vector only if
// major_axis is degenerate (parallel to center_dir) — never expected for a
// baked faction ellipse. The returned loop is CLOSED (index n_points-1
// connects back to index 0 by the caller) and bearing-monotone by
// construction (theta strictly increases with i), so it cannot self-
// intersect.
inline std::vector<glm::dvec3> sample_ellipse_boundary(const EllipseParams& e,
                                                        double R,
                                                        int n_points) {
    std::vector<glm::dvec3> out;
    if (n_points < 3) return out;
    const glm::dvec3 c = glm::normalize(e.center_dir);
    glm::dvec3 m = e.major_axis - c * glm::dot(e.major_axis, c);
    const double m_len = glm::length(m);
    if (m_len < 1e-9) return out;  // degenerate axis: caller has a bad bake
    m /= m_len;
    const glm::dvec3 mn = glm::normalize(glm::cross(c, m));

    const double a = e.major_radius_m;
    const double b = (e.minor_radius_m > 0.0) ? e.minor_radius_m : a;

    out.reserve(static_cast<size_t>(n_points));
    for (int i = 0; i < n_points; ++i) {
        const double theta = (2.0 * M_PI * static_cast<double>(i)) /
                             static_cast<double>(n_points);
        const double cos_th = std::cos(theta);
        const double sin_th = std::sin(theta);
        const double denom = std::sqrt((b * cos_th) * (b * cos_th) +
                                       (a * sin_th) * (a * sin_th));
        const double r = (denom > 1e-9) ? (a * b / denom) : b;
        const glm::dvec3 t = m * cos_th + mn * sin_th;
        const double arc = r / R;
        out.push_back(glm::normalize(c * std::cos(arc) + t * std::sin(arc)));
    }
    return out;
}

// MAP ARROW HEADING (S-maparrow, Chad 2026-08-09: "correct the arrow on the
// map so the point of it faces my actual direction? It gets spun around
// sometimes").
//
// ATTRIBUTION of the spin — the arrow used to be driven by the GROUND TRACK
// (the tangential component of VELOCITY). That source has two independent
// failure modes, both of which read on screen as "spun around":
//   (1) SIGN FLIP. Velocity is not facing. In a stall / tail-slide /
//       hammerhead the plant's own alpha runs to ~180 deg (SPEC 9's
//       BALLISTIC note, "tail-slide lies") — the aircraft is FACING one way
//       and MOVING the other, so a velocity-driven arrow snaps a full 180
//       deg while the pilot's nose never moved.
//   (2) DEGENERACY. The tangential component vanishes on any near-vertical
//       trajectory (steep climb/dive, the apex of a loop, a low-speed park).
//       The old guard only bailed below 1e-6 m/s, so at, say, a 200 m/s
//       vertical dive the surviving ~1 m/s of tangential drift — pure lateral
//       noise — set the whole bearing, and the arrow free-spun.
// The fix addresses BOTH: drive the arrow from the NOSE (body forward), which
// is what "my actual direction" means to a pilot and can never disagree with
// where he is pointed; and HOLD THE LAST VALID BEARING (the sim/aero.h v-hat
// guard idiom) whenever the nose itself is too close to vertical for its
// tangential projection to carry a direction.
//
// `fwd_world` is the world-space nose (state.orientation * (0,0,-1) in this
// codebase's body convention). Since it is unit-length, the length of its
// tangential part is exactly sin(angle from local vertical), so
// `min_tan_frac` IS a sine threshold: 0.15 => hold within ~8.6 deg of
// straight up/down. 0.0 disables the hold (legacy recompute-always behaviour,
// the kill-switch arm).
//
// The held bearing lives in the caller (a display-only latch — nothing here
// feeds back into mouse->aim/control), so the function stays pure and
// unit-testable.
struct MapHeadingState {
    double rad = 0.0;    // last bearing that was well-conditioned
    bool valid = false;  // false until the first well-conditioned sample
};

// Returns the bearing to DRAW (radians, 0 = north, +pi/2 = east) and updates
// `held` in place. Before the first well-conditioned sample (e.g. spawned
// pointing straight up) it returns 0.0 = north, the same benign default the
// old function used.
inline double map_facing_heading_rad(const MapProj& mp, const glm::dvec3& pos,
                                     const glm::dvec3& fwd_world,
                                     double min_tan_frac,
                                     MapHeadingState& held) {
    const double plen = glm::length(pos);
    const double flen = glm::length(fwd_world);
    if (plen > 1e-12 && flen > 1e-12) {
        const glm::dvec3 up = pos / plen;
        const glm::dvec3 f = fwd_world / flen;
        glm::dvec3 f_tan = f - up * glm::dot(f, up);
        const double tlen = glm::length(f_tan);
        if (tlen > 1e-12 && tlen >= min_tan_frac) {
            f_tan /= tlen;
            held.rad = std::atan2(glm::dot(f_tan, mp.east),
                                  glm::dot(f_tan, mp.north));
            held.valid = true;
        }
    }
    return held.valid ? held.rad : 0.0;
}

}  // namespace render
