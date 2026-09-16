// ★★★ L4 — THE MAP (docs/PLAN_20260901_game_loop_millwright.md §4.4).
//
// WHAT THIS FILE CAN AND CANNOT REACH, SAID FIRST.
//
// `render/map_screen.cpp` -- the thing that actually draws the chart -- is in
// the `seads` app target, and no ctest runs the exe. So a test written against
// the DRAWING would be a test that cannot execute (the .blend re-export
// lesson: a gate written against a TU the gate does not link is a gate that
// passes for the wrong reason). Everything L4 added that a test must be able
// to fail on therefore lives in the two raylib-free headers the app target and
// `seads_tests` BOTH compile:
//
//   render/bubble_map.h  the aeqd projection and its new inverse, the view
//                        (zoom / centre / follow), the screen mapping pair,
//                        range-and-bearing from a body
//   render/map_style.h   the per-zoom declutter
//
// and the draw calls them rather than spelling them out, so these legs are
// about the shipped arithmetic and not about a transcribed copy of it.
//
// THE FOUR PINS THE PLAN ASKS FOR, AND THE MUTATION EACH ONE KILLS:
//
//  1. ROUND TRIP AT THREE ZOOMS. world dir -> plane -> screen px -> plane ->
//     world dir closes to floating-point noise at fit, 4x and 64x, centred and
//     following. Kills a dropped Y-flip, a swapped X/Y, a scale applied on one
//     side only, and an inverse that is a small-angle approximation rather
//     than an inverse (VERIFIED: replacing unproject_from_map's
//     `theta = len / R` with `theta = sin(len / R)` turns this red while every
//     other leg here stays green).
//  2. THE CLAMP. [fit, 64 x fit], applied whether or not the wheel moved.
//     Kills a clamp that only guards one end and a step applied additively.
//  3. THE FOLLOW THRESHOLD. Strictly above follow_zoom (§4.4's own wording;
//     see the note in map_view_follows). Kills a `>=` that would make the
//     chart follow at exactly 4x, and a threshold read off the wrong end.
//  4. THE FIT-ZOOM DECLUTTER SET. At zoom 1 every one of the five visibility
//     bools is EXACTLY what shipped before L4. Kills any threshold dialled to
//     1.0 or below (VERIFIED: `trails_zoom = 1.0` turns this red and nothing
//     else here), which is the whole of "zoom = fit draws what it draws
//     today".
//
// Plus one leg the plan does not name but the objectives depend on: range and
// bearing from the ACTIVE BODY read in this codebase's one north convention.
// An objective label that says "FIX 1200 m 090" is only worth printing if 090
// is east.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include <glm/glm.hpp>

#include "render/bubble_map.h"
#include "render/map_style.h"

using Catch::Approx;

namespace {

constexpr double kR = 15000.0;

// A world direction `dist_m` from `c` along the map frame's own tangent basis:
// (1,0) = due east on the chart, (0,1) = due north. Built from the projection
// frame the map itself uses, so a leg written with it cannot silently agree
// with a mutated basis.
glm::dvec3 offset_dir(const render::MapProj& mp, double east_m,
                      double north_m) {
    const double len = std::sqrt(east_m * east_m + north_m * north_m);
    if (len < 1e-12) return glm::normalize(mp.center);
    const glm::dvec3 t = mp.east * (east_m / len) + mp.north * (north_m / len);
    const double arc = len / mp.R;
    return glm::normalize(glm::normalize(mp.center) * std::cos(arc) +
                          t * std::sin(arc));
}

}  // namespace

