// SF1 -- THE ST. CHARLES SNOW MOUNTAIN, as executable law
// (docs/snowhill_sf1_spec.md sec5, WINTER_LAW sec3.6b).
//
// Legs 1-4 and 9 run on a FLAT synthetic HeightField with a SYNTHETIC anchor
// frame (up/east/north as orthonormal world axes, matching the sled_probe
// culture from test_snowpack.cpp: "a measurement that moves when the world is
// re-baked is not a measurement"). Leg 5 parses the shipped hero glb.
//
//   leg 1  snowhill_shape_is_chads_mountain      -- sec5.1 shape numbers
//   leg 2  snowhill_is_bit_localized              -- sec5.2 INV: `==` outside
//                                                     r_cut, protects the
//                                                     signed 0.77 field
//   leg 3  snowhill_survives_the_plow             -- sec5.3 plow-composition
//   leg 4  snowhill_crest_is_trailmain            -- sec5.4 surface class
//   leg 5  snowhill_mesh_conforms_to_the_function -- sec5.5 conformance
//   leg 9  snowhill_pile_is_compacted_not_mush    -- sec5.9 compacted-pile
//                                                     skin (red-team F6)
//
// Legs 6-8 (drive probe / flight goldens / gate housekeeping) are NOT in this
// file -- sec5 assigns them to tools/sled_probe.cpp and the build harness.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <glm/geometric.hpp>
#include <glm/vec3.hpp>
#include <string>
#include <vector>

#include "config/load_world.h"
#include "world/heightfield.h"
#include "world/linework.h"
#include "world/raster.h"
#include "world/props.h"  // SF1 canopy-mask leg
#include "world/snowhill.h"
#include "world/snowpack.h"

// rig-D D.1b precedent (test_asset_validator.cpp): cgltf is header-only/C, and
// that file already carries the ONE CGLTF_IMPLEMENTATION for the whole
// seads_tests binary -- this TU only needs the declarations, never a second
// implementation copy.
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include "cgltf.h"
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#ifdef NDEBUG
#error "SEADS gate requires an assert-live build (SPEC 6.1)"
#endif

namespace {

constexpr double kR = 15000.0;     // config/aircraft.toml world.R
constexpr double kRelief = 350.0;  // config/world.toml ground.relief_scale_m
constexpr double kDegToRad = 3.14159265358979323846 / 180.0;

// ---- the flat fixture (test_snowpack.cpp's make_hf, byte-for-byte) --------

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

world::Raster8 uniform_raster(int w, int h, double value) {
    world::Raster8 r;
    r.w = w;
    r.h = h;
    r.px.assign(static_cast<std::size_t>(w) * h,
               static_cast<std::uint8_t>(std::clamp(value, 0.0, 1.0) * 255.0 + 0.5));
    return r;
}

// ---- the SF1 synthetic anchor + shipped peaks (config/world.toml [snowhill],
// docs/snowhill_sf1_spec.md sec2) --------------------------------------------

world::SnowhillParams make_hill_params() {
    world::SnowhillParams p;
    p.enabled = true;
    p.up = glm::dvec3(0.0, 0.0, 1.0);
    p.east = glm::dvec3(1.0, 0.0, 0.0);
    p.north = glm::dvec3(0.0, 1.0, 0.0);
    p.r_cut_m = 60.0;
    p.feather_m = 15.0;
    p.class_min_m = 0.30;
    p.pack_cap_m = 0.30;
    p.peaks[0] = {0.0, 0.0, 4.6, 4.3, 7.5, 90.0};     // main crest (steep N-S)
    p.peaks[1] = {0.0, 7.0, 1.9, 6.0, 10.0, 0.0};     // overlapped N shoulder
    p.peaks[2] = {-11.0, -4.0, 2.4, 4.5, 5.0, 0.0};   // the kid-built kicker (SW)
    return p;
}

// Local tangent metres (x=east, y=north) -> unit dir, the SAME gnomonic
// mapping tools/snowhill_mesh.cpp and world/snowhill.cpp use (and each other's
// inverse), so the frame is injectable and cancels out of any comparison built
// from it (spec sec4's "the frame cancels" note).
glm::dvec3 local_dir(const world::HeightField& hf, const world::SnowhillParams& p,
                     double x_east, double y_north) {
    return glm::normalize(p.up + (x_east / hf.R) * p.east + (y_north / hf.R) * p.north);
}

double local_h(const world::HeightField& hf, const world::SnowhillParams& p,
              double x_east, double y_north) {
    return world::snowhill_add(&hf, p, local_dir(hf, p, x_east, y_north));
}

// One (dx, dy) ring point at radius r, bearing measured clockwise from local
// north (0 deg = north, 90 deg = east) -- the spec sec5.1 convention.
void bearing_xy(double r, double bearing_deg, double& x, double& y) {
    const double b = bearing_deg * kDegToRad;
    x = r * std::sin(b);
    y = r * std::cos(b);
}

struct Crest {
    double x = 0.0, y = 0.0, h = -1.0;
};

// Global max of snowhill_add over +/-62 m at 0.5 m spacing -- ASKED FOR, never
// assumed (the test_snowpack "sec2.2" leg lesson: assuming a phase/location
// reads as a law failure instead of a test bug).
Crest find_crest(const world::HeightField& hf, const world::SnowhillParams& p) {
    Crest c;
    for (double x = -62.0; x <= 62.0 + 1e-9; x += 0.5)
        for (double y = -62.0; y <= 62.0 + 1e-9; y += 0.5) {
            const double h = local_h(hf, p, x, y);
            if (h > c.h) {
                c.h = h;
                c.x = x;
                c.y = y;
            }
        }
    return c;
}

// One (L, R) ribbon running north-south at local x_east = x0, y_north in
// [y0, y1] -- the same L/R-pair-recovers-centerline convention
// test_snowpack.cpp's straight_ribbon uses, translated into SF1's local
// tangent-metre coordinates.
struct Ribbon {
    std::vector<glm::dvec3> v;
    std::vector<float> s;
};
Ribbon straight_ribbon_local(const world::HeightField& hf, const world::SnowhillParams& p,
                             double y0, double y1, int stations, double x0,
                             double half_w) {
    Ribbon r;
    for (int i = 0; i < stations; ++i) {
        const double y = y0 + (y1 - y0) * i / (stations - 1.0);
        r.v.push_back(local_dir(hf, p, x0 + half_w, y));
        r.v.push_back(local_dir(hf, p, x0 - half_w, y));
        const float arc = static_cast<float>(y - y0);
        r.s.push_back(arc);
        r.s.push_back(arc);
    }
    return r;
}

}  // namespace

