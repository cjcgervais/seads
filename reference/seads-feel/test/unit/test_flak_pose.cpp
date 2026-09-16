// F-POSE -- THE GUNNER'S SKELETON SOLVE, PINNED (render/flak_pose.h).
//
// docs/FLAK_GUN_SPEC.md §7.6 + the P1-9 CROUCH-CURVE ruling (Chad,
// 2026-08-30). The stations come from the SHIPPED GLB (cgltf test-side, the
// test_flak_gun idiom -- nothing retypes a station number); the man is a
// SYNTHETIC rod-man near the real geometry, because every leg below is a
// PROPERTY that must hold for ANY admissible anthro (the test_rider_load
// law): welds hold at every elevation, the feet never move, the crouch is
// monotone, no limb stretches, sides never mirror, and the world map holds
// at up = normalize(1,1,1) where no world axis is up.
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <string>

#include "render/flak_pose.h"

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

// Stations off the shipped gun GLB -- the ONE source (test_flak_gun pins
// this same read against the documented table).
bool load_stations(Stations& s) {
    const std::string path =
        std::string(SEADS_ASSET_DIR) + "/flak/oerlikon_mk4.glb";
    cgltf_options opt{};
    cgltf_data* d = nullptr;
    if (cgltf_parse_file(&opt, path.c_str(), &d) != cgltf_result_success)
        return false;
    if (cgltf_load_buffers(&opt, d, path.c_str()) != cgltf_result_success) {
        cgltf_free(d);
        return false;
    }
    const auto at = [&](const char* name, glm::dvec3& v) {
        for (cgltf_size i = 0; i < d->nodes_count; ++i) {
            const cgltf_node& n = d->nodes[i];
            if (n.name == nullptr || std::string(n.name) != name) continue;
            cgltf_float w[16];
            cgltf_node_transform_world(&n, w);
            v = glm::dvec3(w[12], w[13], w[14]);
            return true;
        }
        return false;
    };
    bool ok = true;
    ok &= at("st_muzzle", s.muzzle);
    ok &= at("st_eye", s.eye);
    ok &= at("st_sight_rear", s.sight_rear);
    ok &= at("st_sight_front", s.sight_front);
    ok &= at("st_grip_l", s.grip_l);
    ok &= at("st_grip_r", s.grip_r);
    ok &= at("st_pad_l", s.pad_l);
    ok &= at("st_pad_r", s.pad_r);
    ok &= at("st_eject", s.eject);
    ok &= at("st_drum", s.drum);
    ok &= at("st_foot_l", s.foot_l);
    ok &= at("st_foot_r", s.foot_r);
    ok &= at("st_approach", s.approach);
    glm::dvec3 tr(0.0), tn(0.0);
    ok &= at(kNodeCradle, tr);
    ok &= at(kNodeTrain, tn);
    s.trunnion_h = tr.y;
    s.train_h = tn.y;
    cgltf_free(d);
    return ok;
}

