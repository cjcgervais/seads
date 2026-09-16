// T2 — the Errington-tunnel greybox generator + portal surgery
// (docs/tunnel_staging.md rung T2; render/tunnel_mesh.*, render/sphere_param.*
// fill_face cut disks). PURE targets only (seads_render_core) — no raylib, no
// GL. The through-line invariant: the visible greybox wall == the T1 query
// surface (net.signed_distance), single-source, so T3 collision and the eye
// never disagree.
//
// Discipline (CLAUDE.md ## Learned): the no-cut arm is BIT-IDENTICAL to the
// pre-change fill (the structural-off pin); a cut fully outside the face is an
// exact no-op; the single-source pin walks EVERY greybox vertex against the
// net.

#include <array>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <cstdio>
#include <glm/glm.hpp>
#include <map>
#include <set>
#include <utility>

#include "render/sphere_param.h"
#include "render/tunnel_mesh.h"
#include "world/heightfield.h"
#include "world/tunnel_geo.h"
#include "world/tunnel_net.h"

// T5d KILL TEST: the real Sudbury DEM is decoded HEADLESSLY via stb_image (the
// header-only PNG decoder raylib vendors — seads_tests links NO raylib, and
// STB_IMAGE_IMPLEMENTATION here can't collide with raylib's copy; same pattern
// as test_asset_validator's CGLTF_IMPLEMENTATION). No other TU pulls stb.
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include <string>

#include "stb_image.h"

#ifdef NDEBUG
#error "SEADS gate requires an assert-live build (SPEC 6.1)"
#endif

namespace {

constexpr double kR = 15000.0;

// The T1 canon dials (the TEST'S OWN params — same set as test_tunnel.cpp).
world::TunnelParams test_tp() {
    world::TunnelParams tp;
    tp.sphere_R = kR;
    tp.tube_width_m = 110.0;  // T6b elliptical bore (horizontal semi)
    tp.tube_height_m = 90.0;  // T6b elliptical bore (vertical semi)
    tp.depth_m = 2000.0;
    tp.soft_m = 40.0;
    tp.ramp_frac = 0.3;
    tp.spacing_m = 150.0;
    tp.floor_height_m =
        0.0;  // floor OFF in the shared fixture (T5a tests set
              // it explicitly; keeps existing legs bit-identical)
    // T11/T12 — THE SEALED-CORE ARENA (the buried egg is GONE): one shallow
    // ARENA ellipsoid, floor sealed over the buried core (T12 removed T11's
    // core-window shaft), breached by the two short bores.
    tp.arena_a_m = 7350.0;
    tp.arena_c_m = 2600.0;
    tp.arena_depth_m = 1500.0;
    tp.cavern_core_m = 2500.0;
    tp.breach_margin_m = 300.0;
    tp.chamber_long_m = 200.0;
    tp.chamber_lat_m = 140.0;
    tp.chamber_vert_m = 120.0;
    tp.chamber_breach_offset_m = 800.0;
    tp.connector_radius_m = 60.0;
    tp.chambers_on = true;  // T13: mesh legs pin the on-arm (chambers built)
    tp.bowl_radius_m = 450.0;
    tp.bowl_depth_m = 300.0;
    tp.mouth_sink_m = 130.0;  // T6c Errington entry pit (bore-center sink)
    tp.min_cover_m = 60.0;    // T6d monotone-descent cover
    tp.trench_len_m = 450.0;  // T13 entry approach trench (config/game.toml)
    tp.trench_rim_m = 150.0;
    return tp;
}

// Uniform terrain at elev_m (the test_ground.cpp fixture) — a flat surface so
// the cut geometry is analytic.
world::HeightField uniform_field(double elev_m, double relief = 4000.0) {
    world::HeightField hf;
    hf.w = 8;
    hf.h = 4;
    hf.R = kR;
    hf.relief_scale = relief;
    hf.u_offset = 0.0;
    const double f = std::min(std::max(elev_m / relief, 0.0), 1.0);
    hf.px.assign(static_cast<std::size_t>(hf.w) * hf.h,
                 static_cast<std::uint16_t>(f * 65535.0 + 0.5));
    return hf;
}

// A vertex direction is inside a cut disk when the great-circle arc * hf.R is
// under the disk radius (the test's independent copy of the predicate — NOT
// calling into the code under test).
bool dir_in_disk(const glm::dvec3& v, const render::CutDisk& c, double R) {
    const glm::dvec3 d = glm::normalize(v);
    const double dot = glm::clamp(glm::dot(d, c.dir), -1.0, 1.0);
    return std::acos(dot) * R < c.radius_m;
}

glm::dvec3 vpos(const render::FaceMesh& m, unsigned short i) {
    return glm::dvec3(m.positions[3 * i + 0], m.positions[3 * i + 1],
                      m.positions[3 * i + 2]);
}

}  // namespace

// ----------------------------------------------------------------------------
// 1. PORTAL SURGERY (render::fill_face cut disks).
// ----------------------------------------------------------------------------

TEST_CASE("T2 portal: no cuts is bit-identical to the pre-change fill") {
    const world::HeightField hf = uniform_field(300.0);
    const int N = 20;
    // Empty-cut overload vs the legacy no-cut path — the structural-off pin.
    const render::FaceMesh legacy = render::fill_face(hf, 2, N);  // +Y face
    const render::FaceMesh with_empty =
        render::fill_face(hf, 2, N, 0, 0, 1, {});

    REQUIRE(legacy.positions.size() == with_empty.positions.size());
    REQUIRE(legacy.normals.size() == with_empty.normals.size());
    REQUIRE(legacy.indices.size() == with_empty.indices.size());
    for (std::size_t i = 0; i < legacy.positions.size(); ++i)
        REQUIRE(legacy.positions[i] == with_empty.positions[i]);  // exact ==
    for (std::size_t i = 0; i < legacy.indices.size(); ++i)
        REQUIRE(legacy.indices[i] == with_empty.indices[i]);
}

TEST_CASE("T2 portal: a cut disk trims the interior (T25b subdividing "
          "over-cover)") {
    const world::HeightField hf = uniform_field(300.0);
    const int N = 40;
    // Cut disk in the middle of the +Y face (dir = +Y). Radius 1500 m => an
    // angular radius ~5.7° (1500/15000 rad), several cells wide on this face so
    // it robustly removes a patch (a 400 m disk is under one warped cell on a
    // subdiv-40 face and can slip between the center verts).
    const render::CutDisk cut{glm::normalize(glm::dvec3{0.0, 1.0, 0.0}),
                              1500.0};
    std::vector<render::CutDisk> cuts{cut};

    const render::FaceMesh full = render::fill_face(hf, 2, N);
    const render::FaceMesh cutm = render::fill_face(hf, 2, N, 0, 0, 1, cuts);

    // T25b changed the drop from a vertex-only WHOLE-triangle axe (which left a
    // facet chording the disk interior hovering whole — the "terrain cover
    // sheet") to a SUBDIVIDING trim: border facets split to kPlanetCutSplitDepth
    // and only fully-inside leaves drop, so a mixed leaf is KEPT (over-cover by
    // up to a leaf, never a void). The invariants change accordingly.

    // (a) The base grid verts are preserved; subdivided leaf verts are APPENDED
    //     (never removed), and normals track positions 1:1 (no cracked edge).
    REQUIRE(cutm.positions.size() >= full.positions.size());
    REQUIRE(cutm.normals.size() == cutm.positions.size());
    REQUIRE(cutm.indices.size() % 3 == 0);
    REQUIRE(cutm.indices.size() > 0);

    // The disk-centre direction, and a ray-in-cone test over surviving tris.
    const glm::dvec3 centre = cut.dir;
    auto covers = [&](const glm::dvec3& q) {
        for (std::size_t t = 0; t + 2 < cutm.indices.size(); t += 3) {
            const glm::dvec3 a = glm::normalize(vpos(cutm, cutm.indices[t]));
            const glm::dvec3 b = glm::normalize(vpos(cutm, cutm.indices[t + 1]));
            const glm::dvec3 c = glm::normalize(vpos(cutm, cutm.indices[t + 2]));
            const double s0 = glm::dot(glm::cross(a, b), q);
            const double s1 = glm::dot(glm::cross(b, c), q);
            const double s2 = glm::dot(glm::cross(c, a), q);
            if ((s0 >= 0 && s1 >= 0 && s2 >= 0) ||
                (s0 <= 0 && s1 <= 0 && s2 <= 0))
                return true;
        }
        return false;
    };
    // (b) The disk INTERIOR is opened: NO surviving triangle covers the centre
    //     (fully-inside leaves were dropped). This is the anti-hover invariant —
    //     the whole-facet axe (kPlanetCutSplitDepth = 0) leaves the chording
    //     facet spanning the centre and FAILS here.
    REQUIRE_FALSE(covers(centre));

    // (c) Over-cover is BOUNDED to the rim: no surviving vertex penetrates
    //     deeper than a leaf-margin inside the disk (survivors stay near/outside
    //     the rim, never reaching the core). A chording survivor would sit at
    //     the centre (arc 0), far inside this bound.
    const double kOverCoverMargin_m = 200.0;  // >> a depth-4 leaf (~37 m here)
    for (std::size_t t = 0; t < cutm.indices.size(); t += 3) {
        for (int k = 0; k < 3; ++k) {
            const glm::dvec3 d = glm::normalize(vpos(cutm, cutm.indices[t + k]));
            const double arc =
                std::acos(glm::clamp(glm::dot(d, centre), -1.0, 1.0)) * hf.R;
            REQUIRE(arc > cut.radius_m - kOverCoverMargin_m);
        }
    }
    // A vertex genuinely inside the disk exists (premise: the cut had something
    // to remove — the +Y face center is inside a +Y-axis disk).
    bool any_inside = false;
    for (std::size_t i = 0; i * 3 + 2 < full.positions.size(); ++i)
        if (dir_in_disk(vpos(full, static_cast<unsigned short>(i)), cut, hf.R))
            any_inside = true;
    REQUIRE(any_inside);
}

TEST_CASE("T2 portal: a disk fully outside the face removes nothing") {
    const world::HeightField hf = uniform_field(300.0);
    const int N = 30;
    // A -Y disk cannot touch the +Y face (they are antipodal caps).
    std::vector<render::CutDisk> cuts{
        {glm::normalize(glm::dvec3{0.0, -1.0, 0.0}), 400.0}};
    const render::FaceMesh full = render::fill_face(hf, 2, N);
    const render::FaceMesh cutm = render::fill_face(hf, 2, N, 0, 0, 1, cuts);
    REQUIRE(cutm.indices.size() == full.indices.size());  // exact no-op
    for (std::size_t i = 0; i < full.indices.size(); ++i)
        REQUIRE(cutm.indices[i] == full.indices[i]);
}

// ----------------------------------------------------------------------------
// 2. GREYBOX GENERATION (render::build_tunnel_mesh).
// ----------------------------------------------------------------------------

TEST_CASE("T2 greybox: single-source, every wall vert is on/inside the net") {
    const world::TunnelParams tp = test_tp();
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);
    const render::TunnelMeshData data = render::build_tunnel_mesh(net, &hf);

    REQUIRE(data.pieces.size() >= 6);  // tube + 2 chambers + 2 connectors + ...

    // T12 build order (floor OFF, arena ON): [0] tube, [1..2] chambers,
    // [3..4] connectors, [5..6] collars, [7] arena. Every NON-collar wall vert
    // must lie on/inside the query volume (T3 would kill the pilot before the
    // visible wall). The collars (mouth pit / bowl wall) are OUTSIDE the net by
    // design (open-cut daylight terrain), so skip them. The arena verts sit ON
    // (or inside) the net boundary, so all satisfy sd<=2. T12: the shaft + core
    // pieces are GONE (the floor is sealed). +2 m slack for the ellipsoid
    // near-field metric.
    for (const render::TunnelPiece& p : data.pieces) {
        if (render::piece_is_exterior(p.kind))
            continue;  // open-cut/exterior pieces live outside the net
        for (std::size_t vi = 0; vi < p.cue.size(); ++vi) {
            const glm::dvec3 v(p.positions[3 * vi + 0], p.positions[3 * vi + 1],
                               p.positions[3 * vi + 2]);
            REQUIRE(net.signed_distance(v) <= 2.0);
        }
    }
}

TEST_CASE("T2 greybox: mid-tube rings lie ON the wall (not collapsed inside)") {
    const world::TunnelParams tp = test_tp();
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);
    const render::TunnelMeshData data = render::build_tunnel_mesh(net, &hf);

    // Piece [0] is the tube: rings of kTubeSegments verts. Take a mid-tube ring
    // (a quarter of the way in — away from the egg, so the tube capsule owns
    // the distance) and REQUIRE at least half its verts have |sd| < 2 m (on the
    // surface, not buried).
    const render::TunnelPiece& tube = data.pieces[0];
    const int seg = render::kTubeSegments;
    const int nring = static_cast<int>(tube.cue.size()) / seg;
    REQUIRE(nring >= 4);
    const int ring = nring / 4;  // quarter down — pure tube section
    int on_wall = 0;
    for (int s = 0; s < seg; ++s) {
        const int vi = ring * seg + s;
        const glm::dvec3 v(tube.positions[3 * vi + 0],
                           tube.positions[3 * vi + 1],
                           tube.positions[3 * vi + 2]);
        if (std::abs(net.signed_distance(v)) < 2.0) ++on_wall;
    }
    REQUIRE(on_wall >= seg / 2);
}

TEST_CASE("T2 greybox: collar inner ring == the tube end ring (no crack)") {
    const world::TunnelParams tp = test_tp();
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);
    const render::TunnelMeshData data = render::build_tunnel_mesh(net, &hf);

    REQUIRE(data.pieces.size() ==
            9);  // T16: +1 entry-trench piece  // T12: + arena (floor OFF);
                 // shaft + core GONE (sealed)
    const render::TunnelPiece& tube = data.pieces[0];
    // Find the two collar pieces by kind (the cavern/core shift nothing before
    // them, but is_collar is index-robust). [0] = Errington pit wall (closes on
    // the tube FIRST ring), [1] = Murray bowl wall (closes on the tube LAST
    // ring) — build order.
    const render::TunnelPiece* collar0_p = nullptr;
    const render::TunnelPiece* bowl_p = nullptr;
    for (const render::TunnelPiece& p : data.pieces) {
        if (!p.is_collar) continue;
        if (collar0_p == nullptr)
            collar0_p = &p;
        else
            bowl_p = &p;
    }
    REQUIRE(collar0_p != nullptr);
    REQUIRE(bowl_p != nullptr);
    const render::TunnelPiece& collar0 = *collar0_p;  // home ENTRY-PIT wall
    const render::TunnelPiece& bowl = *bowl_p;        // Murray bowl wall
    const int seg = render::kTubeSegments;
    const int nv = static_cast<int>(tube.cue.size());
    const int nring = nv / seg;

    // T6c: BOTH mouths are OPEN-PIT walls closing onto the tube end ring with
    // the EXACT floats (no crack). T17: the seam ring is no longer the LAST
    // seg verts (the pit piece ends with the cut sleeve, the bowl with the
    // floor disk) — LOCATE it by exact-float match of the tube ring's first
    // vertex, then require the whole ring contiguous in tube order (the
    // adapter path emits the inner ring as one contiguous s=0..seg-1 run).
    const auto seam_start = [&](const render::TunnelPiece& piece,
                                int tube_ring_base) -> int {
        const int pn = static_cast<int>(piece.cue.size());
        for (int v = 0; v + seg <= pn; ++v) {
            bool all = true;
            for (int s = 0; s < seg && all; ++s)
                for (int c = 0; c < 3; ++c)
                    if (piece.positions[(v + s) * 3 + c] !=
                        tube.positions[(tube_ring_base + s) * 3 + c]) {
                        all = false;
                        break;
                    }
            if (all) return v;
        }
        return -1;
    };
    // The tube's MOUTH rings: ring 0 is still the piece's first seg verts,
    // but the LAST spine ring is no longer at the tail — the T17 trims append
    // subdivided border-leaf vertices after it (nv is not even ring-divisible
    // any more). Locate the Murray mouth ring in the TUBE by proximity to the
    // back spine node, then float-match it into the bowl piece.
    (void)nring;
    const int pit_seam = seam_start(collar0, 0);
    REQUIRE(pit_seam >= 0);  // Errington pit seam == tube FIRST ring, exact
    // Murray: SOME near-mouth tube ring appears contiguously, exact-float, in
    // the bowl piece (the shared seam). Try every candidate near-mouth run —
    // trim-appended leaf vertices make positional assumptions unreliable, but
    // the seam pin only needs ONE shared ring to exist.
    {
        const glm::dvec3 back = net.spine.back().pos;
        bool found = false;
        for (int v = 0; !found && v + seg <= nv; ++v) {
            bool near = true;
            for (int s = 0; s < seg && near; ++s) {
                const glm::dvec3 q(tube.positions[(v + s) * 3 + 0],
                                   tube.positions[(v + s) * 3 + 1],
                                   tube.positions[(v + s) * 3 + 2]);
                if (glm::length(q - back) > 1.5 * net.tube_width) near = false;
            }
            if (near && seam_start(bowl, v) >= 0) found = true;
        }
        REQUIRE(found);  // Murray bowl shares a tube mouth ring, exact floats
    }
}

TEST_CASE("T2 greybox: ring transport does not twist; indices + 16-bit sane") {
    const world::TunnelParams tp = test_tp();
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);
    const render::TunnelMeshData data = render::build_tunnel_mesh(net, &hf);

    // Every piece: indices in range, vertex count < 65535 (16-bit cap).
    for (const render::TunnelPiece& p : data.pieces) {
        const std::size_t nv = p.cue.size();
        REQUIRE(nv < 65535u);
        REQUIRE(p.positions.size() == nv * 3);
        for (unsigned short idx : p.indices) REQUIRE(idx < nv);
    }

    // Ring parallel-transport: consecutive tube-ring "centroid->vert[0]"
    // vectors (the frame's u axis proxy) keep a positive dot — no twist flip.
    // Build the per-ring centroid and the vector to its first vert.
    const render::TunnelPiece& tube = data.pieces[0];
    const int seg = render::kTubeSegments;
    const int nring = static_cast<int>(tube.cue.size()) / seg;
    auto ring_u = [&](int r) {
        glm::dvec3 c(0.0);
        for (int s = 0; s < seg; ++s) {
            const int vi = r * seg + s;
            c += glm::dvec3(tube.positions[3 * vi + 0],
                            tube.positions[3 * vi + 1],
                            tube.positions[3 * vi + 2]);
        }
        c /= seg;
        const glm::dvec3 v0(tube.positions[3 * (r * seg) + 0],
                            tube.positions[3 * (r * seg) + 1],
                            tube.positions[3 * (r * seg) + 2]);
        return glm::normalize(v0 - c);
    };
    // T10: the deep plateau rings are SUPPRESSED, so the descent-breach ring
    // and the ascent-breach ring become buffer-adjacent across the gap — their
    // transported frames differ. Restrict the twist pin to the smooth upper
    // descent (the first several emitted rings from the Errington mouth), where
    // parallel transport is continuous. Non-vacuous: nring is large.
    REQUIRE(nring >= 6);
    const int rmax = std::min(nring, 8);
    for (int r = 1; r < rmax; ++r) {
        REQUIRE(glm::dot(ring_u(r - 1), ring_u(r)) > 0.9);
    }
}

TEST_CASE("T2 greybox: depth cue brighter near a mouth, all cues in [0,1]") {
    const world::TunnelParams tp = test_tp();
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);
    const render::TunnelMeshData data = render::build_tunnel_mesh(net, &hf);

    // All cues in [0,1].
    for (const render::TunnelPiece& p : data.pieces)
        for (float c : p.cue) REQUIRE((c >= 0.0f && c <= 1.0f));

    // A mouth-adjacent vert (the tube's FIRST ring, at the home mouth surface)
    // is brighter than a deep vert. T10: the tube is a SHORT descent bore now,
    // and the last ~600 m before the breach glow with core light (the BREACH
    // SPILL, cue -> 1). So the deep-cue sample must come from the UPPER-MIDDLE
    // bore, NOT the breach-adjacent rings (whose spill would read ~1.0). Sample
    // the minimum cue over the middle third of the emitted rings (below the
    // mouth, above the breach-spill zone).
    const render::TunnelPiece& tube = data.pieces[0];
    const int seg = render::kTubeSegments;
    const int nring = static_cast<int>(tube.cue.size()) / seg;
    REQUIRE(nring >= 6);
    const float mouth_cue = tube.cue[0];  // first ring, at the mouth
    float deep_cue = 1.0f;
    // Middle third of the rings: past the mouth, before the breach spill.
    for (int r = nring / 3; r < 2 * nring / 3; ++r)
        for (int s = 0; s < seg; ++s)
            deep_cue = std::min(deep_cue, tube.cue[r * seg + s]);
    REQUIRE(mouth_cue > deep_cue);
}

TEST_CASE("T2 greybox: null ground still builds a sane greybox (collar on R)") {
    const world::TunnelParams tp = test_tp();
    const world::TunnelNet net = world::build_tunnel_net(tp, nullptr);
    const render::TunnelMeshData data = render::build_tunnel_mesh(net, nullptr);
    REQUIRE(data.pieces.size() ==
            9);  // T16: +1 entry-trench piece  // T12: + arena (floor OFF);
                 // shaft + core GONE (sealed)
    for (const render::TunnelPiece& p : data.pieces) {
        REQUIRE(!p.cue.empty());
        REQUIRE(p.indices.size() % 3 == 0);
    }
    // Single-source still holds with null ground (the non-collar body pieces).
    // T12: the shaft + core pieces are GONE (the floor is sealed).
    for (const render::TunnelPiece& p : data.pieces) {
        if (render::piece_is_exterior(p.kind))
            continue;  // open-cut/exterior pieces live outside the net
        for (std::size_t vi = 0; vi < p.cue.size(); ++vi) {
            const glm::dvec3 v(p.positions[3 * vi + 0], p.positions[3 * vi + 1],
                               p.positions[3 * vi + 2]);
            REQUIRE(net.signed_distance(v) <= 2.0);
        }
    }
}

// ----------------------------------------------------------------------------
// 3. FIX 2 — TWO-SIDED SDF PIN: pocket wall verts are on the surface (not
//    collapsed deep inside the volume).
// ----------------------------------------------------------------------------

TEST_CASE(
    "T2 greybox: pocket walls are on-surface, not collapsed inside "
    "(two-sided)") {
    const world::TunnelParams tp = test_tp();
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);
    const render::TunnelMeshData data = render::build_tunnel_mesh(net, &hf);

    REQUIRE(data.pieces.size() ==
            9);  // T16: +1 entry-trench piece  // T12: + arena (floor OFF);
                 // shaft + core GONE (sealed)

    // T10: the egg is GONE — pieces [1..2] are the two side chambers. For each
    // pocket:
    //   (a) EVERY vert: sd <= +2.0 (the existing one-sided bound — verts on or
    //       just inside the query surface).
    //   (b) At LEAST 70% of verts: |sd| < 2.0 (on the surface, not buried deep
    //       inside the union where the tube passes through the pocket). The
    //       ~30% slack accommodates verts legitimately inside the union volume
    //       (where the tube or connector SDF is tighter), but rules out a
    //       collapsed- semi-axes mutant where most verts are far inside.
    // Mutation target (m9): pocket ellipsoid semi-axes * 0.5 => most verts sit
    // deep inside the volume (sd << -2.0), violating the 70% |sd|<2 bound.
    //
    // NOTE: connectors [3..4] are SHORT tubes between the bore and the chamber;
    // their walls are inside the union SDF (the chamber body is larger and
    // nearer) so nearly all their verts have |sd| >> 2 — the 70% constraint
    // does not apply. Connectors are covered by the single-source sd <= 2.0
    // check above.
    for (std::size_t pi = 1; pi <= 2; ++pi) {
        const render::TunnelPiece& p = data.pieces[pi];
        const std::size_t nv = p.cue.size();
        REQUIRE(nv > 0);

        int on_surface = 0;
        for (std::size_t vi = 0; vi < nv; ++vi) {
            const glm::dvec3 v(p.positions[3 * vi + 0], p.positions[3 * vi + 1],
                               p.positions[3 * vi + 2]);
            const double sd = net.signed_distance(v);
            // (a) not outside:
            REQUIRE(sd <= 2.0);
            // count on-surface:
            if (std::abs(sd) < 2.0) ++on_surface;
        }
        // (b) at least 70% on-surface (not buried):
        REQUIRE(on_surface >= static_cast<int>(nv) * 70 / 100);
    }
}

// ----------------------------------------------------------------------------
// 3b. FIX 2 — MOUTH APERTURE + SKIRT-NEVER-INSIDE (T3 fold, Fix 2).
// ----------------------------------------------------------------------------

TEST_CASE("T6c mouth aperture: bore + pit open; far-lateral solid") {
    // T6c: the Errington mouth is a SUNK bore opening inside an ENTRY PIT. A
    // point 60 m HORIZONTALLY from the sunk bore axis is inside the elliptical
    // bore (horizontal semi 110 m); a point far outside the pit rim (400 m
    // lateral at the mouth-node depth) is solid earth (the pit rim ~ 220 m).
    // The horizontal axis = normalize(cross(tangent, up)) — the same frame the
    // bore ellipse uses.
    //
    // Mutation m14: flip the 60 m REQUIRE(contains) expectation -> FAILS.
    const world::TunnelParams tp = test_tp();
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);

    REQUIRE(net.spine.size() >= 2);

    // The HORIZONTAL bore axis at the mouth: cross(tangent, up) (⟂ up, ⟂ tan) —
    // the bore's wide direction, so a lateral offset is measured against the
    // 110 m horizontal semi.
    const glm::dvec3 seg_tangent =
        glm::normalize(net.spine[1].pos - net.spine[0].pos);
    const glm::dvec3 mouth_axis = glm::normalize(net.spine.front().pos);
    glm::dvec3 lat = glm::cross(seg_tangent, mouth_axis);
    REQUIRE(glm::length(lat) > 1e-6);
    lat = glm::normalize(lat);

    // Premise: lat is horizontal (⟂ up) and ⟂ the segment tangent.
    REQUIRE(std::abs(glm::dot(lat, mouth_axis)) < 1e-6);
    REQUIRE(std::abs(glm::dot(lat, seg_tangent)) < 1e-6);

    const glm::dvec3 mouth_pos = net.spine.front().pos;
    const glm::dvec3 pt60 =
        mouth_pos + lat * 60.0;  // inside the bore (semi 110)
    const glm::dvec3 pt400 =
        mouth_pos + lat * 400.0;  // outside the pit rim (~220)

    // 60 m horizontal: inside the elliptical bore opening.
    REQUIRE(net.contains(pt60));  // m14 target: flip -> FAILS

    // 400 m horizontal: outside the bore AND the entry pit — solid earth.
    REQUIRE(!net.contains(pt400));
}

TEST_CASE(
    "T3 fold Fix2b: skirt pieces tagged is_collar; inner ring on the tube "
    "wall") {
    // Guards the is_collar tag so its deletion is caught by mutation-verify.
    // The collar piece has two rings: outer (annular skirt) + inner (tube end
    // ring, shared floats). The inner ring sits at sd~0 on the tube wall. The
    // outer ring on the tunnel-entry side is geometrically inside the tube SDF
    // (the spine passes nearby), so we do NOT assert "all verts outside" —
    // instead we verify that:
    //   (a) collar_vert_count > 0: the tag is set on at least some pieces
    //       (PREMISE — mutation m13 kills this: is_collar never set => 0 verts
    //        iterated => REQUIRE fails loud).
    //   (b) exactly 2 pieces are tagged is_collar (the home skirt + Murray bowl
    //       wall).
    //   (c) each collar piece's vertex count is a whole number of rings of
    //       kCollarSegments (the home skirt = 2 rings; the Murray bowl wall =
    //       kBowlRings rings, T4a).
    //   (d) the LAST ring (the exact tube end ring — the shared seam) sits
    //       within 2 m of sd=0 (on the tube wall).
    const world::TunnelParams tp = test_tp();
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);
    const render::TunnelMeshData data = render::build_tunnel_mesh(net, &hf);

    REQUIRE(data.pieces.size() ==
            9);  // T16: +1 entry-trench piece  // T12: + arena (floor OFF);
                 // shaft + core GONE (sealed)

    int collar_vert_count = 0;
    int collar_piece_count = 0;
    const int seg = render::kCollarSegments;

    for (const render::TunnelPiece& p : data.pieces) {
        if (!p.is_collar) continue;
        ++collar_piece_count;
        const int nv = static_cast<int>(p.cue.size());
        // (c) the PRE-TRIM roster (trim_base_verts, T23) is a whole number of
        //     seg-rings, >= 2 rings — plus a small NAMED remainder: the Murray
        //     floor-disk centre (T17, +1) and the T22 throat ramp strip
        //     (rows*across, header-derived). The trims append subdivided
        //     border leaves after the mark, always in unshared TRIPLES.
        const int ramp_verts =
            render::kThroatRampRows * render::kThroatRampAcross;
        const int base = static_cast<int>(p.trim_base_verts);
        REQUIRE(base > 0);
        REQUIRE((nv - base) % 3 == 0);
        const int rem = base % seg;
        REQUIRE((rem == 0 || rem == 1 || rem == (1 + ramp_verts) % seg));
        REQUIRE(base / seg >= 2);
        // (d) The tube-end SEAM ring exists ON the tube wall: at least one
        //     full ring of verts sits within 2 m of sd = 0 (T17 moved the
        //     seam off the piece tail — sleeve/disk emit after it — so count
        //     on-wall verts instead of indexing the last ring; the EXACT
        //     float seam is pinned by the T2 no-crack leg).
        int on_wall = 0;
        for (int vi = 0; vi < nv; ++vi) {
            const glm::dvec3 v(p.positions[3 * vi + 0], p.positions[3 * vi + 1],
                               p.positions[3 * vi + 2]);
            if (std::abs(net.signed_distance(v)) < 2.0) ++on_wall;
        }
        REQUIRE(on_wall >= seg);
        collar_vert_count += nv;
    }
    // (a) Premise: some collar verts exist (m13 kill: tag never set => 0 =>
    // FAILS).
    REQUIRE(collar_vert_count > 0);
    // (b) Exactly two collar pieces (home skirt + Murray bowl wall).
    REQUIRE(collar_piece_count == 2);
}

