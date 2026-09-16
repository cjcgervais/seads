// Inc 3 (landing-ready data): compile + pin the GENERATED Greater Sudbury GIS
// table (render/sudbury_gis.gen.h — offline_tool/build_sudbury.py). The header
// is otherwise #included by nothing, so without this the green gate is BLIND to
// whether it even compiles (the "gate is blind to the app binary" class). This
// also pins the INERT landing data the future kernel will query: unit surface
// directions, radial + orthonormal heading-free airstrip bases (from mapped
// runway endpoints, Fable fix #4), and post-remap theatrical elevations
// (radius from center; Fable fix #3). Pure data — no sim/ or control/ touch.

#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "render/building_asset.h"  // shipped .bin parser (shared with render/buildings.cpp)
#include "render/ribbon_clip.h"  // T24 excavation clip (pure, pinned here)
#include "render/ribbon_subdiv.h"  // ★ ROAD-REPAIR: the subdivided drape
#include "render/sudbury_gis.gen.h"
#include "render/tunnel_mesh.h"  // kMouthCutFactor (the cut-radius factor)
#include "world/tunnel_geo.h"    // the two mouth directions
#include "world/tunnel_net.h"    // cut-radius + trench single-source helpers

#ifdef NDEBUG
#error "SEADS gate requires an assert-live build (SPEC 6.1)"
#endif

namespace {
constexpr double kR = 15000.0;  // planet radius (config/aircraft.toml world.R)
constexpr double kReliefCeil =
    2000.0;  // generous theatrical ceiling (relief_scale ~600)

double len(const double v[3]) {
    return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}
double dot3(const double a[3], const double b[3]) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}
void cross3(const double a[3], const double b[3], double out[3]) {
    out[0] = a[1] * b[2] - a[2] * b[1];
    out[1] = a[2] * b[0] - a[0] * b[2];
    out[2] = a[0] * b[1] - a[1] * b[0];
}
bool name_present(const char* needle) {
    for (std::size_t i = 0; i < render::kSudburyLakeCount; ++i) {
        std::string n = render::kSudburyLakes[i].name;
        for (auto& c : n) c = static_cast<char>(std::tolower(c));
        if (n.find(needle) != std::string::npos) return true;
    }
    return false;
}
}  // namespace

TEST_CASE("sudbury_gis: the lake table is well-formed inert data") {
    REQUIRE(render::kSudburyLakeCount >
            100);  // 300+ region; ~250 named emitted
    int landable = 0;
    for (std::size_t i = 0; i < render::kSudburyLakeCount; ++i) {
        const render::GisLake& l = render::kSudburyLakes[i];
        INFO("lake " << l.name);
        CHECK(len(l.dir) ==
              Catch::Approx(1.0).margin(1e-6));  // unit surface dir
        CHECK(l.elev_m >= kR - 1.0);             // at/above planet sea level
        CHECK(l.elev_m <= kR + kReliefCeil);     // within theatrical relief
        CHECK(l.span_m > 0.0);
        if (l.landable) ++landable;
    }
    CHECK(landable > 0);  // at least the named float-plane targets
}

TEST_CASE("sudbury_gis: hero lakes survived the disk crop") {
    // S0 (1:1 aeqd): the disk terminates at theta_edge=R_MAX/R_PLANET
    // (~107deg), NOT pi*R. Ramsey + Whitewater sit near the recentered midpoint
    // (deep in the real disk). Wanapitei is far east — post-recenter it falls
    // toward the wilderness cap (Fable ruling B: far lakes blend to
    // wilderness), but remains a named LABEL dir in the table, so its presence
    // still holds here.
    CHECK(name_present("ramsey"));
    CHECK(name_present("whitewater"));
    CHECK(name_present("wanapitei"));
}

TEST_CASE("sudbury_gis: dedicated water surfaces are well-formed meshes") {
    // The lake mirror-mesh data (render/water_surface.* consumes it): unit
    // sphere dirs + LOCAL 0-based triangle indices per lake, wound
    // CCW-from-outside (radial-out normal). This leg is INERT until the bake
    // emits lakes and fires once it does — the "keyed off the presence of the
    // artifact" gate. It pins the Fable-BEFORE ★ math the green gate is
    // otherwise blind to (unit dirs, index-in-range, outward winding).
    REQUIRE(render::kSudburyWaterLakeCount >
            0);  // Wanapitei/Whitewater/... emitted
    std::size_t tris_checked = 0;
    bool hero = false;
    for (std::size_t li = 0; li < render::kSudburyWaterLakeCount; ++li) {
        const render::GisWaterLake& L = render::kSudburyWaterLakes[li];
        INFO("water lake " << L.name);
        std::string n = L.name;
        for (auto& c : n) c = static_cast<char>(std::tolower(c));
        if (n.find("whitewater") != std::string::npos ||
            n.find("wanapitei") != std::string::npos)
            hero = true;
        CHECK(L.elev_m >= kR - 1.0);
        CHECK(L.elev_m <= kR + kReliefCeil);
        REQUIRE(L.vtx_count >= 3);
        REQUIRE(L.vtx_count < 65536);  // runtime builds a ushort mesh per lake
        REQUIRE(L.idx_count >= 3);
        REQUIRE(L.idx_count % 3 == 0);
        // ranges land inside the global pools
        REQUIRE(L.vtx_off >= 0);
        REQUIRE(L.idx_off >= 0);
        REQUIRE(static_cast<std::size_t>(L.vtx_off + L.vtx_count) <=
                render::kSudburyWaterVertCount);
        REQUIRE(static_cast<std::size_t>(L.idx_off + L.idx_count) <=
                render::kSudburyWaterIndexCount);
        for (int t = 0; t < L.idx_count; t += 3) {
            const unsigned short ia =
                render::kSudburyWaterIndices[L.idx_off + t];
            const unsigned short ib =
                render::kSudburyWaterIndices[L.idx_off + t + 1];
            const unsigned short ic =
                render::kSudburyWaterIndices[L.idx_off + t + 2];
            // LOCAL indices, in this lake's vertex range
            REQUIRE(ia < L.vtx_count);
            REQUIRE(ib < L.vtx_count);
            REQUIRE(ic < L.vtx_count);
            const double* pa = render::kSudburyWaterVerts[L.vtx_off + ia].dir;
            const double* pb = render::kSudburyWaterVerts[L.vtx_off + ib].dir;
            const double* pc = render::kSudburyWaterVerts[L.vtx_off + ic].dir;
            // every vertex is a unit surface direction
            CHECK(len(pa) == Catch::Approx(1.0).margin(1e-6));
            // outward winding (Fable ★2): the face normal cross(pb-pa, pc-pa)
            // must point the SAME way as the outward radial (the triangle
            // centroid), i.e. dot > 0. A CW (inward) triangle would cull to a
            // vanished lake at runtime — this catches it in the gate.
            const double e1[3] = {pb[0] - pa[0], pb[1] - pa[1], pb[2] - pa[2]};
            const double e2[3] = {pc[0] - pa[0], pc[1] - pa[1], pc[2] - pa[2]};
            double nrm[3];
            cross3(e1, e2, nrm);
            const double cen[3] = {pa[0] + pb[0] + pc[0], pa[1] + pb[1] + pc[1],
                                   pa[2] + pb[2] + pc[2]};
            CHECK(dot3(nrm, cen) > 0.0);
            ++tris_checked;
        }
    }
    CHECK(tris_checked * 3 == render::kSudburyWaterIndexCount);
    CHECK(hero);  // at least one named hero lake got a surface
}

