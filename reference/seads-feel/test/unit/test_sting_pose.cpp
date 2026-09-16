// ★★★ ST-5 PHASE D — THE SEAT DEPLOY, EXECUTED.
//
// The three-second draw from under the seat to the shoulder is animation, so
// most of it can only be judged by Chad's eye. What CAN be judged here is
// everything the eye is bad at: that the timer takes the time it claims, that
// the stance-loss CUT is a cut and not a fast blend, that a launch clicked
// mid-draw cannot leave a half-raised launcher, that the two legs of the
// sweep agree on the frame they share, and — the one that matters most —
// that the tube sweeps AROUND his silhouette instead of through his head.
//
// MUTATION NOTES (what each leg would catch):
//  * swap the linear rise for render::blend_toward's exponential -> the
//    "3.0 s reaches 1" leg goes red (an asymptote never arrives).
//  * let `cut` ease down at kStowFallS instead of dropping -> the cut leg.
//  * drop the launch snap -> the mid-draw launch leg.
//  * mix the two key bases component-wise instead of slerping -> the
//    orthonormality leg (a mixed basis shears and shrinks).
//  * lerp stowed straight to shouldered (no pulled key) -> the head/chest
//    clearance leg, which is the defect this rung exists to avoid.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <glm/glm.hpp>

#include "render/sting_deploy.h"

namespace rs = render::sting;

namespace {

// The seated man on a machine at the world origin, facing -Z, up +Y — the
// same local convention render/draw.cpp maps the sled basis onto.
//
// ⚠ MEASURED, NOT QUOTED. The first version of this file certified the pose
// against kSeatedShoulderUpM = 1.20 and z = 0 shoulders — numbers no rider
// in the asset has — and a green suite shipped the collapse defect. These
// come off the indy650 rest skeleton (neck_01 and upperarm_l/r over the
// kernel CG via the mount law): neck (0, 0.657, +0.297), shoulders 0.210 m
// outboard of it, upperarm 0.344 m + forearm 0.269 m.
constexpr double kSeatShoulder =
    rs::kSeatedNeckLocal.y - rs::kShoulderDropM;  // 0.507
constexpr double kShoulderHalfW = 0.210;
constexpr double kArmReachM = 0.613;  // upperarm + forearm, measured

// Run the blend forward at a fixed frame time and return the elapsed seconds
// to reach `target` (or -1 if it never does inside `max_s`).
double time_to(double from, double target, bool shouldered, double dt,
               double max_s) {
    double t = from, el = 0.0;
    rs::DeployIn in;
    in.shouldered = shouldered;
    in.dt = dt;
    while (el < max_s) {
        t = rs::deploy_step(t, in);
        el += dt;
        if (shouldered ? t >= target - 1e-9 : t <= target + 1e-9) return el;
    }
    return -1.0;
}

}  // namespace

// ===========================================================================
// THE TIMER
// ===========================================================================
TEST_CASE("sting deploy: the rise takes the three seconds it claims") {
    const double dt = 1.0 / 120.0;
    const double s = time_to(0.0, 1.0, true, dt, 10.0);
    REQUIRE(s > 0.0);
    // Within one frame of Chad's three seconds. An exponential blend would
    // never return at all here.
    REQUIRE(s == Catch::Approx(rs::kDeployRiseS).margin(dt + 1e-9));
}

TEST_CASE("sting deploy: the rise is monotonic and never overshoots") {
    rs::DeployIn in;
    in.shouldered = true;
    in.dt = 1.0 / 60.0;
    double t = 0.0;
    for (int i = 0; i < 600; ++i) {
        const double n = rs::deploy_step(t, in);
        REQUIRE(n >= t);
        REQUIRE(n <= 1.0);
        t = n;
    }
    REQUIRE(t == Catch::Approx(1.0));
}

TEST_CASE("sting deploy: a voluntary stow eases, a lost stance CUTS") {
    // P-while-shouldered: he puts it away deliberately, and it takes the
    // short stow time — not the three seconds of the draw, and not a frame.
    const double eased = time_to(1.0, 0.0, false, 1.0 / 240.0, 5.0);
    REQUIRE(eased > 0.0);
    REQUIRE(eased == Catch::Approx(rs::kStowFallS).margin(0.01));
    REQUIRE(eased > 2.0 / 240.0);  // it is a blend, not a cut

    // A fall / mount / respawn / X give-up: the hands are gone, so the pose
    // is gone, in ONE step and regardless of dt.
    rs::DeployIn in;
    in.cut = true;
    in.dt = 1.0 / 240.0;
    REQUIRE(rs::deploy_step(1.0, in) == 0.0);
    REQUIRE(rs::deploy_step(0.5, in) == 0.0);
    // And the cut wins even on a frame that is also a launch and also
    // shouldered — the ordering ruling.
    in.launched = true;
    in.shouldered = true;
    REQUIRE(rs::deploy_step(0.5, in) == 0.0);
}

