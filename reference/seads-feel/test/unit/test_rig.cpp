// Fleet Rig core (docs/fleet_rig_plan.md, /orchestrate rig-A.1): the PURE,
// headlessly-pinnable hierarchy + the glm->raylib bridge. These legs pin the
// two load-bearing pieces the Fable before-consult flagged
// (pre_spec_audits/fable_orchestrate_before_consult.md):
//   C1 — to_ray_fields transposes correctly (element identity + translation),
//   C4 — updateRig composes parent-on-left in one topo-ordered pass,
// plus the invariants that keep rig-A/B honest: topological order (parent <
// child), and uniform node scale (Fable: a non-uniformly-scaled parent shears
// its rotating children — here all scl == 1, asserted).

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include "render/rig.h"

#ifdef NDEBUG
#error "SEADS gate requires an assert-live build (SPEC 6.1)"
#endif

using Catch::Approx;

namespace {

bool approx_vec(const glm::vec3& a, const glm::vec3& b, float eps = 1e-4f) {
    return glm::length(a - b) < eps;
}

}  // namespace

// C1 — the load-bearing transpose bridge. Element identity: raylib field for
// (row r, col c) == glm g[c][r], i.e. to_ray_fields(g)[4*r + c] == g[c][r]. A
// non-transposing (raw memcpy) implementation fails this on any off-diagonal.
TEST_CASE(
    "rig: to_ray_fields transposes glm column-major into raylib row-major") {
    glm::mat4 g(0.0f);
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r) g[c][r] = static_cast<float>(1 + 4 * c + r);

    const std::array<float, 16> f = render::to_ray_fields(g);
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c) REQUIRE(f[4 * r + c] == Approx(g[c][r]));
}

// C1 (human-legible companion): a pure translation lands x,y,z in the raylib
// translation fields m12/m13/m14 == field indices 3/7/11, and the diagonal
// is 1.
TEST_CASE("rig: to_ray_fields puts translation in m12/m13/m14") {
    const glm::mat4 t = glm::translate(glm::mat4(1.0f), glm::vec3(1, 2, 3));
    const std::array<float, 16> f = render::to_ray_fields(t);
    REQUIRE(f[3] == Approx(1.0f));   // m12
    REQUIRE(f[7] == Approx(2.0f));   // m13
    REQUIRE(f[11] == Approx(3.0f));  // m14
    REQUIRE(f[0] == Approx(1.0f));   // m0  (diagonal)
    REQUIRE(f[5] == Approx(1.0f));   // m5
    REQUIRE(f[10] == Approx(1.0f));  // m10
    REQUIRE(f[15] == Approx(1.0f));  // m15
}

// C4 — hierarchy composition. Parent-on-left with column vectors: a child at
// body-local (0,0,-1) under a parent translated to (10,0,0) and rotated +90°
// about +Y must land at (9,0,0): R_y(+90)*(0,0,-1) = (-1,0,0), plus the parent
// origin. This pins the multiply ORDER and that rotation is applied (a swapped
// order or a transpose would move the child elsewhere).
TEST_CASE("rig: updateRig composes parent-on-left in one pass") {
    render::Rig r(2);
    r[0].parent = -1;
    r[0].pos = {10.0f, 0.0f, 0.0f};
    r[0].rot = glm::angleAxis(glm::radians(90.0f), glm::vec3(0, 1, 0));
    r[1].parent = 0;
    r[1].pos = {0.0f, 0.0f, -1.0f};
    render::updateRig(r);

    REQUIRE(approx_vec(glm::vec3(r[1].world[3]), glm::vec3(9.0f, 0.0f, 0.0f)));
    // And the cached world is exactly parent.world * local(child).
    const glm::mat4 expect = r[0].world * render::local(r[1]);
    for (int c = 0; c < 4; ++c)
        for (int row = 0; row < 4; ++row)
            REQUIRE(r[1].world[c][row] == Approx(expect[c][row]));
}

// The built aircraft rig obeys topological order (parent index < own index), so
// the single forward pass is valid — and a two-level chain (aileron under wing
// under fuselage) composes additively at rest (identity rotations).
TEST_CASE("rig: aircraft rig is topo-ordered and composes through two levels") {
    const render::Rig r = render::build_aircraft_rig();
    REQUIRE(r.size() == render::kNodeCount);

    for (int i = 0; i < render::kNodeCount; ++i)
        REQUIRE(r[i].parent < i);  // -1 for the root satisfies this too

    const auto& specs = render::aircraft_node_specs();
    // 3-level chain now (fuselage -> wing -> aileron); identity rotations
    // compose additively, so the aileron world position is the sum of the
    // chain's poses.
    const glm::vec3 expect_aileron = specs[render::kFuselage].rest_pos +
                                     specs[render::kLeftWing].rest_pos +
                                     specs[render::kLeftAileron].rest_pos;
    REQUIRE(approx_vec(glm::vec3(r[render::kLeftAileron].world[3]),
                       expect_aileron));
}