TEST_CASE("sudbury_gis: draped ribbon paths are well-formed strips") {
    // S3 linework (render/ribbons.* consumes it): per-path unit sphere dirs +
    // arc-length s + transverse v = -1(L)/+1(R) + LOCAL 0-based triangle
    // indices, wound CCW-from-outside (radial-out normal). Pins the
    // Fable-BEFORE ★ math the green gate is blind to; fires because the bake
    // emitted paths (inert if none).
    REQUIRE(render::kSudburyRibbonPathCount > 0);
    std::size_t tris_checked = 0;
    bool has_trail = false;
    bool has_river = false;
    for (std::size_t pi = 0; pi < render::kSudburyRibbonPathCount; ++pi) {
        const render::GisRibbonPath& P = render::kSudburyRibbonPaths[pi];
        INFO("ribbon path kind " << P.kind);
        CHECK((P.kind == 0 || P.kind == 1 || P.kind == 2 ||
               P.kind == 3));  // major/minor/trail/river
        if (P.kind == 2) has_trail = true;
        if (P.kind == 3) has_river = true;
        REQUIRE(P.vtx_count >= 3);
        REQUIRE(P.vtx_count < 65536);  // runtime builds a ushort mesh per batch
        REQUIRE(P.idx_count >= 3);
        REQUIRE(P.idx_count % 3 == 0);
        REQUIRE(P.vtx_off >= 0);
        REQUIRE(P.idx_off >= 0);
        REQUIRE(static_cast<std::size_t>(P.vtx_off + P.vtx_count) <=
                render::kSudburyRibbonVertCount);
        REQUIRE(static_cast<std::size_t>(P.idx_off + P.idx_count) <=
                render::kSudburyRibbonIndexCount);
        // every vertex is a unit dir + a valid transverse coord
        float smax = 0.0f;
        for (int i = 0; i < P.vtx_count; ++i) {
            const render::GisRibbonVertex& V =
                render::kSudburyRibbonVerts[P.vtx_off + i];
            CHECK(len(V.dir) == Catch::Approx(1.0).margin(1e-6));
            CHECK((V.v == Catch::Approx(-1.0).margin(1e-3) ||
                   V.v == Catch::Approx(1.0).margin(1e-3)));  // L or R edge
            CHECK(std::isfinite(V.s));
            CHECK(V.s >= 0.0f);  // cumulative arc-length, never negative
            if (V.s > smax) smax = V.s;
        }
        // The arc-length actually ACCUMULATES (fixture-no-op guard, Fable-AFTER
        // P2): an all-zero s would silently kill the FS dashes yet pass every
        // check above. Bounded by half the sphere circumference (pi*R).
        CHECK(smax > 0.0f);
        CHECK(smax < static_cast<float>(3.14159265358979 * kR));  // < pi*R
        for (int t = 0; t < P.idx_count; t += 3) {
            const unsigned short ia =
                render::kSudburyRibbonIndices[P.idx_off + t];
            const unsigned short ib =
                render::kSudburyRibbonIndices[P.idx_off + t + 1];
            const unsigned short ic =
                render::kSudburyRibbonIndices[P.idx_off + t + 2];
            REQUIRE(ia <
                    P.vtx_count);  // LOCAL indices, in this batch's vtx range
            REQUIRE(ib < P.vtx_count);
            REQUIRE(ic < P.vtx_count);
            const double* pa = render::kSudburyRibbonVerts[P.vtx_off + ia].dir;
            const double* pb = render::kSudburyRibbonVerts[P.vtx_off + ib].dir;
            const double* pc = render::kSudburyRibbonVerts[P.vtx_off + ic].dir;
            // OUTWARD WINDING (Fable-confirmed): cross(pb-pa, pc-pa) points the
            // same way as the outward radial (centroid), dot > 0. A CW triangle
            // would cull to an invisible strip.
            const double e1[3] = {pb[0] - pa[0], pb[1] - pa[1], pb[2] - pa[2]};
            const double e2[3] = {pc[0] - pa[0], pc[1] - pa[1], pc[2] - pa[2]};
            double nrm[3];
            cross3(e1, e2, nrm);
            const double cen[3] = {pa[0] + pb[0] + pc[0], pa[1] + pb[1] + pc[1],
                                   pa[2] + pb[2] + pc[2]};
            CHECK(dot3(nrm, cen) > 0.0);
            ++tris_checked;
        }
    }
    CHECK(tris_checked * 3 == render::kSudburyRibbonIndexCount);
    CHECK(has_trail);  // the sanctioned light-green snowmobile trail got baked
    CHECK(has_river);  // the S3 rivers (kind 3) got baked (waterway fetch
                       // reachable)
}

