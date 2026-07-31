// v4 rung 1 (S-aimff) — the aim-rate feedforward: omega_des(pitch, yaw) +=
// aim_ff_gain * (the aim's own mouse-induced angular velocity, body frame),
// so the nose LEADS a moving aim instead of chasing its error. These legs pin
// the repo trap classes for the mechanism:
//   - fixture NON-no-op + kill (the FF fires on a genuinely moving aim, and
//     the baseline pointing term is REQUIRE'd nonzero first);
//   - bit-identity at gain 0 (the gated-add discipline) over a varied
//     closed-loop scenario WITH a nonzero aim_rate_world;
//   - protection ceilings hold WITH the FF (config-relative bounds, never
//     today's constants);
//   - BALLISTIC and OVERRIDE discard the FF by construction;
//   - the app-seam smear invariant (the AT-9 companion): the controller-seen
//     rate integral is partition-independent, driven through app::step_frame
//     (never a re-derivation);
//   - moved-consumer legs for every new forwarded field (app::tick ->
//     control::Input; ClosedLoop -> control::Input);
//   - the aim_ff_tau first-order filter step response + GROUNDED reset.

#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <cstdio>

#include "app/instructor_tick.h"
#include "app/loop.h"
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

bool exact_eq(const glm::dvec3& a, const glm::dvec3& b) {
    return a.x == b.x && a.y == b.y && a.z == b.z;
}
bool exact_eq(const glm::dquat& a, const glm::dquat& b) {
    return a.w == b.w && a.x == b.x && a.y == b.y && a.z == b.z;
}

// The controller's own curvature feedforward, reproduced (the MB-rud test
// pattern) so a pointing/FF assertion can subtract it from omega_des_total.
glm::dvec3 curvature_ff_body(const sim::SimState& s) {
    const glm::dvec3 up = glm::normalize(s.position);
    return sim::body_dir_of(
        s.orientation, glm::cross(up, s.velocity) / glm::length(s.position));
}

// One pure controller step from reset on a level fixture with a target a few
// degrees off the nose (FINE pointing live, deadzone unlatched) and a scripted
// world aim rate.
control::Output one_step(const sim::SimState& s, const glm::dvec3& target,
                         const glm::dvec3& aim_rate,
                         const control::ControllerParams& cp,
                         const control::Internal& internal = control::reset()) {
    control::Input in;
    in.target_dir_world = target;
    in.throttle = 0.7;
    in.aim_moved = true;
    in.aim_rate_world = aim_rate;
    return control::step(s, in, internal, kAp, cp, kAp.sim_dt);
}

}  // namespace