TEST_CASE("sting deploy: a launch mid-draw snaps to full extension") {
    rs::DeployIn in;
    in.shouldered = true;
    in.dt = 1.0 / 60.0;
    double t = 0.0;
    for (int i = 0; i < 30; ++i) t = rs::deploy_step(t, in);  // half a second
    REQUIRE(t < 0.5);
    // He clicks. sting_shouldered drops the same frame the drone goes active,
    // so the launch flag has to beat the stow ramp or the flight hands the
    // machine back with the launcher part way up.
    in.launched = true;
    in.shouldered = false;
    REQUIRE(rs::deploy_step(t, in) == 1.0);
}

TEST_CASE("sting deploy: a zero-length frame moves nothing") {
    rs::DeployIn in;
    in.shouldered = true;
    in.dt = 0.0;
    REQUIRE(rs::deploy_step(0.4, in) == Catch::Approx(0.4));
    in.dt = -1.0;  // a clock that went backwards is not a reason to teleport
    REQUIRE(rs::deploy_step(0.4, in) == Catch::Approx(0.4));
}

// ===========================================================================
// THE EASE AND THE TWO LEGS
// ===========================================================================
TEST_CASE("sting deploy: the ease is a clamped, monotonic 0->1") {
    REQUIRE(rs::ease(0.0f) == Catch::Approx(0.0f));
    REQUIRE(rs::ease(1.0f) == Catch::Approx(1.0f));
    REQUIRE(rs::ease(-3.0f) == Catch::Approx(0.0f));
    REQUIRE(rs::ease(7.0f) == Catch::Approx(1.0f));
    float prev = -1.0f;
    for (int i = 0; i <= 100; ++i) {
        const float v = rs::ease(static_cast<float>(i) / 100.0f);
        REQUIRE(v >= prev);
        prev = v;
    }
    // Eased, not linear: the midpoint is the only place they agree.
    REQUIRE(rs::ease(0.5f) == Catch::Approx(0.5f).margin(1e-5f));
    REQUIRE(rs::ease(0.25f) < 0.25f);
    REQUIRE(rs::ease(0.75f) > 0.75f);
}

TEST_CASE("sting deploy: the two legs are continuous at the pull seam") {
    const rs::KeyBlend a = rs::key_blend(rs::kPullFrac - 1e-6f);
    const rs::KeyBlend b = rs::key_blend(rs::kPullFrac + 1e-6f);
    REQUIRE(a.leg == 0);
    REQUIRE(b.leg == 1);
    // Leg 0 ends ON the pulled key and leg 1 starts ON it: the frame drawn
    // either side of the seam is the same frame.
    REQUIRE(a.w == Catch::Approx(1.0f).margin(1e-4f));
    REQUIRE(b.w == Catch::Approx(0.0f).margin(1e-4f));
    REQUIRE(rs::key_blend(0.0f).w == Catch::Approx(0.0f));
    REQUIRE(rs::key_blend(1.0f).leg == 1);
    REQUIRE(rs::key_blend(1.0f).w == Catch::Approx(1.0f));
}

// ===========================================================================
// THE FRAMES
// ===========================================================================
TEST_CASE("sting deploy: frame_from_aim is orthonormal and fires down -Z") {
    const glm::dvec3 fire = glm::normalize(glm::dvec3(0.3, 0.25, -1.0));
    const glm::dmat3 b = rs::frame_from_aim(fire, glm::dvec3(0.0, 1.0, 0.0));
    for (int c = 0; c < 3; ++c)
        REQUIRE(glm::length(b[c]) == Catch::Approx(1.0).margin(1e-9));
    REQUIRE(glm::dot(b[0], b[1]) == Catch::Approx(0.0).margin(1e-9));
    REQUIRE(glm::dot(b[1], b[2]) == Catch::Approx(0.0).margin(1e-9));
    // The asset's fire direction is -Z, so -column 2 IS the aim.
    REQUIRE(glm::length(-glm::dvec3(b[2]) - fire) ==
            Catch::Approx(0.0).margin(1e-9));
    // Right-handed, so `right` cross `up` is the aft column.
    REQUIRE(glm::length(glm::cross(glm::dvec3(b[0]), glm::dvec3(b[1])) -
                        glm::dvec3(b[2])) == Catch::Approx(0.0).margin(1e-9));
    // Degenerate inputs return the identity rather than a NaN that would
    // poison every vertex of the draw.
    const glm::dmat3 d =
        rs::frame_from_aim(glm::dvec3(0.0), glm::dvec3(0.0, 1.0, 0.0));
    REQUIRE(d[0].x == Catch::Approx(1.0));
    const glm::dmat3 par = rs::frame_from_aim(glm::dvec3(0.0, 1.0, 0.0),
                                              glm::dvec3(0.0, 1.0, 0.0));
    REQUIRE(par[1].y == Catch::Approx(1.0));
}