// The DRAWN gun body's box, off the same shipped GLB -- the metal the helmet
// is not allowed to be inside. Cradle-local (the GLB's rest frame), which is
// the frame the pose solves in before the elevation is applied.
bool load_gun_box(glm::dvec3& lo, glm::dvec3& hi) {
    const std::string path =
        std::string(SEADS_ASSET_DIR) + "/flak/oerlikon_mk4.glb";
    cgltf_options opt{};
    cgltf_data* d = nullptr;
    if (cgltf_parse_file(&opt, path.c_str(), &d) != cgltf_result_success)
        return false;
    if (cgltf_load_buffers(&opt, d, path.c_str()) != cgltf_result_success) {
        cgltf_free(d);
        return false;
    }
    bool found = false;
    lo = glm::dvec3(1e30);
    hi = glm::dvec3(-1e30);
    for (cgltf_size i = 0; i < d->nodes_count; ++i) {
        const cgltf_node& n = d->nodes[i];
        if (n.name == nullptr || std::string(n.name) != "flak_gun") continue;
        if (n.mesh == nullptr) break;
        cgltf_float w[16];
        cgltf_node_transform_world(&n, w);
        for (cgltf_size pi = 0; pi < n.mesh->primitives_count; ++pi) {
            const cgltf_primitive& pr = n.mesh->primitives[pi];
            for (cgltf_size ai = 0; ai < pr.attributes_count; ++ai) {
                const cgltf_accessor* acc = pr.attributes[ai].data;
                if (pr.attributes[ai].type != cgltf_attribute_type_position ||
                    acc == nullptr || !acc->has_min || !acc->has_max)
                    continue;
                for (int c = 0; c < 8; ++c) {
                    const glm::dvec3 v(
                        (c & 1) ? acc->max[0] : acc->min[0],
                        (c & 2) ? acc->max[1] : acc->min[1],
                        (c & 4) ? acc->max[2] : acc->min[2]);
                    const glm::dvec3 p(
                        w[0] * v.x + w[4] * v.y + w[8] * v.z + w[12],
                        w[1] * v.x + w[5] * v.y + w[9] * v.z + w[13],
                        w[2] * v.x + w[6] * v.y + w[10] * v.z + w[14]);
                    lo = glm::min(lo, p);
                    hi = glm::max(hi, p);
                    found = true;
                }
            }
        }
        break;
    }
    cgltf_free(d);
    return found;
}

// A synthetic man in the Sudburian's regime (the drawer MEASURES the real
// one off the rig; these are declared, and nothing below asserts them --
// only what the solve does with any of them).
GunnerAnthro rod_man() {
    GunnerAnthro a;
    a.torso_m = 0.52;
    a.thigh_m = 0.45;
    a.calf_m = 0.46;
    a.uarm_m = 0.34;
    a.farm_m = 0.27;
    a.hip_half_m = 0.095;
    a.neck_m = 0.095;
    a.ankle_up_m = 0.10;
    return a;
}

const double kGrid[] = {-5.0, 0.0, 10.0, 20.0, 35.0, 50.0, 65.0, 75.0, 87.0};

}  // namespace

