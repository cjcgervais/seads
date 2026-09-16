// THE FLAK GUN -- the shipped GLB is the contract, and the kinematics are
// pinned at a GENERIC point (docs/FLAK_GUN_SPEC.md §6).
//
// Two halves:
//   A. assets/flak/oerlikon_mk4.glb opened with cgltf (test-side, the
//      test_rider_rig / test_snowhill precedent): node names, the hierarchy,
//      the station positions the header's constants describe, the limits in
//      the extras, the vertex budget, the mono materials. A re-export that
//      moves a station or drops an empty goes RED here, not in a fly.
//   B. render/flak_gun.h at up = normalize(1,1,1) so no world axis coincides
//      with local up (the "flat instrument certifies a flat controller" trap,
//      CLAUDE.md Learned) -- the sign conventions (+train = right, +elev =
//      up), the clamp, the short-way slew, the station transform.
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <vector>

#include "render/flak_gun.h"
#include "render/team_color.h"  // the ONE faction palette (the sight ring)
#include "render/gun_audio.h"  // STAGE C: the reload-foley envelope

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include "cgltf.h"  // ONE CGLTF_IMPLEMENTATION lives in test_asset_validator.cpp
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#ifdef NDEBUG
#error "SEADS gate requires an assert-live build (SPEC 6.1)"
#endif

namespace {

using namespace render::flak;
using Catch::Matchers::WithinAbs;

struct Glb {
    bool ok = false;
    std::map<std::string, glm::dvec3> world;   // node name -> world position
    std::map<std::string, std::string> parent;
    std::map<std::string, bool> has_mesh;
    std::map<std::string, std::map<std::string, double>> extras;
    std::size_t verts = 0;
    std::vector<double> base_color_r, base_color_g, base_color_b;
};

Glb open_glb() {
    Glb g;
    const std::string path = std::string(SEADS_ASSET_DIR) + "/flak/oerlikon_mk4.glb";
    cgltf_options opt{};
    cgltf_data* d = nullptr;
    if (cgltf_parse_file(&opt, path.c_str(), &d) != cgltf_result_success) return g;
    if (cgltf_load_buffers(&opt, d, path.c_str()) != cgltf_result_success) {
        cgltf_free(d);
        return g;
    }
    for (cgltf_size i = 0; i < d->nodes_count; ++i) {
        const cgltf_node& n = d->nodes[i];
        const std::string name = n.name ? n.name : "";
        cgltf_float w[16];
        cgltf_node_transform_world(&n, w);
        g.world[name] = glm::dvec3(w[12], w[13], w[14]);
        g.parent[name] = (n.parent && n.parent->name) ? n.parent->name : "";
        g.has_mesh[name] = n.mesh != nullptr;
        // extras: the exporter writes a JSON object; parse the few numeric
        // keys we care about by hand (cgltf hands us raw JSON text)
        if (n.extras.data) {
            const std::string j = n.extras.data;
            for (const char* key : {"elev_min_deg", "elev_max_deg", "trunnion_h_m",
                                    "muzzle_speed_mps", "rof_hz", "drum_rounds"}) {
                const auto p = j.find(std::string("\"") + key + "\"");
                if (p == std::string::npos) continue;
                const auto c = j.find(':', p);
                if (c == std::string::npos) continue;
                g.extras[name][key] = std::strtod(j.c_str() + c + 1, nullptr);
            }
        }
    }
    for (cgltf_size i = 0; i < d->meshes_count; ++i)
        for (cgltf_size p = 0; p < d->meshes[i].primitives_count; ++p)
            for (cgltf_size a = 0; a < d->meshes[i].primitives[p].attributes_count; ++a)
                if (d->meshes[i].primitives[p].attributes[a].type ==
                    cgltf_attribute_type_position)
                    g.verts += d->meshes[i].primitives[p].attributes[a].data->count;
    for (cgltf_size i = 0; i < d->materials_count; ++i) {
        const cgltf_material& m = d->materials[i];
        REQUIRE(m.has_pbr_metallic_roughness);
        g.base_color_r.push_back(m.pbr_metallic_roughness.base_color_factor[0]);
        g.base_color_g.push_back(m.pbr_metallic_roughness.base_color_factor[1]);
        g.base_color_b.push_back(m.pbr_metallic_roughness.base_color_factor[2]);
    }
    cgltf_free(d);
    g.ok = true;
    return g;
}

const char* kCradleStations[] = {"st_muzzle", "st_eye", "st_sight_rear", "st_sight_front",
                                 "st_grip_l", "st_grip_r", "st_pad_l", "st_pad_r",
                                 "st_eject", "st_drum"};
const char* kTrainStations[] = {"st_foot_l", "st_foot_r", "st_approach"};
const char* kCradleMeshes[] = {"flak_gun", "flak_drum", "flak_sight", "flak_grips",
                               "flak_rest"};

MountFrame generic_frame() {
    // A generic point: no world axis is up, none is forward.
    const glm::dvec3 up = glm::normalize(glm::dvec3(1.0, 1.0, 1.0));
    return make_mount_frame(up * 15000.0, up, glm::dvec3(0.3, -0.2, 0.9));
}

}  // namespace

// ---------------------------------------------------------------- A. the GLB
TEST_CASE("flak GLB: the driven hierarchy and every station are present",
          "[flak][asset]") {
    const Glb g = open_glb();
    REQUIRE(g.ok);
    REQUIRE(g.parent.at(kNodeTrain) == kNodeRoot);
    REQUIRE(g.parent.at(kNodeCradle) == kNodeTrain);
    REQUIRE(g.parent.at("flak_carriage") == kNodeTrain);
    REQUIRE(g.parent.at("flak_pedestal") == kNodeRoot);
    REQUIRE(g.parent.at("flak_pad") == kNodeRoot);
    for (const char* n : kCradleMeshes) {
        INFO(n);
        REQUIRE(g.parent.at(n) == kNodeCradle);
        REQUIRE(g.has_mesh.at(n));
    }
    for (const char* n : kCradleStations) {
        INFO(n);
        REQUIRE(g.parent.at(n) == kNodeCradle);
        REQUIRE_FALSE(g.has_mesh.at(n));   // stations are EMPTIES
    }
    for (const char* n : kTrainStations) {
        INFO(n);
        REQUIRE(g.parent.at(n) == kNodeTrain);
        REQUIRE_FALSE(g.has_mesh.at(n));
    }
    // the driven nodes themselves carry no mesh -- a mesh on flak_train would
    // be drawn twice by a loader that composes per-node
    REQUIRE_FALSE(g.has_mesh.at(kNodeTrain));
    REQUIRE_FALSE(g.has_mesh.at(kNodeCradle));
    // raylib converts u32 -> u16 indices at load (buildings.cpp precedent)
    REQUIRE(g.verts > 1000);
    REQUIRE(g.verts < 65535);
}

TEST_CASE("flak GLB: the stations sit where the sourced dimensions put them",
          "[flak][asset]") {
    const Glb g = open_glb();
    REQUIRE(g.ok);
    const auto& w = g.world;
    // trunnion: built at the Sudburian's shoulder, inside the jack-screw range
    const double th = w.at(kNodeCradle).y;
    CHECK_THAT(th, WithinAbs(kTrunnionBuiltM, 1e-3));
    CHECK(th >= kTrunnionMinM);
    CHECK(th <= kTrunnionMaxM);
    // the extras agree with the header (one source, read back)
    CHECK_THAT(g.extras.at(kNodeRoot).at("trunnion_h_m"), WithinAbs(th, 1e-6));
    CHECK_THAT(g.extras.at(kNodeCradle).at("elev_min_deg"), WithinAbs(kElevMinRad / kDeg, 1e-9));
    CHECK_THAT(g.extras.at(kNodeCradle).at("elev_max_deg"), WithinAbs(kElevMaxRad / kDeg, 1e-9));
    CHECK_THAT(g.extras.at(kNodeRoot).at("muzzle_speed_mps"), WithinAbs(kMuzzleSpeedMps, 1e-9));
    CHECK_THAT(g.extras.at(kNodeRoot).at("rof_hz"), WithinAbs(kRofHz, 1e-9));
    CHECK_THAT(g.extras.at(kNodeRoot).at("drum_rounds"), WithinAbs(kDrumRounds, 1e-9));
    // muzzle FORWARD = +Z, on the mount's centreline, above the trunnion
    CHECK(w.at("st_muzzle").z > 1.0);
    CHECK_THAT(w.at("st_muzzle").x, WithinAbs(0.0, 1e-6));
    CHECK(w.at("st_muzzle").y > th);
    // the pads are 35.5 in BEHIND the train axis [OP 909]
    CHECK_THAT(w.at("st_pad_l").z, WithinAbs(-kPadBehindAxisM, 1e-3));
    CHECK_THAT(w.at("st_pad_r").z, WithinAbs(-kPadBehindAxisM, 1e-3));
    CHECK_THAT(w.at("st_pad_l").x, WithinAbs(-w.at("st_pad_r").x, 1e-9));
    CHECK_THAT(w.at("st_pad_r").x - w.at("st_pad_l").x, WithinAbs(0.470, 1e-3));  // biacromial
    // the gun overall: muzzle to the receiver rear (= grips - the 40 mm block)
    // is 87 in [OP 911]: pinned through the two stations that bracket it
    CHECK_THAT(w.at("st_muzzle").z - (w.at("st_grip_l").z + 0.04), WithinAbs(kGunOverallM, 2e-3));
    // grips: mirrored, at the breech, BELOW the bore (the man hangs on them)
    CHECK_THAT(w.at("st_grip_l").x, WithinAbs(-w.at("st_grip_r").x, 1e-9));
    CHECK(w.at("st_grip_l").z < w.at("st_pad_l").z + 0.2);
    CHECK(w.at("st_grip_l").y < w.at("st_muzzle").y);
    // the sight: eye BEHIND the rear peep BEHIND the front rings, all on ONE
    // line parallel to the bore (same x, same y) -- the parallax rule §2.6
    const glm::dvec3 e = w.at("st_eye"), sr = w.at("st_sight_rear"), sf = w.at("st_sight_front");
    CHECK(e.z < sr.z);
    CHECK(sr.z < sf.z);
    CHECK_THAT(e.x, WithinAbs(sr.x, 1e-9));
    CHECK_THAT(sr.x, WithinAbs(sf.x, 1e-9));
    CHECK_THAT(e.y, WithinAbs(sr.y, 1e-9));
    CHECK_THAT(sr.y, WithinAbs(sf.y, 1e-9));
    CHECK(sf.x < -0.05);  // outboard LEFT of the drum (the eye-view finding)
    CHECK(w.at("st_drum").x > 0.05);
    // his feet and the approach point are ON THE GROUND (the first export had
    // them 0.943 m up -- the independent parse caught it, this pins it)
    for (const char* n : kTrainStations) {
        INFO(n);
        CHECK_THAT(w.at(n).y, WithinAbs(0.0, 1e-6));
        CHECK(w.at(n).z < w.at("st_pad_l").z);  // behind the pads
    }
    CHECK_THAT(w.at("st_foot_l").x, WithinAbs(-w.at("st_foot_r").x, 1e-9));
    // the eye at 1.783 is within 30 mm of the Sudburian's eye in boots
    // (0.936 H bare = 1.732 + 0.030) -- he leans in, he does not stoop
    CHECK_THAT(e.y, WithinAbs(1.732 + 0.030, 0.03));
    // ★★★ MONO MATERIALS -- WITH EXACTLY ONE EXCEPTION, AND IT IS THE
    // PALETTE'S (Chad, 2026-09-01: "a neon color coded in query for the
    // correct blue as neon and just thiner blue metal ring for the pipper").
    // The gun is built in grey tones because the game reads base_color_factor
    // as a tone. The SIGHT is now the ally blue -- and this leg pins it to
    // render::team_colors(), the ONE cross-layer palette, so the asset cannot
    // drift from the map, the liveries and the HUD pipper that share it. A
    // retyped blue in the generator would pass "is it blue" and fail here,
    // which is the point (CLAUDE.md H1: no second colour table).
    REQUIRE(g.base_color_r.size() >= 6);
    const glm::dvec3 ally = render::team_colors().ally;
    int coloured = 0;
    for (std::size_t i = 0; i < g.base_color_r.size(); ++i) {
        const bool mono =
            std::fabs(g.base_color_r[i] - g.base_color_g[i]) < 1e-6 &&
            std::fabs(g.base_color_r[i] - g.base_color_b[i]) < 1e-6;
        if (mono) {
            CHECK(g.base_color_r[i] > 0.05);
            CHECK(g.base_color_r[i] < 0.95);
            continue;
        }
        ++coloured;
        INFO("coloured material " << i);
        CHECK_THAT(g.base_color_r[i], WithinAbs(ally.r, 1e-6));
        CHECK_THAT(g.base_color_g[i], WithinAbs(ally.g, 1e-6));
        CHECK_THAT(g.base_color_b[i], WithinAbs(ally.b, 1e-6));
    }
    CHECK(coloured == 1);  // the sight, and nothing else
}