TEST_CASE("sting deploy: blending two keys stays a rotation") {
    const rs::Frame a = rs::seated_stowed();
    const rs::Frame b = rs::seated_pulled();
    for (int i = 0; i <= 20; ++i) {
        const rs::Frame m = rs::blend_frame(a, b, static_cast<float>(i) / 20.f);
        for (int c = 0; c < 3; ++c)
            REQUIRE(glm::length(m.basis[c]) == Catch::Approx(1.0).margin(1e-6));
        REQUIRE(glm::dot(m.basis[0], m.basis[1]) ==
                Catch::Approx(0.0).margin(1e-6));
        // No shear, no shrink: determinant +1 the whole way across.
        REQUIRE(glm::determinant(m.basis) == Catch::Approx(1.0).margin(1e-6));
    }
    // The ends are the keys themselves.
    REQUIRE(glm::length(rs::blend_frame(a, b, 0.0f).pos - a.pos) ==
            Catch::Approx(0.0).margin(1e-12));
    REQUIRE(glm::length(rs::blend_frame(a, b, 1.0f).pos - b.pos) ==
            Catch::Approx(0.0).margin(1e-12));
}

// ===========================================================================
// THE CLEARANCE — the reason the sweep has a middle key at all
// ===========================================================================
TEST_CASE("sting deploy: the sweep goes around the silhouette, never through") {
    const rs::Stations st;  // nominal until launcher.glb lands
    const glm::dvec3 up(0.0, 1.0, 0.0);
    // Aim straight ahead and a touch up — the P-key's own seed elevation
    // (main.cpp sets sting_aim_el = 0.25), which is the worst case for a
    // muzzle passing his face.
    const glm::dvec3 fire =
        glm::normalize(glm::dvec3(0.0, std::sin(0.25), -std::cos(0.25)));
    rs::Frame shouldered;
    shouldered.basis = rs::frame_from_aim(fire, up);
    // The anchor draw.cpp builds: measured neck (fore/aft term INCLUDED),
    // dropped to the shoulder, out along the aim's right.
    const glm::dvec3 anchor =
        glm::dvec3(rs::kSeatedNeckLocal.x, kSeatShoulder,
                   rs::kSeatedNeckLocal.z) +
        glm::dvec3(shouldered.basis[0]) * rs::kShoulderOutM;
    shouldered.pos = anchor - shouldered.basis * st.shoulder;

    const rs::Frame stowed = rs::seated_stowed();
    const rs::Frame pulled = rs::seated_pulled();

    // His head: a 0.12 m sphere just above the measured neck, and his chest
    // a column below it. Nothing on the launcher's CENTRELINE may enter
    // either at any point of the sweep.
    const glm::dvec3 head(rs::kSeatedNeckLocal.x, rs::kSeatedNeckLocal.y + 0.15,
                          rs::kSeatedNeckLocal.z);
    double closest_head = 1e9, min_x_below_shoulder = 1e9;
    for (int i = 0; i <= 200; ++i) {
        const float raw = static_cast<float>(i) / 200.0f;
        const rs::KeyBlend kb = rs::key_blend(rs::ease(raw));
        const rs::Frame f = kb.leg == 0 ? rs::blend_frame(stowed, pulled, kb.w)
                                        : rs::blend_frame(pulled, shouldered,
                                                          kb.w);
        // Sample the whole tube, buttplate to muzzle.
        for (int k = 0; k <= 20; ++k) {
            const double u = static_cast<double>(k) / 20.0;
            const glm::dvec3 p =
                f.pos + f.basis * (st.shoulder * (1.0 - u) + st.muzzle * u);
            closest_head = std::min(closest_head, glm::length(p - head));
            // The body column: anything at or below shoulder height must
            // stay outboard of his flank.
            if (p.y < kSeatShoulder) min_x_below_shoulder =
                std::min(min_x_below_shoulder, p.x);
        }
    }
    // A helmet is ~0.12 m of radius; a hand's breadth of daylight on top of
    // that is the bar.
    REQUIRE(closest_head > 0.20);
    // ...and the tube never crosses the centreline low, where his chest,
    // his thighs and the windshield are.
    REQUIRE(min_x_below_shoulder > 0.0);
}