TEST_CASE("sudbury_gis: building massing batches are well-formed prisms") {
    // S4 massing (render/buildings.* consumes it): per-batch unit sphere dirs +
    // per-vertex height above terrain (h) + a per-building tone jitter (bj) +
    // LOCAL 0-based triangle indices. Buildings render CULL-OFF (the FS derives
    // + eye-flips a flat facet normal), so face winding is intentionally NOT
    // validated here: a wall's base and eave share a unit dir, so the
    // ribbon/water centroid winding test is degenerate/invalid for these
    // non-radial faces (Fable P0-2). Instead this pins STRUCTURE + a
    // not-a-no-op EXTRUSION tripwire the green gate is blind to. Fires because
    // the bake emitted batches (inert if none — a roads-only bake). The massing
    // now ships as the runtime binary asset assets/sudbury_buildings.bin (moved
    // out of the constexpr header so the whole-map fill scales). Load + parse
    // the SHIPPED bytes with the SAME pure parser the app uses
    // (render/building_asset.* — one source, no fork), so a stale/corrupt .bin
    // fails HERE, in the gate.
    const std::string bin_path =
        std::string(SEADS_ASSET_DIR) + "/sudbury_buildings.bin";
    std::ifstream bin(bin_path, std::ios::binary);
    REQUIRE(bin.good());
    const std::vector<unsigned char> bytes(
        (std::istreambuf_iterator<char>(bin)),
        std::istreambuf_iterator<char>());
    REQUIRE(bytes.size() > 64);
    const render::BuildingAsset A =
        render::parse_building_asset(bytes.data(), bytes.size());
    REQUIRE(A.ok);
    // The re-bake law: the .bin's lock hash MUST match the header the town dirs
    // + hero placements ride (a projection re-bake that forgets the .bin fails
    // here).
    REQUIRE(A.lock_hash == render::kSudburyProjectionLockHash);
    REQUIRE(A.batches.size() > 0);
    REQUIRE(A.verts.size() > 1000);  // a real town, not a stub
    std::size_t tris_checked = 0;
    float hmax = -1.0e9f, hmin = 1.0e9f;
    bool has_roof_tri =
        false;  // a tri with ALL verts above terrain = a cap/roof
    for (std::size_t bi = 0; bi < A.batches.size(); ++bi) {
        const render::GisBuildingBatch& B = A.batches[bi];
        // The last hero_batch_count batches are S5 hero landmarks (tall: the
        // 381 m Superstack); town batches keep the tight runaway-height bound
        // so a bad OSM tag can't ship a 350 m "building" (Fable S5a P1: one
        // wide bound over all batches would silently disarm the tripwire —
        // config-relative-bounds trap).
        const bool is_hero = bi >= A.batches.size() - A.hero_batch_count;
        const float h_ceiling =
            is_hero ? 400.0f : 80.0f;  // town <= H_MAX(60)+rise
        REQUIRE(B.vtx_count >= 3);
        REQUIRE(B.vtx_count < 65536);  // runtime builds a ushort mesh per batch
        REQUIRE(B.idx_count >= 3);
        REQUIRE(B.idx_count % 3 == 0);
        REQUIRE(B.vtx_off >= 0);
        REQUIRE(B.idx_off >= 0);
        REQUIRE(static_cast<std::size_t>(B.vtx_off + B.vtx_count) <=
                A.verts.size());
        REQUIRE(static_cast<std::size_t>(B.idx_off + B.idx_count) <=
                A.indices.size());
        for (int i = 0; i < B.vtx_count; ++i) {
            const render::GisBuildingVertex& V = A.verts[B.vtx_off + i];
            CHECK(len(V.dir) == Catch::Approx(1.0).margin(1e-6));  // unit dir
            CHECK(std::isfinite(V.h));
            CHECK(V.h > -5.0f);  // >= -(base sink); no runaway
            CHECK(V.h <
                  h_ceiling);  // town 80 m; hero batch 400 m (Superstack 381 m)
            CHECK(V.bj > 0.8f);  // per-building tone jitter ~1
            CHECK(V.bj < 1.2f);
            if (V.h > hmax) hmax = V.h;
            if (V.h < hmin) hmin = V.h;
        }
        for (int t = 0; t < B.idx_count; t += 3) {
            const unsigned short ia = A.indices[B.idx_off + t];
            const unsigned short ib = A.indices[B.idx_off + t + 1];
            const unsigned short ic = A.indices[B.idx_off + t + 2];
            REQUIRE(ia <
                    B.vtx_count);  // LOCAL indices, in this batch's vtx range
            REQUIRE(ib < B.vtx_count);
            REQUIRE(ic < B.vtx_count);
            const float ha = A.verts[B.vtx_off + ia].h;
            const float hb = A.verts[B.vtx_off + ib].h;
            const float hc = A.verts[B.vtx_off + ic].h;
            if (ha > 0.0f && hb > 0.0f && hc > 0.0f) has_roof_tri = true;
            ++tris_checked;
        }
    }
    CHECK(tris_checked * 3 == A.indices.size());
    // EXTRUSION tripwire (fixture-no-op guard): a real prism has an eave/roof
    // ABOVE the terrain (h>0) AND a base sunk BELOW it (h<0) — a flat
    // degenerate sheet would have neither, yet pass every structural check
    // above.
    CHECK(hmax > 0.0f);
    CHECK(hmin < 0.0f);
    // A cap/roof was actually emitted (Fable-AFTER P1-3): every WALL triangle
    // contains a base vertex (h<0), so a triangle with all three verts ABOVE
    // terrain can only be an eave cap or a roof face — pins that the
    // ear-clip/roof solid ran, which the global hmax>0 check alone (satisfied
    // by eave verts on wall quads) would miss.
    CHECK(has_roof_tri);

    // Stage-3 sim-neutral COLLISION INDEX: one record per kept town footprint
    // (heroes excluded). NO render consumer yet — this pins the SHIPPED COLL
    // section so a bake that drops it or ships garbage extents fails HERE
    // (future solid-ground/targeting, MASTER_PLAN Phase 1, depends on it). Unit
    // dir, positive bounded radius, finite h.
    REQUIRE(A.colliders.size() > 1000);
    for (const render::GisBuildingCollider& col : A.colliders) {
        CHECK(len(col.center_dir) == Catch::Approx(1.0).margin(1e-6));
        CHECK(col.radius_m > 0.0f);
        CHECK(col.radius_m < 2000.0f);  // no runaway footprint
        CHECK(std::isfinite(col.height_m));
        CHECK(col.height_m > -5.0f);  // >= -(base sink)
    }
}