TEST_CASE("map screen: screen to world round trips at fit, 4x and 64x") {
    const render::MapProj mp =
        render::make_map_proj(render::default_map_center(), kR);

    // A handful of real chart positions: the centre itself, a few kilometres
    // out in each quadrant, and one out near the far edge of the fitted view.
    const glm::dvec3 probes[] = {
        offset_dir(mp, 0.0, 0.0),          offset_dir(mp, 4200.0, 1800.0),
        offset_dir(mp, -9100.0, 3300.0),   offset_dir(mp, 2500.0, -7400.0),
        offset_dir(mp, -1200.0, -1500.0),  offset_dir(mp, 18000.0, 11000.0),
    };

    // The fit scale a 1600x900 window produces for a ~25 km half-extent, then
    // the two zooms the declutter and the follow thresholds sit at.
    const double fit = 0.5 * (900.0 - 80.0) / 25000.0;

    for (double zoom : {1.0, 4.0, 64.0}) {
        for (bool following : {false, true}) {
            for (const glm::dvec3& d : probes) {
                render::MapScreen s;
                s.scale_px_per_m = fit * zoom;
                s.cx = 800.0;
                s.cy = 450.0;
                // Following puts the centre on the probe itself, which is the
                // configuration the chart is actually in above 4x.
                s.centre_m = following ? render::project_to_map(mp, d)
                                       : glm::dvec2(0.0, 0.0);

                const glm::dvec2 plane = render::project_to_map(mp, d);
                const glm::dvec2 px = render::map_plane_to_screen(s, plane);
                const glm::dvec2 back = render::map_screen_to_plane(s, px);
                REQUIRE(back.x == Approx(plane.x).margin(1e-6));
                REQUIRE(back.y == Approx(plane.y).margin(1e-6));

                const glm::dvec3 world = render::unproject_from_map(mp, back);
                // Compare as DIRECTIONS: a dot of 1 is the only honest
                // statement about two unit vectors on a sphere.
                REQUIRE(glm::dot(world, glm::normalize(d)) ==
                        Approx(1.0).margin(1e-12));
            }
        }
    }
}

TEST_CASE("map screen: unproject is the exact inverse of project") {
    const render::MapProj mp =
        render::make_map_proj(render::default_map_center(), kR);
    // The origin is the ONE non-invertible point and the header says so: it
    // must come back as the map centre, not as a NaN and not as garbage.
    const glm::dvec3 at_origin =
        render::unproject_from_map(mp, glm::dvec2(0.0, 0.0));
    REQUIRE(glm::dot(at_origin, glm::normalize(mp.center)) ==
            Approx(1.0).margin(1e-12));

    // 10 km due east projects to (10000, 0) and comes straight back.
    const glm::dvec3 east10 = offset_dir(mp, 10000.0, 0.0);
    const glm::dvec2 p = render::project_to_map(mp, east10);
    REQUIRE(p.x == Approx(10000.0).margin(1e-6));
    REQUIRE(p.y == Approx(0.0).margin(1e-6));
    REQUIRE(glm::dot(render::unproject_from_map(mp, p), east10) ==
            Approx(1.0).margin(1e-12));
}

TEST_CASE("map screen: the wheel zoom clamps to fit and 64x fit") {
    const render::MapStyle st;  // shipped defaults
    REQUIRE(st.zoom_step == Approx(1.25));
    REQUIRE(st.zoom_max == Approx(64.0));

    // One notch in is exactly one step.
    REQUIRE(render::map_zoom_step(1.0, 1, st.zoom_step, 1.0, st.zoom_max) ==
            Approx(1.25));
    // Scrolling out at the floor stays at the floor -- fit is the whole
    // picture and there is nothing further out to see.
    REQUIRE(render::map_zoom_step(1.0, -1, st.zoom_step, 1.0, st.zoom_max) ==
            Approx(1.0));
    REQUIRE(render::map_zoom_step(1.0, -40, st.zoom_step, 1.0, st.zoom_max) ==
            Approx(1.0));
    // And it cannot run past the ceiling however long you spin.
    REQUIRE(render::map_zoom_step(1.0, 40, st.zoom_step, 1.0, st.zoom_max) ==
            Approx(64.0));

    // Walking the wheel notch by notch stays inside the band the whole way in
    // AND the whole way back out, and returns to the floor.
    double z = 1.0;
    for (int i = 0; i < 60; ++i) {
        z = render::map_zoom_step(z, 1, st.zoom_step, 1.0, st.zoom_max);
        REQUIRE(z >= 1.0);
        REQUIRE(z <= 64.0);
    }
    REQUIRE(z == Approx(64.0));
    for (int i = 0; i < 60; ++i) {
        z = render::map_zoom_step(z, -1, st.zoom_step, 1.0, st.zoom_max);
        REQUIRE(z >= 1.0);
        REQUIRE(z <= 64.0);
    }
    REQUIRE(z == Approx(1.0));

    // The clamp is applied on a zero-notch call too, so a value left outside
    // the band by a config change is pulled back rather than carried.
    REQUIRE(render::map_zoom_step(500.0, 0, st.zoom_step, 1.0, st.zoom_max) ==
            Approx(64.0));
    REQUIRE(render::map_zoom_step(0.01, 0, st.zoom_step, 1.0, st.zoom_max) ==
            Approx(1.0));
    // step <= 1 is the documented kill switch: the wheel does nothing.
    REQUIRE(render::map_zoom_step(2.0, 5, 1.0, 1.0, st.zoom_max) ==
            Approx(2.0));
}

