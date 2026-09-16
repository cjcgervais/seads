// SF2-BANKS -- render/bank_mesh.* gate legs (docs/oreo_banks_sf2_spec.md §4,
// WINTER_LAW §3.6c). The bank strip's every vertex radial is
// facet_radius_at(dir) + lift_m + snow.depth_geometry_at(dir) -- the rendered
// terrain facet plus the REAL driven snowpack field's GEOMETRY channel,
// sampled, never re-derived (★ W2b: depth_geometry_at, not depth_at --
// depth_at is the sinkable channel and W2 caps its bank contribution at
// bank_pack_skin_m, which is correct for sinkage but would flatten the drawn
// crest to a film; see world/snowpack.h's depth_at/depth_geometry_at
// comments). These legs pin that anti-fork law and the strip-splitting
// machinery (cuts, run junctions) against a synthetic flat field + a
// synthetic plowed road.
//
// Every leg here calls the INJECTABLE CORE directly -- bank_ring_offsets() and
// build_bank_run() (render/bank_mesh.h) -- never build_bank_strips(), the
// baked iterator that walks the compiled kSudburyRibbonPaths. That iterator
// has no kind knowledge of its own beyond a `P.kind != 0 && P.kind != 1`
// filter over baked global data, so it is not test-seamed here:
//
//   §4 leg 4 "plowed roads only" is DOCUMENTED N/A IN THIS FILE. What IS
//   injectable and covered instead is the CUT-SPLIT path inside
//   build_bank_run (leg 4b below) -- the same run-splitting machinery F1
//   relies on, exercised the one way the injectable core can see it.
//
// Legs:
//   1. bank_rings_cover_feather_and_bank_knots  -- bank_ring_offsets (§4 leg 1
//      is folded into this; the ring-set knots ARE the F2 fix).
//   2. bank_vertices_conform_to_the_field       -- the anti-fork law (§4 leg 1
//      "Conformance").
//   3. bank_crest_reaches_the_profile           -- §4 leg 2 "The crest is the
//      bank", plus the inner-ring bare-road read.
//   4b. bank_strip_splits_at_a_cut              -- §4 leg 4b, the F1 P0
//      splitting machinery via a synthetic CutDisk.
//   4c. bank_junction_gap_is_drawn              -- §4 leg 3/4c, the junction
//      amplitude fade (mid-run crest vs run-endpoint gap).
//
// ★ ROAD-REPAIR legs (2026-09-09, "the snowbank road sections are jagged and
// the sparkle drops out at every road edge"). Legs 2 and 3 above CHANGED and
// carry a why-comment at the change; these five are new:
//   5. bank_strip_ends_taper_instead_of_walling -- the culled station is a
//      taper CAP, not a break before a full-height station, so a junction
//      approach no longer ends in a vertical wall.
//   6. bank_normals_are_unit_outward_and_analytic -- the strips carry normals
//      at all (they were drawn unlit), and their lean follows the profile.
//   7. bank_section_chord_error_is_under_the_tolerance -- the ring set is
//      refined until the section chord error is under chord_tol_m.
//   8. bank_skirt_fall_is_spread_not_creased -- the burial fall rides a
//      smoothstep across skirt_rings sub-rings instead of creasing one quad.
//   9. bank_slope_is_the_analytic_derivative_of_the_driven_profile -- the
//      claim under the normals: bank_profile_slope and
//      depth_geometry_slope_at ARE the derivatives of the driven curve.
//
// ★ ROAD-REPAIR, THE ONAPING ROAD-EDGE PASS, RUNG 2 (2026-09-10, "some drop
// offs coming off the road you dont see"). Three legs for the DRAWN APRON:
//   10. bank_apron_zero_is_the_hand_built_strip -- THE IDENTITY. apron_m 0.0
//       emits exactly the strip an INDEPENDENT re-derivation of the ring law
//       predicts, vertex for vertex and index for index. It is graded against
//       a hand-built expectation and NOT against a second call to
//       build_bank_run with a different argument: that would be the corner
//       rung's tautology (docs/SESSION_HANDOFF_20260910_road_repair.md §6)
//       wearing a new hat.
//   11. bank_apron_reaches_the_driven_surface -- on a CROSS-SLOPED field the
//       apron exists, its lip is vertex-exact with the main strip's outermost
//       skirt ring, its body sits ON SnowpackField::drive_radius_at to 1e-6,
//       and it buries at -skirt_bury_m at its outer edge (F3, still).
//   12. bank_apron_is_free_on_flat_ground -- a flat fixture emits the SAME
//       vertex count with the apron on as with it off: a station whose own
//       terrain does not fall away pays nothing.
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <glm/geometric.hpp>
#include <vector>

#include "render/bank_mesh.h"
#include "world/heightfield.h"
#include "world/linework.h"
#include "world/snowpack.h"

