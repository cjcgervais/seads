// ★ ROAD-REPAIR -- render/road_census.* gate legs.
//
// The census is an INSTRUMENT: it builds nothing and changes nothing, so what
// a gate can pin is that its arithmetic is the arithmetic it claims. Every leg
// here runs against an ANALYTIC ground (a flat synthetic HeightField + one
// straight synthetic plowed run, the test_bank_mesh.cpp fixture idiom) with
// the facet/drawn surfaces INJECTED -- which is exactly the seam the hooks
// exist for, and the reason the census can be gated at all.
//
// Legs:
//   1. census_pct_is_nearest_rank          -- the percentile the summary quotes
//   2. crest_is_measured_off_the_field     -- bank_crest_offset_m
//   3. census_row_arithmetic_is_the_claim  -- the four differences, per row
//   4. sink_and_step_counts_are_centreline -- road_census_stats bookkeeping
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <glm/geometric.hpp>

#include "render/road_census.h"
#include "world/heightfield.h"
#include "world/linework.h"
#include "world/snowpack.h"

namespace {

constexpr double kR = 15000.0;
constexpr double kPi = 3.14159265358979323846;
constexpr double kHalfW = 3.0;
constexpr double kRunLenM = 250.0;
constexpr int kStations = 51;

world::HeightField flat_hf() {
    world::HeightField hf;
    hf.w = 256;
    hf.h = 128;
    hf.R = kR;
    hf.relief_scale = 350.0;
    hf.u_offset = 0.0;
    // 0.5 everywhere -> a perfectly flat sphere of a constant radius.
    hf.px.assign(static_cast<std::size_t>(hf.w) * hf.h,
                 static_cast<std::uint16_t>(0.5 * 65535.0 + 0.5));
    return hf;
}

glm::dvec3 eq_dir(double t) {
    const double a = t * 2.0 * kPi;
    return glm::dvec3(std::sin(a), 0.0, std::cos(a));
}

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

struct Fixture {
    world::HeightField hf;
    world::LineNetwork net;
    render::BankRun run;
};

// One straight equatorial plowed run: the LineNetwork (so the snowpack sees a
// real corridor) and the matching BankRun the census walks.
Fixture make_fixture() {
    Fixture f;
    f.hf = flat_hf();
    const double t0 = 0.10;
    const double dtheta = kRunLenM / (2.0 * kPi * kR);
    const double t1 = t0 + dtheta;

    std::vector<glm::dvec3> v;
    std::vector<float> s;
    const double off = kHalfW / kR;
    const glm::dvec3 north(0.0, 1.0, 0.0);
    for (int i = 0; i < 60; ++i) {
        const double t = t0 + (t1 - t0) * i / 59.0;
        const glm::dvec3 p = eq_dir(t);
        v.push_back(glm::normalize(p + north * off));
        v.push_back(glm::normalize(p - north * off));
        const float arc = static_cast<float>((t - t0) * 2.0 * kPi * kR);
        s.push_back(arc);
        s.push_back(arc);
    }
    f.net.add_path(v.data(), s.data(), v.size(), world::LineKind::RoadMinor,
                   kR);
    f.net.build_index();

    f.run.path_id = 7;
    f.run.kind = 1;
    for (int i = 0; i < kStations; ++i) {
        const double t = t0 + (t1 - t0) * i / (kStations - 1.0);
        f.run.ctr.push_back(eq_dir(t));
        f.run.s.push_back(
            static_cast<float>(kRunLenM * i / (kStations - 1.0)));
        f.run.hw.push_back(kHalfW);
    }
    return f;
}

world::SnowpackField make_field(const Fixture& f) {
    world::SnowpackField snow;
    snow.p = shipped_bank_params();
    snow.hf = &f.hf;
    snow.lines = &f.net;
    return snow;
}

}  // namespace

// ---------------------------------------------------------------- leg 1 -----

TEST_CASE("census_pct_is_nearest_rank") {
    const std::vector<double> v = {5.0, 1.0, 4.0, 2.0, 3.0};
    CHECK(render::census_pct(v, 0.0) == 1.0);
    CHECK(render::census_pct(v, 0.5) == 3.0);
    CHECK(render::census_pct(v, 1.0) == 5.0);
    // Empty is 0, never UB -- the summary prints per-kind buckets that can be
    // empty (a planet with no trails, a pump with no road inside 2500 m).
    CHECK(render::census_pct({}, 0.5) == 0.0);
}

// ---------------------------------------------------------------- leg 2 -----