// ------------------------------------------------------- B. the kinematics
TEST_CASE("flak kinematics: mount frame is orthonormal and tangent at a generic point",
          "[flak]") {
    const MountFrame f = generic_frame();
    CHECK_THAT(glm::length(f.up), WithinAbs(1.0, 1e-12));
    CHECK_THAT(glm::length(f.fwd0), WithinAbs(1.0, 1e-12));
    CHECK_THAT(glm::length(f.right0), WithinAbs(1.0, 1e-12));
    CHECK_THAT(glm::dot(f.up, f.fwd0), WithinAbs(0.0, 1e-12));
    CHECK_THAT(glm::dot(f.up, f.right0), WithinAbs(0.0, 1e-12));
    CHECK_THAT(glm::dot(f.fwd0, f.right0), WithinAbs(0.0, 1e-12));
    // right-handed: right = up x fwd  (so that fwd x up... cross(fwd0, right0) == up? no:
    // cross(up, fwd) = right  =>  cross(fwd, right) = up)
    const glm::dvec3 u2 = glm::cross(f.fwd0, f.right0);
    CHECK_THAT(glm::dot(u2, f.up), WithinAbs(1.0, 1e-12));
    // a hint parallel to up still yields a tangent fwd0
    const MountFrame g = make_mount_frame(f.pos, f.up, f.up * 3.0);
    CHECK_THAT(glm::dot(g.fwd0, g.up), WithinAbs(0.0, 1e-12));
    CHECK_THAT(glm::length(g.fwd0), WithinAbs(1.0, 1e-12));
}

TEST_CASE("flak kinematics: +train is RIGHT, +elev is UP, and the clamp binds", "[flak]") {
    const MountFrame f = generic_frame();
    Pose p;
    REQUIRE(aim_to_pose(f, f.fwd0, p));
    CHECK_THAT(p.train_rad, WithinAbs(0.0, 1e-12));
    CHECK_THAT(p.elev_rad, WithinAbs(0.0, 1e-12));
    REQUIRE(aim_to_pose(f, f.right0, p));
    CHECK_THAT(p.train_rad, WithinAbs(kPi / 2, 1e-12));
    REQUIRE(aim_to_pose(f, -f.right0, p));
    CHECK_THAT(p.train_rad, WithinAbs(-kPi / 2, 1e-12));
    // 30 deg up along fwd0
    REQUIRE(aim_to_pose(f, f.fwd0 * std::cos(30 * kDeg) + f.up * std::sin(30 * kDeg), p));
    CHECK_THAT(p.elev_rad, WithinAbs(30 * kDeg, 1e-12));
    CHECK_THAT(p.train_rad, WithinAbs(0.0, 1e-12));
    // the dead cone: straight up clamps to 87, not 90; 10 deg down clamps to -5
    REQUIRE(aim_to_pose(f, f.up, p));
    CHECK_THAT(p.elev_rad, WithinAbs(kElevMaxRad, 1e-12));
    REQUIRE(aim_to_pose(f, f.fwd0 * std::cos(-10 * kDeg) + f.up * std::sin(-10 * kDeg), p));
    CHECK_THAT(p.elev_rad, WithinAbs(kElevMinRad, 1e-12));
    // a zero aim is refused and leaves the pose alone
    const Pose keep = p;
    REQUIRE_FALSE(aim_to_pose(f, glm::dvec3(0.0), p));
    CHECK(p.train_rad == keep.train_rad);
    CHECK(p.elev_rad == keep.elev_rad);
    // the bore direction round-trips the pose -- the ONE sign in cradle_quat
    // is what this pins: a flipped sign points the bore DOWN 30 deg here
    Pose q;
    q.train_rad = 40 * kDeg;
    q.elev_rad = 30 * kDeg;
    const glm::dvec3 b = bore_dir_world(f, q);
    CHECK_THAT(glm::dot(b, f.up), WithinAbs(std::sin(30 * kDeg), 1e-12));
    CHECK(glm::dot(b, f.right0) > 0.0);  // swung RIGHT
    Pose back;
    REQUIRE(aim_to_pose(f, b, back));
    CHECK_THAT(back.train_rad, WithinAbs(q.train_rad, 1e-12));
    CHECK_THAT(back.elev_rad, WithinAbs(q.elev_rad, 1e-12));
}

TEST_CASE("flak kinematics: the slew is rate-capped and takes the short way round",
          "[flak]") {
    SlewRates r;
    r.train_rad_s = 90 * kDeg;
    r.elev_rad_s = 60 * kDeg;
    Pose cur;
    cur.train_rad = 170 * kDeg;
    cur.elev_rad = 10 * kDeg;
    Pose tgt;
    tgt.train_rad = -170 * kDeg;   // 20 deg away THROUGH 180, 340 the long way
    tgt.elev_rad = 80 * kDeg;
    const Pose a = slew_toward(cur, tgt, r, 0.1);   // caps: 9 deg train, 6 deg elev
    CHECK_THAT(wrap_pi(a.train_rad - cur.train_rad), WithinAbs(9 * kDeg, 1e-12));
    CHECK_THAT(a.elev_rad - cur.elev_rad, WithinAbs(6 * kDeg, 1e-12));
    // within the cap it ARRIVES exactly (no creep, no overshoot)
    const Pose b = slew_toward(cur, tgt, r, 10.0);
    CHECK_THAT(wrap_pi(b.train_rad - tgt.train_rad), WithinAbs(0.0, 1e-12));
    CHECK_THAT(b.elev_rad, WithinAbs(tgt.elev_rad, 1e-12));
    // a target past the limit is clamped before the slew, so the gun never
    // rests against a demand it can't reach
    tgt.elev_rad = 95 * kDeg;
    const Pose c = slew_toward(cur, tgt, r, 10.0);
    CHECK_THAT(c.elev_rad, WithinAbs(kElevMaxRad, 1e-12));
    // dt <= 0 is a no-op
    const Pose d = slew_toward(cur, tgt, r, 0.0);
    CHECK(d.train_rad == cur.train_rad);
    CHECK(d.elev_rad == cur.elev_rad);
}

TEST_CASE("flak kinematics: stations transform through the pose the way the GLB is built",
          "[flak][asset]") {
    const Glb g = open_glb();
    REQUIRE(g.ok);
    Stations s;
    s.trunnion_h = g.world.at(kNodeCradle).y;
    s.train_h = g.world.at(kNodeTrain).y;
    s.muzzle = g.world.at("st_muzzle");
    s.eye = g.world.at("st_eye");
    s.sight_rear = g.world.at("st_sight_rear");
    s.sight_front = g.world.at("st_sight_front");
    s.foot_l = g.world.at("st_foot_l");
    const MountFrame f = generic_frame();
    // zero pose: the muzzle is fwd0 * z + up * y from the base
    Pose z;
    const glm::dvec3 m0 = cradle_station_world(f, s, z, s.muzzle);
    const glm::dvec3 want = f.pos + f.fwd0 * s.muzzle.z + f.up * s.muzzle.y;
    CHECK_THAT(glm::length(m0 - want), WithinAbs(0.0, 1e-9));
    // train 90: the muzzle swings to RIGHT of the base at the same height
    Pose t;
    t.train_rad = kPi / 2;
    const glm::dvec3 m1 = cradle_station_world(f, s, t, s.muzzle);
    CHECK_THAT(glm::dot(m1 - f.pos, f.right0), WithinAbs(s.muzzle.z, 1e-9));
    CHECK_THAT(glm::dot(m1 - f.pos, f.up), WithinAbs(s.muzzle.y, 1e-9));
    // elev 87: the muzzle rises ABOUT THE TRUNNION -- its height above the
    // trunnion is z*sin(87) + (y-th)*cos(87), and it stays over the axis
    Pose e;
    e.elev_rad = kElevMaxRad;
    const glm::dvec3 m2 = cradle_station_world(f, s, e, s.muzzle);
    const double dz = s.muzzle.z, dy = s.muzzle.y - s.trunnion_h;
    CHECK_THAT(glm::dot(m2 - f.pos, f.up) - s.trunnion_h,
               WithinAbs(dz * std::sin(kElevMaxRad) + dy * std::cos(kElevMaxRad), 1e-9));
    CHECK_THAT(glm::dot(m2 - f.pos, f.fwd0),
               WithinAbs(dz * std::cos(kElevMaxRad) - dy * std::sin(kElevMaxRad), 1e-9));
    // the feet do NOT elevate: same world point at elev 0 and elev 87
    const glm::dvec3 f0 = train_station_world(f, z, s.foot_l);
    const glm::dvec3 f1 = train_station_world(f, e, s.foot_l);
    CHECK_THAT(glm::length(f0 - f1), WithinAbs(0.0, 1e-12));
    CHECK_THAT(glm::dot(f0 - f.pos, f.up), WithinAbs(0.0, 1e-9));  // on the ground
    // the sight camera: eye at st_eye, forward parallel to the bore, up
    // perpendicular to forward, no roll (up has no right0 component at zero
    // train)
    Pose q;
    q.train_rad = 25 * kDeg;
    q.elev_rad = 40 * kDeg;
    const SightCamera c = sight_camera(f, s, q);
    CHECK_THAT(glm::length(c.eye - cradle_station_world(f, s, q, s.eye)), WithinAbs(0.0, 1e-12));
    CHECK_THAT(glm::dot(c.forward, bore_dir_world(f, q)), WithinAbs(1.0, 1e-9));
    CHECK_THAT(glm::dot(c.forward, c.up), WithinAbs(0.0, 1e-9));
    CHECK_THAT(glm::length(c.up), WithinAbs(1.0, 1e-12));
    // no roll: the camera's right (forward x up) is tangent to the sphere
    const glm::dvec3 right = glm::cross(c.forward, c.up);
    CHECK_THAT(glm::dot(right, f.up), WithinAbs(0.0, 1e-9));
}

