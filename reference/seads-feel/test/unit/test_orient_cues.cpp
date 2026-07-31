// S-cues (comfort program auto/comfort-orient, REC-3 + REC-4): the pure,
// raylib-free orientation-cue geometry. These tests pin the ghost-horizon line
// and the bank arc against FIRST PRINCIPLES at GENERIC (never world-axis-
// aligned) attitudes — the flat-frame trap: a level/axis-aligned fixture would
// let a wrong basis pass. Every assertion fires on a non-trivial path and
// requires the baseline term > eps before checking a ratio (the fixture-no-op
// killer).

#include <array>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "config/load_aircraft.h"
#include "config/load_controller.h"
#include "render/camera.h"
#include "render/orient_cues.h"

using Catch::Approx;

namespace {

// A GENERIC position on the sphere (no coordinate axis coincides with
// local_up).
glm::dvec3 generic_local_up() {
    return glm::normalize(glm::dvec3{0.37, -0.62, 0.69});
}

// A GENERIC camera forward, not parallel to local_up.
glm::dvec3 generic_forward() {
    return glm::normalize(glm::dvec3{0.81, 0.22, -0.54});
}

// Rotate `v` about unit `axis` by `ang` (Rodrigues).
glm::dvec3 rotate_about(const glm::dvec3& v, const glm::dvec3& axis,
                        double ang) {
    const glm::dvec3 a = glm::normalize(axis);
    return v * std::cos(ang) + glm::cross(a, v) * std::sin(ang) +
           a * glm::dot(a, v) * (1.0 - std::cos(ang));
}

// Build a cam_up that is (a) not parallel to fwd and (b) rolled by `roll` about
// fwd from a seed up. The seed is a generic non-axis vector so no fixture bias.
glm::dvec3 cam_up_rolled(const glm::dvec3& fwd, double roll) {
    const glm::dvec3 seed = glm::normalize(glm::dvec3{-0.2, 0.9, 0.35});
    // Make a valid up ⟂-ish to fwd, then roll it about fwd.
    glm::dvec3 up0 = seed - fwd * glm::dot(seed, fwd);
    up0 = glm::normalize(up0);
    return rotate_about(up0, fwd, roll);
}

// The ghost-line NDC gradient (B, C) — the direction in which dot(d, up_l)
// INCREASES across the screen. The level line is perpendicular to this. Derived
// from the SAME projection convention as project_dir / ghost_horizon_segments.
glm::dvec2 ghost_gradient(const glm::dvec3& fwd, const glm::dvec3& cam_up,
                          const glm::dvec3& local_up, double fovy_rad,
                          double aspect) {
    const glm::dvec3 f = glm::normalize(fwd);
    glm::dvec3 r = glm::cross(f, glm::normalize(cam_up));
    r = glm::normalize(r);
    const glm::dvec3 u = glm::cross(r, f);
    const glm::dvec3 up_l = glm::normalize(local_up);
    const double tan_y = std::tan(0.5 * fovy_rad);
    const double tan_x = aspect * tan_y;
    return glm::dvec2{tan_x * glm::dot(r, up_l), tan_y * glm::dot(u, up_l)};
}

constexpr double kFovy = 60.0 * 3.14159265358979323846 / 180.0;
constexpr double kAspect = 16.0 / 9.0;

}  // namespace

TEST_CASE("ghost horizon: segment is perpendicular to the projected local_up",
          "[orient_cues][ghost]") {
    const glm::dvec3 up_l = generic_local_up();
    const glm::dvec3 fwd = generic_forward();
    const glm::dvec3 cam_up = cam_up_rolled(fwd, 0.4);  // generic roll

    const render::GhostHorizonResult gh = render::ghost_horizon_segments(
        fwd, cam_up, up_l, kFovy, kAspect, /*shift=*/0.0, /*gap=*/0.0);
    REQUIRE(gh.count >= 1);

    const glm::dvec2 grad = ghost_gradient(fwd, cam_up, up_l, kFovy, kAspect);
    // The gradient must be a real direction on this generic path (baseline term
    // > eps — the fixture-no-op guard).
    REQUIRE(glm::length(grad) > 1e-3);
    const glm::dvec2 gdir = glm::normalize(grad);

    const glm::dvec2 seg = gh.segs[0].b - gh.segs[0].a;
    REQUIRE(glm::length(seg) > 1e-3);
    const glm::dvec2 sdir = glm::normalize(seg);

    // The level line is ⊥ the gradient: |dot| ≈ 0.
    REQUIRE(std::abs(glm::dot(sdir, gdir)) < 1e-6);
}

TEST_CASE("ghost horizon: line rotates with camera roll",
          "[orient_cues][ghost]") {
    const glm::dvec3 up_l = generic_local_up();
    const glm::dvec3 fwd = generic_forward();

    const double roll_a = 0.15;
    const double roll_b = 0.15 + 0.6;  // two DISTINCT rolls
    const glm::dvec3 up_a = cam_up_rolled(fwd, roll_a);
    const glm::dvec3 up_b = cam_up_rolled(fwd, roll_b);

    const render::GhostHorizonResult ga = render::ghost_horizon_segments(
        fwd, up_a, up_l, kFovy, kAspect, 0.0, 0.0);
    const render::GhostHorizonResult gb = render::ghost_horizon_segments(
        fwd, up_b, up_l, kFovy, kAspect, 0.0, 0.0);
    REQUIRE(ga.count >= 1);
    REQUIRE(gb.count >= 1);

    const auto seg_angle = [](const render::GhostHorizonResult& g) {
        const glm::dvec2 s = g.segs[0].b - g.segs[0].a;
        return std::atan2(s.y, s.x);
    };
    // The line's orientation (mod pi, since a line has no direction) must MOVE
    // between the two rolls — REQUIRE the baseline difference > eps (kills a
    // fixture that ignores cam_up). The screen line rotates as the camera rolls
    // about forward, so a distinct roll gives a distinct line angle.
    double da = seg_angle(ga) - seg_angle(gb);
    // Fold into (-pi/2, pi/2] (line, not ray).
    while (da > 0.5 * glm::pi<double>()) da -= glm::pi<double>();
    while (da <= -0.5 * glm::pi<double>()) da += glm::pi<double>();
    REQUIRE(std::abs(da) > 0.05);

    // P1-3 SIGN PIN: the direction of the rotation, not just its magnitude.
    // Rolling the camera CCW about +forward (increasing `roll`) rotates the
    // whole IMAGE — including the projected local_up and its ⊥ level line — in
    // the OPPOSITE screen sense. We pin this by computing the expected line
    // orientation change INDEPENDENTLY from the projected-up gradient (B, C):
    // the level line is ⊥ the gradient, so its direction is (-C, B). This is a
    // separate derivation from the clip/emit path, so agreeing signs
    // cross-check the two — a wrong roll handedness in ghost_horizon flips this
    // sign.
    const auto grad_line_angle = [&](double roll) {
        const glm::dvec2 g =
            ghost_gradient(fwd, cam_up_rolled(fwd, roll), up_l, kFovy, kAspect);
        // Line direction ⊥ gradient.
        return std::atan2(g.x, -g.y);  // atan2(y=g.x, x=-g.y) of (-C, B)
    };
    double ea = grad_line_angle(roll_a) - grad_line_angle(roll_b);
    while (ea > 0.5 * glm::pi<double>()) ea -= glm::pi<double>();
    while (ea <= -0.5 * glm::pi<double>()) ea += glm::pi<double>();
    REQUIRE(std::abs(ea) > 0.05);       // the independent baseline fired
    REQUIRE((da > 0.0) == (ea > 0.0));  // SAME rotation sense as ghost line
}