TEST_CASE("crest_is_measured_off_the_field") {
    const Fixture f = make_fixture();
    const world::SnowpackField snow = make_field(f);
    const std::vector<double> rings = render::bank_ring_offsets(snow.p);
    REQUIRE(rings.size() >= 3);

    // Mid-run station, far from both endpoints (whose junction gap fades the
    // bank away -- the leg would then be measuring the gap, not the crest).
    const std::size_t i = kStations / 2;
    const glm::dvec3 up = f.run.ctr[i];
    glm::dvec3 t = f.run.ctr[i + 1] - f.run.ctr[i - 1];
    t -= up * glm::dot(t, up);
    const glm::dvec3 perp = glm::normalize(glm::cross(glm::normalize(t), up));

    const double crest =
        render::bank_crest_offset_m(snow, up, perp, kHalfW, rings);
    // The crest is OUTSIDE the corridor edge and inside the profile's run.
    CHECK(crest >= kHalfW);
    CHECK(crest <= kHalfW + rings.back() + 1e-9);
    // And it really is the maximum: no ring carries more drawn depth.
    const glm::dvec3 dc = glm::normalize(up + perp * (crest / kR));
    const double best = snow.depth_geometry_at(dc);
    for (double r : rings) {
        const glm::dvec3 d = glm::normalize(up + perp * ((kHalfW + r) / kR));
        CHECK(snow.depth_geometry_at(d) <= best + 1e-12);
    }
}

// ---------------------------------------------------------------- leg 3 -----

TEST_CASE("census_row_arithmetic_is_the_claim") {
    const Fixture f = make_fixture();
    const world::SnowpackField snow = make_field(f);

    // ANALYTIC injected surfaces: facet == the field itself, drawn == the
    // field plus a known 0.31 m stand-in fold. Both reach the census the way
    // facet_radius_at reaches SnowpackField -- injected, never called by name.
    const world::HeightField* hf = &f.hf;
    constexpr double kFold = 0.31;
    render::RoadCensusHooks h;
    h.facet_r = [hf](glm::dvec3 d) { return hf->radius_at(d); };
    h.drawn_r = [hf](glm::dvec3 d) { return hf->radius_at(d) + kFold; };
    h.ribbon_lift_m = 0.45;
    h.bank_lift_m = 0.45;

    std::vector<render::RoadCensusRow> rows;
    render::road_census_run(f.run, snow, h, rows);

    const std::size_t per_station =
        render::road_census_lateral_fracs().size() + 1;  // + the crest
    REQUIRE(rows.size() == per_station * kStations);
    CHECK(rows[0].path_id == 7);
    CHECK(rows[0].kind == 1);

    std::size_t centres = 0, crests = 0;
    for (const render::RoadCensusRow& r : rows) {
        const double facet = hf->radius_at(r.dir);
        const world::SnowpackField::GroundSample g = snow.sample_at(r.dir);
        CHECK(std::fabs(r.drive_minus_facet - (g.drive_r - facet)) < 1e-9);
        CHECK(std::fabs(r.ribbon_minus_drive -
                        (facet + kFold + 0.45 - g.drive_r)) < 1e-9);
        CHECK(std::fabs(r.bank_minus_drive -
                        (facet + 0.45 + snow.depth_geometry_at(r.dir) -
                         g.drive_r)) < 1e-9);
        CHECK(std::fabs(r.drawn_minus_drive - (facet + kFold - g.drive_r)) <
              1e-9);
        CHECK(r.half_w_m == kHalfW);
        // The width is constant on this fixture, so EVERY C0 width delta is
        // zero -- a jag metric that fired here would be reading noise.
        CHECK(r.d_half_w_m == 0.0);
        if (r.is_centre) ++centres;
        if (r.is_crest) ++crests;
        // The row names the segment it measured whenever the corridor was
        // found at all.
        if (std::fabs(r.lateral_m) <= kHalfW) CHECK(r.seg_id >= 0);
    }
    CHECK(centres == static_cast<std::size_t>(kStations));
    CHECK(crests == static_cast<std::size_t>(kStations));
}

// ---------------------------------------------------------------- leg 4 -----

TEST_CASE("sink_and_step_counts_are_centreline") {
    std::vector<render::RoadCensusRow> rows;
    // Two stations worth of hand-built rows: one centreline row that sinks,
    // one that does not, and two off-centre rows that sink FAR more. The sink
    // count is a count of STATIONS the body stands in, so the off-centre rows
    // must not be counted.
    auto mk = [](double lat, double rib, double dhw) {
        render::RoadCensusRow r;
        r.lateral_m = lat;
        r.is_centre = (lat == 0.0);
        r.ribbon_minus_drive = rib;
        r.bank_minus_drive = 0.0;
        r.d_half_w_m = dhw;
        return r;
    };
    rows.push_back(mk(0.0, 0.42, 0.90));   // sinks; a 0.90 m width step
    rows.push_back(mk(3.0, 5.00, 0.90));   // off centre -- not a station
    rows.push_back(mk(0.0, 0.02, 0.10));   // flush; no step
    rows.push_back(mk(-3.0, 9.00, 0.10));  // off centre

    const render::RoadCensusStats s =
        render::road_census_stats(rows, "leg4", nullptr);
    CHECK(s.rows == 4);
    CHECK(s.centre_stations == 2);
    CHECK(s.sink_stations == 1);
    CHECK(s.width_steps == 1);
    CHECK(s.ribbon_max == 9.00);

    // The filter selects rows, and the counts follow it.
    const render::RoadCensusStats c = render::road_census_stats(
        rows, "centre-only",
        [](const render::RoadCensusRow& r) { return r.is_centre; });
    CHECK(c.rows == 2);
    CHECK(c.ribbon_max == 0.42);
}