namespace {

constexpr double kR = 15000.0;  // config/aircraft.toml world.R
constexpr int kSubdiv = 200;
constexpr int kTiles = 1;

double flat_half(double, double) { return 0.5; }

// ★ ROAD-REPAIR RUNG 2: a CROSS-SLOPE. equirect v is 0.5 - asin(y)/pi
// (world/heightfield.cpp), so a height linear in v is a height linear in
// LATITUDE -- and an equatorial road's outward direction is +/- latitude, so
// this is a road cut into a hillside: the ground falls away on one side and
// rises on the other, which is the Onaping cross-section exactly. The
// gradient is what a 128-row raster at relief_scale 350 m can carry (about
// 2.7 m per row over ~740 m, i.e. ~0.0037 m/m), so the fall across the 6 m
// skirt is centimetres, not the 4.27 m of the real shelf. The leg grades the
// LAW, not the magnitude, and sets apron_min_drop_m accordingly.
double cross_slope(double, double v) { return v; }

// The test_snowpack.cpp make_hf idiom: 16-bit samples from a callable of
// (u,v) in [0,1].
world::HeightField make_hf(int w, int h, double (*f)(double, double)) {
    world::HeightField hf;
    hf.w = w;
    hf.h = h;
    hf.R = kR;
    hf.relief_scale = 350.0;
    hf.u_offset = 0.0;
    hf.px.resize(static_cast<std::size_t>(w) * h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const double v =
                std::clamp(f((x + 0.5) / w, (y + 0.5) / h), 0.0, 1.0);
            hf.px[static_cast<std::size_t>(y) * w + x] =
                static_cast<std::uint16_t>(v * 65535.0 + 0.5);
        }
    return hf;
}

// Unit dir on the equator at longitude fraction t.
glm::dvec3 eq_dir(double t) {
    const double a = t * 2.0 * 3.14159265358979323846;
    return glm::dvec3(std::sin(a), 0.0, std::cos(a));
}

// One straight equatorial ribbon of constant half-width, the test_snowpack.cpp
// L/R pair idiom -- used ONLY to feed LineNetwork::add_path so snow.depth_at
// sees a real plowed corridor. The geometry handed to build_bank_run itself is
// the SEPARATE ctr/s_arc/half_w arrays in Fixture below: the injectable core
// takes them directly and never touches (or recovers from) a ribbon.
struct Ribbon {
    std::vector<glm::dvec3> v;
    std::vector<float> s;
};
Ribbon straight_ribbon(double t0, double t1, int stations, double half_w_m) {
    Ribbon r;
    const double off = half_w_m / kR;
    for (int i = 0; i < stations; ++i) {
        const double t = t0 + (t1 - t0) * i / (stations - 1.0);
        const glm::dvec3 p = eq_dir(t);
        const glm::dvec3 north(0.0, 1.0, 0.0);
        r.v.push_back(glm::normalize(p + north * off));
        r.v.push_back(glm::normalize(p - north * off));
        const float arc =
            static_cast<float>((t - t0) * 2.0 * 3.14159265358979323846 * kR);
        r.s.push_back(arc);
        r.s.push_back(arc);
    }
    return r;
}

constexpr double kHalfW = 3.0;      // plowed road half-width, m (6 m full)
constexpr double kRunLenM = 250.0;  // >> 3x bank_gap_m (14 m) from either end
constexpr int kStations = 51;       // ~5 m longitudinal spacing

// A flat synthetic HeightField, a LineNetwork carrying ONE straight plowed
// (RoadMinor) run of kRunLenM, and the matching ctr/s_arc/half_w arrays fed
// DIRECTLY to build_bank_run. The LineNetwork and the ctr/half_w arrays trace
// the SAME centerline/width on purpose (so snow.depth_at reads the bank the
// arrays are asking about) -- they are two independent constructions of one
// geometry, not one recovered from the other, which is deliberate: this file
// is the one place INV-6's recovery is NOT exercised (that is test_snowpack's
// job; §4 leg 4's baked recovery is the documented N/A above).
struct Fixture {
    world::HeightField hf;
    world::LineNetwork net;
    std::vector<glm::dvec3> ctr;
    std::vector<float> s_arc;
    std::vector<double> half_w;
};

Fixture make_fixture() {
    Fixture f;
    f.hf = make_hf(256, 128, flat_half);

    const double t0 = 0.10;
    const double dtheta = kRunLenM / (2.0 * 3.14159265358979323846 * kR);
    const double t1 = t0 + dtheta;

    const Ribbon rb = straight_ribbon(t0, t1, 60, kHalfW);
    f.net.add_path(rb.v.data(), rb.s.data(), rb.v.size(),
                   world::LineKind::RoadMinor, kR);
    f.net.build_index();

    for (int i = 0; i < kStations; ++i) {
        const double t = t0 + (t1 - t0) * i / (kStations - 1.0);
        f.ctr.push_back(eq_dir(t));
        f.s_arc.push_back(
            static_cast<float>(kRunLenM * i / (kStations - 1.0)));
        f.half_w.push_back(kHalfW);
    }
    return f;
}

// Shipped bank dials (base_m 0.85, bank_rise/fall/gap/height at config
// defaults), with the slope/curvature/elevation/aspect/drainage gains zeroed
// -- the test_snowpack INV-3/F2 idiom -- so ambient_depth_at is EXACTLY
// base_m everywhere on this flat fixture and the crest/junction legs check
// against a known constant rather than re-deriving one.
world::SnowParams shipped_bank_params() {
    world::SnowParams p;
    p.base_m = 0.85;
    p.slope_shed = 0.0;
    p.curv_gain = 0.0;
    p.elev_gain_per_km = 0.0;
    p.aspect_lee = 0.0;
    p.drain_gain = 0.0;
    return p;
}

constexpr float kSkirtV = 1.25f;  // BankStripCPU uv.v tag for the skirt ring

}  // namespace

// ---------------------------------------------------------------- leg 1 -----

TEST_CASE("bank_rings_cover_feather_and_bank_knots") {
    // §4 leg 1 / red-team F2: the cross-section rings must be the sorted union
    // of the BANK knots (0, rise, rise+fall) and the FEATHER knots (0,
    // corridor_edge) -- a rise-only ring set clips the true crest, which sits
    // at (rise+corridor_edge)/2, not at rise.
    world::SnowParams sp;  // shipped: rise 3, fall 6, corridor_edge 4
    const std::vector<double> e = render::bank_ring_offsets(sp);
    const double end = sp.bank_rise_m + sp.bank_fall_m;

    REQUIRE(!e.empty());
    REQUIRE(e.front() == 0.0);
    for (std::size_t i = 0; i < e.size(); ++i) {
        REQUIRE(e[i] >= 0.0);
        REQUIRE(e[i] <= end + 1e-9);
        if (i > 0) REQUIRE(e[i] > e[i - 1]);  // strictly ascending, no dupes
    }

    auto has = [&](double v) {
        return std::any_of(e.begin(), e.end(), [&](double x) {
            return std::abs(x - v) < 1e-6;
        });
    };
    REQUIRE(has(0.0));
    REQUIRE(has(sp.bank_rise_m * 0.5));                          // 1.5
    REQUIRE(has(sp.bank_rise_m));                                // 3
    REQUIRE(has((sp.bank_rise_m + sp.corridor_edge_m) * 0.5));   // 3.5
    REQUIRE(has(sp.corridor_edge_m));                             // 4
    REQUIRE(has(end));                                            // 9
}

// ---------------------------------------------------------------- leg 2 -----

TEST_CASE("bank_vertices_conform_to_the_field") {
    // §4 leg 1, the anti-fork law: every emitted vertex's radial is exactly
    // facet_radius_at + lift + snow.depth_at, EXCEPT the skirt ring (uv.v ==
    // 1.25), which buries below the facet with NO lift.
    Fixture f = make_fixture();
    world::SnowpackField snow;
    snow.hf = &f.hf;
    snow.lines = &f.net;
    snow.p = shipped_bank_params();

    render::BankBuildParams p;  // shipped defaults: lift_m 0.45, skirt_bury_m 0.5
    std::vector<render::BankStripCPU> out;
    const long nverts =
        render::build_bank_run(f.ctr, f.s_arc, f.half_w, f.hf, kSubdiv,
                               kTiles, snow, p, {}, out);
    REQUIRE(nverts > 0);
    REQUIRE(!out.empty());

    long checked = 0;
    for (const render::BankStripCPU& strip : out) {
        REQUIRE(strip.pos.size() % 3 == 0);
        const std::size_t n = strip.pos.size() / 3;
        REQUIRE(strip.uv.size() == n * 2);
        for (std::size_t i = 0; i < n; ++i) {
            const glm::dvec3 pos(strip.pos[3 * i + 0], strip.pos[3 * i + 1],
                                 strip.pos[3 * i + 2]);
            const double radial = glm::length(pos);
            const glm::dvec3 dir = glm::normalize(pos);
            const double facet =
                render::facet_radius_at(f.hf, dir, kSubdiv, kTiles);
            const float v = strip.uv[2 * i + 1];

            if (v > 1.0f) {
                // ★ ROAD-REPAIR (WHY THIS LEG CHANGED). The burial skirt is
                // no longer one quad: it is `skirt_rings` sub-rings tagged
                // v = 1.0 -> 1.25, over which the strip's EXCESS above the
                // terrain facet walks from the outer bank ring's excess down
                // to -skirt_bury_m on a smoothstep. One quad could only draw
                // that fall as a CREASE, and a crease is exactly what a lit
                // strip shows. Two assertions replace the old one:
                //
                //   * F3 IS INTACT, to the same 1e-6: the OUTERMOST skirt
                //     ring (the shipped v == 1.25 tag) still lands on
                //     `facet - skirt_bury_m` with NO lift, so the strip still
                //     dives under the drawn terrain and cannot draw a
                //     floating terrace edge.
                //   * every INTERIOR skirt ring is bracketed by the two ends
                //     it interpolates -- never below the burial, never above
                //     the drawn snow surface at its own place. (The fixture's
                //     field is flat and its ambient is exactly base_m
                //     everywhere, which is what makes the upper bracket a
                //     constant rather than a re-derivation.)
                if (std::abs(v - kSkirtV) < 1e-6f) {
                    const double expect = facet - p.skirt_bury_m;
                    const double tol = 1e-6 * std::max(1.0, std::abs(expect));
                    REQUIRE(std::abs(radial - expect) < tol);
                } else {
                    const double excess = radial - facet;
                    REQUIRE(excess >= -p.skirt_bury_m - 1e-6);
                    REQUIRE(excess <= static_cast<double>(p.lift_m) +
                                          snow.depth_geometry_at(dir) + 1e-6);
                }
            } else {
                // ★ THE ANTI-FORK LINE, pinned against the built mesh.
                // ★ W2b: the drawn ring is GEOMETRY, so it must agree with
                // depth_geometry_at, not depth_at (depth_at is the sinkable
                // channel, capped at the bank -- see world/snowpack.h).
                const double expect =
                    facet + static_cast<double>(p.lift_m) +
                    snow.depth_geometry_at(dir);
                const double tol = 1e-6 * std::max(1.0, std::abs(expect));
                REQUIRE(std::abs(radial - expect) < tol);
            }
            ++checked;
        }
    }
    REQUIRE(checked == nverts);
}

