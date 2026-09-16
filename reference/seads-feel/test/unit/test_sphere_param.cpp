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
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <glm/geometric.hpp>
#include <vector>

#include "render/sphere_param.h"
#include "world/linework.h"
#include "world/snowpack.h"

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
            // *257 promotes the 8-bit pattern into the 16-bit store WITHOUT
            // changing sample01's value (v*257/65535 == v/255), so every height
            // expectation below (v/255) stays exact under the 16-bit field.
            hf.px[static_cast<std::size_t>(y) * w + x] = static_cast<std::uint16_t>(
                ((x * 32 + y * 16) & 0xFF) * 257);
    return hf;
}

// Invert equirect_uv: a unit direction that maps back to (u,v) at the given
// u_offset (so a texel center can be hit from a known direction).
dvec3 dir_from_uv(double u, double v, double u_offset) {
    const double pi = 3.14159265358979323846;
    const double ang = (u - 0.5 - u_offset) * 2.0 * pi;
    const double y = std::sin((0.5 - v) * pi);
    const double rxz = std::sqrt(std::fmax(0.0, 1.0 - y * y));
    return glm::normalize(dvec3(std::cos(ang) * rxz, y, std::sin(ang) * rxz));
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

// Forward OpenGL cube-map major-axis selection (spec Table 8.19): a unit
// direction -> (face, s, t in [0,1]). INDEPENDENT of gl_cube_dir (which inverts
// it), so the round-trip pins gl_cube_dir against the spec, not against itself.
struct GlFaceST {
    int face;
    double s, t;
};
GlFaceST gl_select(dvec3 d) {
    const double ax = std::fabs(d.x), ay = std::fabs(d.y), az = std::fabs(d.z);
    int face;
    double sc, tc, ma;
    if (ax >= ay && ax >= az) {
        ma = ax;
        if (d.x > 0) {
            face = 0;
            sc = -d.z;
            tc = -d.y;
        }  // +X
        else {
            face = 1;
            sc = d.z;
            tc = -d.y;
        }  // -X
    } else if (ay >= az) {
        ma = ay;
        if (d.y > 0) {
            face = 2;
            sc = d.x;
            tc = d.z;
        }  // +Y
        else {
            face = 3;
            sc = d.x;
            tc = -d.z;
        }  // -Y
    } else {
        ma = az;
        if (d.z > 0) {
            face = 4;
            sc = d.x;
            tc = -d.y;
        }  // +Z
        else {
            face = 5;
            sc = -d.x;
            tc = -d.y;
        }  // -Z
    }
    return {face, (sc / ma + 1.0) * 0.5, (tc / ma + 1.0) * 0.5};
}

}  // namespace

// The GL cubemap texel param inverts GL's OWN major-axis selection: a direction
// selected to (face, s, t) by the spec must reconstruct via gl_cube_dir. This
// is what makes texture(cube, fragDir) return the color baked at fragDir. A
// wrong face axis / sign / an accidental face_basis reuse (flipped/rotated
// faces) fails here on some direction.
TEST_CASE("sphere_param: gl_cube_dir inverts the GL major-axis selection") {
    const dvec3 dirs[] = {
        // +X-dominant with |sc| != |tc|: the ONLY other face-0 dir below is
        // normalize(1,1,1), degenerate (sc==tc), so a case-0 sc/tc TRANSPOSE
        // round-trips vacuously there. This asymmetric dir pins face 0 against
        // that transpose (red-team P1-1; mutation-verified).
        glm::normalize(dvec3(0.9, 0.2, -0.4)),
        glm::normalize(dvec3(0.3, -0.7, 0.2)),
        glm::normalize(dvec3(-0.6, 0.1, 0.4)),
        glm::normalize(dvec3(0.2, 0.9, -0.3)),
        glm::normalize(dvec3(0.1, -0.8, -0.5)),
        glm::normalize(dvec3(0.25, -0.15, 0.9)),
        glm::normalize(dvec3(-0.2, 0.3, -0.85)),
        glm::normalize(dvec3(1, 1, 1)),
    };
    for (const dvec3& d : dirs) {
        const GlFaceST fs = gl_select(d);
        const dvec3 r =
            render::gl_cube_dir(fs.face, 2.0 * fs.s - 1.0, 2.0 * fs.t - 1.0);
        INFO("face " << fs.face << " s " << fs.s << " t " << fs.t);
        CHECK(glm::length(r - d) < 1e-12);
    }
}