// ------------------------------------------------------------------ leg 1 --

TEST_CASE("snowhill_shape_is_chads_mountain") {
    // sec5.1: max height, max grade, and the two named bearing slopes,
    // measured off a dense grid rather than assumed (tune_hill.py's numbers:
    // max 6.13 m, grad 0.880, N 0.573, 150 deg 0.869).
    world::HeightField hf = make_hf(256, 128, flat_half);
    world::SnowhillParams p = make_hill_params();

    const double lo = -62.0, hi = 62.0, step = 0.5;
    const int n = static_cast<int>(std::lround((hi - lo) / step)) + 1;
    std::vector<std::vector<double>> H(static_cast<std::size_t>(n),
                                       std::vector<double>(static_cast<std::size_t>(n)));
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
            H[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] =
                local_h(hf, p, lo + i * step, lo + j * step);

    double h_max = -1e18, x_max = 0.0, y_max = 0.0, grad_max = 0.0;
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j) {
            const double h = H[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)];
            if (h > h_max) {
                h_max = h;
                x_max = lo + i * step;
                y_max = lo + j * step;
            }
            if (i > 0 && i < n - 1 && j > 0 && j < n - 1) {
                const double gx = (H[static_cast<std::size_t>(i + 1)][static_cast<std::size_t>(j)] -
                                   H[static_cast<std::size_t>(i - 1)][static_cast<std::size_t>(j)]) /
                                  (2.0 * step);
                const double gy = (H[static_cast<std::size_t>(i)][static_cast<std::size_t>(j + 1)] -
                                   H[static_cast<std::size_t>(i)][static_cast<std::size_t>(j - 1)]) /
                                  (2.0 * step);
                grad_max = std::max(grad_max, std::sqrt(gx * gx + gy * gy));
            }
        }

    REQUIRE(h_max >= 5.6);
    REQUIRE(h_max <= 6.4);
    REQUIRE(grad_max <= 1.0);

    // Descent slope walking OUT from the found summit along a fixed bearing --
    // the max of the negated radial derivative, sampled at 0.1 m so a narrow
    // sigma (3.6-4.0 m) peak grade is not stepped over.
    auto max_descent = [&](double bearing_deg) {
        double x0 = 0.0, y0 = 0.0;
        bearing_xy(1.0, bearing_deg, x0, y0);
        double prev = local_h(hf, p, x_max, y_max);
        double worst = 0.0;
        const double dr = 0.1;
        for (double r = dr; r <= 80.0; r += dr) {
            const double h = local_h(hf, p, x_max + r * x0, y_max + r * y0);
            worst = std::max(worst, (prev - h) / dr);
            prev = h;
        }
        return worst;
    };

    const double north_slope = max_descent(0.0);
    const double sse_slope = max_descent(150.0);
    REQUIRE(north_slope >= 0.45);
    REQUIRE(north_slope <= 0.65);
    REQUIRE(sse_slope >= 0.70);
}