// ===========================================================================
// THE LAUNCH ORIGIN — the number the game-loop lane's 1.35 / 1.7 must match
// ===========================================================================
TEST_CASE("sting deploy: the muzzle lands where the launch origin is quoted") {
    const rs::Stations st;
    const glm::dvec3 up(0.0, 1.0, 0.0);
    const auto fire_at = [&](double el) {
        return glm::dvec3(0.0, std::sin(el), -std::cos(el));
    };
    // SEATED, over the machine's frame origin. ⚠ THE FIRST PACKET NUMBER WAS
    // WRONG: this test used to certify 1.24-1.49 m off the invented 1.20 m
    // shoulder; the MEASURED seated shoulder is 0.507 m (riding hunch), so
    // the real band is 0.76-0.86 and the loop lane's 1.35 m dial DOES move
    // (packet §5 addendum; the P block's head_h carries the corrected 0.80).
    const double seat_lo =
        rs::muzzle_height_m(st, kSeatShoulder, fire_at(0.0), up);
    const double seat_hi =
        rs::muzzle_height_m(st, kSeatShoulder, fire_at(0.25), up);
    REQUIRE(seat_lo == Catch::Approx(0.76).margin(0.03));
    REQUIRE(seat_hi == Catch::Approx(0.86).margin(0.03));
    REQUIRE(seat_lo < 0.80);
    REQUIRE(seat_hi > 0.80);
    // AFOOT, over his boots. The standing shoulder (1.55 m) was always the
    // calibrated one; the corrected dial there is 1.85.
    const double foot_lo =
        rs::muzzle_height_m(st, rs::kAfootShoulderUpM, fire_at(0.0), up);
    const double foot_hi =
        rs::muzzle_height_m(st, rs::kAfootShoulderUpM, fire_at(0.25), up);
    REQUIRE(foot_lo < 1.85);
    REQUIRE(foot_hi > 1.85);
    // The muzzle rises with elevation — a launcher whose origin ignored the
    // aim would return the same number twice.
    REQUIRE(seat_hi > seat_lo + 0.10);
}

// ===========================================================================
// THE HANDS — "onto the hand hook to make exception and grasp the stock of
// the launcher" (Chad, 2026-09-04)
// ===========================================================================
//
// MUTATION NOTES (what each leg would catch):
//  * give the two hands the same window -> the "right leads left" leg (he
//    would let go of both bars at once, at speed).
//  * latch a weight instead of computing it from the blend -> the cut leg
//    (weight(0) must be 0, unconditionally and with no memory).
//  * widen the shouldered stations or move the anchor outboard -> the reach
//    leg, which is the one that stops the weld being asked to stretch an arm.
TEST_CASE("sting deploy: the grip weights are pure, clamped 0->1 ramps") {
    REQUIRE(rs::grip_weight_r(0.0f) == Catch::Approx(0.0f));
    REQUIRE(rs::grip_weight_l(0.0f) == Catch::Approx(0.0f));
    REQUIRE(rs::grip_weight_r(1.0f) == Catch::Approx(1.0f));
    REQUIRE(rs::grip_weight_l(1.0f) == Catch::Approx(1.0f));
    // Out of range is clamped, not extrapolated: a weight of 1.4 would drive
    // the hand PAST the grip and out the far side of the launcher.
    REQUIRE(rs::grip_weight_r(-2.0f) == Catch::Approx(0.0f));
    REQUIRE(rs::grip_weight_l(4.0f) == Catch::Approx(1.0f));
    float pr = -1.0f, pl = -1.0f;
    for (int i = 0; i <= 200; ++i) {
        const float t = static_cast<float>(i) / 200.0f;
        const float wr = rs::grip_weight_r(t), wl = rs::grip_weight_l(t);
        REQUIRE(wr >= pr);
        REQUIRE(wl >= pl);
        REQUIRE(wr >= 0.0f);
        REQUIRE(wr <= 1.0f);
        REQUIRE(wl >= 0.0f);
        REQUIRE(wl <= 1.0f);
        pr = wr;
        pl = wl;
    }
}

TEST_CASE("sting deploy: the trigger hand lets go first, the bar hand last") {
    // He pulls the launcher off the tunnel by its pistol grip while his left
    // hand is still driving. The right is fully across before the left has
    // started to move at all — never both bars released at once.
    REQUIRE(rs::kGripRFullT <= rs::kGripLOnT);
    for (int i = 0; i <= 200; ++i) {
        const float t = static_cast<float>(i) / 200.0f;
        REQUIRE(rs::grip_weight_r(t) >= rs::grip_weight_l(t));
    }
    // ...and there is a real window where exactly one hand has let go.
    REQUIRE(rs::grip_weight_r(0.52f) == Catch::Approx(1.0f));
    REQUIRE(rs::grip_weight_l(0.52f) == Catch::Approx(0.0f));
    // Both hands are on it by the time the stock is on his shoulder.
    REQUIRE(rs::grip_weight_r(0.85f) == Catch::Approx(1.0f));
    REQUIRE(rs::grip_weight_l(0.85f) == Catch::Approx(1.0f));
}