// Uniform node scale (Fable: non-uniform parent scale shears rotating
// children). Every built node has scl == 1; shape lives in NodeSpec.box_dims
// (the mesh size), so the invariant holds for the whole rig, parents included.
TEST_CASE("rig: every node has uniform (identity) scale") {
    const render::Rig r = render::build_aircraft_rig();
    for (const render::SceneNode& n : r)
        REQUIRE(approx_vec(n.scl, glm::vec3(1.0f)));
}

// The deflection contract (rig-B): ailerons are antisymmetric, elevator hinges
// about +X (pitch), rudder about +Y (yaw). Pins the catalog the animation
// reads.
TEST_CASE("rig: control-surface drive/hinge catalog is wired") {
    const auto& s = render::aircraft_node_specs();
    REQUIRE(s[render::kLeftAileron].driven == render::Driven::RollMirrored);
    REQUIRE(s[render::kRightAileron].driven == render::Driven::Roll);
    REQUIRE(s[render::kLeftElevator].driven == render::Driven::Pitch);
    REQUIRE(s[render::kRightElevator].driven == render::Driven::Pitch);
    REQUIRE(s[render::kRudder].driven == render::Driven::Rudder);
    REQUIRE(s[render::kLeftGearStrut].driven == render::Driven::Gear);
    REQUIRE(
        approx_vec(s[render::kLeftElevator].hinge_axis, glm::vec3(1, 0, 0)));
    REQUIRE(approx_vec(s[render::kRudder].hinge_axis, glm::vec3(0, 1, 0)));
}

// -------------------------------------------------------------------------
// rig-B — apply_deflection surface-direction contract.
//
// The DIRECTION SIGNS are DERIVED from the plant convention (Fable
// before-consult P0-1: do NOT trust the rig-A "-1 flip" comment as the oracle).
// step.cpp:91-97 is tau.{x,y,z} = c*Q*d*deflect(Input.{pitch,roll,yaw}) -
// damp*w, with POSITIVE c coefficients => +Input maps to +w about the +body
// axis (that convention is owned/pinned by AT-18a / AT-0; not re-stepped here).
// SPEC §7: roll-right = -wz, yaw-right = -wy, pitch-up = +wx. So +pitch =
// pitch-up, +roll = roll-LEFT, +yaw = yaw-LEFT. Each surface then deflects in
// the real-aircraft sense for that command; these legs pin that sense (a
// refactor that flips a sign fails here). The FELT direction is Chad's
// fly-verdict — a one-line flip if a hinge reads backwards (fleet_rig_plan.md
// rig-B).
namespace {
// Positive, distinct gains so a swapped gain also shows up.
const render::DeflectGains kG{/*ail*/ 0.30f, /*elev*/ 0.25f, /*rud*/ 0.35f,
                              /*gear*/ 1.4f};
// The node's trailing edge is its local +Z (aft); the posed rotation carries
// it.
glm::vec3 te(const render::Rig& r, int node) {
    return glm::normalize(r[node].rot * glm::vec3(0.0f, 0.0f, 1.0f));
}
bool quat_eq(const glm::quat& a, const glm::quat& b, float eps = 1e-6f) {
    return std::abs(a.w - b.w) < eps && std::abs(a.x - b.x) < eps &&
           std::abs(a.y - b.y) < eps && std::abs(a.z - b.z) < eps;
}
}  // namespace

// Zero surface command + gear_ext = 1 (the DEPLOYED rest) leaves EVERY node at
// its rest rotation, bit-for-bit — the strict-superset no-op (a nonzero default
// would move the mirror at trim). gear_ext=1 gives (1-1)*gain = 0.
TEST_CASE("rig-B: zero command + gear extended leaves every node at rest") {
    render::Rig r = render::build_aircraft_rig();
    const render::Rig rest = render::build_aircraft_rig();
    render::apply_deflection(r, sim::Inputs{}, 1.0f, kG);
    for (int i = 0; i < render::kNodeCount; ++i)
        REQUIRE(quat_eq(r[i].rot, rest[i].rot));
}

