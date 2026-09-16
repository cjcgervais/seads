// S-leanlead instrument (feel/yaw-bank-balance, 2026-09-10) -- the yaw/bank
// BALANCE of a FINE lateral turn entry, as a NUMBER before it is a feeling.
//
// Chad's read: "there is more yaw early on than banking" in the cascade from
// a mouse aim deflection. This file measures it. A lateral aim STEP is
// applied to a level-trim closed loop (the REAL spherical sim::step, HARNESS
// s3) and two clocks are read off the trajectory:
//   t50_nose -- when the nose's horizontal swing has closed HALF the step
//   t50_bank -- when the bank has reached HALF its eventual peak
// and the bank's share at the moment the nose is half-way (phi(t50_nose) /
// phi_peak). Legacy (lean_lead = 0): the bank arrives through two series
// lags (held_bank at auto_level_rate, then K_phi) while the rudder pointing
// is one -- t50_bank lands AFTER t50_nose. With the lead the wings-hold limb
// targets the live lean on the way in, so the two clocks close up.
//
// Pins (the mechanism, not the numbers -- the numbers are printed for the
// fly card and for Chad's "correct me if wrong"):
//   1. on the FINE steps the lead brings t50_bank EARLIER and does not erode
//      the peak bank (a lead that only shifted the peak later or smaller
//      would be a different mechanism);
//   2. the dial is FINE/band-scoped: at blend == 1 (a committed lateral aim
//      at/above blend_hi) the first-tick roll demand is BIT-identical;
//   3. the rest decay is untouched: a banked capture with the aim on the
//      nose (the AT-13 banked-spawn shape) and a lean RELEASE (held bank
//      above the live lean) emit BIT-identical roll demands, tick for tick.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <cstdio>
#include <vector>

#include "config/load_aircraft.h"
#include "config/load_controller.h"
#include "control/controller.h"
#include "sim/world.h"
#include "test/harness/injector.h"
#include "test/harness/instructor.h"

#ifdef NDEBUG
#error \
    "SEADS gate requires an assert-live build (SPEC 6.1); configure with CMAKE_BUILD_TYPE=Debug"
#endif