TEST_CASE(
    "ghost horizon: a level direction projected by the REAL project_dir "
    "lands ON the ghost line (cross-implementation pin)",
    "[orient_cues][ghost]") {
    // P1-2: the camera-projection basis convention now lives in ONE place
    // (render::camera_screen_basis, used by BOTH project_dir and
    // ghost_horizon_segments). This leg pins that they AGREE end-to-end: take a
    // LEVEL world direction (⊥ local_up, in front of the camera) at a generic
    // attitude, project it with the REAL render::project_dir, and REQUIRE the
    // resulting NDC point lies ON the ghost segment's infinite line. If either
    // implementation forked, the point would drift off the line.
    const glm::dvec3 up_l = generic_local_up();
    const glm::dvec3 fwd = generic_forward();
    const glm::dvec3 cam_up = cam_up_rolled(fwd, 0.35);

    const render::GhostHorizonResult gh = render::ghost_horizon_segments(
        fwd, cam_up, up_l, kFovy, kAspect, /*shift=*/0.0, /*gap=*/0.0);
    REQUIRE(gh.count >= 1);

    // Build a LEVEL direction: remove the local_up component from the camera
    // forward (guaranteed ⊥ up_l and, being near fwd, in front of the camera).
    glm::dvec3 dlev = fwd - up_l * glm::dot(fwd, up_l);
    REQUIRE(glm::length(dlev) > 1e-3);  // baseline fired (not degenerate)
    dlev = glm::normalize(dlev);
    REQUIRE(std::abs(glm::dot(dlev, up_l)) < 1e-12);  // truly level

    const render::ScreenPoint sp =
        render::project_dir(dlev, fwd, cam_up, kFovy, kAspect);
    REQUIRE(sp.in_front);

    // The ghost segment's infinite line through (a, b): the point's signed
    // distance to it must be ~0 (shift 0 => projected NDC == display NDC).
    const glm::dvec2 a = gh.segs[0].a;
    const glm::dvec2 b = gh.segs[0].b;
    const glm::dvec2 dir = glm::normalize(b - a);
    const glm::dvec2 nrm{-dir.y, dir.x};  // unit line normal
    const glm::dvec2 pt{sp.x, sp.y};
    const double signed_dist = glm::dot(pt - a, nrm);
    REQUIRE(std::abs(signed_dist) < 1e-9);
}

TEST_CASE(
    "ghost horizon: sky-side tick points toward projected up (sign flips "
    "when inverted)",
    "[orient_cues][ghost]") {
    // P1-4: the bare level line is identical upright vs inverted. The sky tick
    // extends from the segment's inner endpoint along the projected +local_up
    // (the (B,C) gradient) so it points UP-screen upright and DOWN-screen
    // inverted. Pin the SIGN against the independent projected-up gradient in
    // NDC — and pin that a 180° camera roll FLIPS it.
    const glm::dvec3 up_l = generic_local_up();
    const glm::dvec3 fwd = generic_forward();

    const auto tick_vec = [&](double roll) {
        const glm::dvec3 cam_up = cam_up_rolled(fwd, roll);
        const render::GhostHorizonResult gh = render::ghost_horizon_segments(
            fwd, cam_up, up_l, kFovy, kAspect, /*shift=*/0.0, /*gap=*/0.30);
        REQUIRE(gh.count >= 1);
        // The tick as a display vector.
        const glm::dvec2 t = gh.segs[0].tick - gh.segs[0].tick_root;
        REQUIRE(glm::length(t) > 1e-4);  // the tick is a real stub
        return t;
    };

    // Upright: the tick must align with the projected +up gradient (sky side).
    const glm::dvec2 tick_up = tick_vec(0.30);
    const glm::dvec2 grad_up =
        ghost_gradient(fwd, cam_up_rolled(fwd, 0.30), up_l, kFovy, kAspect);
    REQUIRE(glm::length(grad_up) > 1e-3);
    REQUIRE(glm::dot(glm::normalize(tick_up), glm::normalize(grad_up)) >
            0.99);  // parallel to +up

    // Camera rolled 180°: the projected up gradient flips, and so must the tick
    // relative to the SAME screen — the tick's screen y-direction reverses.
    const glm::dvec2 tick_inv = tick_vec(0.30 + glm::pi<double>());
    REQUIRE((tick_up.y > 0.0) != (tick_inv.y > 0.0));  // y-direction flipped
}

TEST_CASE("ghost horizon: center gap is respected (config-relative)",
          "[orient_cues][ghost]") {
    const glm::dvec3 up_l = generic_local_up();
    const glm::dvec3 fwd = generic_forward();
    const glm::dvec3 cam_up = cam_up_rolled(fwd, 0.3);

    // Use the CONFIG value, never a hardcoded 0.30 (the config-relative-bounds
    // discipline — if the shipped default changes, this test tracks it).
    const sim::AircraftParams ap =
        cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");
    const control::ControllerParams cp =
        cfg::load_controller_toml(SEADS_CONFIG_DIR "/controller.toml", ap);
    const double gap = cp.cue_horizon_gap_frac;
    REQUIRE(gap > 0.0);  // a zero gap would make this test vacuous

    // This fixture (roll 0.3) puts the level line ~0.211 NDC from screen center
    // — INSIDE the gap 0.30 disk — so the gap-cut MUST split the line into TWO
    // segments, each with an INNER endpoint sitting on the gap circle. Merely
    // asserting endpoint length >= gap is mutation-vacuous: an UNCUT single
    // segment has clip-box endpoints with |P| ~ 1 >= gap and passes trivially.
    // Deleting the gap-cut block leaves ONE uncut segment -> count != 2 ->
    // FAIL.
    REQUIRE(gap == Approx(0.30).margin(1e-9));  // pins the fixture assumption
    const render::GhostHorizonResult gh = render::ghost_horizon_segments(
        fwd, cam_up, up_l, kFovy, kAspect, /*shift=*/0.0, gap);
    // The line dips inside the gap disk, so BOTH ends are cut: exactly 2
    // pieces.
    REQUIRE(gh.count == 2);
    for (int i = 0; i < gh.count; ++i) {
        // Endpoints are in projected NDC == display NDC (shift 0). No point may
        // sit inside the gap disk.
        REQUIRE(glm::length(gh.segs[i].a) >= gap - 1e-6);
        REQUIRE(glm::length(gh.segs[i].b) >= gap - 1e-6);
        // The INNER endpoint (closer to center) must lie ON the gap circle
        // (within a few %) — proof the cut landed AT the gap, not that the
        // clip-box endpoints merely happen to clear it.
        const double da = glm::length(gh.segs[i].a);
        const double db = glm::length(gh.segs[i].b);
        const double inner = std::min(da, db);
        REQUIRE(inner == Approx(gap).epsilon(0.03));
    }
}

TEST_CASE("ghost horizon: zenith/nadir camera -> no segments, no NaN",
          "[orient_cues][ghost]") {
    const glm::dvec3 up_l = generic_local_up();
    // Camera looking straight UP the local_up: the level plane is edge-on, its
    // projection is at infinity / no crossing -> 0 segments.
    for (double s : {+1.0, -1.0}) {
        const glm::dvec3 fwd = s * up_l;  // straight up / straight down
        const glm::dvec3 cam_up =
            glm::normalize(glm::dvec3{0.9, 0.1, 0.2});  // generic, ⟂-ish
        const render::GhostHorizonResult gh = render::ghost_horizon_segments(
            fwd, cam_up, up_l, kFovy, kAspect, 0.0, 0.3);
        REQUIRE(gh.count == 0);
        // No NaN leaked into the (unused) segment storage.
        for (const auto& seg : gh.segs) {
            REQUIRE(std::isfinite(seg.a.x));
            REQUIRE(std::isfinite(seg.a.y));
            REQUIRE(std::isfinite(seg.b.x));
            REQUIRE(std::isfinite(seg.b.y));
        }
    }
}