TEST_CASE("sting deploy: the cut puts both hands straight back on the bars") {
    // The weights are pure functions of the blend and hold no state, so the
    // one-frame CUT (deploy_step with `cut`) IS the hands going home: the
    // frame after a fall / mount / respawn / X give-up runs them at t = 0.
    rs::DeployIn in;
    in.cut = true;
    in.dt = 1.0 / 240.0;
    const double after = rs::deploy_step(1.0, in);
    REQUIRE(after == 0.0);
    const float te = rs::ease(static_cast<float>(after));
    REQUIRE(rs::grip_weight_r(te) == Catch::Approx(0.0f));
    REQUIRE(rs::grip_weight_l(te) == Catch::Approx(0.0f));
}

TEST_CASE("sting deploy: the shouldered grips are inside a seated man's reach") {
    // THE WELD MUST NEVER BE ASKED TO STRETCH. render/sled_model.cpp
    // substitutes these points for the handlebar sockets and lets the shipped
    // two-bone arm IK bend to them; a target past the chain's length is how a
    // solver goes singular and an arm goes straight and wrong.
    // ⚠ THE STATIONS ARE THE GLB'S (the nominals mirror it digit for digit
    // -- see the Stations struct's banner). The first version of this test
    // solved on invented nominals with a 0.75 m arm and certified a left
    // grip the runtime put 1.05 m from a 0.613 m shoulder.
    const rs::Stations st;
    const glm::dvec3 up(0.0, 1.0, 0.0);
    for (int i = 0; i <= 25; ++i) {
        const double el = 0.25 * static_cast<double>(i) / 25.0;
        const glm::dvec3 fire(0.0, std::sin(el), -std::cos(el));
        rs::Frame sh;
        sh.basis = rs::frame_from_aim(fire, up);
        // The same anchor render/draw.cpp solves the shouldered key onto:
        // measured neck with its fore/aft term, dropped, out to the right.
        const glm::dvec3 shoulder_c(rs::kSeatedNeckLocal.x, kSeatShoulder,
                                    rs::kSeatedNeckLocal.z);
        const glm::dvec3 anchor =
            shoulder_c + glm::dvec3(sh.basis[0]) * rs::kShoulderOutM;
        sh.pos = anchor - sh.basis * st.shoulder;
        const glm::dvec3 gr = sh.pos + sh.basis * st.grip_r;  // right hand
        const glm::dvec3 gl = sh.pos + sh.basis * st.grip_l;  // left hand
        // The MEASURED shoulder joints (upperarm_l/r): half a shoulder width
        // out from the neck on each side. The support hand crossing the body
        // is the reach that actually costs.
        const glm::dvec3 sh_r = shoulder_c + glm::dvec3(kShoulderHalfW, 0, 0);
        const glm::dvec3 sh_l = shoulder_c - glm::dvec3(kShoulderHalfW, 0, 0);
        // The right (trigger) arm must be comfortably BENT, the left may run
        // near-straight (an NLAW support arm does) but must stay inside the
        // chain: the weld's reach clamp is a wound-dressing, not a pose.
        REQUIRE(glm::length(gr - sh_r) < kArmReachM * 0.75);
        REQUIRE(glm::length(gl - sh_l) < kArmReachM * 0.97);
        // And neither grip is INSIDE him: a hand welded to a point behind his
        // own shoulder reads as an arm through his chest.
        REQUIRE(glm::length(gr - sh_r) > 0.10);
        REQUIRE(glm::length(gl - sh_l) > 0.10);
    }
}