namespace {

const sim::AircraftParams kAp =
    cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");
const control::ControllerParams kCp =
    cfg::load_controller_toml(SEADS_CONFIG_DIR "/controller.toml", kAp);

constexpr double kPi = 3.14159265358979323846;
double rad(double d) { return d * kPi / 180.0; }
double deg(double r) { return r * 180.0 / kPi; }

struct Clocks {
    double t50_nose = -1.0;          // [s] nose swing reaches 50% of the step
    double t50_bank = -1.0;          // [s] |bank| reaches 50% of its peak
    double phi_peak = 0.0;           // [rad] peak |bank| in the window
    double share_at_nose50 = 0.0;    // phi(t50_nose)/phi_peak
    double yaw_int_at_nose50 = 0.0;  // [rad] integrated body yaw at t50_nose
    double phi_at_nose50 = 0.0;      // [rad]
};

// One lateral-step probe: level trim, aim `a` right of the nose about body
// up (the golden's convention), fly `secs`. Returns the two clocks.
Clocks lateral_step(const control::ControllerParams& cp, double a, double secs,
                    bool print) {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    double thr = 0.7;
    const sim::SimState s0 =
        harness::level_trim_state(kAp, 140.0, 3000.0, up, heading, &thr);
    harness::ClosedLoop cl(s0, glm::dvec3{0.0, 0.0, -1.0});
    cl.aim_nose();
    cl.tick(thr, kAp, cp, /*grounded=*/true);
    const glm::dvec3 nose0 = cl.state.orientation * glm::dvec3{0, 0, -1};
    const glm::dvec3 up_b = cl.state.orientation * glm::dvec3{0, 1, 0};
    const glm::dvec3 up_w = sim::local_up(cl.state.position);
    cl.aim = glm::normalize(glm::angleAxis(-a, up_b) * nose0);
    // Horizontal reference: nose0 projected on the local horizon.
    const glm::dvec3 h0 = glm::normalize(nose0 - up_w * glm::dot(nose0, up_w));

    const int n = static_cast<int>(secs / kAp.sim_dt + 0.5);
    std::vector<double> t(n), swing(n), phi(n), yaw_int(n);
    double yi = 0.0;
    for (int i = 0; i < n; ++i) {
        cl.tick(thr, kAp, cp);
        const sim::SimState& s = cl.state;
        const glm::dvec3 nose = s.orientation * glm::dvec3{0, 0, -1};
        const glm::dvec3 right = s.orientation * glm::dvec3{1, 0, 0};
        const glm::dvec3 h = glm::normalize(nose - up_w * glm::dot(nose, up_w));
        // signed swing about up_w, RIGHT positive (matches the aim's -a
        // about body up == a clockwise-from-above yaw).
        const double sw =
            std::atan2(glm::dot(glm::cross(h0, h), -up_w), glm::dot(h0, h));
        yi += s.angular_vel.y * kAp.sim_dt;  // body yaw rate, up-positive
        t[i] = (i + 1) * kAp.sim_dt;
        swing[i] = sw;
        phi[i] = -std::asin(glm::dot(right, up_w));  // SPEC s7: + right down
        yaw_int[i] = yi;
    }
    Clocks c;
    for (int i = 0; i < n; ++i)
        c.phi_peak = std::max(c.phi_peak, std::abs(phi[i]));
    for (int i = 0; i < n; ++i) {
        if (c.t50_nose < 0.0 && swing[i] >= 0.5 * a) {
            c.t50_nose = t[i];
            c.phi_at_nose50 = phi[i];
            c.yaw_int_at_nose50 = yaw_int[i];
        }
        if (c.t50_bank < 0.0 && std::abs(phi[i]) >= 0.5 * c.phi_peak) {
            c.t50_bank = t[i];
        }
    }
    c.share_at_nose50 =
        (c.phi_peak > 0.0) ? std::abs(c.phi_at_nose50) / c.phi_peak : 0.0;
    if (print) {
        std::printf(
            "[yawbank] step %5.1f deg lean_lead %.2f | t50_nose %.3f s  "
            "t50_bank %.3f s  phi_peak %5.2f deg  phi@nose50 %5.2f deg "
            "(share %.2f)  body-yaw-int@nose50 %5.2f deg\n",
            deg(a), cp.lean_lead, c.t50_nose, c.t50_bank, deg(c.phi_peak),
            deg(c.phi_at_nose50), c.share_at_nose50, deg(-c.yaw_int_at_nose50));
        // 25 ms trace of the first half second for the fly card.
        std::printf("[yawbank]   t(s)  swing(deg)  phi(deg)\n");
        for (int i = 0; i < n; ++i) {
            if ((i + 1) % 3 != 0 || t[i] > 0.6) continue;
            std::printf("[yawbank]   %.3f  %6.2f  %6.2f\n", t[i], deg(swing[i]),
                        deg(phi[i]));
        }
    }
    return c;
}

// S-leanlead-lateral instrument (2026-09-12). A PURE PITCH input from a
// trimmed, modestly BANKED start -- the shape Chad reported as "nosing down
// often makes my wings bank when unwanted... pulling a straight loop my wings
// are completely unwantedly banking over 180 degrees".
//
// The aim is placed `eps` straight ABOVE the nose about the body RIGHT axis:
// zero lateral mouse, the pilot is only pulling. az_lat's de-roll numerator
// is target_body.x*cosPhiTheta + target_body.y*sin(e.phi), and a pure body-up
// offset has target_body.x == 0, so it collapses to sin(eps)*sin(e.phi):
//     az_lat ~= eps*sin(phi),  share |az_lat|/err ~= |sin(phi)|
// i.e. the LEAN reads a pure pull as a lateral aim the moment the wings are
// off level, and the lead feeds that straight to the ailerons. This probe
// reports the peak |bank| the pull induces over `secs`.
//
// The start is trimmed level, then BOTH the orientation and the velocity are
// rolled about the nose by bank0 -- so body-frame alpha/beta (hence the trim)
// are untouched and only the bank differs from level_trim_state.
struct PitchProbe {
    double phi0 = 0.0;       // [rad] bank at t=0
    double phi_peak = 0.0;   // [rad] peak |bank| over the window
    double phi_end = 0.0;    // [rad] |bank| at the end
    double share_max = 0.0;  // max |az_lat|/err seen (the gate's input)
    double err_min = 0.0, err_max = 0.0;  // [rad] latch-band evidence
    double blend_max = 0.0;
    double cos_pt_min = 1.0;
    bool any_deadzone = false, any_ballistic = false, any_righting = false;
};

PitchProbe pure_pitch(const control::ControllerParams& cp, double bank0,
                      double eps, double secs) {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    double thr = 0.7;
    sim::SimState s0 =
        harness::level_trim_state(kAp, 140.0, 3000.0, up, heading, &thr);
    // Roll the whole trimmed state about its own nose: bank without
    // disturbing the trim (body-frame velocity is carried with it).
    const glm::dvec3 nose0w = s0.orientation * glm::dvec3{0, 0, -1};
    const glm::dquat roll = glm::angleAxis(bank0, nose0w);
    s0.orientation = glm::normalize(roll * s0.orientation);
    s0.velocity = roll * s0.velocity;
    s0.last_vhat = roll * s0.last_vhat;

    harness::ClosedLoop cl(s0, glm::dvec3{0.0, 0.0, -1.0});
    cl.aim_nose();
    cl.tick(thr, kAp, cp, /*grounded=*/true);  // held_bank := the current bank

    // PURE PITCH, SUSTAINED: the aim is re-placed eps above the CURRENT nose
    // every tick -- the hand HELD deflected straight up, which is what
    // "pulling a straight loop" is. (A one-shot world aim lets the nose catch
    // up and the probe decays into the deadzone, grading nothing -- the
    // vacuous-probe trap.) aim_moved is set for the same reason: the hand IS
    // moving, so the rest-dwell deadzone latch must never arm.
    const glm::dvec3 nose = cl.state.orientation * glm::dvec3{0, 0, -1};
    const glm::dvec3 right_b = cl.state.orientation * glm::dvec3{1, 0, 0};
    cl.aim = glm::normalize(glm::angleAxis(eps, right_b) * nose);
    cl.aim_moved = true;

    PitchProbe pr;
    {
        const glm::dvec3 r = cl.state.orientation * glm::dvec3{1, 0, 0};
        pr.phi0 = -std::asin(glm::dot(r, sim::local_up(cl.state.position)));
    }
    pr.err_min = 1e9;
    const int n = static_cast<int>(secs / kAp.sim_dt + 0.5);
    for (int i = 0; i < n; ++i) {
        // Hold the pull: the aim stays eps above the nose, zero lateral.
        const glm::dvec3 nw = cl.state.orientation * glm::dvec3{0, 0, -1};
        const glm::dvec3 rw = cl.state.orientation * glm::dvec3{1, 0, 0};
        cl.aim = glm::normalize(glm::angleAxis(eps, rw) * nw);
        const control::Telemetry tm = cl.tick(thr, kAp, cp);
        const sim::SimState& st = cl.state;
        const glm::dvec3 r = st.orientation * glm::dvec3{1, 0, 0};
        const double phi =
            -std::asin(glm::dot(r, sim::local_up(st.position)));
        pr.phi_peak = std::max(pr.phi_peak, std::abs(phi));
        pr.phi_end = std::abs(phi);
        // The gate's own input, recomputed here from the SAME two extract
        // fields the cascade uses (never a second formula for az_lat itself:
        // this is the share, and it is what the probe is grading).
        if (tm.e > 0.0) {
            pr.share_max = std::max(pr.share_max, std::abs(std::sin(phi)));
        }
        pr.err_min = std::min(pr.err_min, tm.e);
        pr.err_max = std::max(pr.err_max, tm.e);
        pr.blend_max = std::max(pr.blend_max, tm.blend);
        pr.cos_pt_min = std::min(pr.cos_pt_min, tm.extracted.cos_phi_theta);
        pr.any_deadzone = pr.any_deadzone || tm.deadzoned;
        pr.any_ballistic = pr.any_ballistic || tm.ballistic;
        pr.any_righting = pr.any_righting || tm.righting;
    }
    return pr;
}

}  // namespace

