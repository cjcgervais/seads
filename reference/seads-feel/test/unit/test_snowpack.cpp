// Winter S2 -- THE SNOWPACK FUNCTION and the SURFACE LINEWORK, as executable
// law (WINTER_LAW sec2.2, sec2.3, sec2.4b, sec2.4c, sec6c.1, sec6c.2).
//
// Every leg here asserts an INVARIANT, not a behaviour I happened to write:
//
//   INV-1  ONE SURFACE          -- drive_radius_at is radius_at + depth_at and
//                                  nothing else; there is no second ground.
//   INV-3  THE HOMOGENIZER      -- terrain + depth is SMOOTHER than terrain.
//                                  Measured on synthetic terrain, not argued.
//   INV-6  ONE WIDTH PER CLASS  -- the physics corridor half-width is RECOVERED
//                                  from the drawn ribbon vertices, so it cannot
//                                  fork from what the GPU rasterizes.
//   INV-8  FRACTIONAL LANDMASK  -- a `mask > 0` test would put a band of
//                                  half-water at every shore. The leg fails if
//                                  anyone reintroduces one.
//   INV-9  ONE EXPOSURE FIELD   -- `barren` sheds OUR depth (sec6c.1's owed
//                                  absorb); rock exposure is never their field
//                                  alone.
//   sec2.4c THE RAMP BOUND      -- no gradient anywhere in the snowbank profile
//                                  exceeds bank_max_grade. A step stops a sled
//                                  dead or breaks the contact solver; this is
//                                  the number sec2.1c's "depth resolution is the
//                                  dial" finally acquires.
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <glm/geometric.hpp>
#include <vector>

#include <utility>

#include "world/linework.h"
#include "world/props.h"
#include "world/raster.h"
#include "world/snowpack.h"

namespace {

constexpr double kR = 15000.0;       // config/aircraft.toml world.R
constexpr double kRelief = 350.0;    // config/world.toml ground.relief_scale_m
constexpr double kPi = 3.14159265358979323846;

// A height field whose 16-bit samples come from a callable of (u,v) in [0,1].
world::HeightField make_hf(int w, int h, double (*f)(double, double)) {
    world::HeightField hf;
    hf.w = w;
    hf.h = h;
    hf.R = kR;
    hf.relief_scale = kRelief;
    hf.u_offset = 0.0;
    hf.px.resize(static_cast<std::size_t>(w) * h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const double v = std::clamp(f((x + 0.5) / w, (y + 0.5) / h), 0.0, 1.0);
            hf.px[static_cast<std::size_t>(y) * w + x] =
                static_cast<std::uint16_t>(v * 65535.0 + 0.5);
        }
    return hf;
}

double flat_half(double, double) { return 0.5; }

// A ridged terrain: a longitudinal sinusoid with a ~1.6 km wavelength at the
// equator, which is what the curvature probe is meant to see.
double ridges(double u, double) {
    return 0.5 + 0.35 * std::sin(u * 2.0 * 3.14159265358979323846 * 60.0);
}

// A GENTLE sinusoid -- ~785 m wavelength, ~2 m amplitude. Small enough that the
// slope/drainage terms are negligible, so the isolated homogenizer leg measures
// the curvature term and not the terrain's incidental character.
double gentle(double u, double) {
    return 0.5 + (2.0 / kRelief) *
                     std::sin(u * 2.0 * 3.14159265358979323846 * 120.0);
}

// A STEEP terrain for the slope-gate legs (Chad's fly ruling 2026-08-11): a
// single smooth tanh ramp centred at RASTER u=0.5, which is where the legs'
// probe eq_dir(0.25) actually lands -- equirect_uv is u = 0.5 + lon/(2*pi),
// so eq_dir's t and the raster's u differ by 0.5. A first draft centred the
// ramp at raster 0.25 and probed its SATURATED FLAT tail (gate sat at the
// floor). ★ Second trap, same draft: THE HORIZONTAL SCALE OF u IS THE EQUATOR
// CIRCUMFERENCE 2*pi*kR (~94.2 km), NOT kR -- dividing by kR made the
// "steep" ramp a 6.4 deg incline. The constant is chosen so the analytic
// gradient at the centre is
// relief_scale*0.45*420/(2*pi*kR) =~ tan(35deg) -- above
// barren_slope_hi_deg's default 12deg with a wide margin (so the
// gate saturates), yet safely BELOW slope_full_deg's default
// 45deg so the field's OWN independent slope-shed term does not also floor
// out the ambient depth here -- these legs need the BARREN shed isolated,
// not compounded with the everywhere slope law going to its own floor.
// The tanh transition spans ~6/420 of u (~1.3 km), so curv_probe_m=40 m sits
// deep in the near-linear region and the central-difference slope tracks the
// analytic one closely. The ramp saturates well inside (0,1) (0.05..0.95) so
// it never clamps.
double steep_ramp(double u, double) {
    return 0.5 + 0.45 * std::tanh((u - 0.5) * 420.0);
}

// Unit dir on the equator at longitude fraction t.
glm::dvec3 eq_dir(double t) {
    const double a = t * 2.0 * 3.14159265358979323846;
    return glm::dvec3(std::sin(a), 0.0, std::cos(a));
}

// One straight equatorial ribbon of constant half-width, emitted the way
// offline_tool/sudbury_ribbon.py::build_ribbon emits: L,R vertex pairs about
// each centerline station, plus the per-vertex arc length s.
struct Ribbon {
    std::vector<glm::dvec3> v;
    std::vector<float> s;
};
Ribbon straight_ribbon(double t0, double t1, int stations, double half_w_m) {
    Ribbon r;
    const double off = half_w_m / kR;  // tangent offset (small-angle)
    for (int i = 0; i < stations; ++i) {
        const double t = t0 + (t1 - t0) * i / (stations - 1.0);
        const glm::dvec3 p = eq_dir(t);
        const glm::dvec3 north(0.0, 1.0, 0.0);  // the tangent normal here
        r.v.push_back(glm::normalize(p + north * off));
        r.v.push_back(glm::normalize(p - north * off));
        const float arc = static_cast<float>((t - t0) * 2.0 * 3.14159265358979323846 * kR);
        r.s.push_back(arc);
        r.s.push_back(arc);
    }
    return r;
}

// Latitude-offset dir: `m` metres north of the equator at longitude fraction t.
glm::dvec3 north_of(double t, double m) {
    const glm::dvec3 p = eq_dir(t);
    return glm::normalize(p + glm::dvec3(0.0, 1.0, 0.0) * (m / kR));
}

world::Raster8 uniform_raster(int w, int h, double value) {
    world::Raster8 r;
    r.w = w;
    r.h = h;
    r.px.assign(static_cast<std::size_t>(w) * h,
                static_cast<std::uint8_t>(std::clamp(value, 0.0, 1.0) * 255.0 + 0.5));
    return r;
}

}  // namespace

// ---------------------------------------------------------------- linework --

TEST_CASE("INV-6: the corridor half-width is RECOVERED from the drawn ribbon") {
    // The law: "the render ribbon width and the physics hard-pack corridor
    // width must read the SAME number." Not "must be kept equal" -- the same
    // number. So the network is built from the vertices the GPU rasterizes,
    // and a width table is never consulted.
    const double half_w = 4.25;  // sec2.4b main trail, 8.5 m full
    const Ribbon rb = straight_ribbon(0.10, 0.12, 40, half_w);
    world::LineNetwork net;
    net.u_offset = 0.0;
    net.add_path(rb.v.data(), rb.s.data(), rb.v.size(), world::LineKind::Trail, kR);
    net.build_index();

    REQUIRE(net.st.size() == 40);
    for (const world::LineStation& s : net.st)
        REQUIRE(std::abs(s.half_w_m - half_w) < 1e-3);

    // ... and the centerline comes back where it was put, to the millimetre.
    for (int i = 0; i < 40; ++i) {
        const double t = 0.10 + 0.02 * i / 39.0;
        const double err =
            2.0 * kR * std::asin(0.5 * glm::length(net.st[i].dir - eq_dir(t)));
        REQUIRE(err < 0.01);
    }
}

TEST_CASE("linework: a run break is the arc-length RESET, not a distance guess") {
    // build_ribbon restarts s at 0 for every run it emits into one path (a
    // polyline split by the disk edge or clipped off a road corridor), so the
    // reset IS the break. Guessing from a distance threshold would merge two
    // runs whose ends happen to be close -- which is exactly what happens at
    // an intersection, where the merge would then erase a junction.
    Ribbon a = straight_ribbon(0.10, 0.11, 8, 3.0);
    const Ribbon b = straight_ribbon(0.30, 0.31, 8, 3.0);
    a.v.insert(a.v.end(), b.v.begin(), b.v.end());
    a.s.insert(a.s.end(), b.s.begin(), b.s.end());

    world::LineNetwork net;
    net.add_path(a.v.data(), a.s.data(), a.v.size(), world::LineKind::RoadMinor, kR);
    REQUIRE(net.st.size() == 16);
    REQUIRE(net.st[7].run == net.st[0].run);
    REQUIRE(net.st[8].run != net.st[7].run);
}

TEST_CASE("sec2.4b: the trail tier is the run's MEDIAN width, immune to miters") {
    // _miter_offsets inflates the offset at a corner by up to MITER_LIMIT (2x).
    // A per-station threshold would flip a tributary to "main" on every sharp
    // bend, so the tier is decided once per run from the median.
    Ribbon rb = straight_ribbon(0.10, 0.12, 21, 1.6);  // tributary, 3.2 m full
    // Spike ONE station to twice the width, as a miter would.
    {
        const glm::dvec3 p = eq_dir(0.11);
        const double off = 3.2 / kR;
        rb.v[20] = glm::normalize(p + glm::dvec3(0, 1, 0) * off);
        rb.v[21] = glm::normalize(p - glm::dvec3(0, 1, 0) * off);
    }
    world::LineNetwork net;
    net.add_path(rb.v.data(), rb.s.data(), rb.v.size(), world::LineKind::Trail, kR);
    for (const world::LineStation& s : net.st)
        REQUIRE(static_cast<world::LineKind>(s.kind) ==
                world::LineKind::TrailTributary);

    // A main trail at the ruled ~8-9 m full width classifies as main.
    const Ribbon big = straight_ribbon(0.10, 0.12, 21, 4.25);
    world::LineNetwork net2;
    net2.add_path(big.v.data(), big.s.data(), big.v.size(),
                  world::LineKind::Trail, kR);
    REQUIRE(static_cast<world::LineKind>(net2.st[0].kind) ==
            world::LineKind::TrailMain);
}

TEST_CASE("linework: distance is to the SEGMENT, never merely to a station") {
    // Stations sit up to RIBBON_DENSIFY_M = 80 m apart, which is WIDER than any
    // corridor. A nearest-STATION distance would report ~40 m for a point
    // standing in the middle of the road.
    //
    // The segment is a CHORD between two unit dirs, so it sags below the sphere
    // by R*(1-cos(theta/2)). At the bake's 80 m spacing that is 5.3 cm -- below
    // any depth the snowpack function resolves. The leg is written at that real
    // spacing on purpose: at an unrealistic 628 m spacing the same code reads
    // 4.45 m instead of 3.0, which is the chord sag and NOT a bug, and pinning
    // the tolerance against a made-up spacing would have hidden the real bound.
    const int n = 48;  // 0.10..0.12 of a turn == 1885 m => 40 m stations
    const Ribbon rb = straight_ribbon(0.10, 0.12, n, 6.0);
    world::LineNetwork net;
    net.add_path(rb.v.data(), rb.s.data(), rb.v.size(), world::LineKind::RoadMinor, kR);
    net.build_index();

    const double t_mid = 0.10 + 0.02 * 0.5 / (n - 1.0);  // between stations 0,1
    const world::LineHit h = net.nearest(north_of(t_mid, 3.0), 90.0);
    REQUIRE(h.found);
    REQUIRE(std::abs(h.dist_m - 3.0) < 0.10);
    REQUIRE(std::abs(h.half_w_m - 6.0) < 1e-2);
}