TEST_CASE("ghost horizon: degenerate cam_up || fwd does not NaN",
          "[orient_cues][ghost]") {
    const glm::dvec3 up_l = generic_local_up();
    const glm::dvec3 fwd = generic_forward();
    // cam_up parallel to fwd: project_dir-style seed fallback must engage; no
    // NaN, finite (or empty) result.
    const render::GhostHorizonResult gh = render::ghost_horizon_segments(
        fwd, fwd, up_l, kFovy, kAspect, 0.0, 0.2);
    for (const auto& seg : gh.segs) {
        REQUIRE(std::isfinite(seg.a.x));
        REQUIRE(std::isfinite(seg.a.y));
        REQUIRE(std::isfinite(seg.b.x));
        REQUIRE(std::isfinite(seg.b.y));
    }
}

// ---- GHOST PITCH LADDER (the angle indicator) ------------------------------
//
// The ladder rung at elevation `elev` images the PLANE dot(d, local_up) =
// sin(elev). elev = 0 is the ghost horizon (delegation must reproduce it
// bit-for-bit); ±elev are the rungs, offset along the projected sky gradient.
// GENERIC attitudes only (the flat-frame trap). No ']' or ',' in TEST_CASE
// names (the CMake catch_discover_tests lesson — a comma bundles them).

TEST_CASE("ghost ladder elev 0 delegation reproduces the horizon bit-for-bit",
          "[orient_cues][ladder]") {
    const glm::dvec3 up_l = generic_local_up();
    const glm::dvec3 fwd = generic_forward();
    const glm::dvec3 cam_up = cam_up_rolled(fwd, 0.33);  // generic roll
    const double shift = 0.09;
    const double gap = 0.30;

    // The ghost horizon delegates to ghost_plane_segments(elev=0, reach=0);
    // ghost_plane_segments at elev=0 reach=0 MUST equal ghost_horizon_segments
    // exactly (the single-source refactor — no fork).
    const render::GhostHorizonResult h = render::ghost_horizon_segments(
        fwd, cam_up, up_l, kFovy, kAspect, shift, gap);
    const render::GhostHorizonResult p = render::ghost_plane_segments(
        fwd, cam_up, up_l, kFovy, kAspect, shift, gap, /*elev=*/0.0,
        /*reach=*/0.0);
    REQUIRE(h.count >= 1);  // baseline fired (the fixture is non-trivial)
    REQUIRE(p.count == h.count);
    for (int i = 0; i < h.count; ++i) {
        REQUIRE(p.segs[i].a.x == Approx(h.segs[i].a.x).margin(1e-15));
        REQUIRE(p.segs[i].a.y == Approx(h.segs[i].a.y).margin(1e-15));
        REQUIRE(p.segs[i].b.x == Approx(h.segs[i].b.x).margin(1e-15));
        REQUIRE(p.segs[i].b.y == Approx(h.segs[i].b.y).margin(1e-15));
        REQUIRE(p.segs[i].tick.x == Approx(h.segs[i].tick.x).margin(1e-15));
        REQUIRE(p.segs[i].tick.y == Approx(h.segs[i].tick.y).margin(1e-15));
    }
}

TEST_CASE("ghost ladder a plus-15 rung line pins the projection convention",
          "[orient_cues][ladder]") {
    // Cross-implementation pin (the pattern the horizon uses). The rung images
    // the PLANE-OFFSET model dot(d, up_l) = sin(elev) with d in the SAME
    // f-component-1 parametrization the projection uses: d = f + X*tan_x*r +
    // Y*tan_y*u, projected NDC (X, Y). Build such a plane direction from a
    // point ON the rung line, project it through the REAL render::project_dir,
    // and REQUIRE it lands back ON the line — if ghost_ladder forked the
    // projection basis or the offset, the point drifts off. (A generic UNIT
    // +15° dir does NOT land exactly on the line — the plane offset is a
    // documented linear model of the cone, exact only in this parametrization;
    // that is the model the task specifies and the code draws.)
    const glm::dvec3 up_l = generic_local_up();
    const glm::dvec3 fwd = generic_forward();
    const glm::dvec3 cam_up = cam_up_rolled(fwd, 0.28);
    const double elev = 15.0 * glm::pi<double>() / 180.0;

    // Pin the PROJECTION math on the rung's full LINE (reach = 0, no short
    // clamp) — ghost_ladder's reach clamp is a separate concern tested below.
    const render::GhostHorizonResult rung = render::ghost_plane_segments(
        fwd, cam_up, up_l, kFovy, kAspect, /*shift=*/0.0, /*gap=*/0.0, elev,
        /*reach=*/0.0);
    REQUIRE(rung.count >= 1);

    // A point ON the rung segment (its midpoint) — its NDC (X, Y).
    const glm::dvec2 mid = 0.5 * (rung.segs[0].a + rung.segs[0].b);
    // Reconstruct the model direction d = f + X*tan_x*r + Y*tan_y*u (f-comp 1),
    // using the SHARED screen basis (single source, no fork).
    const render::ScreenBasis b = render::camera_screen_basis(fwd, cam_up);
    REQUIRE(b.ok);
    const double tan_y = std::tan(0.5 * kFovy);
    const double tan_x = kAspect * tan_y;
    const glm::dvec3 d = b.f + mid.x * tan_x * b.r + mid.y * tan_y * b.u;
    // It must sit on the plane offset the rung represents (independent check).
    REQUIRE(glm::dot(d, up_l) == Approx(std::sin(elev)).margin(1e-9));

    const render::ScreenPoint sp =
        render::project_dir(glm::normalize(d), fwd, cam_up, kFovy, kAspect);
    REQUIRE(sp.in_front);

    const glm::dvec2 a = rung.segs[0].a;
    const glm::dvec2 bb = rung.segs[0].b;
    const glm::dvec2 dir = glm::normalize(bb - a);
    const glm::dvec2 nrm{-dir.y, dir.x};
    const glm::dvec2 pt{sp.x, sp.y};
    const double signed_dist = glm::dot(pt - a, nrm);
    REQUIRE(std::abs(signed_dist) < 1e-9);  // lands back ON the rung line
}

TEST_CASE("ghost ladder plus rung is skyward of horizon minus rung opposite",
          "[orient_cues][ladder]") {
    // SIGN test: the +elev rung sits offset from the horizon ALONG the
    // projected sky gradient (+local_up direction on screen); the −elev rung
    // sits the OPPOSITE way. Measure each rung's midpoint offset from the
    // horizon line, projected onto the independent sky-gradient direction.
    const glm::dvec3 up_l = generic_local_up();
    const glm::dvec3 fwd = generic_forward();
    const glm::dvec3 cam_up = cam_up_rolled(fwd, 0.22);
    const double elev = 25.0 * glm::pi<double>() / 180.0;

    // Full-line rungs (reach = 0) so the offset sign is measured on the whole
    // line, independent of whether the short clamp keeps them in the disk.
    const render::GhostHorizonResult h = render::ghost_horizon_segments(
        fwd, cam_up, up_l, kFovy, kAspect, /*shift=*/0.0, /*gap=*/0.0);
    const render::GhostHorizonResult up = render::ghost_plane_segments(
        fwd, cam_up, up_l, kFovy, kAspect, 0.0, 0.0, +elev, /*reach=*/0.0);
    const render::GhostHorizonResult dn = render::ghost_plane_segments(
        fwd, cam_up, up_l, kFovy, kAspect, 0.0, 0.0, -elev, /*reach=*/0.0);
    REQUIRE(h.count >= 1);
    REQUIRE(up.count >= 1);
    REQUIRE(dn.count >= 1);

    // Independent sky gradient (from the fixture helper) — the on-screen
    // direction in which dot(d, up_l) increases.
    const glm::dvec2 grad = ghost_gradient(fwd, cam_up, up_l, kFovy, kAspect);
    REQUIRE(glm::length(grad) > 1e-3);  // baseline fired
    const glm::dvec2 g = glm::normalize(grad);

    // A point on the horizon line to measure offsets against.
    const glm::dvec2 h0 = h.segs[0].a;
    const auto mid = [](const render::GhostHorizonResult& r) {
        return 0.5 * (r.segs[0].a + r.segs[0].b);
    };
    const double off_up = glm::dot(mid(up) - h0, g);
    const double off_dn = glm::dot(mid(dn) - h0, g);
    // The +elev rung is skyward (positive along the gradient); −elev opposite.
    REQUIRE(off_up > 0.05);  // real offset (fixture-no-op guard)
    REQUIRE(off_dn < -0.05);
    REQUIRE((off_up > 0.0) != (off_dn > 0.0));  // opposite sides
}