// ---------------------------------------------------------------------------
// THE UN-HUNCH (Chad, 2026-09-05: "the sudburian seems hunched over
// unnecessarily"). The rotation itself is applied in render/sled_model.cpp,
// where no ctest can reach it; what IS pure -- and what the eye is worst at
// -- is the bookkeeping: that the thing is exactly inert when stowed, that
// the three spine shares add up to the fraction that was ruled and not to
// something more, and that the sign takes him UP and not further down.
//
// MUTATION NOTES:
//  * key it off anything but the grip weight (a timer of its own) -> the
//    inertness leg cannot be written at all.
//  * drop the sign -> the direction leg (he would fold onto the tank).
//  * re-cut the shares carelessly so they sum to 1.2 -> the total leg.
//  * forget the [0,1] clamp on a weight -> the saturation leg.
// ---------------------------------------------------------------------------
TEST_CASE("sting: the un-hunch is inert while the launcher is stowed") {
    const float hunch = 0.583f;  // 33.4 deg, the MEASURED rest trunk tilt
    for (int k = 0; k < 3; ++k) {
        REQUIRE(rs::sit_up_rad(hunch, 0.0f, k) == 0.0f);
        REQUIRE(rs::sit_up_rad(hunch, -1.0f, k) == 0.0f);
    }
    // ...and a rig with no hunch to take out is the identity at every weight.
    for (int k = 0; k < 3; ++k) REQUIRE(rs::sit_up_rad(0.0f, 1.0f, k) == 0.0f);
}

TEST_CASE("sting: the un-hunch straightens him by the ruled fraction") {
    const float hunch = 0.583f;
    float total = 0.0f;
    for (int k = 0; k < 3; ++k) {
        const float a = rs::sit_up_rad(hunch, 1.0f, k);
        // NEGATIVE about model +X = backward = sitting up. A positive term
        // here would fold him further over the bars.
        REQUIRE(a < 0.0f);
        total += a;
    }
    // The three shares are a SPREAD of one rotation, not three rotations:
    // the trunk above spine_03 ends up exactly kSitUpFrac of the hunch back.
    REQUIRE(total == Catch::Approx(-hunch * rs::kSitUpFrac).epsilon(1e-5));
    // He is straightened, not stood up: a working forward set survives.
    REQUIRE(hunch + total > 0.05f);
    // Lumbar-heaviest, the way a seated man actually straightens.
    REQUIRE(rs::sit_up_rad(hunch, 1.0f, 0) < rs::sit_up_rad(hunch, 1.0f, 1));
    REQUIRE(rs::sit_up_rad(hunch, 1.0f, 1) < rs::sit_up_rad(hunch, 1.0f, 2));
}

TEST_CASE("sting: the un-hunch rises with the grip weight and saturates") {
    const float hunch = 0.583f;
    float prev = 1.0f;
    for (int i = 0; i <= 10; ++i) {
        const float w = static_cast<float>(i) / 10.0f;
        const float a = rs::sit_up_rad(hunch, w, 0);
        REQUIRE(a <= prev);  // monotone downward (more negative) in the weight
        prev = a;
    }
    // A weight past 1 -- a caller's rounding, a future ease that overshoots --
    // must not over-rotate him: the clamp is the pose's own guard rail.
    REQUIRE(rs::sit_up_rad(hunch, 4.0f, 0) ==
            Catch::Approx(rs::sit_up_rad(hunch, 1.0f, 0)));
    // And an index off the end of the spine is 0, never a read past three.
    REQUIRE(rs::sit_up_rad(hunch, 1.0f, 3) == 0.0f);
    REQUIRE(rs::sit_up_rad(hunch, 1.0f, -1) == 0.0f);
}

// ---------------------------------------------------------------------------
// ★★★ ST-5 THE SWEEP (Chad, 2026-09-05, VERBATIM: "the sudburian shall follow
// the aim, up to 180 degree sweep with head and torso as well moving with the
// rpas, they shall twist head and torso in addition to the arms. They should
// only be able to manoeuvre it in a 180 degree sweep while seated on the
// snowmachine, if they want to target an enemy behind them they need to turn
// the snowmachine around.")
//
// Two rulings, two sets of legs. The CLAMP is the whole of the second ruling
// and it is pure; the TWIST's rotation is applied in render/sled_model.cpp
// where no ctest reaches, but its bookkeeping -- shares, sign, inertness,
// monotonicity -- is exactly what the eye cannot audit in a screenshot.
//
// MUTATION NOTES:
//  * clamp the drawn pose instead of the accumulator -> nothing here goes
//    red, which is why the clamp is a pure function main.cpp calls ON the
//    accumulator and the placement is argued at the call site.
//  * drop the negation in twist_rad -> the sign leg (he turns AWAY from what
//    he is aiming at).
//  * re-cut the shares lumbar-heavy (a copy of kSitShare) -> the thoracic
//    leg.
//  * let the shares sum to something other than 1 -> the total leg.
//  * key the twist off the aim alone instead of the grip weight -> the
//    inertness leg (a stowed man twisted at whatever the last aim was).
// ---------------------------------------------------------------------------