TEST_CASE("flak F-PLACE: the site is on the threat flank, the ruled offset away",
          "[flak]") {
    // generic pumps -- neither on a world axis (the flat-instrument trap)
    const glm::dvec3 up_p = glm::normalize(glm::dvec3(0.7, 1.1, -0.4));
    const glm::dvec3 up_e = glm::normalize(glm::dvec3(-0.2, 1.3, 0.9));
    const double R = 15000.0;
    glm::dvec3 u_site(0.0), fwd(0.0);
    REQUIRE(flak_site(up_p, up_e, R, kFlankOffsetM, u_site, fwd));
    CHECK_THAT(glm::length(u_site), WithinAbs(1.0, 1e-12));
    // the arc from the pump to the site is EXACTLY the ruled offset
    CHECK_THAT(std::acos(glm::dot(u_site, up_p)) * R,
               WithinAbs(kFlankOffsetM, 1e-6));
    // the site moved TOWARD the enemy: closer in angle than the pump is
    CHECK(glm::dot(u_site, up_e) > glm::dot(up_p, up_e));
    // the site lies ON the pump->enemy great circle (coplanar with both)
    const glm::dvec3 n = glm::normalize(glm::cross(up_p, up_e));
    CHECK_THAT(glm::dot(u_site, n), WithinAbs(0.0, 1e-12));
    // the forward hint faces the enemy and is usable by make_mount_frame
    CHECK(glm::dot(glm::normalize(fwd), glm::normalize(up_e - u_site)) > 0.0);
    const MountFrame f = make_mount_frame(u_site * R, u_site, fwd);
    CHECK_THAT(glm::dot(f.fwd0, f.up), WithinAbs(0.0, 1e-12));
    CHECK(glm::dot(f.fwd0, up_e) > 0.0);  // the gun faces the threat
    // degenerate pairs place NO gun: parallel, antiparallel, zero radius
    CHECK_FALSE(flak_site(up_p, up_p, R, kFlankOffsetM, u_site, fwd));
    CHECK_FALSE(flak_site(up_p, -up_p, R, kFlankOffsetM, u_site, fwd));
    CHECK_FALSE(flak_site(up_p, up_e, 0.0, kFlankOffsetM, u_site, fwd));
}

// ------------------------------------------------------- C. F-FIRE (pure)
#include "app/flak_tick.h"

namespace {
app::FlakWorld manned_world() {
    app::FlakWorld fk;
    const glm::dvec3 up = glm::normalize(glm::dvec3(0.6, 1.0, -0.8));
    fk.mount = make_mount_frame(up * 15000.0, up, glm::dvec3(0.2, 0.4, 0.9));
    fk.gw.battery.convergence_range = 800.0;
    fk.gw.battery.guns.push_back({glm::dvec3(0.0, 1.623, -1.448),
                                  weapon::Round::Cannon20mm, kMuzzleSpeedMps,
                                  kRofHz, 0.0008, 30.0});
    fk.drum_rounds = kDrumRounds;
    fk.rounds_left = kDrumRounds;
    fk.reload_s = 4.0;
    fk.selfdestruct_s = 1.6;
    fk.manned = true;
    fk.fire_held = true;
    return fk;
}
constexpr double kDt = 1.0 / 120.0;
}  // namespace

TEST_CASE("flak F-FIRE: cadence, drum, reload, self-destruct -- headless",
          "[flak]") {
    app::FlakWorld fk = manned_world();
    // UNMANNED fires nothing, and the world stays inert
    fk.manned = false;
    for (int t = 0; t < 60; ++t) REQUIRE(app::flak_tick(fk, false, kDt, 9.81, 15000.0) == 0);
    REQUIRE(fk.gw.pool.empty());
    fk.manned = true;
    // MANNED + held trigger: cyclic 7.5 Hz == one round per 16 ticks at 120 Hz
    int spawned = 0;
    for (int t = 0; t < 120; ++t) spawned += app::flak_tick(fk, false, kDt, 9.81, 15000.0);
    CHECK(spawned >= 7);
    CHECK(spawned <= 9);   // 1 s of 7.5 Hz, +-boundary
    CHECK(fk.rounds_left == kDrumRounds - spawned);
    CHECK(fk.spawned_accum == spawned);
    // a GROUNDED (crash/reset) tick is inert
    const int before = fk.rounds_left;
    REQUIRE(app::flak_tick(fk, true, kDt, 9.81, 15000.0) == 0);
    CHECK(fk.rounds_left == before);
    // run the drum dry: the reload starts EXACTLY when the last round leaves
    int guard = 0;
    while (fk.rounds_left > 0 && ++guard < 30000)
        app::flak_tick(fk, false, kDt, 9.81, 15000.0);
    REQUIRE(fk.rounds_left == 0);
    CHECK(fk.reload_left_s > 0.0);
    // held trigger during the reload fires NOTHING
    int during = 0;
    const int reload_ticks = static_cast<int>(fk.reload_s / kDt) + 2;
    for (int t = 0; t < reload_ticks; ++t)
        during += app::flak_tick(fk, false, kDt, 9.81, 15000.0);
    // ...until the drum swaps back in and firing resumes at full count
    CHECK(fk.rounds_left >= kDrumRounds - during - 1);
    CHECK(fk.rounds_left <= kDrumRounds);
    // self-destruct: NO live round older than selfdestruct_s, ever
    double oldest = 0.0;
    for (const weapon::Projectile& p : fk.gw.pool)
        if (p.active) oldest = std::max(oldest, p.age);
    CHECK(oldest <= fk.selfdestruct_s + kDt);
    // and with selfdestruct 0 the retire falls back to kMaxFlightTime -- a
    // round is allowed to live past 1.6 s (the off-arm)
    app::FlakWorld fk2 = manned_world();
    fk2.selfdestruct_s = 0.0;
    bool saw_old = false;
    for (int t = 0; t < 3 * 120; ++t) {
        app::flak_tick(fk2, false, kDt, 9.81, 15000.0);
        for (const weapon::Projectile& p : fk2.gw.pool)
            if (p.active && p.age > 1.7) saw_old = true;
    }
    CHECK(saw_old);
}

TEST_CASE("flak F-FIRE: the synthetic shooter's nose IS the bore", "[flak]") {
    app::FlakWorld fk = manned_world();
    fk.pose.train_rad = 35.0 * kDeg;
    fk.pose.elev_rad = 25.0 * kDeg;
    fk.demand = fk.pose;
    app::flak_rebuild_shooter(fk);
    // body -Z (the nose, SPEC §7) == bore_dir_world, exactly
    const glm::dvec3 nose =
        fk.shooter.orientation * glm::dvec3(0.0, 0.0, -1.0);
    const glm::dvec3 bore = bore_dir_world(fk.mount, fk.pose);
    CHECK_THAT(glm::dot(glm::normalize(nose), bore), WithinAbs(1.0, 1e-12));
    // zero velocity: a gun inherits nothing
    CHECK(glm::length(fk.shooter.velocity) == 0.0);
    // a spawned round leaves ON the bore at muzzle speed, from the MUZZLE
    // station (position + orientation * muzzle_body), not the pedestal base
    app::flak_tick(fk, false, kDt, 9.81, 15000.0);
    REQUIRE_FALSE(fk.gw.pool.empty());
    const weapon::Projectile& p = fk.gw.pool.front();
    REQUIRE(p.active);
    // The spawn dir deviates from the bore by DESIGN: the muzzle toe-in to
    // the 800 m convergence point (~2 mrad -- the muzzle sits 1.6 m off the
    // boresight line) PLUS weapon::harmonization_rise's gravity hold-over
    // (~0.5*g*(conv/v)^2 / conv ~ 5.6 mrad) -- measured 7.1 mrad combined.
    // The bound admits the harmonisation, not a sign/axis flip (a wrong
    // cradle sign is ~0.87 rad here).
    CHECK_THAT(glm::dot(glm::normalize(p.vel), bore), WithinAbs(1.0, 8e-5));
    CHECK_THAT(glm::length(p.vel), WithinAbs(kMuzzleSpeedMps, 1e-6));
    const glm::dvec3 base_to_spawn = p.prev_pos - fk.mount.pos;
    CHECK(glm::length(base_to_spawn) > 1.0);   // NOT the pedestal base
    // height along local up ~ trunnion + bore rise at 25 deg elevation
    CHECK(glm::dot(base_to_spawn, fk.mount.up) > 1.0);
}