TEST_CASE("map screen: the view follows the player only above the threshold") {
    const render::MapStyle st;
    REQUIRE(st.follow_zoom == Approx(4.0));

    REQUIRE_FALSE(render::map_view_follows(1.0, st.follow_zoom));
    REQUIRE_FALSE(render::map_view_follows(3.9, st.follow_zoom));
    // Strictly above (plan §4.4's own ">"): exactly at the threshold is still
    // a chart, not a follow-cam.
    REQUIRE_FALSE(render::map_view_follows(4.0, st.follow_zoom));
    REQUIRE(render::map_view_follows(4.0 * st.zoom_step, st.follow_zoom));
    REQUIRE(render::map_view_follows(64.0, st.follow_zoom));

    // And the reachable wheel positions cross it exactly once, in the right
    // place: the first notch past 4 follows, every notch before it does not.
    double z = 1.0;
    bool seen_follow = false;
    for (int i = 0; i < 20; ++i) {
        const bool f = render::map_view_follows(z, st.follow_zoom);
        if (f) seen_follow = true;
        REQUIRE(f == (z > 4.0));
        // once it starts following it never stops on the way in
        if (seen_follow) REQUIRE(f);
        z = render::map_zoom_step(z, 1, st.zoom_step, 1.0, st.zoom_max);
    }
    REQUIRE(seen_follow);

    // The default view is the pre-L4 chart: fit, centred, not following.
    const render::MapView v;
    REQUIRE(v.zoom == Approx(1.0));
    REQUIRE(v.centre_m.x == Approx(0.0));
    REQUIRE(v.centre_m.y == Approx(0.0));
    REQUIRE_FALSE(v.follow_player);
}

TEST_CASE("map screen: at fit zoom the declutter set is exactly todays") {
    const render::MapStyle base;  // the shipped [map] defaults
    const render::MapStyle fit = render::map_style_at_zoom(base, 1.0);

    // ★ THE PIN. All five basemap layers answer exactly what they answered
    // before L4 existed. This is the leg that goes red if any *_zoom is
    // dialled to 1.0 or below.
    REQUIRE(fit.trails_visible == base.trails_visible);
    REQUIRE(fit.minor_roads_visible == base.minor_roads_visible);
    REQUIRE(fit.rivers_visible == base.rivers_visible);
    REQUIRE(fit.zone_labels_visible == base.zone_labels_visible);
    REQUIRE(fit.place_labels_visible == base.place_labels_visible);
    // And those answers are the shipped ruling, spelled out rather than
    // implied, so the leg still means something if a default ever moves.
    REQUIRE_FALSE(fit.trails_visible);
    REQUIRE(fit.minor_roads_visible);
    REQUIRE_FALSE(fit.place_labels_visible);
    REQUIRE(render::map_declutter_bits(fit) ==
            render::map_declutter_bits(base));

    // Nothing but the three gated bools may move at ANY zoom -- the style is
    // otherwise carried through untouched.
    const render::MapStyle deep = render::map_style_at_zoom(base, 64.0);
    REQUIRE(deep.player_px == Approx(base.player_px));
    REQUIRE(deep.objective_px == Approx(base.objective_px));
    REQUIRE(deep.arrow_hold_sin == Approx(base.arrow_hold_sin));
    REQUIRE(deep.rivers_visible == base.rivers_visible);
    REQUIRE(deep.zone_labels_visible == base.zone_labels_visible);
}

