// R4a THE MACHINE MARKER -- the beacon that makes the run back playable.
// Spec: docs/R4A_THROW_RULING_20260825.md section 2.4 (Chad 2026-08-25).

#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include "render/sled_marker.h"

namespace {
constexpr double kR = 15000.0;  // the world radius

// A GENERIC point -- deliberately NOT on a world axis. A fixture sitting on
// +Y makes a fixed-axis "up" coincide with the true local up, and the whole
// class of flat-earth mutations passes. This is the house trap (CLAUDE.md:
// "place frame tests at GENERIC points so no world axis coincides with the
// truth"), and it has bitten this repo more than once.
glm::dvec3 generic_point(double radius) {
    return glm::normalize(glm::dvec3(1.0, 1.0, 1.0)) * radius;
}
}  // namespace

TEST_CASE("marker: hidden unless shown -- the shipped frame is untouched") {
    const glm::dvec3 p = generic_point(kR);
    REQUIRE_FALSE(render::marker_geom(p, false).visible);
    REQUIRE(render::marker_geom(p, true).visible);
}

TEST_CASE("marker: the cone rises along LOCAL UP, never a world axis") {
    const glm::dvec3 p = generic_point(kR);
    const render::MarkerGeom g = render::marker_geom(p, true);
    REQUIRE(g.visible);

    const glm::dvec3 local_up = glm::normalize(p);
    const glm::dvec3 offset = g.apex - g.base;

    // The apex is directly ABOVE the machine, so the light points
    // straight down at it ...
    REQUIRE(glm::length(offset) > 0.0);
    const double align = glm::dot(glm::normalize(offset), local_up);
    REQUIRE(align > 1.0 - 1e-12);

    // ... and the base sits ON the machine, not under or beside it.
    REQUIRE(glm::length(g.base - p) < 1e-12);

    // ★ THE MUTATION THIS KILLS: `up = (0,1,0)`. At this generic point a fixed
    // world up is 54.7 degrees off the true local up, so `align` collapses to
    // ~0.577 and this leg goes red. On a +Y fixture it would pass.
    const double fixed_axis_align = glm::dot(glm::dvec3(0.0, 1.0, 0.0), local_up);
    REQUIRE(fixed_axis_align < 0.6);  // the fixture really does separate them
}

// ★ CHAD RULED 10 m, 2026-08-25, SUPERSEDING a derivation that answered the
// wrong question (clearing the 24 m canopy -- but the marker is read on foot
// from tens of metres, not from the air). This pins HIS number, and it is
// deliberately an equality: if someone "restores" the canopy derivation the
// leg goes red rather than silently growing the light back to 36 m.
TEST_CASE("marker: the light sits at Chad's 10 m, not the superseded 36") {
    const glm::dvec3 p = generic_point(kR);
    const render::MarkerGeom g = render::marker_geom(p, true);
    const double apex_alt = glm::length(g.apex) - kR;
    REQUIRE(std::fabs(apex_alt - 10.0) < 1e-9);
    REQUIRE(apex_alt < render::kTallestTreeM);  // BELOW the canopy, by ruling
}

TEST_CASE("marker: the height scales with the requested height") {
    const glm::dvec3 p = generic_point(kR);
    const render::MarkerGeom a = render::marker_geom(p, true, 10.0);
    const render::MarkerGeom b = render::marker_geom(p, true, 40.0);
    REQUIRE(std::fabs((glm::length(a.apex - a.base)) - 10.0) < 1e-9);
    REQUIRE(std::fabs((glm::length(b.apex - b.base)) - 40.0) < 1e-9);
}

// ★ IT SHINES DOWN. The cone is NARROW at the source and WIDE where it lands,
// which is what a light pointing at something looks like. Inverted, it is a
// megaphone pointing at the sky -- and the two are one argument-order slip
// apart in DrawCylinderEx, which takes (start, end, startRadius, endRadius).
TEST_CASE("marker: the cone opens DOWNWARD onto the machine") {
    const glm::dvec3 p = generic_point(kR);
    const render::MarkerGeom g = render::marker_geom(p, true);
    REQUIRE(g.base_radius_m > g.apex_radius_m);
    REQUIRE(g.apex_radius_m > 0.0);  // a true zero end renders degenerate
    // The pool covers the 1.70 m chassis end to end, with spill onto the snow.
    REQUIRE(g.base_radius_m * 2.0 > 1.70);
}

// ★ CHAD RULED THE COLUMN OUT 2026-08-25 (it was a wall from the chase
// camera). MarkerGeom carries a lamp and nothing else -- if a column is ever
// re-added it must be his call, not a quiet re-import of the pump idiom.
TEST_CASE("marker: there is NO column -- lamp only, by ruling") {
    const glm::dvec3 p = generic_point(kR);
    const render::MarkerGeom g = render::marker_geom(p, true);
    // The only geometry the marker describes is a lamp at a point. The base is
    // the anchor, not a drawn end -- nothing spans between them.
    REQUIRE(g.visible);
    REQUIRE(g.apex_radius_m > 0.0);
    // The rejected shape was 36 m tall and 0.45 m wide at its BASE -- tall,
    // narrow, and standing UP out of the machine. This one is short and opens
    // downward. Pinning both facts means a quiet re-import of the pump idiom
    // cannot pass as "the marker".
    REQUIRE(glm::length(g.apex - g.base) < 12.0);
    REQUIRE(g.base_radius_m > 0.45);
}

TEST_CASE("marker: degenerate inputs report invisible, never a NaN cone") {
    // A NaN here would poison the draw for the whole frame. normalize(0) is
    // NaN, so the origin must be refused rather than normalized.
    const render::MarkerGeom o = render::marker_geom(glm::dvec3(0.0), true);
    REQUIRE_FALSE(o.visible);
    REQUIRE(std::isfinite(o.apex.x));
    REQUIRE(std::isfinite(o.apex.y));
    REQUIRE(std::isfinite(o.apex.z));

    // A zero or negative height is a misconfiguration, not a request for a
    // zero-length cylinder.
    const glm::dvec3 p = generic_point(kR);
    REQUIRE_FALSE(render::marker_geom(p, true, 0.0).visible);
    REQUIRE_FALSE(render::marker_geom(p, true, -5.0).visible);
}