// ★★★ THE BELL, NOT THE BREECH (Chad, 2026-08-31: "the tracer appears to
// come out of the sudburians view, rather than the end of the barrel"). The
// muzzle is a CRADLE station: elevation swings it about the TRUNNION. A
// muzzle_body measured once at zero elevation and then carried by the
// shooter's orientation rotates it about the mount ORIGIN instead, which at
// +45 deg leaves the round a metre behind and half a metre under the bell --
// out of the breech, beside the gunner's own eye. This leg grades the spawn
// against the SAME cradle_station_world() the muzzle flash is drawn through,
// and -- so it cannot pass by accident -- it also asserts the two placements
// genuinely DIFFER off the horizontal, which is the whole defect.
TEST_CASE("flak F-FIRE: the round leaves the BELL at every elevation",
          "[flak][asset]") {
    const Glb glb = open_glb();
    REQUIRE(glb.ok);
    Stations st;
    st.trunnion_h = glb.world.at(kNodeCradle).y;
    st.train_h = glb.world.at(kNodeTrain).y;
    st.muzzle = glb.world.at("st_muzzle");
    st.eye = glb.world.at("st_eye");
    const double kElevDeg[] = {0.0, 20.0, 45.0, 70.0, 87.0};
    for (const double deg : kElevDeg) {
        INFO("elev " << deg);
        app::FlakWorld fk = manned_world();
        fk.stations = st;
        fk.pose.train_rad = 35.0 * kDeg;
        fk.pose.elev_rad = deg * kDeg;
        fk.demand = fk.pose;
        REQUIRE(app::flak_tick(fk, false, kDt, 9.81, 15000.0) > 0);
        REQUIRE_FALSE(fk.gw.pool.empty());
        const weapon::Projectile& p = fk.gw.pool.front();
        REQUIRE(p.active);
        // the spawn IS the drawn bell -- one kinematic source
        const glm::dvec3 bell =
            cradle_station_world(fk.mount, st, fk.pose, st.muzzle);
        CHECK_THAT(glm::length(p.prev_pos - bell), WithinAbs(0.0, 1e-9));
        // ...and it is never within arm's reach of the man at the sight
        const glm::dvec3 eye_w =
            cradle_station_world(fk.mount, st, fk.pose, st.eye);
        CHECK(glm::length(p.prev_pos - eye_w) > 1.0);
        // THE DISCRIMINATOR: the old about-the-origin offset is a different
        // point once the gun is off the horizontal, so a revert goes red.
        const glm::dvec3 origin_pivot =
            fk.shooter.position +
            fk.shooter.orientation *
                glm::dvec3(st.muzzle.x, st.muzzle.y, -st.muzzle.z);
        if (deg <= 1e-9)
            CHECK_THAT(glm::length(origin_pivot - bell), WithinAbs(0.0, 1e-9));
        else
            CHECK(glm::length(origin_pivot - bell) > 0.3);
    }
}

// -------------------------------------------- C2. THE TRACER STREAK (pure)
//
// ★★★ A TRACER CANNOT START BEFORE THE GUN FIRED IT (Chad, 2026-08-31: "an
// opposite direction trace is blowing out backwards ... an equal and opposite
// tracer in both directions"). The comet is a fixed-length streak drawn back
// along -velocity, and a fresh 835 m/s round has flown only ~7 m on its first
// frame -- so ~22 m of the flak's 28.6 m glow tail used to be drawn BEHIND the
// muzzle, out through the breech and the gunner's own face. On an aeroplane
// that overhang lands behind the camera and is clipped unseen; the ground gun
// is where the eye sits at the breech and it reads as a second tracer.
#include "combat/fx_curves.h"

TEST_CASE("tracer streak never outruns the round that made it", "[flak]") {
    using combat::tracer_tail_m;
    // the spawn frame has no trail at all
    CHECK(tracer_tail_m(28.6, kMuzzleSpeedMps, 0.0) == 0.0);
    // THE CASE THAT BIT: one 120 Hz tick after the muzzle, the 28.6 m glow
    // tail is cut to the 6.96 m the round has actually flown -- so nothing is
    // drawn behind the bell, where the gunner is.
    const double dt = 1.0 / 120.0;
    CHECK_THAT(tracer_tail_m(28.6, kMuzzleSpeedMps, dt),
               WithinAbs(kMuzzleSpeedMps * dt, 1e-12));
    CHECK(tracer_tail_m(28.6, kMuzzleSpeedMps, dt) < 7.0);
    // once it is well downrange the streak is the full authored length again
    CHECK_THAT(tracer_tail_m(28.6, kMuzzleSpeedMps, 1.0), WithinAbs(28.6, 1e-12));
    // properties for ANY round: never longer than either bound, never
    // negative, monotone in age
    double prev = -1.0;
    for (int i = 0; i <= 40; ++i) {
        const double age = i * dt;
        const double v = tracer_tail_m(16.8, kMuzzleSpeedMps, age);
        INFO("age " << age);
        CHECK(v >= 0.0);
        CHECK(v <= 16.8 + 1e-12);
        CHECK(v <= kMuzzleSpeedMps * age + 1e-12);
        CHECK(v >= prev);
        prev = v;
    }
    // degenerate inputs cannot produce a streak pointing the wrong way
    CHECK(tracer_tail_m(0.0, kMuzzleSpeedMps, 1.0) == 0.0);
    CHECK(tracer_tail_m(-5.0, kMuzzleSpeedMps, 1.0) == 0.0);
    CHECK(tracer_tail_m(28.6, 0.0, 1.0) == 0.0);
    CHECK(tracer_tail_m(28.6, kMuzzleSpeedMps, -1.0) == 0.0);
}

// ------------------------------------ D. PROXIMITY BURST + PUFF FX (pure)
//
// The two halves of the flak READ (docs/FLAK_GUN_SPEC.md §2.2 item 5): the
// shell bursts NEAR the aeroplane, and every round that dies visibly leaves
// smoke. The three cases below pin, in order: the self-destruct burst event
// the pure tick books, the proximity widening actually reaching a drone the
// bare radius misses, and -- the one that matters most -- that the DEFAULT
// (radius add 0, burst_fx off) leaves the aircraft gun's hit test EXACTLY
// where it was. That last one is the whole argument for a defaulted parameter
// instead of a copied loop.
#include "combat/fx_curves.h"
#include "combat/kill.h"
#include "config/load_aircraft.h"
#include "drone/drone.h"

namespace {

const sim::AircraftParams kFlakAp =
    cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");

drone::DroneParams flak_dp() {
    drone::DroneParams dp;
    dp.hp = 100.0;
    dp.hit_radius_m = 15.0;
    return dp;
}

combat::CombatWorld flak_cw() {
    combat::CombatWorld cw;
    cw.params.hit_radius_m = 15.0;
    return cw;
}

// A drone parked at pos, stationary (prev == curr) -- the test_combat helper.
drone::DroneState flak_target(const glm::dvec3& pos,
                              const drone::DroneParams& dp) {
    drone::DroneState d;
    sim::SimState s;
    s.position = pos;
    s.velocity = glm::dvec3{0.0};
    s.orientation = glm::dquat{1, 0, 0, 0};
    d.curr = s;
    d.prev = s;
    d.grounded = false;
    d.hp = dp.hp;
    return d;
}

// One live round swept from prev_pos to pos this tick.
weapon::Projectile flak_round(const glm::dvec3& prev_pos,
                              const glm::dvec3& pos) {
    weapon::Projectile p;
    p.prev_pos = prev_pos;
    p.pos = pos;
    p.vel = pos - prev_pos;
    p.age = 0.01;
    p.drag_k = 0.0;
    p.damage = 30.0;
    p.v_ref = glm::length(p.vel);  // normal-incidence center hit == damage
    p.kind = weapon::Round::Cannon20mm;
    p.active = true;
    return p;
}

int active_fx_of(const combat::CombatWorld& cw, combat::FxKind k) {
    int n = 0;
    for (const combat::Fx& f : cw.fx.pool)
        if (f.active && f.kind == k) ++n;
    return n;
}

}  // namespace

TEST_CASE("flak burst: a self-destructed round books exactly one event at its "
          "last position",
          "[flak]") {
    app::FlakWorld fk = manned_world();
    // One round only: fire a single tick, then let the trigger up so the pool
    // never gains a second. The event count must then be exactly one, and its
    // position must be the round's own last position -- not the muzzle, and
    // not prev_pos (a whole tick, ~7 m at 835 m/s, behind the streak the eye
    // is actually tracking).
    REQUIRE(app::flak_tick(fk, false, kDt, 9.81, 15000.0) == 1);
    fk.fire_held = false;
    REQUIRE(fk.bursts.empty());  // a fresh round bursts nothing

    glm::dvec3 last_live(0.0);
    int fired_at = -1;
    for (int t = 0; t < 400 && fk.bursts.empty(); ++t) {
        // snapshot the live round BEFORE the tick that will retire it
        for (const weapon::Projectile& p : fk.gw.pool)
            if (p.active) last_live = p.pos;
        app::flak_tick(fk, false, kDt, 9.81, 15000.0);
        if (!fk.bursts.empty()) fired_at = t;
    }
    REQUIRE(fk.bursts.size() == 1u);
    CHECK(fk.bursts[0].cause == app::FlakBurstCause::SelfDestruct);
    // it burst at the END of the retiring tick, one dt past the snapshot --
    // the event sits AHEAD of the last snapshot, within one tick of flight
    const double step = glm::length(fk.bursts[0].pos - last_live);
    CHECK(step > 0.0);
    CHECK(step < 1.2 * kMuzzleSpeedMps * kDt);
    // and it burst at the self-destruct AGE, not at some max-flight-time
    CHECK(fired_at >= 0);
    const double t_burst = static_cast<double>(fired_at + 2) * kDt;
    CHECK_THAT(t_burst, WithinAbs(fk.selfdestruct_s, 3.0 * kDt));
    // ~870 m downrange (the spec's flak-curtain range), not at the gun
    CHECK(glm::length(fk.bursts[0].pos - fk.mount.pos) > 700.0);
    // the list is the CALLER's to drain; nothing in the pure tick clears it
    CHECK(fk.bursts.size() == 1u);
    // and with the self-destruct dial OFF, no round ever books a burst
    app::FlakWorld fk2 = manned_world();
    fk2.selfdestruct_s = 0.0;
    for (int t = 0; t < 3 * 120; ++t)
        app::flak_tick(fk2, false, kDt, 9.81, 15000.0);
    CHECK(fk2.bursts.empty());
}