TEST_CASE("S-leanlead instrument: yaw/bank balance of a FINE lateral step") {
    control::ControllerParams legacy = kCp;
    legacy.lean_lead = 0.0;
    control::ControllerParams lead = kCp;
    // Config-relative (red-team P1): a walk-back to lean_lead = 0 must not
    // red this instrument -- it measures the MECHANISM at a live value.
    if (lead.lean_lead <= 0.0) lead.lean_lead = 0.3;
    lead.lean_lead_lateral = false;  // the landed-v14 ungated lead
    // S-leanlead-lateral: the SHIPPED arm (gate ON). A genuinely sideways aim
    // carries essentially all of the pointing error on the horizon-lateral
    // axis (share ~ 1), so the gate must be ~1 here and the turn-entry feel
    // the dial was built for must survive it -- pinned below.
    control::ControllerParams lead_gated = lead;
    lead_gated.lean_lead_lateral = true;

    // Steps: two inside FINE (below blend_lo), one in the band, one committed.
    const double steps[] = {rad(2.0), rad(4.0), rad(7.0), rad(12.0)};
    for (const double a : steps) {
        const Clocks cl_legacy = lateral_step(legacy, a, 2.0, true);
        const Clocks cl_lead = lateral_step(lead, a, 2.0, true);
        const Clocks cl_gated = lateral_step(lead_gated, a, 2.0, false);
        REQUIRE(cl_legacy.t50_nose > 0.0);
        REQUIRE(cl_lead.t50_nose > 0.0);
        REQUIRE(cl_legacy.t50_bank > 0.0);
        REQUIRE(cl_lead.t50_bank > 0.0);
        REQUIRE(cl_gated.t50_nose > 0.0);
        REQUIRE(cl_gated.t50_bank > 0.0);
        std::printf(
            "[yawbank] step %4.1f: bank-vs-nose clock ratio "
            "t50_bank/t50_nose legacy %.2f -> lead %.2f -> lead+gate %.2f\n",
            deg(a), cl_legacy.t50_bank / cl_legacy.t50_nose,
            cl_lead.t50_bank / cl_lead.t50_nose,
            cl_gated.t50_bank / cl_gated.t50_nose);
        std::printf(
            "[yawbank] step %4.1f: bank share at nose50  lead %.3f -> "
            "lead+gate %.3f | peak bank %5.2f -> %5.2f deg\n",
            deg(a), cl_lead.share_at_nose50, cl_gated.share_at_nose50,
            deg(cl_lead.phi_peak), deg(cl_gated.phi_peak));
        // S-leanlead-lateral pin: the TURN ENTRY is preserved. A sideways aim
        // reads share ~ 1, so the gate is ~1 and every number the dial was
        // shipped on must stay within a few percent of the ungated arm. If a
        // later edge retune closes the gate on a genuine lateral entry, this
        // is what fails -- not the fly.
        CHECK(cl_gated.t50_bank ==
              Catch::Approx(cl_lead.t50_bank).epsilon(0.05));
        CHECK(cl_gated.share_at_nose50 ==
              Catch::Approx(cl_lead.share_at_nose50).epsilon(0.05));
        CHECK(cl_gated.phi_peak == Catch::Approx(cl_lead.phi_peak).epsilon(0.05));
        if (a < kCp.blend_lo) {
            // Pin 1: the lead brings the bank EARLIER on FINE steps and does
            // not erode the peak (>= 90% of legacy's).
            CHECK(cl_lead.t50_bank < cl_legacy.t50_bank);
            CHECK(cl_lead.phi_peak >= 0.9 * cl_legacy.phi_peak);
            CHECK(cl_lead.share_at_nose50 > cl_legacy.share_at_nose50);
        }
    }
}