TEST_CASE("T16 Murray open-pit: benched wall stays inside the net + funnel") {
    // T16 ENTRANCE-ART: the Murray raid mouth is a proper OPEN-PIT mine — the
    // smooth funnel is reshaped into stepped BENCHES (a face + a berm per
    // bench). This pins the render-mesh reshape's safety + read:
    //   (1) the wall emits 2*kBowlBenches + 1 rings (rim + face/berm per bench;
    //       last berm == the tube end ring);
    //   (2) NO wall vert outside the net (a death surface never reads as open
    //       air) — every vert sits inside/on the bowl_r collision cylinder;
    //   (3) the bench PROFILE is monotonic: radius non-increasing, depth
    //       non-decreasing, rim -> floor;
    //   (4) every bench ring stays AT-OR-OUTSIDE the smooth-funnel radius at
    //   its
    //       depth (benches carve OUTWARD into rock, NEVER inward into the open
    //       flight volume — collision, the cylinder, is never protruded).
    // Collision NOTE: the bowl SDF is a CYLINDER of radius bowl_r (depth-
    // independent, world/tunnel_net.cpp bowl_sd), NOT a cone — so every bench
    // vert (radius <= bowl_r) sits inside the collision cylinder; the render
    // reshape introduces no death surface. Item-3's "smooth funnel" is the
    // cosmetic reference the benches must not dip below.
    const world::TunnelParams tp = test_tp();
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);
    const render::TunnelMeshData data = render::build_tunnel_mesh(net, &hf);

    REQUIRE(data.pieces.size() ==
            9);  // T16: +1 entry-trench piece  // T12: + arena (floor OFF);
                 // shaft + core GONE (sealed)
    // The Murray bowl wall is the LAST is_collar piece (build order: Errington
    // pit wall, then Murray bowl wall). Index-robust vs the cavern/core shift.
    const render::TunnelPiece* bowl_p = nullptr;
    for (const render::TunnelPiece& p : data.pieces) {
        if (!p.is_collar) continue;
        bowl_p = &p;  // keep the LAST is_collar => Murray bowl wall
    }
    REQUIRE(bowl_p != nullptr);
    const render::TunnelPiece& bowl = *bowl_p;  // Murray bowl wall
    const int seg = render::kTubeSegments;
    // T17 ring roster: rim + a face/berm per bench (2*Nb, the rim ring is
    // idx 0 in the no-band path), + the mouth ADAPTER ring, + the exact tube
    // seam ring, + 4 floor-disk rings, + the disk centre vert.
    const int nrings = 2 * render::kBowlBenches /*rim + stairs*/ +
                       1 /*adapter*/ + 1 /*tube seam*/ + 4 /*floor disk*/ +
                       2 /*T26 sub-floor edge curb*/;

    // (1) exact ring count (+1 = the floor-disk centre vertex; + the T22
    //     throat ramp strip, rows*across, header-derived). T23: the trims now
    //     append subdivided border LEAVES after this roster — pin the roster
    //     via the trim_base_verts mark and the tail as unshared triples.
    REQUIRE(static_cast<int>(bowl.trim_base_verts) ==
            nrings * seg + 1 +
                render::kThroatRampRows * render::kThroatRampAcross);
    REQUIRE((bowl.cue.size() - bowl.trim_base_verts) % 3 == 0);

    // (2) Every wall vert sits within ONE BENCH TOOTH of the collision
    // boundary, both ways (T18: collision is the VISIBLE-CONE taper through
    // the staircase's corner lines, so the drawn rungs zigzag around sd = 0
    // by up to a tooth — the drawn wall may sit in rock, and open air may
    // reach a tooth past a face, but never deeper: the fly-14
    // "collided into nothing" class stays bounded by one rung).
    const double tooth =
        (net.bowl.bowl_r - net.bowl.floor_r) / render::kBowlBenches;
    // Referenced verts only: the T17/T18 trims drop FACETS (indices), so a
    // vert orphaned by the bore-slot cut legitimately sits in open air.
    std::vector<char> referenced(bowl.cue.size(), 0);
    for (unsigned short ix : bowl.indices) referenced[ix] = 1;
    // ROSTER verts: the wall-shape bound, unchanged in strength.
    for (std::size_t vi = 0; vi < bowl.trim_base_verts; ++vi) {
        if (!referenced[vi]) continue;
        const glm::dvec3 v(bowl.positions[3 * vi + 0],
                           bowl.positions[3 * vi + 1],
                           bowl.positions[3 * vi + 2]);
        const glm::dvec3& mmb = net.spine.back().pos;
        INFO("vert " << vi << " rel-floor "
                     << glm::dot(v - mmb, glm::normalize(mmb)) << " mouthdist "
                     << glm::length(v - mmb) << " tube "
                     << net.tube_signed_distance(v) << " throat "
                     << net.throat_signed_distance(v));
        REQUIRE(std::abs(net.signed_distance(v)) <= tooth + 3.0);
    }
    // T23 LEAF triples (appended after trim_base_verts): |sd| is NOT a
    // distance proxy here — tube_signed_distance is DISCONTINUOUS across the
    // bore's end-cap plane (measured: adjacent leaf verts 2.5 m apart read
    // +1.9 vs -87.4), so a leaf hugging that seam legitimately carries a
    // deep-negative vert while its surface sits centimetres from rock, and
    // no numeric edge/size cap can be derived from sd (a cap calibrated to
    // today's facet sizes is the AT-15 trap — the ramp rows alone dwarf the
    // ring chords). Pin the trim's load-bearing contract: a kept leaf is
    // NEVER fully inside the bore/throat beyond the trim eps — a
    // fully-swallowed kept leaf is a drawn wall across flyable air (the
    // fly-through class). The see-through and blocked-entrance duals are
    // owned by the T23 leak probe and the T17 entrance-open test.
    for (std::size_t vi = bowl.trim_base_verts; vi + 2 < bowl.cue.size();
         vi += 3) {
        double sd[3];
        for (int k = 0; k < 3; ++k) {
            const glm::dvec3 v(bowl.positions[3 * (vi + k) + 0],
                               bowl.positions[3 * (vi + k) + 1],
                               bowl.positions[3 * (vi + k) + 2]);
            sd[k] = std::min(net.tube_signed_distance(v),
                             net.throat_signed_distance(v));
        }
        // eps + 1 cm: the trim classified the DOUBLE leaf coords; this reads
        // back the float-rounded positions (ulp ~2 mm at |p|=15 km, the S1
        // lesson), so a boundary vert can drift across the exact eps by
        // millimetres.
        const double teps = render::kCutSwallowEps_m + 0.01;
        const int nin = (sd[0] < -teps ? 1 : 0) + (sd[1] < -teps ? 1 : 0) +
                        (sd[2] < -teps ? 1 : 0);
        INFO("leaf at vert " << vi << " sd " << sd[0] << " " << sd[1] << " "
                             << sd[2]);
        REQUIRE(nin < 3);  // never a fully-swallowed kept leaf
    }

    // Per-ring average (radius from the bowl axis, depth below the surface).
    const world::TunnelNet::Bowl& bw = net.bowl;
    auto ring_stat = [&](int ri, double& rad, double& depth) {
        rad = 0.0;
        depth = 0.0;
        for (int s = 0; s < seg; ++s) {
            const int vi = ri * seg + s;
            const glm::dvec3 v(bowl.positions[3 * vi + 0],
                               bowl.positions[3 * vi + 1],
                               bowl.positions[3 * vi + 2]);
            const double ax = glm::dot(v, bw.axis);
            const glm::dvec3 perp = v - ax * bw.axis;
            rad += glm::length(perp);
            depth += bw.surface_r - ax;
        }
        rad /= seg;
        depth /= seg;
    };

    // (3) + (4): monotonic profile + never inside the smooth funnel — over the
    //     BENCH rings (rim .. last face). The LAST ring is the tube-end BORE
    //     ELLIPSE seam (avg perpendicular radius ~ 0.64*tube_width, dipping
    //     inside the cone's floor circle by construction, as the smooth cone's
    //     inner ring always did) — it is the opening handoff, not a bench, so
    //     it is excluded from the wall-profile checks.
    //     In the test path (no terrain grid) the wall rim sits at bw.bowl_r and
    //     tapers to bw.floor_r over bw.bowl_depth — the smooth-funnel
    //     reference.
    // T17: the profile checks cover the BENCH rings only (ri 0..2*Nb-1); the
    // adapter / tube-seam / floor-disk rings that follow are the mouth
    // handoff + flat floor, not wall profile (the disk legitimately re-widens
    // and dips under the smooth-funnel reference).
    const int stair_rings = 2 * render::kBowlBenches;
    double prev_rad = 1e18, prev_depth = -1e18;
    for (int ri = 0; ri < stair_rings; ++ri) {
        double rad, depth;
        ring_stat(ri, rad, depth);
        // (3) monotonic (non-increasing radius, non-decreasing depth), + slack.
        REQUIRE(rad <= prev_rad + 1.0);
        REQUIRE(depth >= prev_depth - 1.0);
        prev_rad = rad;
        prev_depth = depth;
        // (4) at-or-outside the smooth funnel radius at this depth.
        const double f = depth / bw.bowl_depth;  // 0 rim .. 1 floor
        const double r_smooth = bw.bowl_r + (bw.floor_r - bw.bowl_r) * f;
        REQUIRE(rad >= r_smooth - 2.0);
    }

    // Non-vacuous: the profile actually DESCENDS and NARROWS (rim wide+shallow,
    // the tube floor mouth: narrow + deep).
    double rim_rad, rim_depth, floor_rad, floor_depth;
    ring_stat(0, rim_rad, rim_depth);
    // The tube-end SEAM ring (stairs, adapter, then the seam — T17 layout).
    ring_stat(stair_rings + 1, floor_rad, floor_depth);
    REQUIRE(rim_rad > floor_rad + 50.0);      // narrows toward the floor
    REQUIRE(floor_depth > rim_depth + 50.0);  // descends into the pit
}

// ----------------------------------------------------------------------------
// 4. FIX 3 — COLLAR COVERAGE: the collar outer reach covers the full jagged
//    terrain edge so there is no uncovered annulus between the cut hole and the
//    collar rim in any azimuth direction.
//
//    The test builds a fill_face with the actual mouth cut disk, collects the
//    surviving-triangle vertices within 400 m of the mouth dir, bins them into
//    64 azimuth sectors, and REQUIRES that every populated bin has a surviving-
//    terrain vertex at arc-distance <= collar_outer_m (terrain starts before
//    the collar ends — no gap bin).
//
//    Mutation target (m8): collar_outer reverted to cut_radius=140 m. Surviving
//    terrain vertices lie BEYOND 140 m in ragged arcs (cells ~118 m); some bins
//    have their nearest surviving vertex at ~200-300 m > 140 m => FAILS.
// ----------------------------------------------------------------------------

TEST_CASE(
    "T2 collar coverage: no terrain gap between hole edge and collar rim") {
    // Use N=200, tiles=2 — the same planet knobs as the real app
    // (config/world.toml [planet] subdiv=200 tiles=2). Equatorial cell arc ~59
    // m is well under the cut radius (140 m) so several cells are cleanly
    // removed, leaving a ragged edge the collar must span. The collar_reach()
    // formula uses these same knobs.
    const int N = 200;
    const int tiles = 2;

    const world::TunnelParams tp = test_tp();
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);

    // Home mouth direction (same source as the generator). NB the mouth is now
    // SUNK (T6c), so the mouth DIRECTION is the pit axis (==
    // kTunnelMouthErrington).
    const glm::dvec3 mouth_dir = world::kTunnelMouthErrington;
    // T6c: the Errington cut spans the ENTRY PIT rim, not a tube-scaled hole.
    const double cut_r =
        world::errington_cut_radius(net, render::kMouthCutFactor);

    // Collar reach: the formula that also drives build_tunnel_mesh (single
    // source).
    const double collar_m = render::collar_reach(cut_r, N, tiles, kR);

    // Build the terrain face containing the home mouth with a cut disk.
    const render::CutDisk cut{mouth_dir, cut_r};
    const render::FaceCoord fc = render::dir_to_face(mouth_dir);
    const render::FaceMesh cut_mesh =
        render::fill_face(hf, fc.face, N, 0, 0, tiles, {cut});

    // Collect surviving-triangle vertices within 500 m of the mouth dir.
    // A vertex is "in range" when its great-circle arc * R < 500 m.
    // At N=200 (~118 m cells) this captures ~4 rings of cells in every
    // direction, giving enough density for 32 azimuth bins.
    const double range_m = 500.0;
    std::vector<glm::dvec3> near_verts;
    near_verts.reserve(cut_mesh.indices.size());
    for (std::size_t ti = 0; ti + 2 < cut_mesh.indices.size(); ti += 3) {
        for (int k = 0; k < 3; ++k) {
            const unsigned short idx = cut_mesh.indices[ti + k];
            const glm::dvec3 v(cut_mesh.positions[3 * idx + 0],
                               cut_mesh.positions[3 * idx + 1],
                               cut_mesh.positions[3 * idx + 2]);
            const glm::dvec3 d = glm::normalize(v);
            const double dot = glm::clamp(glm::dot(d, mouth_dir), -1.0, 1.0);
            const double arc = std::acos(dot) * kR;
            if (arc < range_m) near_verts.push_back(v);
        }
    }

    // Premise: every azimuth bin around the mouth must be populated — a
    // sparsely-tiled face that has no surviving terrain within 500 m in some
    // bin would make the main check vacuously pass. At N=200 tiles=2 (~59 m
    // cells) a 500 m window covers ~8 cells on each side, giving ample verts
    // per bin.
    constexpr int kBins = 32;
    // Build a tangent frame around mouth_dir for azimuth binning.
    const glm::dvec3 ref = std::fabs(mouth_dir.z) < 0.9 ? glm::dvec3{0, 0, 1}
                                                        : glm::dvec3{1, 0, 0};
    const glm::dvec3 tu =
        glm::normalize(ref - glm::dot(ref, mouth_dir) * mouth_dir);
    const glm::dvec3 tw = glm::normalize(glm::cross(mouth_dir, tu));

    // Per bin: nearest surviving-terrain vertex arc distance.
    const double kInf = 1e18;
    std::array<double, kBins> bin_nearest;
    bin_nearest.fill(kInf);
    std::array<int, kBins> bin_count;
    bin_count.fill(0);

    for (const glm::dvec3& v : near_verts) {
        const glm::dvec3 d = glm::normalize(v);
        const double dot = glm::clamp(glm::dot(d, mouth_dir), -1.0, 1.0);
        const double arc = std::acos(dot) * kR;
        const double vtu = glm::dot(d, tu);
        const double vtw = glm::dot(d, tw);
        const double az = std::atan2(vtw, vtu);  // -pi .. pi
        const int bin =
            static_cast<int>((az + 3.14159265358979323846) /
                             (2.0 * 3.14159265358979323846) * kBins) %
            kBins;
        bin_count[bin]++;
        if (arc < bin_nearest[bin]) bin_nearest[bin] = arc;
    }

    // Premise: all 32 bins populated — at N=200 tiles=2 the terrain mesh has
    // ~59 m cells; a 500 m window around the mouth captures enough surviving
    // verts to populate every bin.
    for (int b = 0; b < kBins; ++b) {
        REQUIRE(bin_count[b] > 0);
    }

    // Main check: the nearest surviving-terrain vertex in each bin must be <=
    // collar_m. This means "terrain begins before the collar ends" — no
    // uncovered annulus in any direction.
    // Mutation m8 kills this: reverting collar to 140 m (= cut_radius) means
    // some bins have their nearest terrain vertex at cut_radius +
    // 0..2*cell_diag
    // (~140-306 m at tiles=2); those beyond 140 m cause bin_nearest > 140 =>
    // FAILS.
    for (int b = 0; b < kBins; ++b) {
        REQUIRE(bin_nearest[b] <= collar_m);
    }
}

// ----------------------------------------------------------------------------
// 5. T4b — GASLAMPS + the lit Black Stope (render::place_tunnel_lamps + the
//    PieceKind tagging / lit egg cue). PURE targets.
// ----------------------------------------------------------------------------
TEST_CASE(
    "T4b lamps: every placed lamp is strictly inside the net (not buried)") {
    // The buried-lamp pin (mutation m19: tube-lamp inset sign flipped => the
    // lamps mount toward/through the wall). Two ways it fails HERE: a leaked
    // buried lamp trips the strict-inside bound; else the strict-inside GUARD
    // drops the now-outside tube lamps and the count collapses below the floor.
    const world::TunnelParams tp = test_tp();
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);
    const std::vector<render::TunnelLamp> lamps =
        render::place_tunnel_lamps(net);

    // Premise: the full TUBE family placed (> 40 for the ~13 km path). m19
    // pushes tube lamps outside the wall; the guard drops them and this floor
    // trips (else a leaked buried lamp trips the strict-inside bound below).
    int tube = 0;
    for (const render::TunnelLamp& L : lamps)
        if (L.intensity == render::kTubeLampIntensity) ++tube;
    REQUIRE(tube > 40);
    for (const render::TunnelLamp& L : lamps) {
        // T10.1: the cavern EMBER + BREACH-BEACON families sit ON the ceiling
        // sphere (sd ~ 0, the net boundary — they light the mantle underside
        // from a point ON it, not from strictly inside). The strict-inside pin
        // is for the tube/chamber/connector families only; the on-ceiling
        // placement is pinned by the T10.1 lamps leg. Skip the ceiling families
        // here.
        if (L.intensity == render::kCavernEmberIntensity ||
            L.intensity == render::kBreachBeaconIntensity)
            continue;
        REQUIRE(net.signed_distance(L.pos) < -2.0);  // strictly inside
    }
}

TEST_CASE("T5b lamps: floor-edge PAIRS on both sides + crown line") {
    // The T5b layout replaces the old ±45° alternating tube lamps: floor-edge
    // pairs lining BOTH sides of the spine + a crown line along the top. Test
    // with the FLOOR ON (the pairs sit at the chord edges). Classify each
    // kTubeLampIntensity lamp by its lateral/up signature relative to its
    // nearest spine station: LEFT/RIGHT floor edge (below center, opposite
    // lateral signs) vs CROWN (above center, ~on the up axis).
    world::TunnelParams tp = test_tp();
    tp.floor_height_m = 20.0;  // floor ON => floor-edge lamps at the chord
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);
    const std::vector<render::TunnelLamp> lamps =
        render::place_tunnel_lamps(net);

    // Config-derived count floor: the spine is ~13 km; the crown line at
    // kCrownLampSpacing=120 m => > 40 crown lamps; the floor pairs at
    // kFloorLampSpacing=60 m on both sides => even more. So the tube family
    // (kTubeLampIntensity = both floor + crown) is a large count.
    int left = 0, right = 0, crown = 0;
    for (const render::TunnelLamp& L : lamps) {
        if (L.intensity != render::kTubeLampIntensity) continue;
        // nearest spine node
        double best = 1e30;
        std::size_t bi = 0;
        for (std::size_t i = 0; i < net.spine.size(); ++i) {
            const double d2 =
                glm::dot(L.pos - net.spine[i].pos, L.pos - net.spine[i].pos);
            if (d2 < best) {
                best = d2;
                bi = i;
            }
        }
        const glm::dvec3 c = net.spine[bi].pos;
        const glm::dvec3 up = glm::normalize(c);
        const glm::dvec3 tan =
            bi + 1 < net.spine.size()
                ? glm::normalize(net.spine[bi + 1].pos - net.spine[bi].pos)
                : glm::normalize(net.spine[bi].pos - net.spine[bi - 1].pos);
        glm::dvec3 u_side = glm::cross(tan, up);
        if (glm::length(u_side) < 1e-9) continue;
        u_side = glm::normalize(u_side);
        const glm::dvec3 off = L.pos - c;
        const double up_c = glm::dot(off, up);     // + above center
        const double lat = glm::dot(off, u_side);  // lateral sign
        if (up_c > 0.0 && std::abs(lat) < 20.0) {
            ++crown;  // near the top, on the up axis
        } else if (up_c < 0.0) {
            if (lat < 0.0)
                ++left;
            else
                ++right;
        }
    }
    // Both floor edges are lined (runway read) AND the crown line exists.
    REQUIRE(left > 15);
    REQUIRE(right > 15);
    REQUIRE(crown > 15);
    // Both sides roughly balanced (the pair places one each; skips affect
    // both).
    REQUIRE(std::abs(left - right) <= left / 3 + 2);
}

TEST_CASE("T5b lamps: floor-OFF fallback still lines both walls (no floor)") {
    // With the floor OFF, the floor-edge pair falls back to low wall positions
    // (~±kFloorEdgeDownDeg below horizontal). Both walls still get a
    // runway-edge line + the crown line still runs. (The shared fixture has
    // floor OFF.)
    const world::TunnelParams tp = test_tp();  // floor OFF
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);
    REQUIRE(net.floor_height == 0.0);  // premise: the fallback path
    const std::vector<render::TunnelLamp> lamps =
        render::place_tunnel_lamps(net);

    int left = 0, right = 0, crown = 0;
    for (const render::TunnelLamp& L : lamps) {
        if (L.intensity != render::kTubeLampIntensity) continue;
        double best = 1e30;
        std::size_t bi = 0;
        for (std::size_t i = 0; i < net.spine.size(); ++i) {
            const double d2 =
                glm::dot(L.pos - net.spine[i].pos, L.pos - net.spine[i].pos);
            if (d2 < best) {
                best = d2;
                bi = i;
            }
        }
        const glm::dvec3 c = net.spine[bi].pos;
        const glm::dvec3 up = glm::normalize(c);
        const glm::dvec3 tan =
            bi + 1 < net.spine.size()
                ? glm::normalize(net.spine[bi + 1].pos - net.spine[bi].pos)
                : glm::normalize(net.spine[bi].pos - net.spine[bi - 1].pos);
        glm::dvec3 u_side = glm::cross(tan, up);
        if (glm::length(u_side) < 1e-9) continue;
        u_side = glm::normalize(u_side);
        const glm::dvec3 off = L.pos - c;
        const double up_c = glm::dot(off, up);
        const double lat = glm::dot(off, u_side);
        if (up_c > 0.0 && std::abs(lat) < 20.0)
            ++crown;
        else if (up_c < 0.0 && lat < 0.0)
            ++left;
        else if (up_c < 0.0 && lat > 0.0)
            ++right;
    }
    REQUIRE(left > 15);
    REQUIRE(right > 15);
    REQUIRE(crown > 15);
}

// ----------------------------------------------------------------------------
// 5b. T10.1 — CAVERN READABILITY LAMPS: the ember field + breach beacon rings.
//     The driver's screenshot verdict (docs/tunnel_staging.md T10.1): the far
//     shell read pure black, the ceiling was a featureless wash, and the
//     breaches were unfindable dark blobs. The fix scatters an ember FIELD on
//     the ceiling (depth/parallax) + a RING of bright beacons around each
//     breach lip (the exits read as rings of light). PURE placement pins here.
// ----------------------------------------------------------------------------

TEST_CASE("T11.1 lamps: ember field + breach beacon rings, on the arena") {
    const world::TunnelParams tp = test_tp();
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);
    REQUIRE(net.arena_on);  // premise: the arena is live (canon dials)
    const std::vector<render::TunnelLamp> lamps =
        render::place_tunnel_lamps(net);

    // Classify by the intensity tier selectors. T12: the MOUTH beacons ALSO use
    // the bright kBreachBeaconIntensity, but sit at the spine mouths (near the
    // surface); the arena BREACH beacons sit on the arena ceiling (thousands of
    // metres deeper). Isolate the breach beacons by nearness to the arena.
    const glm::dvec3 front = net.spine.front().pos;
    const glm::dvec3 back = net.spine.back().pos;
    const double mouth_near = 2.0 * net.tube_width + 50.0;
    int embers = 0, beacons = 0;
    for (const render::TunnelLamp& L : lamps) {
        if (L.intensity == render::kCavernEmberIntensity) ++embers;
        if (L.intensity == render::kBreachBeaconIntensity) {
            const bool at_mouth = glm::length(L.pos - front) < mouth_near ||
                                  glm::length(L.pos - back) < mouth_near;
            if (!at_mouth) ++beacons;  // the arena breach-ring beacons only
        }
    }
    // T14: the SAME breaches + hole predicate the mesh cut with (the single
    // source the round-10 fork violated).
    const std::vector<render::Breach> breaches = render::breach_points(net);
    REQUIRE(breaches.size() == 2);  // premise: both bores enter the arena
    const auto in_hole = [&](const glm::dvec3& dir) {
        return render::breach_hole_hit(breaches, dir);
    };

    // (a) BEACON COUNT — exactly kBreachBeaconCount per breach ring, two rings.
    //     (The two rings are always placed on the arena surface — the beacon
    //     lip is projected onto the arena ceiling. Mouth beacons excluded.)
    REQUIRE(beacons == 2 * render::kBreachBeaconCount);

    // (b) EMBER COUNT is dense: the ceiling lattice (kCavernEmberCount) PLUS
    // the
    //     T12 FLOOR lattice (kFloorEmberCount), minus the breach-hole skip.
    //     Non-vacuous + bounded by the two lattices' total.
    REQUIRE(embers > 40);
    REQUIRE(embers <= render::kCavernEmberCount + render::kFloorEmberCount);
    // Both halves are populated: at least some embers ride the FLOOR half
    // (T12).
    int floor_embers = 0, ceil_embers = 0;
    for (const render::TunnelLamp& L : lamps) {
        if (L.intensity != render::kCavernEmberIntensity) continue;
        if (glm::dot(L.pos - net.arena.center, net.arena.u_long) < 0.0)
            ++floor_embers;
        else
            ++ceil_embers;
    }
    REQUIRE(floor_embers > 20);  // the sealed floor is lit (not a black void)
    REQUIRE(ceil_embers > 20);   // the ceiling still lit

    // (c) EVERY ember + arena BREACH beacon sits ON the arena surface (the
    //     arena owns the SDF min at the ceiling, so |signed_distance| ~ 0
    //     there). The MOUTH beacons (T12) sit on the tube wall (inside the net,
    //     ~inset deep), so they are excluded from this arena-surface check.
    for (const render::TunnelLamp& L : lamps) {
        if (L.intensity == render::kCavernEmberIntensity) {
            REQUIRE(std::abs(net.signed_distance(L.pos)) < 2.0);
        } else if (L.intensity == render::kBreachBeaconIntensity) {
            const bool at_mouth = glm::length(L.pos - front) < mouth_near ||
                                  glm::length(L.pos - back) < mouth_near;
            if (!at_mouth) REQUIRE(std::abs(net.signed_distance(L.pos)) < 2.0);
        }
    }

    // (d) EMBERS ride EITHER hemisphere (T12: the ceiling mantle underside AND
    //     the sealed floor) and none lands inside a breach hole.
    for (const render::TunnelLamp& L : lamps) {
        if (L.intensity != render::kCavernEmberIntensity) continue;
        REQUIRE_FALSE(in_hole(glm::normalize(L.pos)));
    }

    // (e) EACH beacon ring encircles a distinct TRUE breach: every arena
    //     beacon's DIRECTION (the same planet-centre direction space the facet
    //     cut keys on) sits on its ring ellipse around the TRUE crossing axis
    //     (the round-10 bug had them ringing a fake hole ~800 m of arc away,
    //     over rock), and both breaches get a full ring. The bound is the
    //     ring's own baked semi-major arc (config-relative — a hole/stretch
    //     retune moves it through the Breach) + slack; angular, so the
    //     oblique-wall surface projection cannot inflate it.
    int ring0 = 0, ring1 = 0;
    for (const render::TunnelLamp& L : lamps) {
        if (L.intensity != render::kBreachBeaconIntensity) continue;
        // Exclude the T12 mouth beacons (at the spine mouths, not the arena).
        if (glm::length(L.pos - front) < mouth_near ||
            glm::length(L.pos - back) < mouth_near)
            continue;
        const glm::dvec3 ld = glm::normalize(L.pos);
        const auto arc_to = [&](const render::Breach& b) {
            const double R_local = glm::length(b.pos);
            const glm::dvec3 ax = b.pos / R_local;
            return std::acos(glm::clamp(glm::dot(ld, ax), -1.0, 1.0)) * R_local;
        };
        const double a0 = arc_to(breaches[0]);
        const double a1 = arc_to(breaches[1]);
        const render::Breach& nb = a0 < a1 ? breaches[0] : breaches[1];
        const double ring_major =
            nb.hole_major_m + render::kBreachBeaconArcMargin_m;
        REQUIRE(std::min(a0, a1) < ring_major + 100.0);  // rings the REAL lip
        if (a0 < a1)
            ++ring0;
        else
            ++ring1;
    }
    REQUIRE(ring0 == render::kBreachBeaconCount);
    REQUIRE(ring1 == render::kBreachBeaconCount);

    // (f) BREACH-HOLE SKIP LIVE (mutation guard): widen the bore so the hole
    // arc
    //     exceeds the ember spacing — now SOME embers fall inside the holes and
    //     the skip fires, so the placed ember count DROPS below the wide net's
    //     unskipped ceiling count. tube_width 1500 => a huge hole. Dropping "if
    //     (in_hole) continue;" would keep those embers.
    world::TunnelParams wide = tp;
    wide.tube_width_m = 1500.0;
    const world::TunnelNet wnet = world::build_tunnel_net(wide, &hf);
    const std::vector<render::TunnelLamp> wlamps =
        render::place_tunnel_lamps(wnet);
    int wembers = 0;
    for (const render::TunnelLamp& L : wlamps)
        if (L.intensity == render::kCavernEmberIntensity) ++wembers;
    // The wide bore's holes eat some ceiling embers: fewer than the canon net's
    // (the canon holes are tiny). A dropped skip would keep the canon count.
    REQUIRE(wembers < embers);  // the skip actually fired on the wide bore
}

TEST_CASE(
    "T11.1 lamps: arena OFF => no embers; only the mouth beacons remain") {
    // The arena-off arm: a net with arena_on == false places ZERO ember lamps
    // and ZERO breach-ring beacons (the arena readability family is
    // arena-gated) — but the T12 MOUTH BEACON RINGS are ALWAYS present (they
    // ring the bore mouths, which exist regardless of the arena). So the only
    // bright-tier lamps left are exactly the two mouth rings (2 *
    // kMouthBeaconCount).
    world::TunnelParams tp = test_tp();
    tp.cavern_core_m = 0.0;  // arena OFF (the net builder gates arena_on on
                             // arena_a/arena_c/core > 0 && floor > core)
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);
    REQUIRE_FALSE(net.arena_on);  // premise
    const std::vector<render::TunnelLamp> lamps =
        render::place_tunnel_lamps(net);
    int bright = 0;
    for (const render::TunnelLamp& L : lamps) {
        REQUIRE(L.intensity != render::kCavernEmberIntensity);  // no embers
        if (L.intensity == render::kBreachBeaconIntensity) ++bright;
    }
    // The bright-tier lamps are exactly the two mouth rings (no breach beacons
    // with the arena off). Errington places a full ring; the Murray bowl-floor
    // mouth culls a few — so the total is between 1.5 and 2 full rings.
    REQUIRE(bright >= 3 * render::kMouthBeaconCount / 2);
    REQUIRE(bright <= 2 * render::kMouthBeaconCount);
}

TEST_CASE(
    "T14b lamps: the Murray bowl funnel rings light the pit (own tier, "
    "inside the cut, structurally off with the bowl)") {
    // Fly-11 (Chad): "Its a blind entrance to get into the murray tunnel as
    // its black and I can only see one light going down, caused me to crash."
    // The pit had no lamps above the bore mouth ring 300 m down. Two funnel
    // rings now light it: rim (surface) + mid-depth wall.
    const world::TunnelParams tp = test_tp();
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);
    REQUIRE(net.bowl.bowl_depth > 0.0);  // premise: the Murray bowl is live
    const std::vector<render::TunnelLamp> lamps =
        render::place_tunnel_lamps(net);

    // (a) COUNT + PLACEMENT: exactly rim + mid lamps on the funnel tier, and
    //     every one lives inside the bowl's open-cut cylinder (within the rim
    //     radius of the axis, between the surface plane and the floor) — a
    //     funnel lamp buried in rock or floating above grade is a placement
    //     bug. Bounds derived from the net's own bowl (config-relative).
    const world::TunnelNet::Bowl& bw = net.bowl;
    int funnel = 0;
    for (const render::TunnelLamp& L : lamps) {
        if (L.intensity != render::kBowlBeaconIntensity) continue;
        ++funnel;
        const double ax_depth = bw.surface_r - glm::dot(L.pos, bw.axis);
        const glm::dvec3 radial = L.pos - glm::dot(L.pos, bw.axis) * bw.axis;
        REQUIRE(glm::length(radial) <= bw.bowl_r + 1.0);  // inside the rim
        REQUIRE(ax_depth >= -1.0);                        // never above grade
        REQUIRE(ax_depth <= bw.bowl_depth + 1.0);  // never below the floor
    }
    REQUIRE(funnel ==
            render::kBowlRimBeaconCount + render::kBowlMidBeaconCount);

    // (b) TIER ISOLATION: the funnel tier is distinct from the breach/mouth
    //     beacon tier (T11.1's exact-tier ring counts stay honest) and lands
    //     in the draw's bright family.
    REQUIRE(render::kBowlBeaconIntensity != render::kBreachBeaconIntensity);
    REQUIRE(render::kBowlBeaconIntensity >= 2.0f);  // tunnel.cpp bright cut

    // (c) STRUCTURAL OFF: bowl_depth = 0 places ZERO funnel lamps.
    world::TunnelParams tp_off = tp;
    tp_off.bowl_depth_m = 0.0;
    const world::TunnelNet net_off = world::build_tunnel_net(tp_off, &hf);
    int funnel_off = 0;
    for (const render::TunnelLamp& L : render::place_tunnel_lamps(net_off))
        if (L.intensity == render::kBowlBeaconIntensity) ++funnel_off;
    REQUIRE(funnel_off == 0);
}

