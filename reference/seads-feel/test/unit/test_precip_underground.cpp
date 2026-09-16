// ATMOSPHERE AS-1 — NO SNOW UNDERGROUND.
//
// Chad, 2026-09-12: "we do get more snow, but not in the big stope. I noticed
// it there." The W3 precip lattice is world-anchored around the EYE and knows
// nothing about rock overhead, so the Murray/Errington arena snowed indoors.
//
// The fix gates each flake on TWO numbers at the FLAKE's own position: the
// tunnel net's signed distance, and its height above the local terrain. Both,
// not either:
//
//   - Per-flake (not per-eye) because standing in a mouth looking out, the snow
//     OUTSIDE must still fall while the flakes under the rock are gone. A
//     per-eye gate cannot express that.
//   - Terrain as well as SDF because world::TunnelNet::signed_distance reads
//     NEGATIVE for a band of open air ABOVE the ground near the shallow arena.
//     MEASURED at the default spawn, 2026-09-12: eye_sd = alt - 101 m, so every
//     eye below ~98 m AGL — the whole of a sled ride — reports "inside the net".
//     Gating on the SDF alone deleted the snow at exactly the altitude Chad
//     drives at; the --smoke shot at 23 m AGL showed open sky, bare ground and
//     not one flake. A flake ABOVE the terrain has no rock over it, full stop.
//
// This file pins the pure half (render::precip_rock_alpha /
// render::precip_rock_mode). The SDF is injected as a plain callable, so none
// of this needs a real world::TunnelNet — which is exactly why the headless
// gate can pin it at all.

#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <cstdint>
#include <glm/glm.hpp>

#include "render/precip.h"
#include "world/heightfield.h"
#include "world/tunnel_geo.h"
#include "world/tunnel_net.h"

using render::precip_rock_alpha;
using render::precip_rock_mode;
using render::PrecipRockMode;

namespace {
constexpr double kBand = 2.0;
constexpr double kDeepAbove = 500.0;   // well above the terrain
constexpr double kDeepBelow = -500.0;  // well under the terrain
}  // namespace

TEST_CASE("precip rock: a flake under rock is GONE, a flake outside is UNTOUCHED") {
    // The bug and its fix, in two lines. sd < 0 == inside the rock-enclosed
    // void (world::TunnelNet::signed_distance's contract, the same one
    // app::inside_tunnel reads); alt < 0 == under the terrain.
    CHECK(precip_rock_alpha(-10.0, kDeepBelow, kBand) == 0.0);  // in the stope
    CHECK(precip_rock_alpha(10.0, kDeepBelow, kBand) == 1.0);   // solid rock side
    CHECK(precip_rock_alpha(10.0, kDeepAbove, kBand) == 1.0);   // open sky
    // And far past the band on both sides, exactly 0 / exactly 1 — no residue.
    CHECK(precip_rock_alpha(-1000.0, kDeepBelow, kBand) == 0.0);
    CHECK(precip_rock_alpha(1000.0, kDeepBelow, kBand) == 1.0);
}

TEST_CASE("precip rock: a flake ABOVE THE TERRAIN always falls (the regression)") {
    // THE MEASURED TRAP. The tunnel SDF alone says "inside" for ordinary open
    // air over the shallow arena: at the default spawn every eye below ~98 m AGL
    // reports sd < 0. If that were the whole gate, a sled ride would have no
    // snow at all. Above the terrain, no signed distance may kill a flake.
    for (double sd : {-2000.0, -100.0, -10.0, -0.1, 0.0, 10.0, 2000.0})
        CHECK(precip_rock_alpha(sd, kDeepAbove, kBand) == 1.0);
    // Even right at the surface the flake survives as soon as it is a band
    // above it.
    CHECK(precip_rock_alpha(-1000.0, kBand, kBand) == 1.0);
}

TEST_CASE("precip rock: the band is monotone and bounded (a C0 walk out the mouth)") {
    // Walking from inside the rock out through the mouth, the flake alpha rises
    // monotonically from 0 to 1 with no step. A non-monotone or clamped-hard
    // gate would pop the snow on as you cross the wall. Held under the terrain,
    // so the SDF term is the one being measured.
    double prev = -1.0;
    for (int i = 0; i <= 400; ++i) {
        const double sd = -2.0 * kBand + (4.0 * kBand) * (i / 400.0);
        const double a = precip_rock_alpha(sd, kDeepBelow, kBand);
        REQUIRE(a >= 0.0);
        REQUIRE(a <= 1.0);
        REQUIRE(a >= prev - 1e-12);  // non-decreasing
        prev = a;
    }
    CHECK(precip_rock_alpha(0.0, kDeepBelow, kBand) == Catch::Approx(0.5));
    CHECK(precip_rock_alpha(-kBand, kDeepBelow, kBand) == 0.0);
    CHECK(precip_rock_alpha(kBand, kDeepBelow, kBand) == 1.0);
    // The TERRAIN term is C0-monotone in exactly the same way: rising out of
    // the ground inside the net fades the flake back in without a pop.
    prev = -1.0;
    for (int i = 0; i <= 400; ++i) {
        const double alt = -2.0 * kBand + (4.0 * kBand) * (i / 400.0);
        const double a = precip_rock_alpha(-1000.0, alt, kBand);
        REQUIRE(a >= prev - 1e-12);
        prev = a;
    }
}