TEST_CASE("ghost ladder rungs are shorter than the full horizon line",
          "[orient_cues][ladder]") {
    // The rung is clamped to the middle of view (reach frac) so it reads as a
    // ladder rung, not the full-width horizon. Its total length must be shorter
    // than the horizon's for the SAME attitude.
    const glm::dvec3 up_l = generic_local_up();
    const double elev = 15.0 * glm::pi<double>() / 180.0;
    // Aim the camera AT the rung's elevation (generic azimuth, generic roll).
    // The plain generic forward looks ~12 deg BELOW the horizon, putting the
    // +15 deg rung ~27 deg off-center — outside the reach disk — so the
    // ladder legitimately returns 0 segments there (rungs read near center
    // by design) and the old `if (count > 0)` guard made this whole test
    // vacuous (style red-team P1-1, the fixture-no-op class).
    const glm::dvec3 fwd_h = glm::normalize(
        generic_forward() - glm::dot(generic_forward(), up_l) * up_l);
    const glm::dvec3 fwd =
        glm::normalize(std::cos(elev) * fwd_h + std::sin(elev) * up_l);
    const glm::dvec3 cam_up = cam_up_rolled(fwd, 0.2);

    const render::GhostHorizonResult h = render::ghost_horizon_segments(
        fwd, cam_up, up_l, kFovy, kAspect, 0.0, 0.0);
    const render::GhostHorizonResult r =
        render::ghost_ladder(fwd, cam_up, up_l, kFovy, kAspect, 0.0, 0.0, elev);
    REQUIRE(h.count >= 1);
    // The rung must EXIST when the view is centered on it (the non-vacuous
    // fixture the P1-1 fix demands).
    REQUIRE(r.count >= 1);
    const auto seglen = [](const render::GhostHorizonResult& g) {
        double s = 0.0;
        for (int i = 0; i < g.count; ++i)
            s += glm::length(g.segs[i].b - g.segs[i].a);
        return s;
    };
    const double lh = seglen(h);
    REQUIRE(lh > 0.1);  // the horizon really spans the view
    REQUIRE(seglen(r) < lh);  // rung is shorter
    // The reach clamp actually FIRED: every endpoint inside the reach disk
    // (the same header constant the mechanism uses — never a copied 0.45),
    // and at least one endpoint ON the reach circle (this rung's full chord
    // crosses the disk, so a pass-through clip would leave it full-width).
    bool on_circle = false;
    for (int i = 0; i < r.count; ++i) {
        REQUIRE(glm::length(r.segs[i].a) <= render::kRungReachFrac + 1e-6);
        REQUIRE(glm::length(r.segs[i].b) <= render::kRungReachFrac + 1e-6);
        on_circle = on_circle ||
                    std::abs(glm::length(r.segs[i].a) -
                             render::kRungReachFrac) < 1e-6 ||
                    std::abs(glm::length(r.segs[i].b) -
                             render::kRungReachFrac) < 1e-6;
    }
    REQUIRE(on_circle);
}

TEST_CASE("ghost ladder respects the center gap", "[orient_cues][ladder]") {
    // A rung whose line dips inside the gap disk must be cut clear of it — no
    // endpoint inside the gap radius (same convention as the horizon).
    const sim::AircraftParams ap =
        cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");
    const control::ControllerParams cp =
        cfg::load_controller_toml(SEADS_CONFIG_DIR "/controller.toml", ap);
    const double gap = cp.cue_horizon_gap_frac;
    REQUIRE(gap > 0.0);

    const glm::dvec3 up_l = generic_local_up();
    const glm::dvec3 fwd = generic_forward();
    const glm::dvec3 cam_up = cam_up_rolled(fwd, 0.2);
    const double elev = 10.0 * glm::pi<double>() / 180.0;
    // The config gap must sit INSIDE the rung reach, or every rung is
    // annihilated (gap cut leaves only pieces the reach clip then removes)
    // while the horizon survives — the ladder would vanish silently on a gap
    // retune (style red-team P2-2, the config-relative-bounds class). This
    // REQUIRE is the loader-adjacent tripwire.
    REQUIRE(gap < render::kRungReachFrac);

    const render::GhostHorizonResult r = render::ghost_ladder(
        fwd, cam_up, up_l, kFovy, kAspect, /*shift=*/0.0, gap, elev);
    // The rung EXISTS under the gap cut (a count-free loop is vacuous when the
    // clip kills everything — the P1-1 pattern applied to this leg too).
    REQUIRE(r.count >= 1);
    for (int i = 0; i < r.count; ++i) {
        REQUIRE(glm::length(r.segs[i].a) >= gap - 1e-6);
        REQUIRE(glm::length(r.segs[i].b) >= gap - 1e-6);
    }
}

TEST_CASE("ghost ladder zenith camera gives zero segments and stays finite",
          "[orient_cues][ladder]") {
    const glm::dvec3 up_l = generic_local_up();
    for (double s : {+1.0, -1.0}) {
        const glm::dvec3 fwd = s * up_l;  // straight up / down
        const glm::dvec3 cam_up = glm::normalize(glm::dvec3{0.9, 0.1, 0.2});
        for (double elev_deg : {15.0, -15.0, 45.0, -45.0}) {
            const double elev = elev_deg * glm::pi<double>() / 180.0;
            const render::GhostHorizonResult r = render::ghost_ladder(
                fwd, cam_up, up_l, kFovy, kAspect, 0.0, 0.3, elev);
            REQUIRE(r.count == 0);
            for (const auto& seg : r.segs) {
                REQUIRE(std::isfinite(seg.a.x));
                REQUIRE(std::isfinite(seg.a.y));
                REQUIRE(std::isfinite(seg.b.x));
                REQUIRE(std::isfinite(seg.b.y));
            }
        }
    }
}

TEST_CASE("bank arc: pointer SIGN follows Falcon sky-pointer convention",
          "[orient_cues][bank]") {
    // SPEC §7: +φ = RIGHT wing down. Falcon/ADI sky pointer: the pointer swings
    // OPPOSITE the roll, toward the pilot's-LEFT (screen -x) for +φ.
    const double span = 60.0;
    const double sweep = 1.2;

    const render::BankArcGeometry level =
        render::bank_arc_geometry(0.0, span, sweep);
    // Level: pointer straight up (screen (0,-1)).
    REQUIRE(level.pointer_dir.x == Approx(0.0).margin(1e-9));
    REQUIRE(level.pointer_dir.y == Approx(-1.0).margin(1e-9));

    const double phi_right =
        30.0 * glm::pi<double>() / 180.0;  // +30° right down
    const render::BankArcGeometry right =
        render::bank_arc_geometry(phi_right, span, sweep);
    // +φ -> pointer to screen -x (pilot's left). REQUIRE a real deflection
    // (baseline > eps), then the SIGN.
    REQUIRE(std::abs(right.pointer_dir.x) > 0.05);
    REQUIRE(right.pointer_dir.x < 0.0);

    const render::BankArcGeometry left =
        render::bank_arc_geometry(-phi_right, span, sweep);
    REQUIRE(left.pointer_dir.x > 0.0);  // -φ (left wing down) -> screen +x
}