TEST_CASE("T16 Murray throat rings: light the final tube stretch to the exit") {
    // Round-12 finding: leaving the Murray tunnel into the open pit is a BLIND
    // flight — the T14b bowl beacons all sit up in the pit; nothing marks the
    // final tube stretch from inside. Three rings of throat beacons now line
    // the last ~kMurrayThroatRings*kMurrayThroatSpacing of BORE before the
    // Murray floor mouth (placed by spine arc-length from the mouth), a ladder
    // to the exit. Own bright-family tier.
    const world::TunnelParams tp = test_tp();
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);
    const std::vector<render::TunnelLamp> lamps =
        render::place_tunnel_lamps(net);

    const glm::dvec3 mouth = net.spine.back().pos;  // Murray floor mouth
    const double span =
        render::kMurrayThroatRings * render::kMurrayThroatSpacing_m;

    int throat = 0;
    double dmin = 1e18, dmax = 0.0;
    for (const render::TunnelLamp& L : lamps) {
        if (L.intensity != render::kMurrayThroatIntensity) continue;
        ++throat;
        // Inside the BORE (a ladder of rings in the tube, not in rock or the
        // open pit).
        REQUIRE(net.signed_distance(L.pos) < 0.0);
        // Within the final stretch of tube before the Murray mouth.
        const double d = glm::length(L.pos - mouth);
        REQUIRE(d <= span + 80.0);
        dmin = std::min(dmin, d);
        dmax = std::max(dmax, d);
    }
    // Exactly the three rings of eight (all sit well inside the bore, kept).
    REQUIRE(throat ==
            render::kMurrayThroatRings * render::kMurrayThroatPerRing);
    // A LADDER (near ring + far ring), not a single cluster.
    REQUIRE(dmin < 0.5 * span);
    REQUIRE(dmax > 0.5 * span);

    // Distinct tier from the bowl-funnel + breach/mouth beacons (so the T14b /
    // T11.1 / T12 exact-count legs stay honest) and in the bright draw family.
    REQUIRE(render::kMurrayThroatIntensity != render::kBowlBeaconIntensity);
    REQUIRE(render::kMurrayThroatIntensity != render::kBreachBeaconIntensity);
    REQUIRE(render::kMurrayThroatIntensity >= 2.0f);
}

TEST_CASE(
    "T12 mouth beacons: a bright ring at each bore mouth so the shaft reads") {
    // Chad's round-9 fly: "The entrance to errington mine is not very visible,
    // the black hole is but the tunnel shaft is still hard to find." A ring of
    // kMouthBeaconCount bright lamps around the ACTUAL bore opening at each
    // mouth (spine front + back), on the bore ellipse (inset from the wall), so
    // the shaft reads as a ring of light inside the dark crater from approach.
    const world::TunnelParams tp = test_tp();
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);
    const std::vector<render::TunnelLamp> lamps =
        render::place_tunnel_lamps(net);

    // Classify bright-tier lamps near each mouth station. The mouth beacons sit
    // within ~one tube span of the spine front/back; the arena breach beacons
    // sit at the arena apex (thousands of metres deeper), so a nearness gate to
    // the mouth station isolates the mouth rings.
    const glm::dvec3 front = net.spine.front().pos;
    const glm::dvec3 back = net.spine.back().pos;
    const double near = 2.0 * net.tube_width + 50.0;  // within a tube span
    int at_front = 0, at_back = 0;
    for (const render::TunnelLamp& L : lamps) {
        if (L.intensity != render::kBreachBeaconIntensity) continue;
        if (glm::length(L.pos - front) < near) ++at_front;
        if (glm::length(L.pos - back) < near) ++at_back;
    }
    // A FULL ring at the Errington mouth (the entry PIT Chad flies — the whole
    // ring clears the pit). The Murray bowl-floor mouth truncates the bore
    // ellipse against the bowl SDF, so a few of its ring beacons fall outside
    // and are culled; a SUBSTANTIAL ring (>= 2/3) still reads. Mutation: drop
    // the mouth beacon loop => at_front == at_back == 0 => FAILS. Change
    // kMouthBeaconCount => the Errington count moves in lockstep.
    REQUIRE(at_front == render::kMouthBeaconCount);
    REQUIRE(at_back >= 2 * render::kMouthBeaconCount / 3);
    REQUIRE(at_back <= render::kMouthBeaconCount);

    // Placement: every mouth beacon sits ON the bore opening (strictly inside
    // the net near the wall — |sd| within the inset), so it reads as a ring
    // around the actual shaft, not floating in the open crater. The keep_if_
    // inside gate already dropped any that fell outside; confirm the survivors
    // are near the mouth wall (a few tens of metres inside).
    for (const render::TunnelLamp& L : lamps) {
        if (L.intensity != render::kBreachBeaconIntensity) continue;
        const bool at_mouth = glm::length(L.pos - front) < near ||
                              glm::length(L.pos - back) < near;
        if (!at_mouth) continue;
        // Inside the net, ringing the bore opening (at kMouthBeaconFrac of the
        // semi-axes — well inside the wall but not deep in the middle).
        REQUIRE(net.signed_distance(L.pos) < 0.0);
        REQUIRE(net.signed_distance(L.pos) > -1.1 * net.tube_width);
    }
}

// ----------------------------------------------------------------------------
// 6. P3-1 — MURRAY RIM APP-PATH: the bowl wall built with the real
//    collar_reach() value (the APP path, subdiv=200 tiles=2) covers the jagged
//    terrain edge and places its surface rim ring at the expected radius.
//
//    The default build_tunnel_mesh() fallback uses the bare bowl_r (450 m) as
//    the collar outer radius; the APP calls collar_reach(bowl_r, 200, 2, R) ≈
//    637 m, which is the value that actually spans the terrain cut edge.
//
//    (a) The Murray rim ring (ring 0 of piece[7]) has an arc-radius from the
//        bowl axis ≈ collar_reach(bowl_r, 200, 2, R) within 10 m (config-
//        relative recompute, no welded constant).
//    (b) Rim radius >= cut_radius(bowl_r) + cell_diagonal: the collar spans
//        the terrain cut edge (mirrors the T2 collar-coverage leg's bound).
//
//    Mutation m22: build the mesh with the 450 m fallback (no collar_reach
//    call) => ring-0 arc-radius ≈ 450, violating (a)'s lower bound => FAILS.
// ----------------------------------------------------------------------------

TEST_CASE(
    "P3-1 Murray rim: app-path collar_reach places rim at the terrain-spanning "
    "radius") {
    // Reproduce the app-path collar derivation exactly as draw.cpp does it
    // (collar_reach(murray_cut_radius, subdiv=200, tiles=2, R)).
    const int subdiv = 200;
    const int tiles = 2;
    const world::TunnelParams tp = test_tp();  // bowl_radius_m = 450
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);

    // Murray cut radius = bowl_r (world::murray_cut_radius(bowl_r) == bowl_r).
    const double mur_cut = tp.bowl_radius_m;
    const double mur_collar = render::collar_reach(mur_cut, subdiv, tiles, kR);

    // Premise: the app-path collar is materially larger than the fallback
    // bowl_r (so the two arms SEPARATE — the mutation uses the fallback 450).
    REQUIRE(mur_collar > tp.bowl_radius_m + 50.0);

    // Build with the real app-path collar value.
    const render::TunnelMeshData data =
        render::build_tunnel_mesh(net, &hf, 0.0, mur_collar);

    REQUIRE(data.pieces.size() ==
            9);  // T16: +1 entry-trench piece  // T12: + arena (floor OFF);
                 // shaft + core GONE (sealed)
    // The Murray bowl wall = the LAST is_collar piece (index-robust).
    const render::TunnelPiece* bowl_p = nullptr;
    for (const render::TunnelPiece& p : data.pieces)
        if (p.is_collar) bowl_p = &p;
    REQUIRE(bowl_p != nullptr);
    const render::TunnelPiece& bowl = *bowl_p;  // Murray bowl wall
    const int seg = render::kTubeSegments;
    REQUIRE(static_cast<int>(bowl.cue.size()) >= seg);  // at least one ring

    // The bowl axis direction (the Murray centre direction).
    const glm::dvec3 bowl_axis = glm::normalize(net.bowl.axis);

    // (a) The SURFACE RIM RING (ring 0, the outermost verts) has an arc-radius
    //     from the bowl axis ≈ mur_collar within 20 m.
    //     Arc = acos(dot(normalize(v), axis)) * kR. Two small-angle effects
    //     shift it below mur_collar by ~kTuckOuter_m (tuck sinks the vertex)
    //     and ~mur_collar * (1 - kR/surface_r) ≈ 12 m (the surface sits ~300 m
    //     above R=15000 so the arc formula at kR underestimates). Both shift in
    //     the same direction and together ~ ≤ 15 m; a 20 m margin is safe and
    //     the mutation (450 fallback => arc ≈ 440) still fails by > 160 m.
    for (int s = 0; s < seg; ++s) {
        const glm::dvec3 v(bowl.positions[3 * s + 0], bowl.positions[3 * s + 1],
                           bowl.positions[3 * s + 2]);
        const glm::dvec3 vn = glm::normalize(v);
        const double dot = glm::clamp(glm::dot(vn, bowl_axis), -1.0, 1.0);
        const double arc = std::acos(dot) * kR;
        // (a) config-relative check: arc ≈ mur_collar (no welded 637).
        REQUIRE(arc >= mur_collar - 20.0);
        REQUIRE(arc <= mur_collar + 20.0);
    }

    // (b) Rim covers the jagged terrain edge: arc-radius >= cut_radius + one
    //     cell diagonal (the same bound as the T2 collar-coverage leg).
    //     cell_arc = (pi/2 * R) / (tiles * (subdiv - 1)); cell_diag = sqrt(2) *
    //     cell_arc. Config-relative: derived from the same knobs, no bare
    //     number.
    const double pi = 3.14159265358979323846;
    const double cell_arc =
        (pi * 0.5 * kR) / (static_cast<double>(tiles) * (subdiv - 1));
    const double cell_diag = 1.41421356237 * cell_arc;
    // Read back the actual rim arc from the first vert (all seg verts equal by
    // symmetry — already verified in loop (a) above).
    const glm::dvec3 v0(bowl.positions[0], bowl.positions[1],
                        bowl.positions[2]);
    const double rim_arc =
        std::acos(
            glm::clamp(glm::dot(glm::normalize(v0), bowl_axis), -1.0, 1.0)) *
        kR;
    REQUIRE(rim_arc >= mur_cut + cell_diag);
}

TEST_CASE(
    "T4b/T12 tagging: chamber/collar/arena pieces tagged; lit cues == 1.0") {
    const world::TunnelParams tp = test_tp();
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);
    const render::TunnelMeshData data = render::build_tunnel_mesh(net, &hf);

    REQUIRE(data.pieces.size() ==
            9);  // T16: +1 entry-trench piece  // T12: + arena (floor OFF);
                 // shaft + core GONE (sealed)

    // Build order (T12, floor OFF): [0] tube (kWall), [1..2] chambers
    // (kChamber), [3..4] connectors (kWall), [5..6] collars (kCollar),
    // [7] arena (kCavern). T12: the shaft (kCavern) + core (kCore) are GONE.
    using render::PieceKind;
    int wall = 0, chamber = 0, collar = 0, cavern = 0;
    for (const render::TunnelPiece& p : data.pieces) {
        switch (p.kind) {
            case PieceKind::kWall:
                ++wall;
                break;
            case PieceKind::kChamber:
                ++chamber;
                break;
            case PieceKind::kCollar:
                ++collar;
                break;
            case PieceKind::kCavern:
                ++cavern;
                break;
            case PieceKind::kFloor:
                break;  // T5a: no floor in the OFF fixture (own test)
            case PieceKind::kTrench:
                break;  // T16: trench walls (own kind) not counted here
            case PieceKind::kPortal:
                break;  // visual-only surface piece; not counted here
        }
    }
    REQUIRE(chamber == 2);  // two side chambers
    REQUIRE(collar == 2);   // home pit wall + Murray bowl wall
    REQUIRE(wall == 3);     // tube + two connectors
    REQUIRE(cavern == 1);   // the arena (T12: the shaft piece is gone)

    // is_collar mirror stays consistent with kCollar (T3 callers rely on it).
    for (const render::TunnelPiece& p : data.pieces)
        REQUIRE(p.is_collar == (p.kind == PieceKind::kCollar));

    // The LIT interiors — chambers + the arena ceiling — bake every cue at 1.0
    // (the depth floor never darkens them; the core-key/strata own the shading,
    // not the depth cue). Mutation m21 (cue NOT exempted => depth floor 0.30
    // applies to a deep lit piece) => cues drop below 1.0 => FAILS.
    for (const render::TunnelPiece& p : data.pieces) {
        if (p.kind == PieceKind::kChamber || p.kind == PieceKind::kCavern)
            for (float c : p.cue) REQUIRE(c == 1.0f);
    }
    // The deep tube (piece [0]) is NOT exempt — its deepest cue hit the floor.
    float deep_tube = 1.0f;
    for (float c : data.pieces[0].cue) deep_tube = std::min(deep_tube, c);
    REQUIRE(deep_tube < 1.0f);
}

// ----------------------------------------------------------------------------
// 6b. T11/T12 — THE SEALED-CORE ARENA (mesh side): the arena ellipsoid exists,
//     sized on the arena dims, BREACHED by the two bores (facets skipped at the
//     breach axes) with a SEALED floor (T12: no shaft opening, no core sphere);
//     the tube is SUPPRESSED below the breach (no wall across the open arena).
// ----------------------------------------------------------------------------

TEST_CASE(
    "T12 mesh: the arena exists, sized, breached; the floor is SEALED (no "
    "shaft "
    "/ no core)") {
    const world::TunnelParams tp = test_tp();
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);
    const render::TunnelMeshData data = render::build_tunnel_mesh(net, &hf);

    REQUIRE(net.arena_on);  // premise: the canon net has the arena live

    using render::PieceKind;
    // T12: ONLY the arena is kCavern (the shaft piece is gone); there is NO
    // kCore piece (the emissive core is gone — the floor is sealed over the
    // buried core).
    int ncav = 0;
    const render::TunnelPiece* arena = nullptr;
    for (const render::TunnelPiece& p : data.pieces) {
        if (p.kind == PieceKind::kCavern) {
            ++ncav;
            arena = &p;
        }
    }
    // T12: the emissive core piece (kCore) is GONE (the enum value is deleted —
    // an unrepresentable-mutant firewall) and the shaft is gone, so the arena
    // is the ONLY kCavern piece and the total is 8 (checked below).
    REQUIRE(ncav == 1);  // the arena only (T12: the shaft is gone)
    REQUIRE(data.pieces.size() ==
            9);  // T16: +1 entry-trench piece  // tube+2ch+2conn+2collar+arena
                 // (no shaft/core)
    REQUIRE(arena != nullptr);

    // Arena verts all sit ON or INSIDE the net (sd <= 2 m): they ride the arena
    // ellipsoid surface (sd ~ 0) — including the SEALED FLOOR pole verts, which
    // now sit ON the ellipsoid (no shaft opening). Confirm at least one vert is
    // on-surface (~0) so the leg is non-vacuous.
    bool any_on_surface = false;
    for (std::size_t vi = 0; vi < arena->cue.size(); ++vi) {
        const glm::dvec3 v(arena->positions[3 * vi + 0],
                           arena->positions[3 * vi + 1],
                           arena->positions[3 * vi + 2]);
        REQUIRE(net.signed_distance(v) <= 2.0);
        if (std::abs(net.signed_distance(v)) < 2.0) any_on_surface = true;
    }
    REQUIRE(any_on_surface);

    // THE SEALED FLOOR (T12 pin — the executable "filled over the core"). The
    // T11 floor-shaft opening is GONE: arena FACETS now COVER the former shaft
    // column (floor half, horizontal radius < the old shaft radius). Concretely
    // MANY indexed arena verts lie in the floor-half cylinder that the shaft
    // opening used to cut. The old test asserted in_opening == 0 (the window is
    // cut); the SEALED form INVERTS it: in_opening > 0 (the floor is filled).
    // Mutation: re-add the floor-shaft skip => in_opening drops to 0 => FAILS.
    // Use a representative former-shaft radius (a third of arena_a — the T11
    // canon shaft was 2000 m, well within this) so the pin is dial-independent.
    {
        std::set<unsigned short> referenced(arena->indices.begin(),
                                            arena->indices.end());
        const double old_shaft_r = tp.arena_a_m / 4.0;  // >= the T11 canon 2000
        int in_opening = 0;
        for (unsigned short vi : referenced) {
            const glm::dvec3 v(arena->positions[3 * vi + 0],
                               arena->positions[3 * vi + 1],
                               arena->positions[3 * vi + 2]);
            const glm::dvec3 loc = v - net.arena.center;
            const double horiz_r =
                std::sqrt(std::pow(glm::dot(loc, net.arena.u_lat), 2.0) +
                          std::pow(glm::dot(loc, net.arena.u_up), 2.0));
            // Floor half (below the arena equator) inside the old shaft radius.
            if (glm::dot(loc, net.arena.u_long) < 0.0 && horiz_r < old_shaft_r)
                ++in_opening;
        }
        REQUIRE(in_opening >
                0);  // the floor is SEALED (facets fill the column)
    }

    // The BREACH-HOLE mechanism: the arena WALL is breached by the two bores,
    // so its wall-facet count is STRICTLY LESS than a full ellipsoid grid (the
    // breach holes cut facets; T12: NO floor-shaft opening cut). T16: the arena
    // piece now also carries the two on-wall breach collars (verts appended
    // AFTER the grid), so count only the GRID-WALL triangles (all three indices
    // below the grid vertex count) to isolate the hole mechanism from the
    // collar.
    const std::size_t full =
        static_cast<std::size_t>(render::kCavernLong) * render::kCavernLat * 6;
    const unsigned int grid_verts =
        static_cast<unsigned int>(render::kCavernLat + 1) *
        static_cast<unsigned int>(render::kCavernLong + 1);
    std::size_t wall_idx = 0;
    for (std::size_t t = 0; t + 2 < arena->indices.size(); t += 3)
        if (arena->indices[t] < grid_verts &&
            arena->indices[t + 1] < grid_verts &&
            arena->indices[t + 2] < grid_verts)
            wall_idx += 3;
    REQUIRE(wall_idx < full);      // breach holes cut wall facets
    REQUIRE(wall_idx > full / 2);  // not everything removed

    // WIDEN THE BORE => bigger breach holes => strictly fewer arena facets kept
    // (the skip fires harder). Mutation: dropping the breach-hole skip keeps
    // the canon count regardless of bore width.
    world::TunnelParams wide = tp;
    wide.tube_width_m = 1500.0;
    const world::TunnelNet wnet = world::build_tunnel_net(wide, &hf);
    const render::TunnelMeshData wdata = render::build_tunnel_mesh(wnet, &hf);
    const render::TunnelPiece* wcav = nullptr;
    for (const render::TunnelPiece& p : wdata.pieces)
        if (p.kind == PieceKind::kCavern &&
            (wcav == nullptr || p.cue.size() > wcav->cue.size()))
            wcav = &p;  // the arena (the bigger kCavern piece)
    REQUIRE(wcav != nullptr);
    REQUIRE(wcav->indices.size() < arena->indices.size());  // bigger holes
}

TEST_CASE(
    "T11/T14 mesh: tube suppressed exactly at the arena (no wall across open "
    "air, no wall-less tube outside it)") {
    const world::TunnelParams tp = test_tp();
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);
    const render::TunnelMeshData data = render::build_tunnel_mesh(net, &hf);

    REQUIRE(net.arena_on);
    const render::TunnelPiece& tube = data.pieces[0];
    REQUIRE(tube.kind == render::PieceKind::kWall);
    const std::vector<render::Breach> breaches = render::breach_points(net);
    REQUIRE(breaches.size() == 2);

    // (a) NO WALL ACROSS OPEN AIR: no emitted tube vert sits materially inside
    //     the arena ellipsoid. A ring's node is OUTSIDE the arena (that is the
    //     T14 suppression rule), but its ellipse verts extend up to one tube
    //     semi-axis from the node, so the honest bound is "never deeper inside
    //     than one tube semi-axis + slack". Mutation guard: dropping the
    //     suppression emits the whole plateau run — thousands of metres inside.
    const double max_semi = std::max(net.tube_width, net.tube_height);
    for (std::size_t vi = 0; vi < tube.cue.size(); ++vi) {
        const glm::dvec3 v(tube.positions[3 * vi + 0],
                           tube.positions[3 * vi + 1],
                           tube.positions[3 * vi + 2]);
        REQUIRE(world::ellipsoid_sd(v, net.arena) >= -(max_semi + 20.0));
    }
    REQUIRE(tube.cue.size() > 0);
    // Non-vacuous: the suppressed node range exists (those nodes would have
    // emitted deep-inside rings without the rule).
    REQUIRE(breaches[0].node_in <= breaches[1].node_in);
    REQUIRE(world::ellipsoid_sd(net.spine[breaches[0].node_in].pos, net.arena) <
            0.0);

    // (b) NO WALL-LESS TUBE (the round-10 scout finding: the old radius rule
    //     suppressed ~410 m of tube per side OUTSIDE the arena, in solid
    //     rock): the tube reaches CLOSE to each true crossing — some emitted
    //     vert within one node spacing + one tube semi-axis of each breach.
    const double spacing = tp.spacing_m > 0.0 ? tp.spacing_m : 150.0;
    for (const render::Breach& b : breaches) {
        double nearest = 1e30;
        for (std::size_t vi = 0; vi < tube.cue.size(); ++vi) {
            const glm::dvec3 v(tube.positions[3 * vi + 0],
                               tube.positions[3 * vi + 1],
                               tube.positions[3 * vi + 2]);
            nearest = std::min(nearest, glm::length(v - b.pos));
        }
        INFO("nearest emitted tube vert to breach: " << nearest << " m");
        REQUIRE(nearest < spacing + max_semi + 50.0);
    }
}

namespace {
// T14 rim-walk oracle (shared by both config arms below): walk the bore's
// actual TUBE-WALL lines (the centreline offset by the tube ellipse in the
// crossing's own frame) through the arena ellipsoid on world::ellipsoid_sd —
// the SDF's own wall — and require every true-opening rim direction inside the
// T14 cut (pad 0 — the exact hole).
void t14_rim_walk(const world::TunnelParams& tp) {
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);
    REQUIRE(net.arena_on);
    const std::vector<render::Breach> breaches = render::breach_points(net);
    REQUIRE(breaches.size() == 2);
    const double pi = 3.14159265358979323846;

    for (const render::Breach& b : breaches) {
        // (a) The locator sits ON the arena wall (the bisection converged onto
        //     sd == 0), within one segment of its own inside node, with a sane
        //     elongation. (The old radius-rule locator fails THIS instantly:
        //     its "breach" sits 748-897 m off the wall at arena_a_m=4200.)
        REQUIRE(std::abs(world::ellipsoid_sd(b.pos, net.arena)) < 1.0);
        REQUIRE(b.node_in < net.spine.size());
        REQUIRE(glm::length(b.pos - net.spine[b.node_in].pos) <
                2.0 * tp.spacing_m);
        REQUIRE(b.stretch >= 1.0);
        REQUIRE(b.stretch <= render::kBreachMaxStretch);
        REQUIRE(glm::length(b.into) == Catch::Approx(1.0).margin(1e-9));

        // (b) THE CUT ENCLOSES THE TRUE OPENING.
        const glm::dvec3 t = b.into;
        const glm::dvec3 up = glm::normalize(b.pos);
        glm::dvec3 horiz = glm::cross(t, up);
        REQUIRE(glm::length(horiz) > 1e-6);
        horiz = glm::normalize(horiz);
        const glm::dvec3 vert = glm::normalize(glm::cross(horiz, t));
        int rim_pts = 0;
        for (int k = 0; k < 24; ++k) {
            const double a = 2.0 * pi * k / 24.0;
            const glm::dvec3 off = net.tube_width * std::cos(a) * horiz +
                                   net.tube_height * std::sin(a) * vert;
            const auto inside = [&](double s) {
                return world::ellipsoid_sd(b.pos + off + s * t, net.arena) <
                       0.0;
            };
            double lo = -1500.0, hi = 1500.0;
            const bool in_lo = inside(lo);
            if (in_lo == inside(hi)) continue;  // grazing line: no crossing
            for (int it = 0; it < 60; ++it) {
                const double mid = 0.5 * (lo + hi);
                if (inside(mid) == in_lo)
                    lo = mid;
                else
                    hi = mid;
            }
            const glm::dvec3 rim = b.pos + off + (0.5 * (lo + hi)) * t;
            ++rim_pts;
            INFO("rim angle " << k << " offset " << glm::length(rim - b.pos)
                              << " m");
            REQUIRE(render::breach_hole_hit(breaches, glm::normalize(rim)));
        }
        REQUIRE(rim_pts >= 16);  // non-vacuous: the opening was truly walked
    }
}
}  // namespace

TEST_CASE(
    "T14: the locator sits ON the arena wall and the cut hole ENCLOSES the "
    "true bore opening (shipped arena_a_m = 4200)") {
    // THE round-10 regression pin (docs/tunnel_scout_2026-07-20.md): at the
    // SHIPPED T13 arena (4200, not the 7350 fixture canon) the old radius-rule
    // locator put the 275 m holes 748/897 m from the true wall crossings — NO
    // overlap, so both real openings stayed opaque wall. Kills the old-locator
    // mutant via leg (a) + the whole rim walk. HONEST SCOPE (red-team P1):
    // at 4200 the shipped crossing is NOT oblique enough for a stretch:=1
    // mutant to fail this arm (max rim offset ~255 m vs the 275 m round hole)
    // — the stretch mechanism is pinned by the 7350 arm below.
    world::TunnelParams tp = test_tp();
    tp.arena_a_m = 4200.0;  // config/game.toml ships 4200 (T13 centering)
    t14_rim_walk(tp);
}

namespace {
// Moller-Trumbore ray-triangle intersection (two-sided). Returns t >= 0.
bool ray_tri(const glm::dvec3& o, const glm::dvec3& d, const glm::dvec3& a,
             const glm::dvec3& b, const glm::dvec3& c, double& t) {
    const glm::dvec3 e1 = b - a, e2 = c - a;
    const glm::dvec3 pv = glm::cross(d, e2);
    const double det = glm::dot(e1, pv);
    if (std::abs(det) < 1e-12) return false;
    const double inv = 1.0 / det;
    const glm::dvec3 tv = o - a;
    const double u = glm::dot(tv, pv) * inv;
    if (u < 0.0 || u > 1.0) return false;
    const glm::dvec3 qv = glm::cross(tv, e1);
    const double v = glm::dot(d, qv) * inv;
    if (v < 0.0 || u + v > 1.0) return false;
    t = glm::dot(e2, qv) * inv;
    return t > 0.0;
}
}  // namespace

TEST_CASE(
    "T15: no see-through rock void at the breaches (every hole sightline is "
    "sealed by tube, collar, or wall)") {
    // Fly-11 (Chad): "before I enter the murray tunnel from the west, I can
    // see through the ground to the surface." The cut hole is wider than the
    // tube's silhouette, and the rock beyond the wall is NOT geometry (terrain
    // is backface-invisible from below) — so a sightline through the hole
    // margin escaped to the surface. The T15 breach collar seals it. THE KILL
    // TEST: from an eye inside the arena at each breach, cast rays at a grid
    // spanning the whole cut hole + rim; a ray that reaches 700 m above the
    // crossing radius WITHOUT hitting any drawn triangle AND outside the net
    // volume (i.e. not legitimately climbing inside the tube) is a LEAK.
    // Dropping the collar (or re-widening the cut past it) fails this loudly.
    world::TunnelParams tp = test_tp();
    tp.arena_a_m = 4200.0;  // the SHIPPED arena (where fly-11 saw it)
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);
    const render::TunnelMeshData data = render::build_tunnel_mesh(net, &hf);
    const std::vector<render::Breach> breaches = render::breach_points(net);
    REQUIRE(breaches.size() == 2);

    // Collect occluder triangles (double precision) from the tube + cavern
    // pieces near the breaches (the collars/chambers are km away).
    struct Tri {
        glm::dvec3 a, b, c;
    };
    std::vector<Tri> tris;
    for (const render::TunnelPiece& p : data.pieces) {
        if (p.kind != render::PieceKind::kWall &&
            p.kind != render::PieceKind::kCavern)
            continue;
        for (std::size_t t = 0; t + 2 < p.indices.size(); t += 3) {
            const auto vtx = [&](std::size_t k) {
                const std::size_t i = p.indices[t + k];
                return glm::dvec3(p.positions[3 * i + 0],
                                  p.positions[3 * i + 1],
                                  p.positions[3 * i + 2]);
            };
            const Tri tr{vtx(0), vtx(1), vtx(2)};
            const glm::dvec3 cen = (tr.a + tr.b + tr.c) / 3.0;
            bool near = false;
            for (const render::Breach& b : breaches)
                if (glm::length(cen - b.pos) < 2500.0) near = true;
            if (near) tris.push_back(tr);
        }
    }
    REQUIRE(tris.size() > 100);  // non-vacuous occluder set

    for (const render::Breach& b : breaches) {
        const double R_local = glm::length(b.pos);
        const glm::dvec3 ax = b.pos / R_local;
        glm::dvec3 e1 = b.into - glm::dot(b.into, ax) * ax;
        REQUIRE(glm::length(e1) > 1e-9);
        e1 = glm::normalize(e1);
        const glm::dvec3 e2 = glm::normalize(glm::cross(ax, e1));
        const glm::dvec3 eye = b.pos + 400.0 * b.into;  // inside the arena
        const double esc_r = R_local + 700.0;  // "escaped into rock" shell

        int leaks = 0, rays = 0;
        for (int iu = -6; iu <= 6; ++iu) {
            for (int iv = -4; iv <= 4; ++iv) {
                // Target grid spanning the hole + rim margin on the
                // breach-radius shell (angle space, same frame as the cut).
                const double um =
                    (iu / 6.0) * (b.hole_major_m + 150.0) / R_local;
                const double vm =
                    (iv / 4.0) * (b.hole_minor_m + 150.0) / R_local;
                const double rho = std::sqrt(um * um + vm * vm);
                glm::dvec3 wdir = ax;
                if (rho > 1e-12) {
                    const glm::dvec3 dirp = (um * e1 + vm * e2) / rho;
                    wdir = glm::normalize(std::cos(rho) * ax +
                                          std::sin(rho) * dirp);
                }
                const glm::dvec3 target = wdir * R_local;
                const glm::dvec3 dir = glm::normalize(target - eye);
                ++rays;
                // Escape distance: |eye + s*dir| == esc_r (outward root).
                const double bq = glm::dot(eye, dir);
                const double cq = glm::dot(eye, eye) - esc_r * esc_r;
                const double disc = bq * bq - cq;
                REQUIRE(disc > 0.0);
                const double s_esc = -bq + std::sqrt(disc);
                double nearest = 1e30;
                for (const Tri& tr : tris) {
                    double t;
                    if (ray_tri(eye, dir, tr.a, tr.b, tr.c, t))
                        nearest = std::min(nearest, t);
                }
                if (nearest < s_esc) continue;  // sealed (wall/tube/collar)
                // No hit before escape: legal ONLY if the ray is climbing
                // inside the net volume (up the open tube toward the mouth).
                const glm::dvec3 esc_pt = eye + s_esc * dir;
                if (!net.contains(esc_pt)) {
                    ++leaks;
                    WARN("LEAK at grid (" << iu << "," << iv << ") esc |p|="
                                          << glm::length(esc_pt));
                }
            }
        }
        REQUIRE(rays == 13 * 9);
        REQUIRE(leaks == 0);  // NO see-through rock void at this breach
    }
}