TEST_CASE("sec2.4c: junctions come off the road graph -- endpoints and crossings") {
    const Ribbon ew = straight_ribbon(0.10, 0.14, 60, 6.0);
    world::LineNetwork net;
    net.add_path(ew.v.data(), ew.s.data(), ew.v.size(), world::LineKind::RoadMinor, kR);
    // A second run crossing it: a short north-south segment through t = 0.12.
    {
        std::vector<glm::dvec3> v;
        std::vector<float> s;
        for (int i = 0; i < 20; ++i) {
            const double m = -200.0 + 400.0 * i / 19.0;
            const glm::dvec3 p = north_of(0.12, m);
            const glm::dvec3 side = glm::normalize(glm::cross(glm::dvec3(0, 1, 0), p));
            v.push_back(glm::normalize(p + side * (6.0 / kR)));
            v.push_back(glm::normalize(p - side * (6.0 / kR)));
            s.push_back(static_cast<float>(m + 200.0));
            s.push_back(static_cast<float>(m + 200.0));
        }
        net.add_path(v.data(), s.data(), v.size(), world::LineKind::RoadMinor, kR);
    }
    net.build_index();

    REQUIRE(!net.junctions.empty());
    // At the crossing there is a junction within a couple of metres ...
    REQUIRE(net.junction_dist(eq_dir(0.12), 120.0) < 8.0);
    // ... and 300 m down the straight there is not.
    REQUIRE(net.junction_dist(eq_dir(0.125), 120.0) > 100.0);
}

// ---------------------------------------------------------------- snowpack --

TEST_CASE("sec2.4c THE RAMP BOUND: no step anywhere in the snowbank profile") {
    // "The bank profile must be RESOLVED, not stepped. A depth STEP stops a
    // sled dead (or worse, makes the contact solver explode); a depth RAMP
    // launches it." This is the leg that number lives in.
    world::SnowpackField f;
    const double lo = 0.0, hi = f.p.bank_rise_m + f.p.bank_fall_m + 2.0;
    const double dx = 0.002;
    double prev = f.bank_profile(lo), worst = 0.0, crest = 0.0;
    for (double x = lo + dx; x <= hi; x += dx) {
        const double v = f.bank_profile(x);
        worst = std::max(worst, std::abs(v - prev) / dx);
        crest = std::max(crest, v);
        prev = v;
    }
    REQUIRE(worst <= f.p.bank_max_grade);
    // It must actually BE a bank: a crest at the ruled height, zero at both
    // ends. A profile that is flat everywhere would pass a grade bound.
    REQUIRE(std::abs(crest - f.p.bank_height_m) < 1e-3);
    REQUIRE(f.bank_profile(0.0) == 0.0);
    REQUIRE(f.bank_profile(hi) == 0.0);
}

TEST_CASE("sec2.4c: crossing a plowed road is a ramp, a bare roadway, and a bank") {
    world::HeightField hf = make_hf(256, 128, flat_half);
    const Ribbon rb = straight_ribbon(0.10, 0.14, 120, 6.0);
    world::LineNetwork net;
    net.add_path(rb.v.data(), rb.s.data(), rb.v.size(), world::LineKind::RoadMinor, kR);
    net.build_index();

    world::SnowpackField f;
    f.hf = &hf;
    f.lines = &net;

    const double t = 0.12;  // mid-run, far from the endpoint junctions
    const double ambient = f.ambient_depth_at(north_of(t, 400.0));
    REQUIRE(ambient > 0.3);

    // On the roadway: BARE (sec2.4c "roads read bare, never whitened"). The
    // REPORTED (sinkable) depth -- road_bare_m is unaffected by W2 (inside
    // the corridor width, e <= 0, no bank term is ever added).
    REQUIRE(std::abs(f.depth_at(eq_dir(t)) - f.p.road_bare_m) < 1e-9);
    REQUIRE(std::abs(f.depth_at(north_of(t, 3.0)) - f.p.road_bare_m) < 1e-9);

    // Walk out across the shoulder: no step, a crest above ambient, then
    // home. ★ W2.1: the RAMP BOUND and the crest height are about the
    // PHYSICAL bank a contact patch actually rides over -- that is
    // drive_radius_at's GEOMETRY term, which W2 leaves at the bank's FULL
    // amplitude by design (only the REPORTED/sinkable depth_at() is capped,
    // §PHASE W2.1). Reading depth_at() here after W2 would measure the cap,
    // not the bank, so this walk reads the geometry channel -- the exact
    // formula depth_at() used pre-W2 (geometry and reported agreed exactly
    // then), so the numbers below are unchanged from before W2 landed.
    auto geom_depth = [&](glm::dvec3 d) {
        return f.drive_radius_at(d) - hf.radius_at(d);
    };
    const double step = 0.05;
    double prev = geom_depth(north_of(t, 0.0)), worst = 0.0, crest = 0.0;
    double crest_at = 0.0;
    for (double m = step; m <= 40.0; m += step) {
        const double d = geom_depth(north_of(t, m));
        worst = std::max(worst, std::abs(d - prev) / step);
        if (d > crest) {
            crest = d;
            crest_at = m;
        }
        prev = d;
    }
    REQUIRE(worst <= f.p.bank_max_grade);
    REQUIRE(crest > ambient + 0.5 * f.p.bank_height_m);
    REQUIRE(crest_at > 6.0);   // outside the roadway
    REQUIRE(crest_at < 20.0);  // and not somewhere daft
    REQUIRE(std::abs(geom_depth(north_of(t, 40.0)) - ambient) < 1e-6);
}

TEST_CASE("sec2.4c: the banks are BROKEN -- amplitude dies at a junction") {
    world::HeightField hf = make_hf(256, 128, flat_half);
    const Ribbon rb = straight_ribbon(0.10, 0.14, 120, 6.0);
    world::LineNetwork net;
    net.add_path(rb.v.data(), rb.s.data(), rb.v.size(), world::LineKind::RoadMinor, kR);
    net.build_index();
    world::SnowpackField f;
    f.hf = &hf;
    f.lines = &net;

    const double ambient = f.ambient_depth_at(north_of(0.12, 400.0));
    // ★ W2.1: the GEOMETRY channel again (see the leg above) -- depth_at()
    // (reported) is capped by W2 and would read close to ambient everywhere,
    // mid-run or not, telling this leg nothing about whether the PHYSICAL
    // bank pile is there. drive_radius_at's geometry term is untouched by
    // W2's cap, so it is bit-identical to what depth_at() read here pre-W2.
    auto geom_depth = [&](glm::dvec3 d) {
        return f.drive_radius_at(d) - hf.radius_at(d);
    };
    // Mid-run there IS a bank ...
    const double mid = geom_depth(north_of(0.12, 10.0));
    REQUIRE(mid > ambient + 0.5 * f.p.bank_height_m);
    // ... and at the run's END (a junction by sec2.4c(a)) there is not, so the
    // crossing is a decision rather than a wall.
    const double at_end = geom_depth(north_of(0.10, 10.0));
    REQUIRE(at_end < ambient + 0.05);
}

TEST_CASE("sec2.3: a groomed trail is thin hard pack and grows NO bank") {
    world::HeightField hf = make_hf(256, 128, flat_half);
    const Ribbon rb = straight_ribbon(0.10, 0.14, 120, 4.25);
    world::LineNetwork net;
    net.add_path(rb.v.data(), rb.s.data(), rb.v.size(), world::LineKind::Trail, kR);
    net.build_index();
    world::SnowpackField f;
    f.hf = &hf;
    f.lines = &net;

    const double ambient = f.ambient_depth_at(north_of(0.12, 400.0));
    REQUIRE(std::abs(f.depth_at(eq_dir(0.12)) - f.p.trail_pack_m) < 1e-9);
    // A plow made no windrow here: nothing outside the trail exceeds ambient.
    for (double m = 4.25; m <= 30.0; m += 0.25)
        REQUIRE(f.depth_at(north_of(0.12, m)) <= ambient + 1e-9);
}

TEST_CASE("INV-1: the driven surface is radius_at + depth_at and nothing else") {
    world::HeightField hf = make_hf(256, 128, ridges);
    world::SnowpackField f;
    f.hf = &hf;
    for (int i = 0; i < 64; ++i) {
        const glm::dvec3 d = north_of(i / 64.0, (i % 7) * 300.0);
        REQUIRE(f.drive_radius_at(d) == hf.radius_at(d) + f.depth_at(d));
    }

    // ★ W1.5 (P1-5): with a corridor + deck_lift_m in play, INV-1's equality
    // no longer holds byte-for-byte -- drive_radius_at carries the GEOMETRY
    // lift term that depth_at (the REPORTED depth) never does, by W1.2's
    // design. So the divergence must be PINNED, not left to drift blind: on
    // the roadway it is exactly deck_lift_m, and (until W2's bank_pack_skin_m
    // cap lands) it is exactly deck_lift_m on the bank too, because pre-W2
    // the geometry and reported bank terms still agree.
    world::HeightField flat = make_hf(256, 128, flat_half);
    const Ribbon rb = straight_ribbon(0.10, 0.14, 120, 6.0);
    world::LineNetwork net;
    net.add_path(rb.v.data(), rb.s.data(), rb.v.size(), world::LineKind::RoadMinor, kR);
    net.build_index();
    world::SnowpackField fc;
    fc.hf = &flat;
    fc.lines = &net;
    fc.p.deck_lift_m = 0.60;  // an arbitrary nonzero test value

    const double t = 0.12;  // mid-run, far from the endpoint junctions
    // On the roadway centreline: divergence == lift, exactly.
    {
        const glm::dvec3 d = eq_dir(t);
        const double divergence =
            fc.drive_radius_at(d) - (flat.radius_at(d) + fc.depth_at(d));
        REQUIRE(std::abs(divergence - fc.p.deck_lift_m) < 1e-9);
    }
    // On the bank crest (outside the corridor, comfortably inside the
    // rise+fall ring so the lift is still full): divergence == lift +
    // (bank_geometry - bank_reported). ★ W2.1 (§PHASE W2 item 5) extension:
    // the bank_pack_skin_m cap opens a SECOND, independent divergence source
    // here -- drive_radius_at keeps the bank's FULL geometry contribution,
    // depth_at (reported) caps it at bank_pack_skin_m -- so the intended
    // divergence is PINNED to include it, not left to drift blind. half_w=6,
    // so e (distance beyond the corridor edge) = 9.5-6 = 3.5 m, well inside
    // bank_rise_m+bank_fall_m = 3+6 = 9 m, and far enough from either
    // endpoint junction that the gap term is exactly 1.0.
    {
        const glm::dvec3 d = north_of(t, 9.5);
        const double e = 9.5 - 6.0;
        const double bank_full = fc.bank_profile(e);  // gap == 1.0 here
        const double bank_reported =
            fc.p.bank_pack_skin_m >= 0.0
                ? std::min(bank_full, fc.p.bank_pack_skin_m)
                : bank_full;
        const double divergence =
            fc.drive_radius_at(d) - (flat.radius_at(d) + fc.depth_at(d));
        // 1e-6, not 1e-9: `e` here is a hand-computed 9.5-6.0, while
        // corridor_eval's internal e comes from the LineHit's own measured
        // great-circle dist_m (the exact chord/arc geometry the network
        // query does) -- the two agree to ~1e-8 m, not bit-for-bit, and
        // bank_profile's local gradient (~0.32 m/m on the falling side)
        // turns that into a divergence residual just over 1e-9.
        REQUIRE(std::abs(divergence -
                         (fc.p.deck_lift_m + (bank_full - bank_reported))) <
               1e-6);
    }
    // Out past the skirt entirely: the lift has decayed to 0, so INV-1's
    // original equality is restored.
    {
        const glm::dvec3 d = north_of(t, 400.0);
        const double divergence =
            fc.drive_radius_at(d) - (flat.radius_at(d) + fc.depth_at(d));
        REQUIRE(std::abs(divergence) < 1e-9);
    }
}