// +pitch (pitch-up) swings the elevator trailing edge UP (+Y). About the +X
// hinge that is a NEGATIVE angle (the rig-A "-1"); a dropped flip sends the TE
// down and fails. Static-node check: the fuselage/wings never move.
TEST_CASE("rig-B: +pitch deflects the elevator trailing edge up") {
    render::Rig r = render::build_aircraft_rig();
    sim::Inputs in{};
    in.pitch = 1.0f;
    render::apply_deflection(r, in, 1.0f, kG);
    REQUIRE(te(r, render::kLeftElevator).y > 0.05f);   // TE up = pitch-up
    REQUIRE(te(r, render::kRightElevator).y > 0.05f);  // both elevators
    REQUIRE(quat_eq(r[render::kFuselage].rot, glm::quat(1, 0, 0, 0)));
    REQUIRE(quat_eq(r[render::kLeftWing].rot, glm::quat(1, 0, 0, 0)));
    // Elevator only: rudder + ailerons stay at their REST under a pure pitch
    // command (the aileron rest is now the tilted surfL/surfR, not identity).
    const auto& sp = render::aircraft_node_specs();
    REQUIRE(quat_eq(r[render::kRudder].rot, sp[render::kRudder].rest_rot));
    REQUIRE(quat_eq(r[render::kRightAileron].rot,
                    sp[render::kRightAileron].rest_rot));
}

// +roll (roll-left) is ANTISYMMETRIC: right aileron TE down (-Y), left aileron
// TE up (+Y), equal magnitude. Measured as the swing RELATIVE to rest (the
// ailerons now carry a tilted rest_rot surfL/surfR — the wing dihedral bake —
// whose small yz cross-term breaks EXACT absolute antisymmetry; the swing about
// the +X hinge is exactly antisymmetric). MUTATION: RollMirrored -> Roll (both
// +roll) makes the two swings go the SAME way => the equal-and-opposite fails.
TEST_CASE("rig-B: +roll deflects the ailerons antisymmetrically") {
    render::Rig r = render::build_aircraft_rig();
    sim::Inputs in{};
    in.roll = 1.0f;
    render::apply_deflection(r, in, 1.0f, kG);
    const auto& sp = render::aircraft_node_specs();
    // Swing = rest⁻¹·posed = the pure hinge deflection; its TE (local +Z) tilt.
    const auto swing_te = [&](int node) {
        const glm::quat sw = glm::inverse(sp[node].rest_rot) * r[node].rot;
        return glm::normalize(sw * glm::vec3(0, 0, 1));
    };
    const float rY = swing_te(render::kRightAileron).y;
    const float lY = swing_te(render::kLeftAileron).y;
    REQUIRE(rY < -0.05f);                      // right down
    REQUIRE(lY > 0.05f);                       // left up
    REQUIRE(rY == Approx(-lY).margin(1e-4f));  // antisymmetric swing
}

// +yaw (yaw-left) swings the rudder trailing edge LEFT (-X). About the +Y hinge
// a NEGATIVE angle; a dropped flip sends the TE right and fails.
TEST_CASE("rig-B: +yaw deflects the rudder trailing edge left") {
    render::Rig r = render::build_aircraft_rig();
    sim::Inputs in{};
    in.yaw = 1.0f;
    render::apply_deflection(r, in, 1.0f, kG);
    REQUIRE(te(r, render::kRudder).x < -0.05f);  // TE left = yaw-left
}

// Linear in the command: half-stick is half the deflection angle (the surface
// IS the normalized Input, no curve). Measured as the rotation angle of the
// elevator.
TEST_CASE("rig-B: surface deflection is linear in the command") {
    render::Rig full = render::build_aircraft_rig();
    render::Rig half = render::build_aircraft_rig();
    sim::Inputs f{}, h{};
    f.pitch = 1.0f;
    h.pitch = 0.5f;
    render::apply_deflection(full, f, 1.0f, kG);
    render::apply_deflection(half, h, 1.0f, kG);
    const float a_full = glm::angle(full[render::kLeftElevator].rot);
    const float a_half = glm::angle(half[render::kLeftElevator].rot);
    REQUIRE(a_full == Approx(kG.elevator_rad).margin(1e-4f));
    REQUIRE(a_half == Approx(0.5f * kG.elevator_rad).margin(1e-4f));
}