// ---------------------------------------------------------------------------
// Leg 1 — the FF FIRES on a genuinely moving aim, by exactly gain*rate_body,
// and the fixture is NOT a no-op (the baseline pointing term is nonzero
// FIRST). tau = 0 (pass-through) isolates the add from the filter; both cp
// arms are copies of the LOADED table so the fixture tracks every retune.
// ---------------------------------------------------------------------------
TEST_CASE("aim-ff: fires by gain*rate_body on pitch and yaw") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    const sim::SimState s0 =
        harness::level_state(kAp, 140.0, 3000.0, up, heading);
    const glm::dvec3 nose = s0.orientation * glm::dvec3{0.0, 0.0, -1.0};
    const glm::dvec3 right = s0.orientation * glm::dvec3{1.0, 0.0, 0.0};
    const glm::dvec3 up_b = s0.orientation * glm::dvec3{0.0, 1.0, 0.0};

    control::ControllerParams cp_off = kCp;
    cp_off.aim_ff_gain = 0.0;
    cp_off.aim_ff_tau = 0.0;
    control::ControllerParams cp_on = kCp;
    cp_on.aim_ff_gain = 0.6;
    cp_on.aim_ff_tau = 0.0;
    const double r = 0.5;  // [rad/s] scripted aim rate (0.3 rad/s of FF at
                           // gain 0.6 — inside every ceiling at V = 140)

    // PITCH: target 3 deg up (FINE pointing), aim sweeping up about body
    // right (world axis == body right => rate_body = (r, 0, 0)).
    const glm::dvec3 t_up = glm::angleAxis(rad(3.0), right) * nose;
    const control::Output base = one_step(s0, t_up, glm::dvec3{0.0}, cp_off);
    const control::Output ffd = one_step(s0, t_up, r * right, cp_on);
    const glm::dvec3 cff = curvature_ff_body(s0);
    // Fixture NON-no-op: the baseline pointing term itself is well away from
    // zero (the fixture-no-op killer — a ratio against ~0 proves nothing).
    REQUIRE(std::abs(base.telem.omega_des.x - cff.x) > 0.05);
    // The FF adds exactly gain*rate on the pitch demand (same state, same
    // pointing term; nothing here near a clamp).
    CHECK(ffd.telem.omega_des.x - base.telem.omega_des.x ==
          Catch::Approx(cp_on.aim_ff_gain * r).margin(1e-9));
    // KILL: the same moving aim at gain 0 changes NOTHING.
    const control::Output killed = one_step(s0, t_up, r * right, cp_off);
    CHECK(killed.telem.omega_des.x == base.telem.omega_des.x);

    // YAW: target 3 deg right, aim sweeping right about -body_up (rate_body =
    // (0, -r, 0) => yaw-right demand, matching the pointing sign).
    const glm::dvec3 t_rt = glm::angleAxis(-rad(3.0), up_b) * nose;
    const control::Output ybase = one_step(s0, t_rt, glm::dvec3{0.0}, cp_off);
    const control::Output yffd = one_step(s0, t_rt, -r * up_b, cp_on);
    REQUIRE(std::abs(ybase.telem.omega_des.y - cff.y) > 0.05);
    CHECK(yffd.telem.omega_des.y - ybase.telem.omega_des.y ==
          Catch::Approx(-cp_on.aim_ff_gain * r).margin(1e-9));
}

// ---------------------------------------------------------------------------
// Leg 2 — bit-identity at gain 0 over a varied CLOSED-LOOP scenario that
// includes a moving aim AND a nonzero aim_rate_world: trajectory, emitted
// Inputs, and the legacy Internal fields all bit-equal to (a) the same run
// with the field zeroed and (b) by transitivity the build-of-record path
// (aim_rate_filt is the ONLY intentional difference — pure unconsumed state
// at gain 0). The gated-add discipline, executably.
// ---------------------------------------------------------------------------
TEST_CASE("aim-ff: gain 0 is bit-identical with a moving aim and rate") {
    control::ControllerParams cp0 = kCp;
    cp0.aim_ff_gain = 0.0;  // tau stays the loaded value: the filter RUNS,
                            // its state must be unconsumed at gain 0
    // S-rimshot v2 isolation: the universal capture's event emission carries
    // the FULL aim rate (frame-carry, deliberately NOT gated on aim_ff_gain
    // — test_capture's rate-continuity leg pins that), so with the event
    // live this identity is false BY DESIGN. This leg pins the S-aimff GATE
    // alone; carry = 0 is the sanctioned bit-identical isolation arm.
    cp0.capture_carry = 0.0;

    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    double thr = 0.0;
    const sim::SimState s0 =
        harness::level_trim_state(kAp, 140.0, 3000.0, up, heading, &thr);

    auto run = [&](bool with_rate) {
        harness::ClosedLoop cl(s0, glm::dvec3{0.0, 0.0, -1.0});
        cl.aim_nose();
        cl.tick(thr, kAp, cp0, /*grounded=*/true);
        for (int i = 0; i < 500; ++i) {
            // Varied maneuvers: a lateral jut (bank-to-turn), a pull, then a
            // slow continuous sweep — deadzone in and out, blend both sides.
            const glm::dvec3 nose =
                cl.state.orientation * glm::dvec3{0.0, 0.0, -1.0};
            const glm::dvec3 right =
                cl.state.orientation * glm::dvec3{1.0, 0.0, 0.0};
            const glm::dvec3 up_b =
                cl.state.orientation * glm::dvec3{0.0, 1.0, 0.0};
            if (i == 60) cl.aim = glm::angleAxis(-rad(20.0), up_b) * nose;
            if (i == 240) cl.aim = glm::angleAxis(rad(10.0), right) * nose;
            if (i >= 360)  // continuous sweep, the moving-aim phase
                cl.aim = glm::normalize(
                    glm::angleAxis(rad(15.0) * kAp.sim_dt, up_b) * cl.aim);
            cl.aim_moved = i >= 360;
            cl.aim_rate_world = with_rate ? rad(15.0) * up_b : glm::dvec3{0.0};
            cl.tick(thr, kAp, cp0);
        }
        return cl;
    };
    const harness::ClosedLoop a = run(true);
    const harness::ClosedLoop b = run(false);
    CHECK(exact_eq(a.state.position, b.state.position));
    CHECK(exact_eq(a.state.velocity, b.state.velocity));
    CHECK(exact_eq(a.state.orientation, b.state.orientation));
    CHECK(exact_eq(a.state.angular_vel, b.state.angular_vel));
    CHECK(a.last_inputs.pitch == b.last_inputs.pitch);
    CHECK(a.last_inputs.yaw == b.last_inputs.yaw);
    CHECK(a.last_inputs.roll == b.last_inputs.roll);
    CHECK(exact_eq(a.internal.integ, b.internal.integ));
    CHECK(a.internal.held_bank == b.internal.held_bank);
    CHECK(a.internal.deadzoned == b.internal.deadzoned);
    // Premise guard: the rate-bearing run actually carried a nonzero filtered
    // rate (so the identity above proved the GATE, not a dead signal).
    REQUIRE(glm::length(a.internal.aim_rate_filt) > 0.01);
}

