// PRECIPITATION placement core (docs/weather_seasons_plan.md W3 —
// render::precip_sample / precip_center_cell). The gate pins the LOAD-BEARING
// invariants headlessly (the module is raylib-free, pure): NO WIND (the only
// motion is along local_up — no horizontal advection), WORLD-ANCHORED (the base
// is eye-independent, so flying through the field streams flakes past for free),
// the C0 boundary fade, and determinism. The season/weather GATE + the billboard
// draw live in render/precip_draw.cpp (raylib) — verified by the --smoke shot,
// not here (the app-binary-blind gate can't reach a shader).

#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <glm/glm.hpp>

#include "render/precip.h"

using render::precip_center_cell;
using render::precip_sample;
using render::PrecipSample;

namespace {
// A generic (non-axis-aligned) local-up so a world-lattice axis never coincides
// with the truth direction (the flat-frame trap).
const glm::dvec3 kUp = glm::normalize(glm::dvec3(0.3, -0.7, 0.5));
constexpr double kCell = 3.0;
constexpr double kBoxHalf = 21.0;
constexpr double kWrap = 0.12;
}  // namespace

TEST_CASE("precip: NO WIND - the only motion is along local_up (no advection)") {
    // As the fall phase advances, a flake moves ONLY along local_up: pos(p2)-pos(p1)
    // has ZERO component perpendicular to local_up. This holds EVEN across the fall
    // wrap (the frac jump is itself along local_up). A horizontal advection term
    // (wind) would put a perpendicular component here. THE no-wind invariant.
    double max_perp = 0.0;
    for (int ci = -6; ci <= 6; ++ci)
        for (int cj = -6; cj <= 6; ++cj)
            for (int ck = -6; ck <= 6; ++ck) {
                const glm::ivec3 cell(ci, cj, ck);
                const glm::dvec3 eye(0.0);  // eye irrelevant to base+fall
                const glm::dvec3 a =
                    precip_sample(cell, eye, kUp, kCell, kBoxHalf, kWrap, 0.20).pos;
                const glm::dvec3 b =
                    precip_sample(cell, eye, kUp, kCell, kBoxHalf, kWrap, 0.55).pos;
                const glm::dvec3 delta = b - a;
                const glm::dvec3 perp = delta - kUp * glm::dot(delta, kUp);
                max_perp = std::max(max_perp, glm::length(perp));
            }
    CHECK(max_perp < 1e-9);  // exactly parallel up to double round-off
}

TEST_CASE("precip: WORLD-ANCHORED - the flake position is eye-independent") {
    // The base + fall depend on the WORLD cell and local_up, NEVER the eye (only
    // the boundary FADE uses the eye). So at a fixed cell/up/phase, two different
    // eyes see the flake at the SAME world spot — it stays put and streams past as
    // you fly, instead of a camera-frame snow-globe (which reads as wind). Kills a
    // mutant that anchors the base to the eye.
    const glm::dvec3 eyeA(1000.0, -2000.0, 3000.0);
    const glm::dvec3 eyeB(-5000.0, 8000.0, 1234.0);
    for (int ci = -4; ci <= 4; ++ci)
        for (int cj = -4; cj <= 4; ++cj) {
            const glm::ivec3 cell(ci, cj, 2);
            const glm::dvec3 pa =
                precip_sample(cell, eyeA, kUp, kCell, kBoxHalf, kWrap, 0.4).pos;
            const glm::dvec3 pb =
                precip_sample(cell, eyeB, kUp, kCell, kBoxHalf, kWrap, 0.4).pos;
            REQUIRE(pa.x == Catch::Approx(pb.x));
            REQUIRE(pa.y == Catch::Approx(pb.y));
            REQUIRE(pa.z == Catch::Approx(pb.z));
        }
}

TEST_CASE("precip: the flake falls DOWNWARD (along -local_up) within one cell") {
    // A small non-wrapping phase step moves the flake in the -local_up direction by
    // (dphase * cell_size). Find a particle whose step does not cross the wrap
    // (|delta| ~ expected), and confirm it descended. Kills a sign flip / an
    // upward or sideways drift.
    const double dphase = 0.1;
    const double expect = dphase * kCell;
    bool tested = false;
    for (int ci = 0; ci < 40 && !tested; ++ci) {
        const glm::ivec3 cell(ci, 3, -2);
        const glm::dvec3 eye(0.0);
        const glm::dvec3 a =
            precip_sample(cell, eye, kUp, kCell, kBoxHalf, kWrap, 0.30).pos;
        const glm::dvec3 b =
            precip_sample(cell, eye, kUp, kCell, kBoxHalf, kWrap, 0.40).pos;
        const glm::dvec3 delta = b - a;
        if (std::abs(glm::length(delta) - expect) < 1e-6) {  // no wrap in this step
            CHECK(glm::dot(delta, kUp) < 0.0);               // descended
            CHECK(glm::length(delta) == Catch::Approx(expect));
            tested = true;
        }
    }
    REQUIRE(tested);
}

TEST_CASE("precip: boundary fade - alpha in [0,1], 0 beyond the box, ~1 near eye") {
    // The radial boundary fade -> 0 at box_half (C0 re-binning) and is bounded
    // [0,1]. A flake far outside the box is culled (alpha 0); one at the eye centre
    // (mid fall phase, away from the wrap) is ~fully opaque.
    const glm::dvec3 eye(500.0, 500.0, 500.0);
    // Bounds over a sweep.
    for (int ci = -10; ci <= 10; ++ci)
        for (int ck = -10; ck <= 10; ++ck) {
            const glm::ivec3 cell(166 + ci, 167, 167 + ck);  // near eye/cell=~167
            const PrecipSample s =
                precip_sample(cell, eye, kUp, kCell, kBoxHalf, kWrap, 0.5);
            REQUIRE(s.alpha >= 0.0);
            REQUIRE(s.alpha <= 1.0);
        }
    // A cell far outside the box (many box-halves away) => faded to 0.
    const glm::ivec3 far(166 + 200, 167, 167);  // ~600 m away >> box_half
    CHECK(precip_sample(far, eye, kUp, kCell, kBoxHalf, kWrap, 0.5).alpha == 0.0);
}

TEST_CASE("precip: center cell snaps to round(eye / cell_size)") {
    const glm::dvec3 eye(3.4 * kCell, -7.6 * kCell, 100.5 * kCell);
    const glm::ivec3 c = precip_center_cell(eye, kCell);
    CHECK(c.x == 3);     // round(3.4)
    CHECK(c.y == -8);    // round(-7.6)
    CHECK(c.z == 101);   // round(100.5) -> 101 (lround: half away from zero)
}

TEST_CASE("precip: deterministic and pure (no state)") {
    const glm::ivec3 cell(5, -3, 9);
    const glm::dvec3 eye(10.0, 20.0, 30.0);
    const PrecipSample a =
        precip_sample(cell, eye, kUp, kCell, kBoxHalf, kWrap, 0.37);
    // churn other cells/phases
    for (int i = 0; i < 200; ++i)
        precip_sample(glm::ivec3(i, i * 2, -i), eye, kUp, kCell, kBoxHalf, kWrap,
                      i * 0.013);
    const PrecipSample b =
        precip_sample(cell, eye, kUp, kCell, kBoxHalf, kWrap, 0.37);
    CHECK(a.pos.x == b.pos.x);
    CHECK(a.pos.y == b.pos.y);
    CHECK(a.pos.z == b.pos.z);
    CHECK(a.alpha == b.alpha);
}
