// ★★★ L7 — TRAMPLED SNOW AT THE FLAK GUN.
//
// CHAD'S RULING (2026-09-03): "a gun should not be placed on top of snow, just
// trample the snow down around the gun", and "it should be trampled a little
// bigger than the gun pad itself; it's fun to drive right up to the flak gun
// in deep snow, I don't want a big 60 m cleared section."
//
// WHAT WAS ACTUALLY WRONG, MEASURED. The gun's `pos` is BARE TERRAIN
// (app/main.cpp: `u_site * radius_at(u_site)`), deliberately -- the pedestal's
// snow crib is 0.15 m deep and base-at-surface sets its top flush. The SNOW is
// a separate field on top of that, and at the Valley site it is 0.727 m of
// ambient (drawn +0.768 m over the base). Meanwhile the gunner's own sight
// camera FALLS as the barrel rises -- 1.78 m over the base at 0 deg, 0.75 m at
// 42 deg, 0.10 m at the 87 deg stop -- so somewhere around 40 deg of elevation
// his eye goes UNDER the drawn snow and he is sighting through a white wall.
//
// ★ THE FIX IS THE SNOW, NOT THE GUN. Lifting `pos` onto the snow would put a
// pedestal on a surface that deforms and is cut by tracks, and would fork the
// gun's ground from every other consumer of radius_at. So the gun stays exactly
// where it is and the CREW TRAMPLES: a 3.5 m disk of packed floor with a 1.5 m
// feather back to full depth, registered by app/ into
// world::SnowpackField::tramples and applied at the end of ambient_depth_at --
// the ROOT term every channel derives from, so the driven surface, the drawn
// patch, the sinkage kernel and the dash cannot fork.
//
// ★★★ THE NAMED WRONG IMPLEMENTATIONS, each with the leg that catches it:
//   * "just clear it like the trees do" -- a 60 m disk. `deep snow right up to
//     the lip` goes red (a point 6 m out must still be at FULL depth).
//   * "min it always, the empty vector is the same thing" -- an unconditional
//     min(depth, keep). `zero disks are BIT-IDENTICAL` goes red.
//   * "the lattice is fine, the field is smooth at 40 m" -- fold_lat_n 9. `the
//     disk survives into the DRAWN patch` goes red: the whole hollow is 10 m
//     across, ONE step of the old lattice, and bilinear interpolation averages
//     it away wherever it does not happen to land on a node.
//   * "raise the gun onto the snow instead" -- a mutation of app/main.cpp, not
//     of this file; the leg that pins the rule is `the gun does not move`
//     below, which grounds the mount frame on radius_at and nothing else.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include "cgltf.h"  // ONE CGLTF_IMPLEMENTATION lives in test_asset_validator.cpp
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#include "render/flak_gun.h"
#include "render/snow_patch.h"
#include "world/heightfield.h"
#include "world/snowpack.h"

#ifdef NDEBUG
#error "SEADS gate requires an assert-live build (SPEC 6.1)"
#endif

using Catch::Matchers::WithinAbs;

