// M-KEY BUBBLE MAP — pure projection math (render/bubble_map.h). Pins:
//  - make_map_proj: the (east, north) tangent basis is orthonormal and both
//    axes are perpendicular to center (kills a non-orthogonal/unnormalized
//    basis mutant).
//  - round-trip: the map center projects to exactly (0,0); a point placed
//    10 km due east of the center (via point_at_bearing) projects to
//    ~(10000, 0) (kills a swapped X/Y or wrong-axis-dot mutant).
//  - ORIENTATION (the real correctness pin, Chad's spec): about the DEFAULT
//    map center (the Valley/Sudbury midpoint), Capreol must read NORTH-EAST
//    of Chelmsford, and Whitefish must read SOUTH-WEST of the Sudbury town
//    center — both baked unit dirs come straight from
//    test_faction_bubbles.cpp's roster (same provenance: offline_tool/
//    sudbury_geo.py). This is the leg that would catch a flipped north/east
//    handedness that the orthonormality pin above cannot see (that pin
//    passes for EITHER handedness).
//  - ellipse boundary sampler: every sampled point is unit-length, and the
//    sampled boundary reproduces sim/aero.h's OWN r_eff edge exactly — a
//    point sitting ON the sampled boundary reads atm_frac_at's edge factor
//    at d=0 (== 1.0, still just inside per atm_falloff's d<=0 plateau),
//    while the same bearing pushed 1% further out reads strictly less
//    (kills a boundary-formula transcription mutant, not just a "some
//    numbers came out" smoke test).

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <glm/glm.hpp>
#include <vector>

#include "config/load_aircraft.h"
#include "config/load_game.h"
#include "render/bubble_map.h"
#include "sim/aero.h"
#include "sim/fields.h"
#include "world/faction_bubbles.h"

#ifdef NDEBUG
#error "SEADS gate requires an assert-live build (SPEC 6.1)"
#endif

namespace {

const sim::AircraftParams kP =
    cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");
const cfg::GameParams kGame =
    cfg::load_game_toml(SEADS_CONFIG_DIR "/game.toml", kP);

// Baked town/landmark unit directions — copied verbatim from
// test_faction_bubbles.cpp's kBakedTowns roster (same offline provenance:
// offline_tool/sudbury_geo.py::SudburyFrame.geo_to_dir at LAT0/LON0
// 46.5560/-81.1100). Kept local (not shared) so this test's orientation pin
// stands on its own baked evidence.
const glm::dvec3 kCapreol{0.65011466438926646, 0.75319598270371746,
                          0.10023340154366206};
const glm::dvec3 kChelmsford{-0.43510447686431641, 0.19229751306740031,
                             0.87960545739594109};
const glm::dvec3 kWhitefish{-0.6566651062318245, -0.75169116505451417,
                            -0.061248107207766203};
const glm::dvec3 kSudburyTown{0.46892813603295158, -0.44923248180969061,
                              0.76045813857422018};

}  // namespace

TEST_CASE("bubble_map: make_map_proj basis is orthonormal and centered") {
    const render::MapProj mp =
        render::make_map_proj(render::default_map_center(), kP.R);
    CHECK(glm::length(mp.center) == Catch::Approx(1.0).epsilon(1e-9));
    CHECK(glm::length(mp.east) == Catch::Approx(1.0).epsilon(1e-9));
    CHECK(glm::length(mp.north) == Catch::Approx(1.0).epsilon(1e-9));
    CHECK(std::abs(glm::dot(mp.east, mp.north)) < 1e-9);
    CHECK(std::abs(glm::dot(mp.center, mp.east)) < 1e-9);
    CHECK(std::abs(glm::dot(mp.center, mp.north)) < 1e-9);
}