// ---------------------------------------------------------------------------
// T16 — THE WIDENED LEAK AUDIT (the round's proof). The round-11 "46 leaks ->
// 0" banner was cast from ONE eye 400 m inside each of the two arena breaches,
// so it provably never covered the Errington surface, the approach trench, or a
// general chamber viewpoint — which is why round 12 still found see-through
// holes after it. This audits the FINAL T16 geometry from MANY eyes (surface +
// interior) with dense ray fans, and defines a leak by COLLISION TRUTH: a
// sightline that passes through SOLID ROCK (underground AND outside the open
// net) with no render occluder in front, then reaches open sky = you saw
// through unrendered rock to space. Rays that leave through an OPEN cut (up the
// pit / mouth / tube, never entering solid rock) are legitimate and excluded by
// construction (they never traverse !contains underground space).
namespace {
struct AuditTri {
    glm::dvec3 a, b, c;
    glm::dvec3 cen{0.0};  // centroid + bounding radius (ray broad-phase)
    double rad = 0.0;
    int kind = -1;  // T26: PieceKind as int (occluder attribution; -1 = unset)
    int pidx = -1;  // T26: index into data.pieces (occluder attribution)
};

void finish_tri(AuditTri& t) {
    t.cen = (t.a + t.b + t.c) / 3.0;
    t.rad = std::max({glm::length(t.a - t.cen), glm::length(t.b - t.cen),
                      glm::length(t.c - t.cen)});
}

// Nearest render hit along (eye, unit d): a bounding-sphere reject
// (perpendicular distance of the centroid from the ray > radius => the tri
// cannot be hit) skips the ~99% of far tris before the full Moller-Trumbore.
double nearest_hit(const glm::dvec3& eye, const glm::dvec3& d,
                   const std::vector<AuditTri>& tris) {
    double best = 1e30;
    for (const AuditTri& tr : tris) {
        const glm::dvec3 oc = tr.cen - eye;
        const double along = glm::dot(oc, d);
        if (along < -tr.rad || along - tr.rad > best)
            continue;  // behind/beyond
        const double perp2 = glm::dot(oc, oc) - along * along;
        if (perp2 > tr.rad * tr.rad) continue;  // ray misses the bound sphere
        double t;
        if (ray_tri(eye, d, tr.a, tr.b, tr.c, t)) best = std::min(best, t);
    }
    return best;
}

// Collect greybox occluder triangles. `drop_trench` excludes the trench piece
// and `drop_breach_collars` excludes the arena's appended collar tris (indices
// >= the arena grid vertex count) — the two mutation levers.
std::vector<AuditTri> collect_occluders(const render::TunnelMeshData& data,
                                        bool drop_trench,
                                        bool drop_breach_collars) {
    const unsigned int grid_verts =
        static_cast<unsigned int>(render::kCavernLat + 1) *
        static_cast<unsigned int>(render::kCavernLong + 1);
    std::vector<AuditTri> tris;
    for (std::size_t pi = 0; pi < data.pieces.size(); ++pi) {
        const render::TunnelPiece& p = data.pieces[pi];
        if (drop_trench && p.kind == render::PieceKind::kTrench) continue;
        const bool is_arena = p.kind == render::PieceKind::kCavern;
        for (std::size_t t = 0; t + 2 < p.indices.size(); t += 3) {
            const unsigned int i0 = p.indices[t], i1 = p.indices[t + 1],
                               i2 = p.indices[t + 2];
            if (is_arena && drop_breach_collars &&
                (i0 >= grid_verts || i1 >= grid_verts || i2 >= grid_verts))
                continue;  // an appended breach-collar tri (mutation)
            const auto V = [&](unsigned int i) {
                return glm::dvec3(p.positions[3 * i + 0],
                                  p.positions[3 * i + 1],
                                  p.positions[3 * i + 2]);
            };
            AuditTri tr{V(i0), V(i1), V(i2)};
            finish_tri(tr);
            tr.kind = static_cast<int>(p.kind);  // T26 occluder attribution
            tr.pidx = static_cast<int>(pi);
            tris.push_back(tr);
        }
    }
    return tris;
}

// A cut disk on the surface (the mouth pit / bowl / trench opening): a
// direction is "through the opening" when its great-circle arc to the axis is
// under the cut radius + the terrain dropped-triangle margin (~1 cell). Outside
// a cut the surrounding terrain is front-facing solid ground (an occluder from
// above).
struct AuditCut {
    glm::dvec3 axis;
    double arc_r;  // cut radius + dropped-tri margin [arc m]
};
bool in_any_cut(const glm::dvec3& dir, double R,
                const std::vector<AuditCut>& cuts) {
    for (const AuditCut& c : cuts) {
        const double a =
            std::acos(glm::clamp(glm::dot(dir, c.axis), -1.0, 1.0));
        if (a * R < c.arc_r) return true;
    }
    return false;
}

// Does the sightline eye->target leak (see through solid rock to open sky)? The
// surrounding terrain (a front-facing sphere MINUS the cut disks) occludes any
// ray crossing INWARD through the surface outside a cut — so a grazing surface
// ray that would hit real ground is NOT a false leak; only a ray that enters
// through an OPEN cut, traverses unrendered solid rock, and reaches sky counts.
bool ray_leaks(const glm::dvec3& eye, const glm::dvec3& target,
               const std::vector<AuditTri>& tris, const world::TunnelNet& net,
               const world::HeightField& hf, const std::vector<AuditCut>& cuts,
               double R, glm::dvec3* leak_pt = nullptr) {
    const glm::dvec3 d = glm::normalize(target - eye);
    const double t_occ = nearest_hit(eye, d, tris);
    const double ds = 25.0, s_max = 34000.0;  // ~2R to reach the far side
    const glm::dvec3 e0 = eye / std::max(glm::length(eye), 1e-9);
    const double surfR = hf.radius_at(e0);  // uniform field => constant
    bool above_prev = glm::length(eye) > surfR;
    bool went_below = false;
    const double edotd = glm::dot(eye, d);
    for (double s = ds; s < s_max; s += ds) {
        if (s >= t_occ) return false;  // a render tri occludes first => sealed
        const glm::dvec3 p = eye + s * d;
        const double r = glm::length(p);
        const glm::dvec3 dir = p / std::max(r, 1e-9);
        const double surf = hf.radius_at(dir);
        const bool above = r > surf;
        // INWARD surface crossing outside a cut => the front-facing surrounding
        // terrain occludes it (backface-culled only when crossing OUTWARD from
        // below — the see-through direction we hunt).
        if (above_prev && !above && !in_any_cut(dir, R, cuts)) return false;
        if (!above) {
            went_below = true;
            if (!net.contains(p)) {
                // SOLID ROCK reached with no occluder in front. It will exit to
                // the far surface unobstructed iff no render tri sits before
                // the analytic far-side crossing (avoids marching all ~2R).
                const double disc =
                    edotd * edotd - (glm::dot(eye, eye) - surfR * surfR);
                const double s_far =
                    disc > 0.0 ? -edotd + std::sqrt(disc) : s + 1.0;
                const bool leaked =
                    t_occ > s_far - 1.0;  // unobstructed => SEE-THROUGH LEAK
                if (leaked && leak_pt)
                    *leak_pt = p;  // T26: the solid-rock point the sightline
                                   // reached with no occluder in front
                return leaked;
            }
        } else if (went_below) {
            return false;  // returned to open sky through open space => legit
        }
        above_prev = above;
    }
    return false;
}

// A full-sphere direction fan (naz x nel), returning target points at radius
// rad off the eye. Interior eyes use this — the rock criterion excludes legit
// exits.
std::vector<glm::dvec3> sphere_fan(const glm::dvec3& eye, double rad, int naz,
                                   int nel) {
    const double pi = 3.14159265358979323846;
    std::vector<glm::dvec3> t;
    for (int ie = 0; ie <= nel; ++ie) {
        const double el = -pi * 0.5 + pi * ie / nel;
        const double ce = std::cos(el), se = std::sin(el);
        for (int ia = 0; ia < naz; ++ia) {
            const double az = 2.0 * pi * ia / naz;
            t.push_back(eye + rad * glm::dvec3(ce * std::cos(az),
                                               ce * std::sin(az), se));
        }
    }
    return t;
}

// A disk fan aimed at a cut region: targets over the local tangent disk about
// `center_dir` at surface, out to `max_rad` (kept within the covering collar
// reach so a ray can only miss through an UNSEALED cut, not off into un-meshed
// surrounding terrain).
std::vector<glm::dvec3> cut_fan(const glm::dvec3& center_dir, double surf_r,
                                double max_rad, int nr, int naz) {
    const double pi = 3.14159265358979323846;
    const glm::dvec3 up = glm::normalize(center_dir);
    glm::dvec3 tu = up.z < 0.9 ? glm::dvec3(0, 0, 1) : glm::dvec3(1, 0, 0);
    tu = glm::normalize(tu - glm::dot(tu, up) * up);
    const glm::dvec3 tw = glm::normalize(glm::cross(up, tu));
    const glm::dvec3 c = up * surf_r;
    std::vector<glm::dvec3> t;
    t.push_back(c);
    for (int ir = 1; ir <= nr; ++ir) {
        const double rad = max_rad * ir / nr;
        for (int ia = 0; ia < naz; ++ia) {
            const double az = 2.0 * pi * ia / naz;
            t.push_back(c + rad * (std::cos(az) * tu + std::sin(az) * tw));
        }
    }
    return t;
}

// The FINAL T16 shipped geometry (arena 4200 = config/game.toml; portal ON).
world::TunnelParams t16_ship_tp() {
    world::TunnelParams tp = test_tp();
    tp.arena_a_m = 4200.0;   // the SHIPPED arena (where fly-12 saw the holes)
    tp.headframe_on = true;  // build the Errington portal frame (EXTERIOR)
    return tp;
}

// Every eye + its fan; returns total leaks (and fills a per-eye breakdown).
// mode: 0 = all eyes, 1 = INTERIOR only, 2 = SURFACE only (mutation subsets).
int audit_all_eyes(const world::TunnelNet& net, const world::HeightField& hf,
                   const std::vector<AuditTri>& tris,
                   const std::vector<render::Breach>& breaches,
                   std::vector<int>* per_eye = nullptr, int mode = 0) {
    const double surf_e = glm::length(net.spine.front().pos);
    const double surf_m = glm::length(net.spine.back().pos);
    const glm::dvec3 edir = glm::normalize(net.spine.front().pos);
    const glm::dvec3 mdir = glm::normalize(net.spine.back().pos);
    const double R = 15000.0;
    const double cell = render::terrain_cell_arc(200, 2, R);
    const double margin = 1.5 * cell;  // dropped-triangle ragged extent
    // The cut disks the surrounding terrain is missing (pit, bowl, trench).
    std::vector<AuditCut> cuts;
    cuts.push_back({edir, net.pit.bowl_r + margin});
    cuts.push_back({mdir, net.bowl.bowl_r + margin});
    for (int i = 0; i < world::TunnelNet::kTrenchSteps; ++i)
        if (net.trench[i].bowl_depth > 0.0)
            cuts.push_back({net.trench[i].axis, net.trench[i].bowl_r + margin});

    struct Eye {
        glm::dvec3 pos;
        std::vector<glm::dvec3> targets;
    };
    std::vector<Eye> eyes;
    std::size_t n_interior = 0;  // eyes[0..n_interior) are interior

    // INTERIOR eyes — full-sphere fans (24x12).
    eyes.push_back(
        {net.arena.center, sphere_fan(net.arena.center, 500, 24, 12)});
    for (const render::Breach& b : breaches) {
        const glm::dvec3 ax = glm::normalize(b.pos);
        glm::dvec3 e1 = b.into - glm::dot(b.into, ax) * ax;
        e1 = glm::length(e1) > 1e-9 ? glm::normalize(e1) : glm::dvec3(1, 0, 0);
        const glm::dvec3 e2 = glm::normalize(glm::cross(ax, e1));
        const glm::dvec3 ceye = b.pos + 400.0 * b.into;  // the T15 eye
        eyes.push_back({ceye, sphere_fan(ceye, 500, 24, 12)});
        const glm::dvec3 o1 = b.pos + 400.0 * b.into + 300.0 * e1;
        const glm::dvec3 o2 = b.pos + 400.0 * b.into + 300.0 * e2;
        eyes.push_back({o1, sphere_fan(o1, 500, 20, 10)});
        eyes.push_back({o2, sphere_fan(o2, 500, 20, 10)});
    }
    // A tube eye ~200 m before the Murray exit (walk arc back from the mouth).
    {
        const auto& sp = net.spine;
        double arc = 0.0;
        glm::dvec3 teye = sp.back().pos;
        for (std::size_t i = sp.size() - 1; i > 0; --i) {
            arc += glm::length(sp[i].pos - sp[i - 1].pos);
            if (arc >= 200.0) {
                teye = sp[i - 1].pos;
                break;
            }
        }
        eyes.push_back({teye, sphere_fan(teye, 500, 24, 12)});
    }
    // An eye inside the Errington pit (just below the mouth, in the open cut).
    {
        const glm::dvec3 peye = net.spine.front().pos - edir * 40.0;
        eyes.push_back({peye, sphere_fan(peye, 400, 20, 10)});
    }

    n_interior = eyes.size();
    // SURFACE eyes — aimed cut fans (kept within the collar reach). Errington
    // pit + trench, Murray bowl. 300 m + 1500 m straight up, plus 2 oblique.
    const double err_cut = net.pit.bowl_r;
    const double mur_cut = net.bowl.bowl_r;
    const auto add_surface = [&](const glm::dvec3& dir, double surf,
                                 double reach, double cut) {
        const glm::dvec3 c = dir * surf;
        const glm::dvec3 up = dir;
        glm::dvec3 tu = up.z < 0.9 ? glm::dvec3(0, 0, 1) : glm::dvec3(1, 0, 0);
        tu = glm::normalize(tu - glm::dot(tu, up) * up);
        const glm::dvec3 tw = glm::normalize(glm::cross(up, tu));
        std::vector<glm::dvec3> up_eyes = {c + up * 300.0, c + up * 1500.0,
                                           c + up * 700.0 + tu * 500.0,
                                           c + up * 700.0 + tw * 500.0};
        for (const glm::dvec3& e : up_eyes)
            eyes.push_back({e, cut_fan(dir, surf, cut + reach - 25.0, 6, 20)});
        (void)cut;
    };
    // The collar reach the covering walls actually span (mesh single source).
    const double err_reach =
        render::collar_reach(err_cut, 200, 2, 15000.0) - err_cut;
    const double mur_reach =
        render::collar_reach(mur_cut, 200, 2, 15000.0) - mur_cut;
    add_surface(edir, surf_e, err_reach, err_cut);
    add_surface(mdir, surf_m, mur_reach, mur_cut);
    // Trench: a surface eye over each live step aimed at its own cut.
    for (int i = 0; i < world::TunnelNet::kTrenchSteps; ++i) {
        const world::TunnelNet::Bowl& tb = net.trench[i];
        if (tb.bowl_depth <= 0.0) continue;
        const double reach =
            render::collar_reach(tb.bowl_r, 200, 2, 15000.0) - tb.bowl_r;
        const glm::dvec3 c = tb.axis * tb.surface_r;
        eyes.push_back(
            {c + tb.axis * 300.0,
             cut_fan(tb.axis, tb.surface_r, tb.bowl_r + reach - 25.0, 6, 20)});
        eyes.push_back(
            {c + tb.axis * 900.0,
             cut_fan(tb.axis, tb.surface_r, tb.bowl_r + reach - 25.0, 6, 20)});
    }

    int total = 0;
    for (std::size_t ei = 0; ei < eyes.size(); ++ei) {
        if (mode == 1 && ei >= n_interior) continue;  // interior only
        if (mode == 2 && ei < n_interior) continue;   // surface only
        const Eye& e = eyes[ei];
        int n = 0;
        for (const glm::dvec3& tg : e.targets)
            if (ray_leaks(e.pos, tg, tris, net, hf, cuts, R)) ++n;
        if (per_eye) per_eye->push_back(n);
        // Pass-tagged (T17 lesson: the un-tagged per-eye WARNs from the
        // MUTATION arms read as main-pass leaks and cost a long ghost chase).
        if (n > 0)
            WARN("audit mode " << mode << " eye " << ei
                               << " at |p|=" << glm::length(e.pos) << " leaks "
                               << n << " of " << e.targets.size());
        total += n;
    }
    return total;
}
}  // namespace

TEST_CASE(
    "T16 leak audit: multi-eye see-through probe over the FINAL geometry finds "
    "ZERO leaks (surface + interior + trench + breaches)") {
    const world::TunnelParams tp = t16_ship_tp();
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);
    // Route the FULL app collar/trench reach (grid-derived) into the mesh so
    // the probe audits the geometry the app actually ships.
    const double cell = render::terrain_cell_arc(200, 2, 15000.0);
    const double err_collar =
        render::collar_reach(net.pit.bowl_r, 200, 2, 15000.0);
    const double mur_collar =
        render::collar_reach(net.bowl.bowl_r, 200, 2, 15000.0);
    const render::TunnelMeshData data =
        render::build_tunnel_mesh(net, &hf, err_collar, mur_collar, cell);
    const std::vector<render::Breach> breaches = render::breach_points(net);
    REQUIRE(breaches.size() == 2);

    const std::vector<AuditTri> tris =
        collect_occluders(data, /*drop_trench=*/false,
                          /*drop_breach_collars=*/false);
    REQUIRE(tris.size() > 1000);

    std::vector<int> per_eye;
    const int leaks = audit_all_eyes(net, hf, tris, breaches, &per_eye);
    for (std::size_t i = 0; i < per_eye.size(); ++i)
        INFO("eye " << i << " leaks " << per_eye[i]);
    REQUIRE(leaks == 0);  // NO see-through anywhere in the audited geometry

    // MUTATION 1 — drop the entry-trench walls: the Errington approach ramp
    // sees straight through the open cut to space again. The probe must FAIL
    // loudly (an always-green probe is worthless — the
    // golden-pins-only-what-it- recorded lesson applies to probes too).
    const std::vector<AuditTri> no_trench =
        collect_occluders(data, /*drop_trench=*/true,
                          /*drop_breach_collars=*/false);
    const int leaks_no_trench =
        audit_all_eyes(net, hf, no_trench, breaches, nullptr, /*surface=*/2);
    INFO("leaks with the trench walls removed: " << leaks_no_trench);
    REQUIRE(leaks_no_trench > 0);  // the trench seal is load-bearing

    // MUTATION 2 — drop the breach collars: the arena breach margin sees
    // through to the surface again (the T15 class). The probe must FAIL.
    const std::vector<AuditTri> no_collar =
        collect_occluders(data, /*drop_trench=*/false,
                          /*drop_breach_collars=*/true);
    const int leaks_no_collar =
        audit_all_eyes(net, hf, no_collar, breaches, nullptr, /*interior=*/1);
    INFO("leaks with the breach collars removed: " << leaks_no_collar);
    REQUIRE(leaks_no_collar > 0);  // the breach collars are load-bearing
}

// T23 — THE MURRAY PIT-FLOOR/SLOT JUNCTION (fly round-16 tail, Chad: "I went
// straight down my mine and died from something invisible. I can see through
// the bottom where the murray pit meets murray tunnel"). The T16 audit's
// Murray surface fans have ~58 m target-ring spacing and its steep rays run
// 70-80 degrees — a facet-scale see-through sliver at the floor/slot junction
// threads only a near-VERTICAL sightline between them. This is the dedicated
// dense probe: one TRUE-VERTICAL ray per point of a fine annulus over the
// whole pit floor (each ray dives its own local up — the 90-degree line Chad
// flew). Zero leaks on the shipped mesh; the load-bearing arm rebuilds with
// tube_trim_split_depth=0 (the legacy whole-facet bore-swallow drop) and MUST
// leak — that arm IS the round-16 defect, kept alive as the mutation lever.
TEST_CASE(
    "T23 Murray floor junction: vertical-dive sightlines find zero "
    "see-through; the whole-facet trim arm leaks") {
    const world::TunnelParams tp = t16_ship_tp();
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);
    const double cell = render::terrain_cell_arc(200, 2, 15000.0);
    const double err_collar =
        render::collar_reach(net.pit.bowl_r, 200, 2, 15000.0);
    const double mur_collar =
        render::collar_reach(net.bowl.bowl_r, 200, 2, 15000.0);

    const double R = 15000.0;
    const double margin = 1.5 * cell;
    std::vector<AuditCut> cuts;
    cuts.push_back(
        {glm::normalize(net.spine.front().pos), net.pit.bowl_r + margin});
    cuts.push_back(
        {glm::normalize(net.spine.back().pos), net.bowl.bowl_r + margin});

    // The vertical fan: an annulus at the pit-floor plane from the slot out
    // past the funnel's floor edge (radial step 2.5 m — the suspected sliver
    // is ~4 m — by 64 azimuths), each target diven along ITS OWN local up.
    // The outer radius derives from the SAME floor-edge law the benched wall
    // builds with (red-team P2-2: a welded 210 silently un-covers the disk
    // edge — the measured overshoot site — on a bowl/bench retune).
    const world::TunnelNet::Bowl& bw = net.bowl;
    const int Nb23 = render::kBowlBenches;
    const double floor_edge23 =
        bw.bowl_r - (bw.bowl_r - bw.floor_r) * (Nb23 - 1) / Nb23;
    const glm::dvec3 axis = bw.axis;
    glm::dvec3 tu = glm::dvec3{1, 0, 0};
    tu = glm::normalize(tu - glm::dot(tu, axis) * axis);
    const glm::dvec3 tw = glm::normalize(glm::cross(axis, tu));
    const glm::dvec3 fc = axis * (bw.surface_r - bw.bowl_depth);
    const auto vertical_leaks = [&](const std::vector<AuditTri>& tris) {
        int n = 0;
        const double pi = 3.14159265358979323846;
        for (double rr = 30.0; rr <= 1.3 * floor_edge23; rr += 2.5)
            for (int ia = 0; ia < 64; ++ia) {
                const double az = 2.0 * pi * ia / 64.0;
                const glm::dvec3 tgt =
                    fc + rr * (std::cos(az) * tu + std::sin(az) * tw);
                const glm::dvec3 eye =
                    glm::normalize(tgt) * (bw.surface_r + 250.0);
                if (ray_leaks(eye, tgt, tris, net, hf, cuts, R)) ++n;
            }
        return n;
    };

    // Shipped mesh (subdividing tube trim): sealed.
    const render::TunnelMeshData data =
        render::build_tunnel_mesh(net, &hf, err_collar, mur_collar, cell);
    const std::vector<AuditTri> tris = collect_occluders(data, false, false);
    REQUIRE(tris.size() > 1000);
    const int leaks = vertical_leaks(tris);
    INFO("vertical-dive see-through leaks (shipped): " << leaks);
    REQUIRE(leaks == 0);

    // MUTATION — the legacy whole-facet trim: the junction sliver reopens.
    const render::TunnelMeshData data0 = render::build_tunnel_mesh(
        net, &hf, err_collar, mur_collar, cell, /*cut_swallow_trim=*/true,
        /*tube_trim_split_depth=*/0);
    const std::vector<AuditTri> tris0 = collect_occluders(data0, false, false);
    const int leaks0 = vertical_leaks(tris0);
    INFO("vertical-dive see-through leaks (whole-facet arm): " << leaks0);
    REQUIRE(leaks0 > 0);  // the subdividing trim is load-bearing
}

TEST_CASE(
    "T16 render-vs-collision: the arena breach render surface agrees with the "
    "collision opening (no fly-through wall, no wall over open air)") {
    // DEFECT 1 proof. From eyes on the tube spine crossing each breach and from
    // arena-interior eyes looking at the breach walls: wherever a ray's first
    // RENDER hit and its first COLLISION crossing both exist within 600 m they
    // agree within ~6 m; and NO ray from inside the tube toward the arena hits
    // a render tri while collision says open air for > 10 m behind it (the
    // fly-through wall). The render cut is now derived from the tube SDF
    // (collision truth), so the two representations coincide.
    const world::TunnelParams tp = t16_ship_tp();
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);
    const double cell = render::terrain_cell_arc(200, 2, 15000.0);
    const render::TunnelMeshData data = render::build_tunnel_mesh(
        net, &hf, render::collar_reach(net.pit.bowl_r, 200, 2, 15000.0),
        render::collar_reach(net.bowl.bowl_r, 200, 2, 15000.0), cell);
    const std::vector<render::Breach> breaches = render::breach_points(net);
    REQUIRE(breaches.size() == 2);

    // Occluders: the tube + the arena (grid + collars) — the breach surfaces.
    std::vector<AuditTri> tris;
    for (const render::TunnelPiece& p : data.pieces) {
        if (p.kind != render::PieceKind::kWall &&
            p.kind != render::PieceKind::kCavern)
            continue;
        for (std::size_t t = 0; t + 2 < p.indices.size(); t += 3) {
            const auto V = [&](std::size_t k) {
                const std::size_t i = p.indices[t + k];
                return glm::dvec3(p.positions[3 * i + 0],
                                  p.positions[3 * i + 1],
                                  p.positions[3 * i + 2]);
            };
            tris.push_back({V(0), V(1), V(2)});
        }
    }

    // First render hit distance along (eye,d).
    const auto render_hit = [&](const glm::dvec3& eye, const glm::dvec3& d) {
        double t_occ = 1e30;
        for (const AuditTri& tr : tris) {
            double t;
            if (ray_tri(eye, d, tr.a, tr.b, tr.c, t))
                t_occ = std::min(t_occ, t);
        }
        return t_occ;
    };
    // First collision boundary crossing along (eye,d) up to 600 m: the first s
    // where net.contains flips. Returns -1 if none.
    const auto collision_cross = [&](const glm::dvec3& eye,
                                     const glm::dvec3& d) {
        const bool in0 = net.contains(eye);
        const double ds = 2.0;
        for (double s = ds; s <= 600.0; s += ds)
            if (net.contains(eye + s * d) != in0) {
                // Refine.
                double lo = s - ds, hi = s;
                for (int it = 0; it < 30; ++it) {
                    const double mid = 0.5 * (lo + hi);
                    if (net.contains(eye + mid * d) != in0)
                        hi = mid;
                    else
                        lo = mid;
                }
                return 0.5 * (lo + hi);
            }
        return -1.0;
    };

    int agree_checks = 0, flythrough = 0;
    for (const render::Breach& b : breaches) {
        const glm::dvec3 ax = glm::normalize(b.pos);
        glm::dvec3 e1 = b.into - glm::dot(b.into, ax) * ax;
        e1 = glm::length(e1) > 1e-9 ? glm::normalize(e1) : glm::dvec3(1, 0, 0);
        const glm::dvec3 e2 = glm::normalize(glm::cross(ax, e1));
        // Two shot families: FLIGHT shots (from up-tube, head-on into the
        // arena along the bore — the pilot's approach cone, where a wall over
        // open air IS a fly-through) and VIEW shots (from inside the arena
        // looking back at the wall — grazing, used only for on-boundary
        // agreement). `is_flight` gates the fly-through test to flight
        // directions (a grazing view ray amplifies a sub-metre facet-vs-
        // ellipsoid offset into tens of along-ray metres — not a fly-through).
        std::vector<std::tuple<glm::dvec3, glm::dvec3, bool>> shots;
        const glm::dvec3 tube_eye = b.pos - 250.0 * b.into;
        for (int iu = -3; iu <= 3; ++iu)
            for (int iv = -3; iv <= 3; ++iv) {
                const glm::dvec3 d = glm::normalize(
                    b.into + 0.10 * (iu / 3.0) * e1 + 0.10 * (iv / 3.0) * e2);
                shots.push_back({tube_eye, d, true});
            }
        const glm::dvec3 in_eye = b.pos + 500.0 * b.into;
        for (int iu = -3; iu <= 3; ++iu)
            for (int iv = -3; iv <= 3; ++iv) {
                const glm::dvec3 target =
                    b.pos + 220.0 * (iu / 3.0) * e1 + 220.0 * (iv / 3.0) * e2;
                shots.push_back(
                    {in_eye, glm::normalize(target - in_eye), false});
            }

        for (const auto& sh : shots) {
            const glm::dvec3 eye = std::get<0>(sh), dd = std::get<1>(sh);
            const bool is_flight = std::get<2>(sh);
            const double tr = render_hit(eye, dd);
            const double tc = collision_cross(eye, dd);
            // AGREEMENT — the render surface never floats OUT in open rock
            // (sd > 0 would be a wall drawn in solid rock past the boundary).
            // At the render hit point the net SDF is <= ~0: walls sd~0, collars
            // the deliberate 5 m inward nudge, and near the oblique breach the
            // tube/collar throat legitimately dips up to ~one tube semi inside
            // (the T15-allowed poke), so only the OUTSIDE side is pinned here;
            // the fly-through test below owns the open-air side along flight
            // directions. Angle-independent (unlike along-ray |tr-tc|, which
            // grazing view rays amplify).
            if (tr < 600.0) {
                ++agree_checks;
                const glm::dvec3 phit = eye + tr * dd;
                const double sd = net.signed_distance(phit);
                INFO("render hit sd=" << sd << " (tr=" << tr << " tc=" << tc
                                      << ")");
                REQUIRE(sd <
                        3.0);  // render surface never sits out in open rock
            }
            // HEAD-ON AGREEMENT: for flight shots where BOTH the render hit and
            // the collision crossing exist within 600 m, the wall the pilot
            // flies at and the collision boundary coincide (no grazing
            // amplification along the bore).
            if (is_flight && tr < 600.0 && tc >= 0.0) {
                INFO("flight render " << tr << " collision " << tc);
                REQUIRE(std::abs(tr - tc) < 8.0);
            }
            // FLY-THROUGH (flight directions only): a render tri hit while the
            // eye is inside the net and the first collision exit is > 10 m PAST
            // it => a rendered wall standing in open bore air the pilot flies
            // through.
            if (is_flight && net.contains(eye) && tr < 600.0) {
                if (tc < 0.0 || tc - tr > 10.0) ++flythrough;
            }
        }
    }
    REQUIRE(agree_checks > 50);  // non-vacuous
    REQUIRE(flythrough == 0);    // NO rendered wall over open collision air

    // MUTATION — re-nudge a breach collar 30 m INTO the open arena (past the
    // 10 m tolerance): a rendered wall now floats in open bore air and the
    // fly-through count must go positive. Emulated by adding one collar tri
    // pulled 30 m along -into from the breach mouth into the arena.
    {
        std::vector<AuditTri> mut = tris;
        const render::Breach& b = breaches[0];
        const glm::dvec3 ax = glm::normalize(b.pos);
        glm::dvec3 e1 = b.into - glm::dot(b.into, ax) * ax;
        e1 = glm::length(e1) > 1e-9 ? glm::normalize(e1) : glm::dvec3(1, 0, 0);
        const glm::dvec3 e2 = glm::normalize(glm::cross(ax, e1));
        // A big tri straddling the bore, 60 m inside the arena (deep in open
        // air) — the exact defect the fixed collar avoids.
        const glm::dvec3 base = b.pos + 60.0 * b.into;
        mut.push_back({base + 200.0 * e1 + 200.0 * e2, base - 200.0 * e1,
                       base - 200.0 * e2});
        int fly = 0;
        const glm::dvec3 tube_eye = b.pos - 250.0 * b.into;
        for (int iu = -3; iu <= 3; ++iu)
            for (int iv = -3; iv <= 3; ++iv) {
                const glm::dvec3 d = glm::normalize(
                    b.into + 0.10 * (iu / 3.0) * e1 + 0.10 * (iv / 3.0) * e2);
                double t_occ = 1e30;
                for (const AuditTri& ttr : mut) {
                    double t;
                    if (ray_tri(tube_eye, d, ttr.a, ttr.b, ttr.c, t))
                        t_occ = std::min(t_occ, t);
                }
                const double tc = collision_cross(tube_eye, d);
                if (net.contains(tube_eye) && t_occ < 600.0 &&
                    (tc < 0.0 || tc - t_occ > 10.0))
                    ++fly;
            }
        INFO("fly-through count with a collar tri shoved 60 m into open air: "
             << fly);
        REQUIRE(fly > 0);  // the probe catches a wall over open air
    }
}

TEST_CASE(
    "T14: the cut hole encloses the true opening at the WIDE arena too "
    "(arena_a_m = 7350, the arm that pins stretch)") {
    // The stretch (oblique elongation) mutation arm (red-team P1 fold): at the
    // 7350 fixture canon the crossings are oblique enough that stretch:=1
    // leaves 3/24 (E) and 6/24 (M) true-rim directions OUTSIDE a round 275 m
    // hole (measured rim extents 288/309 m) — this arm FAILS under a dropped/
    // flattened stretch while the shipped-4200 arm alone would stay green.
    // Also covers the one-line arena_a_m=7350 revert the fly cards name.
    t14_rim_walk(test_tp());  // fixture canon IS 7350
}

// ----------------------------------------------------------------------------
// 7. T5a — THE FLOOR (mesh side): floor=0 bit-identical; floor-on truncates the
//    tube rings + adds a kFloor strip on the SDF boundary; no vert below floor.
// ----------------------------------------------------------------------------
namespace {
world::TunnelParams test_tp_floor(double fh) {
    world::TunnelParams tp = test_tp();
    tp.floor_height_m = fh;
    return tp;
}
}  // namespace