namespace {

constexpr double kR = 15000.0;     // config/aircraft.toml world.R
constexpr double kRelief = 350.0;  // config/world.toml ground.relief_scale_m
constexpr double kPi = 3.14159265358979323846;
constexpr double kDeg = kPi / 180.0;

// ★ THE MEASURED VALLEY AMBIENT. Not a chosen number: this is what the shipped
// world carries at the Valley gun site (read-only probe, 2026-09-03), and it is
// the depth the whole rung exists to answer.
constexpr double kValleyAmbientM = 0.727;

// The registration app/main.cpp performs, mirrored here so the legs below are
// graded on the shipped numbers rather than on invented ones.
constexpr double kTrampleRadiusM = 3.5;
constexpr double kTrampleFeatherM = 1.5;
constexpr double kTrampleKeepM = 0.05;

// A DEAD FLAT height field. Flat is not a convenience here, it is the
// ISOLATION: slope, curvature, aspect and drainage are all EXACTLY zero on it,
// so the only thing that can move the depth in these legs is the trample term.
world::HeightField flat_hf() {
    world::HeightField hf;
    hf.w = 64;
    hf.h = 32;
    hf.R = kR;
    hf.relief_scale = kRelief;
    hf.u_offset = 0.0;
    hf.px.assign(static_cast<std::size_t>(hf.w) * hf.h,
                 static_cast<std::uint16_t>(32768));
    return hf;
}

// The field, carrying the MEASURED Valley ambient.
//
// ★ base_m HERE IS A FIXTURE, NOT A DIAL. WINTER_LAW's "never lower base_m"
// governs config/world.toml's [snowpack]; nothing in this file touches it. This
// is a hand-built SnowParams (the test_snowpack precedent) whose one job is to
// make a synthetic flat world carry the depth the real Valley site was measured
// to carry, so the legs read the real number instead of a default.
// elev_gain_per_km is zeroed for the same reason: with it live, the fixture's
// own elevation would perturb the number the measurement pinned.
world::SnowpackField valley_field(const world::HeightField& hf) {
    world::SnowpackField f;
    f.hf = &hf;
    f.p.base_m = kValleyAmbientM;
    f.p.elev_gain_per_km = 0.0;
    return f;
}

// A GENERIC site: no world axis is up, so no leg can pass by agreeing with one.
glm::dvec3 site_dir() {
    return glm::normalize(glm::dvec3(0.41, 1.0, 0.27));
}

// Step `s` metres along a great circle from `d` in tangent direction `t`.
glm::dvec3 step_m(const glm::dvec3& d, const glm::dvec3& t, double s) {
    const glm::dvec3 tn = glm::normalize(t - d * glm::dot(t, d));
    const double a = s / kR;
    return glm::normalize(d * std::cos(a) + tn * std::sin(a));
}

// Any tangent at `d`, chosen off `d` itself (never a world axis).
glm::dvec3 tangent_a(const glm::dvec3& d) {
    const glm::dvec3 a =
        std::fabs(d.x) < 0.9 ? glm::dvec3(1, 0, 0) : glm::dvec3(0, 1, 0);
    return glm::normalize(glm::cross(d, a));
}
glm::dvec3 tangent_b(const glm::dvec3& d) {
    return glm::normalize(glm::cross(d, tangent_a(d)));
}

// ★ THE UNTOUCHED AMBIENT, MEASURED OFF THE FIXTURE RATHER THAN ASSERTED
// AGAINST THE LITERAL. On a flat raster the slope/curvature finite differences
// are exactly zero in exact arithmetic and ~1e-8 in doubles (probe_m 40 m
// against a ~15.2 km radius), so the field returns kValleyAmbientM to about
// eight figures and no further. Grading the trample legs on the literal would
// be grading them on THAT rounding; grading them on the field's own untouched
// answer grades them on the disk. The literal is checked ONCE, loosely, so the
// fixture is still pinned to the measurement it claims to carry.
double untouched_ambient(const world::HeightField& hf) {
    world::SnowpackField f = valley_field(hf);
    return f.ambient_depth_at(site_dir());
}

world::SnowpackField::TrampleDisk gun_disk(const glm::dvec3& d) {
    world::SnowpackField::TrampleDisk t;
    t.dir = d;
    t.radius_m = kTrampleRadiusM;
    t.feather_m = kTrampleFeatherM;
    t.keep_m = kTrampleKeepM;
    return t;
}

// --- the shipped GLB's station table (the test_flak_gun open_glb precedent:
// the gunner's eye is READ FROM THE FILE, never a header constant, so a leg
// about where he can see from cannot pass against a ghost model).
struct Glb {
    bool ok = false;
    std::map<std::string, glm::dvec3> world;
};

Glb open_glb() {
    Glb g;
    const std::string path =
        std::string(SEADS_ASSET_DIR) + "/flak/oerlikon_mk4.glb";
    cgltf_options opt{};
    cgltf_data* d = nullptr;
    if (cgltf_parse_file(&opt, path.c_str(), &d) != cgltf_result_success)
        return g;
    if (cgltf_load_buffers(&opt, d, path.c_str()) != cgltf_result_success) {
        cgltf_free(d);
        return g;
    }
    for (cgltf_size i = 0; i < d->nodes_count; ++i) {
        const cgltf_node& n = d->nodes[i];
        cgltf_float w[16];
        cgltf_node_transform_world(&n, w);
        g.world[n.name ? n.name : ""] = glm::dvec3(w[12], w[13], w[14]);
    }
    cgltf_free(d);
    g.ok = true;
    return g;
}

render::flak::Stations stations_from(const Glb& g) {
    render::flak::Stations s;
    s.trunnion_h = g.world.at(render::flak::kNodeCradle).y;
    s.train_h = g.world.at(render::flak::kNodeTrain).y;
    s.eye = g.world.at("st_eye");
    s.sight_rear = g.world.at("st_sight_rear");
    s.sight_front = g.world.at("st_sight_front");
    s.approach = g.world.at("st_approach");
    return s;
}

// The camera the gunner actually looks through: st_eye through the cradle
// kinematics, then kSightEyeBackM BACK along the sight axis
// (render/flak_gun.h -- the F-POSE framing dial, not a near-plane dodge).
glm::dvec3 sight_eye_world(const render::flak::MountFrame& f,
                           const render::flak::Stations& s, double elev_rad) {
    render::flak::Pose p;
    p.elev_rad = elev_rad;
    const render::flak::SightCamera c = render::flak::sight_camera(f, s, p);
    return c.eye - c.forward * render::flak::kSightEyeBackM;
}

}  // namespace