TEST_CASE("ROAD-REPAIR: drawn == driven on a road deck (the registration law)") {
    // The defect this pins (docs/road_repair/census_before.md sec1): the road
    // deck is DRAWN on drawn_radius_at + [ribbons] lift_m (the FOLDED facet --
    // the planet mesh's own ambient snow), while the drive was composed on the
    // UNFOLDED facet + road_bare_m + deck_lift_m. The corridor fold mask is a
    // 60 m ramp and the mesh cell is ~59 m, so the deck keeps a fold RESIDUAL
    // the mask cannot remove and stands above the surface the sled and the
    // walker are placed on: 32.9 % of drawn-ribbon stations, p50 +0.063 m,
    // max +0.608 m at the centreline. The body was inside the asphalt.
    //
    // The law: NEVER DRIVE BELOW WHAT IS DRAWN. On the deck the driven radius
    // is floored to the deck itself, so |drive_r - ribbon_r| collapses to the
    // road_bare_m shim; off the corridor nothing moves at all.
    world::HeightField flat = make_hf(256, 128, flat_half);
    const Ribbon rb = straight_ribbon(0.10, 0.14, 120, 6.0);
    world::LineNetwork net;
    net.add_path(rb.v.data(), rb.s.data(), rb.v.size(), world::LineKind::RoadMinor, kR);
    net.build_index();

    // The residual a real cell carries over a plowed corridor, stood in for
    // analytically: the drawn facet sits this far above the terrain facet.
    // 0.90 m is above the measured max (0.608 m centreline) on purpose -- the
    // law must hold at the tail, not merely at the median.
    const double kResidual = 0.90;

    world::SnowpackField off;  // the OFF arm: no injection, pre-repair
    off.hf = &flat;
    off.lines = &net;
    off.p.deck_lift_m = 0.45;  // [ribbons] lift_m, single-sourced in config

    world::SnowpackField on = off;  // same field, same dials
    on.drawn_radius_fn = [&flat, kResidual](glm::dvec3 d) {
        return flat.radius_at(d) + kResidual;
    };
    // THE DECK, composed exactly as render/ribbons.cpp drapes it.
    const auto deck_r = [&](glm::dvec3 d) {
        return on.drawn_radius_fn(d) + on.p.deck_lift_m;
    };

    const double t = 0.12;  // mid-run, far from the endpoint junctions

    // 1. THE ASK: on a road station the driven surface is the drawn deck, to
    //    within the road_bare_m scrape shim (0.02 m) and no more.
    for (int i = 0; i < 9; ++i) {
        const double lat = -5.5 + i * 1.375;  // across the 6 m half-width
        const glm::dvec3 d = north_of(t, lat);
        const double drive = on.sample_at(d).drive_r;
        REQUIRE(std::abs(drive - deck_r(d)) <= 0.02);
        // ... and it was NOT, before the floor: the deck stood
        // kResidual - road_bare_m above the old driven surface.
        REQUIRE(deck_r(d) - off.sample_at(d).drive_r >
                kResidual - on.p.road_bare_m - 1e-9);
        // sample_at and drive_radius_at may never disagree (INV-1): the floor
        // is applied through ONE body, off ONE corridor_eval.
        REQUIRE(std::abs(on.drive_radius_at(d) - drive) < 1e-12);
    }

    // 2. NEVER LOWERED. A drawn facet BELOW the driven surface is the fold
    //    mask working, not a defect -- the floor is one-sided and must leave
    //    it exactly alone, or the drawn mesh acquires authority to dig the
    //    ground out from under the sled wherever the facet dips.
    {
        world::SnowpackField low = off;
        low.drawn_radius_fn = [&flat](glm::dvec3 d) {
            return flat.radius_at(d) - 1.0;
        };
        const glm::dvec3 d = north_of(t, 0.0);
        REQUIRE(low.sample_at(d).drive_r == off.sample_at(d).drive_r);
    }

    // 3. OFF-CORRIDOR IS BIT-IDENTICAL. Out past the deck skirt the ramp
    //    weight is exactly 0, so every golden, tape and sled test that never
    //    touches a corridor cannot move -- not "within a tolerance": equal.
    for (int i = 0; i < 16; ++i) {
        const glm::dvec3 d = north_of(t, 40.0 + i * 60.0);
        REQUIRE(on.sample_at(d).drive_r == off.sample_at(d).drive_r);
        REQUIRE(on.drive_radius_at(d) == off.drive_radius_at(d));
    }

    // 4. NO STEP AT THE CORRIDOR EDGE. The floor rides the SAME fade the deck
    //    lift does (constant out to bank_rise_m+bank_fall_m, then a smoothstep
    //    across deck_skirt_m), so the added term is continuous from the deck
    //    all the way out to ambient. A step in the driven surface stops a sled
    //    dead (sec2.4c THE RAMP BOUND); this is that bound, for the floor.
    {
        const double end = on.p.bank_rise_m + on.p.bank_fall_m + on.p.deck_skirt_m;
        double prev = 0.0, worst = 0.0;
        bool first = true;
        for (double lat = 0.0; lat <= 6.0 + end + 2.0; lat += 0.05) {
            const glm::dvec3 d = north_of(t, lat);
            const double add = on.sample_at(d).drive_r - off.sample_at(d).drive_r;
            REQUIRE(add >= -1e-12);  // one-sided, everywhere
            if (!first) worst = std::max(worst, std::abs(add - prev));
            prev = add;
            first = false;
        }
        // 5 cm of lateral run may not move the floor by more than 5 cm: a
        // gradient bound of 1.0, an order under bank_max_grade's own regime,
        // and orders under what a STEP would read.
        REQUIRE(worst < 0.05);
        // And it has gone to exactly zero by the end of the skirt.
        const glm::dvec3 out = north_of(t, 6.0 + end + 1.0);
        REQUIRE(on.sample_at(out).drive_r == off.sample_at(out).drive_r);
    }

    // 5. ONE FUNCTION, TWO CONSUMERS. deck_floor_r is the public form the
    //    drawn bank strip (render/bank_mesh.cpp) composes its rings through;
    //    it must be the identical law the drive rides, or the inner ring --
    //    which sits exactly ON the corridor edge -- steps below the deck.
    for (int i = 0; i < 8; ++i) {
        const glm::dvec3 d = north_of(t, -6.0 + i * 1.5);
        // Fed the SAME unfloored radius the drive composes, it returns the
        // SAME floored one.
        const double unfloored = off.sample_at(d).drive_r;
        REQUIRE(std::abs(on.deck_floor_r(d, unfloored) -
                         on.sample_at(d).drive_r) < 1e-12);
    }
}

TEST_CASE("INV-3 THE HOMOGENIZER: terrain + depth is SMOOTHER than terrain") {
    // sec2.1c's claim, executable. Depth loads in concave curvature and scours
    // on convex, so the SUM is a curvature-weighted smoothing of the DEM --
    // which is why no micro-relief term exists (INV-2) and why ride feel is
    // tuned in this function rather than in the ground.
    //
    // ★ THE OTHER TERMS ARE SWITCHED OFF HERE, DELIBERATELY, and the first
    // version of this leg was WRONG for not doing it. Written against the full
    // field it passed -- and then PASSED AGAIN with the curvature term's sign
    // FLIPPED, because the slope and elevation terms happened to dominate the
    // rms. That is precisely the defect S0 threw RT-3 out for: a test that
    // survives the mutation named to kill it. Isolating the term makes the
    // inequality an exact statement about the homogenizer and nothing else --
    // for a wavelength lambda the reduction factor is 1 - curv_gain*(probe/4)*
    // (2*pi/lambda)^2, so it is smoothing iff the sign is right.
    //
    // (Magnitude, stated honestly: at these gains the effect is a fraction of a
    // percent per wavelength, and on the shipped DEM
    // offline_tool/measure_snowpack.py measures -7.84% rms. Snow does not
    // flatten a 1.6 km hill and the law never claimed it did -- the claim is
    // the DIRECTION, and that the dial lives here.)
    world::HeightField hf = make_hf(4096, 2048, gentle);
    world::SnowpackField f;
    f.hf = &hf;
    f.p.base_m = 1.0;  // high enough that nothing clamps at 0
    f.p.slope_shed = 0.0;
    f.p.elev_gain_per_km = 0.0;
    f.p.aspect_lee = 0.0;
    f.p.drain_gain = 0.0;

    auto curv_of = [&](glm::dvec3 d, bool with_snow) {
        const double a = f.p.curv_probe_m / kR;
        auto sample = [&](glm::dvec3 q) {
            q = glm::normalize(q);
            return hf.radius_at(q) + (with_snow ? f.ambient_depth_at(q) : 0.0);
        };
        const glm::dvec3 e1 = glm::normalize(glm::cross(glm::dvec3(0, 0, 1), d));
        const glm::dvec3 e2 = glm::cross(d, e1);
        const double s = sample(d + e1 * a) + sample(d - e1 * a) +
                         sample(d + e2 * a) + sample(d - e2 * a);
        return (0.25 * s - sample(d)) / f.p.curv_probe_m;
    };

    double sum_t = 0.0, sum_d = 0.0;
    int n = 0;
    for (int i = 0; i < 400; ++i) {
        const glm::dvec3 d = eq_dir(0.2 + 0.05 * i / 400.0);
        const double ct = curv_of(d, false), cd = curv_of(d, true);
        sum_t += ct * ct;
        sum_d += cd * cd;
        ++n;
    }
    const double rms_t = std::sqrt(sum_t / n), rms_d = std::sqrt(sum_d / n);
    REQUIRE(rms_t > 0.0);
    REQUIRE(rms_d < rms_t);  // ★ the law's claim, measured
}

TEST_CASE("sec2.2: hollows LOAD and crests SCOUR -- the sign of the whole law") {
    // The single most load-bearing sign in the winter game: it is what makes
    // the snowpack a homogenizer (INV-3), what puts bare rock on the convex
    // knobs the black-rock layer needs (sec6c/INV-9), and what independently
    // agreed with the black-rock agent's vegetation-kill term (sec6c.6).
    // Elevation and aspect are switched off so the comparison is curvature and
    // nothing else -- on a sinusoid the crest is also the HIGH ground, and the
    // elevation term alone would otherwise put more snow there and cancel it.
    world::HeightField hf = make_hf(4096, 2048, ridges);
    world::SnowpackField f;
    f.hf = &hf;
    f.p.elev_gain_per_km = 0.0;
    f.p.aspect_lee = 0.0;
    // The drainage term is off too, and that matters: it reads max(0, curv), so
    // it deepens hollows no matter which way the CURVATURE term points and
    // would carry this leg green through a sign flip.
    f.p.drain_gain = 0.0;

    // The crest and the hollow are found by ASKING THE HEIGHT FIELD over one
    // period, never by assuming a phase. equirect_uv is atan2(z, x) with a
    // +0.5 offset, so a longitude sweep runs BACKWARDS through texture u -- the
    // first draft of this leg assumed the phase, got crest and hollow exactly
    // the wrong way round, and read as a failure of the law instead of a
    // failure of the test's arithmetic.
    const double period = 1.0 / 60.0;
    double crest_u = 0.3, hollow_u = 0.3, hi = -1e18, lo = 1e18;
    for (int i = 0; i < 400; ++i) {
        const double t = 0.3 + period * i / 400.0;
        const double r = hf.radius_at(eq_dir(t));
        if (r > hi) { hi = r; crest_u = t; }
        if (r < lo) { lo = r; hollow_u = t; }
    }
    REQUIRE(hi - lo > 100.0);  // the synthetic terrain really does have relief
    const double d_crest = f.ambient_depth_at(eq_dir(crest_u));
    const double d_hollow = f.ambient_depth_at(eq_dir(hollow_u));
    REQUIRE(d_hollow > d_crest);
}