TEST_CASE("bank arc: pointer clamps to scale end, ticks placed, no NaN",
          "[orient_cues][bank]") {
    const double span = 60.0;
    const double sweep = 1.2;
    // Beyond the scale: clamp, no wrap, finite.
    const double huge = 170.0 * glm::pi<double>() / 180.0;
    const render::BankArcGeometry g =
        render::bank_arc_geometry(huge, span, sweep);
    REQUIRE(std::isfinite(g.pointer_angle));
    REQUIRE(std::abs(g.pointer_angle) <= 0.5 * sweep + 1e-9);
    // Ticks: 0,±10,±20,±30,±45,±60 all within span=60 -> 11 placed.
    REQUIRE(g.tick_count == 11);
    for (int i = 0; i < g.tick_count; ++i) {
        REQUIRE(std::isfinite(g.ticks[i].dir.x));
        REQUIRE(std::isfinite(g.ticks[i].dir.y));
        // Unit direction.
        REQUIRE(glm::length(g.ticks[i].dir) == Approx(1.0).margin(1e-9));
    }
}

TEST_CASE("bank arc: degenerate span/sweep falls back, no divide-by-zero",
          "[orient_cues][bank]") {
    const render::BankArcGeometry g = render::bank_arc_geometry(0.5, 0.0, 0.0);
    REQUIRE(std::isfinite(g.pointer_angle));
    REQUIRE(std::isfinite(g.pointer_dir.x));
    REQUIRE(std::isfinite(g.pointer_dir.y));
}

TEST_CASE(
    "bank arc: FULL-RANGE bank near/at inverted pegs the pointer, never level",
    "[orient_cues][bank]") {
    // P0-1: draw.cpp feeds bank_full (±π unfolded), NOT the folded ±90° phi. A
    // folded phi reads WINGS LEVEL while inverted (phi folds: bank 170 -> 10).
    // The FULL-RANGE input 170° must PEG the pointer at the scale limit, NOT
    // sit near center. Confirms the arc geometry clamps the out-of-scale
    // demand.
    const double span = 60.0;
    const double sweep = 1.2;
    const double peg = 0.5 * sweep;

    const double phi170 = 170.0 * glm::pi<double>() / 180.0;
    const render::BankArcGeometry g170 =
        render::bank_arc_geometry(phi170, span, sweep);
    // Pegged hard-over: |pointer_angle| == the scale limit (170 >> span 60).
    REQUIRE(g170.pointer_angle == Approx(peg).margin(1e-9));  // +side pegged
    // NOT near zero — the folded-phi bug (170 -> 10 -> tiny angle) would land
    // the pointer near straight-up. Here it is hard-over.
    REQUIRE(std::abs(g170.pointer_angle) > 0.5 * peg);

    // At EXACTLY 180°: still pegged (170 already saturates), sign stable at the
    // +limit (positive frac clamps to +1 -> +peg; the sign at 180° is
    // documented as the +side, matching approach from +φ).
    const double phi180 = glm::pi<double>();
    const render::BankArcGeometry g180 =
        render::bank_arc_geometry(phi180, span, sweep);
    REQUIRE(g180.pointer_angle == Approx(peg).margin(1e-9));
    REQUIRE(std::isfinite(g180.pointer_dir.x));
    REQUIRE(std::isfinite(g180.pointer_dir.y));

    // The NEGATIVE inverted side (-170°) pegs the OTHER end (sign preserved
    // through the clamp — no wrap).
    const render::BankArcGeometry gm170 =
        render::bank_arc_geometry(-phi170, span, sweep);
    REQUIRE(gm170.pointer_angle == Approx(-peg).margin(1e-9));
}

// ---- S-carets: screen-edge threat indicators (REC-2) -----------------------
//
// GENERIC attitude (never world-axis aligned) — the flat-frame trap. Fixtures
// build directions in the SHARED camera_screen_basis so "left"/"right"/"behind"
// are exact in screen space at a generic pose. The fixture-no-op killer: the
// left/right/up/behind fixtures must yield DISTINCT screen azimuths (asserted).

namespace {
constexpr double kMargin = 0.06;  // edge inset used by these caret fixtures
}

TEST_CASE("edge caret: dir dead-ahead is on_screen, no caret",
          "[orient_cues][caret]") {
    const glm::dvec3 fwd = generic_forward();
    const glm::dvec3 cam_up = cam_up_rolled(fwd, 0.3);
    // Straight ahead projects to screen center -> inside the frustum.
    const render::EdgeCaret ec = render::edge_caret(
        fwd, fwd, cam_up, kFovy, kAspect, /*shift=*/0.0, kMargin);
    REQUIRE(ec.on_screen);
    REQUIRE_FALSE(ec.behind);
    REQUIRE(std::isfinite(ec.edge.x));
    REQUIRE(std::isfinite(ec.edge.y));
}

TEST_CASE(
    "edge caret: 90-left sits on LEFT edge, 90-right on RIGHT edge (sign)",
    "[orient_cues][caret]") {
    const glm::dvec3 fwd = generic_forward();
    const glm::dvec3 cam_up = cam_up_rolled(fwd, 0.25);
    const render::ScreenBasis b = render::camera_screen_basis(fwd, cam_up);
    REQUIRE(b.ok);

    // A direction well to the pilot's LEFT: dominated by -right, a touch of
    // forward so it is off-screen (not exactly behind). Its screen azimuth is
    // to the left (-x).
    const glm::dvec3 dleft = glm::normalize(0.15 * b.f - b.r);
    const glm::dvec3 dright = glm::normalize(0.15 * b.f + b.r);

    const render::EdgeCaret cl =
        render::edge_caret(dleft, fwd, cam_up, kFovy, kAspect, 0.0, kMargin);
    const render::EdgeCaret cr =
        render::edge_caret(dright, fwd, cam_up, kFovy, kAspect, 0.0, kMargin);

    REQUIRE_FALSE(cl.on_screen);
    REQUIRE_FALSE(cr.on_screen);
    const double half = 1.0 - kMargin;
    // On the LEFT edge: x pinned to -half (baseline > eps guards the sign).
    REQUIRE(cl.edge.x == Approx(-half).margin(1e-9));
    REQUIRE(std::abs(cl.edge.x) > 0.1);
    // On the RIGHT edge: x pinned to +half.
    REQUIRE(cr.edge.x == Approx(half).margin(1e-9));
    // The pointing angle carries the sign too: left points to -x
    // (|angle|>pi/2), right points to +x (|angle|<pi/2). Distinct azimuths
    // (fixture-no-op kill).
    REQUIRE(std::cos(cl.angle) < 0.0);
    REQUIRE(std::cos(cr.angle) > 0.0);
    REQUIRE(std::abs(cl.angle - cr.angle) > 0.1);
}

TEST_CASE(
    "edge caret: dir 90-up/down land on TOP/BOTTOM edges (distinct azimuth)",
    "[orient_cues][caret]") {
    const glm::dvec3 fwd = generic_forward();
    const glm::dvec3 cam_up = cam_up_rolled(fwd, 0.2);
    const render::ScreenBasis b = render::camera_screen_basis(fwd, cam_up);
    REQUIRE(b.ok);
    // Screen UP is +b.u; a dir up-and-slightly-forward is off-screen high.
    const glm::dvec3 dup = glm::normalize(0.15 * b.f + b.u);
    const glm::dvec3 ddn = glm::normalize(0.15 * b.f - b.u);
    const render::EdgeCaret cu =
        render::edge_caret(dup, fwd, cam_up, kFovy, kAspect, 0.0, kMargin);
    const render::EdgeCaret cd =
        render::edge_caret(ddn, fwd, cam_up, kFovy, kAspect, 0.0, kMargin);
    const double half = 1.0 - kMargin;
    REQUIRE_FALSE(cu.on_screen);
    REQUIRE_FALSE(cd.on_screen);
    // TOP edge: display-NDC y pinned to +half (NDC +y is up); BOTTOM: -half.
    REQUIRE(cu.edge.y == Approx(half).margin(1e-9));
    REQUIRE(cd.edge.y == Approx(-half).margin(1e-9));
    // Distinct azimuths from the left/right fixtures (up points +y): angle
    // sign.
    REQUIRE(std::sin(cu.angle) > 0.0);
    REQUIRE(std::sin(cd.angle) < 0.0);
}