TEST_CASE("T5a mesh: floor_height_m=0 mesh buffers are BIT-IDENTICAL") {
    // The knob-off arm: two null-floor builds produce byte-identical vertex +
    // index + cue buffers AND the same piece count (no floor piece added).
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net =
        world::build_tunnel_net(test_tp_floor(0.0), &hf);
    const render::TunnelMeshData a = render::build_tunnel_mesh(net, &hf);
    const render::TunnelMeshData b = render::build_tunnel_mesh(net, &hf);

    REQUIRE(a.pieces.size() == b.pieces.size());
    REQUIRE(a.pieces.size() ==
            9);  // T16: +1 entry-trench piece  // T12: tube + 2 chambers + 2
                 // connectors + 2 collars + arena (no floor; shaft + core
                 // sealed away)
    for (std::size_t pi = 0; pi < a.pieces.size(); ++pi) {
        const render::TunnelPiece& pa = a.pieces[pi];
        const render::TunnelPiece& pb = b.pieces[pi];
        REQUIRE(pa.kind == pb.kind);
        REQUIRE(pa.positions.size() == pb.positions.size());
        REQUIRE(pa.indices.size() == pb.indices.size());
        for (std::size_t k = 0; k < pa.positions.size(); ++k)
            REQUIRE(pa.positions[k] == pb.positions[k]);  // exact float ==
        for (std::size_t k = 0; k < pa.indices.size(); ++k)
            REQUIRE(pa.indices[k] == pb.indices[k]);
        for (std::size_t k = 0; k < pa.cue.size(); ++k)
            REQUIRE(pa.cue[k] == pb.cue[k]);
        // No kFloor pieces exist in the OFF build.
        REQUIRE(pa.kind != render::PieceKind::kFloor);
    }
}

TEST_CASE("T5a mesh: floor ON adds a kFloor strip; its verts hug the SDF") {
    const double fh = 20.0;
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net =
        world::build_tunnel_net(test_tp_floor(fh), &hf);
    const render::TunnelMeshData data = render::build_tunnel_mesh(net, &hf);

    // Exactly one kFloor piece is present, and it has geometry.
    int floor_pieces = 0;
    const render::TunnelPiece* floor = nullptr;
    for (const render::TunnelPiece& p : data.pieces)
        if (p.kind == render::PieceKind::kFloor) {
            ++floor_pieces;
            floor = &p;
        }
    REQUIRE(floor_pieces == 1);
    REQUIRE(floor != nullptr);
    REQUIRE(!floor->indices.empty());
    REQUIRE(floor->indices.size() % 3 == 0);

    // Every floor vert is NEVER outside the net (sd <= 2 — a pilot never dies
    // on visible open air / passes through a visible floor; the death-air
    // rule). T10: the buried plateau (the old FLAT corridor) is SUPPRESSED from
    // the tube+floor mesh, so the emitted floor strip is ENTIRELY the two steep
    // descent/ascent ramps. A steep-RAMP strip vert sits well INSIDE the union
    // (a neighbour segment's lower floor plane owns the airspace just ahead as
    // the ramp continues down — real open space, never buried rock). So there
    // is no flat-corridor "hug sd=0" section to sample any more; the meaningful
    // pin is the never-outside bound + that the strip is genuinely graded.
    double min_sd = 1e30;
    for (std::size_t vi = 0; vi < floor->cue.size(); ++vi) {
        const glm::dvec3 v(floor->positions[3 * vi + 0],
                           floor->positions[3 * vi + 1],
                           floor->positions[3 * vi + 2]);
        const double sd = net.signed_distance(v);
        REQUIRE(sd <=
                2.0);  // never outside (a visible floor is never open air)
        min_sd = std::min(min_sd, sd);
    }
    // The strip is genuinely graded (no flat run survives suppression): its
    // deepest vert sits materially inside the union (the steep-ramp interior),
    // which the old flat-corridor mesh never produced. Confirm the emitted
    // floor stations carry NO flat neighbour-pair (every emitted floored
    // station's floor_r differs from the next by more than 1 m) — the plateau
    // that used to hug the boundary is gone.
    REQUIRE(min_sd < -5.0);  // the graded strip runs deep inside the union
    int emitted_floored = 0, flat_pairs = 0;
    int prev = -1;
    const std::vector<render::Breach> breaches = render::breach_points(net);
    for (std::size_t i = 0; i < net.spine.size(); ++i) {
        // Mirror build_tube's T14 suppression: skip the inside-arena stations.
        if (breaches.size() == 2 && i >= breaches[0].node_in &&
            i <= breaches[1].node_in)
            continue;
        if (net.spine[i].floor_r <= 0.0) continue;
        if (prev >= 0 &&
            std::abs(net.spine[i].floor_r - net.spine[prev].floor_r) < 1.0)
            ++flat_pairs;
        ++emitted_floored;
        prev = static_cast<int>(i);
    }
    REQUIRE(emitted_floored > 0);  // the ramps produced a floored strip
    REQUIRE(flat_pairs == 0);  // no flat corridor survives suppression (T10)
}

TEST_CASE("T5a mesh: no tube/floor/lamp vert sits below the floor plane") {
    // With the floor ON, the tube rings are chord-truncated so nothing pokes
    // below the visible floor, and every lamp stays above it. For each vert
    // find the nearest spine station and check its radial height >= floor_r -
    // eps.
    const double fh = 20.0;
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net =
        world::build_tunnel_net(test_tp_floor(fh), &hf);
    const render::TunnelMeshData data = render::build_tunnel_mesh(net, &hf);

    // Nearest-station floor_r for a world point (0 => floor off there).
    auto floor_r_at = [&](const glm::dvec3& p) {
        double best = 1e30;
        double fr = 0.0;
        for (const world::TunnelNet::Node& n : net.spine) {
            const double d2 = glm::dot(p - n.pos, p - n.pos);
            if (d2 < best) {
                best = d2;
                fr = n.floor_r;
            }
        }
        return fr;
    };

    // Tube (piece [0]) verts: none below its nearest floor plane (2 m slack for
    // transport curvature between stations).
    const render::TunnelPiece& tube = data.pieces[0];
    for (std::size_t vi = 0; vi < tube.cue.size(); ++vi) {
        const glm::dvec3 v(tube.positions[3 * vi + 0],
                           tube.positions[3 * vi + 1],
                           tube.positions[3 * vi + 2]);
        const double fr = floor_r_at(v);
        if (fr > 0.0) REQUIRE(glm::length(v) >= fr - 2.0);
    }

    // Lamps: every lamp stays strictly inside the net (which the floor bounds),
    // so none sits below the floor plane.
    const std::vector<render::TunnelLamp> lamps =
        render::place_tunnel_lamps(net);
    for (const render::TunnelLamp& L : lamps) {
        // T11.1: the arena ember/beacon families sit ON the arena surface far
        // above the deep floor (sd ~ 0) — the floor pin is for the near-floor
        // tube/chamber lamps. Skip them here (pinned on-arena by the T11.1
        // lamps leg). (This fixture is arena-OFF, so none are placed anyway.)
        // T16: the Murray THROAT beacons ring the bore OPENING (like the mouth
        // beacons, kBreachBeaconIntensity, already skipped) — their lower arc
        // legitimately dips below the floor plane (the floor truncates the bore
        // there), so they are exempt from the floor pin too (pinned on-bore by
        // the T16 throat-rings leg).
        if (L.intensity == render::kCavernEmberIntensity ||
            L.intensity == render::kBreachBeaconIntensity ||
            L.intensity == render::kMurrayThroatIntensity)
            continue;
        REQUIRE(net.signed_distance(L.pos) < -2.0);
        const double fr = floor_r_at(L.pos);
        if (fr > 0.0) REQUIRE(glm::length(L.pos) >= fr - 2.0);
    }
}

// ----------------------------------------------------------------------------
// 8. T5d — THE PORTAL-STROBE KILL TEST (real DEM + real app config).
//
// The COARSE mouth collar (2-ring band + flat bowl rim) chorded a single long
// radial line from the terrain rim down to the tube seam / into the pit, so its
// surface sliced ABOVE the fine (~59 m cell) terrain facets by tens of metres
// over the whole overlap annulus — the z-fighting "portal strobe" Chad saw from
// the bore approach. T5d rebuilds the collar/bowl-rim band as a terrain-hugging
// multi-ring annulus (each ring on radius_at(dir) − tuck, at cell density, with
// a 2x-finer azimuth so the azimuthal chords clear too).
//
// This test loads the REAL Sudbury DEM + the REAL app config (subdiv 200,
// tiles 2, relief 350, u_offset 0.806, floor ON) — the offline reproduction of
// the shipped path — and asserts that the ENTIRE collar/bowl-rim band surface
// (interior points of the triangles, not just ring verts) sits STRICTLY BELOW
// the rendered terrain facet (facet_radius_at, the exact fill_face facet the
// screen shows), with positive margin, across the terrain OVERLAP ANNULUS
// (arc in [cut_radius, collar_reach] — inside the cut hole the terrain is
// excavated, no strobe there). It also bounds the SINK (no visible gape).
//
// KILL VERIFICATION: reverting build_collar/build_bowl_wall to the old 2-ring /
// flat-rim band sends the poke to +1.29 m (Errington) / +21 m (Murray) — RED.
//
// COST: the only heavy step is the one-time stb DEM decode (~1 s); the sampling
// is a coarse barycentric grid over the collar/bowl triangles (not a raster).
// ----------------------------------------------------------------------------
namespace {

// Load the real Sudbury DEM into a HeightField exactly as render::load_planet
// does (dem16_unpack of the R/G channels; relief/u_offset from world.toml).
// Returns w==0 if the asset is missing (the test then skips loudly).
world::HeightField load_real_dem() {
    world::HeightField hf;
    const std::string path = std::string(SEADS_ASSET_DIR) + "/sudbury_dem.png";
    int w = 0, h = 0, comp = 0;
    unsigned char* px = stbi_load(path.c_str(), &w, &h, &comp, 3);  // force RGB
    if (px == nullptr) return hf;  // w stays 0 => caller skips
    hf.w = w;
    hf.h = h;
    hf.R = kR;
    hf.relief_scale = 350.0;  // config/world.toml [ground] relief_scale_m
    hf.u_offset = 0.806;      // config/world.toml [planet] u_offset
    hf.px.resize(static_cast<std::size_t>(w) * h);
    for (std::size_t i = 0; i < hf.px.size(); ++i)
        hf.px[i] = world::dem16_unpack(px[i * 3 + 0], px[i * 3 + 1]);
    stbi_image_free(px);
    return hf;
}

// The real app tunnel params (config/game.toml [tunnel]) — floor ON, as
// shipped.
world::TunnelParams app_tp() {
    world::TunnelParams tp = test_tp();  // shared canon dials
    tp.floor_height_m = 20.0;            // the floor is ON in the app
    return tp;
}

// Max poke ABOVE / max sink BELOW the rendered facet over the interior of every
// triangle of `piece`, restricted to the terrain overlap annulus arc in
// [cut, outer] about `axis`. arc < cut is inside the excavated cut hole (no
// terrain); arc > outer is past the collar. `out_max_poke` (>0 => above the
// facet) and `out_max_sink` (>0 => below) are written.
void band_poke_below(const render::TunnelPiece& piece, const glm::dvec3& axis,
                     double cut, double outer, const world::HeightField& hf,
                     int subdiv, int tiles, double& out_max_poke,
                     double& out_max_sink) {
    out_max_poke = -1e30;
    out_max_sink = 0.0;
    const int SUB = 5;  // barycentric subsamples per triangle edge
    for (std::size_t t = 0; t + 2 < piece.indices.size(); t += 3) {
        glm::dvec3 P[3];
        for (int k = 0; k < 3; ++k) {
            const unsigned short idx = piece.indices[t + k];
            P[k] = glm::dvec3(piece.positions[3 * idx + 0],
                              piece.positions[3 * idx + 1],
                              piece.positions[3 * idx + 2]);
        }
        for (int a = 0; a <= SUB; ++a)
            for (int b = 0; a + b <= SUB; ++b) {
                const double wa = double(a) / SUB, wb = double(b) / SUB;
                const double wc = 1.0 - wa - wb;
                const glm::dvec3 p = wa * P[0] + wb * P[1] + wc * P[2];
                const glm::dvec3 dir = glm::normalize(p);
                const double arc =
                    std::acos(glm::clamp(glm::dot(dir, axis), -1.0, 1.0)) * kR;
                if (arc < cut || arc > outer + 5.0) continue;  // outside band
                const double poke =
                    glm::length(p) -
                    render::facet_radius_at(hf, dir, subdiv, tiles);
                out_max_poke = std::max(out_max_poke, poke);
                if (-poke > out_max_sink) out_max_sink = -poke;
            }
    }
}

}  // namespace

TEST_CASE(
    "T5d KILL: the terrain-hugging collar band sits strictly below the "
    "rendered terrain (real DEM)") {
    const world::HeightField hf = load_real_dem();
    if (hf.w == 0) {
        WARN("sudbury_dem.png missing — T5d kill test skipped");
        return;
    }

    // The app path (draw.cpp): subdiv=200, tiles=2; collar reach + terrain cell
    // arc derived through the SAME helpers the app calls.
    const int subdiv = 200, tiles = 2;
    const world::TunnelParams tp = app_tp();
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);

    const double err_cut = world::errington_cut_radius(
        net, render::kMouthCutFactor);  // T6c pit rim
    const double mur_cut = world::murray_cut_radius(tp.bowl_radius_m);
    const double err_collar = render::collar_reach(err_cut, subdiv, tiles, kR);
    const double mur_collar = render::collar_reach(mur_cut, subdiv, tiles, kR);
    const double cell_arc = render::terrain_cell_arc(subdiv, tiles, kR);

    const render::TunnelMeshData data =
        render::build_tunnel_mesh(net, &hf, err_collar, mur_collar, cell_arc);

    // Identify the two is_collar pieces by their nearest mouth (index-robust:
    // the floor adds a piece, so the collar is not always [6]/[7]).
    const render::TunnelPiece* err_collar_p = nullptr;
    const render::TunnelPiece* mur_bowl_p = nullptr;
    for (const render::TunnelPiece& p : data.pieces) {
        if (!p.is_collar || p.cue.empty()) continue;
        // Mean direction of the piece's verts -> nearest mouth.
        glm::dvec3 mean(0.0);
        for (std::size_t vi = 0; vi < p.cue.size(); ++vi)
            mean += glm::normalize(glm::dvec3(p.positions[3 * vi + 0],
                                              p.positions[3 * vi + 1],
                                              p.positions[3 * vi + 2]));
        mean = glm::normalize(mean);
        const double de = glm::dot(mean, world::kTunnelMouthErrington);
        const double dm = glm::dot(mean, world::kTunnelMouthMurray);
        if (de > dm)
            err_collar_p = &p;
        else
            mur_bowl_p = &p;
    }
    REQUIRE(err_collar_p != nullptr);
    REQUIRE(mur_bowl_p != nullptr);

    // Vertex-count sanity (trivially small vs the planet mesh's ~480k
    // tris/face, and well under the u16 index cap 65535 assert_u16 enforces).
    // T17 grew both mouth pieces: the Errington cut sleeve + the Murray floor
    // disk + the mouth adapter rings + the trims' subdivided border leaves
    // (duplicated leaf verts). T19 raised the two MOUTH collars' border split
    // depth 3 -> 4 (kMouthSplitDepth) to shrink the pale flaps, quadrupling
    // those leaves: the Errington collar (three overlapping trench cuts) grows
    // ~2300 -> 8625; the Murray bowl stays small (~649 on this DEM). Budget
    // bumped 6000 -> 12000 to match the depth-4 leaf duplication, honestly.
    // T23 bumped it again 12000 -> 30000: the bore/throat trim now ALSO
    // subdivides (the Murray junction see-through fix), so both mouth pieces
    // carry depth-4 leaves along the whole adit/slot boundary (measured:
    // Errington 21750, Murray ~14k on this DEM). Ceiling stays honest vs the
    // u16 index cap (65535, assert_u16) and the ~480k-tri planet faces.
    REQUIRE(err_collar_p->cue.size() < 30000u);
    REQUIRE(mur_bowl_p->cue.size() < 30000u);

    // --- ERRINGTON collar: strictly below the facet across [err_cut,
    // err_collar].
    {
        double max_poke = 0, max_sink = 0;
        // Inner bound err_cut + 2: the T17 cut SLEEVE stands exactly AT the
        // rim arc and descends the full pit depth by design — the band under
        // test is the terrain-hugging annulus OUTSIDE the rim.
        band_poke_below(*err_collar_p, world::kTunnelMouthErrington,
                        err_cut + 2.0, err_collar, hf, subdiv, tiles, max_poke,
                        max_sink);
        // STRICTLY below with positive margin (the strobe was +1.29 m).
        REQUIRE(max_poke < -0.2);  // >= 0.2 m of clearance under the terrain
        // No visible gape from the bore approach (a few metres at most).
        REQUIRE(max_sink < 5.0);
    }

    // --- MURRAY bowl rim: strictly below the facet across [mur_cut,
    // mur_collar].
    //     (Below the rim the analytic pit cone is legitimately deep — excluded
    //     by the arc < mur_cut filter; the rim band is the strobe surface.)
    {
        double max_poke = 0, max_sink = 0;
        band_poke_below(*mur_bowl_p, world::kTunnelMouthMurray, mur_cut,
                        mur_collar, hf, subdiv, tiles, max_poke, max_sink);
        REQUIRE(max_poke < -0.2);  // strobe was +21 m; now strictly below
        REQUIRE(max_sink < 8.0);   // the rim band; deeper cone is excluded
    }
}

// ============================================================================
// T12 — THE MESH-INTEGRITY VERIFIER (docs/tunnel_staging.md T12; Chad's round-9
// fly: "Look for holes in the mesh. The tunnel has weird ramps and cutoffs of
// the mesh we should mend"). The structural pin of "look for holes in the
// mesh": build the full canon mesh and check EDGE-MANIFOLDNESS per opaque
// piece. Every interior edge is shared by exactly 2 triangles; a boundary edge
// (shared by 1) is allowed ONLY at a DESIGNED opening (the two mouth faces, the
// arena breach holes, and piece-to-piece seams). A stray boundary edge in the
// middle of a surface (a poked-out triangle, a cutoff, an unmended ramp) is a
// HOLE the pilot can see the earth through — this leg REQUIREs there are none.
//
// The mesh is a GREYBOX (degenerate pole quads, coincident seam verts), so the
// edge map keys on ROUNDED vertex POSITION (merging coincident floats) and
// drops zero-length (degenerate) edges. A "non-manifold" edge (>2 triangles) is
// ALWAYS a defect; a "boundary" edge (==1 triangle) is checked against the
// per-kind allowed-opening predicate.
namespace {

// A position key: round to the nearest millimetre so coincident seam/end-ring
// floats (shared exactly by construction) collapse to one vertex.
struct PosKey {
    long long x, y, z;
    bool operator<(const PosKey& o) const {
        if (x != o.x) return x < o.x;
        if (y != o.y) return y < o.y;
        return z < o.z;
    }
};
inline PosKey pos_key(const glm::dvec3& v) {
    return {static_cast<long long>(std::llround(v.x * 1000.0)),
            static_cast<long long>(std::llround(v.y * 1000.0)),
            static_cast<long long>(std::llround(v.z * 1000.0))};
}
inline glm::dvec3 mesh_vert(const render::TunnelPiece& p, unsigned int i) {
    return glm::dvec3(p.positions[3 * i + 0], p.positions[3 * i + 1],
                      p.positions[3 * i + 2]);
}

// Per-piece edge -> triangle-count map (keyed on the sorted position-key pair),
// plus a representative midpoint per edge (world). Degenerate (zero-length)
// edges are skipped (pole quads collapse to lines in a greybox lat/long cap).
struct EdgeStat {
    int tri_count = 0;
    glm::dvec3 mid{0.0};
};
inline std::map<std::pair<PosKey, PosKey>, EdgeStat> mesh_piece_edges(
    const render::TunnelPiece& p) {
    std::map<std::pair<PosKey, PosKey>, EdgeStat> edges;
    for (std::size_t t = 0; t + 2 < p.indices.size(); t += 3) {
        const unsigned int idx[3] = {p.indices[t + 0], p.indices[t + 1],
                                     p.indices[t + 2]};
        const glm::dvec3 v[3] = {mesh_vert(p, idx[0]), mesh_vert(p, idx[1]),
                                 mesh_vert(p, idx[2])};
        // Skip DEGENERATE (near-zero-area) triangles: the greybox lat/long caps
        // keep FULL rings at the poles, so the pole quads collapse to zero-area
        // slivers (a rendering no-op). They carry no surface — their edges are
        // spurious and would falsely read non-manifold at the merged pole. A
        // real hole (a MISSING real triangle) still shows as a boundary edge on
        // the surrounding real triangles, so this cannot hide a hole.
        const double area2 = glm::length(glm::cross(v[1] - v[0], v[2] - v[0]));
        if (area2 < 1.0) continue;  // < 0.5 m^2 triangle — a degenerate sliver
        const PosKey k[3] = {pos_key(v[0]), pos_key(v[1]), pos_key(v[2])};
        for (int e = 0; e < 3; ++e) {
            const int a = e, b = (e + 1) % 3;
            const bool same = !(k[a] < k[b]) && !(k[b] < k[a]);
            if (same) continue;  // degenerate (pole cap) edge — skip
            std::pair<PosKey, PosKey> key = k[a] < k[b]
                                                ? std::make_pair(k[a], k[b])
                                                : std::make_pair(k[b], k[a]);
            EdgeStat& es = edges[key];
            es.tri_count++;
            es.mid = 0.5 * (v[a] + v[b]);
        }
    }
    return edges;
}

}  // namespace

TEST_CASE(
    "T12 mesh integrity: every opaque piece is edge-manifold; boundary edges "
    "only at designed openings") {
    const world::TunnelParams tp = test_tp();  // canon (floor OFF, arena ON)
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);
    const render::TunnelMeshData data = render::build_tunnel_mesh(net, &hf);
    REQUIRE(net.arena_on);
    REQUIRE(data.pieces.size() == 9);  // T16: +1 entry-trench piece

    using render::PieceKind;
    const glm::dvec3 front = net.spine.front().pos;
    const glm::dvec3 back = net.spine.back().pos;
    // T14: the TRUE breaches (single-source with the mesh's own suppression +
    // hole cut) — the arena's two designed openings, and the tube's designed
    // break points. A boundary edge near a breach POSITION is legal (the last
    // emitted ring sits within ~one node spacing of the crossing).
    const std::vector<render::Breach> breaches = render::breach_points(net);
    REQUIRE(breaches.size() == 2);
    const auto near_breach = [&](const glm::dvec3& m, double r) {
        for (const render::Breach& b : breaches)
            if (glm::length(m - b.pos) < r) return true;
        return false;
    };

    int total_boundary = 0, total_stray = 0;
    for (std::size_t pi = 0; pi < data.pieces.size(); ++pi) {
        const render::TunnelPiece& p = data.pieces[pi];
        const auto edges = mesh_piece_edges(p);

        // (1) NON-MANIFOLD: no edge shared by >2 triangles. ALWAYS a defect
        //     (a fold, a duplicated facet). Universal across every piece.
        for (const auto& entry : edges) {
            INFO("piece " << pi << " kind " << static_cast<int>(p.kind)
                          << " non-manifold edge tri_count "
                          << entry.second.tri_count);
            REQUIRE(entry.second.tri_count <= 2);
        }

        // (2) BOUNDARY edges (tri_count == 1) must lie on a DESIGNED opening
        // for
        //     this piece kind. A stray boundary edge = a hole/cutoff.
        for (const auto& entry : edges) {
            if (entry.second.tri_count != 1) continue;
            ++total_boundary;
            const glm::dvec3 m = entry.second.mid;
            bool designed = false;
            switch (p.kind) {
                case PieceKind::kChamber:
                    designed = false;  // closed ellipsoid — no boundary edges
                    break;
                case PieceKind::kWall: {
                    // The tube: end rings at the two mouths + the breach
                    // suppression gap. Connectors: end rings at a bore node and
                    // a chamber center (a designed seam into the fat pocket).
                    const bool at_front = glm::length(m - front) < 400.0;
                    const bool at_back = glm::length(m - back) < 600.0;
                    const bool at_breach = near_breach(m, 450.0);
                    bool at_pocket = false;
                    for (int c = 0; c < 2; ++c)
                        if (glm::length(m - net.chamber[c].center) < 400.0)
                            at_pocket = true;
                    designed = at_front || at_back || at_breach || at_pocket;
                    break;
                }
                case PieceKind::kCollar:
                    // Mouth pit / bowl wall: an annulus — its boundary edges
                    // are the outer terrain rim + the inner tube seam by
                    // construction.
                    designed = true;
                    break;
                case PieceKind::kCavern: {
                    // The arena: the two breach holes are the ONLY designed
                    // openings (T12 sealed the floor). A boundary edge NOT at a
                    // breach hole (and not at the degenerate lat/long POLE
                    // caps) is a HOLE the pilot sees earth through. The two
                    // poles (±u_long, the arena ceiling apex + the sealed floor
                    // point) are closed by degenerate sliver triangles the area
                    // filter drops, so the first real ring reads as a boundary
                    // there — a designed closed cap, not a hole. Recognize a
                    // pole-cap boundary edge by its small ANGULAR distance to
                    // the ±u_long axis (the ellipsoid's own frame): the
                    // floor/ceiling pole caps subtend a tiny cone (the first
                    // lat ring at pi/kCavernLat off the pole).
                    const glm::dvec3 loc = m - net.arena.center;
                    const double cos_lat =
                        std::abs(glm::dot(glm::normalize(loc),
                                          net.arena.u_long));  // 1 == on axis
                    const double pole_cap_cos = std::cos(
                        2.5 * 3.14159265358979323846 / render::kCavernLat);
                    if (cos_lat > pole_cap_cos) designed = true;  // pole cap
                    // Breach holes (the bore openings, T14 predicate). The
                    // any-corner quad-skip (build_arena) drops a facet if ANY
                    // of its 4 corners is inside the hole, so the actual cut
                    // rim extends up to one facet DIAGONAL past the ellipse; a
                    // boundary-edge MIDPOINT sits a further half-cell out.
                    // Allow a 2-facet-arc pad on the shared predicate.
                    const double facet_pad =
                        2.0 * (3.14159265358979323846 / render::kCavernLat) *
                        glm::length(front);
                    const glm::dvec3 wdir = glm::normalize(m);
                    if (render::breach_hole_hit(breaches, wdir, facet_pad))
                        designed = true;
                    break;
                }
                case PieceKind::kFloor:
                    designed = true;  // strip perimeter (OFF in canon)
                    break;
                case PieceKind::kPortal:
                    designed =
                        true;  // closed portal boxes — designed boundary edges
                    break;
                case PieceKind::kTrench:
                    designed = true;  // T16 open-cut trench: rim/floor boundary
                                      // edges are designed (an open excavation)
                    break;
            }
            if (!designed) {
                ++total_stray;
                WARN("piece "
                     << pi << " kind " << static_cast<int>(p.kind)
                     << " STRAY boundary edge at |m|=" << glm::length(m) << " ("
                     << m.x << "," << m.y << "," << m.z << ")");
            }
            REQUIRE(designed);
        }
    }
    // Non-vacuous: the mesh HAS designed boundary edges (the mouths +
    // breaches), so the leg actually exercised the predicate.
    REQUIRE(total_boundary > 0);
    REQUIRE(total_stray == 0);
}

TEST_CASE(
    "T13 chambers off: no chamber/connector pieces; mesh stays edge-manifold") {
    // Chad's spec S1: the pump pockets are gated off. The OFF arm must (a)
    // contain NO chamber or connector pieces (the piece list matches the
    // volume), and (b) stay edge-manifold with boundary edges only at designed
    // openings — the SAME verifier as the on-arm, but ARM-AWARE: with chambers
    // off there is NO designed chamber-seam opening, so the tube wall must have
    // no pocket-seam boundary edge (the rule set drops the at_pocket
    // allowance).
    world::TunnelParams tp = test_tp();  // canon (arena on)
    tp.chambers_on = false;              // T13: pump pockets gated off
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);
    const render::TunnelMeshData data = render::build_tunnel_mesh(net, &hf);
    REQUIRE(net.arena_on);

    using render::PieceKind;
    // (a) NO chamber pieces, NO connector-only walls. The off-arm piece list is
    // tube + 2 collars + arena = 4 (the on-arm is 8: + 2 chambers + 2
    // connectors). Count kinds.
    int chambers = 0, walls = 0, collars = 0, caverns = 0, trenches = 0;
    for (const render::TunnelPiece& p : data.pieces) {
        switch (p.kind) {
            case PieceKind::kChamber:
                ++chambers;
                break;
            case PieceKind::kWall:
                ++walls;
                break;
            case PieceKind::kCollar:
                ++collars;
                break;
            case PieceKind::kCavern:
                ++caverns;
                break;
            case PieceKind::kFloor:
                break;
            case PieceKind::kTrench:
                ++trenches;
                break;  // T16: the entry-trench walls (own piece)
            case PieceKind::kPortal:
                break;  // visual-only surface piece; not counted here
        }
    }
    REQUIRE(chambers == 0);  // no lit side rooms
    REQUIRE(walls == 1);     // ONLY the tube (no connectors)
    REQUIRE(collars == 2);   // both mouth collars
    REQUIRE(caverns == 1);   // the arena
    REQUIRE(trenches == 1);  // T16: the entry-trench walls (trench ON in canon)
    REQUIRE(data.pieces.size() == 5);  // +1 trench vs the pre-T16 4

    // (b) Arm-aware edge-manifold pass. Same predicate as the on-arm verifier,
    // minus the at_pocket allowance (no designed chamber seam exists).
    const glm::dvec3 front = net.spine.front().pos;
    const glm::dvec3 back = net.spine.back().pos;
    // T14: the true breaches (single-source with the mesh cut).
    const std::vector<render::Breach> breaches = render::breach_points(net);
    REQUIRE(breaches.size() == 2);
    const auto near_breach = [&](const glm::dvec3& m, double r) {
        for (const render::Breach& b : breaches)
            if (glm::length(m - b.pos) < r) return true;
        return false;
    };

    int total_boundary = 0, total_stray = 0;
    for (std::size_t pi = 0; pi < data.pieces.size(); ++pi) {
        const render::TunnelPiece& p = data.pieces[pi];
        const auto edges = mesh_piece_edges(p);
        for (const auto& entry : edges) {
            INFO("piece " << pi << " kind " << static_cast<int>(p.kind)
                          << " non-manifold tri_count "
                          << entry.second.tri_count);
            REQUIRE(entry.second.tri_count <= 2);
        }
        for (const auto& entry : edges) {
            if (entry.second.tri_count != 1) continue;
            ++total_boundary;
            const glm::dvec3 m = entry.second.mid;
            bool designed = false;
            switch (p.kind) {
                case PieceKind::kChamber:
                    designed = false;  // no chamber pieces exist off-arm
                    break;
                case PieceKind::kWall: {
                    // The tube ONLY (no connectors off-arm): end rings at the
                    // two mouths + the breach suppression gap. NO at_pocket
                    // allowance — a chamber-seam boundary edge would be a HOLE.
                    const bool at_front = glm::length(m - front) < 400.0;
                    const bool at_back = glm::length(m - back) < 600.0;
                    const bool at_breach = near_breach(m, 450.0);
                    designed = at_front || at_back || at_breach;
                    break;
                }
                case PieceKind::kCollar:
                    designed = true;  // annulus rim + tube seam
                    break;
                case PieceKind::kCavern: {
                    const glm::dvec3 loc = m - net.arena.center;
                    const double cos_lat = std::abs(
                        glm::dot(glm::normalize(loc), net.arena.u_long));
                    const double pole_cap_cos = std::cos(
                        2.5 * 3.14159265358979323846 / render::kCavernLat);
                    if (cos_lat > pole_cap_cos) designed = true;
                    const double facet_pad =
                        2.0 * (3.14159265358979323846 / render::kCavernLat) *
                        glm::length(front);
                    const glm::dvec3 wdir = glm::normalize(m);
                    if (render::breach_hole_hit(breaches, wdir, facet_pad))
                        designed = true;
                    break;
                }
                case PieceKind::kFloor:
                    designed = true;
                    break;
                case PieceKind::kPortal:
                    designed =
                        true;  // closed portal boxes — designed boundary edges
                    break;
                case PieceKind::kTrench:
                    designed = true;  // T16 open-cut trench: rim/floor boundary
                                      // edges are designed (an open excavation)
                    break;
            }
            if (!designed) {
                ++total_stray;
                WARN("piece "
                     << pi << " kind " << static_cast<int>(p.kind)
                     << " STRAY boundary edge at |m|=" << glm::length(m));
            }
            REQUIRE(designed);
        }
    }
    REQUIRE(total_boundary > 0);
    REQUIRE(total_stray == 0);
}