TEST_CASE("INV-8: the landmask is a FRACTION -- `mask > 0` is the bug") {
    world::HeightField hf = make_hf(256, 128, flat_half);
    world::SnowpackField f;
    f.hf = &hf;

    // A shore feather texel at 0.40 water is LAND, and its depth is a blend --
    // not the ice value, and not the dry value either.
    const world::Raster8 feather = uniform_raster(64, 32, 0.40);
    f.landmask = &feather;
    const glm::dvec3 d = eq_dir(0.25);
    REQUIRE(f.surface_at(d) != world::Surface::LakeIce);
    const double blended = f.depth_at(d);

    world::Raster8 dry = uniform_raster(64, 32, 0.0);
    f.landmask = &dry;
    const double land_depth = f.depth_at(d);
    REQUIRE(blended < land_depth);              // the water pulled it down ...
    REQUIRE(blended > f.p.ice_snow_m + 1e-6);   // ... but it is not ice yet

    const world::Raster8 lake = uniform_raster(64, 32, 0.90);
    f.landmask = &lake;
    REQUIRE(f.surface_at(d) == world::Surface::LakeIce);
    // The expected blend reads the raster's OWN value, not the 0.90 asked for:
    // an L8 texel quantizes 0.90 to 230/255. Writing 0.9 here would have made
    // the leg fail by 1.1e-3 for a reason that has nothing to do with the law.
    const double wf = lake.at(d);
    REQUIRE(std::abs(f.depth_at(d) -
                     ((1.0 - wf) * land_depth + wf * f.p.ice_snow_m)) < 1e-9);
}

TEST_CASE("sec6c.1 OWED: the black-rock shed is absorbed into f()") {
    // Their PROVISIONAL shader term was `snowCover *= (1 - 0.75*barren)`, taken
    // because there was no depth field to ask. It is the same physics, so it is
    // absorbed at their value rather than deleted -- and after this there is
    // ONE exposure field (INV-9), not two.
    //
    // ★ MOVED TO STEEP GROUND (Chad's fly ruling 2026-08-11, slope gate). The
    // shed is now GATED by slope -- GATE(slope)=1 only at/above
    // barren_slope_hi_deg. This leg's numbers were pinned against a bare
    // `1 - k*shed(b)` multiply, so it now runs on steep_ramp (slope ~35deg at
    // the probe, verified below to saturate the gate) rather than
    // flat terrain, keeping the SAME pinned relationship. The flat-ground
    // behaviour is its own leg now (sec-slope-gate ruling, below).
    world::HeightField hf = make_hf(4096, 128, steep_ramp);
    world::SnowpackField f;
    f.hf = &hf;
    const glm::dvec3 d = eq_dir(0.25);
    const double gate = f.barren_slope_gate(f.slope_at(d));
    // The design margin (~35deg vs hi=12deg) saturates the smoothstep; the
    // outer flat + (1-flat)*1 sum is float arithmetic, so tolerance not ==.
    REQUIRE(std::abs(gate - 1.0) < 1e-12);
    const double bare_field = f.depth_at(d);

    // ★ THE SHED IS A SATURATING CURVE, NOT A LINEAR MULTIPLY (Chad's fly
    // finding 2026-08-11, "the barrens read too light"). This leg used to pin
    // `bare_field * (1 - 0.75*b)`. That law could not make bare rock at ANY
    // field value: its floor was depth*0.25, i.e. 0.19 m at the shipped
    // ambient, while `bare_rock_depth_m` is 0.10 -- so §6c.2's RockOutcrop was
    // literally unreachable from the shed. `barren` is a COVERAGE product whose
    // MEASURED median on land is 0.212, so the typical shed was ~16%, not the
    // ~75% its authors believed.
    //
    // As in the INV-8 leg: the expectation reads the raster's OWN quantized
    // value (0.5 stores as 128/255), never the value asked for.
    const world::Raster8 half = uniform_raster(64, 32, 0.5);
    f.barren = &half;
    const double bq = half.at(d);
    REQUIRE(std::abs(f.depth_at(d) -
                     bare_field * (1.0 - f.p.k_barren * f.barren_shed(bq) *
                                              gate)) < 1e-9);

    // Above `barren_shed_hi` the shed SATURATES: a fully-killed crest carries
    // no ambient snow at all. This is the property the fly finding needed and
    // the old linear law could not express.
    const world::Raster8 full = uniform_raster(64, 32, 1.0);
    f.barren = &full;
    REQUIRE(std::abs(f.barren_shed(1.0) - 1.0) < 1e-12);
    REQUIRE(std::abs(f.depth_at(d)) < 1e-12);

    // ★ AND THE HALO IS NOT TOUCHED -- the whole point of a curve rather than a
    // bigger constant. At/below `barren_shed_lo` the shed is exactly zero, so
    // valley refugia and the outer feather keep their snow and Chad's signed
    // 0.77 m ambient is unchanged off the barrens.
    const world::Raster8 halo = uniform_raster(64, 32, 0.08);
    f.barren = &halo;
    REQUIRE(std::abs(f.barren_shed(halo.at(d))) < 1e-12);
    REQUIRE(std::abs(f.depth_at(d) - bare_field) < 1e-12);

    // The curve must be MONOTONIC and bounded -- a non-monotonic response would
    // make a MORE barren crest hold MORE snow.
    double prev = -1.0;
    for (int i = 0; i <= 20; ++i) {
        const double s = f.barren_shed(i / 20.0);
        REQUIRE(s >= prev);
        REQUIRE(s >= 0.0);
        REQUIRE(s <= 1.0);
        prev = s;
    }

    // k_barren = 0 must be an exact identity -- the term switches off cleanly.
    f.barren = &full;
    f.p.k_barren = 0.0;
    REQUIRE(f.depth_at(d) == bare_field);
}

TEST_CASE("sec6c.2: Rock outcrop needs THEIR barren AND our depth gone") {
    // "barren > 0.5 => Rock outcrop" is their half. INV-9 is ours: we own
    // depth, so a barren crest still buried under a metre of drift is not a
    // rock outcrop today, whatever the raster says about the soil.
    //
    // ★ MOVED TO A SLOPED PROBE (Chad's fly ruling 2026-08-11, slope gate).
    // The ruling put the rock on the SLOPES: at the gate's default flat floor
    // (0.30) a fully-shed flat point still keeps 70% of its ambient depth,
    // which no longer clears bare_rock_depth_m at the shipped base_m -- so
    // RockOutcrop is unreachable on flat ground by design now (see the
    // dedicated slope-gate ruling leg below), and this leg must probe steep
    // ground (gate ~= 1) to still observe a rock class from barren alone.
    world::HeightField hf = make_hf(4096, 128, steep_ramp);
    world::SnowpackField f;
    f.hf = &hf;
    const world::Raster8 rock = uniform_raster(64, 32, 0.9);
    f.barren = &rock;
    const glm::dvec3 d = eq_dir(0.25);

    // ★ THIS LEG'S EXPECTATION INVERTED ON PURPOSE (2026-08-11). It used to
    // assert that barren 0.9 stays BUSH, on the reasoning that a 67.5% shed of
    // ~0.76 m still leaves enough to bury the rock. That was only ever true
    // because the LINEAR shed was too weak -- the same defect Chad flew as "the
    // barrens read too light", and the reason RockOutcrop was unreachable and
    // therefore DEAD CODE. Under the saturating curve a fully-killed crest
    // carries no snow, so it classes as rock, which is what §6c.2 always meant.
    REQUIRE(f.depth_at(d) < f.p.bare_rock_depth_m);
    REQUIRE(f.surface_at(d) == world::Surface::RockOutcrop);

    // ★ INV-9 IS STILL THE POINT, and it is what this leg actually protects:
    // the class is a CONJUNCTION of their field and OUR depth, never their
    // field alone. Switch our shed off and the same barren 0.9 point is buried
    // again -- so `surface_at` is provably consulting our depth, not just
    // thresholding their raster.
    f.p.k_barren = 0.0;
    REQUIRE(f.depth_at(d) > f.p.bare_rock_depth_m);
    REQUIRE(f.surface_at(d) == world::Surface::Bush);
    f.p.k_barren = 1.0;

    // ...and a point their raster calls barren stays BUSH while our depth
    // survives for any other reason (here: a deeper ambient than the shed can
    // remove, via the class threshold rather than the field).
    f.p.bare_rock_depth_m = 0.0;  // nothing is ever "snow-free enough"
    REQUIRE(f.surface_at(d) == world::Surface::Bush);
    f.p.bare_rock_depth_m = 0.10;

    // Scour the ambient field away and the same point IS rock.
    f.p.base_m = 0.05;
    REQUIRE(f.surface_at(d) == world::Surface::RockOutcrop);
}

TEST_CASE("slope gate: flat floor, saturates above hi, monotonic, exact x") {
    // Chad's fly ruling 2026-08-11: flat barren ground should be snow-
    // covered; sloped/steep barren faces should be the blackest.
    // GATE(slope) = flat + (1-flat)*smoothstep(x_lo,x_hi,1-cos(slope)) is a
    // PURE function of slope + the params -- no terrain needed to probe it.
    world::SnowpackField f;  // f.hf is null; barren_slope_gate never touches it

    // Flat ground (slope 0): 1-cos(0)=0, strictly below x_lo, so the gate
    // sits at its FLOOR -- exactly flat_frac, the load-bearing floor that
    // keeps the hero layer's lobes visible on flats.
    REQUIRE(f.barren_slope_gate(0.0) == f.p.barren_flat_shed_frac);

    // At/above barren_slope_hi_deg the gate is FULL (bare rock reachable).
    const double hi_rad = f.p.barren_slope_hi_deg * kPi / 180.0;
    REQUIRE(std::abs(f.barren_slope_gate(hi_rad) - 1.0) < 1e-12);
    REQUIRE(std::abs(f.barren_slope_gate(hi_rad + 0.3) - 1.0) < 1e-12);

    // Monotonic and bounded across the whole [0, 90deg] range -- a
    // non-monotonic response would make a MORE sloped face hold MORE snow.
    double prev = -1.0;
    for (int i = 0; i <= 90; ++i) {
        const double s = f.barren_slope_gate(i * kPi / 180.0);
        REQUIRE(s >= prev - 1e-15);
        REQUIRE(s >= f.p.barren_flat_shed_frac - 1e-12);
        REQUIRE(s <= 1.0);
        prev = s;
    }

    // The degree -> `1-cos` helpers match their definition exactly, and are
    // the ONE home of that conversion (the shader receives their output as
    // uniforms rather than re-deriving it).
    REQUIRE(std::abs(f.p.barren_slope_x_lo() -
                     (1.0 - std::cos(f.p.barren_slope_lo_deg * kPi /
                                     180.0))) < 1e-15);
    REQUIRE(std::abs(f.p.barren_slope_x_hi() -
                     (1.0 - std::cos(f.p.barren_slope_hi_deg * kPi /
                                     180.0))) < 1e-15);
}