// ---------------------------------------------------------------------------
// Leg 3 — the protection ceilings hold WITH a huge FF (config-relative
// bounds, the AT-15 discipline): pitch omega_des stays inside the G/AoA
// envelope; the pointed+FF yaw sum respects the shared yaw_max ceiling
// (probed at beta ~ 0 so the documented coordination exemption contributes
// nothing).
// ---------------------------------------------------------------------------
TEST_CASE("aim-ff: ceilings hold under a saturating aim rate") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    // Low V: the G ceilings are at their tightest reachable scale.
    const sim::SimState s0 =
        harness::level_state(kAp, 60.0, 3000.0, up, heading);
    const glm::dvec3 nose = s0.orientation * glm::dvec3{0.0, 0.0, -1.0};
    const glm::dvec3 right = s0.orientation * glm::dvec3{1.0, 0.0, 0.0};
    const glm::dvec3 up_b = s0.orientation * glm::dvec3{0.0, 1.0, 0.0};
    const control::Extracted e =
        control::extract(s0, glm::dvec3{0.0, 0.0, -1.0}, kAp.v_dir_eps);
    // Config-relative envelope (the cascade's own formulas, from the LOADED
    // table + this state — never today's constants).
    const double V_clamp = std::max(e.speed, kCp.v_min);
    const double w_max = (kCp.n_max - e.cos_phi_theta) * kAp.g / V_clamp;
    const double w_min = (kCp.n_min - e.cos_phi_theta) * kAp.g / V_clamp;
    const glm::dvec3 cff = curvature_ff_body(s0);

    const double huge = 50.0;  // [rad/s] far beyond every ceiling
    // Pitch up AND down, 5 deg error, FF slamming the same way.
    for (const double sgn : {+1.0, -1.0}) {
        const glm::dvec3 t = glm::angleAxis(sgn * rad(5.0), right) * nose;
        const control::Output o = one_step(s0, t, sgn * huge * right, kCp);
        const double pointed_pitch = o.telem.omega_des.x - cff.x;
        CHECK(pointed_pitch <= w_max + 1e-9);
        CHECK(pointed_pitch >= w_min - 1e-9);
    }
    // Yaw: 5 deg right + a huge rightward rate; level_state has velocity
    // along the nose => beta ~ 0, coordination ~ 0, so the sum IS pointed+FF.
    REQUIRE(std::abs(e.beta) < 1e-6);
    const glm::dvec3 t_rt = glm::angleAxis(-rad(5.0), up_b) * nose;
    const control::Output o = one_step(s0, t_rt, -huge * up_b, kCp);
    CHECK(std::abs(o.telem.omega_des.y - cff.y) <= kCp.yaw_max + 1e-9);
    // Premise: the un-clamped sum would have exceeded the ceiling (else this
    // leg is the fixture-no-op).
    REQUIRE(kCp.aim_ff_gain * huge > kCp.yaw_max);
}