// The bake wiring: a synthetic equirect whose RGB ENCODES its (u,v) lets a
// baked texel be decoded back and checked against equirect_uv(gl_cube_dir(...))
// — this pins the bilinear, the RGB stride, the face order/offset, and that the
// bake composes gl_cube_dir with equirect_uv (an independent recompute of the
// target).
TEST_CASE("sphere_param: bake_equirect_cubemap resamples by 3D direction") {
    const int w = 64, h = 32;
    const double u_offset = 0.3;
    // R = u*255, G = v*255 (B = 0): the pixel value IS its own (u,v) address.
    std::vector<std::uint8_t> src(static_cast<std::size_t>(w) * h * 3);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const double u = (x + 0.5) / w, v = (y + 0.5) / h;
            const std::size_t o = (static_cast<std::size_t>(y) * w + x) * 3;
            src[o + 0] = static_cast<std::uint8_t>(u * 255.0 + 0.5);
            src[o + 1] = static_cast<std::uint8_t>(v * 255.0 + 0.5);
            src[o + 2] = 0;
        }
    }

    const int fs = 16;
    const std::vector<std::uint8_t> cube =
        render::bake_equirect_cubemap(src.data(), w, h, u_offset, fs);
    REQUIRE(cube.size() == static_cast<std::size_t>(6) * fs * fs * 3);

    // Every face + a few interior texels: decode the baked RGB back to (u,v)
    // and confirm it is the equirect address of that texel's 3D direction. Skip
    // texels whose direction lands within one source texel of the u-wrap seam
    // (the encoded ramp is discontinuous there, so bilinear decode is ambiguous
    // — not a bake bug).
    double max_err = 0.0;
    for (int f = 0; f < 6; ++f) {
        for (int py = 1; py < fs; py += 5) {
            for (int px = 1; px < fs; px += 5) {
                const double s = (px + 0.5) / fs, t = (py + 0.5) / fs;
                const dvec3 d =
                    render::gl_cube_dir(f, 2.0 * s - 1.0, 2.0 * t - 1.0);
                const glm::dvec2 uv = render::equirect_uv(d, u_offset);
                if (uv.x < 1.5 / w || uv.x > 1.0 - 1.5 / w) continue;
                const std::size_t o =
                    ((static_cast<std::size_t>(f) * fs + py) * fs + px) * 3;
                const double du = cube[o + 0] / 255.0 - uv.x;
                const double dv = cube[o + 1] / 255.0 - uv.y;
                max_err =
                    std::fmax(max_err, std::fmax(std::fabs(du), std::fabs(dv)));
            }
        }
    }
    INFO("max decoded (u,v) error = " << max_err);
    // Bilinear on an 8-bit ramp: quantization ~1/255 plus half-texel bilinear
    // slope. A face-axis swap or dropped u_offset blows this to O(0.1+).
    CHECK(max_err < 0.02);
}
// The RGBA bake (Inc 2 water landmask): its RGB channels must be IDENTICAL to
// the RGB bake (same albedo, same sampling), and its ALPHA must track the mask.
// Encode the mask with the SAME v-ramp as the green channel, so alpha == G is a
// first-principles cross-check (a swapped/dropped mask channel, a wrong stride,
// or an RGB/RGBA sampling divergence fails here). Mutation-catch: alpha reading
// rgb instead of mask, or the 4-stride write landing on the wrong byte.
TEST_CASE("sphere_param: bake_equirect_cubemap_rgba matches RGB and tracks mask") {
    const int w = 64, h = 32;
    const double u_offset = 0.3;
    const int fs = 16;
    std::vector<std::uint8_t> rgb(static_cast<std::size_t>(w) * h * 3);
    std::vector<std::uint8_t> mask(static_cast<std::size_t>(w) * h);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const double u = (x + 0.5) / w, v = (y + 0.5) / h;
            const std::size_t o = (static_cast<std::size_t>(y) * w + x);
            rgb[o * 3 + 0] = static_cast<std::uint8_t>(u * 255.0 + 0.5);
            rgb[o * 3 + 1] = static_cast<std::uint8_t>(v * 255.0 + 0.5);  // G = v
            rgb[o * 3 + 2] = 0;
            mask[o] = static_cast<std::uint8_t>(v * 255.0 + 0.5);  // A = v (== G)
        }
    }
    const std::vector<std::uint8_t> c3 =
        render::bake_equirect_cubemap(rgb.data(), w, h, u_offset, fs);
    const std::vector<std::uint8_t> c4 = render::bake_equirect_cubemap_rgba(
        rgb.data(), mask.data(), w, h, u_offset, fs);
    REQUIRE(c4.size() == static_cast<std::size_t>(6) * fs * fs * 4);
    for (int f = 0; f < 6; ++f) {
        for (int py = 0; py < fs; ++py) {
            for (int px = 0; px < fs; ++px) {
                const std::size_t i =
                    (static_cast<std::size_t>(f) * fs + py) * fs + px;
                // RGB identical to the RGB bake (same albedo, same convention).
                CHECK(c4[i * 4 + 0] == c3[i * 3 + 0]);
                CHECK(c4[i * 4 + 1] == c3[i * 3 + 1]);
                CHECK(c4[i * 4 + 2] == c3[i * 3 + 2]);
                // Alpha tracks the mask, which encodes the same v-ramp as G.
                CHECK(c4[i * 4 + 3] == c3[i * 3 + 1]);
            }
        }
    }
}

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

    // An exact +X/+Y edge is a tie -> the deterministic lowest-index face wins
    // (+X == face 0). Pins the strict '>' tie-break (a '>=' mutant picks +Y=2).
    const dvec3 edge = glm::normalize(dvec3(1, 1, 0));
    CHECK(render::dir_to_face(edge).face == 0);

    // warp(±1) == ±1 is what WELDS the six faces (tan(π/4)=1). A wrong warp
    // constant leaves inter-face coverage cracks that ART-3 and round-trip
    // miss.
    CHECK(render::warp(1.0) == Catch::Approx(1.0).margin(1e-12));
    CHECK(render::warp(-1.0) == Catch::Approx(-1.0).margin(1e-12));
}

