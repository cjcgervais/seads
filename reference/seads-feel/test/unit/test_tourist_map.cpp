// TOURIST-MAP BASEMAP — pure geometry helpers (render/tourist_map.h). Pins:
//  - polyline splitting: a synthetic ribbon-path centerline made of TWO
//    disjoint segments (the second segment's s resets toward 0) must split
//    into 2 polylines, never 1 merged line spanning the gap between them
//    (the exact defect class the task calls out: a spurious line crossing
//    the map).
//  - polyline splitting also fires on a large physical gap even when s
//    keeps increasing (the second, non-s-based split trigger).
//  - a single, uninterrupted segment (s strictly increasing, stations close
//    together) stays ONE polyline, not spuriously fragmented.
//  - label size-threshold culling: small lakes excluded, large included,
//    boundary-exact at the threshold itself.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <glm/glm.hpp>
#include <vector>

#include "render/tourist_map.h"

namespace {
constexpr double kR = 15000.0;
}

TEST_CASE("tourist_map: split_into_polylines splits on an s-reset splice") {
    // Segment A: 3 stations walking east, s = 0, 100, 200.
    // Segment B: a totally different 3 stations (walking north from the
    // ORIGIN), s = 0, 50, 100 — a real second real-world road concatenated
    // into the same baked path buffer.
    std::vector<render::CenterlineSample> cl;
    for (int i = 0; i < 3; ++i) {
        const double theta = static_cast<double>(i) * (100.0 / kR);
        render::CenterlineSample s;
        s.dir = glm::normalize(
            glm::dvec3(std::sin(theta), 0.0, std::cos(theta)));
        s.s = static_cast<double>(i) * 100.0;
        cl.push_back(s);
    }
    // Segment B walks NORTH, starting 500 m north of the origin — NOT at the
    // origin itself. The 500 m offset is load-bearing in two directions and
    // must not be removed: (a) it keeps B's first station DISTINCT from every
    // station of A, so the membership check below can actually tell the two
    // segments apart (with B starting at the origin it coincided with A's
    // first station, and this leg reported a spurious cross-line against a
    // correctly-split result — the "candidate points must SEPARATE" trap, in
    // the test's own fixture); (b) it keeps the splice well INSIDE max_gap_m
    // (~539 m from A's last station vs the 3000 m threshold), so the split
    // here is driven by the `s` RESET, which is what this leg exists to pin —
    // the large-gap rule has its own leg below.
    constexpr double kBOffsetM = 500.0;
    for (int i = 0; i < 3; ++i) {
        const double theta = (kBOffsetM + static_cast<double>(i) * 50.0) / kR;
        render::CenterlineSample s;
        s.dir =
            glm::normalize(glm::dvec3(0.0, std::sin(theta), std::cos(theta)));
        s.s = static_cast<double>(i) * 50.0;
        cl.push_back(s);
    }

    const auto polys = render::split_into_polylines(cl, kR, 3000.0);
    REQUIRE(polys.size() == 2);
    CHECK(polys[0].size() == 3);
    CHECK(polys[1].size() == 3);

    // The split must NOT connect the last point of segment A to the first
    // point of segment B — i.e. no polyline may contain both a segment-A
    // and a segment-B point (the "spurious cross-map line" defect).
    const glm::dvec3 a_last = cl[2].dir;
    const glm::dvec3 b_first = cl[3].dir;
    bool cross_line_found = false;
    for (const auto& poly : polys) {
        bool has_a_last = false, has_b_first = false;
        for (const glm::dvec3& d : poly) {
            if (glm::length(d - a_last) < 1e-9) has_a_last = true;
            if (glm::length(d - b_first) < 1e-9) has_b_first = true;
        }
        if (has_a_last && has_b_first) cross_line_found = true;
    }
    CHECK_FALSE(cross_line_found);
}