TEST_CASE("S-leanlead: committed aim (blend == 1) is bit-untouched") {
    // Pin 2: open-loop on a fixed level state with the aim at blend_hi + a
    // margin -- blend is EXACTLY 1, the wings-hold limb has weight 0 and the
    // lead branch is skipped: the roll demand must be the same double.
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    const sim::SimState s =
        harness::level_state(kAp, 140.0, 3000.0, up, heading);
    const glm::dvec3 nose = s.orientation * glm::dvec3{0, 0, -1};
    const glm::dvec3 up_b = s.orientation * glm::dvec3{0, 1, 0};
    control::Input in;
    in.target_dir_world =
        glm::normalize(glm::angleAxis(-(kCp.blend_hi + rad(3.0)), up_b) * nose);
    in.throttle = 0.7;
    control::ControllerParams legacy = kCp;
    legacy.lean_lead = 0.0;
    control::Internal ia = control::reset(), ib = control::reset();
    for (int i = 0; i < 60; ++i) {
        const control::Output oa =
            control::step(s, in, ia, kAp, legacy, nullptr, kAp.sim_dt);
        const control::Output ob =
            control::step(s, in, ib, kAp, kCp, nullptr, kAp.sim_dt);
        REQUIRE(oa.telem.blend == 1.0);
        REQUIRE(oa.telem.omega_des.z == ob.telem.omega_des.z);  // EXACT
        REQUIRE(oa.telem.omega_des.y == ob.telem.omega_des.y);
        ia = oa.internal;
        ib = ob.internal;
    }
}