TEST_CASE("edge caret: exactly-behind -> bottom-center fallback, finite",
          "[orient_cues][caret]") {
    const glm::dvec3 fwd = generic_forward();
    const glm::dvec3 cam_up = cam_up_rolled(fwd, 0.3);
    const render::ScreenBasis b = render::camera_screen_basis(fwd, cam_up);
    REQUIRE(b.ok);
    // EXACTLY behind: -forward. z<0 and the screen-plane component is zero
    // (no right/up content) -> azimuth undefined -> documented bottom-center.
    const glm::dvec3 dback = -b.f;
    const render::EdgeCaret ec =
        render::edge_caret(dback, fwd, cam_up, kFovy, kAspect, 0.0, kMargin);
    REQUIRE(ec.behind);
    REQUIRE_FALSE(ec.on_screen);
    REQUIRE(std::isfinite(ec.edge.x));
    REQUIRE(std::isfinite(ec.edge.y));
    REQUIRE(std::isfinite(ec.angle));
    // Bottom-center: x == 0, y at -half, pointing straight down.
    const double half = 1.0 - kMargin;
    REQUIRE(ec.edge.x == Approx(0.0).margin(1e-9));
    REQUIRE(ec.edge.y == Approx(-half).margin(1e-9));
    REQUIRE(std::sin(ec.angle) < 0.0);  // pointing DOWN in NDC (+y up)
}

TEST_CASE("edge caret: behind-but-off-axis uses the azimuth, no NaN",
          "[orient_cues][caret]") {
    // A behind-camera dir that STILL has screen-plane content (not exactly
    // behind) must place the caret by its azimuth, finite — the standard
    // off-screen-indicator behavior for rear threats.
    const glm::dvec3 fwd = generic_forward();
    const glm::dvec3 cam_up = cam_up_rolled(fwd, 0.4);
    const render::ScreenBasis b = render::camera_screen_basis(fwd, cam_up);
    REQUIRE(b.ok);
    // Mostly behind (-f), leaning to the right (+r): azimuth to the right.
    const glm::dvec3 dbr = glm::normalize(-b.f + 0.5 * b.r);
    const render::EdgeCaret ec =
        render::edge_caret(dbr, fwd, cam_up, kFovy, kAspect, 0.0, kMargin);
    REQUIRE(ec.behind);
    REQUIRE_FALSE(ec.on_screen);
    REQUIRE(std::isfinite(ec.edge.x));
    REQUIRE(std::isfinite(ec.edge.y));
    // Right lean -> +x azimuth, on the right edge.
    REQUIRE(std::cos(ec.angle) > 0.0);
    REQUIRE(ec.edge.x > 0.0);
}

TEST_CASE("edge caret: edge respects the margin (config-relative)",
          "[orient_cues][caret]") {
    const glm::dvec3 fwd = generic_forward();
    const glm::dvec3 cam_up = cam_up_rolled(fwd, 0.2);
    const render::ScreenBasis b = render::camera_screen_basis(fwd, cam_up);
    REQUIRE(b.ok);
    const glm::dvec3 dleft = glm::normalize(0.15 * b.f - b.r);
    // Two DISTINCT margins: the pinned edge coordinate must track (1 - margin),
    // not a hardcoded 1.0 — the config-relative-bounds discipline.
    for (double m : {0.02, 0.12}) {
        const render::EdgeCaret ec =
            render::edge_caret(dleft, fwd, cam_up, kFovy, kAspect, 0.0, m);
        REQUIRE_FALSE(ec.on_screen);
        REQUIRE(ec.edge.x == Approx(-(1.0 - m)).margin(1e-9));
        // No point sits OUTSIDE the inset box.
        REQUIRE(std::abs(ec.edge.x) <= 1.0 - m + 1e-9);
        REQUIRE(std::abs(ec.edge.y) <= 1.0 - m + 1e-9);
    }
}

TEST_CASE(
    "edge caret: a dir BARELY outside the inset frustum carets on that edge",
    "[orient_cues][caret]") {
    // Build a direction whose projected NDC x is just PAST the inset edge on
    // the right: sits off-screen by a hair, so a caret appears on the RIGHT
    // edge (not on_screen). Proves the on_screen boundary is the inset box, not
    // [-1,1].
    const glm::dvec3 fwd = generic_forward();
    const glm::dvec3 cam_up = cam_up_rolled(fwd, 0.15);
    const render::ScreenBasis b = render::camera_screen_basis(fwd, cam_up);
    REQUIRE(b.ok);
    const double tan_y = std::tan(0.5 * kFovy);
    const double tan_x = kAspect * tan_y;
    // Want projected px = (dot(d,r)/dot(d,f))/tan_x = (1-m) + eps. Choose
    // dot(d,f)=cf, dot(d,r) = cf*tan_x*px_target, dot(d,u)=0.
    const double m = kMargin;
    const double px_target = (1.0 - m) + 0.02;  // just past the inset edge
    const double cf = 1.0;
    const double cr = cf * tan_x * px_target;
    const glm::dvec3 d = glm::normalize(cf * b.f + cr * b.r);
    const render::EdgeCaret ec =
        render::edge_caret(d, fwd, cam_up, kFovy, kAspect, 0.0, m);
    REQUIRE_FALSE(ec.behind);
    REQUIRE_FALSE(ec.on_screen);  // just outside the inset -> caret
    REQUIRE(ec.edge.x == Approx(1.0 - m).margin(1e-9));  // right edge

    // And a dir just INSIDE the inset (px = (1-m) - eps) is on_screen: the
    // boundary is real (not a no-op that always carets).
    const double cr_in = cf * tan_x * ((1.0 - m) - 0.02);
    const glm::dvec3 din = glm::normalize(cf * b.f + cr_in * b.r);
    const render::EdgeCaret ein =
        render::edge_caret(din, fwd, cam_up, kFovy, kAspect, 0.0, m);
    REQUIRE(ein.on_screen);
}