TEST_CASE("sting: the seated sweep is 180 degrees and the clamp is a stop") {
    // The window itself: half of his 180, either side of the machine's nose.
    REQUIRE(rs::kSeatedAzMaxRad == Catch::Approx(3.14159265358979 / 2.0));
    // Inside the window nothing is touched -- the clamp must not shave a
    // fraction off every frame's aim (that reads as drift, not as a limit).
    for (double a = -1.5; a <= 1.5; a += 0.1)
        REQUIRE(rs::clamp_seated_az(a) == Catch::Approx(a));
    // Outside it, the STOP. Winding the mouse past the limit parks the
    // accumulator ON the limit rather than letting it run away invisibly --
    // so coming back off the stop costs exactly the mouse it should.
    REQUIRE(rs::clamp_seated_az(2.9) == Catch::Approx(rs::kSeatedAzMaxRad));
    REQUIRE(rs::clamp_seated_az(170.0 * 3.14159265358979 / 180.0) ==
            Catch::Approx(rs::kSeatedAzMaxRad));
    REQUIRE(rs::clamp_seated_az(-2.9) == Catch::Approx(-rs::kSeatedAzMaxRad));
    // IDEMPOTENT, because it is applied EVERY frame and not once on an edge:
    // a clamp that crept would walk the aim off the nose while he held still.
    REQUIRE(rs::clamp_seated_az(rs::clamp_seated_az(9.0)) ==
            Catch::Approx(rs::clamp_seated_az(9.0)));
    // 90 exactly is INSIDE -- the sweep is "up to 180 degrees", so both ends
    // of it have to be reachable and not one epsilon short.
    REQUIRE(rs::clamp_seated_az(rs::kSeatedAzMaxRad) ==
            Catch::Approx(rs::kSeatedAzMaxRad));
    // The float overload is the same stop (main.cpp accumulates in double,
    // the rig channel is a float -- one law, two spellings).
    REQUIRE(rs::clamp_seated_az(3.0f) ==
            Catch::Approx(static_cast<float>(rs::kSeatedAzMaxRad)));
}

TEST_CASE("sting: the twist is inert while the launcher is stowed") {
    const float az = 1.2f, el = 0.4f;
    for (int k = 0; k < rs::kTwistCount; ++k) {
        REQUIRE(rs::twist_rad(az, 0.0f, k) == 0.0f);
        REQUIRE(rs::twist_rad(az, -1.0f, k) == 0.0f);
    }
    REQUIRE(rs::head_el_rad(el, 0.0f) == 0.0f);
    REQUIRE(rs::head_el_rad(el, -1.0f) == 0.0f);
    // And an index off either end of the chain is 0, never a read past five.
    REQUIRE(rs::twist_rad(az, 1.0f, rs::kTwistCount) == 0.0f);
    REQUIRE(rs::twist_rad(az, 1.0f, -1) == 0.0f);
    // A dead-ahead aim leaves him square, at any weight.
    for (int k = 0; k < rs::kTwistCount; ++k)
        REQUIRE(rs::twist_rad(0.0f, 1.0f, k) == 0.0f);
}

TEST_CASE("sting: the twist spends the whole azimuth, thoracic-heavy") {
    const float az = 1.0f;
    float total = 0.0f, share_total = 0.0f;
    for (int k = 0; k < rs::kTwistCount; ++k) {
        total += rs::twist_rad(az, 1.0f, k);
        share_total += rs::kTwistShare[k];
    }
    // The five shares are a SPREAD of ONE rotation: the head ends up facing
    // the aim exactly, not 60 % or 140 % of the way to it.
    REQUIRE(share_total == Catch::Approx(1.0f).epsilon(1e-5));
    REQUIRE(total == Catch::Approx(-az).epsilon(1e-5));
    // THORACIC-HEAVY, the inverse of the un-hunch's lumbar-heavy spread: the
    // lumbar rung is the smallest of the three spine rungs.
    REQUIRE(rs::kTwistShare[rs::kTwSpine1] < rs::kTwistShare[rs::kTwSpine2]);
    REQUIRE(rs::kTwistShare[rs::kTwSpine1] < rs::kTwistShare[rs::kTwSpine3]);
    // ...and it is genuinely the opposite table, not a re-tuned copy of the
    // un-hunch's.
    REQUIRE(rs::kTwistShare[rs::kTwSpine1] < rs::kSitShare[0]);
    REQUIRE(rs::kTwistShare[rs::kTwSpine3] > rs::kSitShare[2]);
    // THE HEAD LEADS. Above the chest -- neck + head -- carries about a third
    // of the sweep, which is what makes him look where the sight is before
    // his ribs get there.
    const float above_chest =
        rs::kTwistShare[rs::kTwNeck] + rs::kTwistShare[rs::kTwHead];
    REQUIRE(above_chest > 0.25f);
    REQUIRE(above_chest < 0.45f);
    // The trunk still does most of the work: this is a man twisting, not a
    // swivel head on a rigid body.
    REQUIRE(above_chest < 1.0f - above_chest);
}