TEST_CASE("sudbury_gis: street lamps are well-formed point lights") {
    // S4 night lamps (render/lights.* consumes it): a unit sphere dir + a lamp
    // height above terrain, drawn as additive billboard glows. Fires because
    // the bake emitted lamps (inert if none). Pins the placement the green gate
    // is blind to.
    REQUIRE(render::kSudburyLampCount >
            100);  // a real town street network, not a stub
    for (std::size_t i = 0; i < render::kSudburyLampCount; ++i) {
        const render::GisLightPoint& L = render::kSudburyLamps[i];
        CHECK(len(L.dir) == Catch::Approx(1.0).margin(1e-6));  // unit dir
        CHECK(std::isfinite(L.h));
        CHECK(L.h > 0.0f);   // above the terrain
        CHECK(L.h < 30.0f);  // a lamp, not a tower
    }
}

TEST_CASE("sudbury_gis: airstrip bases are radial + orthonormal") {
    REQUIRE(render::kSudburyAirstripCount >= 5);
    for (std::size_t i = 0; i < render::kSudburyAirstripCount; ++i) {
        const render::GisAirstrip& a = render::kSudburyAirstrips[i];
        INFO("airstrip " << a.name);
        // up == anchor == the radial normal (heading-free, Cartesian).
        CHECK(len(a.anchor) == Catch::Approx(1.0).margin(1e-6));
        CHECK(a.up[0] == Catch::Approx(a.anchor[0]).margin(1e-9));
        CHECK(a.up[1] == Catch::Approx(a.anchor[1]).margin(1e-9));
        CHECK(a.up[2] == Catch::Approx(a.anchor[2]).margin(1e-9));
        // fwd/right/up orthonormal (a heading-free tangent basis from the
        // MAPPED endpoints; a re-applied compass heading or a degenerate strip
        // fails here).
        CHECK(len(a.fwd) == Catch::Approx(1.0).margin(1e-6));
        CHECK(len(a.right) == Catch::Approx(1.0).margin(1e-6));
        CHECK(std::fabs(dot3(a.fwd, a.up)) < 1e-6);
        CHECK(std::fabs(dot3(a.fwd, a.right)) < 1e-6);
        CHECK(std::fabs(dot3(a.right, a.up)) < 1e-6);
        CHECK(a.elev_m >= kR - 1.0);
        CHECK(a.elev_m <= kR + kReliefCeil);
        CHECK(a.length_m > 0.0);
    }
}