// ---------------------------------------------------------------------------
// THE WORLD LAW: what a disk does to the field, at three radii and at none.
// ---------------------------------------------------------------------------

TEST_CASE("gun trample: zero disks are BIT-IDENTICAL", "[snowpack][trample]") {
    const world::HeightField hf = flat_hf();
    world::SnowpackField f = valley_field(hf);
    REQUIRE(f.tramples.empty());

    // The fixture carries the measured Valley ambient, EXACTLY -- and it is the
    // UNTOUCHED analytic value, not a trampled one. An unconditional
    // `min(depth, keep_m)` (the named wrong implementation) reads 0.05 here.
    const glm::dvec3 d = site_dir();
    const double amb = f.ambient_depth_at(d);

    // THE FIXTURE IS THE MEASUREMENT (loosely -- see untouched_ambient), and
    // with no disk the field returns the FULL Valley depth. An unconditional
    // `min(depth, keep_m)` -- the named wrong implementation -- reads 0.05 here
    // and turns this line red.
    CHECK_THAT(amb, WithinAbs(kValleyAmbientM, 1e-6));
    CHECK(amb > kTrampleKeepM * 10.0);

    // Every channel that derives from it agrees EXACTLY, with no disk anywhere:
    // "bit-identical" is the claim, so `==` is the assertion.
    CHECK(f.depth_at(d) == amb);
    CHECK(f.depth_geometry_at(d) == amb);
    CHECK(f.draw_fold_at(d) == amb);
    CHECK(f.drive_radius_at(d) == hf.radius_at(d) + amb);

    // ... and out across the country the disk would have covered.
    const glm::dvec3 ta = tangent_a(d);
    for (int i = 0; i < 24; ++i)
        CHECK_THAT(f.ambient_depth_at(step_m(d, ta, i * 3.0)),
                   WithinAbs(amb, 1e-6));
}