TEST_CASE("sting: the twist follows the aim in sign and magnitude") {
    // ⚠ THE SIGN, and it is the leg the whole rung stands on. `aim_az` is
    // positive to his RIGHT; the return is about MODEL +Y, whose positive
    // sense takes the nose to his LEFT (model +X is his left). So aiming
    // right must return a NEGATIVE rotation at every rung.
    for (int k = 0; k < rs::kTwistCount; ++k) {
        REQUIRE(rs::twist_rad(1.0f, 1.0f, k) < 0.0f);
        REQUIRE(rs::twist_rad(-1.0f, 1.0f, k) > 0.0f);
        // Symmetric: no side of the machine is favoured.
        REQUIRE(rs::twist_rad(-1.0f, 1.0f, k) ==
                Catch::Approx(-rs::twist_rad(1.0f, 1.0f, k)));
    }
    // MONOTONIC IN THE AIM -- a bigger sweep is always more wind-up, so the
    // pose cannot fold back on itself somewhere in the middle of the sweep.
    float prev = 1.0f;
    for (int i = 0; i <= 20; ++i) {
        const float az = static_cast<float>(i) / 20.0f *
                         static_cast<float>(rs::kSeatedAzMaxRad);
        const float a = rs::twist_rad(az, 1.0f, rs::kTwSpine2);
        REQUIRE(a <= prev);
        prev = a;
    }
    // MONOTONIC IN THE WEIGHT, and saturating: the trunk winds up on exactly
    // the schedule the hands leave the bars, and a weight past 1 (a caller's
    // rounding, an ease that overshoots) cannot over-wind him.
    prev = 1.0f;
    for (int i = 0; i <= 10; ++i) {
        const float w = static_cast<float>(i) / 10.0f;
        const float a = rs::twist_rad(1.0f, w, rs::kTwSpine2);
        REQUIRE(a <= prev);
        prev = a;
    }
    REQUIRE(rs::twist_rad(1.0f, 4.0f, rs::kTwSpine2) ==
            Catch::Approx(rs::twist_rad(1.0f, 1.0f, rs::kTwSpine2)));
    // AT THE STOP, the wound-up pose: 90 degrees of aim is 90 degrees of
    // total body twist, spread over five joints so no single one is anywhere
    // near a broken neck.
    float total = 0.0f, worst = 0.0f;
    for (int k = 0; k < rs::kTwistCount; ++k) {
        const float a =
            rs::twist_rad(static_cast<float>(rs::kSeatedAzMaxRad), 1.0f, k);
        total += a;
        worst = std::fmin(worst, a);
    }
    REQUIRE(total == Catch::Approx(-rs::kSeatedAzMaxRad).epsilon(1e-5));
    REQUIRE(std::fabs(worst) < 0.45f);  // < 26 deg at the busiest joint
}

TEST_CASE("sting: the head takes a share of the elevation, looking up") {
    // NEGATIVE about model +X = backward = looking UP, the same sign
    // convention sit_up_rad carries. A positive term here would drop his chin
    // onto his chest as he tracked a target over the treeline.
    REQUIRE(rs::head_el_rad(1.0f, 1.0f) < 0.0f);
    REQUIRE(rs::head_el_rad(-1.0f, 1.0f) > 0.0f);
    REQUIRE(rs::head_el_rad(0.0f, 1.0f) == 0.0f);
    // A SHARE, not the whole of it: the stock is on his shoulder and a head
    // that matched the sight line exactly would leave the cheek weld behind.
    REQUIRE(rs::kHeadElFrac > 0.0f);
    REQUIRE(rs::kHeadElFrac < 1.0f);
    REQUIRE(rs::head_el_rad(1.0f, 1.0f) == Catch::Approx(-rs::kHeadElFrac));
    // At the aim's own elevation ceiling (main.cpp clamps el to 1.40 rad) the
    // head tilt stays inside the head-pitch limit the rig has always had.
    REQUIRE(std::fabs(rs::head_el_rad(1.40f, 1.0f)) < 0.9f);
    // Rises with the grip weight and saturates, like every other term keyed
    // off the hook.
    float prev = 1.0f;
    for (int i = 0; i <= 10; ++i) {
        const float a = rs::head_el_rad(0.8f, static_cast<float>(i) / 10.0f);
        REQUIRE(a <= prev);
        prev = a;
    }
    REQUIRE(rs::head_el_rad(0.8f, 3.0f) ==
            Catch::Approx(rs::head_el_rad(0.8f, 1.0f)));
}