// Gear unfold from the ACTUAL slewed extension: ext=1 = DEPLOYED (rest, no
// swing), ext=0 = retracted (swung by the full gear_deploy angle), monotone in
// between. The DEPLOYED rest is now a SPLAYED/RAKED pose (rig-D D.3b), so the
// unfold is measured as the SWING RELATIVE TO REST (inverse(rest)·posed), NOT
// the absolute angle — the retract composes onto the splay about the same +Z.
// MUTATION: driving by ext (not ext-1) inverts it — ext=1 would swing away from
// rest and the (deployed swing == 0) REQUIRE fails.
TEST_CASE(
    "rig-B: gear unfolds off the deployed rest (ext=1 rest, ext=0 swung)") {
    render::Rig deployed = render::build_aircraft_rig();
    render::Rig up = render::build_aircraft_rig();
    render::Rig mid = render::build_aircraft_rig();
    render::apply_deflection(deployed, sim::Inputs{}, 1.0f, kG);
    render::apply_deflection(up, sim::Inputs{}, 0.0f, kG);
    render::apply_deflection(mid, sim::Inputs{}, 0.5f, kG);
    const int G = render::kLeftGearStrut;
    const glm::quat restG = render::aircraft_node_specs()[G].rest_rot;
    const auto swing = [&](const render::Rig& rg) {
        return glm::angle(glm::inverse(restG) * rg[G].rot);
    };
    REQUIRE(swing(deployed) == Approx(0.0f).margin(1e-4f));
    REQUIRE(swing(up) == Approx(kG.gear_deploy_rad).margin(1e-4f));
    REQUIRE(swing(mid) == Approx(0.5f * kG.gear_deploy_rad).margin(1e-4f));
}

// rig-D D.3b RUGGED STANCE (Fable consult 2026-07-10). Two legs the placeholder
// rig couldn't have:
//   (1) SPLAY — at full deploy each main wheel sits OUTBOARD of its strut
//   pivot,
//       widening the track (the planted rough-field tripod). MUTATION: drop the
//       Rz(±12°) splay from rest_rot and the wheels sit under the pivots
//       (fails).
//   (2) MIRROR retract (Fable P0-1) — the RIGHT main must retract OUTBOARD (+X)
//       and UP, NOT sweep across the belly toward −X. With a shared +Z hinge
//       the right leg swept the wrong way; the fix is hinge_axis = −Z on the
//       right. MUTATION: revert the right hinge to +Z and the right wheel
//       retracts to −X (across the belly), failing the "outboard" REQUIRE.
TEST_CASE("rig-D: main gear splays outboard and retracts mirror-correct") {
    const int WL = render::kLeftWheel, WR = render::kRightWheel;
    render::Rig down = render::build_aircraft_rig();
    render::Rig upr = render::build_aircraft_rig();
    render::apply_deflection(down, sim::Inputs{}, 1.0f, kG);  // deployed
    render::apply_deflection(upr, sim::Inputs{}, 0.0f, kG);   // retracted
    // Wheel world X (children carry the strut sweep); struts sit at ±0.85.
    const float wlx_d = down[WL].world[3].x, wrx_d = down[WR].world[3].x;
    const float wlx_u = upr[WL].world[3].x, wrx_u = upr[WR].world[3].x;
    // (1) SPLAY: deployed wheels outboard of their ±0.85 pivots.
    REQUIRE(wlx_d < -0.85f);  // left wheel further −X than its pivot
    REQUIRE(wrx_d > 0.85f);   // right wheel further +X than its pivot
    // Struts are true mirrors across the centreline at deploy.
    REQUIRE(wlx_d == Approx(-wrx_d).margin(1e-3f));
    // (2) MIRROR retract: each wheel moves OUTBOARD-and-up as it tucks (away
    // from the belly), never across the centreline. A shared +Z right hinge
    // would send the right wheel to −X here.
    REQUIRE(wlx_u < wlx_d);  // left tucks further outboard (more −X) + up
    REQUIRE(wrx_u > wrx_d);  // right tucks further outboard (more +X) + up
    REQUIRE(wrx_u > 0.0f);   // right wheel NEVER crosses the belly to −X
}