// ★★★ P1-10 (Chad, 2026-09-01): "JUST HAVE TO LET THE MAN STEP BACK AND HOLD
// THE SHOULER PADS LIKE THEY ARE HANDLES." The weld MOVED -- it is his HANDS
// on st_pad_l/r now, and the shoulders are solved, not welded. The legs below
// are the RULING, not the build: he holds the handles, the arms keep ONE
// working length through the whole curve, and his feet stand under him.
TEST_CASE("flak pose: the hands hold the pads, the arms never lock",
          "[flak][pose]") {
    Stations s;
    REQUIRE(load_stations(s));
    const GunnerAnthro a = rod_man();
    const double arm = a.uarm_m + a.farm_m;
    for (const double deg : kGrid) {
        INFO("elev " << deg);
        const double e = deg * kDeg;
        const GunnerJoints g = gunner_solve(s, e, a);
        // THE WELD: each mitt grips ONE of the rotated pads, and they grip
        // DIFFERENT ones. ★ NOT matched by suffix: the gun names its
        // stations off the mount's convention (st_pad_l at x = -0.235) and
        // the rig names its bones off the man's (upperarm_l at x = +0.21),
        // and the two disagree in SIGN. The pairing below is the one Chad
        // asked for on 2026-09-01 -- "swap his hands ... on the gun".
        const glm::dvec3 pl = elev_point(s, e, s.pad_l);
        const glm::dvec3 pr = elev_point(s, e, s.pad_r);
        const double lp = std::min(glm::length(g.hand_l - pl),
                                   glm::length(g.hand_l - pr));
        const double rp = std::min(glm::length(g.hand_r - pl),
                                   glm::length(g.hand_r - pr));
        CHECK(lp < 0.09);
        CHECK(rp < 0.09);
        CHECK(glm::length(g.hand_l - g.hand_r) > 0.30);  // not the same pad
        // ★★★ AND HIS LEFT ARM IS ON HIS LEFT -- the whole chain, so no part
        // of it is drawn across his chest. `left_sign` is measured off the
        // rig, so this holds for a mirrored rig too (swept below).
        CHECK((g.hand_l.x - g.hand_r.x) * a.left_sign > 0.0);
        CHECK((g.elbow_l.x - g.elbow_r.x) * a.left_sign > 0.0);
        CHECK((g.shoulder_l.x - g.shoulder_r.x) * a.left_sign > 0.0);
        // THE ARMS: one working length at EVERY elevation -- never locked
        // straight, never overreached. This is what makes the elevation
        // curve a crouch instead of a stretch.
        for (const double d : {glm::length(g.hand_l - g.shoulder_l),
                               glm::length(g.hand_r - g.shoulder_r)}) {
            CHECK(d < kArmWorkFrac * arm + 0.06);
            CHECK(d > kArmWorkFrac * arm - 0.06);
            CHECK(d < arm);  // the elbow always has somewhere to go
        }
        // ...and the shoulders are NOT in the pads any more: he stands off
        // them by most of an arm. (This is the assertion that would have
        // gone red against the old build, and vice versa -- the two rulings
        // are mutually exclusive, which is what makes this leg mean
        // something.)
        CHECK(glm::length(g.shoulder_l - elev_point(s, e, s.pad_l)) > 0.35);
        CHECK(glm::length(g.shoulder_r - elev_point(s, e, s.pad_r)) > 0.35);
        // FEET: the stance WIDTH is the stations' to give, exactly; the
        // fore-aft is DERIVED (he steps back), and both feet stay on the
        // deck at the same station height.
        CHECK_THAT(g.ankle_l.x - s.foot_l.x, WithinAbs(0.0, 1e-12));
        CHECK_THAT(g.ankle_r.x - s.foot_r.x, WithinAbs(0.0, 1e-12));
        CHECK_THAT(g.ankle_l.y - (s.foot_l.y + a.ankle_up_m),
                   WithinAbs(0.0, 1e-12));
        CHECK_THAT(g.ankle_r.y - (s.foot_r.y + a.ankle_up_m),
                   WithinAbs(0.0, 1e-12));
        // he stands under his own shoulder line, both feet together
        const glm::dvec3 sh_mid = 0.5 * (g.shoulder_l + g.shoulder_r);
        CHECK_THAT(g.ankle_l.z - (sh_mid.z + kFootAheadOfShoulderZ),
                   WithinAbs(0.0, 1e-12));
        CHECK_THAT(g.ankle_r.z - g.ankle_l.z, WithinAbs(0.0, 1e-12));
    }
    // ★ AND HE STEPPED BACK. At the 0 deg stand his feet are AFT of the
    // stations that were drawn for a man standing IN the pads -- by most of
    // the arm he is now holding them with.
    const GunnerJoints g0 = gunner_solve(s, 0.0, a);
    CHECK(g0.ankle_l.z < s.foot_l.z - 0.20);
    CHECK(g0.ankle_r.z < s.foot_r.z - 0.20);
    // ★★★ THE PAIRING FOLLOWS THE RIG, NOT A TYPED SIDE. Hand a MIRRORED rig
    // (its left arm on the other side of the model) to the same stations and
    // the hands swap with it -- which is the whole reason `left_sign` is a
    // measured number and not a constant. A build that hard-coded the pairing
    // passes the sweep above and fails this.
    {
        GunnerAnthro m = rod_man();
        m.left_sign = -1.0;
        const GunnerJoints gm = gunner_solve(s, 20.0 * kDeg, m);
        const GunnerJoints gn = gunner_solve(s, 20.0 * kDeg, a);
        CHECK((gm.hand_l.x - gm.hand_r.x) * m.left_sign > 0.0);
        // ...and it really is the OTHER pad, not the same answer twice
        CHECK(glm::length(gm.hand_l - gn.hand_l) > 0.30);
    }
}