TEST_CASE("flak burst: a round passing inside the prox radius retires and "
          "damages the aircraft",
          "[flak]") {
    const drone::DroneParams dp = flak_dp();
    // The drone is 2 km up the +X axis; the round sweeps PAST it at a
    // perpendicular miss distance of 17 m -- outside the 15 m body radius,
    // inside 15 + 4. A bare 20 mm shell goes by; a fuzed one bursts.
    const glm::dvec3 tgt{6371000.0 + 2000.0, 0.0, 0.0};
    const glm::dvec3 miss = glm::dvec3{0.0, 17.0, 0.0};
    const glm::dvec3 a = tgt + miss + glm::dvec3{0.0, 0.0, 40.0};
    const glm::dvec3 b = tgt + miss - glm::dvec3{0.0, 0.0, 40.0};

    // ARM ON: fuze radius 4 m, burst FX on -- the flak call site.
    std::vector<drone::DroneState> drones{flak_target(tgt, dp)};
    std::vector<weapon::Projectile> pool{flak_round(a, b)};
    combat::CombatWorld cw = flak_cw();
    combat::combat_tick(pool, drones, cw, kFlakAp, dp, kFlakAp.sim_dt, 4.0,
                        /*burst_fx=*/true);
    CHECK_FALSE(pool[0].active);  // the shell burst; it does not fly on
    CHECK(drones[0].hp < dp.hp);  // HE damage, not a graze for zero
    CHECK(drones[0].took_fire);
    CHECK(cw.hits == 1);
    // the FX is a PUFF, and there is no metal spark -- the shell never
    // touched the aeroplane
    CHECK(active_fx_of(cw, combat::FxKind::FlakPuff) == 1);
    CHECK(active_fx_of(cw, combat::FxKind::HitSpark) == 0);

    // ARM OFF at the SAME geometry: 17 m is a clean miss and nothing at all
    // happens. This is the liveness half -- without it the case above could
    // pass on a hit test that had always been wide enough.
    std::vector<drone::DroneState> d2{flak_target(tgt, dp)};
    std::vector<weapon::Projectile> p2{flak_round(a, b)};
    combat::CombatWorld cw2 = flak_cw();
    combat::combat_tick(p2, d2, cw2, kFlakAp, dp, kFlakAp.sim_dt, 0.0, true);
    CHECK(p2[0].active);
    CHECK(d2[0].hp == dp.hp);
    CHECK(cw2.hits == 0);
    CHECK(active_fx_of(cw2, combat::FxKind::FlakPuff) == 0);

    // A DIRECT hit with the fuze dialed to ZERO still bursts: burst_fx is its
    // own flag, never read off prox_radius_add_m > 0. (The trap this rung has
    // paid three times -- a binary read of a continuous feel dial. Dial the
    // fuze off and you get the bare 20 mm; you do NOT get rounds that
    // silently stop making smoke.)
    std::vector<drone::DroneState> d3{flak_target(tgt, dp)};
    std::vector<weapon::Projectile> p3{
        flak_round(tgt + glm::dvec3{0.0, 0.0, 40.0},
                   tgt - glm::dvec3{0.0, 0.0, 40.0})};
    combat::CombatWorld cw3 = flak_cw();
    combat::combat_tick(p3, d3, cw3, kFlakAp, dp, kFlakAp.sim_dt, 0.0, true);
    CHECK_FALSE(p3[0].active);
    CHECK(active_fx_of(cw3, combat::FxKind::FlakPuff) == 1);
    CHECK(active_fx_of(cw3, combat::FxKind::HitSpark) == 0);
    // the puff outlives the spark it replaced, and dies at EXACTLY zero alpha
    // (an FX that retires bright pops)
    CHECK(combat::kFlakPuffLifetime > combat::kSparkLifetime);
    CHECK(combat::flak_puff_state(combat::kFlakPuffLifetime, 1.0).smoke_alpha ==
          0.0);
    CHECK(combat::flak_puff_state(0.02, 1.0).core_brightness > 0.0);
    CHECK(combat::flak_puff_state(0.5, 1.0).core_brightness == 0.0);
    CHECK(combat::flak_puff_state(0.5, 1.0).smoke_alpha > 0.0);
}

TEST_CASE("flak DAMAGING-HIT blast variant: bigger/longer flash + bigger "
          "puff; the curtain arm is bit-identical",
          "[flak]") {
    // Chad 2026-08-29 verdict round 2, ask 3 ("increase the blast for hits
    // of the proximity fused flak rounds"). VISUAL ONLY: the discrimination
    // is Fx::variant == kFlakPuffVariantHit, set by combat_tick at a
    // damaging burst (pinned in test_flak_ai.cpp); this leg pins the CURVE.
    using combat::flak_puff_state;
    constexpr int kHit = combat::kFlakPuffVariantHit;

    // 1) The defaulted arm IS the explicit variant-0 arm, bit-identical --
    //    the curtain / self-destruct puff cannot have moved.
    for (const double a : {0.02, 0.08, 0.14, 0.5, 1.2}) {
        const combat::FlakPuffState d0 = flak_puff_state(a, 1.0);
        const combat::FlakPuffState v0 = flak_puff_state(a, 1.0, 0);
        CHECK(d0.core_brightness == v0.core_brightness);
        CHECK(d0.core_radius_m == v0.core_radius_m);
        CHECK(d0.smoke_radius_m == v0.smoke_radius_m);
        CHECK(d0.smoke_alpha == v0.smoke_alpha);
    }

    // 2) The hit flash OUTLIVES the curtain flash: at 0.14 s the ordinary
    //    flash (0.10 s life) is exactly dark while the hit flash still burns.
    CHECK(flak_puff_state(0.14, 1.0).core_brightness == 0.0);
    CHECK(flak_puff_state(0.14, 1.0, kHit).core_brightness > 0.0);
    // ...and it still dies to a hard cutoff before the smoke does.
    CHECK(flak_puff_state(0.20, 1.0, kHit).core_brightness == 0.0);

    // 3) Bigger and brighter while both are alive (night legibility rides
    //    the ADDITIVE core -- emissive light cannot vanish on the night sky
    //    the way soot black did, handoff §9 stage A).
    const combat::FlakPuffState m = flak_puff_state(0.05, 1.0);
    const combat::FlakPuffState h = flak_puff_state(0.05, 1.0, kHit);
    CHECK(h.core_radius_m > 1.5 * m.core_radius_m);
    CHECK(h.core_brightness > m.core_brightness);

    // 4) The smoke ball is ~1.6x, denser but NEVER opaque, and still
    //    retires at exactly zero alpha (the pop discipline).
    const combat::FlakPuffState ms = flak_puff_state(0.6, 1.0);
    const combat::FlakPuffState hs = flak_puff_state(0.6, 1.0, kHit);
    CHECK(hs.smoke_radius_m > 1.5 * ms.smoke_radius_m);
    CHECK(hs.smoke_alpha > ms.smoke_alpha);
    CHECK(hs.smoke_alpha <= combat::kFlakHitSmokeAlphaCap);
    CHECK(flak_puff_state(combat::kFlakPuffLifetime, 1.0, kHit).smoke_alpha ==
          0.0);

    // 5) The hit ENERGY FLOOR: a ~9 HP prox graze arrives at energy01 = 0.3
    //    (dmg/kFxRefDamage), which used to render the reward burst as a runt
    //    of the curtain. A damaging hit draws at full energy -- while the
    //    curtain arm still scales with energy as before.
    const combat::FlakPuffState hlow = flak_puff_state(0.05, 0.3, kHit);
    const combat::FlakPuffState hfull = flak_puff_state(0.05, 1.0, kHit);
    CHECK(hlow.core_radius_m == hfull.core_radius_m);
    CHECK(hlow.smoke_radius_m == hfull.smoke_radius_m);
    CHECK(flak_puff_state(0.05, 0.3).core_radius_m <
          flak_puff_state(0.05, 1.0).core_radius_m);
}

TEST_CASE("flak burst: the DEFAULTED combat_tick arms leave the aircraft gun "
          "hit test unchanged",
          "[flak]") {
    // THE PIN. Every pre-existing combat_tick caller -- above all the
    // aircraft gun at instructor_tick.h -- passes six arguments. If the two
    // defaults were ever anything but "radius add 0, spark not puff", that
    // call site would change behaviour with no edit to it. So: run the
    // identical geometry through the 6-arg call and through an explicit
    // 8-arg no-op call, and require the two to agree on BOTH sides of the
    // radius boundary.
    const drone::DroneParams dp = flak_dp();
    const glm::dvec3 tgt{6371000.0 + 2000.0, 0.0, 0.0};

    for (const double miss_m : {0.0, 14.0, 16.0, 17.0, 18.5, 25.0}) {
        const glm::dvec3 off{0.0, miss_m, 0.0};
        const glm::dvec3 a = tgt + off + glm::dvec3{0.0, 0.0, 40.0};
        const glm::dvec3 b = tgt + off - glm::dvec3{0.0, 0.0, 40.0};

        std::vector<drone::DroneState> dA{flak_target(tgt, dp)};
        std::vector<weapon::Projectile> pA{flak_round(a, b)};
        combat::CombatWorld cwA = flak_cw();
        combat::combat_tick(pA, dA, cwA, kFlakAp, dp, kFlakAp.sim_dt);

        std::vector<drone::DroneState> dB{flak_target(tgt, dp)};
        std::vector<weapon::Projectile> pB{flak_round(a, b)};
        combat::CombatWorld cwB = flak_cw();
        combat::combat_tick(pB, dB, cwB, kFlakAp, dp, kFlakAp.sim_dt, 0.0,
                            /*burst_fx=*/false);

        CHECK(pA[0].active == pB[0].active);
        CHECK(dA[0].hp == dB[0].hp);
        CHECK(cwA.hits == cwB.hits);
        CHECK(active_fx_of(cwA, combat::FxKind::HitSpark) ==
              active_fx_of(cwB, combat::FxKind::HitSpark));
        // the default NEVER makes a puff
        CHECK(active_fx_of(cwA, combat::FxKind::FlakPuff) == 0);
        // ...and the boundary really is the bare 15 m body radius: 14 m in,
        // 16 m out. A default that had silently widened would fail HERE.
        if (miss_m < 15.0) {
            CHECK_FALSE(pA[0].active);
            CHECK(active_fx_of(cwA, combat::FxKind::HitSpark) == 1);
        } else {
            CHECK(pA[0].active);
            CHECK(active_fx_of(cwA, combat::FxKind::HitSpark) == 0);
        }
    }
}

// ============================================================================
// STAGE B -- the immersion cosmetics (muzzle flash, camera shake, snow blast).
// Only the CLOSED-FORM SHAPES are gateable headlessly; the geometry they turn
// into is certified by screenshot (the R2.3 rule). What these cases pin is the
// part a later dial-turn can break silently: that a fresh envelope is exactly
// zero (the bit-identical off-arm and the signed at-rest sight picture), that
// each one is shot-driven and not clock-driven, that the saturation at the
// real 7.5 Hz cyclic is bounded, and that the shake stays sub-milliradian.
// ============================================================================