TEST_CASE("precip rock: band <= 0 degenerates to a hard step, never to a no-op") {
    // A zero band must still GATE (the bug must stay fixed with the softening
    // dialled out); it must not silently become "always 1".
    CHECK(precip_rock_alpha(-0.001, -0.001, 0.0) == 0.0);
    CHECK(precip_rock_alpha(0.001, -0.001, 0.0) == 1.0);
    CHECK(precip_rock_alpha(-0.001, 0.001, 0.0) == 1.0);
}

TEST_CASE("precip rock: the whole-box mode picks the CHEAP path outside the net") {
    // The per-frame cost decision. One SDF call at the eye; the (2H+1)^3
    // per-flake scan runs ONLY where the box actually straddles rock.
    const double box = 21.0;
    const double reach = box + kBand;
    // Flying the open world, far outside the net => no per-flake test at all.
    CHECK(precip_rock_mode(500.0, 500.0, box, kBand) ==
          PrecipRockMode::AllOutside);
    CHECK(precip_rock_mode(reach + 0.001, -500.0, box, kBand) ==
          PrecipRockMode::AllOutside);
    // HIGH ABOVE THE TERRAIN but "inside" the net by the SDF (the shallow-arena
    // case): still no test, because nothing in the box can be buried. This
    // clause is what keeps a normal flight — and a sled ride — free.
    CHECK(precip_rock_mode(-2000.0, reach + 0.001, box, kBand) ==
          PrecipRockMode::AllOutside);
    // Deep in the stope: inside the net AND under the terrain => skip the draw.
    CHECK(precip_rock_mode(-500.0, -500.0, box, kBand) ==
          PrecipRockMode::AllInside);
    // Under the terrain but OUTSIDE the net (solid rock, or a deep valley floor
    // nowhere near the tunnel) is NOT AllInside — the box may still hold sky.
    CHECK(precip_rock_mode(reach + 1.0, -500.0, box, kBand) ==
          PrecipRockMode::AllOutside);
    // In the mouth: straddling => the per-flake gate runs. Both sides of zero.
    CHECK(precip_rock_mode(0.0, 0.0, box, kBand) == PrecipRockMode::MouthBand);
    CHECK(precip_rock_mode(box, 0.0, box, kBand) == PrecipRockMode::MouthBand);
    CHECK(precip_rock_mode(-box, -box, box, kBand) == PrecipRockMode::MouthBand);
}

TEST_CASE("precip rock: the box reach is CONSERVATIVE (no flake escapes the test)") {
    // The cheap paths are only sound if the reach they compare against covers
    // every flake the box can hold PLUS the band. A mutant that drops the band
    // from the reach would leave a sliver of un-tested flakes at the box edge.
    const double box = 70.0;  // the AS-3 far veil, the worst case
    for (int i = 0; i <= 200; ++i) {
        const double d = -(box + kBand) + (2.0 * (box + kBand)) * (i / 200.0);
        REQUIRE(precip_rock_mode(d, d, box, kBand) == PrecipRockMode::MouthBand);
    }
}

TEST_CASE("precip rock: a NULL net is identity (the gate is opt-in)") {
    // The renderer's contract: rock_sdf == nullptr => no gate, and ground_r <= 0
    // (no terrain reference) => the altitude term is inert. There is nothing to
    // call here, so the invariant is pinned where it lives: with "no rock
    // anywhere" numbers the SAME arithmetic is the identity, so the gated and
    // ungated branches cannot fork.
    const double far_outside = 1.0e9;
    CHECK(precip_rock_alpha(far_outside, far_outside, kBand) == 1.0);
    CHECK(precip_rock_mode(far_outside, far_outside, 21.0, kBand) ==
          PrecipRockMode::AllOutside);
    // ...and the renderer's "no terrain reference" sentinel (a huge alt) can
    // never delete a flake, whatever the SDF says.
    CHECK(precip_rock_alpha(-1.0e9, 1.0e12, kBand) == 1.0);
}

// ---------------------------------------------------------------------------
// THE ROOFED SDF (red-team P1-1). The gate must ask "is there rock ABOVE this
// flake", and world::TunnelNet::signed_distance cannot answer that: it mins in
// the Murray bowl, the Errington entry pit and the approach trench, which are
// OPEN CUTS -- sky above, by construction. Gating on it deleted the snowfall
// inside the open pits, which main draws. world::TunnelNet::
// roofed_signed_distance is the same union MINUS those three families;
// signed_distance / contains / app::inside_tunnel are untouched.
// ---------------------------------------------------------------------------