TEST_CASE("S-leanlead: rest decay and lean release are bit-untouched") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    control::ControllerParams legacy = kCp;
    legacy.lean_lead = 0.0;

    SECTION("banked capture, aim on the nose (the AT-13 banked-spawn shape)") {
        // Pin 3a: lean_target == 0 exactly (az_lat = 0 on the nose), so
        // d*lean_t == 0 and hold_target IS held_bank -- the decay tree is
        // legacy's, tick for tick.
        const sim::SimState s = harness::flight_state(
            kAp, 140.0, 3000.0, up, heading, rad(30.0), 0.0, 0.0);
        const glm::dvec3 nose = s.orientation * glm::dvec3{0, 0, -1};
        control::Input in;
        in.target_dir_world = nose;
        in.throttle = 0.7;
        control::Internal ia = control::reset(), ib = control::reset();
        ia.held_bank = rad(30.0);
        ib.held_bank = rad(30.0);
        for (int i = 0; i < 200; ++i) {
            const control::Output oa =
                control::step(s, in, ia, kAp, legacy, nullptr, kAp.sim_dt);
            const control::Output ob =
                control::step(s, in, ib, kAp, kCp, nullptr, kAp.sim_dt);
            REQUIRE(oa.telem.omega_des.z == ob.telem.omega_des.z);  // EXACT
            REQUIRE(oa.internal.held_bank == ob.internal.held_bank);
            ia = oa.internal;
            ib = ob.internal;
        }
    }

    SECTION("lean release: held bank ABOVE the live lean decays as flown") {
        // Pin 3b: aim 1 deg right (lean_t = lean_gain*1 deg < held 25 deg,
        // same sign) -> d < 0 while lean_t > 0 -> d*lean_t < 0 -> no lead.
        const double a = rad(1.0);
        REQUIRE(kCp.lean_gain * a < rad(25.0));  // premise: releasing
        const sim::SimState s = harness::flight_state(
            kAp, 140.0, 3000.0, up, heading, rad(25.0), 0.0, 0.0);
        const glm::dvec3 nose = s.orientation * glm::dvec3{0, 0, -1};
        const glm::dvec3 up_b = s.orientation * glm::dvec3{0, 1, 0};
        control::Input in;
        in.target_dir_world = glm::normalize(glm::angleAxis(-a, up_b) * nose);
        in.throttle = 0.7;
        control::Internal ia = control::reset(), ib = control::reset();
        ia.held_bank = rad(25.0);
        ib.held_bank = rad(25.0);
        for (int i = 0; i < 200; ++i) {
            const control::Output oa =
                control::step(s, in, ia, kAp, legacy, nullptr, kAp.sim_dt);
            const control::Output ob =
                control::step(s, in, ib, kAp, kCp, nullptr, kAp.sim_dt);
            REQUIRE(oa.telem.omega_des.z == ob.telem.omega_des.z);  // EXACT
            ia = oa.internal;
            ib = ob.internal;
        }
    }

    SECTION("continuity across az_lat = 0 with an OPPOSITE-sign carry") {
        // Red-team P0 of the first cut: a MANEUVER->FINE capture leaves
        // held_bank at the old bank (-20 deg here) while the hand dithers
        // the aim through the nose. The first predicate (d*lean_t > 0 on
        // d = lean_t - held_bank) fired for ANY opposite-sign lean, so the
        // hold target STEPPED by lean_lead*|held_bank| across az_lat = 0.
        // The same-sign-excess form leads by lean_t alone: the roll demand
        // at +eps and -eps aim must differ by no more than the lean's own
        // (tiny) contribution -- K_phi * lean_lead * lean_gain * 2 eps --
        // times 1.5 slack, well under the old K_phi*lean_lead*20 deg step.
        // OUTSIDE the deadzone latch (deadzone_lo 0.03 deg -- a 0.02 deg
        // probe latched deadzoned and made this pin VACUOUS on first cut,
        // the fixture-no-op class) and well inside FINE.
        const double eps = rad(0.3);
        REQUIRE(eps > 2.0 * kCp.deadzone_hi);
        REQUIRE(eps < 0.2 * kCp.blend_lo);
        const sim::SimState s =
            harness::level_state(kAp, 140.0, 3000.0, up, heading);
        const glm::dvec3 nose = s.orientation * glm::dvec3{0, 0, -1};
        const glm::dvec3 up_b = s.orientation * glm::dvec3{0, 1, 0};
        double wz[2];
        for (int k = 0; k < 2; ++k) {
            const double a = (k == 0) ? eps : -eps;
            control::Input in;
            in.target_dir_world =
                glm::normalize(glm::angleAxis(-a, up_b) * nose);
            in.throttle = 0.7;
            control::Internal ii = control::reset();
            ii.held_bank = rad(-20.0);
            const control::Output o =
                control::step(s, in, ii, kAp, kCp, nullptr, kAp.sim_dt);
            wz[k] = o.telem.omega_des.z;
        }
        const double lead_v = (kCp.lean_lead > 0.0) ? kCp.lean_lead : 0.3;
        const double bound =
            1.5 * kCp.K_phi * lead_v * kCp.lean_gain * 2.0 * eps + 1e-9;
        const double old_step = kCp.K_phi * lead_v * rad(20.0);
        std::printf("[yawbank] continuity: |dwz| %.6f rad/s (bound %.6f, "
                    "first-cut step %.4f)\n",
                    std::abs(wz[0] - wz[1]), bound, old_step);
        REQUIRE(bound < 0.5 * old_step);  // premise: the pin can see it
        CHECK(std::abs(wz[0] - wz[1]) < bound);
        // Mutation killed (RUN): restore d = lean_t - held_bank with the
        // d*lean_t > 0 predicate -> |dwz| ~ old_step (0.52 rad/s at 0.3).
    }

    SECTION("fresh lateral aim from level: the lead FIRES (mutation arm)") {
        // The positive control for 3a/3b: same fixture family, level wings,
        // aim 2 deg right -> lean_t > 0 = d -> the lead is live and the
        // first-tick roll demand differs (rolls right = -omega_z, harder).
        const sim::SimState s =
            harness::level_state(kAp, 140.0, 3000.0, up, heading);
        const glm::dvec3 nose = s.orientation * glm::dvec3{0, 0, -1};
        const glm::dvec3 up_b = s.orientation * glm::dvec3{0, 1, 0};
        control::Input in;
        in.target_dir_world =
            glm::normalize(glm::angleAxis(-rad(2.0), up_b) * nose);
        in.throttle = 0.7;
        control::Internal ia = control::reset(), ib = control::reset();
        const control::Output oa =
            control::step(s, in, ia, kAp, legacy, nullptr, kAp.sim_dt);
        const control::Output ob =
            control::step(s, in, ib, kAp, kCp, nullptr, kAp.sim_dt);
        CHECK(ob.telem.omega_des.z < oa.telem.omega_des.z);
        CHECK(ob.telem.omega_des.z < 0.0);
    }
}