TEST_CASE("flak stage B: an unfired envelope is EXACTLY zero and stays there",
          "[flak]") {
    // The off-arm. Nothing has fired, so every envelope must be bit-zero for
    // any dt -- this is what makes the at-rest sight picture bit-identical
    // and draws no flash, no cloud.
    for (const double dt : {0.0, 1.0 / 285.0, 0.016, 0.5, 10.0}) {
        CHECK(render::flak::flash_step(0.0, dt, 0) == 0.0);
        CHECK(render::flak::shake_step(0.0, dt, 0) == 0.0);
        CHECK(render::flak::blast_step(0.0, dt, 0) == 0.0);
    }
    // ...and a zero envelope makes no state at all.
    CHECK(render::flak::flash_state(0.0).brightness == 0.0);
    CHECK(render::flak::flash_state(0.0).cone_len_m == 0.0);
    CHECK(render::flak::blast_state(0.0, 0.0).alpha == 0.0);
    const render::flak::ShakeSample s0 = render::flak::shake_sample(7, 0.0);
    CHECK(s0.yaw_rad == 0.0);
    CHECK(s0.pitch_rad == 0.0);
    CHECK(s0.right_m == 0.0);
    CHECK(s0.up_m == 0.0);
    CHECK(s0.back_m == 0.0);
}

TEST_CASE("flak stage B: every envelope is SHOT driven, exponential, capped",
          "[flak]") {
    using namespace render::flak;
    struct Arm {
        double (*step)(double, double, int);
        double tau, per_shot, ceiling;
    };
    const Arm arms[3] = {{&flash_step, kFlashTauS, kFlashPerShot, kFlashCeil},
                         {&shake_step, kShakeTauS, kShakePerShot, kShakeCeil},
                         {&blast_step, kBlastTauS, kBlastPerShot, kBlastCeil}};
    for (const Arm& a : arms) {
        // A shot ADDS exactly per_shot at dt = 0.
        CHECK_THAT(a.step(0.0, 0.0, 1),
                   Catch::Matchers::WithinAbs(a.per_shot, 1e-12));
        // Two rounds in one frame count TWICE (the accumulator is a count,
        // not a flag -- at 285 fps that is rare, at a frame hitch it is not),
        // up to the ceiling, which on the shipped dials binds first.
        CHECK_THAT(a.step(0.0, 0.0, 2),
                   Catch::Matchers::WithinAbs(
                       std::min(2.0 * a.per_shot, a.ceiling), 1e-12));
        // One time constant with no shot leaves 1/e.
        CHECK_THAT(a.step(1.0, a.tau, 0),
                   Catch::Matchers::WithinAbs(std::exp(-1.0), 1e-12));
        // The ceiling binds and cannot be walked past.
        double v = 0.0;
        for (int i = 0; i < 200; ++i) v = a.step(v, 0.0, 1);
        CHECK(v == a.ceiling);
        // Held fire at the REAL cyclic (7.5 Hz, kRofHz) settles BELOW the
        // ceiling or exactly at it -- never above, and never runaway.
        double h = 0.0;
        const double shot_dt = 1.0 / kRofHz;
        for (int i = 0; i < 400; ++i) h = a.step(h, shot_dt, 1);
        CHECK(h <= a.ceiling);
        CHECK(h > 0.0);
        // And a stopped gun decays monotonically to (numerically) nothing.
        // Twelve time constants of silence -- long enough for the SLOWEST
        // arm (the blast, tau 0.9 s), not just the flash.
        double d = h;
        const int quiet_frames = static_cast<int>(12.0 * a.tau * 285.0) + 1;
        for (int i = 0; i < quiet_frames; ++i) {
            const double prev = d;
            d = a.step(d, 1.0 / 285.0, 0);
            CHECK(d < prev);
        }
        CHECK(d < 1e-3);
    }
}

TEST_CASE("flak stage B: the muzzle flash is a 40 ms envelope, not a strobe",
          "[flak]") {
    using namespace render::flak;
    // The whole reason the flash is an envelope: at 285 fps a one-frame pop
    // is lit for 3.5 ms in every 133. Require the flash to still be visibly
    // alive several frames after the shot, and dead well before the next
    // round arrives at the cyclic -- so the gun HAMMERS instead of glowing.
    const double frame = 1.0 / 285.0;
    double v = flash_step(0.0, 0.0, 1);
    int lit_frames = 0;
    double t = 0.0;
    while (t < 1.0 / kRofHz) {
        if (flash_state(v).brightness > 0.15) ++lit_frames;
        v = flash_step(v, frame, 0);
        t += frame;
    }
    CHECK(lit_frames >= 8);   // not a strobe
    CHECK(lit_frames <= 60);  // and not a continuous lamp between rounds
    CHECK(flash_state(v).brightness < 0.15);

    // The state is monotone in the envelope: brighter, bigger, longer cone.
    render::flak::FlashState prev = flash_state(0.05);
    for (double u = 0.10; u <= 1.0; u += 0.05) {
        const render::flak::FlashState f = flash_state(u);
        CHECK(f.brightness > prev.brightness);
        CHECK(f.star_radius_m > prev.star_radius_m);
        CHECK(f.cone_len_m > prev.cone_len_m);
        prev = f;
    }
    // Fresh is white-hot, the tail is orange: green falls as it cools, and
    // the flash is never blue.
    CHECK(flash_state(1.0).color.g > flash_state(0.2).color.g);
    CHECK(flash_state(1.0).color.r >= flash_state(1.0).color.b);
    // Saturated fire cannot blow the geometry up past the barrel's own scale
    // (the gun is 2.21 m overall).
    const render::flak::FlashState sat = flash_state(kFlashCeil);
    CHECK(sat.star_radius_m < 1.0);
    CHECK(sat.cone_len_m < kGunOverallM);
}

TEST_CASE("flak stage B: the shake is deterministic and stays sub milliradian",
          "[flak]") {
    using namespace render::flak;
    // DETERMINISM: the same shot index always jolts the same way. This is
    // what makes a smoke run reproduce and a frame hitch harmless -- there is
    // no clock anywhere in the sample.
    for (int i = 0; i < 64; ++i) {
        const ShakeSample a = shake_sample(i, 1.0);
        const ShakeSample b = shake_sample(i, 1.0);
        CHECK(a.yaw_rad == b.yaw_rad);
        CHECK(a.pitch_rad == b.pitch_rad);
        CHECK(a.right_m == b.right_m);
    }
    // ...and consecutive shots do NOT jolt the same way (a constant offset
    // would be a lens shift, not a shake).
    int differing = 0;
    for (int i = 0; i < 64; ++i)
        if (shake_sample(i, 1.0).yaw_rad != shake_sample(i + 1, 1.0).yaw_rad)
            ++differing;
    CHECK(differing == 64);

    // THE MAGNITUDE BOUND. The sight picture must SHIVER, not swim: the
    // pipper it frames is the thing he shoots with, so a shake that grew to
    // degrees would break the rung below this one. At full saturation the
    // angular jolt stays under 1.5 mrad and the eye travel under 1 cm.
    double max_ang = 0.0, max_tr = 0.0;
    for (int i = 0; i < 512; ++i) {
        const ShakeSample s = shake_sample(i, kShakeCeil);
        max_ang = std::max(max_ang, std::fabs(s.yaw_rad));
        max_ang = std::max(max_ang, std::fabs(s.pitch_rad));
        max_tr = std::max(max_tr, std::fabs(s.right_m));
        max_tr = std::max(max_tr, std::fabs(s.up_m));
        max_tr = std::max(max_tr, std::fabs(s.back_m));
    }
    CHECK(max_ang > 0.0);
    CHECK(max_ang < 1.5e-3);
    CHECK(max_tr < 0.010);
    // The gun CLIMBS under recoil: the pitch jolt is biased positive and
    // never dives. A symmetric jitter would read as noise, not as a weapon.
    for (int i = 0; i < 512; ++i) CHECK(shake_sample(i, 1.0).pitch_rad > 0.0);
    // Linear in the envelope, so a decaying envelope is a decaying jolt.
    CHECK_THAT(shake_sample(11, 0.5).yaw_rad,
               Catch::Matchers::WithinAbs(0.5 * shake_sample(11, 1.0).yaw_rad,
                                          1e-15));
}

TEST_CASE("flak stage B: the snow blast builds on fire, dies after it, and "
          "weakens with elevation",
          "[flak]") {
    using namespace render::flak;
    const double shot_dt = 1.0 / kRofHz;
    // BUILD: about a second of sustained fire brings it up.
    double v = 0.0;
    for (int i = 0; i < static_cast<int>(kRofHz); ++i) v = blast_step(v, shot_dt, 1);
    CHECK(blast_state(v, 0.0).alpha > 0.5 * kBlastPeakAlpha);
    // DISSIPATE: about two seconds after the last round it is gone from the
    // eye (well under a tenth of the peak alpha).
    double d = kBlastCeil;
    const double frame = 1.0 / 285.0;
    for (int i = 0; i < static_cast<int>(2.0 * 285.0); ++i)
        d = blast_step(d, frame, 0);
    // ~11% of peak at 2.0 s on the shipped tau -- gone to the eye over
    // snow, and the tail keeps falling rather than switching off.
    CHECK(blast_state(d, 0.0).alpha < 0.15 * kBlastPeakAlpha);
    for (int i = 0; i < 285; ++i) d = blast_step(d, frame, 0);
    CHECK(blast_state(d, 0.0).alpha < 0.06 * kBlastPeakAlpha);

    // ELEVATION: unchanged to the knee, then monotone DOWN to the stop --
    // never zero (a hard cut-off would pop mid-burst), and never inverted.
    CHECK(blast_elev_gain(-kElevMinRad) == 1.0);
    CHECK(blast_elev_gain(0.0) == 1.0);
    CHECK(blast_elev_gain(kBlastElevKneeRad) == 1.0);
    double prev = 1.0;
    for (double e = kBlastElevKneeRad; e <= kElevMaxRad + 1e-9; e += kDeg) {
        const double g = blast_elev_gain(e);
        CHECK(g <= prev + 1e-12);
        CHECK(g > 0.0);
        prev = g;
    }
    CHECK_THAT(blast_elev_gain(kElevMaxRad),
               Catch::Matchers::WithinAbs(kBlastHighElevGain, 1e-12));
    // A full cloud at the elevation stop is weaker than the same cloud level.
    CHECK(blast_state(1.0, kElevMaxRad).alpha <
          0.5 * blast_state(1.0, 0.0).alpha);

    // The cloud is LOW AND WIDE -- it hugs the pad. If a dial ever made it
    // taller than it is broad it has stopped being a ground blast.
    const render::flak::BlastState full = blast_state(1.0, 0.0);
    CHECK(full.spread_m > 2.0 * full.height_m);
    // ...and it sits in the 3-6 m band ahead of the mount, outside the 10 ft
    // working circle's radius so it never engulfs the gunner.
    CHECK(full.dist_m >= 3.0);
    CHECK(full.dist_m <= 6.0);
    CHECK(full.dist_m > 3.048 / 2.0);
}