TEST_CASE("flak pose: P1-9b -- a crouching gunner over his own feet, never a sit",
          "[flak][pose]") {
    // ★ THE ROUND-1 LESSON, kept on purpose: the old leg here asserted the
    // pelvis drop was MONOTONE and > 0.25 m -- numbers the round-1 solve
    // satisfied by SITTING HIM ON THE SNOW at +87 (pelvis y 0.147), which
    // P1-9 ruled out. The invariants below are the ruling, not the build:
    // he stays OVER HIS STEPPED FEET, ABOVE the deck, out of the pedestal
    // column, upright through the mid elevations -- at every elevation.
    Stations s;
    REQUIRE(load_stations(s));
    const GunnerAnthro a = rod_man();
    const double deck_y = 0.5 * (s.foot_l.y + s.foot_r.y);
    for (int i = 0; i <= 30; ++i) {
        const double deg = -5.0 + (92.0 * i / 30.0);
        const GunnerJoints g = gunner_solve(s, deg * kDeg, a);
        const glm::dvec3 ank_mid = 0.5 * (g.ankle_l + g.ankle_r);
        const glm::dvec3 sh_mid = 0.5 * (g.shoulder_l + g.shoulder_r);
        // never a sit: hard floor above the deck (round 1 read 0.147 here)
        CHECK(g.pelvis.y > deck_y + 0.22);
        // over his own feet in plan (P1-10: the feet are DERIVED under the
        // shoulder line, so this is now nearly exact rather than a cap)
        CHECK(std::abs(g.pelvis.z - ank_mid.z) < kHipOverFootM + 0.10);
        // out of the pedestal column
        CHECK(g.pelvis.z < kPelvisAftLimitZ + 0.02);
        // ★★★ P1-10 BOUGHT THE POSTURE. Standing OFF the gun, the torso
        // never folds: rounds 1-4 read ~74 deg of forward fold at +87 (and
        // ~51 after the drop-aware seed) because his shoulders were welded
        // to pads that had swung down and forward. Holding the handles at
        // arm's length he stays UPRIGHT through the entire curve -- measured
        // -2.7 deg at the stand, -8.9 deg at +87, leaning slightly BACK
        // against the pull, which is what a man holding a bar does. 15 deg
        // is that measurement plus honest margin; it is FIVE TIMES tighter
        // than the bound the welded build needed and it would go red against
        // that build at once.
        const double pitch = std::atan2(sh_mid.z - g.pelvis.z,
                                        sh_mid.y - g.pelvis.y) / kDeg;
        CHECK(std::abs(pitch) < 15.0);
    }
    // ★★★ THE 0 DEG STAND IS ITSELF A BRACED CROUCH (Chad, 2026-09-01: "he
    // needs to bend his knees and bring his shoulders down too"). Rounds
    // 1-4 and the first P1-10 build stood him with near-locked knees --
    // measured 0.968 of a straight leg -- because the shoulder line was
    // whatever the geometry handed him. He is braced on a bar now: the knee
    // carries a real bend before the elevation asks for any. Measured 0.859;
    // the upper bound would go RED against every earlier build.
    const GunnerJoints g0 = gunner_solve(s, 0.0, a);
    CHECK(g0.pelvis.y > 0.75);
    const double straight = a.thigh_m + a.calf_m;
    const double leg0 = glm::length(g0.ankle_l - g0.hip_l);
    CHECK(leg0 < 0.92 * straight);   // genuinely bent, not standing tall
    CHECK(leg0 > 0.78 * straight);   // ...and not already collapsed
    // ... and the +87 crouch is still DEEP relative to that stand (measured
    // 0.326 m of pelvis drop; less than the old 0.35 only because he now
    // STARTS lower, which is the point of the ruling)
    const GunnerJoints g87 = gunner_solve(s, 87.0 * kDeg, a);
    CHECK(g0.pelvis.y - g87.pelvis.y > 0.25);
    // ...and it is the LEGS that pay for it, not the spine: the knee folds
    // from near-straight at the stand to about half extension at +87.
    CHECK(glm::length(g87.ankle_l - g87.hip_l) < 0.62 * (a.thigh_m + a.calf_m));
}