TEST_CASE("gun trample: inside is packed, the feather ramps, outside is deep",
          "[snowpack][trample]") {
    const world::HeightField hf = flat_hf();
    world::SnowpackField f = valley_field(hf);
    const glm::dvec3 d = site_dir();
    const double amb = untouched_ambient(hf);
    f.tramples.push_back(gun_disk(d));
    const glm::dvec3 ta = tangent_a(d);

    // INSIDE: flat, packed, all the way out to the radius.
    CHECK_THAT(f.ambient_depth_at(d), WithinAbs(kTrampleKeepM, 1e-12));
    for (double s : {0.0, 0.5, 1.524, 2.5, 3.4999})
        CHECK_THAT(f.ambient_depth_at(step_m(d, ta, s)),
                   WithinAbs(kTrampleKeepM, 1e-9));

    // THE FEATHER: monotone, continuous at BOTH knees, never a step. A step at
    // the lip is what stops a machine dead -- the same bound §2.4c puts on the
    // snowbank, and the lip is exactly where Chad wants to drive.
    double prev = kTrampleKeepM;
    for (int i = 0; i <= 60; ++i) {
        const double s = kTrampleRadiusM + kTrampleFeatherM * (i / 60.0);
        const double v = f.ambient_depth_at(step_m(d, ta, s));
        CHECK(v >= prev - 1e-12);   // monotone up
        CHECK(v <= amb + 1e-6);     // never overshoots ambient
        CHECK(v - prev < 0.05);     // and never steps
        prev = v;
    }

    // OUTSIDE: full depth again the instant the feather closes.
    CHECK_THAT(f.ambient_depth_at(step_m(d, ta, 5.0)), WithinAbs(amb, 1e-6));

    // ★★★ "IT'S FUN TO DRIVE RIGHT UP TO THE FLAK GUN IN DEEP SNOW." Six metres
    // out is UNTOUCHED. Mutation: radius_m 60 (the tree-pad clearing Chad ruled
    // against) turns this red.
    for (double s : {6.0, 10.0, 30.0, 60.0})
        CHECK_THAT(f.ambient_depth_at(step_m(d, ta, s)), WithinAbs(amb, 1e-6));

    // ISOTROPIC: it is a disk on the sphere, not an axis-aligned square.
    const glm::dvec3 tb = tangent_b(d);
    for (int i = 0; i < 8; ++i) {
        const double a = i * (kPi / 4.0);
        const glm::dvec3 t = ta * std::cos(a) + tb * std::sin(a);
        CHECK_THAT(f.ambient_depth_at(step_m(d, t, 2.0)),
                   WithinAbs(kTrampleKeepM, 1e-9));
        CHECK_THAT(f.ambient_depth_at(step_m(d, t, 6.0)),
                   WithinAbs(amb, 1e-6));
    }
}

TEST_CASE("gun trample: it only ever SUBTRACTS", "[snowpack][trample]") {
    // The Sudbury gun stands on 0.096 m of ambient -- thin ground. A disk that
    // ASSIGNED keep_m would PILE snow onto ground thinner than its floor; the
    // law is min(), so thin ground is left exactly alone.
    const world::HeightField hf = flat_hf();
    world::SnowpackField f = valley_field(hf);
    f.p.base_m = 0.030;  // thinner than keep_m: the disk must do NOTHING
    const glm::dvec3 d = site_dir();
    f.tramples.push_back(gun_disk(d));
    CHECK_THAT(f.ambient_depth_at(d), WithinAbs(0.030, 1e-6));
    CHECK_THAT(f.ambient_depth_at(step_m(d, tangent_a(d), 4.2)),
               WithinAbs(0.030, 1e-6));
}

TEST_CASE("gun trample: the disks compose and stay local",
          "[snowpack][trample]") {
    const world::HeightField hf = flat_hf();
    world::SnowpackField f = valley_field(hf);
    const glm::dvec3 a = site_dir();
    const glm::dvec3 b = step_m(a, tangent_a(a), 400.0);  // the other faction
    const double amb = untouched_ambient(hf);
    f.tramples.push_back(gun_disk(a));
    f.tramples.push_back(gun_disk(b));
    CHECK_THAT(f.ambient_depth_at(a), WithinAbs(kTrampleKeepM, 1e-12));
    CHECK_THAT(f.ambient_depth_at(b), WithinAbs(kTrampleKeepM, 1e-12));
    // ... and the 200 m of country between them is untouched.
    CHECK_THAT(f.ambient_depth_at(step_m(a, tangent_a(a), 200.0)),
               WithinAbs(amb, 1e-6));
}