// ---------------------------------------------------------------- leg 3 -----

TEST_CASE("bank_crest_reaches_the_profile") {
    // §4 leg 2: at a mid-run station (far from both run-endpoint junctions),
    // the crest ring reaches ambient + bank_height_m within the chord-loss
    // tolerance, and the INNER ring (right at the corridor edge, e == 0) reads
    // the plowed roadway's bare depth, not a whitened one.
    Fixture f = make_fixture();
    world::SnowpackField snow;
    snow.hf = &f.hf;
    snow.lines = &f.net;
    snow.p = shipped_bank_params();

    render::BankBuildParams p;
    std::vector<render::BankStripCPU> out;
    render::build_bank_run(f.ctr, f.s_arc, f.half_w, f.hf, kSubdiv, kTiles,
                           snow, p, {}, out);
    REQUIRE(!out.empty());

    // ★ ROAD-REPAIR (WHY THIS LEG CHANGED). It used to index out[0].pos as
    // `station * nr + ring` with nr == rings + 1 -- the same latent assumption
    // the GI3 P0 fix already had to take out of leg 4c. Two things now break
    // it: the ring set is REFINED (7 knots -> 12 at the shipped dials, chord
    // error 0.178 m -> 0.045 m) and the skirt is 3 sub-rings, so `nr` is not
    // rings+1; and a strip may now OPEN on a taper cap, so station `mid` is
    // not at index `mid`. Both are exactly the sort of thing a stride read
    // cannot notice, so this leg reads the way leg 4c does: vertices are
    // addressed by LONGITUDE (the fixture's run is equatorial) over ALL
    // strips, and the ring is identified by its uv.v TAG (0 == the corridor
    // edge, > 1 == the burial skirt) rather than by an index.
    const int mid = kStations / 2;  // arc ~125 m: >> bank_gap_m (14) from
                                    // BOTH run endpoints
    const glm::dvec3 cmid = f.ctr[static_cast<std::size_t>(mid)];
    const double ambient = snow.ambient_depth_at(cmid);
    auto lon_of = [](const glm::dvec3& d) { return std::atan2(d.x, d.z); };
    const double lon_win =
        0.6 * (kRunLenM / (kStations - 1.0)) / kR;  // ~60% station spacing
    const double lon_c = lon_of(cmid);

    double max_h = -1e30, inner_h = 1e30;
    long seen = 0;
    for (const render::BankStripCPU& strip : out)
        for (std::size_t v = 0; v * 3 + 2 < strip.pos.size(); ++v) {
            const float tag = strip.uv[v * 2 + 1];
            if (tag > 1.0f) continue;  // the burial skirt, not the bank
            const glm::dvec3 pos(strip.pos[v * 3 + 0], strip.pos[v * 3 + 1],
                                 strip.pos[v * 3 + 2]);
            const glm::dvec3 dir = glm::normalize(pos);
            if (std::abs(lon_of(dir) - lon_c) > lon_win) continue;
            const double facet =
                render::facet_radius_at(f.hf, dir, kSubdiv, kTiles);
            const double h =
                glm::length(pos) - facet - static_cast<double>(p.lift_m);
            max_h = std::max(max_h, h);
            if (tag == 0.0f) inner_h = std::min(inner_h, h);
            ++seen;
        }
    REQUIRE(seen > 0);
    REQUIRE(inner_h < 1e29);

    // ★ ROAD-REPAIR: the chord tolerance TIGHTENS from 0.15 m to 0.08 m,
    // because that is what the ring refinement bought -- the crest ring can no
    // longer sit 0.178 m of chord below the true crest (the measured deficit
    // on this fixture is 0.062 m, against 0.15 m of headroom before).
    std::printf("[bank_crest] deficit=%.4f m\n",
                ambient + snow.p.bank_height_m - max_h);
    REQUIRE(max_h >= ambient + snow.p.bank_height_m - 0.08);
    REQUIRE(inner_h < 0.2);  // the plowed roadway, not the whitened shoulder
}

// --------------------------------------------------------------- leg 4b -----
// §4 leg 4 "plowed roads only" is DOCUMENTED N/A HERE: it tests the BAKED
// iterator build_bank_strips, which filters the compiled kSudburyRibbonPaths
// by P.kind and has no injectable seam of its own -- the injectable core
// (build_bank_run) has no kind knowledge at all. What IS injectable is the
// CUT-SPLIT path inside build_bank_run, which is the same run-splitting
// machinery the F1 run-break fix relies on -- covered below.

TEST_CASE("bank_strip_splits_at_a_cut") {
    // §4 leg 4b: a CutDisk mid-run splits the strip into MORE pieces (the
    // ribbon T24 mirror, F4), and no emitted vertex direction lies inside it.
    Fixture f = make_fixture();
    world::SnowpackField snow;
    snow.hf = &f.hf;
    snow.lines = &f.net;
    snow.p = shipped_bank_params();
    render::BankBuildParams p;

    std::vector<render::BankStripCPU> no_cut;
    render::build_bank_run(f.ctr, f.s_arc, f.half_w, f.hf, kSubdiv, kTiles,
                           snow, p, {}, no_cut);
    REQUIRE(!no_cut.empty());

    const int mid = kStations / 2;
    const std::vector<render::CutDisk> cuts{
        render::CutDisk{f.ctr[static_cast<std::size_t>(mid)], 30.0}};
    std::vector<render::BankStripCPU> with_cut;
    render::build_bank_run(f.ctr, f.s_arc, f.half_w, f.hf, kSubdiv, kTiles,
                           snow, p, cuts, with_cut);

    REQUIRE(with_cut.size() > no_cut.size());  // the split

    for (const render::BankStripCPU& strip : with_cut)
        for (std::size_t i = 0; i < strip.pos.size() / 3; ++i) {
            const glm::dvec3 dir = glm::normalize(
                glm::dvec3(strip.pos[3 * i + 0], strip.pos[3 * i + 1],
                          strip.pos[3 * i + 2]));
            REQUIRE_FALSE(render::dir_in_any_cut(dir, f.hf.R, cuts));
        }
}