// ★★★ THE HEAD SITS ON THE NECK -- AND THAT IS ALSO THE SCARF LEG.
// Chad, 2026-09-01: "his head appears low compared to shoulders. Also the
// scarf is sticking straight up out of the sudburains helmet." ONE cause:
// the crane aimed at a point that, once he STANDS BACK, falls BELOW the
// shoulder line at high elevation. The head then hung under his own
// shoulders -- and because the rig's scarf chains (scarf_01.., scarf_s01..)
// are children of neck_01, and the drawer aims neck_01 AT THE HEAD, the
// inverted neck bone threw the scarf up over his helmet.
//
// So this leg grades the bound that fixes both: the head leans off the
// torso's own up by at most kHeadLeanMaxRad and NEVER below the shoulders.
// There is no scarf in the pure solve to test -- the guarantee for it is
// exactly "neck_01 never inverts", which is what the lean cap states.
TEST_CASE("flak pose: the head rides on top of the neck, never under it",
          "[flak][pose]") {
    Stations s;
    REQUIRE(load_stations(s));
    const GunnerAnthro a = rod_man();
    for (int i = 0; i <= 30; ++i) {
        const double deg = -5.0 + (92.0 * i / 30.0);
        INFO("elev " << deg);
        const GunnerJoints g = gunner_solve(s, deg * kDeg, a);
        const glm::dvec3 sh_mid = 0.5 * (g.shoulder_l + g.shoulder_r);
        const glm::dvec3 up_t = glm::normalize(sh_mid - g.pelvis);
        const glm::dvec3 nh = g.head - g.neck;
        // the neck bone keeps its length (the crane only chooses a direction)
        CHECK_THAT(glm::length(nh), WithinAbs(a.neck_m, 1e-12));
        // ...and its direction never leaves the cap off the torso's up
        const double lean = std::acos(std::min(
            1.0, std::max(-1.0, glm::dot(glm::normalize(nh), up_t))));
        CHECK(lean <= kHeadLeanMaxRad + 1e-9);
        // THE PLAIN CLAIM, in the shape Chad said it: his head is above his
        // shoulders. (Unclamped this read NEGATIVE past ~35 deg elevation.)
        CHECK(g.head.y > sh_mid.y + 0.5 * a.neck_m);
    }
}

TEST_CASE("flak pose: the helmet clears the breech through the whole curve",
          "[flak][pose]") {
    // Graded against the DRAWN gun (body-chain law): the gun assembly as a
    // capsule down the bore from the muzzle station to the aft end
    // (kGunOverallM behind it -- OP 911's 87 in overall). Round 1 put the
    // head at x = 0 and the helmet INSIDE the receiver at 45 deg (distance
    // to the bore line a few cm); the head now cranes onto the sight
    // axis, which is outboard LEFT + above -- both offsets are clearance.
    Stations s;
    REQUIRE(load_stations(s));
    const GunnerAnthro a = rod_man();
    for (const double deg : kGrid) {
        const double e = deg * kDeg;
        const GunnerJoints g = gunner_solve(s, e, a);
        const glm::dvec3 mz = elev_point(s, e, s.muzzle);
        const glm::dvec3 aft =
            elev_point(s, e, s.muzzle - glm::dvec3(0.0, 0.0, kGunOverallM));
        const glm::dvec3 ab = mz - aft;
        double t = glm::dot(g.head - aft, ab) / glm::dot(ab, ab);
        t = std::min(1.0, std::max(0.0, t));
        const double clr = glm::length(g.head - (aft + ab * t));
        CHECK(clr > 0.12);
        // ... and the head is genuinely off the centreline, on the SIGHT'S
        // side of the gun (the sign comes from the GLB, never typed)
        const double sight_side = s.eye.x >= 0.0 ? 1.0 : -1.0;
        // measured minimum over the curve is 0.019 m, at +87
        CHECK(sight_side * (g.head.x - g.neck.x) > 0.012);
    }
}