// ---------------------------------------------------------------------------
// Leg 3b (red-team P1) — the ceiling's SHAPE, at the corner the goldens can't
// see: with the pointed+FF yaw SATURATED and SAME-direction coordination live
// (beta > 0 => K_coord*(-beta) yaw-right, matching a large rightward error),
// the total demand EXCEEDS yaw_max by exactly the coordination term — the
// documented [rate_clamps] coordination-above-ceiling exemption survives the
// FF clamp (a total-yaw clamp would silently revoke it). Config-relative
// bounds; the coordination term is REQUIRE'd nonzero FIRST (the fixture-no-op
// killer). The beta ~ 0 twin re-asserts the ceiling itself on pointed+FF.
// ---------------------------------------------------------------------------
TEST_CASE("aim-ff: coordination rides above the yaw ceiling under saturation") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    const sim::SimState s_base =
        harness::level_state(kAp, 140.0, 3000.0, up, heading);
    const glm::dvec3 nose = s_base.orientation * glm::dvec3{0.0, 0.0, -1.0};
    const glm::dvec3 up_b = s_base.orientation * glm::dvec3{0.0, 1.0, 0.0};
    // Target 20 deg RIGHT (the leg-1 sign convention) — far outside the
    // deadzone, and err >= coord_center_band so the S-yaw-magnet smoothstep
    // is saturated: coord_scale == 1 (config-relative guard below).
    const glm::dvec3 t_rt = glm::angleAxis(-rad(20.0), up_b) * nose;
    REQUIRE(rad(19.0) > kCp.coord_center_band);

    // Sideslip fixture: velocity 5 deg RIGHT of the nose => beta > 0
    // (SPEC §7), so coordination pushes yaw-right — the SAME way as the
    // pointed error and the FF below.
    sim::SimState s0 = s_base;
    s0.velocity =
        glm::length(s_base.velocity) * (glm::angleAxis(-rad(5.0), up_b) * nose);
    const control::Extracted e = control::extract(s0, nose, kAp.v_dir_eps);
    REQUIRE(e.beta > rad(1.0));  // premise: real sideslip
    const double coord = kCp.K_coord * control::coordination_yaw_demand(e.beta);
    REQUIRE(std::abs(coord) > 1e-3);  // the exemption term is ALIVE
    REQUIRE(coord < 0.0);             // and same-direction (yaw-right)

    control::ControllerParams cp_t = kCp;  // shipped gain, tau = 0 so the
    cp_t.aim_ff_tau = 0.0;                 // saturation premise is exact
    const double huge = 50.0;              // [rad/s]
    REQUIRE(cp_t.aim_ff_gain * huge > kCp.yaw_max);  // FF alone saturates

    // (a) exemption intact: total = coordination + (pointed+FF clamped to
    // -yaw_max) — the demand exceeds the ceiling by ~the coordination term.
    const control::Output o = one_step(s0, t_rt, -huge * up_b, cp_t);
    const glm::dvec3 cff = curvature_ff_body(s0);
    CHECK(o.telem.omega_des.y - cff.y ==
          Catch::Approx(coord - kCp.yaw_max).margin(1e-9));
    CHECK(std::abs(o.telem.omega_des.y - cff.y) > kCp.yaw_max + 1e-4);

    // (b) the ceiling itself holds at beta ~ 0: same target, same saturating
    // FF, velocity along the nose — pointed+FF stays inside yaw_max.
    const control::Extracted e1 =
        control::extract(s_base, glm::dvec3{0.0, 0.0, -1.0}, kAp.v_dir_eps);
    REQUIRE(std::abs(e1.beta) < 1e-6);
    const control::Output o1 = one_step(s_base, t_rt, -huge * up_b, cp_t);
    const glm::dvec3 cff1 = curvature_ff_body(s_base);
    CHECK(std::abs(o1.telem.omega_des.y - cff1.y) <= kCp.yaw_max + 1e-9);
}