// --------------------------------------------------------------- leg 4c -----

TEST_CASE("bank_junction_gap_is_drawn") {
    // §4 leg 3/4c: the drawn gap is the driven gap -- at the run's very
    // endpoint (a junction by construction: LineNetwork records every run
    // endpoint), the crest amplitude above ambient collapses; mid-run it does
    // not.
    Fixture f = make_fixture();
    world::SnowpackField snow;
    snow.hf = &f.hf;
    snow.lines = &f.net;
    snow.p = shipped_bank_params();
    render::BankBuildParams p;

    std::vector<render::BankStripCPU> out;
    render::build_bank_run(f.ctr, f.s_arc, f.half_w, f.hf, kSubdiv, kTiles,
                           snow, p, {}, out);
    REQUIRE(!out.empty());

    const int last = kStations - 1;  // the run's endpoint -- a junction
    const int mid = kStations / 2;

    // ★ GI3 P0 FIX (2026-08-13, latent since the leg was written): the old
    // read indexed out[0].pos as `station * nr + ring`, assuming the whole
    // run stays ONE strip -- but the junction fade's min_amp cull SPLITS the
    // strip near the endpoint (the exact machinery this leg exists to pin:
    // measured, out[0] carries 49 of the 51 stations), so the endpoint read
    // ran past the buffer and asserted on heap garbage. It "passed" for as
    // long as the garbage happened to be small; the GI3 rung's unrelated
    // allocations shifted the heap and the garbage came up 1e33. Vertices
    // are now addressed by LONGITUDE (the fixture's run is equatorial), over
    // ALL strips, skirt ring excluded by its uv tag; a station whose rings
    // were culled entirely draws NO bank, which is amplitude 0 -- the gap
    // drawn by absence, the strongest form of the pass.
    auto lon_of = [](const glm::dvec3& d) { return std::atan2(d.x, d.z); };
    const double lon_win =
        0.6 * (kRunLenM / (kStations - 1.0)) / kR;  // ~60% station spacing
    auto max_height_above_ambient = [&](int station) {
        const glm::dvec3 c = f.ctr[static_cast<std::size_t>(station)];
        const double ambient = snow.ambient_depth_at(c);
        const double lon_c = lon_of(c);
        double max_h = 0.0;
        for (const render::BankStripCPU& strip : out)
            for (std::size_t v = 0; v * 3 + 2 < strip.pos.size(); ++v) {
                if (strip.uv[v * 2 + 1] == kSkirtV) continue;  // skirt ring
                const glm::dvec3 pos(strip.pos[v * 3 + 0],
                                     strip.pos[v * 3 + 1],
                                     strip.pos[v * 3 + 2]);
                const glm::dvec3 dir = glm::normalize(pos);
                if (std::abs(lon_of(dir) - lon_c) > lon_win) continue;
                const double facet =
                    render::facet_radius_at(f.hf, dir, kSubdiv, kTiles);
                const double h = glm::length(pos) - facet -
                                 static_cast<double>(p.lift_m) - ambient;
                max_h = std::max(max_h, h);
            }
        return max_h;
    };

    const double end_amp = max_height_above_ambient(last);
    const double mid_amp = max_height_above_ambient(mid);
    std::printf("[bank_junction] strips=%zu end_amp=%.3f mid_amp=%.3f\n",
                out.size(), end_amp, mid_amp);

    REQUIRE(end_amp < 0.25 * snow.p.bank_height_m);   // the gap
    REQUIRE(mid_amp > 0.85 * snow.p.bank_height_m);   // the bank is really there
}

// ------------------------------------------------------------ leg 5 (RR) -----

TEST_CASE("bank_strip_ends_taper_instead_of_walling") {
    // ★ ROAD-REPAIR leg 1: THE JAG AT THE JUNCTIONS. A station whose measured
    // bank amplitude has gap-faded below min_amp_m still SPLITS the strip --
    // that is Chad's "clear the intersections" ruling and it is untouched --
    // but it is now EMITTED first, as a taper cap. Before, the strip ended on
    // the last FULL-amplitude station, so every junction approach put a
    // vertical white wall up to bank_height_m (1.30 m) tall across the end of
    // the bank. After, a strip end is a station whose amplitude is below
    // min_amp_m BY THE DEFINITION OF THE CULL, so the wall is bounded by that
    // dial and nothing else.
    //
    // The cap's vertices are composed by the identical anti-fork line every
    // other vertex is (leg 2 checks every vertex, these included), so this
    // closes the end WITHOUT drawing any snow the field does not carry.
    Fixture f = make_fixture();
    world::SnowpackField snow;
    snow.hf = &f.hf;
    snow.lines = &f.net;
    snow.p = shipped_bank_params();

    render::BankBuildParams legacy;  // the pre-repair arm, exactly
    legacy.taper_caps = false;
    legacy.skirt_rings = 1;
    legacy.chord_tol_m = 0.0;
    render::BankBuildParams fixed;  // the shipped arm

    auto ends_of = [&](const render::BankBuildParams& p) {
        std::vector<render::BankStripCPU> out;
        render::build_bank_run(f.ctr, f.s_arc, f.half_w, f.hf, kSubdiv, kTiles,
                               snow, p, {}, out);
        REQUIRE(!out.empty());
        const int nr =
            static_cast<int>(
                render::bank_ring_offsets(snow.p, p.chord_tol_m, p.max_rings)
                    .size()) +
            std::max(1, p.skirt_rings);
        return render::bank_strip_continuity(out, nr);
    };

    const render::BankStripContinuity a = ends_of(legacy);
    const render::BankStripContinuity b = ends_of(fixed);
    std::printf(
        "[bank_taper] legacy end_amp max=%.3f p99=%.3f step max=%.3f | fixed "
        "end_amp max=%.3f p99=%.3f step max=%.3f\n",
        a.end_amp_max, a.end_amp_p99, a.step_max, b.end_amp_max,
        b.end_amp_p99, b.step_max);

    // The wall the strip used to end on is real, and it is over the dial that
    // was supposed to bound it. (On this 250 m synthetic run the junction fade
    // has 5 m stations to work with and the worst legacy wall measures ~0.35 m;
    // on the shipped bake, where a run can end ON a full-height station, it is
    // far worse -- docs/road_repair/census_after_banks.md carries the
    // planet-wide number.)
    REQUIRE(a.end_amp_max > fixed.min_amp_m);
    // The tapered end is bounded by the cull dial itself. (A little headroom:
    // this metric is the crest ring's height above the strip's own OUTER bank
    // ring, while the cull measures it above the AMBIENT at that ring -- the
    // same quantity to within the ring set's chord.)
    REQUIRE(b.end_amp_max < fixed.min_amp_m + 0.10);
    REQUIRE(b.end_amp_max < a.end_amp_max);
}

// ------------------------------------------------------------ leg 6 (RR) -----