TEST_CASE(
    "T12 mesh integrity mutant: a poked-out tube triangle is caught as a "
    "hole") {
    // The verifier must FAIL if a real triangle is missing (a hole). Simulate
    // the mutant's EFFECT: take the canon tube piece and DELETE one triangle
    // from a mid-tube ring (well away from any mouth/breach opening), then run
    // the same edge-manifold pass. The deleted triangle's three edges each drop
    // from 2 triangles to 1 (or vanish), so a BOUNDARY edge appears in the
    // middle of the tube wall — NOT at any designed opening. This pins that the
    // verifier's boundary-edge check is a live, non-vacuous hole detector.
    const world::TunnelParams tp = test_tp();
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);
    render::TunnelMeshData data = render::build_tunnel_mesh(net, &hf);

    render::TunnelPiece& tube = data.pieces[0];
    REQUIRE(tube.kind == render::PieceKind::kWall);
    // Baseline: the tube has NO stray boundary edge in its interior (every
    // interior edge is shared by 2 triangles; boundary edges are only at the
    // mouth/breach rings). Count the tube's mid-body boundary edges FAR from
    // the two mouths + the breach radius.
    const glm::dvec3 front = net.spine.front().pos;
    const glm::dvec3 back = net.spine.back().pos;
    // T14: designed tube openings are at the two mouths + the TRUE breaches.
    const std::vector<render::Breach> breaches = render::breach_points(net);
    REQUIRE(breaches.size() == 2);
    const auto breach_dist = [&](const glm::dvec3& m) {
        double d = 1e30;
        for (const render::Breach& b : breaches)
            d = std::min(d, glm::length(m - b.pos));
        return d;
    };
    auto interior_boundary_count = [&](const render::TunnelPiece& p) {
        const auto edges = mesh_piece_edges(p);
        int n = 0;
        for (const auto& e : edges) {
            if (e.second.tri_count != 1) continue;
            const glm::dvec3 m = e.second.mid;
            const bool near_open = glm::length(m - front) < 400.0 ||
                                   glm::length(m - back) < 600.0 ||
                                   breach_dist(m) < 450.0;
            if (!near_open) ++n;  // a mid-body boundary edge (a hole)
        }
        return n;
    };
    REQUIRE(interior_boundary_count(tube) ==
            0);  // premise: canon tube is whole

    // POKE: delete one triangle from a mid-tube ring (a triangle whose verts
    // sit well past 600 m from either mouth and away from the breaches).
    bool poked = false;
    for (std::size_t t = 0; t + 2 < tube.indices.size() && !poked; t += 3) {
        const glm::dvec3 v0(tube.positions[3 * tube.indices[t] + 0],
                            tube.positions[3 * tube.indices[t] + 1],
                            tube.positions[3 * tube.indices[t] + 2]);
        if (glm::length(v0 - front) > 800.0 && glm::length(v0 - back) > 800.0 &&
            breach_dist(v0) > 800.0) {
            tube.indices.erase(tube.indices.begin() + t,
                               tube.indices.begin() + t + 3);
            poked = true;
        }
    }
    REQUIRE(poked);  // we actually removed a mid-body triangle
    // The poke leaves a HOLE: a stray boundary edge in the tube mid-body.
    REQUIRE(interior_boundary_count(tube) > 0);  // MUTANT CAUGHT
}

// ============================================================================
// T12 P2 — THE MANIFOLD VERIFIER ON THE SHIPPED FLOOR-ON GEOMETRY.
// The leg above runs floor_height_m = 0.0 (the fixture default), but game.toml
// SHIPS floor_height_m = 20.0: the kFloor strip chord-truncates the tube's
// lower wall and ADDS a floor-strip piece, all unseen by the floor-OFF leg.
// This leg runs the IDENTICAL edge-manifold pass on the floor-ON canon and
// enumerates the ADDITIONAL designed boundary loops the floor legitimately
// introduces — the kFloor strip perimeter (its two long chord RAILS + the
// transverse CAP edges where the strip breaks at the two mouths and the two
// breach suppressions). It does NOT blanket-allow kFloor: a stray transverse
// edge in the middle of the ribbon (a hole in the floor) is a REAL finding.
//
// HONEST FLOOR-ON RESULT (probed at authoring): the tube's own boundary-edge
// count is UNCHANGED by the truncation (the below-floor ring verts slide UP to
// the floor plane and merge, producing degenerate slivers the area filter drops
// — no new tube-wall strays), so the kWall predicate above covers the tube as
// is; the ONLY new pieces are the closed kFloor ribbon. The mesh PASSES.
namespace {
// The lateral offset of a world point from its nearest spine station's floor
// chord axis (== cross(up, tangent)). A kFloor RAIL vert sits ~half a chord
// off-axis; a TRANSVERSE (left<->right) edge midpoint sits ~on-axis.
double floor_lateral(const world::TunnelNet& net, const glm::dvec3& v) {
    double best = 1e30;
    std::size_t bi = 0;
    for (std::size_t i = 0; i < net.spine.size(); ++i) {
        const double d2 = glm::dot(v - net.spine[i].pos, v - net.spine[i].pos);
        if (d2 < best) {
            best = d2;
            bi = i;
        }
    }
    const glm::dvec3 c = net.spine[bi].pos;
    const glm::dvec3 up = glm::normalize(c);
    const glm::dvec3 t =
        bi + 1 < net.spine.size()
            ? glm::normalize(net.spine[bi + 1].pos - net.spine[bi].pos)
            : glm::normalize(net.spine[bi].pos - net.spine[bi - 1].pos);
    glm::dvec3 lat = glm::cross(up, t);
    if (glm::length(lat) < 1e-9) return 0.0;
    lat = glm::normalize(lat);
    return glm::dot(v - c, lat);
}
}  // namespace

TEST_CASE(
    "T12 P2: floor-ON canon is edge-manifold; boundary edges only at designed "
    "openings (incl. the kFloor strip perimeter)") {
    world::TunnelParams tp = test_tp();
    tp.floor_height_m = 20.0;  // game.toml ships 20.0 (this leg's whole point)
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);
    const render::TunnelMeshData data = render::build_tunnel_mesh(net, &hf);
    REQUIRE(net.arena_on);
    REQUIRE(net.floor_height > 0.0);    // premise: floor is LIVE
    REQUIRE(data.pieces.size() == 10);  // +1 kFloor, +1 entry-trench

    using render::PieceKind;
    const glm::dvec3 front = net.spine.front().pos;
    const glm::dvec3 back = net.spine.back().pos;
    // T14: the true breaches (single-source with the mesh cut).
    const std::vector<render::Breach> breaches = render::breach_points(net);
    REQUIRE(breaches.size() == 2);
    const auto near_breach = [&](const glm::dvec3& m, double r) {
        for (const render::Breach& b : breaches)
            if (glm::length(m - b.pos) < r) return true;
        return false;
    };

    int total_boundary = 0, total_stray = 0, floor_transverse = 0;
    int floor_pieces = 0;
    for (std::size_t pi = 0; pi < data.pieces.size(); ++pi) {
        const render::TunnelPiece& p = data.pieces[pi];
        const auto edges = mesh_piece_edges(p);
        if (p.kind == PieceKind::kFloor) ++floor_pieces;

        // (1) NON-MANIFOLD is ALWAYS a defect, every piece (floor included).
        for (const auto& entry : edges) {
            INFO("piece " << pi << " kind " << static_cast<int>(p.kind)
                          << " non-manifold edge tri_count "
                          << entry.second.tri_count);
            REQUIRE(entry.second.tri_count <= 2);
        }

        // (2) BOUNDARY edges must lie on a DESIGNED opening for this kind.
        for (const auto& entry : edges) {
            if (entry.second.tri_count != 1) continue;
            ++total_boundary;
            const glm::dvec3 m = entry.second.mid;
            bool designed = false;
            switch (p.kind) {
                case PieceKind::kChamber:
                    designed = false;  // closed ellipsoid
                    break;
                case PieceKind::kWall: {
                    const bool at_front = glm::length(m - front) < 400.0;
                    const bool at_back = glm::length(m - back) < 600.0;
                    const bool at_breach = near_breach(m, 450.0);
                    bool at_pocket = false;
                    for (int c = 0; c < 2; ++c)
                        if (glm::length(m - net.chamber[c].center) < 400.0)
                            at_pocket = true;
                    designed = at_front || at_back || at_breach || at_pocket;
                    break;
                }
                case PieceKind::kCollar:
                    designed = true;  // annular skirt (rim + tube seam)
                    break;
                case PieceKind::kFloor: {
                    // The floor strip is a RIBBON of quads. Its ONLY legitimate
                    // boundary edges are (a) the two long chord RAILS (a vert
                    // ~half a chord off the floor axis) and (b) the TRANSVERSE
                    // caps where a strip SEGMENT ends — the two mouths + the
                    // two breach-suppression breaks. A transverse edge
                    // (midpoint ~on the floor axis) at an INTERIOR station is a
                    // HOLE in the floor (a missing quad). Derived from the
                    // geometry, NOT a blanket allow: only rails + break-located
                    // caps pass.
                    const double lat = floor_lateral(net, m);
                    if (std::abs(lat) > 20.0) {
                        designed = true;  // a chord rail — always designed
                    } else {
                        ++floor_transverse;  // a strip-cap candidate
                        const bool at_front = glm::length(m - front) < 400.0;
                        const bool at_back = glm::length(m - back) < 600.0;
                        const bool at_breach = near_breach(m, 450.0);
                        designed = at_front || at_back || at_breach;
                    }
                    break;
                }
                case PieceKind::kCavern: {
                    const glm::dvec3 loc = m - net.arena.center;
                    const double cos_lat = std::abs(
                        glm::dot(glm::normalize(loc), net.arena.u_long));
                    const double pole_cap_cos = std::cos(
                        2.5 * 3.14159265358979323846 / render::kCavernLat);
                    if (cos_lat > pole_cap_cos) designed = true;
                    const double facet_pad =
                        2.0 * (3.14159265358979323846 / render::kCavernLat) *
                        glm::length(front);
                    const glm::dvec3 wdir = glm::normalize(m);
                    if (render::breach_hole_hit(breaches, wdir, facet_pad))
                        designed = true;
                    break;
                }
                case PieceKind::kPortal:
                    designed =
                        true;  // closed portal boxes — designed boundary edges
                    break;
                case PieceKind::kTrench:
                    designed = true;  // T16 open-cut trench: rim/floor boundary
                                      // edges are designed (an open excavation)
                    break;
            }
            if (!designed) {
                ++total_stray;
                WARN("piece "
                     << pi << " kind " << static_cast<int>(p.kind)
                     << " STRAY boundary edge at |m|=" << glm::length(m) << " ("
                     << m.x << "," << m.y << "," << m.z << ")");
            }
            REQUIRE(designed);
        }
    }
    // Non-vacuous: exactly one floor strip, it HAS boundary edges (rails +
    // caps), and the transverse-cap classification actually fired (the strip
    // breaks at the two mouths + two breaches, so a few transverse caps exist).
    REQUIRE(floor_pieces == 1);
    REQUIRE(total_boundary > 0);
    REQUIRE(floor_transverse > 0);  // the strip has real caps (predicate ran)
    REQUIRE(total_stray == 0);      // HONEST: the shipped floor-ON mesh passes
}

TEST_CASE("T12 P2 mutant: a poked-out FLOOR-strip quad is caught as a hole") {
    // The verifier must FAIL if a floor quad is missing (a hole in the runway).
    // Delete an interior floor quad (away from the mouths + breaches) and
    // re-run the floor predicate: the deleted quad's transverse edges drop to
    // boundary at an INTERIOR station (not a designed cap) => a stray. Pins
    // that the floor predicate is a live, non-vacuous hole detector (not a
    // blanket allow).
    world::TunnelParams tp = test_tp();
    tp.floor_height_m = 20.0;
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);
    render::TunnelMeshData data = render::build_tunnel_mesh(net, &hf);

    const glm::dvec3 front = net.spine.front().pos;
    const glm::dvec3 back = net.spine.back().pos;
    // T14: designed floor caps sit at the mouths + the TRUE breaches.
    const std::vector<render::Breach> breaches = render::breach_points(net);
    REQUIRE(breaches.size() == 2);
    const auto breach_dist = [&](const glm::dvec3& m) {
        double d = 1e30;
        for (const render::Breach& b : breaches)
            d = std::min(d, glm::length(m - b.pos));
        return d;
    };

    render::TunnelPiece* floor = nullptr;
    for (render::TunnelPiece& p : data.pieces)
        if (p.kind == render::PieceKind::kFloor) floor = &p;
    REQUIRE(floor != nullptr);

    // Count INTERIOR transverse floor boundary edges (a hole signature): a
    // near-axis boundary edge far from every designed break.
    auto interior_transverse = [&](const render::TunnelPiece& p) {
        const auto edges = mesh_piece_edges(p);
        int n = 0;
        for (const auto& e : edges) {
            if (e.second.tri_count != 1) continue;
            const glm::dvec3 m = e.second.mid;
            if (std::abs(floor_lateral(net, m)) > 20.0) continue;  // a rail
            const bool near_break = glm::length(m - front) < 400.0 ||
                                    glm::length(m - back) < 600.0 ||
                                    breach_dist(m) < 450.0;
            if (!near_break) ++n;  // a hole-signature transverse edge
        }
        return n;
    };
    REQUIRE(interior_transverse(*floor) == 0);  // premise: canon floor is whole

    // POKE: delete one interior floor quad (both its triangles: 6 indices),
    // chosen well away from the mouths + breaches.
    bool poked = false;
    for (std::size_t t = 0; t + 5 < floor->indices.size() && !poked; t += 6) {
        const glm::dvec3 v0(floor->positions[3 * floor->indices[t] + 0],
                            floor->positions[3 * floor->indices[t] + 1],
                            floor->positions[3 * floor->indices[t] + 2]);
        if (glm::length(v0 - front) > 800.0 && glm::length(v0 - back) > 800.0 &&
            breach_dist(v0) > 800.0) {
            floor->indices.erase(floor->indices.begin() + t,
                                 floor->indices.begin() + t + 6);
            poked = true;
        }
    }
    REQUIRE(poked);
    REQUIRE(interior_transverse(*floor) > 0);  // MUTANT CAUGHT (a runway hole)
}

// ----------------------------------------------------------------------------
// 8. T16 ENTRANCE-ART — THE ERRINGTON PORTAL FRAME (framed adit). Chad's
//    2026-07-21 ruling replaced the T13 sinking headframe with a proper mine
//    PORTAL FRAME: thick posts + lintel + wing walls + a canopy hood framing
//    the bore mouth. VISUAL-ONLY (no SDF). Every member is a CLOSED manifold
//    box. ALL frame geometry sits OUTSIDE the bore ellipse so the flyable
//    opening is never narrowed. Gated on headframe_on (the repurposed
//    surface-structure flag).
// ----------------------------------------------------------------------------
namespace {

// The canon params with the surface-structure flag ON (the fixture default is
// OFF, so every existing leg stays bit-identical). This now gates the PORTAL.
world::TunnelParams portal_tp() {
    world::TunnelParams tp = test_tp();
    tp.headframe_on = true;
    tp.headframe_h_m = 80.0;
    return tp;
}

// The Errington bore mouth frame, recomputed INDEPENDENTLY from the net's spine
// (the test's own copy — not the builder's). Mirrors errington_mouth_frame().
struct MouthFrameRef {
    glm::dvec3 c, horiz, vert;
    double rw, rh;
};
MouthFrameRef mouth_frame_ref(const world::TunnelNet& net) {
    MouthFrameRef f;
    f.c = net.spine.front().pos;
    const glm::dvec3 up = glm::normalize(f.c);
    const glm::dvec3 tan =
        glm::normalize(net.spine[1].pos - net.spine.front().pos);
    f.horiz = glm::normalize(glm::cross(tan, up));
    f.vert = glm::normalize(glm::cross(f.horiz, tan));
    f.rw = net.tube_width;
    f.rh = net.tube_height;
    return f;
}

}  // namespace

TEST_CASE(
    "T16 portal: ON adds a kPortal piece framing the Errington bore, clear of "
    "the opening") {
    const world::TunnelParams tp = portal_tp();
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);
    const render::TunnelMeshData data = render::build_tunnel_mesh(net, &hf);

    // Exactly ONE kPortal piece, with verts.
    int pn = 0;
    const render::TunnelPiece* pp = nullptr;
    for (const render::TunnelPiece& p : data.pieces)
        if (p.kind == render::PieceKind::kPortal) {
            ++pn;
            pp = &p;
        }
    REQUIRE(pn == 1);
    REQUIRE(pp != nullptr);
    REQUIRE(pp->cue.size() > 0);
    REQUIRE(pp->indices.size() % 3 == 0);
    // T19: the kPortal piece now carries BOTH the pale portal frame AND the
    // 1931 ruin ensemble (folded in on the same headframe_on gate). The FRAME
    // members bake cue 1.0; the RUIN members bake cue = kRuinVal/kPortalVal so
    // the FS draws them darker (silvered timber vs pale concrete). Every vert
    // is one of the two families, and BOTH are present (non-vacuous).
    const float ruin_cue = render::kRuinVal / render::kPortalVal;
    int frame_v = 0, ruin_v = 0;
    for (float c : pp->cue) {
        if (c == 1.0f)
            ++frame_v;
        else if (c == ruin_cue)
            ++ruin_v;
        else
            FAIL("unexpected portal cue " << c);
    }
    REQUIRE(frame_v > 0);  // the pale portal frame
    REQUIRE(ruin_v > 0);   // the darker ruin ensemble

    const MouthFrameRef f = mouth_frame_ref(net);

    // (a) BOUNDS: every portal vertex sits within a DERIVED sphere of the bore
    //     mouth. Config-relative — every term is a constant the geometry uses.
    //     T19: the piece now also spans the ruin ensemble on the pit FLANK, so
    //     the bound is max(frame reach, ruin reach). The ruin reach derives
    //     from the pit: the far dump flank (kRuinReachFrac * pit rim) + the pit
    //     depth (the flank surface sits bowl_depth radially above the floor
    //     mouth f.c)
    //     + the headframe height + a margin for mound spans / sheave beams.
    const double frame_bound =
        (render::kPortalCanopyReach + render::kPortalWingSpread + 2.0) * f.rw +
        2.0 * f.rh + 60.0;
    const double ruin_bound = render::kRuinReachFrac * net.pit.bowl_r +
                              net.pit.bowl_depth + net.headframe_h + 90.0;
    const double bound = std::max(frame_bound, ruin_bound);
    for (std::size_t vi = 0; vi < pp->cue.size(); ++vi) {
        const glm::dvec3 v(pp->positions[3 * vi + 0], pp->positions[3 * vi + 1],
                           pp->positions[3 * vi + 2]);
        REQUIRE(glm::length(v - f.c) <= bound);
    }

    // (b) CLEARANCE: the frame NEVER narrows the flyable bore. Every portal
    //     vertex, projected onto the mouth plane (horiz, vert), lies OUTSIDE
    //     the bore ellipse (x/rw)^2 + (y/rh)^2 >= 1 with a clear margin. A
    //     mutation that set the post/lintel inset negative (clr < 0) would pull
    //     frame verts INTO the ellipse and fail here.
    double min_ell = 1e18;
    for (std::size_t vi = 0; vi < pp->cue.size(); ++vi) {
        const glm::dvec3 v(pp->positions[3 * vi + 0], pp->positions[3 * vi + 1],
                           pp->positions[3 * vi + 2]);
        const double x = glm::dot(v - f.c, f.horiz);
        const double y = glm::dot(v - f.c, f.vert);
        const double e = (x / f.rw) * (x / f.rw) + (y / f.rh) * (y / f.rh);
        min_ell = std::min(min_ell, e);
    }
    // Strictly outside the bore, with the kPortalClearance margin (well above
    // 1).
    REQUIRE(min_ell >= 1.05);
}

TEST_CASE("T16 portal: OFF => no portal piece; rest is BIT-IDENTICAL") {
    // The structural-off pin: headframe_on == false yields ZERO portal pieces
    // AND the remaining mesh is byte-for-byte identical to the on-arm's
    // non-portal pieces. Mutation (drop the headframe_on gate): the piece would
    // appear in the off build too.
    const world::HeightField hf = uniform_field(300.0);

    world::TunnelParams off = test_tp();  // fixture default: portal OFF
    REQUIRE(off.headframe_on == false);
    const world::TunnelNet net_off = world::build_tunnel_net(off, &hf);
    const render::TunnelMeshData data_off =
        render::build_tunnel_mesh(net_off, &hf);

    world::TunnelParams on = portal_tp();
    const world::TunnelNet net_on = world::build_tunnel_net(on, &hf);
    const render::TunnelMeshData data_on =
        render::build_tunnel_mesh(net_on, &hf);

    // OFF arm: no portal piece at all.
    for (const render::TunnelPiece& p : data_off.pieces)
        REQUIRE(p.kind != render::PieceKind::kPortal);

    // ON arm: exactly one extra piece (the portal), appended LAST.
    REQUIRE(data_on.pieces.size() == data_off.pieces.size() + 1);
    REQUIRE(data_on.pieces.back().kind == render::PieceKind::kPortal);

    // Every non-portal piece is BIT-IDENTICAL between the two builds (the
    // portal perturbs nothing — it is appended after all body pieces).
    for (std::size_t pi = 0; pi < data_off.pieces.size(); ++pi) {
        const render::TunnelPiece& a = data_off.pieces[pi];
        const render::TunnelPiece& b = data_on.pieces[pi];
        REQUIRE(a.kind == b.kind);
        REQUIRE(a.positions.size() == b.positions.size());
        REQUIRE(a.indices.size() == b.indices.size());
        REQUIRE(a.cue.size() == b.cue.size());
        for (std::size_t i = 0; i < a.positions.size(); ++i)
            REQUIRE(a.positions[i] == b.positions[i]);  // exact float ==
        for (std::size_t i = 0; i < a.indices.size(); ++i)
            REQUIRE(a.indices[i] == b.indices[i]);
        for (std::size_t i = 0; i < a.cue.size(); ++i)
            REQUIRE(a.cue[i] == b.cue[i]);
    }
}

TEST_CASE("T16 portal: the kPortal piece is edge-manifold, closed boxes") {
    // Every portal member is a CLOSED box, so the kPortal piece has NO
    // non-manifold edges AND NO boundary edges (fully closed geometry). Same
    // edge-manifold pass the T12 verifier uses.
    const world::TunnelParams tp = portal_tp();
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);
    const render::TunnelMeshData data = render::build_tunnel_mesh(net, &hf);

    const render::TunnelPiece* pp = nullptr;
    for (const render::TunnelPiece& p : data.pieces)
        if (p.kind == render::PieceKind::kPortal) pp = &p;
    REQUIRE(pp != nullptr);

    const auto edges = mesh_piece_edges(*pp);
    REQUIRE(!edges.empty());  // non-vacuous: the piece has real geometry
    int boundary = 0, non_manifold = 0;
    for (const auto& e : edges) {
        if (e.second.tri_count > 2) ++non_manifold;
        if (e.second.tri_count == 1) ++boundary;
    }
    REQUIRE(non_manifold == 0);  // no folds / duplicated facets
    REQUIRE(boundary == 0);      // every box is CLOSED — no boundary edge
}

TEST_CASE("T16 portal: lintel beacons ride the frame, in the bright tier") {
    // Round-17 CONTRACT FLIP: the portal LINTEL (and canopy) box was removed
    // from build_portal (it hung over the trench-side excavation — the flown
    // "terrain cover sheet" slab; attribution comment at the build_portal
    // site), and the beacons RIDE THE LINTEL (red-team P2-4: beacons at the
    // removed lintel's top would float in open air with nothing beneath).
    // This case now pins the ABSENCE: no bright-tier lamp above the bore top
    // near the Errington mouth, portal on or off — the exact signature a
    // lintel-beacon resurrection (without a terrain-grounded lintel under it)
    // would trip. The pre-round-17 arm REQUIREd lintel == kPortalLintelBeacons.
    const world::TunnelParams tp = portal_tp();
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);
    const std::vector<render::TunnelLamp> lamps =
        render::place_tunnel_lamps(net);
    const MouthFrameRef f = mouth_frame_ref(net);

    int lintel = 0;
    for (const render::TunnelLamp& L : lamps) {
        // A portal-lintel beacon: bright tier, near the Errington mouth, ABOVE
        // the bore top (vert component > the vertical semi).
        if (L.intensity != render::kBreachBeaconIntensity) continue;
        if (glm::length(L.pos - f.c) > 4.0 * f.rw) continue;  // near the mouth
        const double y = glm::dot(L.pos - f.c, f.vert);
        if (y > f.rh) ++lintel;
    }
    REQUIRE(lintel == 0);

    // OFF arm: no portal beacons above the bore top near the mouth.
    world::TunnelParams off = test_tp();
    const world::TunnelNet net_off = world::build_tunnel_net(off, &hf);
    const MouthFrameRef fo = mouth_frame_ref(net_off);
    int lintel_off = 0;
    for (const render::TunnelLamp& L : render::place_tunnel_lamps(net_off)) {
        if (L.intensity != render::kBreachBeaconIntensity) continue;
        if (glm::length(L.pos - fo.c) > 4.0 * fo.rw) continue;
        if (glm::dot(L.pos - fo.c, fo.vert) > fo.rh) ++lintel_off;
    }
    REQUIRE(lintel_off == 0);
}

// ==================== T16 — visibility culling (perf) =======================
// docs/tunnel_staging.md T16. The greybox + lamps were drawn in full every
// frame regardless of the camera. Pieces split into two visibility classes and
// a pure per-frame gate decides which draw. These pin the classification
// (arena/tube INTERIOR, pit/bowl/portal EXTERIOR) and the gate logic.
TEST_CASE(
    "T16 cull: arena is INTERIOR, pit/bowl collars + portal are EXTERIOR") {
    // Direct classification of every PieceKind (the source of truth).
    CHECK(render::piece_is_exterior(render::PieceKind::kCollar));
    CHECK(render::piece_is_exterior(render::PieceKind::kPortal));
    CHECK(render::piece_is_exterior(render::PieceKind::kTrench));  // T16
    CHECK_FALSE(render::piece_is_exterior(render::PieceKind::kCavern));
    CHECK_FALSE(render::piece_is_exterior(render::PieceKind::kWall));
    CHECK_FALSE(render::piece_is_exterior(render::PieceKind::kFloor));
    CHECK_FALSE(render::piece_is_exterior(render::PieceKind::kChamber));

    // And on the real built pieces: the arena (kCavern) reads INTERIOR, the
    // mouth pit + bowl walls (kCollar) + the portal frame (kPortal) EXTERIOR.
    world::TunnelParams tp = test_tp();
    tp.headframe_on = true;  // build the portal frame too (EXTERIOR)
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);
    const render::TunnelMeshData data = render::build_tunnel_mesh(net, &hf);

    int cavern = 0, collar = 0, portal = 0;
    for (const render::TunnelPiece& p : data.pieces) {
        if (p.kind == render::PieceKind::kCavern) {
            ++cavern;
            CHECK_FALSE(render::piece_is_exterior(p.kind));
        } else if (p.kind == render::PieceKind::kCollar) {
            ++collar;
            CHECK(render::piece_is_exterior(p.kind));
        } else if (p.kind == render::PieceKind::kPortal) {
            ++portal;
            CHECK(render::piece_is_exterior(p.kind));
        }
    }
    REQUIRE(cavern == 1);  // the single arena piece exists (non-vacuous)
    REQUIRE(collar >= 2);  // Errington pit + Murray bowl walls
    REQUIRE(portal == 1);  // the surface portal frame
}

TEST_CASE("T16 cull: the draw gate reads exterior/interior from the position") {
    // A synthetic net frame: centre at 11 km up +Y, radius 6 km, mouths 5 km
    // off-centre on either side (a stand-in for the real geometry).
    const glm::dvec3 center{0.0, 11000.0, 0.0};
    const double radius = 6000.0;
    const glm::dvec3 mouth_e{-5000.0, 15000.0, 0.0};
    const glm::dvec3 mouth_m{5000.0, 15000.0, 0.0};

    // (a) FAR away (Ramsey Lake): beyond the exterior draw distance from the
    //     sphere AND far from both mouths, not underground => draw NOTHING.
    {
        const glm::dvec3 far = center + glm::dvec3{0.0, 40000.0, 0.0};
        const render::TunnelDrawGate g = render::tunnel_draw_gate(
            far, center, radius, mouth_e, mouth_m, /*underground=*/false);
        CHECK_FALSE(g.exterior);
        CHECK_FALSE(g.interior);
    }
    // (b) Within the exterior range but not underground and > 2.5 km from a
    //     mouth => EXTERIOR on, INTERIOR off (surface landmarks, no corridor).
    {
        const glm::dvec3 near_surf = center + glm::dvec3{0.0, 10000.0, 0.0};
        const render::TunnelDrawGate g = render::tunnel_draw_gate(
            near_surf, center, radius, mouth_e, mouth_m, /*underground=*/false);
        CHECK(g.exterior);
        CHECK_FALSE(g.interior);
    }
    // (c) Diving a mouth (< 2.5 km of mouth_e), not (yet) underground =>
    //     INTERIOR on so the corridor is present as you enter.
    {
        const glm::dvec3 at_mouth = mouth_e + glm::dvec3{0.0, 1000.0, 0.0};
        const render::TunnelDrawGate g = render::tunnel_draw_gate(
            at_mouth, center, radius, mouth_e, mouth_m, /*underground=*/false);
        CHECK(g.exterior);
        CHECK(g.interior);
    }
    // (d) Underground (inside the net) => INTERIOR on regardless of mouth
    //     distance; a point inside the sphere is trivially within exterior
    //     range.
    {
        const render::TunnelDrawGate g = render::tunnel_draw_gate(
            center, center, radius, mouth_e, mouth_m, /*underground=*/true);
        CHECK(g.exterior);
        CHECK(g.interior);
    }
}