TEST_CASE("slope gate ruling: flat barrens stay rideable, steep barrens shed fully") {
    // The RULING itself, pinned as a behaviour on real terrain (Chad's fly
    // 2026-08-11): "flat barren ground should be snow-covered; sloped/steep
    // barren faces should be the blackest." b=1.0 (fully-killed crest) is the
    // worst case for snow retention.
    world::HeightField hf_flat = make_hf(256, 128, flat_half);
    world::HeightField hf_steep = make_hf(4096, 128, steep_ramp);
    const world::Raster8 full = uniform_raster(64, 32, 1.0);
    const glm::dvec3 d = eq_dir(0.25);

    // FLAT point: the gate's floor keeps depth >= (1 - k*flat_frac) of the
    // un-shed ambient -- rideable snow, not rock.
    world::SnowpackField flat;
    flat.hf = &hf_flat;
    const double flat_bare = flat.depth_at(d);
    flat.barren = &full;
    REQUIRE(std::abs(flat.slope_at(d)) < 1e-9);  // confirms this probe IS flat
    const double flat_gated = flat.depth_at(d);
    REQUIRE(flat_gated >=
           flat_bare * (1.0 - flat.p.k_barren * flat.p.barren_flat_shed_frac) -
               1e-9);
    REQUIRE(flat.surface_at(d) == world::Surface::Bush);  // rideable, not rock

    // STEEP point (>= barren_slope_hi_deg): the shed is unopposed -- it sheds
    // FULLY and classes RockOutcrop, which is the whole point of moving the
    // rock to the slopes.
    world::SnowpackField steep;
    steep.hf = &hf_steep;
    steep.barren = &full;
    const double hi_rad = steep.p.barren_slope_hi_deg * kPi / 180.0;
    REQUIRE(steep.slope_at(d) >= hi_rad);  // confirms this probe IS steep
    REQUIRE(std::abs(steep.depth_at(d)) < 1e-9);
    REQUIRE(steep.surface_at(d) == world::Surface::RockOutcrop);
}

TEST_CASE("sec2.3: the surface classes are a corridor read, and MineWorks wins") {
    world::HeightField hf = make_hf(256, 128, flat_half);
    const Ribbon road = straight_ribbon(0.10, 0.14, 120, 6.0);
    const Ribbon trail = straight_ribbon(0.30, 0.34, 120, 4.25);
    world::LineNetwork net;
    net.add_path(road.v.data(), road.s.data(), road.v.size(),
                 world::LineKind::RoadMinor, kR);
    net.add_path(trail.v.data(), trail.s.data(), trail.v.size(),
                 world::LineKind::Trail, kR);
    net.build_index();
    world::SnowpackField f;
    f.hf = &hf;
    f.lines = &net;

    REQUIRE(f.surface_at(eq_dir(0.12)) == world::Surface::Road);
    REQUIRE(f.surface_at(eq_dir(0.32)) == world::Surface::TrailMain);
    REQUIRE(f.surface_at(north_of(0.32, 200.0)) == world::Surface::Bush);
    // The excavation hook: a tunnel/pit interior is MineWorks whatever is
    // painted above it (sec2.3 "tunnels stay snow-free").
    REQUIRE(f.surface_at(eq_dir(0.12), true) == world::Surface::MineWorks);
}

TEST_CASE("snowpack: pure and deterministic -- no state, no clock, no order") {
    world::HeightField hf = make_hf(512, 256, ridges);
    world::SnowpackField f;
    f.hf = &hf;
    std::vector<double> first;
    for (int i = 0; i < 200; ++i) first.push_back(f.depth_at(eq_dir(i / 200.0)));
    for (int i = 199; i >= 0; --i)
        REQUIRE(f.depth_at(eq_dir(i / 200.0)) == first[i]);
}

TEST_CASE("snowpack: an absent source switches its term off by DATA, not branch") {
    // Every optional source is a null/empty pointer away from off. That is what
    // lets the black-rock raster be missing from this tree without the winter
    // side growing an `#ifdef HAVE_BARREN`.
    world::SnowpackField f;
    REQUIRE(f.depth_at(eq_dir(0.1)) == 0.0);  // no height field at all
    world::HeightField hf = make_hf(64, 32, flat_half);
    f.hf = &hf;
    const world::Raster8 nothing;  // w == h == 0
    f.barren = &nothing;
    f.landmask = &nothing;
    const double d = f.depth_at(eq_dir(0.1));
    f.barren = nullptr;
    f.landmask = nullptr;
    REQUIRE(f.depth_at(eq_dir(0.1)) == d);
}

TEST_CASE("sec2.4b: the trail corridor CLEARS the canopy, and only the canopy") {
    // Chad's S2 fly: "snow machine trails still have trees on it." sec2.4b:
    // "with all large trees collidable, a trail with trees standing in it is
    // not a trail -- and that mask is exactly what makes a woods trail
    // identifiable." So the corridor suppresses the scatter out to the DRAWN
    // ribbon width plus a margin.
    world::HeightField hf = make_hf(512, 256, flat_half);
    const Ribbon rb = straight_ribbon(0.10, 0.14, 200, 4.25);
    world::LineNetwork net;
    net.add_path(rb.v.data(), rb.s.data(), rb.v.size(), world::LineKind::Trail, kR);
    net.build_index();

    world::DensityField dense;  // uniform full density: trees everywhere
    dense.w = 64;
    dense.h = 32;
    dense.px.assign(64 * 32, 255);

    world::TreeParams tp;
    tp.cells_per_face = 256;
    tp.chunk_cells = 256;
    tp.gain = 1.0;

    world::CorridorMask mask;
    mask.lines = &net;
    mask.margin_m = 3.0;

    // The chunk that contains the ribbon. Face/chunk chosen by asking the
    // scatter itself rather than by assuming an index.
    struct Counts {
        int on_running_surface = 0;  // dist <= half_w  (a trunk IN the trail)
        int in_cleared_band = 0;     // dist <= half_w + margin (what the mask takes)
        int total = 0;
    };
    auto survey = [&](const world::CorridorMask& m) {
        Counts c;
        for (int face = 0; face < 6; ++face) {
            const auto inst =
                world::place_chunk(face, 0, 0, hf, dense, tp, {}, m);
            c.total += static_cast<int>(inst.size());
            for (const world::TreeInstance& t : inst) {
                const world::LineHit h = net.nearest(t.up, 90.0);
                if (!h.found) continue;
                if (h.dist_m <= h.half_w_m) ++c.on_running_surface;
                if (h.dist_m <= h.half_w_m + mask.margin_m) ++c.in_cleared_band;
            }
        }
        return c;
    };

    const Counts masked = survey(mask);
    const Counts unmasked = survey(world::CorridorMask{});

    // The unmasked scatter really does put trunks in the trail -- otherwise
    // this leg would pass on a world where the bug could not occur.
    REQUIRE(unmasked.on_running_surface > 0);
    // ... and the mask clears the whole band, running surface and margin both.
    REQUIRE(masked.on_running_surface == 0);
    REQUIRE(masked.in_cleared_band == 0);
    // ★ AND IT TOUCHES NOTHING ELSE. The suppression sits in the same
    // reject-the-candidate band as the cut test, BEFORE the density read, so no
    // acceptance hash is consumed and the bush outside the corridor is
    // bit-identical. A mask that quietly re-rolled the whole scatter would
    // "fix" the trail and move every tree in the world -- so the survivor count
    // must be EXACTLY the total minus the cleared band, not merely close.
    //
    // (The first version of this leg subtracted `on_running_surface` here and
    // read 7 short. That was the leg forgetting its own 3 m margin, not the
    // mask over-clearing -- worth keeping, because "the test was wrong" and
    // "the code cleared too much" look identical in the failure output.)
    REQUIRE(masked.total == unmasked.total - unmasked.in_cleared_band);
}

TEST_CASE("sec2.3: every surface class has a name the HUD can print") {
    for (int i = 0; i < static_cast<int>(world::Surface::kCount); ++i) {
        const char* n = world::surface_name(static_cast<world::Surface>(i));
        REQUIRE(n != nullptr);
        REQUIRE(n[0] != '\0');
    }
}

// ------------------------------------------------- W1.4/W1.5 (deck registration)

TEST_CASE("snowpack_road_deck_registration_headless") {
    // ★ W1.4: the registration leg. deck_lift_m (W1.2) enters ONLY the
    // GEOMETRY term (drive_radius_at); the REPORTED depth (depth_at) never
    // moves. KILLS: deck_lift folded into depth_at (centreline/trail
    // assertions below would then read road_bare_m/trail_pack_m + 2*lift, or
    // depth_at itself would move off road_bare_m/trail_pack_m).
    world::HeightField flat = make_hf(256, 128, flat_half);

    // -- ROAD (plowed) --------------------------------------------------
    const Ribbon road = straight_ribbon(0.10, 0.14, 120, 6.0);
    world::LineNetwork road_net;
    road_net.add_path(road.v.data(), road.s.data(), road.v.size(),
                      world::LineKind::RoadMinor, kR);
    road_net.build_index();
    world::SnowpackField fr;
    fr.hf = &flat;
    fr.lines = &road_net;
    fr.p.deck_lift_m = 0.45;  // shipped [ribbons] lift_m
    // ★ W2.1: this leg is scoped to the DECK-LIFT registration (W1.2/W1.3)
    // alone -- its shoulder samples below (e = 3/6/9 m) fall inside the bank
    // ring, where §PHASE W2's bank_pack_skin_m cap now opens a SECOND,
    // unrelated divergence between drive_radius_at (full bank) and depth_at
    // (capped bank) even at deck_lift_m's own shipped value. Disabling W2 by
    // its own one-switch sentinel here keeps this leg testing exactly what it
    // always tested (the lift term only) -- W2's own divergence is pinned
    // separately, in INV-1 (test_snowpack.cpp) and the new snowbank_* legs.
    fr.p.bank_pack_skin_m = -1.0;

    const double t = 0.12;  // mid-run, far from the endpoint junctions
    const glm::dvec3 centreline = eq_dir(t);

    // Centreline: depth stays road_bare_m; drive - radius == lift + bare.
    REQUIRE(std::abs(fr.depth_at(centreline) - fr.p.road_bare_m) < 1e-9);
    REQUIRE(std::abs((fr.drive_radius_at(centreline) - flat.radius_at(centreline)) -
                     (fr.p.deck_lift_m + fr.p.road_bare_m)) < 1e-9);

    // -- TRAIL (groomed, not plowed) -------------------------------------
    const Ribbon trail = straight_ribbon(0.30, 0.34, 120, 4.25);
    world::LineNetwork trail_net;
    trail_net.add_path(trail.v.data(), trail.s.data(), trail.v.size(),
                       world::LineKind::Trail, kR);
    trail_net.build_index();
    world::SnowpackField ft;
    ft.hf = &flat;
    ft.lines = &trail_net;
    ft.p.deck_lift_m = 0.45;

    const double tt = 0.32;
    const glm::dvec3 trail_pt = eq_dir(tt);
    REQUIRE(std::abs(ft.depth_at(trail_pt) - ft.p.trail_pack_m) < 1e-9);
    REQUIRE(std::abs((ft.drive_radius_at(trail_pt) - flat.radius_at(trail_pt)) -
                     (ft.p.deck_lift_m + ft.p.trail_pack_m)) < 1e-9);

    // -- SHOULDER SAMPLES: the W1.3 envelope, e = 3, 6, 9, 12 m beyond the
    // roadway edge (half_w = 6 m). bank_rise_m + bank_fall_m = 3 + 6 = 9 m
    // is the shipped ring extent, so e = 9 sits exactly at the ring edge --
    // the boundary where the formula switches from the constant plateau to
    // the smoothstep ramp branch. deck_skirt_m is set to 3.0 here (a
    // test-local choice distinct from the shipped [bank_mesh] skirt_m = 6.0)
    // so the ramp finishes EXACTLY at e = 12, giving the four samples the
    // clean "constant, constant, ramp-edge, zero" pattern W1.3 describes.
    fr.p.deck_skirt_m = 3.0;
    auto lift_at = [&](double e) {
        const glm::dvec3 d = north_of(t, 6.0 + e);  // half_w + e
        return fr.drive_radius_at(d) - (flat.radius_at(d) + fr.depth_at(d));
    };
    const double l3 = lift_at(3.0);
    const double l6 = lift_at(6.0);
    const double l9 = lift_at(9.0);
    const double l12 = lift_at(12.0);
    REQUIRE(std::abs(l3 - fr.p.deck_lift_m) < 1e-9);   // constant
    REQUIRE(std::abs(l6 - fr.p.deck_lift_m) < 1e-9);   // constant
    REQUIRE(std::abs(l9 - fr.p.deck_lift_m) < 1e-9);   // the ring edge: still
                                                       // full, but this is
                                                       // where the ramp branch
                                                       // begins (C1, so the
                                                       // value agrees at the
                                                       // seam)
    REQUIRE(l12 < 1e-9);                              // ramp complete: zero
    // And it really is a RAMP in between, not a second step: strictly
    // decreasing from the ring edge to the skirt's end.
    REQUIRE(lift_at(10.0) < l9);
    REQUIRE(lift_at(11.0) < lift_at(10.0));
    REQUIRE(l12 < lift_at(11.0));

    std::printf(
        "snowpack_road_deck_registration_headless: centreline drive-radius=%.6f "
        "road_bare-part=%.6f lift-part=%.6f | trail drive-radius=%.6f "
        "trail_pack-part=%.6f lift-part=%.6f | shoulder e=3/6/9/12 lift="
        "%.6f/%.6f/%.6f/%.6f\n",
        fr.drive_radius_at(centreline) - flat.radius_at(centreline),
        fr.p.road_bare_m, fr.p.deck_lift_m,
        ft.drive_radius_at(trail_pt) - flat.radius_at(trail_pt),
        ft.p.trail_pack_m, ft.p.deck_lift_m, l3, l6, l9, l12);
}