TEST_CASE("bank_normals_are_unit_outward_and_analytic") {
    // ★ ROAD-REPAIR leg 2: THE SPARKLE. The strips had NO normals at all --
    // they were drawn unlit, which is why the winter stopped at every road
    // edge. They carry one per vertex now, and it has to be a real surface
    // normal, not a placeholder:
    //
    //   * unit length, and always pointing AWAY from the planet centre (the
    //     pass draws with culling off, so a flipped normal is a black band,
    //     which is worse than a missing triangle, not better);
    //   * on the INNER face of the bank (corridor edge -> crest, where
    //     bank_profile is RISING) the normal leans BACK TOWARD THE ROAD,
    //     because that face climbs away from it;
    //   * on the OUTER face (past the crest, bank_profile FALLING) it leans
    //     the other way.
    //
    // A finite difference across the strips could not answer either question
    // at a chunk seam or between the left and right strips; the analytic
    // derivative (SnowpackField::depth_geometry_slope_at, whose bank term is
    // bank_profile_slope) is the same curve everywhere.
    Fixture f = make_fixture();
    world::SnowpackField snow;
    snow.hf = &f.hf;
    snow.lines = &f.net;
    snow.p = shipped_bank_params();

    render::BankBuildParams p;
    std::vector<render::BankStripCPU> out;
    render::build_bank_run(f.ctr, f.s_arc, f.half_w, f.hf, kSubdiv, kTiles,
                           snow, p, {}, out);
    REQUIRE(!out.empty());

    const double end = snow.p.bank_rise_m + snow.p.bank_fall_m;
    const double crest_v = snow.p.bank_rise_m / end;
    long inner = 0, outer = 0;
    for (const render::BankStripCPU& strip : out) {
        REQUIRE(strip.nrm.size() == strip.pos.size());
        for (std::size_t i = 0; i * 3 + 2 < strip.pos.size(); ++i) {
            const glm::dvec3 n(strip.nrm[3 * i + 0], strip.nrm[3 * i + 1],
                               strip.nrm[3 * i + 2]);
            REQUIRE(std::abs(glm::length(n) - 1.0) < 1e-4);
            const glm::dvec3 pos(strip.pos[3 * i + 0], strip.pos[3 * i + 1],
                                 strip.pos[3 * i + 2]);
            const glm::dvec3 up = glm::normalize(pos);
            REQUIRE(glm::dot(n, up) > 0.0);  // never inward

            const float v = strip.uv[2 * i + 1];
            if (v <= 0.02f || v > 1.0f) continue;  // the edge and the skirt
            // The outward direction at this vertex. The fixture's run is
            // equatorial, so "across the road" is the NORTH axis and the side
            // is the sign of the vertex's own latitude.
            const glm::dvec3 north(0.0, 1.0, 0.0);
            const double side = glm::dot(up, north) >= 0.0 ? 1.0 : -1.0;
            const glm::dvec3 outward = north * side;
            const double lean = glm::dot(n, outward);
            // ★ WHY THE SIGN TEST IS ONE-SIDED WITH A DEAD BAND. A station
            // inside a junction gap has its bank amplitude faded to nothing,
            // so its "inner face" and "outer face" are FLAT -- the analytic
            // slope really is ~0 there and the sign of a zero is not a claim
            // worth asserting (measured: -6e-4 on a capped station). So each
            // face asserts the WRONG sign never appears beyond a 1e-3 dead
            // band, and separately counts the vertices where the lean is
            // unambiguous, which must exist.
            if (v < crest_v * 0.75) {
                REQUIRE(lean < 1e-3);          // rising inner face
                if (lean < -0.05) ++inner;
            } else if (v > 0.55f && v < 0.80f) {
                // The STEEP part of the falling outer face (e ~ 5-7 m, either
                // side of the fall's inflexion at rise + fall/2). Past it the
                // profile flattens to ambient by construction.
                REQUIRE(lean > -1e-3);         // falling outer face
                if (lean > 0.05) ++outer;
            }
        }
    }
    std::printf("[bank_normals] inner=%ld outer=%ld\n", inner, outer);
    REQUIRE(inner > 0);
    REQUIRE(outer > 0);
}

// ------------------------------------------------------------ leg 7 (RR) -----

TEST_CASE("bank_section_chord_error_is_under_the_tolerance") {
    // ★ ROAD-REPAIR leg 3: the ring set is REFINED until no chord of the
    // composed section misses the curve by more than chord_tol_m. The shipped
    // 7-knot union missed by 0.178 m -- a visible facet running the length of
    // a 1.30 m bank, and one of the two things that read as "jagged".
    world::SnowParams sp;
    world::SnowpackField probe;
    probe.p = sp;
    const double ce = std::max(1e-6, sp.corridor_edge_m);
    const double feather_amp = std::max(0.0, sp.base_m - sp.road_bare_m);
    auto ss01 = [](double x) {
        const double t = std::clamp(x, 0.0, 1.0);
        return t * t * (3.0 - 2.0 * t);
    };
    auto h = [&](double x) {
        return feather_amp * ss01(x / ce) + probe.bank_profile(x);
    };
    auto worst_chord = [&](const std::vector<double>& e) {
        double worst = 0.0;
        for (std::size_t i = 0; i + 1 < e.size(); ++i)
            for (int k = 1; k < 8; ++k) {
                const double x = e[i] + (e[i + 1] - e[i]) * k / 8.0;
                const double c = h(e[i]) + (h(e[i + 1]) - h(e[i])) *
                                               (x - e[i]) / (e[i + 1] - e[i]);
                worst = std::max(worst, std::abs(h(x) - c));
            }
        return worst;
    };

    const std::vector<double> shipped = render::bank_ring_offsets(sp);
    render::BankBuildParams p;
    const std::vector<double> refined =
        render::bank_ring_offsets(sp, p.chord_tol_m, p.max_rings);
    std::printf(
        "[bank_chord] shipped %zu knots err=%.4f | refined %zu knots "
        "err=%.4f\n",
        shipped.size(), worst_chord(shipped), refined.size(),
        worst_chord(refined));

    REQUIRE(worst_chord(shipped) > p.chord_tol_m);   // the defect was real
    REQUIRE(worst_chord(refined) <= p.chord_tol_m);  // and it is closed
    REQUIRE(refined.size() >= shipped.size());
    REQUIRE(static_cast<int>(refined.size()) <= p.max_rings);
    // Every shipped knot SURVIVES: refinement only bisects, never moves or
    // drops a knot, so the F2 union (leg 1) still holds on the refined set.
    for (double k : shipped)
        REQUIRE(std::any_of(refined.begin(), refined.end(),
                            [&](double x) { return std::abs(x - k) < 1e-9; }));
}

// ------------------------------------------------------------ leg 8 (RR) -----