// ---------------------------------------------------------------------------
// Leg 3c (red-team P1) — the STILL-MOUSE identity: at the shipped gain with a
// ZERO aim rate, the yaw demand is bit-identical to gain 0 on the same tick —
// the params.h "steady state untouched" contract, executable. The fixture is
// the coordination+pointing corner above (both terms live), where the
// subtract/clamp/re-add shape must still reduce to the legacy value exactly.
// ---------------------------------------------------------------------------
TEST_CASE("aim-ff: still mouse at shipped gain is bit-identical to gain 0") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    sim::SimState s0 = harness::level_state(kAp, 140.0, 3000.0, up, heading);
    const glm::dvec3 nose = s0.orientation * glm::dvec3{0.0, 0.0, -1.0};
    const glm::dvec3 up_b = s0.orientation * glm::dvec3{0.0, 1.0, 0.0};
    s0.velocity =
        glm::length(s0.velocity) * (glm::angleAxis(-rad(5.0), up_b) * nose);
    const glm::dvec3 t_rt = glm::angleAxis(-rad(20.0), up_b) * nose;
    // Premises: the shipped table flies the FF, and BOTH yaw terms are live
    // (coordination + pointing — the corner, not a trivial fixture).
    REQUIRE(kCp.aim_ff_gain > 0.0);
    const control::Extracted e = control::extract(s0, nose, kAp.v_dir_eps);
    REQUIRE(std::abs(kCp.K_coord * control::coordination_yaw_demand(e.beta)) >
            1e-3);

    control::ControllerParams cp_off = kCp;
    cp_off.aim_ff_gain = 0.0;
    const control::Output a = one_step(s0, t_rt, glm::dvec3{0.0}, kCp);
    const control::Output b = one_step(s0, t_rt, glm::dvec3{0.0}, cp_off);
    CHECK(a.telem.omega_des.y == b.telem.omega_des.y);  // bit-identical
    CHECK(a.inputs.yaw == b.inputs.yaw);
}

// ---------------------------------------------------------------------------
// Leg 4 — BALLISTIC: below v_ballistic the FF is absent (the attitude-hold
// branch is untouched; alpha lies in a tail-slide and so would any aim lead).
// ---------------------------------------------------------------------------
TEST_CASE("aim-ff: ballistic attitude-hold ignores the aim rate") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    sim::SimState s0 = harness::level_state(kAp, 140.0, 3000.0, up, heading);
    s0.velocity = 0.5 * kCp.v_ballistic * glm::normalize(s0.velocity);
    const glm::dvec3 nose = s0.orientation * glm::dvec3{0.0, 0.0, -1.0};
    const glm::dvec3 right = s0.orientation * glm::dvec3{1.0, 0.0, 0.0};
    const glm::dvec3 t = glm::angleAxis(rad(10.0), right) * nose;

    const control::Output quiet = one_step(s0, t, glm::dvec3{0.0}, kCp);
    const control::Output rated = one_step(s0, t, 50.0 * right, kCp);
    REQUIRE(quiet.telem.ballistic);  // premise: the fixture IS ballistic
    CHECK(exact_eq(quiet.telem.omega_des, rated.telem.omega_des));
    CHECK(quiet.inputs.pitch == rated.inputs.pitch);
    CHECK(quiet.inputs.yaw == rated.inputs.yaw);
    CHECK(quiet.inputs.roll == rated.inputs.roll);
}