// ---------------------------------------------------------------------------
// THE GUNNER'S VIEW: the geometry the rung exists for.
// ---------------------------------------------------------------------------

TEST_CASE("gun trample: the gunner is above the floor and the lip is low",
          "[flak][snowpack][trample][asset]") {
    const Glb g = open_glb();
    REQUIRE(g.ok);
    const render::flak::Stations st = stations_from(g);

    const world::HeightField hf = flat_hf();
    world::SnowpackField f = valley_field(hf);
    const glm::dvec3 up = site_dir();
    const double amb = untouched_ambient(hf);
    // ★ THE GUN DOES NOT MOVE: its position is BARE TERRAIN, exactly the rule
    // app/main.cpp uses (`u_site * radius_at(u_site)`). If the fix were ever
    // re-implemented by lifting the pedestal onto the snow, this line is where
    // the change would have to be made -- and every number below would move.
    const double r_ground = hf.radius_at(up);
    const render::flak::MountFrame mf = render::flak::make_mount_frame(
        up * r_ground, up, glm::dvec3(0.3, -0.2, 0.9));

    f.tramples.push_back(gun_disk(up));

    // 1. THE DRAWN SNOW AT THE GUN'S OWN POSITION IS THE PACKED FLOOR.
    //    draw_fold_at is what the render surface carries; depth_at is what the
    //    machine sinks into. Both read the pad, from ONE application.
    CHECK(f.draw_fold_at(up) <= kTrampleKeepM + 0.001);
    CHECK(f.depth_at(up) <= kTrampleKeepM + 0.001);
    const double r_floor = r_ground + f.draw_fold_at(up);

    // 2. THE SIGHT CAMERA CLEARS THE FLOOR AT EVERY ELEVATION, INCLUDING THE
    //    87 deg STOP -- which is the one that was buried.
    for (double e_deg : {-5.0, 0.0, 20.0, 42.0, 70.0, 87.0}) {
        const glm::dvec3 eye = sight_eye_world(mf, st, e_deg * kDeg);
        INFO("elev " << e_deg << " deg, eye " << (glm::length(eye) - r_ground)
                     << " m over the base");
        CHECK(glm::length(eye) > r_floor);
    }
    // ... and the buried case is REAL, not hypothetical: at the stop the eye is
    // below the UNTRAMPLED snow. That is the defect, pinned so nobody can call
    // this rung cosmetic.
    const glm::dvec3 eye87 = sight_eye_world(mf, st, 87.0 * kDeg);
    CHECK(glm::length(eye87) < r_ground + amb);

    // 3. THE RIM IS LOW. The snow comes back to full depth at radius+feather;
    //    from the gunner's eye at the stop, the top of that lip must sit no
    //    more than 15 deg above his own horizontal, or the trampled pad is a
    //    well he is looking out of rather than a floor he is standing on.
    const glm::dvec3 ta = tangent_a(up), tb = tangent_b(up);
    double worst_deg = -90.0;
    for (int i = 0; i < 32; ++i) {
        const double a = i * (2.0 * kPi / 32.0);
        const glm::dvec3 t = ta * std::cos(a) + tb * std::sin(a);
        const glm::dvec3 rd = step_m(up, t, kTrampleRadiusM + kTrampleFeatherM);
        const double rim_depth = f.draw_fold_at(rd);  // the lip is FULL depth
        CHECK_THAT(rim_depth, WithinAbs(amb, 1e-6));
        const glm::dvec3 rim = rd * (hf.radius_at(rd) + rim_depth);
        const glm::dvec3 v = glm::normalize(rim - eye87);
        worst_deg = std::max(worst_deg, std::asin(glm::dot(v, up)) / kDeg);
    }
    INFO("worst rim elevation from the sight eye at the 87 deg stop: "
         << worst_deg << " deg");
    CHECK(worst_deg <= 15.0);

    // 4. AND THE MAN'S OWN MARK IS ON THE PAD. st_approach is 1.70 m behind the
    //    train axis, which is what sized the 3.5 m radius (pad 1.524 + the
    //    step-back + margin): he walks up onto trampled snow, not into a drift.
    const double approach_m = glm::length(st.approach);
    CHECK(approach_m < kTrampleRadiusM);
    CHECK_THAT(f.ambient_depth_at(step_m(up, ta, approach_m)),
               WithinAbs(kTrampleKeepM, 1e-9));
}