TEST_CASE("bank_skirt_fall_is_spread_not_creased") {
    // ★ ROAD-REPAIR leg 4: THE SKIRT CREASE. The burial skirt used to drop the
    // whole excess -- lift + the drawn snow depth + skirt_bury_m, ~1.7 m at
    // the shipped dials -- across ONE quad, and land on the terrain facet at a
    // hard angle. Unlit that was invisible; lit it is a black line down both
    // sides of every road. The fall now rides a smoothstep across skirt_rings
    // sub-rings, whose zero end-slope makes the join to the outer bank ring
    // C1 by construction.
    //
    // The measurable form of "no crease": the LARGEST single ring-to-ring drop
    // across the skirt band falls, and the drop across the FIRST skirt quad --
    // the join the crease used to sit on -- becomes a small fraction of the
    // total fall instead of all of it.
    Fixture f = make_fixture();
    world::SnowpackField snow;
    snow.hf = &f.hf;
    snow.lines = &f.net;
    snow.p = shipped_bank_params();

    auto skirt_profile = [&](int skirt_rings) {
        render::BankBuildParams p;
        p.skirt_rings = skirt_rings;
        std::vector<render::BankStripCPU> out;
        render::build_bank_run(f.ctr, f.s_arc, f.half_w, f.hf, kSubdiv, kTiles,
                               snow, p, {}, out);
        REQUIRE(!out.empty());
        const int nb = static_cast<int>(
            render::bank_ring_offsets(snow.p, p.chord_tol_m, p.max_rings)
                .size());
        const int nr = nb + skirt_rings;
        // A mid-strip station of the longest strip: the outer bank ring and
        // then the skirt rings, in order.
        const render::BankStripCPU* big = &out[0];
        for (const render::BankStripCPU& s : out)
            if (s.pos.size() > big->pos.size()) big = &s;
        const std::size_t ns = big->pos.size() / 3 / static_cast<std::size_t>(nr);
        REQUIRE(ns >= 3);
        const std::size_t st = ns / 2;
        std::vector<double> r;
        for (int k = nb - 1; k < nr; ++k) {
            const std::size_t v =
                st * static_cast<std::size_t>(nr) + static_cast<std::size_t>(k);
            r.push_back(std::sqrt(
                static_cast<double>(big->pos[3 * v + 0]) * big->pos[3 * v + 0] +
                static_cast<double>(big->pos[3 * v + 1]) * big->pos[3 * v + 1] +
                static_cast<double>(big->pos[3 * v + 2]) * big->pos[3 * v + 2]));
        }
        return r;
    };

    const std::vector<double> one = skirt_profile(1);    // the pre-repair skirt
    const std::vector<double> many = skirt_profile(3);   // the shipped skirt
    const double total_one = one.front() - one.back();
    const double total_many = many.front() - many.back();
    double worst_many = 0.0;
    for (std::size_t i = 0; i + 1 < many.size(); ++i)
        worst_many = std::max(worst_many, many[i] - many[i + 1]);
    std::printf(
        "[bank_skirt] fall=%.3f m | one-quad drop=%.3f | spread worst "
        "quad=%.3f first quad=%.3f\n",
        total_one, one.front() - one.back(), worst_many,
        many[0] - many[1]);

    // The two skirts END in the same place (F3: facet - skirt_bury_m), so the
    // TOTAL fall is the same -- this leg is about how it is spent, not how big
    // it is.
    REQUIRE(std::abs(total_one - total_many) < 1e-6);
    REQUIRE(total_one > 1.0);  // the crease was ~1.7 m in one quad
    // No single quad now carries more than 60 % of the fall...
    REQUIRE(worst_many < 0.60 * total_many);
    // ...and the join the crease sat on carries under a quarter of it.
    REQUIRE(many[0] - many[1] < 0.30 * total_many);
}

// ------------------------------------------------------------ leg 9 (RR) -----

TEST_CASE("bank_slope_is_the_analytic_derivative_of_the_driven_profile") {
    // ★ ROAD-REPAIR leg 5: THE CLAIM UNDER THE NORMALS. The strip's transverse
    // shading slope is not a chord and not a finite difference across the
    // mesh -- it is the closed-form derivative of the SAME curve the sled
    // drives. Two pins, because there are two functions:
    //
    //   a) bank_profile_slope IS d(bank_profile)/d(distance). Checked against a
    //      central difference of bank_profile itself across the whole support,
    //      which is the only honest way to say "analytic" without re-typing
    //      the algebra in the test and grading the code against a copy of
    //      itself. And while the derivative is in hand, the §2.4c RAMP BOUND
    //      (bank_max_grade) becomes a direct read instead of a sampled one.
    //
    //   b) depth_geometry_slope_at IS d(depth_geometry_at)/d(distance outward
    //      from the corridor edge), on the real corridor -- feather term, bank
    //      term, junction gap and all. Checked the same way, by walking OUTWARD
    //      from a mid-run station of the fixture's plowed road.
    //
    // If either drifts, the banks shade a surface they are not standing on.
    Fixture f = make_fixture();
    world::SnowpackField snow;
    snow.hf = &f.hf;
    snow.lines = &f.net;
    snow.p = shipped_bank_params();

    // (a) the profile alone.
    const double end = snow.p.bank_rise_m + snow.p.bank_fall_m;
    const double h = 1e-5;
    double worst = 0.0, peak_grade = 0.0;
    for (int i = 1; i < 2000; ++i) {
        const double d = end * 1.05 * i / 2000.0;
        const double fd =
            (snow.bank_profile(d + h) - snow.bank_profile(d - h)) / (2.0 * h);
        const double an = snow.bank_profile_slope(d);
        worst = std::max(worst, std::abs(fd - an));
        peak_grade = std::max(peak_grade, std::abs(an));
    }
    std::printf("[bank_slope] profile worst |fd-analytic| = %.3e, peak grade "
                "%.4f (bound %.2f)\n",
                worst, peak_grade, snow.p.bank_max_grade);
    REQUIRE(worst < 1e-5);
    // §2.4c: the profile is sized so its peak gradient is 1.5*height/width --
    // a RAMP that launches, never a step that stops the machine dead. With the
    // derivative in closed form this is exact, not sampled.
    REQUIRE(peak_grade <= snow.p.bank_max_grade);
    // Exactly at the rise half's inflexion, where the peak lives (the 2000
    // sample sweep above straddles it, so it is read directly here).
    REQUIRE(std::abs(snow.bank_profile_slope(snow.p.bank_rise_m * 0.5) -
                     1.5 * snow.p.bank_height_m / snow.p.bank_rise_m) < 1e-12);
    // C1 at BOTH ends and at the crest, which is what lets the skirt join and
    // the crest ring shade without a seam.
    REQUIRE(std::abs(snow.bank_profile_slope(0.0)) < 1e-12);
    REQUIRE(std::abs(snow.bank_profile_slope(snow.p.bank_rise_m)) < 1e-12);
    REQUIRE(std::abs(snow.bank_profile_slope(end)) < 1e-12);
    REQUIRE(std::abs(snow.bank_profile_slope(end + 1.0)) < 1e-12);

    // (b) the composed geometry depth, on the real corridor. Walk OUTWARD from
    // a mid-run station (far from both run-endpoint junctions, so the gap
    // factor is ~1 and locally flat) and difference depth_geometry_at itself.
    const int mid = kStations / 2;
    const glm::dvec3 up = f.ctr[static_cast<std::size_t>(mid)];
    const glm::dvec3 north(0.0, 1.0, 0.0);
    const glm::dvec3 perp = glm::normalize(north - glm::dot(north, up) * up);
    auto dir_at = [&](double off_m) {
        return glm::normalize(up + perp * (off_m / kR));
    };
    const double step = 0.02;  // 2 cm: well inside the C1 curve's radius
    double worst2 = 0.0;
    long samples = 0;
    for (double e = 0.3; e < end - 0.3; e += 0.05) {
        const double off = kHalfW + e;
        const double fd = (snow.depth_geometry_at(dir_at(off + step)) -
                           snow.depth_geometry_at(dir_at(off - step))) /
                          (2.0 * step);
        const double an = snow.depth_geometry_slope_at(dir_at(off));
        worst2 = std::max(worst2, std::abs(fd - an));
        ++samples;
    }
    std::printf("[bank_slope] composed worst |fd-analytic| = %.4f m/m over "
                "%ld samples\n",
                worst2, samples);
    REQUIRE(samples > 100);
    // 2 cm central differences of a curve whose second derivative peaks near
    // 6*height/rise^2 carry O(h^2 * f''') truncation of their own, so this is
    // a tolerance on the DIFFERENCE method, not on the derivative.
    REQUIRE(worst2 < 0.01);
    // Inside the scraped roadway the drawn snow is flat by construction.
    REQUIRE(std::abs(snow.depth_geometry_slope_at(dir_at(kHalfW * 0.5))) <
            1e-12);
}