TEST_CASE("snowpack_deck_lift_off_is_bit_identical") {
    // ★ W1.4: deck_lift_m's struct default is 0.0 (world/snowpack.h), so
    // every hand-built SnowpackField that never touches the field must stay
    // bit-identical to the pre-W1.2 world: drive_radius_at == radius_at +
    // depth_at everywhere, on bush, trail, and road alike. KILLS: any
    // envelope arithmetic (W1.3's smoothstep ramp) that leaks a nonzero
    // contribution even when deck_lift_m itself is 0 (e.g. a stray additive
    // constant instead of a multiply-by-deck_lift_m).
    world::HeightField hf = make_hf(256, 128, ridges);
    const Ribbon road = straight_ribbon(0.10, 0.14, 120, 6.0);
    world::LineNetwork road_net;
    road_net.add_path(road.v.data(), road.s.data(), road.v.size(),
                      world::LineKind::RoadMinor, kR);
    road_net.build_index();
    const Ribbon trail = straight_ribbon(0.30, 0.34, 120, 4.25);
    world::LineNetwork trail_net;
    trail_net.add_path(trail.v.data(), trail.s.data(), trail.v.size(),
                       world::LineKind::Trail, kR);
    trail_net.build_index();

    world::SnowpackField fr;
    fr.hf = &hf;
    fr.lines = &road_net;
    REQUIRE(fr.p.deck_lift_m == 0.0);  // the default itself, asserted
    // ★ W2.1: OUT OF SCOPE for this leg, by the same reasoning as the
    // registration leg above -- bank_pack_skin_m's cap (SHIPPED default
    // 0.065) is a SEPARATE divergence source from deck_lift_m, and it fires
    // on the road's own bank ring regardless of deck_lift_m's value. This
    // leg's claim is "deck_lift_m == 0 keeps INV-1 exact" specifically, so
    // W2 is switched off by its own sentinel to keep testing exactly that.
    fr.p.bank_pack_skin_m = -1.0;

    world::SnowpackField ft;
    ft.hf = &hf;
    ft.lines = &trail_net;

    for (int i = 0; i < 64; ++i) {
        const double t = 0.10 + 0.04 * i / 63.0;
        // centreline/on-corridor, and a fan of shoulder distances spanning
        // the whole envelope built in W1.3 (roadway, bank ring, skirt, and
        // well beyond).
        for (double m : {0.0, 3.0, 6.0, 9.0, 12.0, 20.0, 60.0, 400.0}) {
            const glm::dvec3 d = north_of(t, m);
            REQUIRE(fr.drive_radius_at(d) == hf.radius_at(d) + fr.depth_at(d));
            REQUIRE(ft.drive_radius_at(d) == hf.radius_at(d) + ft.depth_at(d));
        }
    }
}

// ---------------------------------------------- PHASE W2 (snowbank hardpack)

TEST_CASE("snowbank_crest_class_is_trailmain") {
    // ★ W2.2 (P2-6): surface_at()/classify() read TrailMain at the bank crest
    // once the (junction-gap-suppressed) amplitude clears bank_class_min_m --
    // gated on the SAME sentinel as W2.1's reported-depth cap
    // (bank_pack_skin_m), one switch for both. 1 m past the drawn bank ring
    // (bank_rise_m + bank_fall_m beyond the corridor edge -- "sec2.4c THE
    // RAMP BOUND" above already pins bank_profile == 0 there) the amplitude
    // is exactly 0, so the class reverts to Bush AND the reported depth is
    // bit-identical to a W2-off field there -- the cap has nothing to act on.
    // KILL: bank_pack_skin_m's sentinel disabled reads Bush at the crest
    // instead of TrailMain (the class branch dies with the cap, by design).
    world::HeightField hf = make_hf(256, 128, flat_half);
    const Ribbon rb = straight_ribbon(0.10, 0.14, 120, 6.0);
    world::LineNetwork net;
    net.add_path(rb.v.data(), rb.s.data(), rb.v.size(), world::LineKind::RoadMinor, kR);
    net.build_index();
    world::SnowpackField f;
    f.hf = &hf;
    f.lines = &net;

    const double t = 0.12;  // mid-run, far from the endpoint junctions
    const glm::dvec3 crest = north_of(t, 6.0 + f.p.bank_rise_m);  // half_w + rise
    REQUIRE(f.surface_at(crest) == world::Surface::TrailMain);

    const double ring_end = f.p.bank_rise_m + f.p.bank_fall_m;
    const glm::dvec3 past = north_of(t, 6.0 + ring_end + 1.0);  // 1 m past the ring
    REQUIRE(f.surface_at(past) == world::Surface::Bush);

    world::SnowpackField f_off = f;
    f_off.p.bank_pack_skin_m = -1.0;  // the W2.1 sentinel, OFF
    REQUIRE(f.depth_at(past) == f_off.depth_at(past));
    REQUIRE(f.surface_at(past) == f_off.surface_at(past));

    REQUIRE(f_off.surface_at(crest) == world::Surface::Bush);  // the KILL
}

TEST_CASE("snowbank_junction_gap_no_leak") {
    // ★ W2.2 (P2-6): the class branch must read the SAME junction-gap-
    // suppressed amplitude the W2.1 depth cap uses -- a junction gap that
    // kills the physical bank (sec2.4c "the banks are BROKEN -- amplitude
    // dies at a junction", the leg above this one) must also kill the class
    // here, or a sled could read a phantom TrailMain hardpack surface where
    // there is no matching bank amplitude to stand on: a LEAK. KILL: the
    // class branch computed off the UNGAPPED bank_profile (never reading
    // h.junction_m) would still read TrailMain at the run's own endpoint.
    world::HeightField hf = make_hf(256, 128, flat_half);
    const Ribbon rb = straight_ribbon(0.10, 0.14, 120, 6.0);
    world::LineNetwork net;
    net.add_path(rb.v.data(), rb.s.data(), rb.v.size(), world::LineKind::RoadMinor, kR);
    net.build_index();
    world::SnowpackField f;
    f.hf = &hf;
    f.lines = &net;

    // At the run's END (t=0.10, a junction by sec2.4c(a)) the bank amplitude
    // is suppressed to ~0 -- the same fixture point the "banks are BROKEN"
    // leg above uses.
    const glm::dvec3 crest_at_end = north_of(0.10, 6.0 + f.p.bank_rise_m);
    REQUIRE(f.surface_at(crest_at_end) == world::Surface::Bush);

    // ... and the reported depth stays close to a W2-off field there (loose
    // bound, not exact: junction_m is small but not identically 0 this close
    // to -- but not exactly at -- the endpoint station, so the gap term is
    // small but not bit-exact zero, the same tolerance the pre-existing
    // "banks are BROKEN" leg uses).
    world::SnowpackField f_off = f;
    f_off.p.bank_pack_skin_m = -1.0;
    REQUIRE(std::abs(f.depth_at(crest_at_end) - f_off.depth_at(crest_at_end)) <
           0.05);

    // Mid-run (far from a junction) the SAME offset DOES class TrailMain --
    // proving the fixture really can trigger the branch, so the endpoint
    // result above is the gap suppressing it, not a fixture that never
    // reaches the branch at all.
    const glm::dvec3 crest_mid = north_of(0.12, 6.0 + f.p.bank_rise_m);
    REQUIRE(f.surface_at(crest_mid) == world::Surface::TrailMain);
}

// ---------------------------------------------- PHASE W3 (faceted-ground A/B)

TEST_CASE("snowpack_faceted_ground_off_is_bit_identical") {
    // ★ W3: hf_faceted_ground's struct default is false (world/snowpack.h),
    // and facet_radius_fn's struct default is empty/null -- so ANY field that
    // does not opt BOTH in stays bit-identical to the pre-W3 world:
    // drive_radius_at's radius base is exactly hf->radius_at(dir). Three
    // fixtures probe the three ways to be "off": (a) toggle false, fn unset;
    // (b) toggle false, fn SET (a live injection the toggle still gates);
    // (c) toggle true, fn unset (no injection to read). KILL: any of the
    // three reading the facet fn's shifted value instead of hf->radius_at.
    world::HeightField hf = make_hf(256, 128, ridges);
    const Ribbon road = straight_ribbon(0.10, 0.14, 120, 6.0);
    world::LineNetwork road_net;
    road_net.add_path(road.v.data(), road.s.data(), road.v.size(),
                      world::LineKind::RoadMinor, kR);
    road_net.build_index();

    world::SnowpackField f_off;
    f_off.hf = &hf;
    f_off.lines = &road_net;
    REQUIRE(f_off.p.hf_faceted_ground == false);  // the default itself

    world::SnowpackField f_toggle_off_fn_set = f_off;
    f_toggle_off_fn_set.facet_radius_fn = [](glm::dvec3) { return 999999.0; };
    // toggle stays false

    world::SnowpackField f_toggle_on_no_fn = f_off;
    f_toggle_on_no_fn.p.hf_faceted_ground = true;
    // facet_radius_fn stays unset

    for (int i = 0; i < 48; ++i) {
        const double t = 0.10 + 0.04 * i / 47.0;
        for (double m : {0.0, 3.0, 6.0, 9.0, 12.0, 20.0, 60.0, 400.0}) {
            const glm::dvec3 d = north_of(t, m);
            REQUIRE(f_off.sample_at(d).drive_r == f_off.drive_radius_at(d));
            REQUIRE(f_toggle_off_fn_set.drive_radius_at(d) ==
                   f_off.drive_radius_at(d));
            REQUIRE(f_toggle_off_fn_set.sample_at(d).drive_r ==
                   f_off.drive_radius_at(d));
            REQUIRE(f_toggle_on_no_fn.drive_radius_at(d) ==
                   f_off.drive_radius_at(d));
            REQUIRE(f_toggle_on_no_fn.sample_at(d).drive_r ==
                   f_off.drive_radius_at(d));
        }
    }
}