// ------------------------------------------------------------------ leg 2 --

TEST_CASE("snowhill_is_bit_localized") {
    // sec5.2, the INV that protects the signed 0.77 field: with the hill
    // enabled vs disabled, depth_at/drive_radius_at/surface_at are BIT-EXACT
    // at every sampled point at or beyond r_cut -- including on a synthetic
    // corridor and under a landmask fraction (fields that touch depth_base_at
    // through paths the hill must never perturb).
    world::HeightField hf = make_hf(256, 128, flat_half);
    world::SnowhillParams hillp = make_hill_params();

    // A TrailMain corridor running north-south right at x_east = r_cut, so one
    // sampled point sits both ON the corridor and exactly at the boundary.
    const Ribbon rb = straight_ribbon_local(hf, hillp, -300.0, 300.0, 200, 61.0, 4.25);
    world::LineNetwork net;
    net.add_path(rb.v.data(), rb.s.data(), rb.v.size(), world::LineKind::Trail, kR);
    net.build_index();

    // A landmask fraction present everywhere, BELOW water_class_frac (0.5) so
    // surface_at does not collapse every sample to LakeIce before the hill
    // check ever runs -- the comparison below still has to walk through the
    // hill/corridor/bush branches, not just the water short-circuit. (The
    // samples asserted are all >= r_cut, which is the only place the
    // localization law applies.)
    const world::Raster8 ice = uniform_raster(64, 32, 0.3);

    world::SnowpackField on, off;
    on.hf = &hf;
    off.hf = &hf;
    on.lines = &net;
    off.lines = &net;
    on.landmask = &ice;
    off.landmask = &ice;
    on.hill = hillp;
    on.hill.enabled = true;
    off.hill = hillp;
    off.hill.enabled = false;

    std::vector<glm::dvec3> pts;
    for (double r : {61.0, 75.0, 120.0, 500.0})
        for (double bearing = 0.0; bearing < 360.0; bearing += 45.0) {
            double x = 0.0, y = 0.0;
            bearing_xy(r, bearing, x, y);
            pts.push_back(local_dir(hf, hillp, x, y));
        }
    pts.push_back(local_dir(hf, hillp, 61.0, 0.0));  // on the corridor, at r_cut

    for (const glm::dvec3& d : pts) {
        REQUIRE(on.depth_at(d) == off.depth_at(d));
        REQUIRE(on.drive_radius_at(d) == off.drive_radius_at(d));
        REQUIRE(on.surface_at(d) == off.surface_at(d));
    }
}

// ------------------------------------------------------------------ leg 3 --