// T24 — THE RIBBON EXCAVATION CLIP (fly round-16 tail, Chad on the Errington
// right flank: "there is the snowmachine trail and another line. There is
// almost overhang there"). The draped road/trail ribbons hovered across the
// Errington pit + trench cut voids — the terrain drops its triangles over the
// cuts, the ribbons did not. ribbon_indices_outside_cuts (render/ribbon_clip.h,
// the SHIPPED clip build_ribbon_surfaces routes through) must (a) be a
// bit-identical no-op with no cuts, (b) actually FIRE on the canonical tunnel
// cut list (a never-firing clip is theater), and (c) never keep a triangle
// with a vertex inside a cut (the terrain's own any-vertex rule).
TEST_CASE("T24 ribbon clip: cut-hovering triangles drop; no-cut arm verbatim") {
    // The canonical tunnel cut list, mirroring app/main.cpp's planet_cuts
    // through the SAME world:: single-source helpers (T1 canon dials — the
    // shared tunnel-test fixture values).
    const double tube_width = 110.0, bowl_radius = 450.0;
    const double trench_len = 450.0, trench_rim = 150.0, mouth_sink = 130.0;
    const double err_pit_rim = world::errington_pit_rim(tube_width);
    std::vector<render::CutDisk> cuts;
    cuts.push_back(
        {world::kTunnelMouthErrington,
         world::errington_cut_radius(err_pit_rim, render::kMouthCutFactor)});
    cuts.push_back(
        {world::kTunnelMouthMurray, world::murray_cut_radius(bowl_radius)});
    for (const world::TrenchStep& s : world::errington_trench_steps(
             world::kTunnelMouthErrington, world::kTunnelMouthMurray,
             trench_len, trench_rim, mouth_sink, kR))
        cuts.push_back({s.dir, s.rim_m});

    int total_baked = 0, total_kept = 0;
    for (std::size_t pi = 0; pi < render::kSudburyRibbonPathCount; ++pi) {
        const render::GisRibbonPath& P = render::kSudburyRibbonPaths[pi];
        if (P.vtx_count < 3 || P.idx_count < 3) continue;
        // (a) empty cuts == the baked list verbatim.
        const std::vector<unsigned short> noop =
            render::ribbon_indices_outside_cuts(pi, {}, kR);
        REQUIRE(noop.size() == static_cast<std::size_t>(P.idx_count));
        for (int k = 0; k < P.idx_count; ++k)
            REQUIRE(noop[static_cast<std::size_t>(k)] ==
                    render::kSudburyRibbonIndices[P.idx_off + k]);
        // (c) with the tunnel cuts: every kept triangle fully outside.
        const std::vector<unsigned short> kept =
            render::ribbon_indices_outside_cuts(pi, cuts, kR);
        REQUIRE(kept.size() % 3 == 0);
        for (std::size_t k = 0; k < kept.size(); ++k) {
            const render::GisRibbonVertex& V =
                render::kSudburyRibbonVerts[P.vtx_off + kept[k]];
            REQUIRE(!render::dir_in_any_cut(
                glm::dvec3(V.dir[0], V.dir[1], V.dir[2]), kR, cuts));
        }
        total_baked += P.idx_count;
        total_kept += static_cast<int>(kept.size());
    }
    // (b) the clip FIRES on the canon geometry: the Errington approach's
    // road/trail lines cross the pit + trench cuts (Chad flew under them),
    // so a nonzero triangle count must drop. A zero drop means the cut list
    // or the metric is broken and the clip is theater (mutation lever:
    // radius_m = 0 on every disk reproduces the no-op and FAILS here).
    REQUIRE(total_baked > 0);
    REQUIRE(total_kept < total_baked);
}

// T24-river — Chad's follow-up on the SAME excavation, same fly: "we cleaned
// up the snowmachine trail going over the opening, but there is another black
// line there — it's a road/trail that needs to be cut out". The line was a
// RIVER (kind 3, drawn as a black mirror by render/river_surfaces.cpp, NOT
// the road/trail ribbon FS) — build_ribbon_surfaces explicitly SKIPS kind==3
// (river_surfaces owns it), and river_surfaces.cpp never called
// ribbon_indices_outside_cuts at all, so the black mirror strip hovered
// straight across the Errington pit. The clip test above already proves the
// PURE function works correctly on every path kind including rivers (it
// never filters by kind) — this leg isolates kind==3 alone so a future
// re-fork of the river build can't silently drop the wiring again: (1) at
// least one kind==3 path exists (the bake emitted rivers at all — else this
// leg is vacuous), (2) the clip actually FIRES on a kind==3 path under the
// canonical tunnel cuts (mirrors (b) above, scoped to rivers — a
// river_surfaces.cpp that stopped calling ribbon_indices_outside_cuts, or
// called it with an empty cuts list, is caught the moment the fix regresses:
// the no-cuts no-op keeps every triangle, so kept_river == baked_river would
// pass ONLY if the clip were dead).
TEST_CASE("T24-river: the black-mirror river paths clip at the SAME excavation cuts") {
    const double tube_width = 110.0, bowl_radius = 450.0;
    const double trench_len = 450.0, trench_rim = 150.0, mouth_sink = 130.0;
    const double err_pit_rim = world::errington_pit_rim(tube_width);
    std::vector<render::CutDisk> cuts;
    cuts.push_back(
        {world::kTunnelMouthErrington,
         world::errington_cut_radius(err_pit_rim, render::kMouthCutFactor)});
    cuts.push_back(
        {world::kTunnelMouthMurray, world::murray_cut_radius(bowl_radius)});
    for (const world::TrenchStep& s : world::errington_trench_steps(
             world::kTunnelMouthErrington, world::kTunnelMouthMurray,
             trench_len, trench_rim, mouth_sink, kR))
        cuts.push_back({s.dir, s.rim_m});

    bool saw_river = false;
    int river_baked = 0, river_kept = 0;
    for (std::size_t pi = 0; pi < render::kSudburyRibbonPathCount; ++pi) {
        const render::GisRibbonPath& P = render::kSudburyRibbonPaths[pi];
        if (P.kind != 3) continue;  // rivers only — the class T24 skipped
        if (P.vtx_count < 3 || P.idx_count < 3) continue;
        saw_river = true;
        const std::vector<unsigned short> kept =
            render::ribbon_indices_outside_cuts(pi, cuts, kR);
        river_baked += P.idx_count;
        river_kept += static_cast<int>(kept.size());
    }
    REQUIRE(saw_river);  // else the bake emitted no rivers and this leg is moot
    REQUIRE(river_baked > 0);
    // The clip must FIRE on the river class specifically (not just in the
    // road/trail aggregate above) — this is the exact assertion that would
    // catch river_surfaces.cpp reverting to the unclipped raw index copy.
    REQUIRE(river_kept < river_baked);
}

// ===========================================================================
// ★ ROAD-REPAIR / ONAPING SINK — THE SUBDIVIDED DRAPE (render/ribbon_subdiv.h)
//
// Chad, 2026-09-10 fly: "a few spots near Onaping (Valley) pump I went into the
// road on snowmachine ... on a hill to the north of the pump in the middle of
// the road going up slightly inclined pavement."  The road census reported SINK
// 0 there, because it samples the ribbon FUNCTION at its own stations, where
// that function is the drawn deck EXACTLY.  The drawn deck between two baked
// rungs is a flat CHORD, and the baked rungs are 52.67 m apart at the median.
// build_ribbon_batches subdivides that chord at build time.
// ===========================================================================