// ★★★ THE GAZE (Chad, 2026-08-31: "the sudburian should be looking through
// the pipper, currently he looks at the ground"). look_dir is what the
// drawer must aim his FACE along; the crane onto st_eye is only where his
// HEAD sits. The two are not interchangeable -- the last assertion measures
// how far apart they are, which is exactly the angle the head was wrong by
// when the drawer aimed the skull axis at the eye station. (The separation
// SHRANK under P1-10 -- standing back, his crane runs closer to the sight
// line -- so the bound was re-measured against the new stance, not kept.)
TEST_CASE("flak pose: the gaze is the sight axis, never the head crane",
          "[flak][pose]") {
    Stations s;
    REQUIRE(load_stations(s));
    const GunnerAnthro a = rod_man();
    for (const double deg : kGrid) {
        INFO("elev " << deg);
        const double e = deg * kDeg;
        const GunnerJoints g = gunner_solve(s, e, a);
        CHECK_THAT(glm::length(g.look_dir), WithinAbs(1.0, 1e-12));
        // it IS the rear-peep -> front-bead line under this elevation...
        const glm::dvec3 axis =
            glm::normalize(elev_point(s, e, s.sight_front) -
                           elev_point(s, e, s.sight_rear));
        CHECK_THAT(glm::dot(g.look_dir, axis), WithinAbs(1.0, 1e-12));
        // ...which the GLB builds parallel to the bore, so he looks exactly
        // where the gun points: the rise angle IS the elevation.
        CHECK_THAT(std::asin(g.look_dir.y), WithinAbs(e, 1e-6));
        CHECK(g.look_dir.z > 0.0);  // downrange, never back over his shoulder
        // THE DISCRIMINATOR: the head CRANE is a different direction. Aiming
        // the skull axis along it (what the drawer did before) tips the face
        // by this angle -- tens of degrees, and downward.
        // THE DISCRIMINATOR: the head CRANE is a different direction. Aiming
        // the skull axis along it (what the drawer did before 2026-08-31)
        // tips the face by this angle. Measured over the curve the two are
        // never closer than 19.3 deg (dot 0.944, at +10) and are nearly
        // opposed at full elevation (-0.954); 12 deg is that worst case with
        // honest margin.
        const glm::dvec3 crane = glm::normalize(g.head - g.neck);
        CHECK(glm::dot(crane, g.look_dir) < std::cos(12.0 * kDeg));
    }
}