TEST_CASE("snowhill_survives_the_plow") {
    // sec5.3: a TrailMain corridor straight through the anchor. The pile's
    // GEOMETRY (drive_radius_at) cannot be deleted or capped by the plow --
    // snowpack.cpp:198-204 sums the FULL hill onto drive_radius_at after
    // depth_base_at, whatever the corridor did to depth_base_at. This is
    // asserted as the exact arithmetic identity the composition promises,
    // AT the crest, UNDER the corridor -- the one point a regression would
    // zero it.
    //
    // ★ SPEC AMBIGUITY, NAMED, NOT SILENTLY RESOLVED: sec5.3's literal text
    // also reads `depth_at(crest) >= snowhill_add(crest)`. That cannot hold
    // simultaneously with sec5.9's compacted-pile skin (hill_reported_depth,
    // snowpack.cpp:183-190): for any hill_m > class_min_m (0.30 m) the
    // REPORTED depth is smoothstep-blended down to pack_cap_m (0.30 m), while
    // snowhill_add at the crest is ~6.13 m -- so depth_at(crest) is ~0.30,
    // provably less than snowhill_add(crest), by construction, on the
    // committed code (and leg 9 below pins that same fact as the LAW). The
    // "plowing cannot delete the mountain" claim is real and is asserted here
    // -- on drive_radius_at, which is what sec3's composition prose actually
    // describes ("plowing ... can never zero or cap it" said of
    // drive_radius_at, not of the reported/sinkable depth_at). Flagging this
    // rather than writing a REQUIRE that a correct build must fail.
    world::HeightField hf = make_hf(256, 128, flat_half);
    world::SnowhillParams hillp = make_hill_params();

    const Ribbon rb = straight_ribbon_local(hf, hillp, -300.0, 300.0, 400, 0.0, 4.25);
    world::LineNetwork net;
    net.add_path(rb.v.data(), rb.s.data(), rb.v.size(), world::LineKind::Trail, kR);
    net.build_index();

    world::SnowpackField f;
    f.hf = &hf;
    f.lines = &net;
    f.hill = hillp;

    const Crest crest = find_crest(hf, hillp);
    const glm::dvec3 cdir = local_dir(hf, hillp, crest.x, crest.y);
    const double hill_at_crest = world::snowhill_add(&hf, hillp, cdir);
    REQUIRE(hill_at_crest > 5.6);  // sanity: this really is the summit

    REQUIRE(f.drive_radius_at(cdir) - hf.radius_at(cdir) >= hill_at_crest);
    REQUIRE(f.drive_radius_at(cdir) > hf.R + 5.6);

    // 100+ m beyond the footprint (r_cut = 60 m), still on the corridor
    // centerline: the hill contributes exactly 0, so depth_at reads the plain
    // groomed hard pack.
    const glm::dvec3 far = local_dir(hf, hillp, 0.0, 200.0);
    REQUIRE(std::abs(f.depth_at(far) - f.p.trail_pack_m) < 1e-9);
}

// ------------------------------------------------------------------ leg 4 --

TEST_CASE("snowhill_crest_is_trailmain") {
    // sec5.4: the crest reads TrailMain even with no corridor at all (the
    // hill check runs before the corridor branch), and outside r_cut the
    // class is bit-unchanged vs the hill disabled.
    world::HeightField hf = make_hf(256, 128, flat_half);
    world::SnowhillParams hillp = make_hill_params();

    world::SnowpackField f;
    f.hf = &hf;
    f.hill = hillp;  // f.lines stays null -- no corridor anywhere

    const Crest crest = find_crest(hf, hillp);
    const glm::dvec3 cdir = local_dir(hf, hillp, crest.x, crest.y);
    REQUIRE(f.surface_at(cdir) == world::Surface::TrailMain);

    world::SnowpackField on = f;
    world::SnowpackField off = f;
    off.hill.enabled = false;
    for (double r : {65.0, 100.0, 300.0})
        for (double bearing = 0.0; bearing < 360.0; bearing += 60.0) {
            double x = 0.0, y = 0.0;
            bearing_xy(r, bearing, x, y);
            const glm::dvec3 d = local_dir(hf, hillp, x, y);
            REQUIRE(on.surface_at(d) == off.surface_at(d));
        }
}

// ------------------------------------------------------------------ leg 9 --

TEST_CASE("snowhill_pile_is_compacted_not_mush") {
    // sec5.9, red-team F6: at the crest the REPORTED depth is capped toward
    // pack_cap_m (the sinkable skin) while drive_r keeps the full pile
    // geometry -- both asserted from the same GroundSample. Then a continuity
    // walk across the footprint ring: no step in the reported depth anywhere
    // the class flips (§2.4c's ramp-bound reasoning, applied to the hill's own
    // smoothstep transition).
    world::HeightField hf = make_hf(256, 128, flat_half);
    world::SnowhillParams hillp = make_hill_params();
    world::SnowpackField f;
    f.hf = &hf;
    f.hill = hillp;  // no corridor: depth_base_at is bare ambient here

    const Crest crest = find_crest(hf, hillp);
    const glm::dvec3 cdir = local_dir(hf, hillp, crest.x, crest.y);
    const world::SnowpackField::GroundSample g = f.sample_at(cdir);

    REQUIRE(g.depth_m <= hillp.pack_cap_m + 1e-9);
    REQUIRE(f.depth_base_at(cdir) == f.ambient_depth_at(cdir));
    const double expected_r = hf.radius_at(cdir) + f.depth_base_at(cdir) +
                              world::snowhill_add(&hf, hillp, cdir);
    REQUIRE(g.drive_r == expected_r);

    // Outside r_cut the reported depth is bit-unchanged (localization again,
    // this time on depth_at alone, walking straight through the ring).
    world::SnowpackField off = f;
    off.hill.enabled = false;
    const glm::dvec3 beyond = local_dir(hf, hillp, 0.0, 200.0);
    REQUIRE(f.depth_at(beyond) == off.depth_at(beyond));

    double prev = f.depth_at(local_dir(hf, hillp, 0.0, 25.0));
    for (double r = 25.5; r <= 61.0 + 1e-9; r += 0.5) {
        const double d = f.depth_at(local_dir(hf, hillp, 0.0, r));
        REQUIRE(std::abs(d - prev) <= 0.25);
        prev = d;
    }
}