TEST_CASE("bubble_map: center projects to origin; 10km east lands at (10000,0)") {
    const render::MapProj mp =
        render::make_map_proj(render::default_map_center(), kP.R);
    const glm::dvec2 p0 = render::project_to_map(mp, mp.center);
    CHECK(p0.x == Catch::Approx(0.0).margin(1e-6));
    CHECK(p0.y == Catch::Approx(0.0).margin(1e-6));

    const glm::dvec3 east_pt =
        render::point_at_bearing(mp.center, mp.east, 10000.0, 0.0, kP.R);
    const glm::dvec2 pe = render::project_to_map(mp, east_pt);
    CHECK(pe.x == Catch::Approx(10000.0).epsilon(1e-6));
    CHECK(pe.y == Catch::Approx(0.0).margin(1e-3));

    // And 10km due north (theta = pi/2, since point_at_bearing's theta=0 is
    // along axis_a==east and rotates toward cross(center,east)==north).
    const glm::dvec3 north_pt = render::point_at_bearing(
        mp.center, mp.east, 10000.0, 3.14159265358979323846 / 2.0, kP.R);
    const glm::dvec2 pn = render::project_to_map(mp, north_pt);
    CHECK(pn.x == Catch::Approx(0.0).margin(1e-3));
    CHECK(pn.y == Catch::Approx(10000.0).epsilon(1e-6));
}

TEST_CASE("bubble_map: orientation - Capreol NE of Chelmsford, Whitefish SW of Sudbury") {
    const render::MapProj mp =
        render::make_map_proj(render::default_map_center(), kP.R);

    const glm::dvec2 capreol = render::project_to_map(mp, kCapreol);
    const glm::dvec2 chelmsford = render::project_to_map(mp, kChelmsford);
    INFO("capreol=(" << capreol.x << "," << capreol.y << ") chelmsford=("
                     << chelmsford.x << "," << chelmsford.y << ")");
    CHECK(capreol.x > chelmsford.x);  // Capreol is EAST of Chelmsford
    CHECK(capreol.y > chelmsford.y);  // Capreol is NORTH of Chelmsford

    const glm::dvec2 whitefish = render::project_to_map(mp, kWhitefish);
    const glm::dvec2 sudbury = render::project_to_map(mp, kSudburyTown);
    INFO("whitefish=(" << whitefish.x << "," << whitefish.y << ") sudbury=("
                       << sudbury.x << "," << sudbury.y << ")");
    CHECK(whitefish.x < sudbury.x);  // Whitefish is WEST of Sudbury
    CHECK(whitefish.y < sudbury.y);  // Whitefish is SOUTH of Sudbury
}

// ---------------------------------------------------------------------------
// S-maparrow (Chad 2026-08-09: "correct the arrow on the map so the point of
// it faces my actual direction? It gets spun around sometimes").
//
// These legs pin the ATTRIBUTED defect, not just "a number comes out":
//  - the bearing is read off the NOSE, so a tail-slide (velocity opposite the
//    nose) can no longer flip the arrow 180 deg — the leg drives exactly that
//    state and asserts the arrow still reads the nose's way;
//  - a near-vertical nose HOLDS the last good bearing instead of free-spinning
//    on tangential noise — the leg holds through a full vertical pass and
//    checks the bearing never moved, then that it RELEASES on the far side;
//  - the guard is a config-relative kill-switch: min_tan_frac = 0 reproduces
//    the legacy recompute-always behaviour exactly.
// ---------------------------------------------------------------------------
TEST_CASE("bubble_map: map arrow reads the NOSE, not the ground track") {
    const render::MapProj mp =
        render::make_map_proj(render::default_map_center(), kP.R);
    const glm::dvec3 pos = mp.center * (kP.R + 3000.0);

    // Nose due EAST => bearing +pi/2. (The paired MOVING-BACKWARD case below
    // is the real pin: a velocity-driven arrow would read -pi/2 there.)
    render::MapHeadingState h;
    const double east_rad =
        render::map_facing_heading_rad(mp, pos, mp.east, 0.15, h);
    CHECK(east_rad == Catch::Approx(3.14159265358979323846 / 2.0).margin(1e-9));
    CHECK(h.valid);

    // Nose due NORTH => bearing 0.
    render::MapHeadingState h2;
    CHECK(render::map_facing_heading_rad(mp, pos, mp.north, 0.15, h2) ==
          Catch::Approx(0.0).margin(1e-9));

    // TAIL-SLIDE: nose east while the aircraft MOVES west (alpha ~ 180, the
    // regime SPEC 9 calls out as "tail-slide lies"). The arrow must still read
    // east — this is failure mode (1) of the old ground-track source, the
    // literal "spun around".
    render::MapHeadingState h3;
    const double slide =
        render::map_facing_heading_rad(mp, pos, mp.east, 0.15, h3);
    CHECK(slide == Catch::Approx(3.14159265358979323846 / 2.0).margin(1e-9));
    // And the old source, computed here as the oracle, would have read the
    // OPPOSITE bearing — proving the legs are not vacuous.
    const glm::dvec3 vel_west = -mp.east * 180.0;
    glm::dvec3 vt = vel_west - glm::normalize(pos) *
                                   glm::dot(vel_west, glm::normalize(pos));
    vt = glm::normalize(vt);
    const double ground_track =
        std::atan2(glm::dot(vt, mp.east), glm::dot(vt, mp.north));
    CHECK(std::abs(ground_track - slide) ==
          Catch::Approx(3.14159265358979323846).margin(1e-6));
}

