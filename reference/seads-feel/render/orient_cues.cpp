#include "render/orient_cues.h"

#include <algorithm>
#include <cmath>

#include "render/camera.h"

namespace render {

namespace {

// Clip the infinite line A + B*X + C*Y = 0 to the axis-aligned box
// [-1,1]x[-1,1] in the DISPLAY NDC frame. Returns the two endpoints; ok=false
// if the line misses the box. (Liang-Barsky-free: intersect with the 4 edges
// and keep the in-box crossings.)
struct Line2 {
    glm::dvec2 p0{0.0};
    glm::dvec2 p1{0.0};
    bool ok = false;
};

Line2 clip_line_to_box(double A, double B, double C) {
    Line2 out;
    // |gradient| ~ 0 => not a real line (level plane edge-on: no crossing).
    const double gn = std::sqrt(B * B + C * C);
    if (!(gn > 1e-12)) return out;

    glm::dvec2 pts[4];
    int n = 0;
    const double lim = 1.0;
    // Cross the left/right edges (X = -/+1): solve for Y.
    if (std::abs(C) > 1e-12) {
        for (double X : {-lim, lim}) {
            const double Y = -(A + B * X) / C;
            if (Y >= -lim - 1e-9 && Y <= lim + 1e-9 && n < 4)
                pts[n++] = {X, std::clamp(Y, -lim, lim)};
        }
    }
    // Cross the bottom/top edges (Y = -/+1): solve for X.
    if (std::abs(B) > 1e-12) {
        for (double Y : {-lim, lim}) {
            const double X = -(A + C * Y) / B;
            if (X >= -lim - 1e-9 && X <= lim + 1e-9 && n < 4)
                pts[n++] = {std::clamp(X, -lim, lim), Y};
        }
    }
    if (n < 2) return out;
    // Pick the two most-separated crossings (dedupes corner double-hits).
    int bi = 0, bj = 1;
    double best = -1.0;
    for (int i = 0; i < n; ++i)
        for (int j = i + 1; j < n; ++j) {
            const double d = glm::dot(pts[i] - pts[j], pts[i] - pts[j]);
            if (d > best) {
                best = d;
                bi = i;
                bj = j;
            }
        }
    if (!(best > 1e-12)) return out;
    out.p0 = pts[bi];
    out.p1 = pts[bj];
    out.ok = true;
    return out;
}

// Clip the DISPLAY-NDC segment p0->p1 to the disk of radius `reach` about
// screen center (the outer clamp that turns the full-width horizon line into a
// SHORT ladder rung). `reach <= 0` (or non-finite) = no clamp. Returns
// ok=false if the segment lies entirely OUTSIDE the reach disk (rung out of
// view for a cone whose visible crossing is all in the periphery).
struct SegClip {
    glm::dvec2 p0{0.0};
    glm::dvec2 p1{0.0};
    bool ok = false;
};

SegClip clip_seg_to_reach(const glm::dvec2& p0, const glm::dvec2& p1,
                          double reach) {
    SegClip out;
    if (!(reach > 0.0)) {
        out.p0 = p0;
        out.p1 = p1;
        out.ok = true;
        return out;
    }
    // |p0 + t*(p1-p0)|^2 <= reach^2 : solve the quadratic for the in-disk
    // sub-interval [t0, t1] ∩ [0,1].
    const glm::dvec2 d = p1 - p0;
    const double a = glm::dot(d, d);
    const double bb = 2.0 * glm::dot(p0, d);
    const double c = glm::dot(p0, p0) - reach * reach;
    if (a < 1e-18) {
        // Degenerate zero-length segment: in only if its point is inside.
        if (c <= 0.0) {
            out.p0 = p0;
            out.p1 = p1;
            out.ok = true;
        }
        return out;
    }
    const double disc = bb * bb - 4.0 * a * c;
    if (disc <= 0.0) return out;  // never enters the disk
    const double sq = std::sqrt(disc);
    double t0 = (-bb - sq) / (2.0 * a);
    double t1 = (-bb + sq) / (2.0 * a);
    if (t0 > t1) std::swap(t0, t1);
    t0 = std::clamp(t0, 0.0, 1.0);
    t1 = std::clamp(t1, 0.0, 1.0);
    if (!(t1 - t0 > 1e-6)) return out;  // no in-disk portion on [0,1]
    out.p0 = p0 + t0 * d;
    out.p1 = p0 + t1 * d;
    out.ok = true;
    return out;
}

}  // namespace

// The generalized ghost-plane geometry: the line where the PLANE
// dot(d, local_up) = sin(elev_rad) images into the view. elev_rad = 0 is the
// LEVEL plane (the ghost horizon); ±elev_rad are the pitch-ladder rungs (the
// plane offset — for the ghost-horizon linear math a cone is treated as its
// offset plane, exactly as documented in the header). `reach_frac <= 0` draws
// the full-width line (the horizon); a positive value clamps each emitted
// segment to that NDC radius from center so ladder rungs read SHORT. This is
// the ONE projection/clip implementation; ghost_horizon_segments and
// ghost_ladder both delegate here (no copy-paste — the single-source rule).
GhostHorizonResult ghost_plane_segments(const glm::dvec3& cam_fwd,
                                        const glm::dvec3& cam_up,
                                        const glm::dvec3& local_up,
                                        double fovy_rad, double aspect,
                                        double shift_ndc,
                                        double center_gap_frac, double elev_rad,
                                        double reach_frac) {
    GhostHorizonResult res;
    const ScreenBasis b = camera_screen_basis(cam_fwd, cam_up);
    if (!b.ok) return res;

    const double ul = glm::length(local_up);
    if (!(ul > 1e-12)) return res;
    const glm::dvec3 up_l = local_up / ul;

    const double tan_y = std::tan(0.5 * fovy_rad);
    const double tan_x = aspect * tan_y;
    if (!(tan_y > 1e-12) || !(tan_x > 1e-12)) return res;
    if (!std::isfinite(elev_rad)) return res;

    // A direction at projected NDC (X, Y) is d = f + X*tan_x*r + Y*tan_y*u
    // (up to scale). The PLANE at elevation `elev` is dot(d_hat, up_l) =
    // sin(elev); with d's f-component fixed at 1 this is the OFFSET plane
    //   dot(f,up_l) - sin(elev) + X*tan_x*dot(r,up_l) + Y*tan_y*dot(u,up_l) =
    //   0.
    // That is A + B*X + C*Y = 0 in PROJECTED NDC, with A carrying the offset.
    // Every point ON the line has dot(d,f) = 1 (f-component of d is 1) so the
    // whole clipped segment is in front — no behind-camera fold. elev = 0
    // recovers the LEVEL plane bit-for-bit.
    const double A = glm::dot(b.f, up_l) - std::sin(elev_rad);
    const double B = tan_x * glm::dot(b.r, up_l);
    const double C = tan_y * glm::dot(b.u, up_l);

    // Move to the DISPLAY NDC frame (Y_disp = Y_proj - shift), matching
    // draw.cpp's to_pxf. Substitute Y_proj = Y_disp + shift:
    //   A + B*X + C*(Y_disp + shift) = 0  ->  (A + C*shift) + B*X + C*Y_disp =
    //   0
    const double Ad = A + C * shift_ndc;
    const Line2 ln = clip_line_to_box(Ad, B, C);
    if (!ln.ok) return res;

    // Center exclusion gap (isotropic disk of radius `gap` in DISPLAY NDC,
    // centered at screen center = display origin). Cut it out of the segment.
    const double gap = std::clamp(center_gap_frac, 0.0, 0.999);
    const glm::dvec2 p0 = ln.p0;  // display-frame endpoints
    const glm::dvec2 p1 = ln.p1;

    // SKY direction in NDC: the gradient (B, C) is the direction across the
    // screen in which dot(d, up_l) INCREASES — i.e. toward the projected
    // +local_up (the sky side). A translation by shift does not rotate it, so
    // the same unit vector holds in projected and display NDC. Degenerate
    // (edge-on) is already excluded by clip_line_to_box (gn > 1e-12).
    const double gn = std::sqrt(B * B + C * C);
    const glm::dvec2 sky_dir =
        gn > 1e-12 ? glm::dvec2{B / gn, C / gn} : glm::dvec2{0.0, 0.0};
    const double kTickLen = 0.03;  // NDC

    // Parametrize P(t) = p0 + t*(p1-p0), t in [0,1]. Find the sub-interval(s)
    // OUTSIDE the gap disk |P(t)| >= gap. |P(t)|^2 = a*t^2 + b*t + c.
    // da/db are DISPLAY-NDC endpoints; the tick roots at whichever is CLOSER to
    // screen center (the inner endpoint) and points along +sky_dir. Each piece
    // is additionally clamped to the outer `reach_frac` disk (short rungs).
    const double reach = reach_frac > 0.0 ? reach_frac : 0.0;
    auto emit_display = [&](const glm::dvec2& da_in, const glm::dvec2& db_in) {
        const SegClip sc = clip_seg_to_reach(da_in, db_in, reach);
        if (!sc.ok) return;  // rung entirely past the outer reach: draw nothing
        const glm::dvec2 da = sc.p0;
        const glm::dvec2 db = sc.p1;
        // Back to PROJECTED NDC (add shift to y) so the caller's to_pxf, which
        // subtracts shift, lands it correctly.
        NdcSegment s;
        s.a = {da.x, da.y + shift_ndc};
        s.b = {db.x, db.y + shift_ndc};
        const glm::dvec2& inner =
            glm::dot(da, da) <= glm::dot(db, db) ? da : db;
        s.tick_root = {inner.x, inner.y + shift_ndc};
        s.tick = {inner.x + sky_dir.x * kTickLen,
                  inner.y + sky_dir.y * kTickLen + shift_ndc};
        if (res.count < 2) res.segs[res.count++] = s;
    };

    if (gap <= 0.0) {
        emit_display(p0, p1);
        return res;
    }

    const glm::dvec2 dseg = p1 - p0;
    const double a = glm::dot(dseg, dseg);
    const double bb = 2.0 * glm::dot(p0, dseg);
    const double c = glm::dot(p0, p0) - gap * gap;
    if (a < 1e-18) {
        // Degenerate (zero-length) — nothing to draw.
        return res;
    }
    const double disc = bb * bb - 4.0 * a * c;
    if (disc <= 0.0) {
        // The whole segment is entirely outside the gap disk (or tangent):
        // keep it whole.
        emit_display(p0, p1);
        return res;
    }
    const double sq = std::sqrt(disc);
    double t0 = (-bb - sq) / (2.0 * a);
    double t1 = (-bb + sq) / (2.0 * a);
    if (t0 > t1) std::swap(t0, t1);
    // [t0,t1] is the portion INSIDE the gap. Emit [0,t0] and [t1,1] outside it.
    const double lo = std::clamp(t0, 0.0, 1.0);
    const double hi = std::clamp(t1, 0.0, 1.0);
    const double kMinLen = 1e-4;  // drop hair-thin remnants
    if (lo > kMinLen) emit_display(p0, p0 + lo * dseg);
    if (1.0 - hi > kMinLen) emit_display(p0 + hi * dseg, p1);
    return res;
}

GhostHorizonResult ghost_horizon_segments(const glm::dvec3& cam_fwd,
                                          const glm::dvec3& cam_up,
                                          const glm::dvec3& local_up,
                                          double fovy_rad, double aspect,
                                          double shift_ndc,
                                          double center_gap_frac) {
    // The ghost horizon is the elev = 0 (LEVEL plane) special case with NO
    // outer reach clamp (full-width line). Delegates to the single generalized
    // implementation — bit-for-bit identical to the pre-refactor horizon.
    return ghost_plane_segments(cam_fwd, cam_up, local_up, fovy_rad, aspect,
                                shift_ndc, center_gap_frac, /*elev_rad=*/0.0,
                                /*reach_frac=*/0.0);
}

GhostHorizonResult ghost_ladder(const glm::dvec3& cam_fwd,
                                const glm::dvec3& cam_up,
                                const glm::dvec3& local_up, double fovy_rad,
                                double aspect, double shift_ndc,
                                double center_gap_frac, double elev_rad) {
    // A pitch-ladder rung: the plane at elevation `elev_rad`, clamped to the
    // middle of the view (kRungReachFrac NDC radius from center, orient_cues.h)
    // so it reads as a SHORT rung, not the full-width horizon line. Same
    // sky-side tick and center gap as the horizon.
    return ghost_plane_segments(cam_fwd, cam_up, local_up, fovy_rad, aspect,
                                shift_ndc, center_gap_frac, elev_rad,
                                kRungReachFrac);
}

BankArcGeometry bank_arc_geometry(double phi, double arc_span_deg,
                                  double arc_sweep_rad) {
    BankArcGeometry g;

    // Guard the config: a non-positive span/sweep collapses the mapping — fall
    // back to a sane scale rather than divide by zero.
    const double span_deg = arc_span_deg > 1e-6 ? arc_span_deg : 60.0;
    const double sweep = arc_sweep_rad > 1e-6 ? arc_sweep_rad : 1.0;

    // Map a bank value [deg] to an arc ANGLE about the pivot. 0 bank ->
    // straight up (angle 0). Falcon/ADI "sky pointer": + phi (RIGHT wing down)
    // swings the pointer to the pilot's LEFT (screen -x). In screen coords (+x
    // right, +y DOWN) the pivot is BELOW the arc looking up, so a pointer that
    // leans toward screen -x is angle = +theta measured CCW from up? In a
    // +y-down frame, rotating a direction CCW (mathematically) goes up-vector
    // (0,-1) -> toward (+x). We want +phi -> screen -x, so we NEGATE: pointer
    // screen direction = (-sin(theta), -cos(theta)) with theta = +phi mapping.
    // Encode that by giving each element an angle t = f(bank) and a unit dir
    // built the SAME way, so ticks and pointer are consistent by construction.
    const double half_sweep = 0.5 * sweep;
    auto bank_to_angle = [&](double bank_deg) {
        // Positive bank -> positive angle magnitude, direction handled in
        // dir().
        double frac = bank_deg / span_deg;  // [-1,1] across the scale
        frac = std::clamp(frac, -1.0, 1.0);
        return frac * half_sweep;  // signed arc angle
    };
    // Screen direction from the pivot for an arc angle `t`. Straight up is
    // (0,-1). +phi (right wing down) must swing to screen -x, so a POSITIVE t
    // rotates the up-vector toward -x. In +y-down coords that is:
    //   dir = ( -sin(t), -cos(t) ).
    auto angle_to_dir = [](double t) {
        return glm::dvec2{-std::sin(t), -std::cos(t)};
    };

    const double pt = bank_to_angle(phi * 180.0 / 3.14159265358979323846);
    g.pointer_angle = pt;
    g.pointer_dir = angle_to_dir(pt);

    const double marks[] = {0.0,   10.0, -10.0, 20.0, -20.0, 30.0,
                            -30.0, 45.0, -45.0, 60.0, -60.0};
    for (double m : marks) {
        if (g.tick_count >= static_cast<int>(g.ticks.size())) break;
        // Only place ticks that fall within the scale span.
        if (std::abs(m) > span_deg + 1e-9) continue;
        BankTick tk;
        tk.bank_deg = m;
        tk.angle = bank_to_angle(m);
        tk.dir = angle_to_dir(tk.angle);
        const double am = std::abs(m);
        tk.major = (am == 0.0 || am == 30.0 || am == 45.0 || am == 60.0);
        g.ticks[g.tick_count++] = tk;
    }
    return g;
}

// ---- S-carets: screen-edge threat indicators (REC-2) -----------------------
EdgeCaret edge_caret(const glm::dvec3& dir_world, const glm::dvec3& cam_fwd,
                     const glm::dvec3& cam_up, double fovy_rad, double aspect,
                     double shift_ndc, double edge_margin_ndc) {
    EdgeCaret out;

    // SINGLE-SOURCE basis (never a re-derived one — the projection-basis-fork
    // lesson). f/r/u agree bit-for-bit with project_dir and the ghost horizon.
    const ScreenBasis b = camera_screen_basis(cam_fwd, cam_up);
    if (!b.ok) {  // degenerate camera forward: bottom-center, finite.
        out.edge = {0.0,
                    -1.0 + std::clamp(edge_margin_ndc, 0.0, 0.999) + shift_ndc};
        out.angle = -0.5 * 3.14159265358979323846;  // pointing DOWN (NDC +y up)
        return out;
    }
    const double dl = glm::length(dir_world);
    if (!(dl > 1e-12)) {  // zero direction: same documented fallback.
        out.edge = {0.0,
                    -1.0 + std::clamp(edge_margin_ndc, 0.0, 0.999) + shift_ndc};
        out.angle = -0.5 * 3.14159265358979323846;
        return out;
    }
    const glm::dvec3 d = dir_world / dl;

    const double tan_y = std::tan(0.5 * fovy_rad);
    const double tan_x = aspect * tan_y;
    if (!(tan_y > 1e-12) || !(tan_x > 1e-12)) {
        out.edge = {0.0,
                    -1.0 + std::clamp(edge_margin_ndc, 0.0, 0.999) + shift_ndc};
        out.angle = -0.5 * 3.14159265358979323846;
        return out;
    }

    const double z = glm::dot(d, b.f);   // forward component
    const double xr = glm::dot(d, b.r);  // right component
    const double yu = glm::dot(d, b.u);  // up component
    out.behind = z <= 1e-9;

    // Inset half-extent of the screen-edge rectangle (DISPLAY NDC). A positive
    // margin pulls the caret inward from the very edge; clamp so it can't
    // invert.
    const double m = std::clamp(edge_margin_ndc, 0.0, 0.999);
    const double half = 1.0 - m;  // in (0.001, 1]

    // In-front: is the projected point inside the (inset) frustum? If so, no
    // caret. Projected NDC (px, py) then DISPLAY NDC py_disp = py - shift.
    if (!out.behind) {
        const double px = (xr / z) / tan_x;
        const double py = (yu / z) / tan_y;
        const double py_disp = py - shift_ndc;
        if (std::abs(px) <= half && std::abs(py_disp) <= half) {
            out.on_screen = true;
            // No caret; edge/angle left at the projected point for callers that
            // want it (unused when on_screen). Return in to_pxf convention.
            out.edge = {px, py_disp + shift_ndc};
            out.angle = std::atan2(py_disp, px);
            return out;
        }
    }

    // Off-screen (in front but outside the inset box) OR behind. The AZIMUTH is
    // the DISPLAY-NDC ray direction from screen center — it MUST match the same
    // DISPLAY frame the on_screen inset box and to_pxf use, or the caret point
    // jumps at the visible->caret transition (P0-1). For a front dir the
    // display point is (xr/(z*tan_x), yu/(z*tan_y) - shift); the ray to it,
    // scaled by z (z>0), is (xr/tan_x, yu/tan_y - z*shift). That is the DISPLAY
    // azimuth for z>0, is CONTINUOUS through z=0, and for z<0 (behind) reduces
    // to the screen-plane projection minus a small shifted-y term (no
    // z-division, so no behind-flip). Single expression, valid for all z:
    glm::dvec2 a{xr / tan_x, yu / tan_y - z * shift_ndc};
    const double al = glm::length(a);
    if (!(al > 1e-3)) {
        // EXACTLY behind (or exactly along forward off-inset): azimuth
        // undefined / vanishing (deadband ~1e-3 kills the noise-spin near the
        // pole, P2). Documented fallback = BOTTOM-CENTER, pointing down.
        out.edge = {0.0, -half + shift_ndc};
        out.angle = -0.5 * 3.14159265358979323846;
        return out;
    }
    a /= al;  // unit ray direction in DISPLAY NDC (+y up)

    // Intersect the ray  t*a  (t > 0) from screen center with the inset box
    // [-half, half]^2: the edge is at the smaller positive t of the two axis
    // hits (|t*a.x| = half or |t*a.y| = half).
    const double tx = std::abs(a.x) > 1e-12 ? half / std::abs(a.x) : 1e18;
    const double ty = std::abs(a.y) > 1e-12 ? half / std::abs(a.y) : 1e18;
    const double t = std::min(tx, ty);
    const glm::dvec2 edge_disp = a * t;  // on the inset rectangle, DISPLAY NDC

    // Return in the to_pxf convention (add shift back so to_pxf's subtract
    // lands it at edge_disp). angle is the DISPLAY-NDC ray direction (where to
    // turn), independent of the shift.
    out.edge = {edge_disp.x, edge_disp.y + shift_ndc};
    out.angle = std::atan2(a.y, a.x);
    return out;
}

EdgeCaret edge_caret(const glm::dvec3& dir_world, const glm::dvec3& cam_fwd,
                     const glm::dvec3& cam_up, double fovy_rad, double aspect,
                     double shift_ndc, double edge_margin_ndc,
                     bool prev_visible, double hyst_band_ndc) {
    EdgeCaret out = edge_caret(dir_world, cam_fwd, cam_up, fovy_rad, aspect,
                               shift_ndc, edge_margin_ndc);
    const double band = std::max(0.0, hyst_band_ndc);
    // No band, or the caret is currently HIDDEN (prev_visible == false): the
    // base decision stands — appear the instant the point leaves the inset box.
    if (band <= 0.0 || !prev_visible || out.behind) return out;
    // The caret is currently SHOWN. Keep it shown (on_screen = false) until the
    // point returns INSIDE the inset box shrunk by `band` on each side — the
    // hysteresis that kills the boundary strobe. Only relevant when the base
    // already says the point is inside the inset box (on_screen == true), where
    // out.edge is the actual projected point (to_pxf convention).
    if (out.on_screen) {
        const double m = std::clamp(edge_margin_ndc, 0.0, 0.999);
        const double inner = (1.0 - m) - band;  // the tighter "hide" box
        const glm::dvec2 disp{out.edge.x, out.edge.y - shift_ndc};
        if (inner > 0.0 &&
            (std::abs(disp.x) > inner || std::abs(disp.y) > inner))
            out.on_screen = false;  // still in the band -> keep the caret shown
    }
    return out;
}

// ---- REC-6: stylized cockpit frame (the steady-state rest frame) -----------
CockpitFrame cockpit_frame(double aspect) {
    CockpitFrame fr;
    // Guard the aspect (only used to shape the strut reach); keep in a sane
    // band so nothing scales out of the box or degenerates.
    const double asp = (std::isfinite(aspect) && aspect > 0.1 && aspect < 10.0)
                           ? aspect
                           : 16.0 / 9.0;

    auto add = [&](glm::dvec2 a, glm::dvec2 b, bool inner) {
        if (fr.count < CockpitFrame::kMax)
            fr.segs[fr.count++] = CockpitSegment{a, b, inner};
    };

    // (a) FOUR CORNER STRUTS — from each corner angling inward ~15% of screen
    // size, DOUBLED (two parallel lines kOffset apart). The inner line carries
    // the ember rim-light accent. Reach along the corner->center diagonal keeps
    // both endpoints far outside the 0.55 center-exclusion radius (a corner is
    // at radius sqrt(2) ~ 1.414; reach 0.30 pulls the inner end to ~1.1).
    constexpr double kReach = 0.30;    // NDC along the diagonal (~15% of size)
    constexpr double kOffset = 0.008;  // NDC gap between the doubled pair
    // Anchor a hair inside the exact corner so the ±kOffset doubled pair (and
    // any aspect-biased lean) never pushes an endpoint outside the [-1,1] box.
    constexpr double kCornerInset = 1.0 - 2.0 * kOffset;  // 0.984
    const glm::dvec2 corners[4] = {
        {-1.0, 1.0}, {1.0, 1.0}, {1.0, -1.0}, {-1.0, -1.0}};
    for (const glm::dvec2& cc : corners) {
        const glm::dvec2 c = cc * kCornerInset;
        // Inward direction toward screen center (unit). Multiplying x by asp
        // LEANS the strut toward horizontal in pixel space (pixel dx carries
        // another aspect factor, so the on-screen lean is ~asp²:1 — a flat
        // canopy-rail read, NOT square; square would divide by asp). The flat
        // lean is the shipped, eyeball-passed look (style red-team P2-1:
        // comment corrected to match the code, code kept).
        glm::dvec2 in{-c.x * asp, -c.y};
        in = glm::normalize(in);
        const glm::dvec2 tip = c + in * kReach;
        // Perpendicular offset for the doubled pair.
        const glm::dvec2 perp{-in.y * kOffset, in.x * kOffset};
        // Outer line (steel), then inner line (ember rim-light), offset toward
        // screen center so the accent sits on the inboard edge.
        add(c - perp, tip - perp, /*inner=*/false);
        add(c + perp, tip + perp, /*inner=*/true);
    }

    // (b) LOWER COWLING ARC — a shallow polyline across the bottom ~15%, apex
    // at bottom-center rising slightly (suggesting the dash). Spans x in
    // [-0.7, 0.7] at y ~ -0.90, apex rising to -0.84 at center. The closest
    // point to center is the apex at radius 0.84 > 0.55 (peripheral). It sits
    // at the bottom, clear of the top-center bank-arc zone.
    {
        constexpr int kN = 9;             // 9 points -> 8 chords
        constexpr double kSpan = 0.70;    // half-width in x
        constexpr double kBaseY = -0.90;  // arc ends' y
        constexpr double kRise = 0.06;    // apex lifts this much above kBaseY
        glm::dvec2 prev{0.0, 0.0};
        for (int i = 0; i < kN; ++i) {
            const double u = -1.0 + 2.0 * i / (kN - 1);  // [-1, 1]
            const double x = u * kSpan;
            const double y =
                kBaseY + kRise * (1.0 - u * u);  // parabola apex up
            const glm::dvec2 p{x, y};
            if (i > 0) add(prev, p, /*inner=*/false);
            prev = p;
        }
    }

    // (c) TWO UPPER CANOPY-BOW diagonals near the top corners — short struts
    // that read as the canopy bow. Both stay at |x| >= 0.55 (outside the
    // top-center 30% width bank-arc zone) and radius > 0.55.
    add({0.58, 0.94}, {0.74, 0.80}, /*inner=*/true);
    add({-0.58, 0.94}, {-0.74, 0.80}, /*inner=*/true);

    return fr;
}

}  // namespace render