// ------------------------------------------------------------------ leg 5 --

namespace {

struct GlbCore {
    bool ok = false;
    std::vector<glm::vec3> pos;  // raw glb-local positions (glb axes)
    double pad = 0.0;            // parsed from asset.generator's "pad %.3f m"
};

GlbCore load_snowhill_glb(const std::string& path) {
    GlbCore m;
    cgltf_options opt{};
    cgltf_data* d = nullptr;
    if (cgltf_parse_file(&opt, path.c_str(), &d) != cgltf_result_success) return m;
    if (cgltf_load_buffers(&opt, d, path.c_str()) != cgltf_result_success) {
        cgltf_free(d);
        return m;
    }
    if (d->asset.generator != nullptr) {
        const std::string gen = d->asset.generator;
        const std::size_t at = gen.find("pad ");
        if (at != std::string::npos) m.pad = std::atof(gen.c_str() + at + 4);
    }
    if (d->meshes_count >= 1 && d->meshes[0].primitives_count >= 1) {
        const cgltf_primitive& prim = d->meshes[0].primitives[0];
        const cgltf_accessor* posA = nullptr;
        for (cgltf_size a = 0; a < prim.attributes_count; ++a)
            if (prim.attributes[a].type == cgltf_attribute_type_position)
                posA = prim.attributes[a].data;
        if (posA != nullptr) {
            m.pos.reserve(posA->count);
            for (cgltf_size i = 0; i < posA->count; ++i) {
                float v[3];
                cgltf_accessor_read_float(posA, i, v, 3);
                m.pos.emplace_back(v[0], v[1], v[2]);
            }
            m.ok = true;
        }
    }
    cgltf_free(d);
    return m;
}

}  // namespace

TEST_CASE("snowhill_mesh_conforms_to_the_function") {
    // sec5.5: the mesh is GENERATED FROM snowhill_add (tools/snowhill_mesh.cpp)
    // so conformance is by construction -- this leg is the regression bound
    // that catches a stale glb after a config retune, or a generator rot in
    // the axis map / pad.
    const std::string path = std::string(SEADS_ASSET_DIR) + "/heroes/snowhill_stcharles.glb";
    if (!std::filesystem::exists(path)) {
        // Absent (not yet baked) is distinguished from present-but-corrupt
        // below: a missing asset warns and passes provisionally; a present
        // file that fails to parse is a real defect and FAILS via REQUIRE.
        WARN("snowhill_stcharles.glb not baked yet at "
             << path << " -- SF1 mesh conformance leg cannot run yet.");
        SUCCEED("mesh asset absent (pre-bake)");
        return;
    }

    const GlbCore m = load_snowhill_glb(path);
    REQUIRE(m.ok);
    REQUIRE(!m.pos.empty());
    REQUIRE(m.pad > 0.0);  // parsed from the file, never assumed 0.862/0.78

    const cfg::WorldParams w = cfg::load_world_toml(std::string(SEADS_CONFIG_DIR) + "/world.toml");
    world::SnowhillParams p = w.snowhill;
    p.enabled = true;  // "the mesh of a disabled hill is still the authored one"
    // The glb is baked in LOCAL metres off the anchor; the dir formula and its
    // gnomonic inverse use the SAME up/east/north, so the frame cancels out of
    // this comparison (spec sec4) -- override to the synthetic frame the rest
    // of this file uses rather than pulling in the real GIS anchor.
    p.up = glm::dvec3(0.0, 0.0, 1.0);
    p.east = glm::dvec3(1.0, 0.0, 0.0);
    p.north = glm::dvec3(0.0, 1.0, 0.0);

    world::HeightField hf;
    hf.R = kR;
    hf.w = hf.h = 2;
    hf.px.assign(4, 0);

    double worst_err = 0.0;
    std::vector<double> radii_core;
    for (const glm::vec3& v : m.pos) {
        // AXIS MAP, PINNED (tools/snowhill_mesh.cpp): glb.X = y_north,
        // glb.Y = h, glb.Z = x_east.
        const double y_north = v.x;
        const double h = v.y;
        const double x_east = v.z;
        const double r = std::sqrt(x_east * x_east + y_north * y_north);
        if (r <= 44.0) {  // the CORE, where the pad window s1 == 1 exactly
            const glm::dvec3 dir = local_dir(hf, p, x_east, y_north);
            const double expected = world::snowhill_add(&hf, p, dir) + m.pad;
            worst_err = std::max(worst_err, std::abs(h - expected));
            radii_core.push_back(r);
        }
    }
    REQUIRE(!radii_core.empty());
    REQUIRE(worst_err <= 0.10);

    // Direction two: grid density. No feature at min sigma (3.6-4.0 m) can
    // hide between verts if consecutive distinct radii never gap past 2.5 m.
    std::sort(radii_core.begin(), radii_core.end());
    radii_core.erase(
        std::unique(radii_core.begin(), radii_core.end(),
                    [](double a, double b) { return std::abs(a - b) < 1e-6; }),
        radii_core.end());
    double max_gap = 0.0;
    for (std::size_t i = 0; i + 1 < radii_core.size(); ++i)
        max_gap = std::max(max_gap, radii_core[i + 1] - radii_core[i]);
    REQUIRE(max_gap <= 2.5);
}