// --------------------------------------------------------------- leg 10 -----

TEST_CASE("bank_apron_zero_is_the_hand_built_strip") {
    // ★ ROAD-REPAIR RUNG 2 -- THE IDENTITY LEG, and it is graded against an
    // INDEPENDENT RULER. The expectation below is composed here, from the
    // field and the dials, by the law the header states in prose -- it never
    // calls build_bank_run a second time. A test that compares a function to
    // itself with a different default argument proves only that the default
    // is the default (the corner rung learned this the expensive way).
    Fixture f = make_fixture();
    world::SnowpackField snow;
    snow.hf = &f.hf;
    snow.lines = &f.net;
    snow.p = shipped_bank_params();

    render::BankBuildParams p;
    // ★ THE IDENTITY ARM, SET EXPLICITLY. [bank_mesh] apron_m ships 12.0 --
    // the struct default is 0.0 and they are deliberately different numbers,
    // so this leg names the value it is testing instead of inheriting one. A
    // leg that leaned on the default would quietly stop testing the identity
    // the day somebody changed the default, which is the same class of
    // mistake as grading a function against a second call to itself.
    p.apron_m = 0.0;
    REQUIRE(render::bank_apron_ring_count(p) == 0);
    std::vector<render::BankStripCPU> out;
    const long nverts =
        render::build_bank_run(f.ctr, f.s_arc, f.half_w, f.hf, kSubdiv, kTiles,
                               snow, p, {}, out);
    REQUIRE(nverts > 0);
    REQUIRE(!out.empty());

    // The ring column the builder must have used: the refined bank knots, then
    // skirt_rings burial sub-rings across skirt_m.
    const std::vector<double> knots =
        render::bank_ring_offsets(snow.p, p.chord_tol_m, p.max_rings);
    const int nb = static_cast<int>(knots.size());
    const int nsk = std::max(1, p.skirt_rings);
    const int nr = nb + nsk;
    const double end = snow.p.bank_rise_m + snow.p.bank_fall_m;

    long checked = 0;
    for (const render::BankStripCPU& s : out) {
        // Not one apron strip may exist at the identity value.
        REQUIRE_FALSE(s.is_apron);
        REQUIRE(s.rings == nr);
        const std::size_t nv = s.pos.size() / 3;
        REQUIRE(nv % static_cast<std::size_t>(nr) == 0);
        const std::size_t ns = nv / static_cast<std::size_t>(nr);
        REQUIRE(ns >= 2);
        // The index buffer is the grid's, exactly: two triangles per quad.
        REQUIRE(s.idx.size() == (ns - 1) * static_cast<std::size_t>(nr - 1) * 6);
        REQUIRE(s.nrm.size() == nv * 3);
        REQUIRE(s.uv.size() == nv * 2);

        for (std::size_t i = 0; i < ns; ++i) {
            // The station's own half-width is recovered from its INNER ring:
            // ring 0 sits at offset half_w + 0, so the inner ring's direction
            // IS the point the expectation is built around. (The fixture's
            // half-width is the constant kHalfW, which is what makes this a
            // hand-built expectation and not a recovery.)
            for (int r = 0; r < nr; ++r) {
                const std::size_t k =
                    i * static_cast<std::size_t>(nr) + static_cast<std::size_t>(r);
                const glm::dvec3 pos(s.pos[3 * k + 0], s.pos[3 * k + 1],
                                     s.pos[3 * k + 2]);
                const double radial = glm::length(pos);
                const glm::dvec3 dir = glm::normalize(pos);
                const double facet =
                    render::facet_radius_at(f.hf, dir, kSubdiv, kTiles);
                double expect = 0.0;
                if (r < nb) {
                    expect = snow.deck_floor_r(
                        dir, facet + static_cast<double>(p.lift_m) +
                                 snow.depth_geometry_at(dir));
                } else {
                    // The burial law, re-derived: the excess over the facet
                    // walks from the outer BANK ring's excess to -skirt_bury_m
                    // on a smoothstep across the sub-rings.
                    const std::size_t ko =
                        i * static_cast<std::size_t>(nr) +
                        static_cast<std::size_t>(nb - 1);
                    const glm::dvec3 po(s.pos[3 * ko + 0], s.pos[3 * ko + 1],
                                        s.pos[3 * ko + 2]);
                    const glm::dvec3 dout = glm::normalize(po);
                    const double h_out =
                        glm::length(po) -
                        render::facet_radius_at(f.hf, dout, kSubdiv, kTiles);
                    const double fr =
                        static_cast<double>(r - nb + 1) / nsk;
                    const double ss = fr * fr * (3.0 - 2.0 * fr);
                    expect = facet + h_out + (-p.skirt_bury_m - h_out) * ss;
                }
                REQUIRE(std::abs(radial - expect) <
                        1e-6 * std::max(1.0, std::abs(expect)));
                // The v tag is the grid's too, and the skirt still ends at the
                // shipped 1.25.
                const double vexp =
                    r < nb ? knots[static_cast<std::size_t>(r)] / end
                           : 1.0 + 0.25 * static_cast<double>(r - nb + 1) / nsk;
                REQUIRE(std::abs(static_cast<double>(s.uv[2 * k + 1]) - vexp) <
                        1e-6);
                ++checked;
            }
        }
    }
    REQUIRE(checked == nverts);
}

// --------------------------------------------------------------- leg 11 -----