TEST_CASE("S-leanlead-lateral instrument: a PURE PITCH pull must not bank") {
    // Chad's loop regression, as a number. A trimmed 10 deg banked start and
    // the aim HELD 8 deg straight above the nose -- zero lateral mouse, the
    // pilot is only pulling -- flown 2.5 s closed loop (~150 deg of a loop).
    // Three arms:
    //   lean_lead 0    -- the pre-v14 tree, the reference "no lead" bank
    //   0.3, gate OFF  -- the landed v14 Chad rejected
    //   0.3, gate ON   -- shipped
    //
    // OPERATING POINT, derived not guessed. The lean's own feedback on a pure
    // pull is phi <- lean_gain*eps*sin(phi), so wings-level only goes unstable
    // past eps > 1/lean_gain = 1/8 rad = 7.2 deg -- and the lead limb needs
    // blend < 1, i.e. eps < blend_hi = 9 deg. The window where a pure pull can
    // bank the wings AT ALL is therefore 7.2..9 deg, and 8 deg sits in it with
    // margin either side. Below ~7 deg all three arms are identical and the
    // bank simply decays (measured) -- a probe there would be vacuous.
    // 2.5 s keeps the whole run UPRIGHT (cos_phi_theta stays ~0.2, clear of
    // wings_level_band), so it is the wings-hold limb being graded and not
    // S7-loop-invert's fade.
    const double bank0 = rad(10.0);
    const double eps = rad(8.0);
    const double secs = 2.5;

    control::ControllerParams none = kCp;
    none.lean_lead = 0.0;
    control::ControllerParams v14 = kCp;
    if (v14.lean_lead <= 0.0) v14.lean_lead = 0.3;
    v14.lean_lead_lateral = false;
    control::ControllerParams gated = v14;
    gated.lean_lead_lateral = true;

    const PitchProbe p_none = pure_pitch(none, bank0, eps, secs);
    const PitchProbe p_v14 = pure_pitch(v14, bank0, eps, secs);
    const PitchProbe p_gate = pure_pitch(gated, bank0, eps, secs);

    std::printf(
        "[leanlat] PURE PITCH %.1f deg up, held, from %.1f deg bank, %.1f s\n",
        deg(eps), deg(bank0), secs);
    std::printf(
        "[leanlat]   peak |phi|   lean_lead 0 %6.2f | 0.3 gate OFF %6.2f | "
        "0.3 gate ON %6.2f  deg\n",
        deg(p_none.phi_peak), deg(p_v14.phi_peak), deg(p_gate.phi_peak));
    std::printf(
        "[leanlat]   UNWANTED bank (peak - start)  %+6.2f | %+6.2f | %+6.2f  "
        "deg   (gate OFF adds %.0f%%)\n",
        deg(p_none.phi_peak - std::abs(p_none.phi0)),
        deg(p_v14.phi_peak - std::abs(p_v14.phi0)),
        deg(p_gate.phi_peak - std::abs(p_gate.phi0)),
        100.0 * ((p_v14.phi_peak - std::abs(p_v14.phi0)) /
                     (p_none.phi_peak - std::abs(p_none.phi0)) -
                 1.0));
    std::printf(
        "[leanlat]   bands: err %.2f..%.2f deg (blend_lo %.1f blend_hi %.1f "
        "dz_hi %.3f) blend_max %.2f cosPT_min %.2f share_max %.3f (lat_lo "
        "%.2f) | dz %d ball %d right %d\n",
        deg(p_v14.err_min), deg(p_v14.err_max), deg(kCp.blend_lo),
        deg(kCp.blend_hi), deg(kCp.deadzone_hi), p_v14.blend_max,
        p_v14.cos_pt_min, p_v14.share_max, kCp.lean_lead_lat_lo,
        int(p_v14.any_deadzone), int(p_v14.any_ballistic),
        int(p_v14.any_righting));

    // OUTSIDE EVERY LATCH BAND -- the probe must grade the lean limb and
    // nothing else (the "vacuous probe inside the deadzone latch" trap).
    CHECK_FALSE(p_v14.any_deadzone);
    CHECK_FALSE(p_v14.any_ballistic);
    CHECK_FALSE(p_v14.any_righting);
    CHECK(p_v14.err_min > kCp.deadzone_hi);
    CHECK(p_v14.blend_max < 1.0);  // the wings-hold limb still has weight
    CHECK(p_v14.cos_pt_min > kCp.wings_level_band);  // upright the whole run
    // The gate's input is in its OFF region here (share == |sin(phi)| for a
    // pure-pitch aim), which is WHY the ON arm must land on the no-lead one.
    CHECK(p_gate.share_max < kCp.lean_lead_lat_lo);

    // THE REGRESSION. The landed v14 banks the wings measurably harder on a
    // pull that asked for no bank at all; if this ever stops being true the
    // probe has drifted off the mechanism and every pin below is vacuous, so
    // the MARGIN is pinned, not just the sign.
    CHECK(p_v14.phi_peak > p_none.phi_peak);
    CHECK((p_v14.phi_peak - std::abs(p_v14.phi0)) >
          1.2 * (p_none.phi_peak - std::abs(p_none.phi0)));

    // THE FIX. share < lat_lo => smoothstep is EXACTLY 0 => lead is exactly
    // 0.0 => the branch is skipped => the gated arm is the lean_lead 0 tree
    // BIT-IDENTICALLY, not merely close. Pinned tick for tick below.
    CHECK(p_gate.phi_peak == p_none.phi_peak);
    CHECK(p_gate.phi_end == p_none.phi_end);
}