TEST_CASE("map screen: layers come on at their own zoom and never go off") {
    const render::MapStyle base;

    // Trails: off at fit, on from 4x (plan §4.4's ">= 4x").
    REQUIRE_FALSE(render::map_style_at_zoom(base, 3.99).trails_visible);
    REQUIRE(render::map_style_at_zoom(base, 4.0).trails_visible);
    REQUIRE(render::map_style_at_zoom(base, 64.0).trails_visible);

    // Place labels: off until 8x.
    REQUIRE_FALSE(render::map_style_at_zoom(base, 7.99).place_labels_visible);
    REQUIRE(render::map_style_at_zoom(base, 8.0).place_labels_visible);

    // ★ THE THRESHOLDS ADD, THEY NEVER SUBTRACT. A layer Chad dialled ON stays
    // on at every zoom -- including the overview, where the threshold has not
    // been reached. A rule that could turn a layer OFF would silently
    // overrule his config, which is the opposite of what a threshold is for.
    render::MapStyle roads_off = base;
    roads_off.minor_roads_visible = false;
    REQUIRE_FALSE(render::map_style_at_zoom(roads_off, 1.0)
                      .minor_roads_visible);
    REQUIRE(render::map_style_at_zoom(roads_off, 4.0).minor_roads_visible);

    render::MapStyle trails_on = base;
    trails_on.trails_visible = true;
    REQUIRE(render::map_style_at_zoom(trails_on, 1.0).trails_visible);
    REQUIRE(render::map_style_at_zoom(trails_on, 64.0).trails_visible);
}

TEST_CASE("map screen: range and bearing read from the active body") {
    const render::MapProj mp =
        render::make_map_proj(render::default_map_center(), kR);
    const glm::dvec3 here = glm::normalize(mp.center);

    // Due east, 5 km: 5000 m at 090.
    {
        const render::MapRangeBearing rb =
            render::map_range_bearing(here, offset_dir(mp, 5000.0, 0.0), kR);
        REQUIRE(rb.dist_m == Approx(5000.0).margin(1e-6));
        REQUIRE(rb.bearing_deg == Approx(90.0).margin(1e-9));
    }
    // Due north, 1.2 km: 000 (and NOT 180 -- the sign of north is the pin).
    {
        const render::MapRangeBearing rb =
            render::map_range_bearing(here, offset_dir(mp, 0.0, 1200.0), kR);
        REQUIRE(rb.dist_m == Approx(1200.0).margin(1e-6));
        REQUIRE(rb.bearing_deg == Approx(0.0).margin(1e-9));
    }
    // Due south and due west, so a swapped or negated axis cannot hide in a
    // single quadrant.
    {
        const render::MapRangeBearing s =
            render::map_range_bearing(here, offset_dir(mp, 0.0, -800.0), kR);
        REQUIRE(s.bearing_deg == Approx(180.0).margin(1e-9));
        const render::MapRangeBearing w =
            render::map_range_bearing(here, offset_dir(mp, -800.0, 0.0), kR);
        REQUIRE(w.bearing_deg == Approx(270.0).margin(1e-9));
    }
    // Bearing is always in [0, 360) -- an objective label never prints -90.
    for (int i = 0; i < 36; ++i) {
        const double a = (2.0 * M_PI * i) / 36.0;
        const render::MapRangeBearing rb = render::map_range_bearing(
            here, offset_dir(mp, 3000.0 * std::sin(a), 3000.0 * std::cos(a)),
            kR);
        REQUIRE(rb.bearing_deg >= 0.0);
        REQUIRE(rb.bearing_deg < 360.0);
        REQUIRE(rb.dist_m == Approx(3000.0).margin(1e-6));
    }

    // ★ MEASURED FROM THE BODY, NOT FROM THE MAP CENTRE. Standing 20 km east
    // of the chart centre and looking at a point due north OF HIM must read
    // 000 -- if the frame were the map's fixed one instead of a local frame at
    // the body, it would not.
    {
        const glm::dvec3 body = offset_dir(mp, 20000.0, 0.0);
        const render::MapProj lp = render::make_map_proj(body, kR);
        const double arc = 900.0 / kR;
        const glm::dvec3 north_of_him = glm::normalize(
            glm::normalize(lp.center) * std::cos(arc) + lp.north * std::sin(arc));
        const render::MapRangeBearing rb =
            render::map_range_bearing(body, north_of_him, kR);
        REQUIRE(rb.dist_m == Approx(900.0).margin(1e-6));
        REQUIRE(rb.bearing_deg == Approx(0.0).margin(1e-9));
    }

    // Degenerate inputs are reported as zero, not as NaN.
    {
        const render::MapRangeBearing rb =
            render::map_range_bearing(glm::dvec3(0.0), here, kR);
        REQUIRE(rb.dist_m == Approx(0.0));
        REQUIRE(rb.bearing_deg == Approx(0.0));
    }
}