namespace {

// A path with a real, densely-rung geometry to subdivide (path 0 is the
// road_major batch, 18,172 baked vertices).
const render::GisRibbonPath& subdiv_path() {
    return render::kSudburyRibbonPaths[0];
}

// The baked triangle list of a path, LOCAL 0-based — what the no-cut clip
// returns verbatim.
std::vector<unsigned short> baked_indices(const render::GisRibbonPath& P) {
    std::vector<unsigned short> ix;
    ix.reserve(static_cast<std::size_t>(P.idx_count));
    for (int k = 0; k < P.idx_count; ++k)
        ix.push_back(render::kSudburyRibbonIndices[P.idx_off + k]);
    return ix;
}

// A SMOOTH analytic ground with a real dip: a 1 km-wavelength sinusoid in the
// planet-local x direction, 40 m of amplitude.  Curved enough that a 50 m flat
// chord across it sags by metres, and smooth enough that the sag has to fall
// quadratically as the chord shortens.
double dip_radius(const glm::dvec3& d) {
    return kR + 40.0 * std::sin(d.x * kR * (2.0 * 3.14159265358979 / 1000.0));
}

// The worst positive sag of a batch list, measured the way app/main.cpp's
// SEADS_RIBBON_SAG ruler measures it: subsample every drawn chord and compare
// the chord's radius against the analytic ground it is draped on.
double worst_chord_sag(const std::vector<render::RibbonBatchCPU>& bs,
                       double lift) {
    double worst = 0.0;
    for (const render::RibbonBatchCPU& b : bs)
        for (std::size_t k = 0; k + 2 < b.idx.size(); k += 3)
            for (int e = 0; e < 3; ++e) {
                const unsigned short i0 = b.idx[k + e];
                const unsigned short i1 = b.idx[k + (e + 1) % 3];
                const glm::dvec3 A(b.pos[i0 * 3], b.pos[i0 * 3 + 1],
                                   b.pos[i0 * 3 + 2]);
                const glm::dvec3 B(b.pos[i1 * 3], b.pos[i1 * 3 + 1],
                                   b.pos[i1 * 3 + 2]);
                for (int s = 1; s < 8; ++s) {
                    const glm::dvec3 Pm = A + (B - A) * (s / 8.0);
                    const double r = glm::length(Pm);
                    const double g = dip_radius(Pm / r) + lift;
                    if (r - g > worst) worst = r - g;
                }
            }
    return worst;
}

}  // namespace

TEST_CASE("ribbon subdiv: max_seg_m 0 is the byte-identical identity") {
    const render::GisRibbonPath& P = subdiv_path();
    const std::vector<unsigned short> kept = baked_indices(P);
    const std::vector<render::RibbonBatchCPU> bs = render::build_ribbon_batches(
        P, kept, dip_radius, 0.45, kR, 0.0);
    REQUIRE(bs.size() == 1u);
    const render::RibbonBatchCPU& b = bs[0];
    // The hand-built expectation: EVERY baked vertex, in baked order, draped at
    // radius(dir) + lift, with the baked (s, v) — exactly what the pre-cut
    // build_path_mesh allocated and filled.
    REQUIRE(b.pos.size() == static_cast<std::size_t>(P.vtx_count) * 3);
    REQUIRE(b.uv.size() == static_cast<std::size_t>(P.vtx_count) * 2);
    REQUIRE(b.idx == kept);
    for (int i = 0; i < P.vtx_count; ++i) {
        const render::GisRibbonVertex& V =
            render::kSudburyRibbonVerts[P.vtx_off + i];
        const glm::dvec3 d(V.dir[0], V.dir[1], V.dir[2]);
        const double r = dip_radius(d) + 0.45;
        CHECK(b.pos[i * 3 + 0] == static_cast<float>(d.x * r));
        CHECK(b.pos[i * 3 + 1] == static_cast<float>(d.y * r));
        CHECK(b.pos[i * 3 + 2] == static_cast<float>(d.z * r));
        CHECK(b.uv[i * 2 + 0] == V.s);
        CHECK(b.uv[i * 2 + 1] == V.v);
    }
}

TEST_CASE("ribbon subdiv: the chord over a dip falls under tolerance") {
    const render::GisRibbonPath& P = subdiv_path();
    const std::vector<unsigned short> kept = baked_indices(P);
    const double lift = 0.45;
    const std::vector<render::RibbonBatchCPU> off =
        render::build_ribbon_batches(P, kept, dip_radius, lift, kR, 0.0);
    const double sag_off = worst_chord_sag(off, lift);
    // The defect is REAL on this ground: a ~50 m chord over a 1 km / 40 m
    // sinusoid sags by metres.  A test that could pass with no defect present
    // is theater (the probe noise-floor law).
    REQUIRE(sag_off > 0.5);

    const std::vector<render::RibbonBatchCPU> on =
        render::build_ribbon_batches(P, kept, dip_radius, lift, kR, 8.0);
    const double sag_on = worst_chord_sag(on, lift);
    CHECK(sag_on < 0.10);          // the road census's own SINK threshold
    CHECK(sag_on < sag_off / 5);  // and it is the SUBDIVISION that did it

    // No drawn triangle may be lost or invented: the same quads, each split
    // into its own sub-quads, so the triangle count rises and never falls.
    std::size_t tri_off = 0, tri_on = 0, vmax = 0;
    for (const render::RibbonBatchCPU& b : off) tri_off += b.idx.size() / 3;
    for (const render::RibbonBatchCPU& b : on) {
        tri_on += b.idx.size() / 3;
        vmax = std::max(vmax, b.pos.size() / 3);
        for (unsigned short i : b.idx) REQUIRE(i < b.pos.size() / 3);
        REQUIRE(b.uv.size() * 3 == b.pos.size() * 2);
    }
    CHECK(tri_on >= tri_off);
    // raylib indexes a Mesh with UNSIGNED SHORT.  A subdivided road path runs
    // past that, which is why build_ribbon_batches returns a LIST — and the
    // slicing is the load-bearing part.
    REQUIRE(vmax <= 65535u);
    REQUIRE(on.size() > 1u);  // path 0 really does need slicing at 8 m
}