TEST_CASE("S-leanlead-lateral: a pure-pitch pull is the lean_lead 0 tree") {
    // The bit-identical half of the instrument above, tick for tick: on the
    // pure-pitch pull the gate is EXACTLY 0, so shipped (0.3 + gate ON) and
    // the pre-v14 kernel (lean_lead 0) must emit the same doubles. This is
    // what makes "gate ON sits within noise of lean_lead 0" an equality.
    const double bank0 = rad(10.0), eps = rad(8.0);
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    double thr = 0.7;
    sim::SimState s0 =
        harness::level_trim_state(kAp, 140.0, 3000.0, up, heading, &thr);
    const glm::dvec3 n0 = s0.orientation * glm::dvec3{0, 0, -1};
    const glm::dquat roll = glm::angleAxis(bank0, n0);
    s0.orientation = glm::normalize(roll * s0.orientation);
    s0.velocity = roll * s0.velocity;
    s0.last_vhat = roll * s0.last_vhat;

    control::ControllerParams none = kCp;
    none.lean_lead = 0.0;
    control::ControllerParams gated = kCp;
    if (gated.lean_lead <= 0.0) gated.lean_lead = 0.3;
    gated.lean_lead_lateral = true;

    harness::ClosedLoop ca(s0, glm::dvec3{0.0, 0.0, -1.0});
    harness::ClosedLoop cb(s0, glm::dvec3{0.0, 0.0, -1.0});
    ca.aim_nose();
    cb.aim_nose();
    ca.aim_moved = true;
    cb.aim_moved = true;
    ca.tick(thr, kAp, none, /*grounded=*/true);
    cb.tick(thr, kAp, gated, /*grounded=*/true);
    for (int i = 0; i < 600; ++i) {  // 2.5 s
        const glm::dvec3 na = ca.state.orientation * glm::dvec3{0, 0, -1};
        const glm::dvec3 ra = ca.state.orientation * glm::dvec3{1, 0, 0};
        ca.aim = glm::normalize(glm::angleAxis(eps, ra) * na);
        const glm::dvec3 nb = cb.state.orientation * glm::dvec3{0, 0, -1};
        const glm::dvec3 rb = cb.state.orientation * glm::dvec3{1, 0, 0};
        cb.aim = glm::normalize(glm::angleAxis(eps, rb) * nb);
        const control::Telemetry ta = ca.tick(thr, kAp, none);
        const control::Telemetry tb = cb.tick(thr, kAp, gated);
        REQUIRE(ta.omega_des.z == tb.omega_des.z);  // EXACT
        REQUIRE(ta.held_bank == tb.held_bank);
        REQUIRE(ca.state.orientation.w == cb.state.orientation.w);
    }
}