// ★★★ L5 — THE FLAK MARKER'S DISPLACEMENT (ported from `origin/main`'s inline
// map block, whose arithmetic this rung moved into `render/bubble_map.h` so it
// could be reached from here at all: `render/map_screen.cpp` is app-only, and
// no ctest links it — the banner at the top of this file).
//
// The rule with a right and a wrong answer is: a gun that overlaps the pump
// assembly is pushed out along the TRUE pump->gun bearing (world, projected),
// and a gun that already clears is left exactly where it projects. The bearing
// is what must be honest; the standoff is symbolic.
TEST_CASE("map screen: a flak gun beside its pump is displaced along the true bearing") {
    const glm::dvec2 pump(400.0, 300.0);
    const double need = 40.0;

    // The gun is 2 px from the pump — inside the owner rings — and the WORLD
    // bearing (carried in by a projected step) is due east on the chart.
    {
        const glm::dvec2 gun(401.4, 301.4);
        const glm::dvec2 bearing(460.0, 300.0);  // east of the pump
        const glm::dvec2 out =
            render::map_displace_from(pump, gun, bearing, need);
        // Pushed to exactly the clearance...
        REQUIRE(std::sqrt(glm::dot(out - pump, out - pump)) ==
                Approx(need).margin(1e-9));
        // ...and along the BEARING, not along the 2 px screen delta (which
        // points up-right, i.e. y BELOW the pump in y-down screen space).
        REQUIRE(out.x == Approx(pump.x + need).margin(1e-9));
        REQUIRE(out.y == Approx(pump.y).margin(1e-9));
    }

    // A different true bearing gives a different marker: due south on the
    // chart is +y in screen space. A displacement that ignored the bearing
    // (or reused the screen delta) could not tell these two cases apart.
    {
        const glm::dvec2 gun(401.4, 301.4);
        const glm::dvec2 bearing(400.0, 380.0);  // south of the pump
        const glm::dvec2 out =
            render::map_displace_from(pump, gun, bearing, need);
        REQUIRE(out.x == Approx(pump.x).margin(1e-9));
        REQUIRE(out.y == Approx(pump.y + need).margin(1e-9));
    }

    // ★ A GUN THAT ALREADY CLEARS IS DRAWN WHERE IT IS. The displacement is a
    // legibility fix for an overlap, not a permanent lie about where guns
    // stand; zoom in far enough and the marker sits on the gun.
    {
        const glm::dvec2 gun(400.0 + 120.0, 300.0 - 55.0);
        const glm::dvec2 bearing(460.0, 300.0);
        const glm::dvec2 out =
            render::map_displace_from(pump, gun, bearing, need);
        REQUIRE(out.x == Approx(gun.x));
        REQUIRE(out.y == Approx(gun.y));
    }

    // Degenerate: the stepped point projects back onto the pump itself, so
    // there is no usable bearing. The marker must still land somewhere
    // DETERMINISTIC at the right radius rather than at a NaN or jittering.
    {
        const glm::dvec2 out =
            render::map_displace_from(pump, glm::dvec2(400.5, 300.5), pump, need);
        REQUIRE(std::sqrt(glm::dot(out - pump, out - pump)) ==
                Approx(need).margin(1e-9));
        const glm::dvec2 again =
            render::map_displace_from(pump, glm::dvec2(400.5, 300.5), pump, need);
        REQUIRE(out.x == Approx(again.x));
        REQUIRE(out.y == Approx(again.y));
    }
}