// ---------------------------------------------------------------------------
// Leg 5 — OVERRIDE: a held axis's omega_des is the override ramp value
// exactly; the FF is discarded by the per-axis overwrite (pinned, not
// re-derived). The unheld twin REQUIREs the rate would otherwise have bitten.
// ---------------------------------------------------------------------------
TEST_CASE("aim-ff: a held override axis discards the FF") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    const sim::SimState s0 =
        harness::level_state(kAp, 140.0, 3000.0, up, heading);
    const glm::dvec3 nose = s0.orientation * glm::dvec3{0.0, 0.0, -1.0};
    const glm::dvec3 right = s0.orientation * glm::dvec3{1.0, 0.0, 0.0};
    const glm::dvec3 t = glm::angleAxis(rad(3.0), right) * nose;

    auto held_step = [&](const glm::dvec3& rate) {
        control::Input in;
        in.target_dir_world = t;
        in.throttle = 0.7;
        in.aim_moved = true;
        in.aim_rate_world = rate;
        in.override_mask[0] = true;
        in.override_sign[0] = +1.0;
        return control::step(s0, in, control::reset(), kAp, kCp, kAp.sim_dt);
    };
    const control::Output h0 = held_step(glm::dvec3{0.0});
    const control::Output h1 = held_step(5.0 * right);
    // Held axis: bit-equal with and without the rate (the overwrite wins).
    CHECK(exact_eq(h0.telem.omega_des, h1.telem.omega_des));
    CHECK(h0.inputs.pitch == h1.inputs.pitch);
    // Premise (fixture non-no-op): unheld, the same rate DOES move the demand.
    const control::Output u0 = one_step(s0, t, glm::dvec3{0.0}, kCp);
    const control::Output u1 = one_step(s0, t, 5.0 * right, kCp);
    REQUIRE(u1.telem.omega_des.x != u0.telem.omega_des.x);
}

// ---------------------------------------------------------------------------
// Leg 6 — the smear invariant through the APP SEAM (the AT-9 companion): the
// same total mouse delta as one 4-tick frame vs four 1-tick frames gives the
// SAME controller-seen rate integral (FrameResult::aim_rate_dt_sum). The
// SCALAR per-frame magnitude sum is exact to fp (a pure single-axis delta
// composes exactly); the VECTOR sum matches to the few-tick transport/state
// divergence (the world axes rotate under the diverging closed loop).
// ---------------------------------------------------------------------------
TEST_CASE(
    "aim-ff: smear integral is partition-independent through step_frame") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    double thr = 0.7;
    const sim::SimState s0 =
        harness::level_trim_state(kAp, 150.0, 4000.0, up, heading, &thr);
    const double D = 240.0;  // total mouse dy (pure single axis — exact
                             // composition, header comment)

    struct Part {
        glm::dvec3 vec_sum{0.0};
        double mag_sum = 0.0;
    };
    auto run = [&](int n_frames, double frame_dt, double per_frame_dy) {
        app::LoopState st;
        st.curr = s0;
        st.prev = s0;
        st.prev_up = sim::local_up(s0.position);
        st.aim.reseed(s0.orientation, st.prev_up);
        st.internal = control::reset();
        st.grounded = false;
        app::Accumulator accum(kAp.sim_dt);
        app::FrameInput fin;
        fin.throttle = thr;
        Part p;
        for (int f = 0; f < n_frames; ++f) {
            double pdx = 0.0, pdy = per_frame_dy;
            const app::FrameResult fr =
                app::step_frame(st, accum, frame_dt, fin, pdx, pdy, kAp, kCp);
            REQUIRE(fr.ticks ==
                    static_cast<int>(std::lround(frame_dt / kAp.sim_dt)));
            p.vec_sum += fr.aim_rate_dt_sum;
            p.mag_sum += glm::length(fr.aim_rate_dt_sum);
        }
        return p;
    };
    const Part one = run(1, 4.0 * kAp.sim_dt, D);         // 1 x 4-tick frame
    const Part four = run(4, 1.0 * kAp.sim_dt, D / 4.0);  // 4 x 1-tick frames

    const double expect = D * kCp.aim_sensitivity;  // the total aim rotation
    std::printf(
        "[aim-ff smear] mag one=%.12f four=%.12f expect=%.12f  vec_diff=%.3e\n",
        one.mag_sum, four.mag_sum, expect,
        glm::length(one.vec_sum - four.vec_sum));
    REQUIRE(one.mag_sum > 0.01);  // non-trivially exercised
    // The sharp invariant: the integral MAGNITUDE is the frame rotation,
    // identical across partitions and equal to sens*D (fp-tight).
    CHECK(one.mag_sum == Catch::Approx(expect).epsilon(1e-12));
    CHECK(four.mag_sum == Catch::Approx(expect).epsilon(1e-12));
    // Vector alignment: bounded by ~4 ticks of transport (~8e-5 rad/tick)
    // rotating the world axes between partitions.
    CHECK(glm::length(one.vec_sum - four.vec_sum) < 1e-3 * one.mag_sum);
}