TEST_CASE("ribbon subdiv: a clipped triangle clips all of its children") {
    const render::GisRibbonPath& P = subdiv_path();
    std::vector<unsigned short> kept = baked_indices(P);
    // The T24 clip runs PER TRIANGLE: a quad with one vertex inside a cut disk
    // loses ONE of its two triangles and keeps the other.  Drop the second
    // triangle of the first quad and nothing else.
    REQUIRE(kept.size() >= 6u);
    kept.erase(kept.begin() + 3, kept.begin() + 6);

    const std::vector<render::RibbonBatchCPU> bs = render::build_ribbon_batches(
        P, kept, dip_radius, 0.45, kR, 8.0);
    REQUIRE(!bs.empty());
    // The dropped triangle's role pattern (which of the quad's four corners it
    // used) must not appear anywhere among the first quad's children.  Its
    // corner set is what identifies it: rebuild the set and look for it.
    const int m0 = kept[0] / 2;

    // Count the children of quad m0.  It is the FIRST quad emitted, so its
    // rungs are the first 2*(nsp+1) vertices of the first batch and its
    // children are exactly the triangles built only from those.  (Classifying
    // by arc-length s would be wrong: s RESETS at every run break, so many
    // rungs of this path share the same s -- the linework run-break law.)
    const render::RibbonBatchCPU& b = bs[0];
    // One surviving pattern x nsp sub-quads.  If the clip had been applied per
    // QUAD instead of per TRIANGLE, the surviving triangle would have been
    // dropped too (0 children); if it had been ignored, the dropped one would
    // have been resurrected (2 x nsp children).
    const glm::dvec3 dL0(render::kSudburyRibbonVerts[P.vtx_off + 2 * m0].dir[0],
                         render::kSudburyRibbonVerts[P.vtx_off + 2 * m0].dir[1],
                         render::kSudburyRibbonVerts[P.vtx_off + 2 * m0].dir[2]);
    const render::GisRibbonVertex& VR0 =
        render::kSudburyRibbonVerts[P.vtx_off + 2 * m0 + 1];
    const render::GisRibbonVertex& VL1 =
        render::kSudburyRibbonVerts[P.vtx_off + 2 * m0 + 2];
    const render::GisRibbonVertex& VR1 =
        render::kSudburyRibbonVerts[P.vtx_off + 2 * m0 + 3];
    const glm::dvec3 dR0(VR0.dir[0], VR0.dir[1], VR0.dir[2]);
    const glm::dvec3 dL1(VL1.dir[0], VL1.dir[1], VL1.dir[2]);
    const glm::dvec3 dR1(VR1.dir[0], VR1.dir[1], VR1.dir[2]);
    const int nsp = render::ribbon_seg_splits(glm::normalize(dL0 + dR0),
                                              glm::normalize(dL1 + dR1), kR,
                                              8.0);
    REQUIRE(nsp > 1);
    const std::size_t own = static_cast<std::size_t>(2 * (nsp + 1));
    std::size_t children = 0;
    for (std::size_t k = 0; k + 2 < b.idx.size(); k += 3)
        if (b.idx[k] < own && b.idx[k + 1] < own && b.idx[k + 2] < own)
            ++children;
    CHECK(children == static_cast<std::size_t>(nsp));

    // ★ THE TRANSVERSE HALF OF THE SAME RULE. A quad that lost one triangle
    // owns only HALF its area, along the R0->L1 diagonal, so it must NOT be
    // split into a grid: a cell straddling that diagonal would resurrect the
    // clipped half. With the transverse dial ARMED the halved quad therefore
    // still emits exactly nsp children (a single full-width span), while the
    // NEXT quad -- whole -- gets the full grid.
    const std::vector<render::RibbonBatchCPU> tr = render::build_ribbon_batches(
        P, kept, dip_radius, 0.45, kR, 8.0, 3.0);
    REQUIRE(!tr.empty());
    const int nt0 = std::max(
        render::ribbon_tr_splits(dL0, dR0, kR, 3.0),
        render::ribbon_tr_splits(dL1, dR1, kR, 3.0));
    REQUIRE(nt0 > 1);          // the dial really does split this ribbon ...
    REQUIRE(nt0 % 2 == 0);     // ... and always into an EVEN column count, so
                               // u = 0.5 (v = 0, the dashed centreline) is a
                               // real vertex row.
    const std::size_t own_tr = static_cast<std::size_t>(nsp + 1);  // nt == 1
    std::size_t children_tr = 0;
    for (std::size_t k = 0; k + 2 < tr[0].idx.size(); k += 3)
        if (tr[0].idx[k] < own_tr * 2 && tr[0].idx[k + 1] < own_tr * 2 &&
            tr[0].idx[k + 2] < own_tr * 2)
            ++children_tr;
    CHECK(children_tr == static_cast<std::size_t>(nsp));

    // And the UNCLIPPED list of the same path DOES get the grid: every whole
    // quad emits 2 x nsp x nt triangles, so the triangle count rises by
    // exactly the column factor over the longitudinal-only arm.
    const std::vector<unsigned short> whole = baked_indices(P);
    std::size_t t_seg = 0, t_both = 0;
    for (const render::RibbonBatchCPU& x :
         render::build_ribbon_batches(P, whole, dip_radius, 0.45, kR, 8.0, 0.0))
        t_seg += x.idx.size() / 3;
    for (const render::RibbonBatchCPU& x :
         render::build_ribbon_batches(P, whole, dip_radius, 0.45, kR, 8.0, 3.0))
        t_both += x.idx.size() / 3;
    CHECK(t_both > t_seg);

    // And with the dial OFF the same clipped list is still carried verbatim.
    const std::vector<render::RibbonBatchCPU> id =
        render::build_ribbon_batches(P, kept, dip_radius, 0.45, kR, 0.0);
    REQUIRE(id.size() == 1u);
    CHECK(id[0].idx == kept);
}