TEST_CASE("bubble_map: a near-vertical nose HOLDS the last bearing (no spin)") {
    const render::MapProj mp =
        render::make_map_proj(render::default_map_center(), kP.R);
    const glm::dvec3 pos = mp.center * (kP.R + 3000.0);
    const glm::dvec3 up = glm::normalize(pos);
    constexpr double kMinTan = 0.15;  // sin(8.6 deg)

    // Establish a good bearing (nose east), then pitch up through vertical
    // with a TINY, ROTATING tangential residue — the exact noise that used to
    // free-spin the arrow. Every sample inside the guard band must return the
    // stored bearing bit-for-bit.
    render::MapHeadingState h;
    const double ref = render::map_facing_heading_rad(mp, pos, mp.east, kMinTan, h);
    REQUIRE(h.valid);
    for (int i = 0; i < 64; ++i) {
        const double az = (2.0 * 3.14159265358979323846 * i) / 64.0;
        // tangential fraction 0.05 << 0.15: inside the hold band.
        const glm::dvec3 noisy = glm::normalize(
            up * std::sqrt(1.0 - 0.05 * 0.05) +
            (mp.east * std::cos(az) + mp.north * std::sin(az)) * 0.05);
        const double got =
            render::map_facing_heading_rad(mp, pos, noisy, kMinTan, h);
        CHECK(got == ref);  // EXACT: the guard must not let noise through
    }

    // ... and it RELEASES the moment the nose is well off vertical again
    // (a frozen-forever arrow would be its own bug — the loader rejects
    // arrow_hold_sin == 1 for the same reason).
    const double released =
        render::map_facing_heading_rad(mp, pos, mp.north, kMinTan, h);
    CHECK(released == Catch::Approx(0.0).margin(1e-9));
    CHECK(released != ref);
}

TEST_CASE("bubble_map: arrow_hold_sin = 0 is the legacy recompute-always arm") {
    const render::MapProj mp =
        render::make_map_proj(render::default_map_center(), kP.R);
    const glm::dvec3 pos = mp.center * (kP.R + 3000.0);
    const glm::dvec3 up = glm::normalize(pos);

    render::MapHeadingState h;
    (void)render::map_facing_heading_rad(mp, pos, mp.east, 0.0, h);
    // The SAME tiny-tangential sample that the guard would have swallowed now
    // updates the bearing — the kill-switch is live, so a fly-time
    // arrow_hold_sin = 0 restores the pre-S-maparrow degeneracy exactly.
    const glm::dvec3 noisy = glm::normalize(
        up * std::sqrt(1.0 - 0.01 * 0.01) + mp.north * 0.01);
    const double got = render::map_facing_heading_rad(mp, pos, noisy, 0.0, h);
    CHECK(got == Catch::Approx(0.0).margin(1e-6));
}