// ---------------------------------------------------------------------------
// Leg 7 — moved-consumer: app::tick forwards the field (mouse applied ->
// controller sees the smeared rate; freelook-held / grounded -> zero), and
// the TickResult report mirrors what control::step saw.
// ---------------------------------------------------------------------------
TEST_CASE("aim-ff: app tick forwards the rate and gates freelook and ground") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    double thr = 0.7;
    const sim::SimState s0 =
        harness::level_trim_state(kAp, 150.0, 4000.0, up, heading, &thr);
    auto fresh = [&]() {
        app::LoopState st;
        st.curr = s0;
        st.prev = s0;
        st.prev_up = sim::local_up(s0.position);
        st.aim.reseed(s0.orientation, st.prev_up);
        st.internal = control::reset();
        st.grounded = false;
        return st;
    };

    // Mouse applied on a declared 2-tick frame: the reported rate is the
    // applied rotation over 2*sim_dt (the smear denominator forwards).
    {
        app::LoopState st = fresh();
        app::TickInput in;
        in.throttle = thr;
        in.aim_dy = 120.0;
        in.frame_ticks = 2;
        const app::TickResult r = app::tick(st, in, kAp, kCp);
        const double expect = 120.0 * kCp.aim_sensitivity / (2.0 * kAp.sim_dt);
        CHECK(glm::length(r.aim_rate_ff) ==
              Catch::Approx(expect).epsilon(1e-9));
    }
    // A non-consuming tick uses the forwarded value verbatim.
    {
        app::LoopState st = fresh();
        app::TickInput in;
        in.throttle = thr;
        in.aim_rate_ff = glm::dvec3{0.1, 0.2, 0.3};
        const app::TickResult r = app::tick(st, in, kAp, kCp);
        CHECK(exact_eq(r.aim_rate_ff, glm::dvec3{0.1, 0.2, 0.3}));
    }
    // Freelook held: the mouse feeds the orbit, the controller sees ZERO
    // (and a forwarded value is discarded too — mouse_aim_live gates both).
    {
        app::LoopState st = fresh();
        app::TickInput in;
        in.throttle = thr;
        in.freelook_held = true;
        in.aim_dy = 120.0;
        in.aim_rate_ff = glm::dvec3{0.1, 0.2, 0.3};
        const app::TickResult r = app::tick(st, in, kAp, kCp);
        CHECK(exact_eq(r.aim_rate_ff, glm::dvec3{0.0}));
    }
    // Grounded spawn tick: zero.
    {
        app::LoopState st = fresh();
        st.grounded = true;
        app::TickInput in;
        in.throttle = thr;
        in.aim_dy = 120.0;
        const app::TickResult r = app::tick(st, in, kAp, kCp);
        CHECK(exact_eq(r.aim_rate_ff, glm::dvec3{0.0}));
    }
}