// ------------------------------------------------------------------ mask --

TEST_CASE("snowhill_clears_the_canopy") {
    // SF1: the mountain footprint suppresses the tree scatter the way the
    // sec2.4b corridors do -- a plow does not build a 6 m pile through a
    // stand of spruce, and a snowform under trees is invisible AND
    // undrivable. The mask is snowhill_add(dir) > 0, i.e. exactly the
    // authored r_cut window: one function, no second constant. Outside the
    // footprint the scatter must be BIT-IDENTICAL (the corridor-mask
    // guarantee, inherited).
    world::HeightField hf = make_hf(512, 256, flat_half);
    world::DensityField dense;
    dense.w = 64;
    dense.h = 32;
    dense.px.assign(64 * 32, 255);
    world::TreeParams tp;
    tp.cells_per_face = 256;
    tp.chunk_cells = 256;
    tp.gain = 1.0;

    // Find a real instance to centre the hill on -- ask the scatter, never
    // assume a face/chunk indexing.
    const auto bare = world::place_chunk(0, 0, 0, hf, dense, tp, {}, {});
    REQUIRE(!bare.empty());
    world::SnowhillParams hill = make_hill_params();
    hill.up = glm::normalize(bare[std::min<std::size_t>(7, bare.size() - 1)].up);
    // Orthonormal tangent frame about the new anchor.
    glm::dvec3 e1 = glm::normalize(
        glm::cross(hill.up, std::abs(hill.up.z) < 0.9 ? glm::dvec3(0, 0, 1)
                                                      : glm::dvec3(1, 0, 0)));
    hill.east = e1;
    hill.north = glm::normalize(glm::cross(hill.up, e1));

    world::CorridorMask m;
    m.hill = &hill;
    m.hill_hf = &hf;
    const auto masked = world::place_chunk(0, 0, 0, hf, dense, tp, {}, m);

    int inside_bare = 0, inside_masked = 0, outside_bare = 0,
        outside_masked = 0;
    auto arc_m = [&](glm::dvec3 a) {
        return hf.R * std::acos(std::clamp(glm::dot(glm::normalize(a),
                                                    hill.up),
                                           -1.0, 1.0));
    };
    for (const auto& t : bare)
        (arc_m(t.up) < hill.r_cut_m ? inside_bare : outside_bare)++;
    for (const auto& t : masked)
        (arc_m(t.up) < hill.r_cut_m ? inside_masked : outside_masked)++;

    REQUIRE(inside_bare > 0);      // the fixture actually covers the footprint
    REQUIRE(inside_masked == 0);   // ... and the mask clears it
    REQUIRE(outside_masked == outside_bare);  // bit-identity outside (count)
}