// ★★★ THE HELMET IS NOT INSIDE THE GUN (Chad, 2026-08-31: "he is smushing
// his face into the back of the flak gum, pressed right up against it"). The
// crane used to aim the head JOINT at st_eye; the DRAWN head reaches 0.209 m
// further forward again (measured off the rig through its bind matrices), so
// the helmet front sat ~0.09 m inside flak_gun's aft box. The face point is
// now what the crane aims, pulled back by kEyeReliefM -- and this leg grades
// it against the SHIPPED GUN MESH's own box, not a typed radius.
//
// It is a PROPERTY over the whole admissible range of face reaches, because
// the anthro ARRIVES: a rig with a bigger helmet must pull the head further
// back by itself, never trade clearance for reach.
TEST_CASE("flak pose: the drawn head clears the gun body at every elevation",
          "[flak][pose]") {
    Stations s;
    REQUIRE(load_stations(s));
    glm::dvec3 lo(0.0), hi(0.0);
    REQUIRE(load_gun_box(lo, hi));
    // sanity: the box is the barrel+receiver, forward of the breech
    REQUIRE(hi.z > 1.0);
    REQUIRE(lo.z < 0.0);
    // The clearance a point must keep from the drawn gun, on the axis it is
    // outside on. Positive = outside the box.
    const auto outside_by = [&](const glm::dvec3& p) {
        return std::max(std::max(lo.x - p.x, p.x - hi.x),
                        std::max(std::max(lo.y - p.y, p.y - hi.y),
                                 std::max(lo.z - p.z, p.z - hi.z)));
    };
    for (const double reach : {0.10, 0.15, 0.2087, 0.24, 0.30}) {
        GunnerAnthro a = rod_man();
        a.face_fwd_m = reach;
        for (const double deg : kGrid) {
            INFO("elev " << deg << " face_fwd " << reach);
            const double e = deg * kDeg;
            const GunnerJoints g = gunner_solve(s, e, a);
            // Every part of him that could reach the metal, carried BACK out
            // of the elevation into the frame the gun's own box lives in.
            // ★ P1-10 made this a MARGIN, not a squeak: standing off the gun
            // at arm's length, the whole man clears by more than a helmet's
            // depth at every elevation -- there is no dial left to get wrong.
            const glm::dvec3 sh_mid = 0.5 * (g.shoulder_l + g.shoulder_r);
            const glm::dvec3 pts[] = {
                g.head + g.look_dir * a.face_fwd_m,  // his face
                g.head, g.neck, sh_mid, g.pelvis, g.shoulder_l, g.shoulder_r};
            for (const glm::dvec3& w : pts) {
                const glm::dvec3 q = elev_point(s, -e, w);
                INFO("body pt " << q.x << "," << q.y << "," << q.z
                                << "  box z " << lo.z << ".." << hi.z
                                << "  y " << lo.y << ".." << hi.y);
                CHECK(outside_by(q) > 0.05);
            }
        }
    }
    // ★ AND HE IS BEHIND IT AT THE STAND -- not merely outside the box on
    // some axis, but aft of the breech, which is the shape of Chad's ruling.
    {
        GunnerAnthro a = rod_man();
        a.face_fwd_m = 0.2087;  // the shipped rig's measured reach
        const GunnerJoints g = gunner_solve(s, 0.0, a);
        const glm::dvec3 face = g.head + g.look_dir * a.face_fwd_m;
        INFO("stand face z " << face.z << " vs breech " << lo.z);
        CHECK(face.z < lo.z - 0.30);
    }
}