TEST_CASE("bubble_map: ellipse boundary sampler returns a unit-length, closed, bearing-monotone loop") {
    render::EllipseParams e;
    e.center_dir = world::kValleyCenterDir;
    e.major_axis = world::kValleyMajorAxis;
    e.major_radius_m = world::kValleyMajorRadiusM;
    e.minor_radius_m = world::kValleyMinorRadiusM;

    const std::vector<glm::dvec3> loop = render::sample_ellipse_boundary(e, kP.R, 64);
    REQUIRE(loop.size() == 64);

    const glm::dvec3 c = glm::normalize(e.center_dir);
    glm::dvec3 m = e.major_axis - c * glm::dot(e.major_axis, c);
    m = glm::normalize(m);
    const glm::dvec3 mn = glm::normalize(glm::cross(c, m));

    double prev_bearing = -1.0;
    for (size_t i = 0; i < loop.size(); ++i) {
        CHECK(glm::length(loop[i]) == Catch::Approx(1.0).epsilon(1e-9));
        // Recover the bearing of this sample point about the ellipse center
        // and check it strictly increases (monotone => the loop cannot
        // self-intersect).
        glm::dvec3 t = loop[i] - c * glm::dot(loop[i], c);
        t = glm::normalize(t);
        double bearing = std::atan2(glm::dot(t, mn), glm::dot(t, m));
        if (bearing < 0.0) bearing += 2.0 * 3.14159265358979323846;
        if (i > 0) CHECK(bearing > prev_bearing);
        prev_bearing = bearing;
    }
}

TEST_CASE("bubble_map: sampled boundary reproduces sim/aero.h's exact r_eff edge") {
    // Build a real AtmosphereField with ONE ellipse bubble (Valley's baked
    // shape) and check atm_frac_at's edge factor AT a sampled boundary point
    // (should read the d<=0 plateau, u_h==1.0 exactly) vs. the SAME bearing
    // pushed 1% further out (should read strictly less, given a real
    // edge_soft_m > 0).
    sim::AtmosphereField::Bubble b{};
    b.center_dir = world::kValleyCenterDir;
    b.major_axis = world::kValleyMajorAxis;
    b.ground_radius_m = world::kValleyMajorRadiusM;
    b.minor_radius_m = world::kValleyMinorRadiusM;
    b.ceiling_m = kGame.atmosphere.bubble_ceiling_m;
    b.edge_soft_m = kGame.atmosphere.bubble_edge_soft_m;
    b.ceil_soft_m = kGame.atmosphere.bubble_ceil_soft_m;
    REQUIRE(b.edge_soft_m > 0.0);

    sim::AtmosphereField af;
    af.bubbles.push_back(b);
    sim::Environment env;
    env.atm = &af;

    render::EllipseParams e;
    e.center_dir = b.center_dir;
    e.major_axis = b.major_axis;
    e.major_radius_m = b.ground_radius_m;
    e.minor_radius_m = b.minor_radius_m;
    const std::vector<glm::dvec3> loop = render::sample_ellipse_boundary(e, kP.R, 16);
    REQUIRE(loop.size() == 16);

    // Sample well ABOVE the global "deck" (config [atmosphere]
    // deck_agl_m=120 + deck_soft_m=200 => full air below 320 m AGL
    // regardless of any bubble) and well below the 4000 m ceiling, so the
    // bubble's OWN horizontal edge is what's actually being read here (at
    // ground level atm_frac_at reads 1.0 everywhere via the deck term,
    // masking the very edge this test exists to pin).
    constexpr double kTestAlt = 1000.0;
    const double test_r = kP.R + kTestAlt;
    const glm::dvec3 c = glm::normalize(b.center_dir);
    for (const glm::dvec3& dir : loop) {
        const glm::dvec3 on_edge = dir * test_r;
        const double frac_on = sim::atm_frac_at(on_edge, &env, kP);

        // Push 2% further ALONG THE SAME GREAT-CIRCLE BEARING from the
        // ellipse center (not just radially off the sphere) — i.e. widen
        // theta = arc/R by 2% while keeping the same tangent direction from
        // center, same altitude R.
        const double c_dot = glm::clamp(glm::dot(c, dir), -1.0, 1.0);
        const double theta = std::acos(c_dot);
        glm::dvec3 t = dir - c * c_dot;
        const double t_len = glm::length(t);
        REQUIRE(t_len > 1e-9);
        t /= t_len;
        const double theta_beyond = theta * 1.02;
        const glm::dvec3 dir_beyond = glm::normalize(
            c * std::cos(theta_beyond) + t * std::sin(theta_beyond));
        const glm::dvec3 beyond = dir_beyond * test_r;
        const double frac_beyond = sim::atm_frac_at(beyond, &env, kP);

        CHECK(frac_on > frac_beyond);
    }
}