// ClosedLoop forwards its member into control::Input (the harness arm of the
// moved-consumer pin): with the FF live, a scripted rate moves the demand by
// exactly gain*rate; with freelook held the forward is gated to zero.
TEST_CASE("aim-ff: ClosedLoop forwards its scripted rate") {
    control::ControllerParams cp = kCp;
    cp.aim_ff_gain = 0.5;
    cp.aim_ff_tau = 0.0;  // pass-through: single-tick exactness
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    double thr = 0.0;
    const sim::SimState s0 =
        harness::level_trim_state(kAp, 140.0, 3000.0, up, heading, &thr);

    auto one = [&](const glm::dvec3& rate, bool freelook) {
        harness::ClosedLoop cl(s0, glm::dvec3{0.0, 0.0, -1.0});
        cl.aim_nose();
        const glm::dvec3 nose =
            cl.state.orientation * glm::dvec3{0.0, 0.0, -1.0};
        const glm::dvec3 right =
            cl.state.orientation * glm::dvec3{1.0, 0.0, 0.0};
        cl.aim = glm::angleAxis(rad(3.0), right) * nose;
        cl.aim_moved = true;
        cl.aim_rate_world = rate;
        if (freelook) cl.hold_freelook();
        return cl.tick(thr, kAp, cp);
    };
    const glm::dvec3 right = s0.orientation * glm::dvec3{1.0, 0.0, 0.0};
    const control::Telemetry t0 = one(glm::dvec3{0.0}, false);
    const control::Telemetry t1 = one(0.4 * right, false);
    CHECK(t1.omega_des.x - t0.omega_des.x ==
          Catch::Approx(cp.aim_ff_gain * 0.4).margin(1e-9));
    // Freelook: the forward is CQ2-gated to zero (freelook snaps aim := nose,
    // so compare the freelook pair — rate on vs off — not against t0).
    const control::Telemetry f0 = one(glm::dvec3{0.0}, true);
    const control::Telemetry f1 = one(0.4 * right, true);
    CHECK(f1.omega_des.x == f0.omega_des.x);
}

// ---------------------------------------------------------------------------
// Leg 8 — the aim_ff_tau filter: first-order step response at fixed sim_dt
// (state-carried across steps on a FIXED state — the controller is pure);
// tau = 0 is the exact pass-through; GROUNDED resets the filter state.
// ---------------------------------------------------------------------------
TEST_CASE("aim-ff: tau filter steps first-order and resets on GROUNDED") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    const sim::SimState s0 =
        harness::level_state(kAp, 140.0, 3000.0, up, heading);
    const glm::dvec3 nose = s0.orientation * glm::dvec3{0.0, 0.0, -1.0};
    const glm::dvec3 right = s0.orientation * glm::dvec3{1.0, 0.0, 0.0};
    const glm::dvec3 t = glm::angleAxis(rad(3.0), right) * nose;
    const glm::dvec3 r = 0.4 * right;

    control::ControllerParams cp = kCp;
    cp.aim_ff_tau = 0.05;
    const double lp = std::clamp(kAp.sim_dt / cp.aim_ff_tau, 0.0, 1.0);
    control::Internal it = control::reset();
    for (int k = 1; k <= 12; ++k) {
        const control::Output o = one_step(s0, t, r, cp, it);
        it = o.internal;
        const double expect = 0.4 * (1.0 - std::pow(1.0 - lp, k));
        REQUIRE(glm::length(it.aim_rate_filt - expect * glm::dvec3(right)) <
                1e-12);
    }
    // GROUNDED: the whole Internal reseeds — filter state zeroed.
    control::Input gin;
    gin.target_dir_world = t;
    gin.grounded = true;
    gin.aim_rate_world = r;
    const control::Output g = control::step(s0, gin, it, kAp, cp, kAp.sim_dt);
    CHECK(exact_eq(g.internal.aim_rate_filt, glm::dvec3{0.0}));

    // tau = 0: exact pass-through in one step.
    control::ControllerParams cp0 = kCp;
    cp0.aim_ff_tau = 0.0;
    const control::Output p = one_step(s0, t, r, cp0);
    CHECK(exact_eq(p.internal.aim_rate_filt, r));
}