TEST_CASE("bank_apron_reaches_the_driven_surface") {
    // ★ ROAD-REPAIR RUNG 2 -- THE CUT. On a road cut into a side-slope the
    // apron must exist on the falling side, ride the DRIVEN surface, open on
    // the main strip's outermost skirt ring vertex-for-vertex, and bury at its
    // own outer edge.
    Fixture f = make_fixture();
    f.hf = make_hf(256, 128, cross_slope);
    world::SnowpackField snow;
    snow.hf = &f.hf;
    snow.lines = &f.net;
    snow.p = shipped_bank_params();

    render::BankBuildParams p;
    p.apron_m = 12.0;
    p.apron_tol_m = 0.25;
    // The fixture's raster can only carry centimetres of fall across the 6 m
    // skirt (see cross_slope above), so the qualification threshold is set to
    // what this ground HAS. The law under test is the apron's shape, not the
    // shipped threshold's value.
    p.apron_min_drop_m = 0.005;
    const int nap = render::bank_apron_ring_count(p);
    REQUIRE(nap == 6);  // apron_m 12 / (skirt_m 6 / skirt_rings 3) = 6
    const int nra = 1 + nap + std::max(1, p.skirt_rings);

    std::vector<render::BankStripCPU> out;
    const long nverts =
        render::build_bank_run(f.ctr, f.s_arc, f.half_w, f.hf, kSubdiv, kTiles,
                               snow, p, {}, out);
    REQUIRE(nverts > 0);

    long apron_strips = 0, apron_verts = 0, bank_strips = 0;
    long lips = 0, buried = 0, on_driven = 0;
    for (const render::BankStripCPU& s : out) {
        if (!s.is_apron) {
            ++bank_strips;
            continue;
        }
        ++apron_strips;
        REQUIRE(s.rings == nra);
        const std::size_t nv = s.pos.size() / 3;
        apron_verts += static_cast<long>(nv);
        const std::size_t ns = nv / static_cast<std::size_t>(nra);
        REQUIRE(ns >= 2);
        for (std::size_t i = 0; i < ns; ++i) {
            for (int r = 0; r < nra; ++r) {
                const std::size_t k = i * static_cast<std::size_t>(nra) +
                                      static_cast<std::size_t>(r);
                const glm::dvec3 pos(s.pos[3 * k + 0], s.pos[3 * k + 1],
                                     s.pos[3 * k + 2]);
                const double radial = glm::length(pos);
                const glm::dvec3 dir = glm::normalize(pos);
                const double facet =
                    render::facet_radius_at(f.hf, dir, kSubdiv, kTiles);
                if (r == 0) {
                    // THE LIP: the main strip's outermost skirt ring, which is
                    // facet - skirt_bury_m by that strip's own law. Vertex-
                    // exact, so the two strips share an edge.
                    REQUIRE(std::abs(radial - (facet - p.skirt_bury_m)) <
                            1e-6 * facet);
                    ++lips;
                } else if (r <= nap) {
                    // THE APRON BODY: drive_radius_at, sampled. Not "facet +
                    // lift + depth" re-composed -- the function itself.
                    const double drv = snow.drive_radius_at(dir);
                    REQUIRE(std::abs(radial - drv) < 1e-6 * drv);
                    ++on_driven;
                } else if (r == nra - 1) {
                    // ...and it buries (F3: no free edge, ever).
                    REQUIRE(std::abs(radial - (facet - p.skirt_bury_m)) <
                            1e-6 * facet);
                    ++buried;
                }
                // Every apron vertex is plain snow in the bank FS (v > 1.2),
                // and carries a real unit normal.
                REQUIRE(s.uv[2 * k + 1] >= 1.25f);
                const glm::vec3 n(s.nrm[3 * k + 0], s.nrm[3 * k + 1],
                                  s.nrm[3 * k + 2]);
                REQUIRE(std::abs(glm::length(n) - 1.0f) < 1e-4f);
                REQUIRE(glm::dot(glm::dvec3(n), dir) > 0.0);
            }
        }
    }
    std::printf("[bank_apron] %ld apron strips, %ld apron verts, %ld lips, "
                "%ld driven rings, %ld buried edges (%ld bank strips)\n",
                apron_strips, apron_verts, lips, on_driven, buried,
                bank_strips);
    REQUIRE(apron_strips > 0);
    REQUIRE(lips > 0);
    REQUIRE(on_driven > 0);
    REQUIRE(buried > 0);
    REQUIRE(bank_strips > 0);

    // The apron is ON ONE SIDE: the side the ground falls away on. The other
    // side of a cut is a bank, and an apron there would be drawn geometry over
    // ground that is rising -- exactly the "never flatten the hill" line.
    // bank_apron_reach_m is the single function that decides it, so it is
    // asked directly, at a mid-run station, on both sides.
    const std::size_t mid = f.ctr.size() / 2;
    const glm::dvec3 up = f.ctr[mid];
    glm::dvec3 tg = f.ctr[mid + 1] - f.ctr[mid - 1];
    tg -= up * glm::dot(tg, up);
    const glm::dvec3 perp = glm::normalize(glm::cross(glm::normalize(tg), up));
    auto facet_fn = [&](glm::dvec3 d) {
        return render::facet_radius_at(f.hf, d, kSubdiv, kTiles);
    };
    auto drawn_fn = [&](glm::dvec3 d) {
        return render::drawn_radius_at(f.hf, d, kSubdiv, kTiles, &snow);
    };
    const double foot = kHalfW + snow.p.bank_rise_m + snow.p.bank_fall_m;
    const double rp = render::bank_apron_reach_m(
        snow, facet_fn, drawn_fn, up, perp, foot, p.skirt_m, p.apron_tol_m,
        p.apron_min_drop_m, nap, p.apron_m);
    const double rm = render::bank_apron_reach_m(
        snow, facet_fn, drawn_fn, up, -perp, foot, p.skirt_m, p.apron_tol_m,
        p.apron_min_drop_m, nap, p.apron_m);
    std::printf("[bank_apron] reach +side %.2f m, -side %.2f m\n", rp, rm);
    REQUIRE(((rp > 0.0) != (rm > 0.0)));  // exactly one side falls away
    REQUIRE(std::max(rp, rm) <= p.apron_m + 1e-9);
    // ...and the apron is OFF entirely at the identity, on the same ground.
    REQUIRE(render::bank_apron_reach_m(snow, facet_fn, drawn_fn, up, perp, foot,
                                       p.skirt_m, p.apron_tol_m,
                                       p.apron_min_drop_m, nap, 0.0) == 0.0);
}

// --------------------------------------------------------------- leg 12 -----

TEST_CASE("bank_apron_is_free_on_flat_ground") {
    // ★ ROAD-REPAIR RUNG 2: flat ground gets ZERO new vertices. The apron is
    // paid for by the +72 % bank-vertex debt this lane already owes (census
    // critic #10), so a feature that billed every station on the map for a
    // defect that lives on 12 % of them would not be shippable.
    Fixture f = make_fixture();  // flat_half
    world::SnowpackField snow;
    snow.hf = &f.hf;
    snow.lines = &f.net;
    snow.p = shipped_bank_params();

    render::BankBuildParams off;
    off.apron_m = 0.0;  // the identity arm, named (see leg 10)
    render::BankBuildParams on;
    on.apron_m = 12.0;
    on.apron_tol_m = 0.25;
    on.apron_min_drop_m = 0.5;  // the SHIPPED threshold

    std::vector<render::BankStripCPU> a, b;
    const long na = render::build_bank_run(f.ctr, f.s_arc, f.half_w, f.hf,
                                           kSubdiv, kTiles, snow, off, {}, a);
    const long nb2 = render::build_bank_run(f.ctr, f.s_arc, f.half_w, f.hf,
                                            kSubdiv, kTiles, snow, on, {}, b);
    REQUIRE(na > 0);
    REQUIRE(nb2 == na);
    REQUIRE(a.size() == b.size());
    for (const render::BankStripCPU& s : b) REQUIRE_FALSE(s.is_apron);
}