TEST_CASE("S-leanlead-lateral: OFF is the landed v14 tree, bit-identically") {
    // The gate is a STRUCTURAL off-switch, not a value: with the flag false
    // the edges must be inert, so the shipped edges and absurd ones emit the
    // SAME doubles tick for tick. That is what pins the OFF arm == v14.
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    double thr = 0.7;
    const sim::SimState s0 =
        harness::level_trim_state(kAp, 140.0, 3000.0, up, heading, &thr);

    control::ControllerParams a = kCp;
    if (a.lean_lead <= 0.0) a.lean_lead = 0.3;
    a.lean_lead_lateral = false;
    control::ControllerParams b = a;
    b.lean_lead_lat_lo = 0.0;  // would make the gate ~= the raw share
    b.lean_lead_lat_hi = 1.0;  // (i.e. anything BUT 1) if it were live

    harness::ClosedLoop ca(s0, glm::dvec3{0.0, 0.0, -1.0});
    harness::ClosedLoop cb(s0, glm::dvec3{0.0, 0.0, -1.0});
    ca.aim_nose();
    cb.aim_nose();
    ca.tick(thr, kAp, a, /*grounded=*/true);
    cb.tick(thr, kAp, b, /*grounded=*/true);
    const glm::dvec3 nose = ca.state.orientation * glm::dvec3{0, 0, -1};
    const glm::dvec3 up_b = ca.state.orientation * glm::dvec3{0, 1, 0};
    // A 3 deg lateral aim: inside FINE, the lead limb live, share ~ 1 -- so
    // a LIVE gate would not be 1.0 under b's edges and the arms would fork.
    ca.aim = glm::normalize(glm::angleAxis(-rad(3.0), up_b) * nose);
    cb.aim = ca.aim;
    for (int i = 0; i < 480; ++i) {  // 2 s
        const control::Telemetry ta = ca.tick(thr, kAp, a);
        const control::Telemetry tb = cb.tick(thr, kAp, b);
        REQUIRE(ta.omega_des.z == tb.omega_des.z);  // EXACT
        REQUIRE(ta.held_bank == tb.held_bank);
        REQUIRE(ca.state.orientation.w == cb.state.orientation.w);
    }
}

TEST_CASE("S-leanlead-lateral: lean_lead 0 is bit-identical with the gate ON") {
    // The walk-back arm. lean_lead == 0 never enters the branch, so NEITHER
    // new key can reach the tree -- the pre-v14 kernel is recovered exactly
    // whatever the gate says. Flown on a lateral step (the lead's own shape)
    // and on the pure-pitch shape.
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    double thr = 0.7;
    const sim::SimState s0 =
        harness::level_trim_state(kAp, 140.0, 3000.0, up, heading, &thr);

    control::ControllerParams off = kCp;
    off.lean_lead = 0.0;
    off.lean_lead_lateral = false;
    control::ControllerParams on = off;
    on.lean_lead_lateral = true;
    on.lean_lead_lat_lo = 0.10;  // deliberately wide-open edges: if the gate
    on.lean_lead_lat_hi = 0.20;  // could reach the tree at all, it would here

    harness::ClosedLoop ca(s0, glm::dvec3{0.0, 0.0, -1.0});
    harness::ClosedLoop cb(s0, glm::dvec3{0.0, 0.0, -1.0});
    ca.aim_nose();
    cb.aim_nose();
    ca.tick(thr, kAp, off, /*grounded=*/true);
    cb.tick(thr, kAp, on, /*grounded=*/true);
    const glm::dvec3 nose = ca.state.orientation * glm::dvec3{0, 0, -1};
    const glm::dvec3 up_b = ca.state.orientation * glm::dvec3{0, 1, 0};
    const glm::dvec3 right_b = ca.state.orientation * glm::dvec3{1, 0, 0};
    SECTION("lateral aim") {
        ca.aim = glm::normalize(glm::angleAxis(-rad(3.0), up_b) * nose);
        cb.aim = ca.aim;
    }
    SECTION("pure pitch aim") {
        ca.aim = glm::normalize(glm::angleAxis(rad(4.0), right_b) * nose);
        cb.aim = ca.aim;
    }
    for (int i = 0; i < 480; ++i) {  // 2 s
        const control::Telemetry ta = ca.tick(thr, kAp, off);
        const control::Telemetry tb = cb.tick(thr, kAp, on);
        REQUIRE(ta.omega_des.z == tb.omega_des.z);  // EXACT
        REQUIRE(ta.held_bank == tb.held_bank);
        REQUIRE(ca.state.orientation.w == cb.state.orientation.w);
    }
}