TEST_CASE("flak stage B: the blast anchor is on the PAD and swings with TRAIN "
          "only",
          "[flak]") {
    // The ruled choice, pinned: the cloud is placed with train_station_world,
    // so it rides the traverse, sits on the ground (y == 0 in model terms)
    // and does NOT climb with elevation. A future change that carried it
    // through the cradle would fail here.
    const glm::dvec3 up = glm::normalize(glm::dvec3(1.0, 1.0, 1.0));
    const glm::dvec3 pos = up * 6371000.0;
    const glm::dvec3 fwd(0.0, 0.0, 1.0);
    const render::flak::MountFrame f =
        render::flak::make_mount_frame(pos, up, fwd);
    const render::flak::BlastState bs = render::flak::blast_state(1.0, 0.0);
    const glm::dvec3 ahead(0.0, 0.0, bs.dist_m);

    render::flak::Pose lo;
    lo.train_rad = 0.7;
    lo.elev_rad = 0.0;
    render::flak::Pose hi = lo;
    hi.elev_rad = render::flak::kElevMaxRad;
    const glm::dvec3 a = render::flak::train_station_world(f, lo, ahead);
    const glm::dvec3 b = render::flak::train_station_world(f, hi, ahead);
    // Elevation moves it not at all...
    CHECK(glm::length(a - b) < 1e-9);
    // ...it is exactly dist_m from the mount, in the tangent plane (on the
    // pad, not above it)...
    CHECK_THAT(glm::length(a - f.pos),
               Catch::Matchers::WithinAbs(bs.dist_m, 1e-9));
    CHECK_THAT(glm::dot(a - f.pos, f.up),
               Catch::Matchers::WithinAbs(0.0, 1e-9));
    // ...and train DOES swing it.
    render::flak::Pose other = lo;
    other.train_rad = lo.train_rad + 1.2;
    CHECK(glm::length(render::flak::train_station_world(f, other, ahead) - a) >
          1.0);
}

// ===========================================================================
// STAGE C -- the drum swap and the spent brass (immersion ladder 3/4). Both
// are COSMETIC closed forms in render/flak_gun.h; nothing here touches the
// reload FACT in app/flak_tick.h.
// ===========================================================================

TEST_CASE("flak drum swap is identity at rest and clear mid reload",
          "[flak][drum]") {
    using namespace render::flak;
    // THE OFF-ARM. reload_left_s == 0 is the rest state and it must be
    // EXACTLY identity -- not "small", exactly zero, in both components and
    // with the drum on the gun. flak_model.cpp additionally guards on
    // reload_frac > 0.0 so the node translation is never even touched.
    const DrumSwap rest = drum_swap(0.0);
    CHECK(rest.visible);
    CHECK(rest.out_m == 0.0);
    CHECK(rest.down_m == 0.0);
    // Negative / nonsense fraction is also the rest state, never a throw.
    CHECK(drum_swap(-1.0).out_m == 0.0);
    CHECK(drum_swap(-1.0).visible);

    // The instant the reload BEGINS (frac == 1, progress 0) the drum is still
    // seated -- the animation is continuous into the rest pose at both ends.
    const DrumSwap start = drum_swap(1.0);
    CHECK(start.visible);
    CHECK_THAT(start.out_m, Catch::Matchers::WithinAbs(0.0, 1e-12));
    CHECK_THAT(start.down_m, Catch::Matchers::WithinAbs(0.0, 1e-12));

    // MIDDLE of the reload: no drum on the gun at all.
    const DrumSwap mid = drum_swap(0.5);
    CHECK_FALSE(mid.visible);

    // Phase 1 (progress 0 -> kDrumOutFrac) carries the drum monotonically OUT
    // and DOWN, ending at the full travel.
    double prev_out = -1.0, prev_down = -1.0;
    for (int i = 0; i <= 20; ++i) {
        const double p = kDrumOutFrac * (static_cast<double>(i) / 20.0) * 0.999;
        const DrumSwap d = drum_swap(1.0 - p);
        INFO("progress " << p);
        CHECK(d.visible);
        CHECK(d.out_m >= prev_out);
        CHECK(d.down_m >= prev_down);
        prev_out = d.out_m;
        prev_down = d.down_m;
    }
    CHECK(prev_out > kDrumOutM * 0.98);
    CHECK(prev_down > kDrumDropM * 0.95);

    // Phase 3 is phase 1 REVERSED: the fresh drum comes back the same way, so
    // the swap is symmetric about the middle in both components.
    for (int i = 1; i <= 10; ++i) {
        const double u = static_cast<double>(i) / 11.0;
        const DrumSwap out = drum_swap(1.0 - kDrumOutFrac * u);   // going out
        const DrumSwap in = drum_swap(kDrumInFrac * u);           // coming in
        INFO("u " << u);
        CHECK_THAT(in.out_m, Catch::Matchers::WithinAbs(out.out_m, 1e-12));
        CHECK_THAT(in.down_m, Catch::Matchers::WithinAbs(out.down_m, 1e-12));
        CHECK(in.visible);
    }
    // It FALLS: the drop is quadratic, so at half the phase it has covered
    // well under half the distance (a linear drop would sit at exactly half).
    const DrumSwap half = drum_swap(1.0 - kDrumOutFrac * 0.5);
    CHECK(half.down_m < kDrumDropM * 0.5 * 0.75);
    CHECK_THAT(half.out_m, Catch::Matchers::WithinAbs(kDrumOutM * 0.5, 1e-12));
}

TEST_CASE("flak brass ejects right and down, is deterministic, and caps",
          "[flak][brass]") {
    using namespace render::flak;
    // The generic point again -- no world axis coincides with local up.
    const glm::dvec3 up = glm::normalize(glm::dvec3(1.0, 1.0, 1.0));
    const glm::dvec3 pos = up * 6371000.0;
    const MountFrame f = make_mount_frame(pos, up, glm::dvec3(0.0, 0.0, 1.0));
    Stations s;
    // A plausible station table: the chute on the RIGHT (+X), at bore height,
    // ahead of the trunnion -- the shipped GLB shape.
    s.trunnion_h = kTrunnionBuiltM;
    s.eject = glm::dvec3(0.12, kTrunnionBuiltM + 0.10, 0.35);
    Pose p;  // zero train, zero elevation

    // --- 1. IT GOES RIGHT AND DOWN, in the gun own frame.
    {
        BrassPool bp;
        brass_spawn(bp, f, s, p, 0);
        const BrassCase& k = bp.cases[0];
        CHECK(k.active);
        CHECK(glm::dot(k.vel, f.right0) > 0.0);  // to the gunner RIGHT
        CHECK(glm::dot(k.vel, f.up) < 0.0);      // and DOWN
        const double sp = glm::length(k.vel);
        CHECK(sp >= kBrassSpeedMinMps);
        CHECK(sp <= kBrassSpeedMaxMps);
    }

    // --- 2. DETERMINISM. The same shot index always throws the same case;
    // different indices differ (a jitter that never varies is not a jitter).
    {
        BrassPool a, b;
        brass_spawn(a, f, s, p, 7);
        brass_spawn(b, f, s, p, 7);
        CHECK(glm::length(a.cases[0].vel - b.cases[0].vel) == 0.0);
        CHECK(glm::length(a.cases[0].pos - b.cases[0].pos) == 0.0);
        BrassPool c;
        brass_spawn(c, f, s, p, 8);
        CHECK(glm::length(c.cases[0].vel - a.cases[0].vel) > 1e-6);
    }

    // --- 3. THE CAP AND THE WRAP. 400 shots into a 150-case ring leaves
    // exactly kBrassCap slots, never more, and the ring keeps writing.
    {
        BrassPool bp;
        for (int i = 0; i < 400; ++i) brass_spawn(bp, f, s, p, i);
        CHECK(bp.cases.size() == static_cast<std::size_t>(kBrassCap));
        CHECK(bp.spawned == 400);
        CHECK(brass_live(bp) == kBrassCap);
        CHECK(bp.next == 400 % kBrassCap);
    }

    // --- 4. THEY LAND ON THE PAD AND STAY THERE -- no case ever below it.
    {
        BrassPool bp;
        for (int i = 0; i < 60; ++i) brass_spawn(bp, f, s, p, i);
        for (int step = 0; step < 2000; ++step) {
            brass_step(bp, f, 9.81, 1.0 / 120.0);
            for (const BrassCase& k : bp.cases) {
                if (!k.active) continue;
                // AT OR ABOVE the pad, always -- mid-flight and at rest. The
                // rest height is exactly one case radius (it lies on its side;
                // the drawn cylinder touches the snow, never crosses).
                CHECK(glm::dot(k.pos - f.pos, f.up) >= kBrassRadM - 1e-9);
            }
        }
        int resting = 0;
        for (const BrassCase& k : bp.cases)
            if (k.active && k.at_rest) {
                ++resting;
                // At rest it LIES FLAT: the long axis is in the tangent plane.
                CHECK_THAT(glm::dot(k.axis, f.up),
                           Catch::Matchers::WithinAbs(0.0, 1e-9));
                CHECK(glm::length(k.vel) == 0.0);
                CHECK_THAT(glm::dot(k.pos - f.pos, f.up),
                           Catch::Matchers::WithinAbs(kBrassRadM, 1e-9));
            }
        CHECK(resting == 60);  // 16 s of falling: every one of them is down
        // They land NEAR the gun -- a pile at his feet, not a spray.
        for (const BrassCase& k : bp.cases)
            if (k.active) CHECK(glm::length(k.pos - f.pos) < 8.0);
    }

    // --- 4b. THE PAD IS THE DRAWN SURFACE. A gun stands at the BARE TERRAIN
    // radius, so the snow the player sees is up to ~0.75 m above the mount --
    // cases rested at the mount are buried and invisible (the defect the
    // first screenshot caught). With a pad height they come to rest ON it.
    {
        const double pad = 0.75;
        BrassPool bp;
        for (int i = 0; i < 40; ++i) brass_spawn(bp, f, s, p, i);
        for (int step = 0; step < 2000; ++step) {
            brass_step(bp, f, 9.81, 1.0 / 120.0, pad);
            for (const BrassCase& k : bp.cases) {
                if (!k.active) continue;
                // Never below the SNOW, not merely never below the rock.
                CHECK(glm::dot(k.pos - f.pos, f.up) >= pad + kBrassRadM - 1e-9);
            }
        }
        for (const BrassCase& k : bp.cases)
            if (k.active && k.at_rest)
                CHECK_THAT(glm::dot(k.pos - f.pos, f.up),
                           Catch::Matchers::WithinAbs(pad + kBrassRadM, 1e-9));
    }

    // --- 5. THE FADE, and the slot freeing itself at the end of life.
    {
        CHECK(brass_alpha(0.0) == 1.0);
        CHECK(brass_alpha(kBrassLifeS - kBrassFadeS) == 1.0);
        CHECK(brass_alpha(kBrassLifeS) == 0.0);
        CHECK(brass_alpha(kBrassLifeS + 100.0) == 0.0);
        CHECK(brass_alpha(kBrassLifeS - kBrassFadeS * 0.5) < 1.0);
        CHECK(brass_alpha(kBrassLifeS - kBrassFadeS * 0.5) > 0.0);
        BrassPool bp;
        brass_spawn(bp, f, s, p, 3);
        CHECK(brass_live(bp) == 1);
        for (int i = 0; i < 200; ++i) brass_step(bp, f, 9.81, 0.5);
        CHECK(brass_live(bp) == 0);  // 100 s: gone, slot reusable
    }

    // --- 6. dt <= 0 is a NO-OP (a paused frame must not settle the pile).
    {
        BrassPool bp;
        brass_spawn(bp, f, s, p, 1);
        const glm::dvec3 was = bp.cases[0].pos;
        brass_step(bp, f, 9.81, 0.0);
        brass_step(bp, f, 9.81, -1.0);
        CHECK(glm::length(bp.cases[0].pos - was) == 0.0);
        CHECK(bp.cases[0].age_s == 0.0);
    }

    // --- 7. THE CHUTE TURNS WITH THE GUN. Trained 90 deg right, the throw
    // must follow the gun, not a world axis.
    {
        Pose t90;
        t90.train_rad = kPi / 2.0;
        BrassPool bp;
        brass_spawn(bp, f, s, t90, 0);
        // The gun right at +90 deg train is the mount -fwd0.
        CHECK(glm::dot(bp.cases[0].vel, -f.fwd0) > 0.0);
        CHECK(glm::dot(bp.cases[0].vel, f.up) < 0.0);
    }
}