TEST_CASE("tourist_map: split_into_polylines splits on a large physical gap even with increasing s") {
    // s keeps increasing monotonically across the splice (a pathological
    // bake that resets s badly), but the physical jump is enormous — the
    // gap-distance trigger must still catch it.
    std::vector<render::CenterlineSample> cl;
    render::CenterlineSample s0;
    s0.dir = glm::dvec3(0.0, 0.0, 1.0);
    s0.s = 0.0;
    cl.push_back(s0);
    render::CenterlineSample s1;
    s1.dir = glm::dvec3(0.0, 0.0, 1.0);  // same point, small s step
    s1.s = 10.0;
    cl.push_back(s1);
    render::CenterlineSample s2;
    // Antipodal-ish jump: a huge great-circle distance, s still increasing.
    s2.dir = glm::dvec3(0.0, 1.0, 0.0);
    s2.s = 20.0;
    cl.push_back(s2);
    render::CenterlineSample s3;
    s3.dir = glm::normalize(glm::dvec3(0.001, 1.0, 0.0));
    s3.s = 30.0;
    cl.push_back(s3);

    const auto polys = render::split_into_polylines(cl, kR, 3000.0);
    REQUIRE(polys.size() == 2);
    CHECK(polys[0].size() == 2);
    CHECK(polys[1].size() == 2);
}

TEST_CASE("tourist_map: split_into_polylines keeps one continuous segment as ONE polyline") {
    std::vector<render::CenterlineSample> cl;
    for (int i = 0; i < 20; ++i) {
        const double theta = static_cast<double>(i) * (10.0 / kR);
        render::CenterlineSample s;
        s.dir = glm::normalize(
            glm::dvec3(std::sin(theta), 0.0, std::cos(theta)));
        s.s = static_cast<double>(i) * 10.0;
        cl.push_back(s);
    }
    const auto polys = render::split_into_polylines(cl, kR, 3000.0);
    REQUIRE(polys.size() == 1);
    CHECK(polys[0].size() == 20);
}

TEST_CASE("tourist_map: split_into_polylines drops single-point fragments") {
    // A lone trailing station after a split (segment ends with exactly one
    // station) must not appear as a 1-point "polyline" (nothing to draw).
    std::vector<render::CenterlineSample> cl;
    render::CenterlineSample a;
    a.dir = glm::dvec3(0.0, 0.0, 1.0);
    a.s = 0.0;
    cl.push_back(a);
    render::CenterlineSample b;
    b.dir = glm::normalize(glm::dvec3(0.001, 0.0, 1.0));
    b.s = 10.0;
    cl.push_back(b);
    render::CenterlineSample c;  // new segment, single station only
    c.dir = glm::dvec3(1.0, 0.0, 0.0);
    c.s = 0.0;
    cl.push_back(c);

    const auto polys = render::split_into_polylines(cl, kR, 3000.0);
    REQUIRE(polys.size() == 1);
    CHECK(polys[0].size() == 2);
}

TEST_CASE("tourist_map: lake_label_eligible thresholds small lakes out, large lakes in") {
    CHECK_FALSE(render::lake_label_eligible(500.0, 3000.0));
    CHECK_FALSE(render::lake_label_eligible(2999.999, 3000.0));
    CHECK(render::lake_label_eligible(3000.0, 3000.0));  // boundary-exact, inclusive
    CHECK(render::lake_label_eligible(11502.4, 3000.0));
}

TEST_CASE("tourist_map: TouristMapKey equality is a plain field comparison") {
    render::TouristMapKey a;
    a.sw = 1920;
    a.sh = 1080;
    a.scale_px_per_m = 0.01;
    a.cx = 960.0;
    a.cy = 540.0;
    a.center_x = 0.1;
    a.center_y = 0.2;
    a.center_z = 0.97;
    render::TouristMapKey b = a;
    CHECK(a == b);
    b.sw = 1921;
    CHECK(a != b);
    b = a;
    b.scale_px_per_m = 0.0100001;
    CHECK(a != b);
}