// ---------------------------------------------------------------------------
// T17 — BOTH ENTRANCES ARE OPEN (fly-13, Chad: "both entrances are occluded I
// could not fly into either"). The T16 leak audit rewards SEALING and nothing
// rewarded OPENING: pushing leaks to zero walled off the two designed
// entrances (the full-circle trench drapes roofed the overlapping Errington
// approach cuts; the inclined Murray bore's crown — above the pit floor,
// inside the bowl's open collision cylinder — plugged the floor mouth as a
// black dome, plainly visible yet unremarked in the committed
// murray_bowl_t16.png certification smoke). These are the DUAL pins: the
// entry paths a pilot actually flies must be free of greybox render
// occluders, always-on. The load-bearing arm rebuilds the mesh with the T17
// cut-swallowed trim OFF (the test-only lever) and requires the SAME paths
// BLOCKED — kills a trim-deletion mutant and proves the open-pins are
// non-vacuous. (Terrain is not in the occluder set: these pins are about the
// tunnel greybox lying across its own designed openings.)
TEST_CASE(
    "T17 entrances open: the Murray pit dive and the Errington approach read "
    "clear of greybox occluders, and the trim-off arm is blocked") {
    const world::TunnelParams tp = t16_ship_tp();
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);
    const double cell = render::terrain_cell_arc(200, 2, 15000.0);
    const double err_collar =
        render::collar_reach(net.pit.bowl_r, 200, 2, 15000.0);
    const double mur_collar =
        render::collar_reach(net.bowl.bowl_r, 200, 2, 15000.0);
    const render::TunnelMeshData trimmed = render::build_tunnel_mesh(
        net, &hf, err_collar, mur_collar, cell, /*cut_swallow_trim=*/true);
    const render::TunnelMeshData raw = render::build_tunnel_mesh(
        net, &hf, err_collar, mur_collar, cell, /*cut_swallow_trim=*/false);
    const std::vector<AuditTri> occ_on =
        collect_occluders(trimmed, false, false);
    const std::vector<AuditTri> occ_off = collect_occluders(raw, false, false);
    REQUIRE(occ_on.size() > 1000);
    // The trim reshapes the mesh (drops swallowed facets, subdivides border
    // facets into leaves — the count can go EITHER way; only equality would
    // mean the trim was a no-op).
    REQUIRE(occ_on.size() != occ_off.size());

    // Blocked = the nearest greybox hit lands before the segment end, with a
    // 2 m end margin so an exact-seam graze at the target never counts.
    const auto blocked = [](const std::vector<AuditTri>& tris,
                            const glm::dvec3& a, const glm::dvec3& b) {
        const double len = glm::length(b - a);
        const glm::dvec3 d = (b - a) / len;
        return nearest_hit(a, d, tris) < len - 2.0;
    };

    // ---- MURRAY: the pit dive. The mouth ring frame exactly as build_tube
    // makes it (the independent-oracle recompute): local-up-aligned ellipse
    // about the back spine node.
    const std::size_t n = net.spine.size();
    REQUIRE(n >= 2);
    const glm::dvec3 mm = net.spine[n - 1].pos;
    const glm::dvec3 t_m =
        glm::normalize(net.spine[n - 1].pos - net.spine[n - 2].pos);
    const glm::dvec3 up_m = net.spine[n - 1].up;
    glm::dvec3 horiz_m = glm::cross(t_m, up_m);
    REQUIRE(glm::length(horiz_m) > 1e-9);
    horiz_m = glm::normalize(horiz_m);
    const glm::dvec3 vert_m = glm::normalize(glm::cross(horiz_m, t_m));
    // Dive targets: the ring centre + the UPPER-half mouth-ring points pulled
    // to 60% (above the bowl floor plane, clear of the funnel seam). A
    // vertical dive onto each must reach it — pre-trim the bore crown dome
    // stood exactly in that air.
    std::vector<glm::dvec3> dive_targets{mm};
    for (int k = 0; k < 12; ++k) {
        const double a = 2.0 * 3.14159265358979323846 * k / 12.0;
        const glm::dvec3 q =
            mm + 0.6 * (net.tube_width * std::cos(a) * horiz_m +
                        net.tube_height * std::sin(a) * vert_m);
        if (glm::dot(q - mm, up_m) >= 0.0) dive_targets.push_back(q);
    }
    REQUIRE(dive_targets.size() >= 5);  // centre + a real upper arc
    int dives_open = 0, dives_blocked_off = 0;
    for (const glm::dvec3& q : dive_targets) {
        const glm::dvec3 hi = q + up_m * 500.0;
        const glm::dvec3 lo = q + up_m * 6.0;
        if (!blocked(occ_on, hi, lo)) ++dives_open;
        if (blocked(occ_off, hi, lo)) ++dives_blocked_off;
    }
    INFO("murray dives open (trim on): "
         << dives_open << "/" << dive_targets.size()
         << ", blocked with trim off: " << dives_blocked_off);
    REQUIRE(dives_open == static_cast<int>(dive_targets.size()));
    // The LOAD-BEARING arm for the crown trim: a dive onto the bore
    // centreline one node in (the stretch whose crown stands ABOVE the pit
    // floor inside the open bowl cylinder — the black dome). Untrimmed it is
    // roofed by kWall; trimmed it reads clear into the bore.
    {
        const glm::dvec3 c1 = net.spine[n - 2].pos;
        int crown_open = 0, crown_blocked_off = 0;
        for (int k = -1; k <= 1; ++k) {
            const glm::dvec3 q = c1 + horiz_m * (30.0 * k);
            const glm::dvec3 hi = q + up_m * 500.0;
            const glm::dvec3 lo = q + up_m * 6.0;
            if (!blocked(occ_on, hi, lo)) ++crown_open;
            if (blocked(occ_off, hi, lo)) ++crown_blocked_off;
        }
        INFO("murray crown dives open (trim on): "
             << crown_open
             << "/3, blocked with trim off: " << crown_blocked_off);
        REQUIRE(crown_open == 3);
        REQUIRE(crown_blocked_off == 3);  // the dome is the trim's own kill
    }
    // The swing into the bore: from low over the mouth down the bore axis.
    const glm::dvec3 swing_a = mm + up_m * 10.0 + t_m * 60.0;
    const glm::dvec3 swing_b = mm - t_m * 160.0;
    REQUIRE_FALSE(blocked(occ_on, swing_a, swing_b));

    // ---- ERRINGTON: the descending approach channel must READ open — a
    // vertical sightline over each trench step reaches that step's own floor
    // (pre-trim the shallower neighbours' full-circle drapes roofed it), and
    // the bore-tangent entry line (the line the T13 trench was dug to clear)
    // reaches the bore.
    int steps_open = 0, steps_blocked_off = 0, steps_live = 0;
    for (int ti = 0; ti < world::TunnelNet::kTrenchSteps; ++ti) {
        const world::TunnelNet::Bowl& tb = net.trench[ti];
        if (tb.bowl_depth <= 0.0) continue;
        ++steps_live;
        const glm::dvec3 hi = tb.axis * (tb.surface_r + 300.0);
        const glm::dvec3 lo = tb.axis * (tb.surface_r - tb.bowl_depth + 6.0);
        if (!blocked(occ_on, hi, lo)) ++steps_open;
        if (blocked(occ_off, hi, lo)) ++steps_blocked_off;
    }
    INFO("errington steps open (trim on): " << steps_open << "/" << steps_live
                                            << ", blocked with trim off: "
                                            << steps_blocked_off);
    REQUIRE(steps_live == world::TunnelNet::kTrenchSteps);
    REQUIRE(steps_open == steps_live);
    REQUIRE(steps_blocked_off >= 2);  // the drape roof over the channel
    // The bore-tangent entry line, from up-trench outside into the bore.
    const glm::dvec3 me = net.spine[0].pos;
    const glm::dvec3 t_e = glm::normalize(net.spine[1].pos - net.spine[0].pos);
    const glm::dvec3 entry_a = me - t_e * 900.0;
    const glm::dvec3 entry_b = me + t_e * 150.0;
    {
        const double len = glm::length(entry_b - entry_a);
        const glm::dvec3 d = (entry_b - entry_a) / len;
        INFO("errington entry line: trim-on nearest hit "
             << nearest_hit(entry_a, d, occ_on) << " trim-off "
             << nearest_hit(entry_a, d, occ_off) << " of " << len);
        CHECK_FALSE(blocked(occ_on, entry_a, entry_b));
        CHECK(blocked(occ_off, entry_a, entry_b));
    }
}

// ---------------------------------------------------------------------------
// T18 — ESCAPE SURVIVABILITY (fly-14, Chad: "the murray mine pit has to be a
// real space. I collided into nothing escaping the pit. It still thinks there
// is ground there."). The terrain heightfield still carries FULL surface
// height over the pits, so below grade a pilot lives exactly where
// net.contains says — and the open tube DEAD-ENDED at the inclined Murray
// mouth ring: in front of the arch below the pit-floor plane there was no
// open volume at all. The T18 mouth THROAT extends the bore's volume out
// until it clears the pit floor. THE PIN: exit paths flown out of both bores
// never pass a LETHAL point (below terrain, outside the net) unless a RENDER
// surface is visibly there to hit (dying AT a drawn wall is honest; dying on
// nothing is the bug). Load-bearing arm: the same Murray paths against a
// throat_on=false net must die on nothing (kills a throat-deletion mutant).
TEST_CASE(
    "T18 escape survivable: bore exit paths never die on invisible rock, and "
    "the throat-off arm does") {
    const world::TunnelParams tp = t16_ship_tp();
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);
    REQUIRE(net.throat_on);  // the shipped Murray mouth is inclined
    world::TunnelNet no_throat = net;
    no_throat.throat_on = false;  // the T18 mutation lever

    const double cell = render::terrain_cell_arc(200, 2, 15000.0);
    const double err_collar =
        render::collar_reach(net.pit.bowl_r, 200, 2, 15000.0);
    const double mur_collar =
        render::collar_reach(net.bowl.bowl_r, 200, 2, 15000.0);
    const render::TunnelMeshData data =
        render::build_tunnel_mesh(net, &hf, err_collar, mur_collar, cell);
    const std::vector<AuditTri> occ = collect_occluders(data, false, false);
    REQUIRE(occ.size() > 1000);

    // March an exit path: LETHAL-ON-NOTHING when a point is below terrain and
    // outside the net while the nearest render surface ahead is > 25 m away
    // (a pilot dying within 25 m of a drawn wall hit the wall; farther = he
    // "collided into nothing"). Stops at the render hit or +30 m above grade.
    // A death is HONEST when a rendered surface sits within kHonest_m of the
    // death point in ANY direction (the pilot hit — or skimmed under — a
    // drawn wall/floor); "collided into nothing" = lethal with no render
    // anywhere near.
    constexpr double kHonest_m = 30.0;
    // T23: the render-side arm is parameterized (default = the shipped mesh)
    // so the pure-vertical rows can be mutation-verified against the
    // whole-facet-trim mesh — the round-16 "died from something invisible".
    const auto near_render_in = [&](const glm::dvec3& q,
                                    const std::vector<AuditTri>& o) {
        for (const AuditTri& tr : o)
            if (glm::length(tr.cen - q) - tr.rad < kHonest_m) return true;
        return false;
    };
    const auto lethal_on_nothing_in =
        [&](const world::TunnelNet& n, const glm::dvec3& p0,
            const glm::dvec3& d, const std::vector<AuditTri>& o) {
            const double t_hit = nearest_hit(p0, d, o);
            for (double s = 5.0; s < 1500.0; s += 5.0) {
                if (s > t_hit - 25.0)
                    return false;  // a drawn wall owns the death
                const glm::dvec3 q = p0 + s * d;
                const double r = glm::length(q);
                if (r > hf.radius_at(q / r) + 30.0) return false;  // escaped
                if (r < hf.radius_at(q / r) && !n.contains(q))
                    return !near_render_in(q, o);
            }
            return false;
        };
    const auto lethal_on_nothing = [&](const world::TunnelNet& n,
                                       const glm::dvec3& p0,
                                       const glm::dvec3& d) {
        return lethal_on_nothing_in(n, p0, d, occ);
    };

    // The exit fan at a mouth: paths from a start point inside the bore, at
    // climb angles above horizontal in the exit plane, plus small azimuth
    // spread. Returns how many paths die on nothing.
    const auto fan_deaths = [&](const world::TunnelNet& n, bool murray) {
        const auto& sp = net.spine;
        const std::size_t nn = sp.size();
        const glm::dvec3 mouth = murray ? sp[nn - 1].pos : sp[0].pos;
        const glm::dvec3 t_out =
            murray ? glm::normalize(sp[nn - 1].pos - sp[nn - 2].pos)
                   : glm::normalize(sp[0].pos - sp[1].pos);
        const glm::dvec3 up = glm::normalize(mouth);
        glm::dvec3 fwd = t_out - glm::dot(t_out, up) * up;
        fwd = glm::normalize(fwd);
        const glm::dvec3 side = glm::normalize(glm::cross(up, fwd));
        // Starts inside the bore ABOVE and BELOW the centreline (the fly-14
        // death was an at-or-below-centreline exit riding the bore slope out
        // of the mouth), across climb angles bracketing the exit incline.
        const glm::dvec3 node = murray ? sp[nn - 2].pos : sp[1].pos;
        int deaths = 0;
        const double ups[] = {20.0, -40.0};
        const double degs[] = {5.0, 12.0, 18.0, 22.0, 32.0, 45.0};
        for (double u0 : ups)
            for (double el_deg : degs)
                for (int az = -1; az <= 1; ++az) {
                    const double el = el_deg * 3.14159265358979323846 / 180.0;
                    glm::dvec3 d = std::cos(el) * fwd + std::sin(el) * up +
                                   0.15 * static_cast<double>(az) * side;
                    d = glm::normalize(d);
                    if (lethal_on_nothing(n, node + up * u0, d)) ++deaths;
                }
        return deaths;
    };

    // T21 — the INBOUND fan (fly-16: "I died a few times going into the
    // errington tunnel... something invisible is there" — the T18 fans only
    // flew OUTBOUND, so an entry-line regression shipped: the Errington pit
    // cone strangled the descent sag below the trench floor). Paths from
    // outside/above the approach, descending onto ring targets at and below
    // the mouth centre — the lines a pilot actually flies IN.
    const auto inbound_deaths = [&](const world::TunnelNet& n, bool murray) {
        const auto& sp = net.spine;
        const std::size_t nn = sp.size();
        const glm::dvec3 mouth = murray ? sp[nn - 1].pos : sp[0].pos;
        const glm::dvec3 t_out =
            murray ? glm::normalize(sp[nn - 1].pos - sp[nn - 2].pos)
                   : glm::normalize(sp[0].pos - sp[1].pos);
        const glm::dvec3 up = glm::normalize(mouth);
        glm::dvec3 fwd = t_out - glm::dot(t_out, up) * up;
        fwd = glm::normalize(fwd);
        const glm::dvec3 side = glm::normalize(glm::cross(up, fwd));
        int deaths = 0;
        // Start heights are MOUTH-SPECIFIC honest pilot lines: at Errington
        // the approach flies the trench channel (mouth is 130 below grade);
        // at Murray the approach dives in from ABOVE THE RIM (the pit floor
        // is 300 down — starts below the rim would sit inside the staircase
        // rock no pilot flies through).
        const double starts_e[] = {80.0, 160.0, 280.0};
        const double starts_m[] = {320.0, 420.0, 550.0};
        const double* starts_h = murray ? starts_m : starts_e;
        const double tgt_dz[] = {30.0, 0.0, -35.0};  // ring-relative aims
        for (int hi = 0; hi < 3; ++hi)
            for (double dz : tgt_dz)
                for (int az = -1; az <= 1; ++az) {
                    const glm::dvec3 p0 = mouth + fwd * 380.0 +
                                          up * starts_h[hi] +
                                          side * (40.0 * az);
                    const glm::dvec3 tgt =
                        mouth + up * dz - t_out * 60.0;  // just inside
                    const glm::dvec3 d = glm::normalize(tgt - p0);
                    if (lethal_on_nothing(n, p0, d)) ++deaths;
                }
        return deaths;
    };
    {
        const int mur_in = inbound_deaths(net, /*murray=*/true);
        const int err_in = inbound_deaths(net, /*murray=*/false);
        INFO("inbound deaths-on-nothing murray " << mur_in << ", errington "
                                                 << err_in);
        REQUIRE(mur_in == 0);
        REQUIRE(err_in == 0);
    }

    // T22 — STEEP DIVES into the FRONT half of the Murray dark strip (fly-16:
    // "coming down right down the pipe hit something invisible"): the floor
    // disk is trimmed over the throat's footprint too, so the visible dark
    // opening spans BOTH sides of the ring — and a near-vertical descent into
    // the front half bottoms on the throat's collision RAMP. With the T22
    // ramp face drawn, that stop is a rendered surface (the honest-death
    // metric); before it, it was 40-60 m of nothing.
    {
        const std::size_t nn = net.spine.size();
        const glm::dvec3 mouth = net.spine[nn - 1].pos;
        const glm::dvec3 t_out =
            glm::normalize(net.spine[nn - 1].pos - net.spine[nn - 2].pos);
        const glm::dvec3 up = glm::normalize(mouth);
        glm::dvec3 fwd = t_out - glm::dot(t_out, up) * up;
        fwd = glm::normalize(fwd);
        const glm::dvec3 side = glm::normalize(glm::cross(up, fwd));
        int strip_deaths = 0;
        for (double s_along : {60.0, 110.0, 160.0})
            for (int az = -1; az <= 1; ++az) {
                const glm::dvec3 tgt =
                    mouth + t_out * s_along - up * 20.0 + side * (25.0 * az);
                const glm::dvec3 p0 = tgt + up * 550.0 + fwd * 40.0;
                const glm::dvec3 d = glm::normalize(tgt - p0);
                if (lethal_on_nothing(net, p0, d)) ++strip_deaths;
            }
        INFO("steep strip dives dying on nothing: " << strip_deaths);
        REQUIRE(strip_deaths == 0);

        // T23 — PURE-VERTICAL dives (fly round-16 tail: "I went straight
        // down my mine and died from something invisible" — the rows above
        // are ~70-80 degrees; his was 90). One true-vertical line per point
        // of a coarse annulus across the pit floor and the floor/slot
        // junction: any stop below grade must be within kHonest_m of a drawn
        // surface. Rides the same subdivided-trim cover T23's leak probe
        // certifies see-through-free.
        const glm::dvec3 fc =
            net.bowl.axis * (net.bowl.surface_r - net.bowl.bowl_depth);
        glm::dvec3 vu = side;  // any tangent basis about the bowl axis
        const glm::dvec3 vw = glm::normalize(glm::cross(net.bowl.axis, vu));
        // The funnel's floor-edge radius, by the SAME law the benched wall
        // builds with (cone_rim_r == bowl_r on the shipped hug-rim path;
        // floor_edge_r = rim - (Nb-1)/Nb * (rim - floor_r)) — config-relative,
        // so a bench/bowl retune moves the probe with the junction.
        const int Nb = render::kBowlBenches;
        const double floor_edge_r =
            net.bowl.bowl_r -
            (net.bowl.bowl_r - net.bowl.floor_r) * (Nb - 1) / Nb;
        const auto drop_vertical = [&](const std::vector<AuditTri>& o,
                                       const glm::dvec3& tgt) {
            // Start just under the harness's escaped ceiling (grade +30) on
            // tgt's own vertical; dive straight down that local up (the
            // 90-degree line). A grade+300 start VACUOUSLY passes — the
            // first march sample reads "escaped" and no row ever fires
            // (measured; the fixture-no-op class).
            const glm::dvec3 up_t = glm::normalize(tgt);
            const glm::dvec3 start = up_t * (net.bowl.surface_r + 25.0);
            return lethal_on_nothing_in(net, start, -up_t, o);
        };
        const auto vertical_deaths = [&](const std::vector<AuditTri>& o) {
            int n = 0;
            // The pit-floor/slot JUNCTION annulus (where Chad saw through).
            for (double rf : {0.70, 0.90, 1.00, 1.06})
                for (int ia = 0; ia < 8; ++ia) {
                    const double aa = 2.0 * 3.14159265358979323846 * ia / 8.0;
                    if (drop_vertical(o, fc + rf * floor_edge_r *
                                                  (std::cos(aa) * vu +
                                                   std::sin(aa) * vw)))
                        ++n;
                }
            // Stations straight down the dark STRIP over the throat.
            for (double s_along : {50.0, 100.0, 150.0})
                if (drop_vertical(o, mouth + t_out * s_along)) ++n;
            return n;
        };
        INFO("pure-vertical dives dying on nothing: " << vertical_deaths(occ));
        REQUIRE(vertical_deaths(occ) == 0);
        // Mutation arm: with the whole MURRAY MOUTH PIECE undrawn, the pit
        // has no rendered excavation at all and the vertical stops go naked —
        // the rows must fire. Honesty note, both measured: two sharper arms
        // do NOT kill these rows — the whole-facet-trim mesh (every junction
        // death sits within kHonest_m of some drawn surface; its defect is
        // SEE-THROUGH, pinned by the T23 leak probe's own arm) and throat-off
        // collision (the T22 ramp face keeps those stops honest). The rows
        // guard gross cover regressions on the 90-degree line, nothing finer.
        render::TunnelMeshData data_nc = data;  // shipped pieces, collar cut
        {
            std::vector<render::TunnelPiece> kept_pieces;
            const glm::dvec3 mdir = glm::normalize(mouth);
            for (render::TunnelPiece& pc : data_nc.pieces) {
                // Drop only the MURRAY mouth piece: collar-tagged and
                // nearest the Murray mouth (the Errington collar centroid
                // sits by the other mouth).
                bool murray_collar = false;
                if (pc.is_collar && !pc.positions.empty()) {
                    glm::dvec3 cen{0.0};
                    const std::size_t nv = pc.cue.size();
                    for (std::size_t i = 0; i < nv; ++i)
                        cen += glm::dvec3(pc.positions[3 * i + 0],
                                          pc.positions[3 * i + 1],
                                          pc.positions[3 * i + 2]);
                    cen /= static_cast<double>(nv);
                    murray_collar = glm::length(cen - mouth) <
                                    glm::length(cen - net.spine.front().pos);
                }
                if (!murray_collar) kept_pieces.push_back(std::move(pc));
            }
            data_nc.pieces = std::move(kept_pieces);
        }
        const std::vector<AuditTri> occ_nc =
            collect_occluders(data_nc, false, false);
        REQUIRE(occ_nc.size() < occ.size());  // the piece really dropped
        const int nc_deaths = vertical_deaths(occ_nc);
        INFO("pure-vertical dives with the Murray mouth piece undrawn: "
             << nc_deaths);
        REQUIRE(nc_deaths > 0);
    }

    // The shipped net: every exit path at BOTH mouths is honest.
    const int mur_deaths = fan_deaths(net, /*murray=*/true);
    const int err_deaths = fan_deaths(net, /*murray=*/false);
    INFO("murray deaths-on-nothing " << mur_deaths << ", errington "
                                     << err_deaths);
    REQUIRE(mur_deaths == 0);
    REQUIRE(err_deaths == 0);

    // The mutation arm: the exit CORRIDOR itself. At-or-below the centreline
    // in front of the Murray arch (the fly-14 death line) the air must be
    // OPEN; with the throat off that exact corridor is rock again. A direct
    // volume pin — unlike the path fan it cannot be satisfied by making the
    // dead pocket merely VISIBLE (the apron); the pilot needs it FLYABLE.
    {
        const glm::dvec3 mm = net.spine.back().pos;
        const glm::dvec3 t_out = glm::normalize(
            net.spine.back().pos - net.spine[net.spine.size() - 2].pos);
        const glm::dvec3 up = glm::normalize(mm);
        const double floor_shell = net.bowl.surface_r - net.bowl.bowl_depth;
        int open_on = 0, open_off = 0, total = 0;
        for (double s = 10.0; s <= 120.0; s += 10.0)
            for (double dz : {-60.0, -35.0, -10.0}) {
                const glm::dvec3 q = mm + t_out * s + up * dz;
                // Only the BELOW-FLOOR corridor counts (above it the bowl
                // owns the air with or without the throat).
                if (glm::dot(q, net.bowl.axis) - floor_shell > -2.0) continue;
                ++total;
                if (net.contains(q)) ++open_on;
                if (no_throat.contains(q)) ++open_off;
            }
        INFO("below-floor exit corridor: " << total << " samples, open with "
                                           << "throat " << open_on << ", off "
                                           << open_off);
        REQUIRE(total >= 12);           // non-vacuous sample set
        REQUIRE(open_on == total);      // the corridor is flyable air
        REQUIRE(open_off < total / 4);  // the throat is load-bearing
    }
}

// ---------------------------------------------------------------------------
// T26 — THE LEAK CENSUS. The seal-pass strategy (docs/... /seal-pass): instead
// of fly-discover-fix-fly whack-a-mole, ONE offline sweep that (a) covers the
// WHOLE tunnel from every viewpoint class a pilot can occupy, (b) CLUSTERS the
// leaking sightlines into discrete holes with a world point + repro string +
// suspect piece, and (c) STANDS as the forever regression tripwire
// (REQUIRE(total_leak_rays == 0)). This locates the flown findings offline:
//   * Finding 3 — a rectangular see-through gap BELOW the tunnel-mouth bottom
//     at BOTH chamber<->bore junction mouths (the uncovered-annulus class); the
//     junction eyes rake the band below each mouth down to the chamber wall.
//   * Finding 4 — one more see-through hole into the arena egg from the
//     Errington side; the chamber panoramic + the exterior->in grid net it.
// Leak semantics are the SHARED ray_leaks (a sightline through solid rock
// underground with no render occluder in front, reaching open sky) — never
// re-derived. The mutation arm (drop the breach collars) proves the census
// actually watches the junctions.
namespace {
const char* kPieceName(int k) {
    switch (k) {
        case 0: return "kWall";
        case 1: return "kChamber";
        case 2: return "kCollar";
        case 3: return "kFloor";
        case 4: return "kCavern";
        case 5: return "kTrench";
        case 6: return "kPortal";
        default: return "?";
    }
}
}  // namespace