TEST_CASE("flak pose: no limb stretches, no side mirrors", "[flak][pose]") {
    Stations s;
    REQUIRE(load_stations(s));
    const GunnerAnthro a = rod_man();
    for (const double deg : kGrid) {
        const GunnerJoints g = gunner_solve(s, deg * kDeg, a);
        // the spine does not stretch
        const glm::dvec3 sh_mid = 0.5 * (g.shoulder_l + g.shoulder_r);
        CHECK_THAT(glm::length(sh_mid - g.pelvis),
                   WithinAbs(a.torso_m, 1e-9));
        // thighs/upper arms are exact by the closed form; the distal bone
        // may take the clamped remainder only under overreach (guarded to a
        // few cm by the reach clamp)
        CHECK_THAT(glm::length(g.knee_l - g.hip_l), WithinAbs(a.thigh_m, 1e-9));
        CHECK_THAT(glm::length(g.knee_r - g.hip_r), WithinAbs(a.thigh_m, 1e-9));
        CHECK(glm::length(g.ankle_l - g.knee_l) < a.calf_m + 0.03);
        CHECK(glm::length(g.ankle_r - g.knee_r) < a.calf_m + 0.03);
        CHECK_THAT(glm::length(g.elbow_l - g.shoulder_l),
                   WithinAbs(a.uarm_m, 1e-9));
        CHECK_THAT(glm::length(g.elbow_r - g.shoulder_r),
                   WithinAbs(a.uarm_m, 1e-9));
        CHECK(glm::length(g.hand_l - g.elbow_l) < a.farm_m + 0.03);
        CHECK(glm::length(g.hand_r - g.elbow_r) < a.farm_m + 0.03);
        // the knee's BEND SIDE is toward the gun: its offset off the
        // hip->ankle chord (when it bends at all) points +Z, never aft --
        // a knee aft of the hip on a near-straight aft-sloping leg is
        // legitimate, a knee bent BACKWARD is not
        const auto bend_z = [](const glm::dvec3& hip, const glm::dvec3& knee,
                               const glm::dvec3& ankle) {
            const glm::dvec3 c = ankle - hip;
            const double t = glm::dot(knee - hip, c) / glm::dot(c, c);
            const glm::dvec3 off = knee - (hip + c * t);
            return glm::length(off) < 1e-3 ? 1.0 : off.z;
        };
        CHECK(bend_z(g.hip_l, g.knee_l, g.ankle_l) > 0.0);
        CHECK(bend_z(g.hip_r, g.knee_r, g.ankle_r) > 0.0);
    }
    // sides never mirror: nudge ONE grip and only ITS hand moves
    // (P1-10: the HANDS ride the pads now, so it is a pad that gets nudged.
    // WHICH hand it moves is the side pairing's business -- this leg only
    // says that EXACTLY ONE hand moves, and the other is bit-identical.)
    Stations s2 = s;
    s2.pad_l += glm::dvec3(0.01, 0.02, -0.01);
    const GunnerJoints ga = gunner_solve(s, 40.0 * kDeg, a);
    const GunnerJoints gb = gunner_solve(s2, 40.0 * kDeg, a);
    const double dl = glm::length(gb.hand_l - ga.hand_l);
    const double dr = glm::length(gb.hand_r - ga.hand_r);
    CHECK(std::max(dl, dr) > 0.005);
    CHECK_THAT(std::min(dl, dr), WithinAbs(0.0, 1e-12));
}

TEST_CASE("flak pose: the world map holds where no axis is up",
          "[flak][pose]") {
    Stations s;
    REQUIRE(load_stations(s));
    const GunnerAnthro a = rod_man();
    // the man is a TRAIN rider: joints go to the world through the same map
    // as st_foot (train_station_world), at a generic mount
    const glm::dvec3 up = glm::normalize(glm::dvec3(1.0, 1.0, 1.0));
    const MountFrame f =
        make_mount_frame(up * 15000.0, up, glm::dvec3(0.3, -0.2, 0.9));
    const GunnerJoints g = gunner_solve(s, 55.0 * kDeg, a);
    for (const double train : {0.0, 1.1, -2.4}) {
        Pose p;
        p.train_rad = train;
        // the ankle lands its solved height above the deck station along
        // LOCAL up; the P1-9b step-in is tangent, so the rest of the offset
        // is rigid (length-preserving), never leaked into up
        const glm::dvec3 wa = train_station_world(f, p, g.ankle_l);
        const glm::dvec3 wf = train_station_world(f, p, s.foot_l);
        CHECK_THAT(glm::dot(wa - wf, f.up),
                   WithinAbs(g.ankle_l.y - s.foot_l.y, 1e-9));
        CHECK_THAT(glm::length(wa - wf),
                   WithinAbs(glm::length(g.ankle_l - s.foot_l), 1e-9));
        // rigid under train: pairwise distances survive the rotation
        const glm::dvec3 wh = train_station_world(f, p, g.hand_l);
        const glm::dvec3 wp = train_station_world(f, p, g.pelvis);
        CHECK_THAT(glm::length(wh - wp),
                   WithinAbs(glm::length(g.hand_l - g.pelvis), 1e-9));
    }
}