// The pin the flat-planet mutant would slip: radius_at must apply BOTH the
// relief term and u_offset. No other leg exercises equirect_uv, and the
// zero-field / sample01 legs don't fire on `radius_at -> R` (a completely flat
// planet). Hit a KNOWN texel from a constructed direction at two offsets.
TEST_CASE("sphere_param: radius_at applies relief AND u_offset (not flat)") {
    const double R = 1000.0, relief = 200.0;
    // Texel (x=3,y=1) of the 8x4 synth field holds 3*32 + 1*16 = 112.
    const double uc = (3 + 0.5) / 8.0, vc = (1 + 0.5) / 4.0;
    const double expect = R + (112.0 / 255.0) * relief;
    for (double uoff : {0.0, 0.5}) {
        const render::HeightField hf = synth_field(8, 4, R, relief, uoff);
        const dvec3 d = dir_from_uv(uc, vc, uoff);
        // equirect_uv round-trips the direction to the texel center.
        const glm::dvec2 uv = render::equirect_uv(d, uoff);
        CHECK(uv.x == Catch::Approx(uc).margin(1e-9));
        CHECK(uv.y == Catch::Approx(vc).margin(1e-9));
        // radius_at applies relief (a flat `->R` mutant fails) AND u_offset (a
        // dropped-offset mutant hits the wrong texel at uoff=0.5 and fails).
        CHECK(hf.radius_at(d) == Catch::Approx(expect).margin(1e-6));
    }
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

// R1 bit-depth tripwire: the height field carries 16-bit precision (px is
// uint16, sample01 divides by 65535), so two fields differing by ONE 16-bit LSB
// at a texel resolve to sampled heights 1/65535 apart — strictly FINER than the
// old 8-bit field's 1/255 floor. A uint8 px (value truncation) OR a divisor that
// regresses to 255 (step == 1/255) both fail this. This is the precision the
// ground-roll query (R4) needs; the golden mesh can't substitute for it.
TEST_CASE("sphere_param R1: 16-bit height field resolves sub-1/255 precision") {
    auto field = [](std::uint16_t v) {
        render::HeightField hf;
        hf.w = 8;
        hf.h = 4;
        hf.R = 1000.0;
        hf.relief_scale = 200.0;
        hf.u_offset = 0.0;
        hf.px.assign(static_cast<std::size_t>(hf.w) * hf.h, v);
        return hf;
    };
    const double uc = (3 + 0.5) / 8.0, vc = (1 + 0.5) / 4.0;
    const double diff = field(1001).sample01(uc, vc) - field(1000).sample01(uc, vc);
    INFO("one 16-bit-LSB sample01 delta = " << diff);
    CHECK(diff == Catch::Approx(1.0 / 65535.0).epsilon(1e-9));
    CHECK(diff < 1.0 / 255.0);  // strictly finer than any 8-bit field can express
}

// R4c dem16 convention tripwires — the pack/unpack seam between the offline
// bake (PNG R/G = hi/lo) and the loader (world::dem16_unpack). Three legs:
// (1) round-trip: every (hi,lo) byte pair reconstructs its uint16 exactly, so
//     the packed PNG carries FULL 16-bit information (a loader that reads only
//     R, or swaps channels, fails on asymmetric pairs);
// (2) legacy equivalence: unpack(r, r) == r*257 for ALL r — the 8-bit
//     grayscale path (LoadImageColors expands gray to r == g) decodes through
//     the SAME function bit-identically to the R1 promote (goldens/mesh
//     unmoved under the loader change with the old asset);
// (3) oddness: values a *257 promote can NEVER produce (hi != lo) are
//     reachable — the sub-metre heights the 16-bit bake exists to carry.
TEST_CASE("heightfield R4c: dem16_unpack round-trip + legacy *257 identity") {
    for (unsigned hi = 0; hi < 256; ++hi) {
        const auto h8 = static_cast<std::uint8_t>(hi);
        // (2) legacy grayscale: unpack(r, r) == r*257 (exact, all 256 values).
        REQUIRE(world::dem16_unpack(h8, h8) == h8 * 257);
        // (1) round-trip at asymmetric lo bytes (full sweep is 65536 cases;
        //     the diagonal + two generic lo probes kill the real mutants:
        //     channel swap, lo dropped, shift-by-7/9).
        for (unsigned lo : {0u, 91u, 255u}) {
            const auto l8 = static_cast<std::uint8_t>(lo);
            REQUIRE(world::dem16_unpack(h8, l8) == ((hi << 8) | lo));
        }
    }
    // (3) a genuinely-16-bit value (unreachable by any r*257): one LSB above a
    //     promote. With relief 350 m this is the ~5 mm step ground roll reads.
    CHECK(world::dem16_unpack(100, 101) == 100 * 257 + 1);
    CHECK(world::dem16_unpack(100, 101) % 257 != 0);
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

    // Winding: the first triangle is CCW seen from OUTSIDE, so its geometric
    // normal points outward. A v10/v01 transcription flip would backface-cull
    // the whole planet with the gate green (no render test would see it).
    {
        auto vtx = [&](unsigned short k) {
            return dvec3(fm.positions[k * 3 + 0], fm.positions[k * 3 + 1],
                         fm.positions[k * 3 + 2]);
        };
        const dvec3 p0 = vtx(fm.indices[0]), p1 = vtx(fm.indices[1]),
                    p2 = vtx(fm.indices[2]);
        CHECK(glm::dot(glm::cross(p1 - p0, p2 - p0), glm::normalize(p0)) > 0.0);
    }

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

// ==========================================================================
// R4d face TILING + facet_radius_at — the road-on-terrain conformance seam.
// ==========================================================================

// Tiles are watertight BY CONSTRUCTION: a shared tile-edge vertex has the same
// GLOBAL grid index in both tiles, so position AND normal come out of the
// identical double expression — bit-identical floats, no seam crack, no
// shading split. A tile-local param (offset re-derivation) or a per-tile eps
// breaks this bit-exactly, which is what the == pins catch.
TEST_CASE("sphere_param R4d: tile-edge vertices are bit-identical") {
    const int N = 17, T = 2, f = 1;  // generic face, small grid
    const render::HeightField hf = synth_field(16, 8, 1000.0, 200.0, 0.3);
    const render::FaceMesh t00 = render::fill_face(hf, f, N, 0, 0, T);
    const render::FaceMesh t10 = render::fill_face(hf, f, N, 1, 0, T);
    const render::FaceMesh t01 = render::fill_face(hf, f, N, 0, 1, T);
    // Right edge of (0,0) == left edge of (1,0): column i=N-1 vs i=0, all j.
    for (int j = 0; j < N; ++j) {
        const int a = j * N + (N - 1), b = j * N + 0;
        for (int c = 0; c < 3; ++c) {
            REQUIRE(t00.positions[a * 3 + c] == t10.positions[b * 3 + c]);
            REQUIRE(t00.normals[a * 3 + c] == t10.normals[b * 3 + c]);
        }
    }
    // Top edge of (0,0) == bottom edge of (0,1): row j=N-1 vs j=0, all i.
    for (int i = 0; i < N; ++i) {
        const int a = (N - 1) * N + i, b = 0 * N + i;
        for (int c = 0; c < 3; ++c) {
            REQUIRE(t00.positions[a * 3 + c] == t01.positions[b * 3 + c]);
            REQUIRE(t00.normals[a * 3 + c] == t01.normals[b * 3 + c]);
        }
    }
    // Legacy identity: fill_face(hf,f,N) == the (0,0,1) tile, bit-exact.
    const render::FaceMesh legacy = render::fill_face(hf, f, N);
    const render::FaceMesh one = render::fill_face(hf, f, N, 0, 0, 1);
    REQUIRE(legacy.positions == one.positions);
    REQUIRE(legacy.normals == one.normals);
    REQUIRE(legacy.indices == one.indices);
}

// The eps pin (adversarial review P2-1): a T-tiled face at N verts/tile must be
// an EXACT sub-block of the single mesh with the same effective grid —
// fill_face(N=9, T=2) tiles == the matching vertices of fill_face(N=17, T=1)
// (ecells = 16 both ways: same global params, same eps = 2/16 into the normal
// probe). This is the test the tile-edge leg above can NOT provide: a mutant
// that reverts eps to the tile-LOCAL 2/(N-1) computes the identical wrong eps
// in every tile (edges still match tile-to-tile) but its normals diverge from
// the equivalent finer single mesh — this pin kills it bit-exactly, plus any
// tile-origin offset slip.
TEST_CASE("sphere_param R4d: tiles are exact sub-blocks of the finer mesh") {
    const int Nt = 9, T = 2, Nf = 17, f = 3;  // ecells = 16 for both builds
    const render::HeightField hf = synth_field(16, 8, 1000.0, 200.0, 0.3);
    const render::FaceMesh fine = render::fill_face(hf, f, Nf, 0, 0, 1);
    for (int ty = 0; ty < T; ++ty)
        for (int tx = 0; tx < T; ++tx) {
            const render::FaceMesh tile = render::fill_face(hf, f, Nt, tx, ty, T);
            for (int j = 0; j < Nt; ++j)
                for (int i = 0; i < Nt; ++i) {
                    const int vt = j * Nt + i;
                    const int gi = tx * (Nt - 1) + i, gj = ty * (Nt - 1) + j;
                    const int vf = gj * Nf + gi;
                    for (int c = 0; c < 3; ++c) {
                        REQUIRE(tile.positions[vt * 3 + c] ==
                                fine.positions[vf * 3 + c]);
                        REQUIRE(tile.normals[vt * 3 + c] ==
                                fine.normals[vf * 3 + c]);
                    }
                }
        }
}

// An INTERIOR tile's vertices still sit on the shared height surface (the H1
// anti-fork leg, re-run through the tiled path at a generic off-origin tile —
// a tile-origin offset bug shifts every vertex off radius_at).
TEST_CASE("sphere_param R4d: tiled fill_face vertices lie on radius_at") {
    const int N = 9, T = 3, f = 4;
    const render::HeightField hf = synth_field(16, 8, 1000.0, 200.0, 0.3);
    const render::FaceMesh fm = render::fill_face(hf, f, N, 2, 1, T);
    REQUIRE(fm.positions.size() == static_cast<std::size_t>(N) * N * 3);
    for (int k = 0; k < N * N; ++k) {
        const dvec3 pos(fm.positions[k * 3 + 0], fm.positions[k * 3 + 1],
                        fm.positions[k * 3 + 2]);
        const dvec3 d = glm::normalize(pos);
        // float verts vs double radius_at: ~R*2^-23 rounding, use 1e-3 abs.
        REQUIRE(glm::length(pos) ==
                Catch::Approx(hf.radius_at(d)).margin(1.0e-3));
    }
}

// facet_radius_at == the rendered surface. Reference is INDEPENDENT of the
// cell-lookup logic: intersect the query ray against EVERY triangle of the
// actual FaceMesh (barycentric containment), brute force. A wrong cell, a
// wrong diagonal split, an unwarp slip, or a tile-index bug lands in the wrong
// triangle and misses by metres; the pin is millimetres (float mesh verts vs
// double corners).
TEST_CASE("sphere_param R4d: facet_radius_at matches the actual mesh facets") {
    const int N = 9, T = 2;
    const render::HeightField hf = synth_field(16, 8, 1000.0, 200.0, 0.3);
    // Brute-force reference over one face's T*T tile meshes.
    auto facet_brute = [&](int f, const dvec3& d) -> double {
        double best = -1.0;
        for (int ty = 0; ty < T; ++ty)
            for (int tx = 0; tx < T; ++tx) {
                const render::FaceMesh fm = render::fill_face(hf, f, N, tx, ty, T);
                for (std::size_t k = 0; k + 2 < fm.indices.size(); k += 3) {
                    auto vtx = [&](std::size_t kk) {
                        const unsigned short vi = fm.indices[kk];
                        return dvec3(fm.positions[vi * 3 + 0],
                                     fm.positions[vi * 3 + 1],
                                     fm.positions[vi * 3 + 2]);
                    };
                    const dvec3 a = vtx(k), b = vtx(k + 1), c = vtx(k + 2);
                    const dvec3 n = glm::cross(b - a, c - a);
                    const double denom = glm::dot(n, d);
                    if (std::abs(denom) < 1e-12) continue;
                    const double r = glm::dot(n, a) / denom;
                    if (r <= 0.0) continue;
                    const dvec3 p = d * r;
                    // barycentric containment (small slack: float verts)
                    const dvec3 v0 = b - a, v1 = c - a, v2 = p - a;
                    const double d00 = glm::dot(v0, v0), d01 = glm::dot(v0, v1);
                    const double d11 = glm::dot(v1, v1), d20 = glm::dot(v2, v0);
                    const double d21 = glm::dot(v2, v1);
                    const double den = d00 * d11 - d01 * d01;
                    if (std::abs(den) < 1e-18) continue;
                    const double u = (d11 * d20 - d01 * d21) / den;
                    const double v = (d00 * d21 - d01 * d20) / den;
                    if (u < -1e-9 || v < -1e-9 || u + v > 1.0 + 1e-9) continue;
                    best = r;
                }
            }
        return best;
    };
    // Probes on ALL SIX faces (adversarial review P3-4: a single-face probe
    // set is structurally blind to a per-face mirror/axis slip): strictly
    // inside cells, both sides of cell diagonals, on a cell edge, near the
    // face corner. NOTE the split test runs in unwarped param space while the
    // rendered diagonal is the gnomonic chord — an O(cell²) sliver near cell
    // diagonals may take the adjacent (coplanar-at-the-edge) plane; that
    // discrepancy is cm-scale at real subdivs, absorbed by the drape lift.
    const double ecells = static_cast<double>(T) * (N - 1);
    const double cases[][2] = {
        {3.25, 5.75},   // upper triangle (fy > fx)
        {3.75, 5.25},   // lower triangle (fx > fy)
        {9.31, 2.62},   // crosses into the second tile in x
        {12.5, 12.5},   // exactly on a cell diagonal (shared edge, continuous)
        {8.0, 4.5},     // exactly on a vertical cell edge (continuous)
        {0.01, 0.01},   // corner cell, near the face corner
        {15.6, 9.9},    // generic
    };
    for (int f = 0; f < 6; ++f) {
        const render::FaceBasis bas = render::face_basis(f);
        auto probe_f = [&](double gx, double gy) {
            const double s = render::warp(-1.0 + 2.0 * gx / ecells);
            const double t = render::warp(-1.0 + 2.0 * gy / ecells);
            return glm::normalize(bas.n + bas.r * s + bas.u * t);
        };
        for (const auto& cs : cases) {
            const dvec3 d = probe_f(cs[0], cs[1]);
            const double ref = facet_brute(f, d);
            REQUIRE(ref > 0.0);  // the reference actually hit a facet
            const double got = render::facet_radius_at(hf, d, N, T);
            INFO("face " << f << " probe gx=" << cs[0] << " gy=" << cs[1]);
            CHECK(got == Catch::Approx(ref).margin(5.0e-3));
        }
    }
    const render::FaceBasis bas = render::face_basis(2);
    auto probe = [&](double gx, double gy) {
        const double s = render::warp(-1.0 + 2.0 * gx / ecells);
        const double t = render::warp(-1.0 + 2.0 * gy / ecells);
        return glm::normalize(bas.n + bas.r * s + bas.u * t);
    };
    // Fixture-no-op guard: over a sweep, facet and field MUST diverge (the
    // synthetic relief is rough at cell scale). A facet_radius_at that just
    // returns radius_at(d) fails here.
    double max_dev = 0.0;
    for (double gx = 0.3; gx < ecells; gx += 0.77)
        for (double gy = 0.3; gy < ecells; gy += 0.77) {
            const dvec3 d = probe(gx, gy);
            max_dev = std::max(max_dev,
                               std::abs(render::facet_radius_at(hf, d, N, T) -
                                        hf.radius_at(d)));
        }
    CHECK(max_dev > 0.05);  // metres of genuine facet-vs-field divergence
}

// ==========================================================================
// T25b — the SUBDIVIDING cut trim. Chad's round-17 Errington finding: a base
// terrain facet whose 3 corners all sit OUTSIDE a tunnel CutDisk but whose
// INTERIOR the disk chords across survived WHOLE (vertex-only any-corner drop),
// hovering as a flat slab over the pit (the "terrain cover sheet"). The T23
// kill shape: a disk straddling a facet interior touching no corner vertex.
// fill_face must trim it — no retained triangle may cover the disk centre — and
// split_depth = 0 (no subdivision) must FAIL that (the whole facet chords).
// ==========================================================================
TEST_CASE("sphere_param T25b: a cut disk interior to a facet is trimmed, "
          "not hovered whole") {
    const int N = 33, f = 2;  // +Y, generic; coarse grid = fat cells
    const render::HeightField hf = synth_field(16, 8, 1000.0, 200.0, 0.3);
    const double R = hf.R;

    // One interior cell (i,j) — well clear of the face edges. Its four corner
    // directions come through the IDENTICAL expression fill_face lays verts on.
    const int ci = 15, cj = 17;
    auto corner_dir = [&](int gi, int gj) {
        return render::face_dir(f, render::warp(-1.0 + 2.0 * gi / (N - 1)),
                                render::warp(-1.0 + 2.0 * gj / (N - 1)));
    };
    const dvec3 k00 = corner_dir(ci, cj), k10 = corner_dir(ci + 1, cj),
                k01 = corner_dir(ci, cj + 1), k11 = corner_dir(ci + 1, cj + 1);
    // Disk centred at the cell centre, radius a fraction of the centre->corner
    // arc so it fits INSIDE the cell touching no corner (all 3 corners of both
    // triangles stay outside — the T23 shape), yet spans many depth-4 leaves.
    const dvec3 centre = glm::normalize(k00 + k10 + k01 + k11);
    auto arc = [&](const dvec3& a, const dvec3& b) {
        return std::acos(glm::clamp(glm::dot(a, b), -1.0, 1.0)) * R;
    };
    const double near_corner = std::min(std::min(arc(centre, k00), arc(centre, k10)),
                                        std::min(arc(centre, k01), arc(centre, k11)));
    render::CutDisk disk;
    disk.dir = centre;
    disk.radius_m = 0.40 * near_corner;  // < corner arc => no corner inside
    const std::vector<render::CutDisk> cuts{disk};

    // Confirm the kill shape: NO cell corner lies inside the disk.
    REQUIRE_FALSE(render::dir_in_any_cut(k00, R, cuts));
    REQUIRE_FALSE(render::dir_in_any_cut(k10, R, cuts));
    REQUIRE_FALSE(render::dir_in_any_cut(k01, R, cuts));
    REQUIRE_FALSE(render::dir_in_any_cut(k11, R, cuts));

    // Does ANY retained triangle's cone (from the origin) contain direction q?
    // A retained facet that chords the disk covers the centre; a trimmed one
    // leaves the centre open (its inside leaves were dropped).
    auto covers = [&](const render::FaceMesh& fm, const dvec3& q) {
        for (std::size_t t = 0; t + 2 < fm.indices.size(); t += 3) {
            auto vd = [&](std::size_t kk) {
                const unsigned short vi = fm.indices[kk];
                return glm::normalize(dvec3(fm.positions[vi * 3 + 0],
                                            fm.positions[vi * 3 + 1],
                                            fm.positions[vi * 3 + 2]));
            };
            const dvec3 a = vd(t), b = vd(t + 1), c = vd(t + 2);
            const double s0 = glm::dot(glm::cross(a, b), q);
            const double s1 = glm::dot(glm::cross(b, c), q);
            const double s2 = glm::dot(glm::cross(c, a), q);
            const double e = 1e-12;
            if ((s0 >= -e && s1 >= -e && s2 >= -e) ||
                (s0 <= e && s1 <= e && s2 <= e))
                return true;
        }
        return false;
    };

    // A few probe directions THROUGH the disk-centre region (all deep inside
    // the open hole for the trimmed mesh).
    const dvec3 tangent = glm::normalize(k10 - k00);
    std::vector<dvec3> probes{centre};
    for (double frac : {0.2, 0.35}) {
        const double a = frac * disk.radius_m / R;  // arc offset, still inside
        probes.push_back(glm::normalize(centre + std::tan(a) * tangent));
        probes.push_back(glm::normalize(centre - std::tan(a) * tangent));
    }

    // DEFAULT depth (kPlanetCutSplitDepth): the interior is trimmed OPEN — no
    // retained triangle covers the centre region.
    const render::FaceMesh trimmed = render::fill_face(hf, f, N, 0, 0, 1, cuts);
    for (const dvec3& q : probes) {
        INFO("trimmed mesh must not cover a disk-centre probe");
        CHECK_FALSE(covers(trimmed, q));
    }

    // MUTATION LEVER: split_depth = 0 => the legacy whole-facet behavior. The
    // facet is retained whole and CHORDS the disk, so it covers the centre —
    // the kill test FAILS at depth 0, proving the subdivision is load-bearing.
    const render::FaceMesh legacy =
        render::fill_face(hf, f, N, 0, 0, 1, cuts, 0);
    CHECK(covers(legacy, centre));

    // Sanity: the trim removed triangles but left the mesh non-empty.
    CHECK(trimmed.indices.size() > 0);
    CHECK(trimmed.indices.size() != legacy.indices.size());
}

// ==========================================================================
// ROAD-REPAIR P1 (red-team 2026-09-09) — THE FOLD-CORNER MEMO.
//
// The sink floor (SnowpackField::apply_deck_floor) put a drawn-facet query on
// sample_at's per-substep path. A drawn-facet query evaluates THREE grid
// corners and each corner runs draw_fold_at -> a lines->nearest() plus an
// ambient evaluation, so an on-corridor ground sample was paying FOUR corridor
// lookups where W1.1/INV-1 budget ONE (measured: 4.00/sample, +42.9 us/sample
// in Debug — app/main.cpp SEADS_DECK_COST). The memo caches the per-corner
// value, keyed by provider + generation + hf + N + tiles + face + grid i/j.
//
// TWO PINS, and the second is the one that would catch a memo that "works" by
// returning a nearby answer:
//   (1) BIT-IDENTICAL. Memo ON must equal memo OFF with ==, not a tolerance.
//   (2) THE BUDGET. Re-querying inside one cell must stop paying lookups.
// The kill lever is set_facet_fold_memo(false): leg (2) fails with it off,
// which is what proves the memo, not the cache line, is doing the work.
// ==========================================================================
TEST_CASE("sphere_param ROAD-REPAIR: the fold-corner memo is free and exact") {
    const int N = 33, T = 1;
    const double R = 1000.0;
    const render::HeightField hf = synth_field(16, 8, R, 200.0, 0.0);

    // A corridor through the probe region: three stations along a great circle,
    // 6 m half-width — the shape a plowed road has, so the fold mask actually
    // varies across the cell instead of being a constant.
    const dvec3 c = glm::normalize(dvec3(0.3, 0.2, 1.0));
    dvec3 tang = glm::normalize(glm::cross(c, dvec3(0.0, 1.0, 0.0)));
    world::LineNetwork ln;
    ln.R = R;
    for (int k = -2; k <= 2; ++k) {
        world::LineStation st;
        st.dir = glm::normalize(c + tang * (k * 40.0 / R));
        st.half_w_m = 6.0f;
        st.run = 0;
        st.kind = static_cast<std::int16_t>(world::LineKind::RoadMinor);
        ln.st.push_back(st);
    }
    ln.build_index();
    REQUIRE_FALSE(ln.empty());

    world::SnowpackField fold;
    fold.hf = &hf;
    fold.lines = &ln;
    // Rebinding a provider must invalidate: this test runs after whatever the
    // rest of the file did, and `fold` is a fresh object at an arbitrary
    // address.
    render::invalidate_facet_fold_memo();

    // The sled's own access pattern: samples centimetres apart, all inside one
    // ~grid cell, walking along the corridor.
    std::vector<dvec3> dirs;
    for (int i = 0; i < 200; ++i) {
        const double along = i * 0.02;         // m
        const double lat = (i % 3 - 1) * 0.55;  // the three patches
        dirs.push_back(glm::normalize(c + tang * (along / R) +
                                      glm::normalize(glm::cross(tang, c)) *
                                          (lat / R)));
    }

    // (1) BIT-IDENTICAL, both ways round.
    render::set_facet_fold_memo(false);
    std::vector<double> cold;
    for (const dvec3& d : dirs)
        cold.push_back(render::drawn_radius_at(hf, d, N, T, &fold));
    render::set_facet_fold_memo(true);
    for (std::size_t i = 0; i < dirs.size(); ++i) {
        INFO("memo changed the drawn surface at sample " << i);
        CHECK(render::drawn_radius_at(hf, dirs[i], N, T, &fold) == cold[i]);
    }
    // The fold is LIVE on these dirs — otherwise leg (1) would pass on a memo
    // that never ran (fold == terrain everywhere is not a test of anything).
    bool folded = false;
    for (std::size_t i = 0; i < dirs.size(); ++i)
        if (std::abs(cold[i] - render::facet_radius_at(hf, dirs[i], N, T)) >
            1e-9)
            folded = true;
    CHECK(folded);

    // (2) THE BUDGET. One query warms the corners; the next 199 must add
    // essentially nothing, while the same run with the memo off pays three
    // lookups per query.
    render::set_facet_fold_memo(true);
    render::drawn_radius_at(hf, dirs[0], N, T, &fold);  // warm
    world::reset_line_nearest_calls();
    for (const dvec3& d : dirs) render::drawn_radius_at(hf, d, N, T, &fold);
    const long long memo_on = world::line_nearest_calls();

    render::set_facet_fold_memo(false);
    world::reset_line_nearest_calls();
    for (const dvec3& d : dirs) render::drawn_radius_at(hf, d, N, T, &fold);
    const long long memo_off = world::line_nearest_calls();

    INFO("lookups: memo ON " << memo_on << ", memo OFF " << memo_off);
    CHECK(memo_off >= 3 * static_cast<long long>(dirs.size()));
    CHECK(memo_on * 10 < memo_off);

    render::set_facet_fold_memo(true);  // leave the tree in its shipped state
}