TEST_CASE("snowpack_faceted_ground_swaps_only_the_base") {
    // ★ W3: with a synthetic injected fn returning radius_at(dir) + 0.1, both
    // the toggle AND the fn live, drive_radius_at (and sample_at's drive_r)
    // must shift by EXACTLY +0.1 everywhere -- the depth/lift terms compose
    // unchanged on top, they never read the base themselves. depth_at (the
    // reported, sinkable channel) and depth_geometry_at (the drawn-geometry
    // channel) must be UNCHANGED: the leg's own name is the claim -- the
    // depth channels never read the base at all. KILL: any base-swap that
    // leaks into depth_at/depth_geometry_at, or a shift that is not exactly
    // 0.1 (a scale bug instead of an additive one).
    world::HeightField hf = make_hf(256, 128, ridges);
    const Ribbon road = straight_ribbon(0.10, 0.14, 120, 6.0);
    world::LineNetwork road_net;
    road_net.add_path(road.v.data(), road.s.data(), road.v.size(),
                      world::LineKind::RoadMinor, kR);
    road_net.build_index();
    const Ribbon trail = straight_ribbon(0.30, 0.34, 120, 4.25);
    world::LineNetwork trail_net;
    trail_net.add_path(trail.v.data(), trail.s.data(), trail.v.size(),
                       world::LineKind::Trail, kR);
    trail_net.build_index();

    world::SnowpackField base_r;
    base_r.hf = &hf;
    base_r.lines = &road_net;

    world::SnowpackField faceted_r = base_r;
    faceted_r.p.hf_faceted_ground = true;
    faceted_r.facet_radius_fn = [&hf](glm::dvec3 d) {
        return hf.radius_at(d) + 0.1;
    };

    world::SnowpackField base_t;
    base_t.hf = &hf;
    base_t.lines = &trail_net;

    world::SnowpackField faceted_t = base_t;
    faceted_t.p.hf_faceted_ground = true;
    faceted_t.facet_radius_fn = [&hf](glm::dvec3 d) {
        return hf.radius_at(d) + 0.1;
    };

    for (int i = 0; i < 48; ++i) {
        const double t = 0.10 + 0.04 * i / 47.0;
        for (double m : {0.0, 3.0, 6.0, 9.0, 12.0, 20.0, 60.0, 400.0}) {
            const glm::dvec3 d = north_of(t, m);

            REQUIRE(std::abs((faceted_r.drive_radius_at(d) -
                              base_r.drive_radius_at(d)) - 0.1) < 1e-9);
            REQUIRE(std::abs((faceted_r.sample_at(d).drive_r -
                              base_r.sample_at(d).drive_r) - 0.1) < 1e-9);
            REQUIRE(faceted_r.depth_at(d) == base_r.depth_at(d));
            REQUIRE(faceted_r.depth_geometry_at(d) == base_r.depth_geometry_at(d));
            REQUIRE(faceted_r.sample_at(d).depth_m == base_r.sample_at(d).depth_m);

            REQUIRE(std::abs((faceted_t.drive_radius_at(d) -
                              base_t.drive_radius_at(d)) - 0.1) < 1e-9);
            REQUIRE(faceted_t.depth_at(d) == base_t.depth_at(d));
            REQUIRE(faceted_t.depth_geometry_at(d) == base_t.depth_geometry_at(d));
        }
    }

    // Off the corridor entirely (ambient-only), same claim.
    for (double m : {800.0, 4000.0}) {
        const glm::dvec3 d = north_of(0.50, m);
        REQUIRE(std::abs((faceted_r.drive_radius_at(d) -
                          base_r.drive_radius_at(d)) - 0.1) < 1e-9);
        REQUIRE(faceted_r.depth_at(d) == base_r.depth_at(d));
    }
}

// ================= ROAD-REPAIR: THE C0 CORNER BLEND ======================
//
// The defect (docs/road_repair/census_after_sink.md §5 item 2): LineNetwork::
// nearest is a hard argmin, so `half_w_m` STEPS the instant the winning
// segment flips to the road across the intersection -- 5.5 m major to 3.0 m
// minor is a 2.5 m jump -- and corridor_eval feeds `e = dist - half_w`
// straight into the feather and into bank_profile. A step in the width is a
// step in the DRIVEN SURFACE, which is the jag Chad saw from the street.
//
// These legs build the two shapes that produce it -- a crossing and a shallow
// major/minor merge -- and grade the surface ALONG the road, which is the
// direction the machine travels and therefore the direction a step is felt in.

namespace {

// A ribbon of `n` stations from `t0` to `t1` (turn fractions along the
// equator), offset `north_m` metres north of the equator, running east.
Ribbon offset_ribbon(double t0, double t1, int n, double half_w,
                     double north0_m, double north1_m) {
    Ribbon r;
    const double off = half_w / kR;
    for (int i = 0; i < n; ++i) {
        const double f = n > 1 ? i / (n - 1.0) : 0.0;
        const double t = t0 + (t1 - t0) * f;
        const double north = north0_m + (north1_m - north0_m) * f;
        const glm::dvec3 p = north_of(t, north);
        const glm::dvec3 nrm(0.0, 1.0, 0.0);
        r.v.push_back(glm::normalize(p + nrm * off));
        r.v.push_back(glm::normalize(p - nrm * off));
        const double arc = (t - t0) * 2.0 * kPi * kR;
        r.s.push_back(static_cast<float>(arc));
        r.s.push_back(static_cast<float>(arc));
    }
    return r;
}

// The steepest LONGITUDINAL gradient the corridor law itself puts along a
// plowed road when nothing else changes: the junction gap spending the whole
// bank amplitude on a smoothstep over bank_gap_m, whose peak slope is
// 1.5*h/w by construction (the same algebra §2.4c sizes the section by).
double gap_fade_bound(const world::SnowpackField& f) {
    return 1.5 * f.p.bank_height_m / f.p.bank_gap_m;
}

// Worst |d(drive_r)/ds| along a walk, sampled at `ds` metres.
double worst_grade_along(const world::SnowpackField& f,
                         const std::vector<glm::dvec3>& walk, double ds) {
    double worst = 0.0;
    double prev = f.drive_radius_at(walk[0]);
    for (std::size_t i = 1; i < walk.size(); ++i) {
        const double v = f.drive_radius_at(walk[i]);
        worst = std::max(worst, std::abs(v - prev) / ds);
        prev = v;
    }
    return worst;
}

// ★ ROAD-REPAIR P1 FOLD (red-team 2026-09-09): a HAND-BUILT ARGMIN, so the
// identity leg below has something real to be identical TO. The leg used to
// compare nearest(d, 90.0) against nearest(d, 90.0, 0.0) -- and 0.0 IS the
// default argument, so it compared the function to itself and would have
// passed no matter what the blend did. This walks EVERY segment of the network
// with the pre-commit arithmetic written out longhand (the same clamped
// point-to-chord projection, the same 2R*asin(chord/2) arc, the same
// float-then-double width lerp), keeps the single nearest, and reports the
// gap to the runner-up so the caller can skip the samples where an exact tie
// makes "which segment won" genuinely ambiguous.
world::LineHit brute_nearest(const world::LineNetwork& net, glm::dvec3 q,
                             double search_m, double& runner_up_gap_m) {
    world::LineHit h;
    h.dist_m = world::kBigDistance;
    h.junction_m = world::kBigDistance;
    double best = 1e300, second = 1e300;
    for (std::size_t i = 0; i + 1 < net.st.size(); ++i) {
        if (net.st[i + 1].run != net.st[i].run) continue;
        const world::LineKind ki =
            static_cast<world::LineKind>(net.st[i].kind);
        if (!world::is_corridor(ki)) continue;
        const glm::dvec3 a = net.st[i].dir, b = net.st[i + 1].dir;
        const glm::dvec3 ab = b - a;
        const double den = glm::dot(ab, ab);
        double t = den > 1e-30 ? glm::dot(q - a, ab) / den : 0.0;
        t = t < 0.0 ? 0.0 : (t > 1.0 ? 1.0 : t);
        const glm::dvec3 dv = q - (a + ab * t);
        const double chord = std::sqrt(glm::dot(dv, dv));
        if (chord < best) {
            second = best;
            best = chord;
            h.found = true;
            h.kind = ki;
            h.dist_m = 2.0 * net.R * std::asin(std::min(1.0, 0.5 * chord));
            h.half_w_m = net.st[i].half_w_m +
                         (net.st[i + 1].half_w_m - net.st[i].half_w_m) * t;
            h.foot = glm::normalize(net.st[i].dir +
                                    (net.st[i + 1].dir - net.st[i].dir) * t);
            h.seg_a = static_cast<std::int64_t>(i);
        } else if (chord < second) {
            second = chord;
        }
    }
    runner_up_gap_m = (second >= 1e299) ? 1e9 : (second - best) * net.R;
    if (h.found && h.dist_m > search_m) {
        h = world::LineHit{};
        h.dist_m = world::kBigDistance;
        h.junction_m = world::kBigDistance;
        return h;
    }
    h.plowed_w = world::is_plowed(h.kind) ? 1.0 : 0.0;
    h.junction_m = net.junction_dist(h.foot, search_m);
    return h;
}

}  // namespace

TEST_CASE("ROAD-REPAIR: corner_blend_m = 0 is the shipped query, BIT-for-BIT") {
    // The identity value is not "close enough": it is the same branch the
    // pre-repair build took. Every golden, every headless fixture and every
    // off-corridor test runs at 0.0, so this leg is what says they cannot
    // have moved.
    const Ribbon ew = straight_ribbon(0.10, 0.14, 120, 5.5);
    world::LineNetwork net;
    net.add_path(ew.v.data(), ew.s.data(), ew.v.size(),
                 world::LineKind::RoadMajor, kR);
    const Ribbon ns = offset_ribbon(0.1195, 0.1205, 40, 3.0, -300.0, 300.0);
    net.add_path(ns.v.data(), ns.s.data(), ns.v.size(),
                 world::LineKind::RoadMinor, kR);
    net.build_index();

    // ★ AGAINST A HAND-BUILT ARGMIN, not against itself. `blend_m` defaults
    // to 0.0, so the old form of this loop (nearest(d,90) vs nearest(d,90,0.0))
    // was the same call twice and could not fail. brute_nearest walks every
    // segment with the pre-commit arithmetic written out longhand; the bucket
    // grid, the candidate buffer and the whole blend block have to be exactly
    // invisible for this to pass.
    int checked = 0, tie_skipped = 0;
    for (int i = 0; i < 400; ++i) {
        const double t = 0.100 + 0.040 * i / 399.0;
        const glm::dvec3 d = north_of(t, -40.0 + 0.2 * i);
        const world::LineHit a = net.nearest(d, 90.0, 0.0);
        double gap = 0.0;
        const world::LineHit ref = brute_nearest(net, d, 90.0, gap);
        REQUIRE(a.found == ref.found);
        // `dist_m` is a min over the identical set of identically computed
        // doubles, so it is exact whoever won, tie or no tie.
        REQUIRE(a.dist_m == ref.dist_m);
        if (gap < 1e-9) {
            // An exact tie: WHICH of two equidistant segments the argmin names
            // is unspecified and always was. Named, not silently tolerated.
            ++tie_skipped;
            continue;
        }
        ++checked;
        REQUIRE(a.kind == ref.kind);
        REQUIRE(a.half_w_m == ref.half_w_m);
        REQUIRE(a.junction_m == ref.junction_m);
        REQUIRE(a.seg_a == ref.seg_a);
        REQUIRE(a.foot == ref.foot);
        // ★ P1 FOLD: the plowed WEIGHT is the argmin's own is_plowed off a
        // tie. corridor_eval branches on exactly 1.0 / 0.0, so this is what
        // keeps every kind-gated term bit-for-bit what it was.
        REQUIRE(a.plowed_w == ref.plowed_w);
        REQUIRE((a.plowed_w == 1.0 || a.plowed_w == 0.0));
    }
    INFO("hand-built argmin: " << checked << " checked, " << tie_skipped
                               << " exact ties skipped");
    REQUIRE(checked > 300);

    // The default argument IS the identity value -- worth one assertion, but
    // it is a statement about the SIGNATURE, not about the blend.
    {
        const glm::dvec3 d = north_of(0.115, 7.0);
        REQUIRE(net.nearest(d, 90.0).half_w_m == net.nearest(d, 90.0, 0.0).half_w_m);
    }

    // ... and OUTSIDE the band the blend is the same bits too: the winner is
    // still uniquely the nearest, so a sample standing well clear of any tie
    // gets the shipped segment's own numbers and no averaging at all. 400 m
    // down the major, the minor is 400 m away -- 200x the band.
    for (int i = 0; i < 200; ++i) {
        const double t = 0.100 + 0.008 * i / 199.0;  // well west of the cross
        const glm::dvec3 d = north_of(t, 4.0 + 0.05 * i);
        const world::LineHit a = net.nearest(d, 90.0, 0.0);
        const world::LineHit b = net.nearest(d, 90.0, 2.0);
        REQUIRE(a.half_w_m == b.half_w_m);
        REQUIRE(a.foot == b.foot);
        REQUIRE(a.junction_m == b.junction_m);
        REQUIRE(a.dist_m == b.dist_m);
    }
}