TEST_CASE("flak brass burst indexes each shot separately", "[flak][brass]") {
    using namespace render::flak;
    const glm::dvec3 up = glm::normalize(glm::dvec3(1.0, 1.0, 1.0));
    const MountFrame f =
        make_mount_frame(up * 6371000.0, up, glm::dvec3(0.0, 0.0, 1.0));
    Stations s;
    s.trunnion_h = kTrunnionBuiltM;
    s.eject = glm::dvec3(0.12, kTrunnionBuiltM + 0.10, 0.35);
    const Pose p;
    // A three-round frame must produce three DIFFERENT cases -- if the burst
    // reused one index the pile would be three cases flying in lockstep.
    BrassPool burst;
    brass_spawn_burst(burst, f, s, p, 3, 100);
    CHECK(burst.spawned == 3);
    CHECK(brass_live(burst) == 3);
    CHECK(glm::length(burst.cases[0].vel - burst.cases[1].vel) > 1e-6);
    CHECK(glm::length(burst.cases[1].vel - burst.cases[2].vel) > 1e-6);
    // And it must equal the same three shots spawned one at a time.
    BrassPool one;
    for (int i = 0; i < 3; ++i) brass_spawn(one, f, s, p, 100 + i);
    for (int i = 0; i < 3; ++i)
        CHECK(glm::length(one.cases[i].vel - burst.cases[i].vel) == 0.0);
    // Zero shots is a no-op.
    BrassPool none;
    brass_spawn_burst(none, f, s, p, 0, 0);
    CHECK(none.spawned == 0);
}

// The reload FOLEY envelope (render/gun_audio.h). Cosmetic; the two voices
// differ only in life/tau/centre frequencies, so ONE shape serves both.
TEST_CASE("flak reload foley envelope decays to exactly zero", "[flak][audio]") {
    // Outside (0, life) it is exactly 0 -- the voice-free arm.
    CHECK(render::clank_amp(-1.0, render::kClankLifeS,
                            render::kClankDecayTau) == 0.0);
    CHECK(render::clank_amp(0.0, render::kClankLifeS,
                            render::kClankDecayTau) == 0.0);
    CHECK(render::clank_amp(render::kClankLifeS, render::kClankLifeS,
                            render::kClankDecayTau) == 0.0);
    CHECK(render::clank_amp(1.0, 0.0, 0.1) == 0.0);  // zero life, never a NaN
    // Inside it is in (0,1] and MONOTONE DECREASING -- a strike, not a swell.
    double prev = 2.0;
    for (int i = 1; i < 60; ++i) {
        const double age = render::kClankLifeS * (static_cast<double>(i) / 60.0);
        const double a =
            render::clank_amp(age, render::kClankLifeS, render::kClankDecayTau);
        INFO("age " << age);
        CHECK(a > 0.0);
        CHECK(a <= 1.0);
        CHECK(a < prev);
        prev = a;
    }
    // The LATCH is the heavier of the two: longer life, slower decay, so at a
    // fixed age well into both it is still ringing louder than the clank.
    const double t = 0.20;
    CHECK(render::clank_amp(t, render::kLatchLifeS, render::kLatchDecayTau) >
          render::clank_amp(t, render::kClankLifeS, render::kClankDecayTau));
    CHECK(render::kLatchLifeS > render::kClankLifeS);
    CHECK(render::kLatchRingFc < render::kClankRingFc);  // lower = heavier
}

// F-POSE PULLOUT (Chad's 2026-08-31 ruling): the sight picture is clean and
// free-look eases the camera OUT behind his shoulder, where the man is drawn.
// The fade must keep him out of the lens while the camera is still at his eye,
// reach solid by the time it is outside, and move with no pop at either knee.
TEST_CASE("flak gunner fade is 0 at the sight, 1 when pulled out, monotone "
          "and popless between",
          "[flak][pose]") {
    using render::flak::gunner_fade;
    using render::flak::kExtFadeEnd;
    using render::flak::kExtFadeStart;
    CHECK(kExtFadeStart > 0.0);
    CHECK(kExtFadeEnd > kExtFadeStart);
    CHECK(kExtFadeEnd <= 1.0);
    // Down the sight NOTHING of him is drawn -- the ruling, as a number.
    CHECK(gunner_fade(0.0) == 0.0);
    CHECK(gunner_fade(kExtFadeStart) == 0.0);
    // Solid once the camera is properly outside.
    CHECK(gunner_fade(kExtFadeEnd) == 1.0);
    CHECK(gunner_fade(1.0) == 1.0);
    double prev = 0.0;
    for (int i = 1; i < 40; ++i) {
        const double t = kExtFadeStart + (kExtFadeEnd - kExtFadeStart) *
                                             (static_cast<double>(i) / 40.0);
        const double v = gunner_fade(t);
        INFO("t " << t);
        CHECK(v > prev);
        CHECK(v > 0.0);
        CHECK(v < 1.0);
        prev = v;
    }
    const double eps = 1e-4;
    CHECK(gunner_fade(kExtFadeStart + eps) < 0.001);
    CHECK(gunner_fade(kExtFadeEnd - eps) > 0.999);
}

// The pullout camera must sit BEHIND the breech (where the gunner stands),
// ABOVE the mount, at the asked distance -- and it must do all three with no
// world axis assumed, so it still frames him on the far side of the sphere.
TEST_CASE("flak pullout camera sits behind and above the gunner at any "
          "attitude",
          "[flak][pose]") {
    const glm::dvec3 up = glm::normalize(glm::dvec3(1, 1, 1));
    const glm::dvec3 pos = up * 1000.0;
    // any fwd0 not parallel to up; make_mount_frame re-projects it.
    const render::flak::MountFrame mf =
        render::flak::make_mount_frame(pos, up, glm::dvec3(0, 0, 1));
    for (const double train : {0.0, 1.1, -2.4, 3.0}) {
        render::flak::Pose pose;
        pose.train_rad = train;
        pose.elev_rad = 0.3;
        const render::flak::ExternalCamera c = render::flak::external_camera(
            mf, pose, 0.0, 0.0, render::flak::kExtDistM);
        INFO("train " << train);
        // Up is the mount's local up -- never a world axis, never rolled.
        CHECK(glm::length(c.up - mf.up) < 1e-12);
        // The aim point is over the mount, at the ruled height.
        const glm::dvec3 tgt_off = c.target - mf.pos;
        CHECK(std::abs(glm::dot(tgt_off, mf.up) -
                       render::flak::kExtTargetUpM) < 1e-9);
        // The eye is exactly kExtDistM from it...
        const glm::dvec3 back = c.eye - c.target;
        CHECK(std::abs(glm::length(back) - render::flak::kExtDistM) < 1e-9);
        // ...ABOVE the mount (he is not filmed from underground)...
        CHECK(glm::dot(back, mf.up) > 0.0);
        // ...and BEHIND the muzzle: the camera is on the breech side, so it
        // must sit opposite the trained bore, never out in front of the gun.
        const glm::dvec3 bore = render::flak::bore_dir_world(mf, pose);
        CHECK(glm::dot(glm::normalize(back), bore) < 0.0);
        // The eye stays clear of the ground plane through the mount.
        CHECK(glm::dot(c.eye - mf.pos, mf.up) > 0.5);
    }
}

// Free-look must ORBIT the pullout, not slide it: the eye stays exactly
// kExtDistM from the same aim point whatever the head is doing.
TEST_CASE("flak pullout free-look orbits at a fixed radius", "[flak][pose]") {
    const glm::dvec3 up = glm::normalize(glm::dvec3(-2, 5, 1));
    const render::flak::MountFrame mf = render::flak::make_mount_frame(
        up * 900.0, up, glm::dvec3(1, 0, 0));
    render::flak::Pose pose;
    pose.train_rad = 0.7;
    pose.elev_rad = 1.2;
    const render::flak::ExternalCamera ref =
        render::flak::external_camera(mf, pose, 0.0, 0.0,
                                      render::flak::kExtDistM);
    for (const double az : {-2.0, -0.5, 0.5, 2.0}) {
        for (const double el : {-0.5, 0.0, 0.8}) {
            const render::flak::ExternalCamera c =
                render::flak::external_camera(mf, pose, az, el,
                                              render::flak::kExtDistM);
            INFO("az " << az << " el " << el);
            CHECK(glm::length(c.target - ref.target) < 1e-12);
            CHECK(std::abs(glm::length(c.eye - c.target) -
                           render::flak::kExtDistM) < 1e-9);
        }
    }
}