// rig-D round 2 (Fable tripwire): the wing control surfaces MUST carry the
// mirrored dihedral/sweep tilt in rest_rot so their canonical +X hinge sits on
// the swept wing TE. A regression to identity (flat plates) would silently
// detach them from the wing across the span — invisible to the AABB/hinge legs.
// MUTATION: drop surfL/surfR back to identity -> the non-trivial-tilt REQUIRE
// fails.
TEST_CASE("rig-D: wing surfaces carry the mirrored dihedral tilt (not flat)") {
    const auto& s = render::aircraft_node_specs();
    for (int node : {render::kLeftAileron, render::kLeftFlap}) {
        const glm::quat q = s[node].rest_rot;
        REQUIRE(glm::angle(q) > 0.02f);                // tilt present (~3 deg)
        REQUIRE(glm::angle(q) < 0.12f);                // but small
        const glm::vec3 dir = q * glm::vec3(1, 0, 0);  // hinge direction
        REQUIRE(dir.x > 0.99f);   // still ~+X (canonical hinge)
        REQUIRE(dir.z < -0.01f);  // swept aft with the wing TE
    }
    // Left/right are TRUE mirrors: equal tilt magnitude, opposite y on
    // +X-image.
    REQUIRE(
        glm::angle(s[render::kLeftAileron].rest_rot) ==
        Approx(glm::angle(s[render::kRightAileron].rest_rot)).margin(1e-5f));
    const glm::vec3 dl = s[render::kLeftAileron].rest_rot * glm::vec3(1, 0, 0);
    const glm::vec3 dr = s[render::kRightAileron].rest_rot * glm::vec3(1, 0, 0);
    // The +X-image hinge dirs are mirror LINES: dirR = (x, -y, -z) of dirL.
    REQUIRE(dl.y == Approx(-dr.y).margin(1e-4f));  // dihedral mirrors across X
    REQUIRE(dl.z == Approx(-dr.z).margin(1e-4f));
}

// RA9 firewall (unit-level half): apply_deflection is a PURE function of its
// inputs — same inputs, same rig; it only writes the Rig it is handed (the
// caller passes ctrl + gear_ext by value, const draw state). The end-to-end
// sim-bit-identity firewall runs where the app loop lives
// (test_instructor_tick).
TEST_CASE("rig-B: apply_deflection is deterministic and writes only the rig") {
    render::Rig a = render::build_aircraft_rig();
    render::Rig b = render::build_aircraft_rig();
    sim::Inputs in{};
    in.pitch = 0.7f;
    in.roll = -0.3f;
    in.yaw = 0.2f;
    render::apply_deflection(a, in, 0.4f, kG);
    render::apply_deflection(b, in, 0.4f, kG);
    for (int i = 0; i < render::kNodeCount; ++i)
        REQUIRE(quat_eq(a[i].rot, b[i].rot));
}

// R4 GROUND-ROLL WHEEL SPIN. The main tyres are Driven::Wheel: the caller's
// accumulated roll angle spins them about their lateral +X axle; forward roll
// (positive accumulated angle) = NEGATIVE rotation about +X (top of the tyre
// toward the nose). Defaulted 0 must reproduce the rest pose BIT-EXACTLY (the
// strict-superset proof for every pre-wheel caller). MUTATIONS: drop the
// Driven::Wheel case -> the spun-vs-rest angle REQUIRE fails; flip the sign ->
// the direction REQUIRE fails (a point on the tyre top must move toward -Z).
TEST_CASE("rig-R4: main wheels spin with the accumulated ground roll") {
    const int WL = render::kLeftWheel;
    render::Rig rest = render::build_aircraft_rig();
    render::Rig spun = render::build_aircraft_rig();
    render::apply_deflection(rest, sim::Inputs{}, 1.0f, kG);  // wheel arg = 0
    render::apply_deflection(spun, sim::Inputs{}, 1.0f, kG, 0.4f);
    const glm::quat restW = render::aircraft_node_specs()[WL].rest_rot;
    // Zero roll = rest exactly (strict superset).
    REQUIRE(rest[WL].rot.w == restW.w);
    REQUIRE(rest[WL].rot.x == restW.x);
    REQUIRE(rest[WL].rot.y == restW.y);
    REQUIRE(rest[WL].rot.z == restW.z);
    // The spun tyre is rotated by the commanded angle relative to rest.
    const glm::quat rel = glm::inverse(restW) * spun[WL].rot;
    REQUIRE(glm::angle(rel) == Approx(0.4f).margin(1e-4f));
    // Direction: forward roll carries the tyre-top point (+Y in the wheel
    // frame) toward the NOSE (-Z), never the tail.
    const glm::vec3 top = rel * glm::vec3{0.0f, 1.0f, 0.0f};
    REQUIRE(top.z < -1e-3f);
    // The tailwheel is Driven::Gear (retract), untouched by the roll angle.
    const int TW = render::kTailWheel;
    REQUIRE(spun[TW].rot.w == rest[TW].rot.w);
    REQUIRE(spun[TW].rot.x == rest[TW].rot.x);
}