TEST_CASE("T26 leak census: full-coverage sweep clusters leaks into holes") {
    // -------- density constants (tune to keep runtime < ~90 s) --------
    constexpr int kJunN = 9;             // junction 2x-bore disk half-grid
    constexpr int kJunBelowRows = 8;     // extra rows raking below the mouth
    constexpr double kJunBelowMul = 4.0; // rake to this x hole_minor below
    constexpr int kChAz = 44, kChEl = 22;    // chamber panoramic fan
    constexpr int kExtRings = 5, kExtAz = 16;    // exterior eye grid
    constexpr int kExtTRings = 4, kExtTAz = 12;  // exterior target grid
    constexpr double kBoreStep_m = 300.0;    // bore-eye arclength spacing
    constexpr int kBoAz = 14, kBoEl = 7;     // bore fan
    constexpr double kDiveStep_m = 9.0;      // mouth vertical-dive ring step
    constexpr int kDiveAz = 30;              // mouth vertical-dive azimuths
    constexpr double kClusterMerge_m = 120.0;    // greedy 3D cluster radius
    const double pi = 3.14159265358979323846;
    const double R = 15000.0;

    // -------- geometry (mirror T16 exactly) --------
    const world::TunnelParams tp = t16_ship_tp();
    const world::HeightField hf = uniform_field(300.0);
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);
    const double cell = render::terrain_cell_arc(200, 2, R);
    const double err_collar =
        render::collar_reach(net.pit.bowl_r, 200, 2, R);
    const double mur_collar =
        render::collar_reach(net.bowl.bowl_r, 200, 2, R);
    const render::TunnelMeshData data =
        render::build_tunnel_mesh(net, &hf, err_collar, mur_collar, cell);
    const std::vector<render::Breach> breaches = render::breach_points(net);
    REQUIRE(breaches.size() == 2);
    const std::vector<AuditTri> tris =
        collect_occluders(data, /*drop_trench=*/false,
                          /*drop_breach_collars=*/false);
    REQUIRE(tris.size() > 1000);

    // The cut disks (pit / bowl / live trench steps) the surrounding terrain is
    // missing — SAME construction as audit_all_eyes (T16), so a ray leaving
    // through an OPEN cut is never a false leak.
    const glm::dvec3 edir = glm::normalize(net.spine.front().pos);
    const glm::dvec3 mdir = glm::normalize(net.spine.back().pos);
    const double margin = 1.5 * cell;
    std::vector<AuditCut> cuts;
    cuts.push_back({edir, net.pit.bowl_r + margin});
    cuts.push_back({mdir, net.bowl.bowl_r + margin});
    for (int i = 0; i < world::TunnelNet::kTrenchSteps; ++i)
        if (net.trench[i].bowl_depth > 0.0)
            cuts.push_back({net.trench[i].axis, net.trench[i].bowl_r + margin});

    // -------- spine arclength (for the s_arc field + BORE repro) --------
    std::vector<double> arc(net.spine.size(), 0.0);
    for (std::size_t i = 1; i < net.spine.size(); ++i)
        arc[i] = arc[i - 1] +
                 glm::length(net.spine[i].pos - net.spine[i - 1].pos);
    const auto nearest_arc = [&](const glm::dvec3& p) {
        double best = 0.0, bd = 1e30;
        for (std::size_t i = 0; i < net.spine.size(); ++i) {
            const double d = glm::length(net.spine[i].pos - p);
            if (d < bd) { bd = d; best = arc[i]; }
        }
        return best;
    };

    // -------- CAVERN repro azimuth (main.cpp:2481 semantics) --------
    const glm::dvec3 GE = glm::normalize(world::kTunnelMouthErrington);
    const glm::dvec3 GM = glm::normalize(world::kTunnelMouthMurray);
    const glm::dvec3 Gn = glm::normalize(glm::cross(GE, GM));
    const glm::dvec3 Gb = glm::normalize(glm::cross(Gn, GE));
    const auto cav_az_deg = [&](const glm::dvec3& p) {
        const glm::dvec3 d = glm::normalize(p);
        return std::atan2(glm::dot(d, Gb), glm::dot(d, GE)) * 180.0 / pi;
    };

    // Nudge an interior eye toward the arena centre until it is inside the open
    // volume (a below-breach eye can start buried in rock, which would fabricate
    // a leak from the eye's own cell).
    const auto inside_eye = [&](glm::dvec3 e) {
        for (int k = 0; k < 10 && !net.contains(e); ++k)
            e = e + 0.25 * (net.arena.center - e);
        return e;
    };

    enum Region { JUNCTION = 0, CHAMBER, EXTERIOR, BORE, MOUTH, NREG };
    const char* rname[NREG] = {"junctions", "chamber", "exterior-in", "bore",
                               "mouths"};
    struct EyeGrp {
        int region;
        glm::dvec3 pos;
        std::vector<glm::dvec3> targets;
    };
    std::vector<EyeGrp> groups;

    // A shell target in a breach's angle frame (T15 construction).
    const auto shell_target = [&](const glm::dvec3& ax, const glm::dvec3& e1,
                                  const glm::dvec3& e2, double um, double vm,
                                  double Rl) {
        const double rho = std::sqrt(um * um + vm * vm);
        glm::dvec3 wdir = ax;
        if (rho > 1e-12) {
            const glm::dvec3 dirp = (um * e1 + vm * e2) / rho;
            wdir = glm::normalize(std::cos(rho) * ax + std::sin(rho) * dirp);
        }
        return wdir * Rl;
    };

    // ==== (a) JUNCTION eyes — finding 3 ====
    for (const render::Breach& b : breaches) {
        const glm::dvec3 ax = glm::normalize(b.pos);
        glm::dvec3 e1 = b.into - glm::dot(b.into, ax) * ax;
        e1 = glm::length(e1) > 1e-9 ? glm::normalize(e1) : glm::dvec3(1, 0, 0);
        const glm::dvec3 e2 = glm::normalize(glm::cross(ax, e1));
        const double Rl = glm::length(b.pos);
        const double majr = 2.0 * b.hole_major_m;  // 2x the bore cross-section
        const double minr = 2.0 * b.hole_minor_m;
        // The three eyes: centre-side, lateral, and BELOW the breach.
        const std::vector<glm::dvec3> eyes = {
            inside_eye(b.pos + 350.0 * b.into),
            inside_eye(b.pos + 300.0 * b.into + 350.0 * e1),
            inside_eye(b.pos + 300.0 * b.into - 300.0 * ax)};
        for (const glm::dvec3& eye : eyes) {
            std::vector<glm::dvec3> t;
            // The 2x-bore disk over the breach.
            for (int iu = -kJunN; iu <= kJunN; ++iu)
                for (int iv = -kJunN; iv <= kJunN; ++iv) {
                    const double um = (double(iu) / kJunN) * majr / Rl;
                    const double vm = (double(iv) / kJunN) * minr / Rl;
                    t.push_back(shell_target(ax, e1, e2, um, vm, Rl));
                }
            // Extra rows raking the band BELOW the mouth bottom down to the
            // chamber wall (the reported rectangular gap).
            for (int ib = 1; ib <= kJunBelowRows; ++ib) {
                const double below =
                    b.hole_minor_m + (double(ib) / kJunBelowRows) *
                                         (kJunBelowMul - 1.0) * b.hole_minor_m;
                const double vm = -below / Rl;
                for (int iu = -kJunN; iu <= kJunN; ++iu) {
                    const double um = (double(iu) / kJunN) * majr / Rl;
                    t.push_back(shell_target(ax, e1, e2, um, vm, Rl));
                }
            }
            groups.push_back({JUNCTION, eye, std::move(t)});
        }
    }

    // ==== (b) CHAMBER panoramic — finding 4 net (interior->out) ====
    {
        const glm::dvec3 toward_err =
            glm::normalize(breaches[0].pos - net.arena.center);
        const double a_long = net.arena.a_pos;
        const std::vector<glm::dvec3> ce = {
            net.arena.center,
            inside_eye(net.arena.center + 0.35 * a_long * toward_err),
            inside_eye(net.arena.center + 0.60 * a_long * toward_err)};
        for (const glm::dvec3& e : ce)
            groups.push_back({CHAMBER, e, sphere_fan(e, 500, kChAz, kChEl)});
    }

    // ==== (c) EXTERIOR->IN over the Errington half of the arena footprint ====
    {
        const glm::dvec3 cdir = glm::normalize(net.arena.center);
        const double surfC = hf.radius_at(cdir);
        glm::dvec3 tu =
            std::abs(cdir.z) < 0.9 ? glm::dvec3(0, 0, 1) : glm::dvec3(1, 0, 0);
        tu = glm::normalize(tu - glm::dot(tu, cdir) * cdir);
        const glm::dvec3 tw = glm::normalize(glm::cross(cdir, tu));
        // Errington reference direction in the tangent plane.
        glm::dvec3 eref = GE - glm::dot(GE, cdir) * cdir;
        eref = glm::length(eref) > 1e-9 ? glm::normalize(eref) : tu;
        const double foot = net.arena.b;  // across-arena semi-axis [m]
        // A shared surface target set over the Errington half of the footprint.
        std::vector<glm::dvec3> etargets;
        etargets.push_back(cdir * hf.radius_at(cdir));
        for (int ir = 1; ir <= kExtTRings; ++ir)
            for (int ia = 0; ia < kExtTAz; ++ia) {
                const double ang = (foot * ir / kExtTRings) / surfC;
                const double az = 2.0 * pi * ia / kExtTAz;
                const glm::dvec3 tang =
                    std::cos(az) * tu + std::sin(az) * tw;
                if (glm::dot(tang, eref) < -0.15) continue;  // Errington half
                const glm::dvec3 dd =
                    glm::normalize(std::cos(ang) * cdir + std::sin(ang) * tang);
                etargets.push_back(dd * hf.radius_at(dd));
            }
        // Eyes 300 m above the surface across the same half-footprint.
        for (int ir = 1; ir <= kExtRings; ++ir)
            for (int ia = 0; ia < kExtAz; ++ia) {
                const double ang = (foot * ir / kExtRings) / surfC;
                const double az = 2.0 * pi * ia / kExtAz;
                const glm::dvec3 tang =
                    std::cos(az) * tu + std::sin(az) * tw;
                if (glm::dot(tang, eref) < -0.15) continue;
                const glm::dvec3 dd =
                    glm::normalize(std::cos(ang) * cdir + std::sin(ang) * tang);
                const glm::dvec3 e = dd * (hf.radius_at(dd) + 300.0);
                groups.push_back({EXTERIOR, e, etargets});
            }
    }

    // ==== (d) BORE sweep — every ~kBoreStep_m of arclength ====
    {
        double acc = kBoreStep_m;  // emit the mouth node first
        for (std::size_t i = 0; i < net.spine.size(); ++i) {
            if (i > 0) acc += glm::length(net.spine[i].pos - net.spine[i - 1].pos);
            if (acc < kBoreStep_m) continue;
            acc = 0.0;
            const glm::dvec3 e = net.spine[i].pos;
            groups.push_back({BORE, e, sphere_fan(e, 500, kBoAz, kBoEl)});
        }
    }

    // ==== (e) MOUTH exterior — vertical dives + oblique + orbit eyes ====
    const auto add_mouth = [&](const world::TunnelNet::Bowl& bw) {
        if (bw.bowl_depth <= 0.0) return;
        const glm::dvec3 axis = bw.axis;
        glm::dvec3 tu =
            std::abs(axis.z) < 0.9 ? glm::dvec3(0, 0, 1) : glm::dvec3(1, 0, 0);
        tu = glm::normalize(tu - glm::dot(tu, axis) * axis);
        const glm::dvec3 tw = glm::normalize(glm::cross(axis, tu));
        const glm::dvec3 fc = axis * (bw.surface_r - bw.bowl_depth);  // floor
        // True-vertical dives over the whole pit floor (T23 pattern): each
        // target diven along ITS OWN local up.
        for (double rr = 30.0; rr <= 1.3 * bw.bowl_r; rr += kDiveStep_m)
            for (int ia = 0; ia < kDiveAz; ++ia) {
                const double az = 2.0 * pi * ia / kDiveAz;
                const glm::dvec3 tgt =
                    fc + rr * (std::cos(az) * tu + std::sin(az) * tw);
                const glm::dvec3 eye =
                    glm::normalize(tgt) * (bw.surface_r + 250.0);
                groups.push_back({MOUTH, eye, {tgt}});
            }
        // Oblique + orbit eyes casting aimed cut fans (kept within the collar
        // reach so surrounding terrain is never a false leak).
        const double reach = render::collar_reach(bw.bowl_r, 200, 2, R) -
                             bw.bowl_r;
        const glm::dvec3 c = axis * bw.surface_r;
        const std::vector<glm::dvec3> obl = {
            c + axis * 300.0,          c + axis * 1200.0,
            c + axis * 700.0 + tu * 500.0, c + axis * 700.0 + tw * 500.0,
            c + axis * 500.0 - tu * 400.0};
        for (const glm::dvec3& e : obl)
            groups.push_back(
                {MOUTH, e,
                 cut_fan(axis, bw.surface_r, bw.bowl_r + reach - 25.0, 6, 24)});
    };
    add_mouth(net.pit);
    add_mouth(net.bowl);

    // -------- run the census (mode 0) --------
    struct Leak {
        glm::dvec3 pt;
        int region;
    };
    std::vector<Leak> all_leaks;
    long reyes[NREG] = {0}, rrays[NREG] = {0}, rleaks[NREG] = {0};
    for (const EyeGrp& g : groups) {
        reyes[g.region]++;
        for (const glm::dvec3& tg : g.targets) {
            rrays[g.region]++;
            glm::dvec3 lp{0.0};
            if (ray_leaks(g.pos, tg, tris, net, hf, cuts, R, &lp)) {
                all_leaks.push_back({lp, g.region});
                rleaks[g.region]++;
            }
        }
    }
    const long total_leak_rays = static_cast<long>(all_leaks.size());
    long total_rays = 0, total_eyes = 0;
    for (int r = 0; r < NREG; ++r) {
        total_rays += rrays[r];
        total_eyes += reyes[r];
    }

    // -------- greedy 3D cluster (< kClusterMerge_m) --------
    struct Cluster {
        glm::dvec3 cen;
        long n;
        long rvotes[NREG];
    };
    std::vector<Cluster> clusters;
    for (const Leak& lk : all_leaks) {
        int best = -1;
        double bd = kClusterMerge_m;
        for (std::size_t i = 0; i < clusters.size(); ++i) {
            const double d = glm::length(lk.pt - clusters[i].cen);
            if (d < bd) { bd = d; best = static_cast<int>(i); }
        }
        if (best < 0) {
            Cluster c;
            c.cen = lk.pt;
            c.n = 1;
            for (int r = 0; r < NREG; ++r) c.rvotes[r] = 0;
            c.rvotes[lk.region] = 1;
            clusters.push_back(c);
        } else {
            Cluster& c = clusters[best];
            c.cen = (c.cen * double(c.n) + lk.pt) / double(c.n + 1);
            c.n++;
            c.rvotes[lk.region]++;
        }
    }

    // -------- report: header --------
    WARN("audit mode 0 CENSUS SUMMARY: eyes=" << total_eyes << " rays="
         << total_rays << " leak_rays=" << total_leak_rays << " holes="
         << clusters.size());

    // -------- report: one machine-parseable line per hole --------
    for (std::size_t k = 0; k < clusters.size(); ++k) {
        const Cluster& c = clusters[k];
        // Suspect: nearest occluder triangle to the cluster point.
        int nk = -1, np = -1;
        double bestd = 1e30;
        for (const AuditTri& tr : tris) {
            const double d = glm::length(tr.cen - c.cen);
            if (d < bestd) { bestd = d; nk = tr.kind; np = tr.pidx; }
        }
        const double d0 = glm::length(c.cen - breaches[0].pos);
        const double d1 = glm::length(c.cen - breaches[1].pos);
        const double dmouthE = glm::length(c.cen - net.spine.front().pos);
        const double dmouthM = glm::length(c.cen - net.spine.back().pos);
        std::string prox;
        if (d0 < 300.0 || d1 < 300.0) prox += "[near-breach]";
        if (dmouthE < 400.0 || dmouthM < 400.0) prox += "[near-mouth]";
        for (int i = 0; i < world::TunnelNet::kTrenchSteps; ++i)
            if (net.trench[i].bowl_depth > 0.0 &&
                glm::length(c.cen - net.trench[i].axis * net.trench[i].surface_r) <
                    300.0)
                prox += "[near-trench]";
        if (prox.empty()) prox = "[open-rock]";
        // Majority region -> repro string.
        int mr = 0;
        for (int r = 1; r < NREG; ++r)
            if (c.rvotes[r] > c.rvotes[mr]) mr = r;
        char repro[192];
        if (mr == BORE) {
            std::snprintf(repro, sizeof(repro), "SEADS_TUNCAM_BORE=\"%.1f\"",
                          nearest_arc(c.cen));
        } else if (mr == MOUTH) {
            const bool errside = glm::dot(glm::normalize(c.cen), GE) >
                                 glm::dot(glm::normalize(c.cen), GM);
            const glm::dvec3 mpos =
                errside ? net.spine.front().pos : net.spine.back().pos;
            const glm::dvec3 up = errside ? edir : mdir;
            const glm::dvec3 other = errside ? GM : GE;
            glm::dvec3 ref = other - glm::dot(other, up) * up;
            ref = glm::length(ref) > 1e-9 ? glm::normalize(ref) : glm::dvec3(1, 0, 0);
            const glm::dvec3 v = c.cen - mpos;
            const glm::dvec3 vh = v - glm::dot(v, up) * up;
            const double azd =
                std::atan2(glm::dot(vh, glm::cross(up, ref)), glm::dot(vh, ref)) *
                180.0 / pi;
            const double eld =
                std::asin(glm::clamp(
                    glm::dot(glm::normalize(v), up), -1.0, 1.0)) *
                180.0 / pi;
            std::snprintf(repro, sizeof(repro),
                          "SEADS_TUNCAM_MOUTH=\"%.1f,%.1f,%.1f,%s\"",
                          glm::length(v), azd, eld, errside ? "err" : "mur");
        } else {
            const char* look = (d0 < 700.0) ? "breach"
                               : (d1 < 700.0) ? "breach2"
                                              : "core";
            std::snprintf(repro, sizeof(repro),
                          "SEADS_TUNCAM_CAVERN=\"%.1f,%.1f,%s\"",
                          glm::length(c.cen), cav_az_deg(c.cen), look);
        }
        char line[512];
        std::snprintf(
            line, sizeof(line),
            "CENSUS HOLE %zu: rays=%ld world=(%.1f,%.1f,%.1f) |p|=%.1f "
            "s_arc=%.1f breach_dist=(%.1f,%.1f) region=%s suspect=%s(%d) "
            "piece#%d %s repro=%s",
            k, c.n, c.cen.x, c.cen.y, c.cen.z, glm::length(c.cen),
            nearest_arc(c.cen), d0, d1, rname[mr], kPieceName(nk), nk, np,
            prox.c_str(), repro);
        WARN("audit mode 0 " << line);
    }

    // -------- report: honest negatives (per region) --------
    for (int r = 0; r < NREG; ++r)
        if (rleaks[r] == 0)
            WARN("audit mode 0 CENSUS CLEAN: " << rname[r] << " eyes="
                 << reyes[r] << " rays=" << rrays[r]);

    // -------- MUTATION ARM (census mode 1): drop the breach collars; the
    // junction band below each mouth must see through again — proves the census
    // is actually watching the junctions (an always-green census is worthless).
    const std::vector<AuditTri> no_collar =
        collect_occluders(data, /*drop_trench=*/false,
                          /*drop_breach_collars=*/true);
    long mut_junction_leaks = 0;
    for (const EyeGrp& g : groups) {
        if (g.region != JUNCTION) continue;
        for (const glm::dvec3& tg : g.targets)
            if (ray_leaks(g.pos, tg, no_collar, net, hf, cuts, R))
                ++mut_junction_leaks;
    }
    WARN("audit mode 1 junction leaks with breach collars dropped: "
         << mut_junction_leaks);
    REQUIRE(mut_junction_leaks > 0);  // the census watches the junctions

    // -------- THE FOREVER TRIPWIRE --------
    // EXPECTED to fail right now if findings 3/4 are real — that failure is the
    // deliverable: the CENSUS HOLE lines above pinpoint the holes to seal.
    REQUIRE(total_leak_rays == 0);
}

// T27 — EXIT-CORRIDOR SCRAPE CENSUS (fly round-18 certification, Chad: "a
// ceiling of invisible collidable dirt coming out of Murray — I see a puff of
// smoke and lose like 70-90 m/s in a matter of a second or two"). The INVERSE
// class of the T26 leak census: T26 finds render-OPEN where collision is solid
// (see-through); this finds COLLISION-SOLID where NOTHING is rendered (invisible
// dirt).
//
// THE MECHANISM (attributed, single-source with the app path, NOT re-derived):
//  - app/instructor_tick.h + sim/step.cpp: OUTSIDE the net (net.contains ==
//    false) the kernel runs sim::ground_contact (sim/ground.h). It grabs the CG
//    the instant it drops below the terrain CONTACT surface
//       r_s = hf.radius_at(local_up) + contact_height_m.
//  - A grab shallower than deep_penetration_m (config 50 m) is graded a LANDING
//    (gentle sink — a climb-out has negative sink): ground_contact ZEROES the
//    radial velocity (a steep climb-out loses ~all its speed — the 70-90 m/s),
//    and, now on_ground, sim::ground_dynamics fires WING_STRIKE which
//    app/instructor_tick.h turns into wing damage + the smoke puff. No death
//    because a crash needs a penetration DEEPER than deep_penetration_m.
//  - The pit terrain heightfield still carries FULL surface height over the
//    excavation (T18): any point below r_s + outside the net is collidable —
//    HONEST where drawn rock is right there, DIRTY where the excavation reads
//    open with no rendered surface near. (T18 hunts only the DEEP/LETHAL band;
//    the shallow grab that scrapes-but-does-not-kill is this census's quarry.)
//
// THE PIN: fly realistic climb-out fans out of BOTH mouths (steep + shallow
// azimuth/elevation grids, lateral start spread across the throat). A path
// flies until it hits a drawn surface or escapes above grade; if it is GRABBED
// (a collidable point) first, in open drawn space, with NO rendered surface
// within kHonest_m of the grab, that is invisible collidable dirt. Cluster +
// report like T26. Mutation arm: drop the Murray-area render pieces — every
// exit-path grab goes naked (proves the census is watching the corridor AND the
// cover is load-bearing).
TEST_CASE("T27 exit-corridor scrape census: collidable-but-invisible = 0") {
    const world::TunnelParams tp = t16_ship_tp();
    // T27-REV: the census now flies the REAL Sudbury DEM — the SAME height
    // source the shipped kernel collides with (sim/ground.h ground_contact reads
    // env.ground->radius_at, built by render::planet_heightfield from THIS PNG).
    // The original T27 census ran a UNIFORM field, so the flat lip cap always
    // cleared the (constant) contact shell and the census could never see the
    // off-centre relief poke Chad flew into. Skip loudly if the asset is missing
    // (the mutation lever needs real relief to fire).
    const world::HeightField hf = load_real_dem();
    if (hf.w == 0) {
        WARN("sudbury_dem.png missing — T27-REV scrape census skipped");
        return;
    }
    const world::TunnelNet net = world::build_tunnel_net(tp, &hf);
    const double R = 15000.0;
    const double cell = render::terrain_cell_arc(200, 2, R);
    const double err_collar = render::collar_reach(net.pit.bowl_r, 200, 2, R);
    const double mur_collar = render::collar_reach(net.bowl.bowl_r, 200, 2, R);
    const render::TunnelMeshData data =
        render::build_tunnel_mesh(net, &hf, err_collar, mur_collar, cell);
    const std::vector<AuditTri> occ = collect_occluders(data, false, false);
    REQUIRE(occ.size() > 1000);
    const double pi = 3.14159265358979323846;

    // Report the terrain relief the grade-following cap must clear over each
    // footprint (evidence: how far the real DEM rises above the single centre
    // sample the OLD flat cap anchored to).
    const auto footprint_max_relief = [&](const world::TunnelNet::Bowl& bw) {
        if (bw.bowl_depth <= 0.0) return 0.0;
        glm::dvec3 tu = std::abs(bw.axis.z) < 0.9 ? glm::dvec3(0, 0, 1)
                                                  : glm::dvec3(1, 0, 0);
        tu = glm::normalize(tu - glm::dot(tu, bw.axis) * bw.axis);
        const glm::dvec3 tw = glm::normalize(glm::cross(bw.axis, tu));
        double m = 0.0;
        for (int ir = 1; ir <= 6; ++ir) {
            const double ang = (bw.bowl_r * ir / 6.0) / R;
            for (int ia = 0; ia < 24; ++ia) {
                const double a = 2.0 * 3.14159265358979323846 * ia / 24.0;
                const glm::dvec3 lat = std::cos(a) * tu + std::sin(a) * tw;
                const glm::dvec3 dir = glm::normalize(std::cos(ang) * bw.axis +
                                                      std::sin(ang) * lat);
                m = std::max(m, hf.radius_at(dir) - bw.surface_r);
            }
        }
        return m;
    };
    WARN("T27-REV footprint relief above centre surface_r: bowl="
         << footprint_max_relief(net.bowl) << " m  pit="
         << footprint_max_relief(net.pit) << " m (the OLD flat 6 m cap could"
         << " not clear this off-centre)");

    // RENDER-UNCHANGED PROOF: the mouth lip cap AND the T27-REV grade-following
    // cap are COLLISION-ONLY. The render cut-swallow trim (render/tunnel_mesh
    // drop_cut_swallowed_tris) calls bowl_sd, so pin that the shipped mesh is
    // BIT-IDENTICAL to a cap-OFF build (lip_cap_m zeroed AND the DEM detached) —
    // the cap only bites ABOVE grade where no facet lives after the CutDisk
    // removal. If this ever moves, the (now grade-raised) cap has grown into
    // rendered geometry and the visual smoke would show it.
    {
        world::TunnelNet net_cap0 = net;
        net_cap0.bowl.lip_cap_m = 0.0;
        net_cap0.bowl.ground = nullptr;
        net_cap0.pit.lip_cap_m = 0.0;
        net_cap0.pit.ground = nullptr;
        for (int i = 0; i < world::TunnelNet::kTrenchSteps; ++i) {
            net_cap0.trench[i].lip_cap_m = 0.0;
            net_cap0.trench[i].ground = nullptr;
        }
        const render::TunnelMeshData d0 = render::build_tunnel_mesh(
            net_cap0, &hf, err_collar, mur_collar, cell);
        REQUIRE(d0.pieces.size() == data.pieces.size());
        std::size_t ni = 0, nv = 0, ni0 = 0, nv0 = 0;
        for (const render::TunnelPiece& p : data.pieces) {
            ni += p.indices.size();
            nv += p.positions.size();
        }
        for (const render::TunnelPiece& p : d0.pieces) {
            ni0 += p.indices.size();
            nv0 += p.positions.size();
        }
        INFO("mesh index/vert counts capON=(" << ni << "," << nv << ") capOFF=("
             << ni0 << "," << nv0 << ")");
        REQUIRE(ni == ni0);  // the cap moved NO rendered triangle
        REQUIRE(nv == nv0);
    }

    // config/game.toml [ground] — the grab surface + the crash/scrape split.
    constexpr double kContactHeight_m = 2.45;  // ground_contact r_s offset
    constexpr double kDeepPen_m = 50.0;        // shallow<=>LANDING(scrape) / deep<=>crash
    constexpr double kHonest_m = 30.0;         // T18 idiom: a drawn surface this near = honest
    constexpr double kAbove_m = 40.0;          // escaped to open air this far above grade

    // The SURFACE cut disks (pit / bowl / live trench steps) — where the DEM
    // terrain is DRAWN OPEN. SAME construction as the T16/T26 audits. OUTSIDE a
    // cut the surrounding DEM terrain surface is intact, front-facing SOLID
    // GROUND: a below-grade point there is covered by real (rendered) rock and a
    // climb-out could only reach it by punching through that drawn surface — an
    // HONEST collision, never invisible. So the census only counts grabs INSIDE
    // a cut (terrain removed => only tunnel render can cover). The occluder set
    // is the tunnel MESH only (the planet DEM sphere is not in `occ`), so this
    // cut gate is what keeps a plain terrain-rim grab from reading as invisible.
    const glm::dvec3 edir = glm::normalize(net.spine.front().pos);
    const glm::dvec3 mdir = glm::normalize(net.spine.back().pos);
    const double cutmargin = 1.5 * cell;
    std::vector<AuditCut> cuts;
    cuts.push_back({edir, net.pit.bowl_r + cutmargin});
    cuts.push_back({mdir, net.bowl.bowl_r + cutmargin});
    for (int i = 0; i < world::TunnelNet::kTrenchSteps; ++i)
        if (net.trench[i].bowl_depth > 0.0)
            cuts.push_back({net.trench[i].axis,
                            net.trench[i].bowl_r + cutmargin});

    // The active net for the containment test — SWAPPED between the shipped
    // (grade-following) net and the flat-cap BEFORE arm without rebuilding the
    // fan geometry (only the above-grade cap differs; the bore/throat geometry
    // the fan starts from is identical). The collision grab surface is
    // single-source with sim/ground.h ground_contact (r_s = hf.radius_at(up) +
    // contact_height_m): collidable == the kernel would engage terrain contact
    // here (land shallow, crash deep — either way a consequence with NO render,
    // inside a drawn-open cut, is dishonest).
    const world::TunnelNet* active = &net;
    const auto collidable = [&](const glm::dvec3& p) {
        const double r = std::max(glm::length(p), 1e-9);
        return glm::length(p) < hf.radius_at(p / r) + kContactHeight_m &&
               !active->contains(p) && in_any_cut(p / r, R, cuts);
    };
    const auto near_render = [&](const glm::dvec3& q,
                                 const std::vector<AuditTri>& o) {
        for (const AuditTri& tr : o)
            if (glm::length(tr.cen - q) - tr.rad < kHonest_m) return true;
        return false;
    };

    struct Find {
        glm::dvec3 pt;
        double depth;  // r_s - |p| at the grab (>0; <=kDeepPen_m == scrape)
        int murray;    // 1 == Murray mouth, 0 == Errington
    };
    // March a climb-out path from p0 along d: fly until a drawn surface (t_hit)
    // or escape above grade; if grabbed first with no drawn surface near, record.
    const auto march = [&](const glm::dvec3& p0, const glm::dvec3& d,
                           const std::vector<AuditTri>& o, int murray,
                           std::vector<Find>& out) {
        const double t_hit = nearest_hit(p0, d, o);
        const double s_end = std::min(t_hit, 1600.0);
        for (double s = 4.0; s < s_end; s += 4.0) {
            const glm::dvec3 q = p0 + s * d;
            const double r = glm::length(q);
            const glm::dvec3 up = q / std::max(r, 1e-9);
            const double surf = hf.radius_at(up);
            if (r > surf + kAbove_m) return;  // escaped to open air
            if (collidable(q)) {
                if (!near_render(q, o))
                    out.push_back({q, surf + kContactHeight_m - r, murray});
                return;  // the first collidable contact is the grab
            }
        }
    };

    // A climb-out fan at a mouth. TWO start families:
    //  (A) the bore/throat cross-section, spread over the TUBE (the pilot
    //      leaving the adit), and
    //  (B) T27-REV: the WHOLE BOWL exit cone — start points spread across the
    //      full opening radius at a few shallow depths below grade (the pilot
    //      climbing out "slightly RIGHT of centre", well off the tube axis where
    //      the DEM relief lives). Both fan outward over a dense elevation x
    //      azimuth grid (steep INCLUDED — Chad's line was steep/normal) plus the
    //      pure-radial straight-up line.
    const auto build_fan = [&](bool murray, std::vector<Find>& out,
                               const std::vector<AuditTri>& o) {
        const auto& sp = net.spine;
        const std::size_t nn = sp.size();
        const glm::dvec3 mouth = murray ? sp[nn - 1].pos : sp[0].pos;
        const glm::dvec3 t_out =
            murray ? glm::normalize(sp[nn - 1].pos - sp[nn - 2].pos)
                   : glm::normalize(sp[0].pos - sp[1].pos);
        const glm::dvec3 up = glm::normalize(mouth);
        glm::dvec3 fwd = t_out - glm::dot(t_out, up) * up;
        fwd = glm::length(fwd) > 1e-9 ? glm::normalize(fwd) : glm::dvec3(1, 0, 0);
        const glm::dvec3 side = glm::normalize(glm::cross(up, fwd));
        // PERF: a climb-out fan never leaves the mouth region, and near_render
        // only cares about cover NEAR the grab — so prefilter the ~thousands of
        // occluders to the local subset once per mouth (nearest_hit/near_render
        // are O(occ) per ray). Verdict-identical: any tri farther than this from
        // the mouth cannot be the nearest hit of a <=1.6 km bore-mouth ray nor
        // within kHonest of a mouth-region grab.
        const double local_reach =
            (murray ? net.bowl.bowl_r : net.pit.bowl_r) + 1700.0;
        std::vector<AuditTri> local;
        local.reserve(o.size());
        for (const AuditTri& tr : o)
            if (glm::length(tr.cen - mouth) < local_reach + tr.rad)
                local.push_back(tr);
        // Family (A): the last few spine nodes (inside the bore/throat) spread
        // across the tube cross-section AND a little in local up.
        std::vector<glm::dvec3> bases;
        bases.push_back(murray ? sp[nn - 1].pos : sp[0].pos);
        bases.push_back(murray ? sp[nn - 2].pos : sp[1].pos);
        if (nn >= 3) bases.push_back(murray ? sp[nn - 3].pos : sp[2].pos);
        if (murray && net.throat_on) bases.push_back(net.throat_a);
        const double halfw = 0.6 * tp.tube_width_m;
        const double halfh = 0.6 * tp.tube_height_m;
        std::vector<glm::dvec3> starts;
        for (const glm::dvec3& b : bases)
            for (double su : {-1.0, -0.5, 0.0, 0.5, 1.0})
                for (double vu : {-0.6, 0.0, 0.6}) {
                    glm::dvec3 s0 = b + su * halfw * side + vu * halfh * up;
                    // Nudge onto the flyable side of the boundary (starts must
                    // begin inside the net, else a start already in rock
                    // fabricates a grab from its own cell).
                    for (int k = 0; k < 8 && !net.contains(s0); ++k)
                        s0 = s0 + 0.3 * (b - s0);
                    if (net.contains(s0)) starts.push_back(s0);
                }
        // Family (B): the WHOLE bowl exit cone. Sample the opening disk about
        // the mouth axis across radii out to ~0.85 * bowl_r, at a few shallow
        // depths below grade (30/70/130 m). Each candidate is nudged DOWN the
        // local up until it is inside the net (the open cut), so a start always
        // begins in flyable air. This is the "offsets across the full bowl
        // width, both directions" coverage the pilot's path demands.
        const world::TunnelNet::Bowl& bw = murray ? net.bowl : net.pit;
        if (bw.bowl_depth > 0.0) {
            const glm::dvec3 bax = bw.axis;
            glm::dvec3 bu = std::abs(bax.z) < 0.9 ? glm::dvec3(0, 0, 1)
                                                  : glm::dvec3(1, 0, 0);
            bu = glm::normalize(bu - glm::dot(bu, bax) * bax);
            const glm::dvec3 bw2 = glm::normalize(glm::cross(bax, bu));
            for (double rr = 0.2 * bw.bowl_r; rr <= 0.85 * bw.bowl_r;
                 rr += 0.2 * bw.bowl_r)
                for (int ia = 0; ia < 12; ++ia) {
                    const double a = 2.0 * pi * ia / 12.0;
                    const glm::dvec3 lat =
                        std::cos(a) * bu + std::sin(a) * bw2;
                    for (double dep : {40.0, 110.0}) {
                        if (dep >= bw.bowl_depth) continue;
                        glm::dvec3 s0 = bax * (bw.surface_r - dep) + rr * lat;
                        // Sink toward the axis floor until inside the open cut.
                        const glm::dvec3 floor =
                            bax * (bw.surface_r - bw.bowl_depth + 5.0);
                        for (int k = 0; k < 10 && !net.contains(s0); ++k)
                            s0 = s0 + 0.25 * (floor - s0);
                        if (net.contains(s0)) starts.push_back(s0);
                    }
                }
        }
        // Directions: elevation (climb) x azimuth about fwd, plus radial-up.
        for (const glm::dvec3& s0 : starts) {
            for (double el_deg : {5.0, 10.0, 16.0, 22.0, 30.0, 40.0, 52.0, 66.0,
                                  80.0, 88.0})
                for (double az_deg :
                     {-40.0, -24.0, -12.0, 0.0, 12.0, 24.0, 40.0}) {
                    const double el = el_deg * pi / 180.0;
                    const double az = az_deg * pi / 180.0;
                    const glm::dvec3 h =
                        std::cos(az) * fwd + std::sin(az) * side;
                    const glm::dvec3 d =
                        glm::normalize(std::cos(el) * h + std::sin(el) * up);
                    march(s0, d, local, murray ? 1 : 0, out);
                }
            march(s0, up, local, murray ? 1 : 0, out);  // straight up
        }
    };

    // A greedy 3D cluster + report of a finds vector (like T26). Reused for the
    // BEFORE (flat-cap) and AFTER (shipped) arms.
    constexpr double kClusterMerge_m = 100.0;
    struct SCluster {
        glm::dvec3 cen;
        long n;
        double max_depth;
        int murray;
    };
    const auto report = [&](const char* tag, const std::vector<Find>& finds) {
        std::vector<SCluster> clusters;
        for (const Find& f : finds) {
            int best = -1;
            double bd = kClusterMerge_m;
            for (std::size_t i = 0; i < clusters.size(); ++i) {
                const double dd = glm::length(f.pt - clusters[i].cen);
                if (dd < bd) { bd = dd; best = static_cast<int>(i); }
            }
            if (best < 0) {
                clusters.push_back({f.pt, 1, f.depth, f.murray});
            } else {
                SCluster& c = clusters[best];
                c.cen = (c.cen * double(c.n) + f.pt) / double(c.n + 1);
                c.n++;
                c.max_depth = std::max(c.max_depth, f.depth);
            }
        }
        WARN(tag << " T27-REV SCRAPE CENSUS (real DEM): grab-rays="
                 << finds.size() << " holes=" << clusters.size());
        for (std::size_t k = 0; k < clusters.size(); ++k) {
            const SCluster& c = clusters[k];
            const glm::dvec3& bwaxis = c.murray ? net.bowl.axis : net.pit.axis;
            const double axr = c.murray ? net.bowl.bowl_r : net.pit.bowl_r;
            const double arc_axis =
                std::acos(glm::clamp(glm::dot(glm::normalize(c.cen), bwaxis),
                                     -1.0, 1.0)) *
                R;
            int nk = -1;
            double nd = 1e30;
            for (const AuditTri& tr : occ) {
                const double dd = glm::length(tr.cen - c.cen) - tr.rad;
                if (dd < nd) { nd = dd; nk = tr.kind; }
            }
            char line[360];
            std::snprintf(
                line, sizeof(line),
                "%s HOLE %zu: %s rays=%ld world=(%.1f,%.1f,%.1f) |p|=%.1f "
                "depth=%.1f(%s) arc_axis=%.1f cut_r=%.1f nearest=%s@%.1fm",
                tag, k, c.murray ? "MURRAY" : "ERRINGTON", c.n, c.cen.x, c.cen.y,
                c.cen.z, glm::length(c.cen), c.max_depth,
                c.max_depth <= kDeepPen_m ? "SCRAPE" : "lethal", arc_axis, axr,
                kPieceName(nk), nd);
            WARN(line);
        }
    };

    // -------- BEFORE ARM (the MUTATION LEVER): the flat lip cap only. Zero the
    // T27-REV grade addition on every open cut and re-fly the SAME fans against
    // the SAME shipped render. With the cap flat at grade+lip_cap_m, the real
    // DEM relief pokes the contact shell above it off-centre — the round-18
    // ceiling reappears. This IS the evidence line + the regression lever: a
    // grade_cap regression is caught loudly. --------
    // The BEFORE arm is the ACTUAL pre-fix shipped behaviour: the flat lip cap
    // (6 m) anchored to the single centre surface_r — detach the DEM so bowl_sd
    // falls back to the flat ceiling. The census `collidable` still reads the
    // real DEM contact shell, so the off-centre relief pokes through.
    world::TunnelNet net_flat = net;
    net_flat.bowl.ground = nullptr;
    net_flat.pit.ground = nullptr;
    for (int i = 0; i < world::TunnelNet::kTrenchSteps; ++i)
        net_flat.trench[i].ground = nullptr;
    std::vector<Find> before;
    active = &net_flat;
    build_fan(/*murray=*/true, before, occ);
    build_fan(/*murray=*/false, before, occ);
    report("audit mode 1 (flat cap / BEFORE)", before);
    REQUIRE(before.size() > 0);  // the census + the grade cap are load-bearing

    // -------- AFTER ARM (shipped grade-following cap): 0 scrapes. --------
    std::vector<Find> finds;
    active = &net;
    build_fan(/*murray=*/true, finds, occ);
    build_fan(/*murray=*/false, finds, occ);
    report("audit mode 0 (shipped / AFTER)", finds);
    long murray_finds = 0, err_finds = 0;
    for (const Find& f : finds) (f.murray ? murray_finds : err_finds)++;
    INFO("shipped invisible-dirt grabs: murray=" << murray_finds
         << " errington=" << err_finds);
    REQUIRE(finds.size() == 0);
}