// ---------------------------------------------------------------- leg 5 -----
//
// ★ ROAD-REPAIR ONAPING RUNG 1 -- the two new edge columns, on their SIGN.
//
// A column whose sign is wrong is worse than no column: it would rank the
// safe side of every road. So this leg does not check that the census agrees
// with itself -- it checks the two statements the names make, against an
// analytic ground the test builds:
//
//   * skirt_drop_m is ZERO on flat terrain, and on a CONSTANT cross-slope its
//     magnitude is |slope| x deck_skirt_m (the foot and the outer skirt ring
//     are exactly deck_skirt_m apart, whatever the ring set does), POSITIVE,
//     and reported on the side the ground FALLS AWAY on -- so REVERSING the
//     slope must FLIP skirt_side and leave the value alone.
//   * float_*x_m is `drawn - drive` at that multiple of half_w, measured
//     against the field directly rather than against the census's own answer.
TEST_CASE("onaping_edge_columns_carry_their_sign") {
    const Fixture f = make_fixture();
    const world::SnowpackField snow = make_field(f);
    const world::HeightField* hf = &f.hf;
    constexpr double kFold = 0.31;
    constexpr double kSlopeMag = 0.05;  // metres of fall per metre outward

    // --- arm A: FLAT. No cross-slope anywhere, so there is no fall to find.
    render::RoadCensusHooks h;
    h.facet_r = [hf](glm::dvec3 d) { return hf->radius_at(d); };
    h.drawn_r = [hf](glm::dvec3 d) { return hf->radius_at(d) + kFold; };
    std::vector<render::RoadCensusRow> flat;
    render::road_census_run(f.run, snow, h, flat);
    REQUIRE(!flat.empty());
    for (const render::RoadCensusRow& r : flat) {
        CHECK(std::fabs(r.skirt_drop_m) < 1e-9);
        CHECK((r.skirt_side == -1 || r.skirt_side == 1));
        CHECK((r.float_side == -1 || r.float_side == 1));
    }

    // The float triple, against the FIELD. The frame is rebuilt here the way
    // leg 2 rebuilds it (hand-built, not borrowed from the census), and the
    // fixture is symmetric about its centreline, so whichever side the census
    // picked reads the same numbers.
    {
        const std::size_t i = kStations / 2;
        const glm::dvec3 up = f.run.ctr[i];
        glm::dvec3 t = f.run.ctr[i + 1] - f.run.ctr[i - 1];
        t -= up * glm::dot(t, up);
        const glm::dvec3 perp =
            glm::normalize(glm::cross(glm::normalize(t), up));
        const std::size_t per_station =
            render::road_census_lateral_fracs().size() + 1;
        const render::RoadCensusRow& r = flat[i * per_station];
        const double want[3] = {r.float_2x_m, r.float_3x_m, r.float_4x_m};
        for (int k = 0; k < 3; ++k) {
            const glm::dvec3 d = glm::normalize(
                up + perp * (r.float_side * (k + 2) * kHalfW / kR));
            const double got =
                hf->radius_at(d) + kFold - snow.sample_at(d).drive_r;
            CHECK(std::fabs(want[k] - got) < 1e-9);
        }
    }

    // --- arms B and C: a CONSTANT cross-slope, then the same slope reversed.
    // The census never sees the heightfield here; the facet is the injected
    // plane, which is the whole point of the hook seam.
    auto tilted = [hf](double sign) {
        return [hf, sign](glm::dvec3 d) {
            return hf->radius_at(d) + sign * kSlopeMag * (d.y * kR);
        };
    };
    const double want_drop = kSlopeMag * snow.p.deck_skirt_m;
    int side_b = 0, side_c = 0;
    for (int arm = 0; arm < 2; ++arm) {
        render::RoadCensusHooks ht = h;
        ht.facet_r = tilted(arm == 0 ? +1.0 : -1.0);
        std::vector<render::RoadCensusRow> rows;
        render::road_census_run(f.run, snow, ht, rows);
        REQUIRE(!rows.empty());
        for (const render::RoadCensusRow& r : rows) {
            CHECK(r.skirt_drop_m > 0.0);
            CHECK(std::fabs(r.skirt_drop_m - want_drop) < 1e-4);
        }
        (arm == 0 ? side_b : side_c) = rows[0].skirt_side;
    }
    // The reported side is the side the ground falls away on, so it must
    // follow the terrain and not the frame.
    CHECK(side_b != 0);
    CHECK(side_b == -side_c);
}