namespace {

// The T1 canon fixture, verbatim from test/unit/test_tunnel.cpp (the numbers are
// that file's own test dials, not game.toml -- graded against the inputs they
// read).
world::TunnelParams roofed_tp(double sphere_R) {
    world::TunnelParams tp;
    tp.sphere_R = sphere_R;
    tp.tube_width_m = 110.0;
    tp.tube_height_m = 90.0;
    tp.depth_m = 1600.0;
    tp.soft_m = 40.0;
    tp.ramp_frac = 0.3;
    tp.spacing_m = 150.0;
    tp.floor_height_m = 0.0;
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
    tp.chambers_on = true;
    tp.bowl_radius_m = 450.0;
    tp.bowl_depth_m = 300.0;
    tp.mouth_sink_m = 130.0;
    tp.min_cover_m = 60.0;
    tp.trench_len_m = 450.0;
    tp.trench_rim_m = 150.0;
    return tp;
}

world::HeightField roofed_field(double sphere_R, double elev_m) {
    world::HeightField hf;
    hf.w = 8;
    hf.h = 4;
    hf.R = sphere_R;
    hf.relief_scale = 4000.0;
    hf.u_offset = 0.0;
    const double f = std::min(std::max(elev_m / hf.relief_scale, 0.0), 1.0);
    hf.px.assign(static_cast<std::size_t>(hf.w) * hf.h,
                 static_cast<std::uint16_t>(f * 65535.0 + 0.5));
    return hf;
}

}  // namespace

TEST_CASE("precip rock: an OPEN PIT keeps its snow (the roofed SDF)") {
    // THE REGRESSION THIS CLOSES. A pilot 100 m down inside the open Murray
    // bowl is under open sky; main snows on him. The full SDF says "inside the
    // net" there, so the first cut of AS-1 deleted that snow.
    const double R = 15000.0;
    const world::HeightField hf = roofed_field(R, 300.0);
    const world::TunnelNet net = world::build_tunnel_net(roofed_tp(R), &hf);
    const glm::dvec3 murray = glm::normalize(world::kTunnelMouthMurray);
    const double surf = hf.radius_at(murray);

    // Sweep down the bowl axis from the rim. SOMEWHERE in there the full SDF is
    // negative (we are in the cut) while the roofed SDF is positive (nothing
    // over us). That is the case main draws and AS-1 must keep drawing.
    int open_cut_samples = 0;
    for (int i = 1; i <= 24; ++i) {
        const double drop = 10.0 * i;  // 10..240 m below the local surface
        const glm::dvec3 p = murray * (surf - drop);
        const double full = net.signed_distance(p);
        const double roofed = net.roofed_signed_distance(p);
        if (full < 0.0 && roofed > 0.0) {
            ++open_cut_samples;
            // The gate keeps it: below the terrain, but nothing roofed over it.
            CHECK(precip_rock_alpha(roofed, -drop, kBand) == 1.0);
            // ...and the OLD gate would have killed it. This line is the
            // regression itself, written down.
            CHECK(precip_rock_alpha(full, -drop, kBand) == 0.0);
        }
    }
    REQUIRE(open_cut_samples > 0);  // the fixture really does contain open cut
}

TEST_CASE("precip rock: the ARENA still kills its snow (the roofed SDF bites)") {
    // The other half: removing the open cuts must not remove the fix. Deep in
    // the sealed arena both SDFs are negative and the flake dies.
    const double R = 15000.0;
    const world::HeightField hf = roofed_field(R, 300.0);
    const world::TunnelNet net = world::build_tunnel_net(roofed_tp(R), &hf);
    REQUIRE(net.arena_on);
    const glm::dvec3 centre = net.arena.center;
    const double alt = glm::length(centre) - hf.radius_at(glm::normalize(centre));
    REQUIRE(alt < 0.0);  // the arena is under the terrain, as it must be
    const double roofed = net.roofed_signed_distance(centre);
    CHECK(roofed < 0.0);
    CHECK(precip_rock_alpha(roofed, alt, kBand) == 0.0);
    // And the whole-box decision skips the pass outright down there.
    CHECK(precip_rock_mode(roofed, alt, 70.0, kBand) ==
          PrecipRockMode::AllInside);
}

TEST_CASE("precip rock: the roofed SDF never claims MORE volume than the full one") {
    // roofed is a min over a SUBSET of the same primitives, so it can only be
    // greater or equal everywhere. A mutant that drops a roofed primitive (the
    // arena, say) would still pass that -- the arena case above is what catches
    // it -- but an inverted sense or a stray extra term fails here.
    const double R = 15000.0;
    const world::HeightField hf = roofed_field(R, 300.0);
    const world::TunnelNet net = world::build_tunnel_net(roofed_tp(R), &hf);
    const glm::dvec3 e = glm::normalize(world::kTunnelMouthErrington);
    const glm::dvec3 m = glm::normalize(world::kTunnelMouthMurray);
    for (int i = 0; i <= 60; ++i) {
        const double t = i / 60.0;
        const glm::dvec3 dir = glm::normalize(e * (1.0 - t) + m * t);
        for (double drop : {-500.0, 0.0, 200.0, 800.0, 1600.0, 3000.0}) {
            const glm::dvec3 p = dir * (hf.radius_at(dir) - drop);
            REQUIRE(net.roofed_signed_distance(p) >=
                    net.signed_distance(p) - 1e-9);
        }
    }
}