TEST_CASE("edge caret: hysteresis band kills the frustum-edge strobe (P1-3)",
          "[orient_cues][caret]") {
    // P1-3: a drone dwelling AT the inset edge with the lagged-camera ripple
    // would strobe the on_screen boundary without hysteresis. Sweep a direction
    // whose projected px ripples +/-0.02 around the inset right edge, thread
    // the prev-shown state through the hysteretic overload, and REQUIRE the
    // shown state changes at most ONCE across the whole dwell (no flicker).
    const glm::dvec3 fwd = generic_forward();
    const glm::dvec3 cam_up = cam_up_rolled(fwd, 0.15);
    const render::ScreenBasis b = render::camera_screen_basis(fwd, cam_up);
    REQUIRE(b.ok);
    const double tan_y = std::tan(0.5 * kFovy);
    const double tan_x = kAspect * tan_y;
    const double m = kMargin;
    const double half = 1.0 - m;
    const double band = 0.05;
    const double cf = 1.0;

    auto dir_at_px = [&](double px) {
        const double cr = cf * tan_x * px;  // dot(d,r) for projected px
        return glm::normalize(cf * b.f + cr * b.r);  // py = 0 (dead level)
    };

    // First, WITHOUT hysteresis the same ripple DOES strobe (baseline fires so
    // the test is not vacuous): count the raw on_screen transitions.
    int raw_changes = 0;
    bool raw_prev_shown = false;  // seed hidden
    bool raw_first = true, raw_last = false;
    // dwell px ripples across the edge: inside(0.98*half) ..
    // outside(1.02*half).
    const int N = 40;
    for (int rep = 0; rep < 3; ++rep) {
        for (int i = 0; i < N; ++i) {
            const double phase = std::sin(2.0 * glm::pi<double>() * i / N);
            const double px = half + 0.02 * phase;  // +/-0.02 around the edge
            const render::EdgeCaret ec = render::edge_caret(
                dir_at_px(px), fwd, cam_up, kFovy, kAspect, 0.0, m);
            const bool shown = !ec.on_screen;  // caret drawn?
            if (!raw_first && shown != raw_last) ++raw_changes;
            raw_first = false;
            raw_last = shown;
            (void)raw_prev_shown;
        }
    }
    REQUIRE(raw_changes > 1);  // the un-hysteretic gate really does strobe

    // Now WITH hysteresis threaded: at most ONE transition (it appears once and
    // stays, or stays hidden — never flickers back and forth).
    int hyst_changes = 0;
    bool prev_shown = false;  // seed hidden
    bool first = true, last = false;
    for (int rep = 0; rep < 3; ++rep) {
        for (int i = 0; i < N; ++i) {
            const double phase = std::sin(2.0 * glm::pi<double>() * i / N);
            const double px = half + 0.02 * phase;
            const render::EdgeCaret ec =
                render::edge_caret(dir_at_px(px), fwd, cam_up, kFovy, kAspect,
                                   0.0, m, prev_shown, band);
            const bool shown = !ec.on_screen;
            if (!first && shown != last) ++hyst_changes;
            first = false;
            last = shown;
            prev_shown = shown;  // thread the state
        }
    }
    REQUIRE(hyst_changes <= 1);
}

TEST_CASE("edge caret: hyst band 0 reduces to the base overload bit-for-bit",
          "[orient_cues][caret]") {
    const glm::dvec3 fwd = generic_forward();
    const glm::dvec3 cam_up = cam_up_rolled(fwd, 0.25);
    const render::ScreenBasis b = render::camera_screen_basis(fwd, cam_up);
    REQUIRE(b.ok);
    const glm::dvec3 dleft = glm::normalize(0.15 * b.f - b.r);
    for (bool prev : {false, true}) {
        const render::EdgeCaret base = render::edge_caret(
            dleft, fwd, cam_up, kFovy, kAspect, 0.0, kMargin);
        const render::EdgeCaret h = render::edge_caret(
            dleft, fwd, cam_up, kFovy, kAspect, 0.0, kMargin, prev, 0.0);
        REQUIRE(h.on_screen == base.on_screen);
        REQUIRE(h.behind == base.behind);
        REQUIRE(h.edge.x == Approx(base.edge.x).margin(1e-12));
        REQUIRE(h.edge.y == Approx(base.edge.y).margin(1e-12));
    }
}

TEST_CASE("edge caret: lens shift moves the edge point by exactly the shift",
          "[orient_cues][caret]") {
    // The caret rides the scene (SPEC §9.2): the returned edge is in the to_pxf
    // convention (projected = display + shift). A pure UP/DOWN-azimuth caret's
    // edge-y must differ from the no-shift case by exactly the shift. (KEPT
    // alongside the oblique legs below — a pure-up fixture alone is the
    // fixture-no-op trap: with xr=0 the z*shift term is invisible.)
    const glm::dvec3 fwd = generic_forward();
    const glm::dvec3 cam_up = cam_up_rolled(fwd, 0.2);
    const render::ScreenBasis b = render::camera_screen_basis(fwd, cam_up);
    REQUIRE(b.ok);
    const glm::dvec3 dup = glm::normalize(0.15 * b.f + b.u);
    const double shift = 0.07;
    const render::EdgeCaret e0 =
        render::edge_caret(dup, fwd, cam_up, kFovy, kAspect, 0.0, kMargin);
    const render::EdgeCaret es =
        render::edge_caret(dup, fwd, cam_up, kFovy, kAspect, shift, kMargin);
    // Both off-screen high; the display edge is the same, so the returned
    // (projected) edge.y differs by the shift.
    REQUIRE_FALSE(e0.on_screen);
    REQUIRE_FALSE(es.on_screen);
    REQUIRE(es.edge.y - e0.edge.y == Approx(shift).margin(1e-9));
}

TEST_CASE(
    "edge caret: OBLIQUE exit is CONTINUOUS through the visible->caret "
    "boundary at a large lens shift (P0-1)",
    "[orient_cues][caret]") {
    // P0-1 REGRESSION: the caret azimuth must be DISPLAY-NDC, not
    // projected-NDC. At a large shift (~0.46) the projected-vs-display azimuths
    // diverge, so an OBLIQUE (x != 0) threat crossing the right frustum edge
    // would TELEPORT if the ray were built in projected NDC. Build two
    // directions straddling the inset right edge at a nonzero display-y and
    // REQUIRE the caret point is continuous across the transition AND lands on
    // the RIGHT edge.
    const glm::dvec3 fwd = generic_forward();
    const glm::dvec3 cam_up = cam_up_rolled(fwd, 0.18);
    const render::ScreenBasis b = render::camera_screen_basis(fwd, cam_up);
    REQUIRE(b.ok);
    const double tan_y = std::tan(0.5 * kFovy);
    const double tan_x = kAspect * tan_y;
    const double shift = 0.46;
    const double m = kMargin;
    const double half = 1.0 - m;

    // Target DISPLAY point: py_disp = +0.5 (well inside top/bottom -> the RIGHT
    // edge is the exit), px straddling the inset right edge (half). Projected
    // py = py_disp + shift; projected point (px, py) at forward depth cf.
    const double cf = 1.0;
    const double py_disp = 0.5;
    const double py_proj = py_disp + shift;
    auto dir_at_px = [&](double px_disp) {
        const double cr = cf * tan_x * px_disp;  // dot(d,r)
        const double cu = cf * tan_y * py_proj;  // dot(d,u) (projected y)
        return glm::normalize(cf * b.f + cr * b.r + cu * b.u);
    };
    // Just INSIDE the inset edge (visible) and just OUTSIDE (carets).
    const glm::dvec3 din = dir_at_px(half - 0.01);
    const glm::dvec3 dout = dir_at_px(half + 0.01);

    const render::EdgeCaret ein =
        render::edge_caret(din, fwd, cam_up, kFovy, kAspect, shift, m);
    const render::EdgeCaret eout =
        render::edge_caret(dout, fwd, cam_up, kFovy, kAspect, shift, m);
    REQUIRE(ein.on_screen);         // just inside -> no caret
    REQUIRE_FALSE(eout.on_screen);  // just outside -> caret

    // The caret must land on the RIGHT edge (display x == +half, i.e. returned
    // projected x == +half since x carries no shift).
    REQUIRE(eout.edge.x == Approx(half).margin(1e-9));

    // CONTINUITY: the frustum-exit point (where din leaves the inset box, in
    // the to_pxf/return convention) and the caret point must nearly coincide.
    // din's projected point is inside the box by 0.01; compute its
    // return-convention display point and compare to eout.edge.
    const double px_in_disp = (glm::dot(din, b.r) / glm::dot(din, b.f)) / tan_x;
    const double py_in_disp =
        (glm::dot(din, b.u) / glm::dot(din, b.f)) / tan_y - shift;
    const glm::dvec2 exit_pt{px_in_disp, py_in_disp + shift};  // to_pxf conv
    const glm::dvec2 caret_pt = eout.edge;
    REQUIRE(glm::length(caret_pt - exit_pt) < 0.03);  // no teleport

    // BOTTOM-exit at x != 0 stays on the BOTTOM edge at shift 0.46: a dir low
    // and slightly right, off-screen low, carets on the BOTTOM (display y ==
    // -half).
    const glm::dvec3 dbot = glm::normalize(cf * b.f + 0.35 * b.r - 2.0 * b.u);
    const render::EdgeCaret ebot =
        render::edge_caret(dbot, fwd, cam_up, kFovy, kAspect, shift, m);
    REQUIRE_FALSE(ebot.on_screen);
    // Display y (returned projected y minus shift) is pinned to the bottom
    // edge.
    REQUIRE(ebot.edge.y - shift == Approx(-half).margin(1e-9));
}