// ---------------------------------------------------------------------------
// THE DRAWN PATCH: the disk has to survive the lattice, or it is driven-only.
// ---------------------------------------------------------------------------

TEST_CASE("gun trample: the disk survives into the DRAWN rider patch",
          "[render][snowpack][trample]") {
    const world::HeightField hf = flat_hf();
    world::SnowpackField f = valley_field(hf);
    const glm::dvec3 gun = site_dir();
    const double amb = untouched_ambient(hf);
    f.tramples.push_back(gun_disk(gun));

    render::SnowPatchParams pp;  // the SHIPPED patch, fold_lat_n included
    const double span_m = pp.cell_m * (pp.n_side - 1);
    const double step_shipped = span_m / (pp.fold_lat_n - 1);
    const double step_old = span_m / (9 - 1);  // the pre-L7 lattice

    const glm::dvec3 ta = tangent_a(gun), tb = tangent_b(gun);

    // The patch anchors on the EYE, not on the gun, so the disk centre lands
    // wherever it lands relative to the lattice. The offsets below are the
    // WORST CASE for a lattice -- half a step in each axis -- one for the old
    // lattice, one for the shipped one, plus the coincidence case (anchored on
    // the gun) that an odd lattice passes for free and that is therefore NOT
    // evidence on its own.
    struct Case {
        const char* name;
        double off_m;
    };
    const Case cases[] = {
        {"half a step of the OLD 9-node lattice", 0.5 * step_old},
        {"half a step of the SHIPPED lattice", 0.5 * step_shipped},
        {"anchored on the gun itself (the free coincidence)", 0.0}};
    for (const Case& c : cases) {
        INFO(c.name << " (" << c.off_m << " m)");
        const glm::dvec3 anchor = step_m(step_m(gun, ta, c.off_m), tb, c.off_m);
        render::SnowPatchBuild b;
        render::snow_patch_begin(b, pp, anchor, hf.R, &f);
        REQUIRE(b.fold_n == pp.fold_lat_n);
        // SOME node of the lattice must land inside the trampled flat, or the
        // bilinear read averages the hollow away and the pad is DRAWN buried
        // while it is DRIVEN packed -- the exact fork ambient_depth_at exists
        // to prevent.
        const double lo = *std::min_element(b.fold.begin(), b.fold.end());
        CHECK(lo <= kTrampleKeepM + 0.001);
        // ... and the patch still carries FULL depth out in the country, so
        // the refinement did not flatten the world it draws.
        const double hi = *std::max_element(b.fold.begin(), b.fold.end());
        CHECK_THAT(hi, WithinAbs(amb, 1e-5));
    }

    // ★ THE MEASUREMENT BEHIND fold_lat_n. The whole hollow is
    // 2*(radius+feather) = 10 m across; a lattice step must be small enough
    // that a node lands in the 3.5 m FLAT from ANY offset, i.e.
    // step*sqrt(2)/2 < radius. The old 9-node lattice misses the whole hollow
    // at its own worst offset, which is why the first case above is the one
    // that fails under the fold_lat_n 9 mutation.
    CHECK(step_shipped * std::sqrt(2.0) / 2.0 < kTrampleRadiusM);
    CHECK(step_shipped <= 2.5);
    CHECK(step_old * std::sqrt(2.0) / 2.0 > kTrampleRadiusM + kTrampleFeatherM);
}