TEST_CASE("ribbon subdiv: ribbon_drawn_segments follows the triangles") {
    const render::GisRibbonPath& P = subdiv_path();
    const std::vector<unsigned short> kept = baked_indices(P);
    const std::vector<int> segs = render::ribbon_drawn_segments(kept);
    REQUIRE(!segs.empty());
    // A path concatenates many disjoint RUNS: array-adjacent rungs that share
    // no triangle.  So the drawn-segment count must be STRICTLY less than the
    // rung count — if it were equal, the helper would be reading vertex order
    // and not the index list, and the ruler would measure chords across run
    // breaks that are never drawn.
    const int nrungs = P.vtx_count / 2;
    CHECK(static_cast<int>(segs.size()) < nrungs - 1);
    CHECK(segs.back() <= nrungs - 2);
    // Dropping every triangle drops every segment.
    CHECK(render::ribbon_drawn_segments({}).empty());
}

TEST_CASE("ribbon subdiv: the centreline stops being a chord across the road") {
    const render::GisRibbonPath& P = subdiv_path();
    // A ground that curves ACROSS a road as well as along it -- the transverse
    // term is invisible to any longitudinal split, so it needs its own leg.
    // Measured at the RUNGS, where the longitudinal chord error is zero by
    // construction: this is the term alone, not a mixture.
    // A 30 m wavelength: the 1 km dip above curves too gently ACROSS a 5 m
    // ribbon to be a non-vacuous ruler for this term (measured: 0.082 m, under
    // the 0.10 m threshold the whole rung is graded on -- the probe noise-floor
    // law again).
    auto fine = [](const glm::dvec3& d) {
        return kR + 2.0 * std::sin(d.x * kR * (2.0 * 3.14159265358979 / 30.0));
    };
    auto cross = [&](double max_tr_m) {
        double worst = 0.0;
        for (int m = 0; m + 1 < P.vtx_count / 2; m += 37) {  // a wide sample
            const render::GisRibbonVertex& VL =
                render::kSudburyRibbonVerts[P.vtx_off + 2 * m];
            const render::GisRibbonVertex& VR =
                render::kSudburyRibbonVerts[P.vtx_off + 2 * m + 1];
            const glm::dvec3 dL(VL.dir[0], VL.dir[1], VL.dir[2]);
            const glm::dvec3 dR(VR.dir[0], VR.dir[1], VR.dir[2]);
            const int nt = render::ribbon_tr_splits(dL, dR, kR, max_tr_m);
            // The DRAWN deck at the centreline, off the mesh's own rung
            // polyline -- exactly what SEADS_RIBBON_SAG samples.
            const glm::dvec3 Pc =
                render::ribbon_rung_point(dL, dR, nt, 0.5, fine, 0.45);
            const double r = glm::length(Pc);
            worst = std::max(worst, std::fabs(r - (fine(Pc / r) + 0.45)));
        }
        return worst;
    };
    const double before = cross(0.0);   // one flat span across the whole road
    const double after = cross(3.0);    // a real centre vertex row
    REQUIRE(before > 0.10);             // the defect is present to begin with
    CHECK(after < before / 4);
    // On every rung WIDER than max_tr_m, u = 0.5 is a VERTEX and therefore sits
    // ON the ground exactly. The residual is the run-end taper: a rung narrower
    // than the dial keeps its single span by design (a 5 m trail gets no centre
    // row), so its centreline is still a short chord. Measured 5.5e-4 m.
    CHECK(after < 0.01);
}

TEST_CASE("ribbon subdiv: the transverse dial works with no longitudinal one") {
    const render::GisRibbonPath& P = subdiv_path();
    const std::vector<unsigned short> kept = baked_indices(P);
    // Both dials at 0 is the identity; the transverse dial ALONE must still
    // build (a road can be split across without being split along).
    const std::vector<render::RibbonBatchCPU> id =
        render::build_ribbon_batches(P, kept, dip_radius, 0.45, kR, 0.0, 0.0);
    REQUIRE(id.size() == 1u);
    REQUIRE(id[0].idx == kept);

    const std::vector<render::RibbonBatchCPU> tr =
        render::build_ribbon_batches(P, kept, dip_radius, 0.45, kR, 0.0, 3.0);
    std::size_t v = 0, t = 0, vmax = 0;
    for (const render::RibbonBatchCPU& b : tr) {
        v += b.pos.size() / 3;
        t += b.idx.size() / 3;
        vmax = std::max(vmax, b.pos.size() / 3);
        for (unsigned short i : b.idx) REQUIRE(i < b.pos.size() / 3);
    }
    REQUIRE(vmax <= 65535u);
    CHECK(t > id[0].idx.size() / 3);
    CHECK(v > 0u);
    // The centre column carries v == 0 -- the dashed centreline's own value
    // (the FS tests |v| < road_center_frac). If it did not, the dash would
    // land somewhere other than the middle of the road.
    bool saw_centre = false;
    for (const render::RibbonBatchCPU& b : tr)
        for (std::size_t i = 1; i < b.uv.size(); i += 2)
            if (std::fabs(b.uv[i]) < 1e-6f) saw_centre = true;
    CHECK(saw_centre);
}
