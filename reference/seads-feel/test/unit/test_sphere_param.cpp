// sphere_param (docs/world_build_plan.md §2): the pure cubesphere geometry +
// persistent height field extracted from render/planet.cpp so the gate can pin
// it headlessly (planet.cpp is raylib/app-only) and so mesh + future props +
// future airstrip contact read ONE source (the H1 anti-fork). Objective
// acceptance: ART-1 (cube<->sphere round-trip), ART-3 (tangent-warp density),
// the sampler wrap/clamp, and the single-source pin. Each leg is shaped to
// catch a break (Fable pre-impl audit §Q3): a face r/u swap, a dropped
// major-axis select, warp inside face_dir, warp->identity, a broken u-wrap, a
// fill_face height fork.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <cstdint>
#include <glm/geometric.hpp>
#include <vector>

#include "render/sphere_param.h"

#ifdef NDEBUG
#error "SEADS gate requires an assert-live build (SPEC 6.1)"
#endif

namespace {

using glm::dvec3;

// A small synthetic height field — no PNG, no raylib (the whole point of the
// extraction: testable headlessly). Value varies in x AND y so relief is real.
render::HeightField synth_field(int w, int h, double R, double relief,
                                double u_offset) {
    render::HeightField hf;
    hf.w = w;
    hf.h = h;
    hf.R = R;
    hf.relief_scale = relief;
    hf.u_offset = u_offset;
    hf.px.resize(static_cast<std::size_t>(w) * h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            hf.px[static_cast<std::size_t>(y) * w + x] =
                static_cast<std::uint8_t>((x * 32 + y * 16) & 0xFF);
    return hf;
}

// Solid-angle proxy of the warped cell (i,j) on face f of an N-grid: the area
// of the parallelogram its two edge directions span.
double cell_area(int f, int N, int i, int j) {
    auto node = [&](int a, int b) {
        return render::face_dir(f, render::warp(-1.0 + 2.0 * a / (N - 1)),
                                render::warp(-1.0 + 2.0 * b / (N - 1)));
    };
    const dvec3 d00 = node(i, j), d10 = node(i + 1, j), d01 = node(i, j + 1);
    return glm::length(glm::cross(d10 - d00, d01 - d00));
}

}  // namespace

// ART-1: cube -> sphere -> cube round-trips exactly on every face interior, at
// a GENERIC direction (no world axis aligned — the S8 lesson), and at a -axis
// face; exact edges resolve to ONE face deterministically.
TEST_CASE("sphere_param ART-1: face_dir / dir_to_face round-trip") {
    // Interior points recover the generating face and coords (s != t catches an
    // r/u column swap; a warp slipped into face_dir would break the inverse).
    const double s = 0.3, t = -0.7;
    for (int f = 0; f < 6; ++f) {
        const dvec3 d = render::face_dir(f, s, t);
        const render::FaceCoord fc = render::dir_to_face(d);
        CHECK(fc.face == f);
        CHECK(fc.s == Catch::Approx(s).margin(1e-12));
        CHECK(fc.t == Catch::Approx(t).margin(1e-12));
        // Forward composition returns the direction.
        const dvec3 d2 = render::face_dir(fc.face, fc.s, fc.t);
        CHECK(glm::length(d2 - d) < 1e-12);
    }

    // Generic point normalize(1,1,1): a cube corner — deterministic, and its
    // face_dir reconstructs it.
    const dvec3 g = glm::normalize(dvec3(1, 1, 1));
    const render::FaceCoord gc = render::dir_to_face(g);
    CHECK(glm::length(render::face_dir(gc.face, gc.s, gc.t) - g) < 1e-12);

    // A -axis-dominant direction routes to that -axis face (a mishandled sign
    // in the major-axis select would misroute it).
    CHECK(render::dir_to_face(glm::normalize(dvec3(-1.0, 0.2, 0.1))).face == 1);

    // An exact +X/+Y edge is a tie -> the SAME face every call (determinism).
    const dvec3 edge = glm::normalize(dvec3(1, 1, 0));
    CHECK(render::dir_to_face(edge).face == render::dir_to_face(edge).face);
}

// ART-3: the tangent warp keeps corner:center quad area near-uniform. Naive
// normalize (warp -> identity) blows this to ~5.1; warped is ~1.4.
TEST_CASE("sphere_param ART-3: tangent-warp corner density is bounded") {
    const int N = 33;
    const int f = 4;  // +Z
    const double corner = cell_area(f, N, 0, 0);
    const double center = cell_area(f, N, N / 2, N / 2);
    const double ratio = corner > center ? corner / center : center / corner;
    INFO("corner:center area ratio = " << ratio);
    CHECK(ratio < 1.6);  // naive normalize would be ~5.1
}

TEST_CASE("sphere_param: height sampler wrap / clamp / zero field") {
    // Zero field: radius is exactly R everywhere (no relief).
    const render::HeightField zero = synth_field(8, 4, 1000.0, 200.0, 0.0);
    render::HeightField flat = zero;
    for (auto& p : flat.px) p = 0;
    CHECK(flat.radius_at(glm::normalize(dvec3(1, 0.3, -0.2))) ==
          Catch::Approx(1000.0));

    const render::HeightField hf = synth_field(8, 4, 1000.0, 200.0, 0.0);
    // A texel center returns that texel's value (x=1,y=0 -> px=32).
    CHECK(hf.sample01((1 + 0.5) / 8.0, (0 + 0.5) / 4.0) ==
          Catch::Approx(32.0 / 255.0).margin(1e-9));
    // u wraps by exactly one period (longitude): sample01(u) == sample01(u+1).
    CHECK(hf.sample01(0.375, 0.25) ==
          Catch::Approx(hf.sample01(1.375, 0.25)).margin(1e-12));
    // v clamps at the poles (no read past the edge; value stays in [0,1]).
    const double vlo = hf.sample01(0.375, -0.5);
    const double vhi = hf.sample01(0.375, 10.0);
    CHECK(vlo >= 0.0);
    CHECK(vlo <= 1.0);
    CHECK(vhi >= 0.0);
    CHECK(vhi <= 1.0);
}

// The H1 anti-fork pin (the load-bearing leg): every mesh vertex sits on the
// SAME height field the props/landing will read — position == dir * radius_at,
// recomputed independently from the public API (a fill_face height fork, a
// wrong warp, or a wrong face basis fails here).
TEST_CASE("sphere_param: fill_face vertices lie on the shared height surface") {
    const int N = 17;
    const int f = 2;  // +Y — generic, not axis-degenerate for the grid
    const render::HeightField hf = synth_field(8, 4, 1000.0, 200.0, 0.3);
    const render::FaceMesh fm = render::fill_face(hf, f, N);

    REQUIRE(fm.positions.size() == static_cast<std::size_t>(N) * N * 3);
    REQUIRE(fm.indices.size() ==
            static_cast<std::size_t>(2) * (N - 1) * (N - 1) * 3);
    for (unsigned short idx : fm.indices) REQUIRE(idx < N * N);

    double max_resid = 0.0;
    for (int j = 0; j < N; ++j) {
        for (int i = 0; i < N; ++i) {
            const int vi = j * N + i;
            const dvec3 pos(fm.positions[vi * 3 + 0], fm.positions[vi * 3 + 1],
                            fm.positions[vi * 3 + 2]);
            const dvec3 dir = glm::normalize(pos);
            // Single-source: the vertex radius IS the height field's radius.
            max_resid = std::fmax(
                max_resid, std::fabs(glm::length(pos) - hf.radius_at(dir)));

            // Independent reconstruction of THIS vertex from the public API
            // (warped grid -> face_dir -> radius_at) — pins the warp + basis.
            const dvec3 d =
                render::face_dir(f, render::warp(-1.0 + 2.0 * i / (N - 1)),
                                 render::warp(-1.0 + 2.0 * j / (N - 1)));
            const dvec3 expect = d * hf.radius_at(d);
            CHECK(glm::length(pos - expect) < 1e-2);

            // Normals are unit and outward.
            const dvec3 nrm(fm.normals[vi * 3 + 0], fm.normals[vi * 3 + 1],
                            fm.normals[vi * 3 + 2]);
            CHECK(glm::length(nrm) == Catch::Approx(1.0).margin(1e-4));
            CHECK(glm::dot(nrm, dir) > 0.0);
        }
    }
    INFO("max |len(vertex) - radius_at| = " << max_resid);
    CHECK(max_resid < 1e-2);
}