TEST_CASE("ROAD-REPAIR: a baked INTERSECTION stops stepping the driven road") {
    world::HeightField hf = make_hf(256, 128, flat_half);
    // A 5.5 m half-width major running east, crossed by a 3.0 m half-width
    // minor -- the width pair the census names (major 5.5 / minor 3.0).
    const Ribbon ew = straight_ribbon(0.10, 0.14, 120, 5.5);
    world::LineNetwork net;
    net.add_path(ew.v.data(), ew.s.data(), ew.v.size(),
                 world::LineKind::RoadMajor, kR);
    {
        std::vector<glm::dvec3> v;
        std::vector<float> s;
        for (int i = 0; i < 40; ++i) {
            const double m = -300.0 + 600.0 * i / 39.0;
            const glm::dvec3 p = north_of(0.12, m);
            const glm::dvec3 side =
                glm::normalize(glm::cross(glm::dvec3(0, 1, 0), p));
            v.push_back(glm::normalize(p + side * (3.0 / kR)));
            v.push_back(glm::normalize(p - side * (3.0 / kR)));
            s.push_back(static_cast<float>(m + 300.0));
            s.push_back(static_cast<float>(m + 300.0));
        }
        net.add_path(v.data(), s.data(), v.size(), world::LineKind::RoadMinor,
                     kR);
    }
    net.build_index();

    world::SnowpackField f;
    f.hf = &hf;
    f.lines = &net;

    // The walk: ALONG the major, out at the shoulder (9 m north, past the
    // 5.5 m roadway edge and into the section), through the crossing. The
    // argmin flips to the minor wherever the minor centreline is nearer than
    // 9 m -- i.e. over a 18 m stretch centred on the intersection.
    const double ds = 0.05;
    const double span = 60.0;  // metres either side of the crossing
    std::vector<glm::dvec3> walk;
    for (double x = -span; x <= span; x += ds) {
        const double t = 0.12 + x / (2.0 * kPi * kR);
        walk.push_back(north_of(t, 9.0));
    }

    world::reset_line_blend_overflows();
    f.p.corner_blend_m = 0.0;
    const double off = worst_grade_along(f, walk, ds);
    f.p.corner_blend_m = 2.0;
    const double on = worst_grade_along(f, walk, ds);

    // THE THRESHOLD is not invented here: §2.4c already rules one, and it is
    // the only gradient bound the snowpack law has -- bank_max_grade, "a RAMP
    // that launches, never a step that stops the machine dead". The blended
    // road must live under the same bound the bank SECTION lives under. The
    // unblended one is not merely over it, it is over it by more than 4x,
    // which is what a C0 step looks like when you sample it at 5 cm.
    INFO("worst d(drive_r)/ds along the road: OFF " << off << "  ON " << on
         << "  bank_max_grade " << f.p.bank_max_grade);
    REQUIRE(off > 4.0 * f.p.bank_max_grade);
    REQUIRE(on <= f.p.bank_max_grade);
    // The blend never silently dropped an in-band competitor.
    REQUIRE(world::line_blend_overflows() == 0);

    // The JUNCTION GAP LAW IS PRESERVED: the crossing still opens the bank.
    // (Blending a width could otherwise have re-grown one at the very place
    // §2.4c orders it broken.)
    const double ambient = f.ambient_depth_at(north_of(0.12, 400.0));
    auto geom = [&](glm::dvec3 d) { return f.drive_radius_at(d) - hf.radius_at(d); };
    REQUIRE(geom(north_of(0.12, 9.0)) < ambient + 0.05);
    // ... and 60 m down the major, well clear of the gap, it is still a bank.
    const double t_far = 0.12 + 60.0 / (2.0 * kPi * kR);
    REQUIRE(geom(north_of(t_far, 9.0)) > ambient + 0.5 * f.p.bank_height_m);
}

TEST_CASE("ROAD-REPAIR: a major/minor MERGE stops stepping the bank") {
    // The intersection above is the easy half: the junction gap has already
    // faded the bank there, so only the corridor FEATHER steps. The hard half
    // is a shallow merge -- a minor leaving a major at ~3 deg -- where the
    // bisector between the two corridors is crossed HUNDREDS of metres from
    // the junction, with the bank at full amplitude on both sides of it.
    world::HeightField hf = make_hf(256, 128, flat_half);
    const Ribbon ew = straight_ribbon(0.10, 0.14, 120, 5.5);
    world::LineNetwork net;
    net.add_path(ew.v.data(), ew.s.data(), ew.v.size(),
                 world::LineKind::RoadMajor, kR);
    // The minor: leaves the major at t = 0.12 and climbs 0.052 m north per
    // metre east (~3 deg) for 600 m.
    const double t_start = 0.12;
    const double t_end = t_start + 600.0 / (2.0 * kPi * kR);
    const Ribbon mn = offset_ribbon(t_start, t_end, 30, 3.0, 0.0, 31.2);
    net.add_path(mn.v.data(), mn.s.data(), mn.v.size(),
                 world::LineKind::RoadMinor, kR);
    net.build_index();

    world::SnowpackField f;
    f.hf = &hf;
    f.lines = &net;

    // Walk east along the major at 6 m north -- inside the wedge between the
    // two roads. The minor is nearer than the major once it has climbed past
    // 12 m, i.e. ~230 m east of the junction: 16x bank_gap_m away, so this is
    // a width flip with NO junction fade to hide behind.
    const double ds = 0.05;
    std::vector<glm::dvec3> walk;
    for (double x = 120.0; x <= 360.0; x += ds) {
        const double t = t_start + x / (2.0 * kPi * kR);
        walk.push_back(north_of(t, 6.0));
    }

    world::reset_line_blend_overflows();
    f.p.corner_blend_m = 0.0;
    const double off = worst_grade_along(f, walk, ds);
    f.p.corner_blend_m = 2.0;
    const double on = worst_grade_along(f, walk, ds);
    INFO("merge: worst d(drive_r)/ds OFF " << off << "  ON " << on);
    REQUIRE(off > 4.0 * f.p.bank_max_grade);
    REQUIRE(on <= f.p.bank_max_grade);
    REQUIRE(world::line_blend_overflows() == 0);

    // The blend is a SMOOTHING, not a widening: at the two ends of the walk,
    // far from the bisector, the queried half-width is still each road's own
    // number to the bit.
    f.p.corner_blend_m = 2.0;
    const world::LineHit west = net.nearest(walk.front(), 90.0, 2.0);
    const world::LineHit east = net.nearest(walk.back(), 90.0, 2.0);
    // West of the bisector (x = 230 m, where the climbing minor is exactly as
    // far as the major) the MINOR is the nearer road; east of it the major is.
    REQUIRE(std::abs(west.half_w_m - 3.0) < 1e-6);
    REQUIRE(std::abs(east.half_w_m - 5.5) < 1e-6);

    // And the gap fade bound is what the blended walk is actually living
    // under -- reported so the number in the doc can be read off the test.
    INFO("gap fade bound " << gap_fade_bound(f));
    REQUIRE(gap_fade_bound(f) > 0.0);
}

TEST_CASE("ROAD-REPAIR: a road/TRAIL merge stops stepping the KIND-GATED terms") {
    // ★ P1 FOLD (red-team 2026-09-09). The two legs above are same-KIND
    // pairs, so only half_w/foot/junction_m ever stepped and the corner blend
    // caught all of it. This is the leg that was missing: a PLOWED road and a
    // GROOMED trail competing, where `kind` itself flips. Three corridor terms
    // are gated on that bool -- the deck-lift EXTENT (0.45 m held to 9 m on a
    // road, zero past 4 m on a trail), the inside depth (0.02 vs 0.12 m) and
    // the bank amplitude (roads only) -- and the shipped answer to that was
    // "the junction gap has already faded the bank there". It has not:
    //
    //   * the deck-lift extent is not multiplied by the gap fade AT ALL, so
    //     the 0.45 m step is there junction or no junction; and
    //   * this geometry is the MERGE geometry above -- a ~3 deg limb, whose
    //     bisector is crossed ~230 m from the junction node, 16x bank_gap_m,
    //     with the bank at FULL amplitude.
    //
    // Same walk, same widths, same 5 cm sampling as the major/minor merge; the
    // ONLY change is that the merging limb is a TrailMain instead of a
    // RoadMinor. If plowed_w did not exist this walk would step by the whole
    // deck lift.
    world::HeightField hf = make_hf(256, 128, flat_half);
    const Ribbon ew = straight_ribbon(0.10, 0.14, 120, 5.5);
    world::LineNetwork net;
    net.add_path(ew.v.data(), ew.s.data(), ew.v.size(),
                 world::LineKind::RoadMinor, kR);
    const double t_start = 0.12;
    const double t_end = t_start + 600.0 / (2.0 * kPi * kR);
    const Ribbon mn = offset_ribbon(t_start, t_end, 30, 3.0, 0.0, 31.2);
    net.add_path(mn.v.data(), mn.s.data(), mn.v.size(),
                 world::LineKind::TrailMain, kR);
    net.build_index();

    world::SnowpackField f;
    f.hf = &hf;
    f.lines = &net;

    const double ds = 0.05;
    std::vector<glm::dvec3> walk;
    for (double x = 120.0; x <= 360.0; x += ds) {
        const double t = t_start + x / (2.0 * kPi * kR);
        walk.push_back(north_of(t, 6.0));
    }

    world::reset_line_blend_overflows();
    f.p.corner_blend_m = 0.0;
    const double off = worst_grade_along(f, walk, ds);
    f.p.corner_blend_m = 2.0;
    const double on = worst_grade_along(f, walk, ds);
    INFO("road/trail merge: worst d(drive_r)/ds OFF " << off << "  ON " << on
         << "  bank_max_grade " << f.p.bank_max_grade);
    // THE BOUND IS §2.4c's OWN: bank_max_grade, "a RAMP that launches, never
    // a step that stops the machine dead" -- not a number picked to pass.
    REQUIRE(off > f.p.bank_max_grade);
    REQUIRE(on <= f.p.bank_max_grade);
    REQUIRE(world::line_blend_overflows() == 0);

    // The weight itself: saturated at both ends of the walk, a genuine
    // fraction somewhere in the middle. Without the middle sample this leg
    // could pass on a query that never blended at all.
    const world::LineHit west = net.nearest(walk.front(), 90.0, 2.0);
    const world::LineHit east = net.nearest(walk.back(), 90.0, 2.0);
    REQUIRE(west.plowed_w == 0.0);   // the TRAIL limb is nearer here
    REQUIRE(east.plowed_w == 1.0);   // ... and the ROAD out east
    bool saw_fraction = false;
    for (const glm::dvec3& d : walk) {
        const double w = net.nearest(d, 90.0, 2.0).plowed_w;
        if (w > 1e-6 && w < 1.0 - 1e-6) {
            saw_fraction = true;
            break;
        }
    }
    REQUIRE(saw_fraction);

    // ... and with the dial at 0 the weight is a HARD bool everywhere, which
    // is what makes every fixture, golden and off-corridor test unmoved.
    for (const glm::dvec3& d : walk) {
        const double w = net.nearest(d, 90.0, 0.0).plowed_w;
        REQUIRE((w == 0.0 || w == 1.0));
    }
}