// S-freelook360: render::wrap_pi is the single wrap convention for the
// UNLIMITED freelook-orbit yaw (yaw_max = 0) — the stored angle must stay in
// (-pi, pi] under arbitrary accumulation so the release decay (yaw *= k)
// always eases home the SHORT way. Pinned at the wrap boundaries and on a
// multi-revolution sweep (the "keep going around indefinitely" path).
TEST_CASE("wrap_pi wraps to the half-open pi interval at the boundaries",
          "[freelook360]") {
    constexpr double kPi = 3.14159265358979323846;
    REQUIRE(render::wrap_pi(0.0) == Approx(0.0).margin(1e-15));
    REQUIRE(render::wrap_pi(1.0) == Approx(1.0).margin(1e-15));
    REQUIRE(render::wrap_pi(-1.0) == Approx(-1.0).margin(1e-15));
    // Just past +pi wraps NEGATIVE (the short way home is leftward).
    REQUIRE(render::wrap_pi(kPi + 0.1) == Approx(-kPi + 0.1).margin(1e-12));
    // Just past -pi wraps POSITIVE.
    REQUIRE(render::wrap_pi(-kPi - 0.1) == Approx(kPi - 0.1).margin(1e-12));
    // Full revolutions collapse to the same in-range angle.
    REQUIRE(render::wrap_pi(2.0 * kPi) == Approx(0.0).margin(1e-12));
    REQUIRE(render::wrap_pi(0.7 + 6.0 * kPi) == Approx(0.7).margin(1e-11));
    REQUIRE(render::wrap_pi(0.7 - 6.0 * kPi) == Approx(0.7).margin(1e-11));
    // +pi itself stays +pi (half-open on the negative side).
    REQUIRE(render::wrap_pi(kPi) == Approx(kPi).margin(1e-12));
}

TEST_CASE("wrap_pi multi-revolution accumulation never leaves the interval",
          "[freelook360]") {
    constexpr double kPi = 3.14159265358979323846;
    // Simulate the caller loop: per-frame deltas accumulate through the wrap
    // exactly as app/main.cpp applies them (wrap applied every step). Ten full
    // turns each way; REQUIRE the invariant every step (the honest per-frame
    // contract, not just the endpoint).
    double yaw = 0.0;
    const double step = 0.113;  // generic, not a divisor of pi
    const int n = static_cast<int>(std::ceil(20.0 * kPi / step));
    for (int i = 0; i < n; ++i) {
        yaw = render::wrap_pi(yaw + step);
        REQUIRE(yaw > -kPi);
        REQUIRE(yaw <= kPi + 1e-12);
    }
    for (int i = 0; i < 2 * n; ++i) {
        yaw = render::wrap_pi(yaw - step);
        REQUIRE(yaw > -kPi);
        REQUIRE(yaw <= kPi + 1e-12);
    }
}

// ---- REC-6: stylized cockpit frame (the steady-state rest frame) -----------
//
// The cockpit frame is PERIPHERAL (nothing within 0.55 NDC of center, all
// inside the screen box) and PURE/deterministic. No ']' or ',' in TEST_CASE
// names (the CMake catch_discover_tests lesson).

TEST_CASE("cockpit frame all segments sit outside the center exclusion radius",
          "[orient_cues][cockpit]") {
    // The REST-FRAME constitution: nothing where the pilot aims. Every endpoint
    // of every segment must be >= 0.55 NDC from screen center (isotropic).
    for (double aspect : {16.0 / 9.0, 4.0 / 3.0}) {
        const render::CockpitFrame fr = render::cockpit_frame(aspect);
        REQUIRE(fr.count > 0);  // the frame really draws something
        for (int i = 0; i < fr.count; ++i) {
            REQUIRE(glm::length(fr.segs[i].a) >= 0.55);
            REQUIRE(glm::length(fr.segs[i].b) >= 0.55);
        }
    }
}

TEST_CASE("cockpit frame all segments stay inside the NDC screen box",
          "[orient_cues][cockpit]") {
    for (double aspect : {16.0 / 9.0, 4.0 / 3.0, 21.0 / 9.0}) {
        const render::CockpitFrame fr = render::cockpit_frame(aspect);
        REQUIRE(fr.count > 0);
        for (int i = 0; i < fr.count; ++i) {
            REQUIRE(std::abs(fr.segs[i].a.x) <= 1.0 + 1e-9);
            REQUIRE(std::abs(fr.segs[i].a.y) <= 1.0 + 1e-9);
            REQUIRE(std::abs(fr.segs[i].b.x) <= 1.0 + 1e-9);
            REQUIRE(std::abs(fr.segs[i].b.y) <= 1.0 + 1e-9);
        }
    }
}

TEST_CASE("cockpit frame clears the top-center bank-arc zone",
          "[orient_cues][cockpit]") {
    // The bank arc lives at top-center; the frame must keep the top-center 30%
    // width clear. Any segment endpoint with y > 0 (upper half) must have
    // |x| >= 0.30 (outside the reserved top-center 30% width).
    for (double aspect : {16.0 / 9.0, 4.0 / 3.0}) {
        const render::CockpitFrame fr = render::cockpit_frame(aspect);
        for (int i = 0; i < fr.count; ++i) {
            if (fr.segs[i].a.y > 0.0) REQUIRE(std::abs(fr.segs[i].a.x) >= 0.30);
            if (fr.segs[i].b.y > 0.0) REQUIRE(std::abs(fr.segs[i].b.x) >= 0.30);
        }
    }
}

TEST_CASE("cockpit frame is pure and deterministic across two calls",
          "[orient_cues][cockpit]") {
    const render::CockpitFrame a = render::cockpit_frame(16.0 / 9.0);
    const render::CockpitFrame b = render::cockpit_frame(16.0 / 9.0);
    REQUIRE(a.count == b.count);
    for (int i = 0; i < a.count; ++i) {
        REQUIRE(a.segs[i].a.x == Approx(b.segs[i].a.x).margin(1e-15));
        REQUIRE(a.segs[i].a.y == Approx(b.segs[i].a.y).margin(1e-15));
        REQUIRE(a.segs[i].b.x == Approx(b.segs[i].b.x).margin(1e-15));
        REQUIRE(a.segs[i].b.y == Approx(b.segs[i].b.y).margin(1e-15));
        REQUIRE(a.segs[i].inner == b.segs[i].inner);
    }
}

TEST_CASE("cockpit frame has a doubled inner accent and scales with aspect",
          "[orient_cues][cockpit]") {
    // The doubled struts give inner (ember-accent) lines; there must be some.
    const render::CockpitFrame fr = render::cockpit_frame(16.0 / 9.0);
    int inner = 0;
    for (int i = 0; i < fr.count; ++i)
        if (fr.segs[i].inner) ++inner;
    REQUIRE(inner >= 4);  // one inner line per corner strut, at least

    // Different aspects still produce an in-box, peripheral frame (already
    // pinned above) AND a non-empty set — the aspect path does not degenerate.
    const render::CockpitFrame w = render::cockpit_frame(4.0 / 3.0);
    REQUIRE(w.count == fr.count);  // same topology, aspect only shapes reach
}
