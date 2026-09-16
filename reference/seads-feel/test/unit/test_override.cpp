// Section 4c — keyboard override (SPEC §9.5). The pilot seizes an axis and the
// instructor lets go GRACEFULLY: the 80 ms engage ramp is in-core state under
// the tests, the integrator is FROZEN (not zeroed) so trim survives, pursuit of
// the parked cursor is suspended so a keyboard loop can't corkscrew-fight a
// stale aim, coordination stays live OUTSIDE the pointing gate, and on release
// phi_held is recaptured and the braking law shapes the catch (AT-7). Every
// mechanism attacked here is one the SOLUTION §5.5 override branch red-teamed;
// these are its executable pins, closed-loop through the REAL sim::step()
// (HARNESS §3, never a flat stub).
//
// The 4b controller golden (test_controller_golden.cpp) is the complementary
// tripwire: it flies with NO override and must reproduce bit-identically, so
// override is proven a strict superset — off, the pure instructor is untouched.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <cstdio>

#include "config/load_aircraft.h"
#include "config/load_controller.h"
#include "control/controller.h"
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

bool finite_state(const sim::SimState& s) {
    auto ok3 = [](const glm::dvec3& v) {
        return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
    };
    return ok3(s.position) && ok3(s.velocity) && ok3(s.angular_vel) &&
           std::isfinite(s.orientation.w) && std::isfinite(s.orientation.x) &&
           std::isfinite(s.orientation.y) && std::isfinite(s.orientation.z);
}

// Body-axis index into control::Input / Internal (matches the inner loop).
constexpr int kPitch = 0, kYaw = 1;

}  // namespace

// ---------------------------------------------------------------------------
// The engage ramp (SPEC §9.5 S7-ovr / §9.4): a held axis eases its COMMANDED
// RATE (omega_des) to the clamped in-envelope maximum over ovr_ramp_time
// (~80 ms), NOT a hard step. The ramp is in-core (internal.ovr_ramp) and still
// climbs by dt/ovr_ramp_time each tick; what it scales is now the max in-
// envelope rate (S7-ovr), driven through the rate-PI, instead of a raw ±1
// deflection. The override stays inside the -G envelope throughout (the whole
// point of S7-ovr: it carves at the wing's limit, it does not blow past it).
// ---------------------------------------------------------------------------
TEST_CASE(
    "S7-ovr override: engage ramps the commanded rate to the in-envelope max") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    const sim::SimState s0 =
        harness::level_trim_state(kAp, 140.0, 3000.0, up, heading);

    // Hold pitch-DOWN (sign -1): expect a pitch-down commanded rate, ramping.
    harness::ClosedLoop cl(s0, glm::dvec3{0.0, 0.0, -1.0});
    cl.aim_nose();
    cl.hold_override(kPitch, -1.0);

    const double per_tick = kAp.sim_dt / kCp.ovr_ramp_time;
    const int full_by = static_cast<int>(std::ceil(1.0 / per_tick));

    control::Telemetry t = cl.tick(0.7, kAp, kCp);
    // First held tick: the ramp took exactly one step (from rest); the
    // commanded pitch RATE is a small pitch-DOWN value, NOT a hard step to the
    // max, and the emitted deflection is a valid Input (never NaN / out of
    // range).
    CHECK(cl.internal.ovr_ramp[kPitch] == Catch::Approx(per_tick).margin(1e-9));
    CHECK(t.omega_des.x < 0.0);  // pitch-down commanded rate
    CHECK(cl.last_inputs.pitch >= -1.0f);
    CHECK(cl.last_inputs.pitch <= 1.0f);
    const double rate_first = t.omega_des.x;

    for (int i = 1; i < full_by; ++i) {
        t = cl.tick(0.7, kAp, kCp);
        REQUIRE(finite_state(cl.state));
    }
    // By ceil(ramp_time/dt) ticks the ramp saturates: the commanded rate is now
    // the steady in-envelope floor, well past the first-tick fraction.
    CHECK(cl.internal.ovr_ramp[kPitch] == Catch::Approx(1.0));
    CHECK(t.omega_des.x < rate_first);  // ramped to a bigger pitch-down rate
    // In-envelope (S7-ovr): the achieved load factor never dived past the -G
    // floor n_min — a raw ±1 override would blow through it. (One tick of
    // slack.)
    CHECK(t.load_factor >= kCp.n_min - 0.5);
}

// ---------------------------------------------------------------------------
// Freeze, not zero (SPEC §9.5): the overridden axis's integrator is held at its
// entry value — neither integrated nor zeroed — so the trim it carries is there
// when the axis re-enters the cascade on release (zeroing manufactures the
// post-maneuver sag the freeze exists to prevent). The NON-overridden axes keep
// integrating normally.
// ---------------------------------------------------------------------------
TEST_CASE("4c override: overridden axis integrator is frozen, not zeroed") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    const sim::SimState s = harness::flight_state(kAp, 140.0, 3000.0, up,
                                                  heading, 0.0, rad(3.0), 0.0);
    // A known nonzero trim on every axis.
    control::Internal internal = control::reset();
    internal.integ = {0.017, -0.023, 0.011};
    const glm::dvec3 trim = internal.integ;

    control::Input in;
    // Aim 2 deg LATERAL (past the S-yaw-magnet relief band): pursuit is
    // suspended by the held override (pointing zero), so the live yaw demand
    // is the COORDINATION term — which the relief floors to center_frac at
    // err = 0 (0.0 since Rung M1, making an on-nose aim's yaw demand exactly
    // zero and this leg's "not frozen" check vacuous). Past the band the
    // coordination is full and the injected sideslip drives the yaw rate-PI.
    in.target_dir_world =
        glm::normalize(s.orientation *
                       glm::dvec3{std::sin(rad(2.0)), 0.0, -std::cos(rad(2.0))});
    in.throttle = 0.7;
    in.override_mask[kPitch] = true;
    in.override_sign[kPitch] = 1.0;  // full pitch-up

    const control::Output o =
        control::step(s, in, internal, kAp, kCp, nullptr, kAp.sim_dt);
    // Pitch integral FROZEN at trim (bit-exact: it is never touched).
    CHECK(o.internal.integ[kPitch] == trim[kPitch]);
    // The other axes integrated (coordination on the injected sideslip gives
    // yaw a nonzero demand) — not frozen.
    CHECK(o.internal.integ[kYaw] != trim[kYaw]);
    // The pitch Input is the rate-PI driving the override's in-envelope
    // pitch-up rate (S7-ovr), NOT a raw ramp deflection: it commands pitch-UP
    // (> 0).
    CHECK(o.telem.omega_des.x > 0.0);  // commanded pitch-up rate (sign +1)
    CHECK(o.inputs.pitch > 0.0f);      // rate loop drives toward it
}

// ---------------------------------------------------------------------------
// Pursuit suspension keeps coordination LIVE (SPEC §9.5): while any override is
// held, pursuit of the parked cursor is suspended — the pointing terms fall to
// zero exactly as in the deadzone — but yaw coordination lives OUTSIDE the
// pointing gate, so a suspended pursuit cannot kill it. With a big lateral aim
// (would command a hard bank-to-turn roll) AND injected sideslip, holding a
// pitch override must: report pursuit == false, drop the roll pursuit, and
// still command the coordination yaw -K_coord*beta.
// ---------------------------------------------------------------------------
TEST_CASE("4c override: pursuit suspended kills pointing, keeps coordination") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    // beta > 0 (velocity right of nose) -> coordination yaw < 0
    // (-K_coord*beta).
    const double beta = rad(8.0);
    const sim::SimState s =
        harness::flight_state(kAp, 140.0, 3000.0, up, heading, 0.0, beta, 0.0);
    const glm::dvec3 nose = s.orientation * glm::dvec3{0.0, 0.0, -1.0};
    const glm::dvec3 up_b = s.orientation * glm::dvec3{0.0, 1.0, 0.0};
    // A 40 deg RIGHT aim: a live pursuit would command a hard roll (MANEUVER).
    const glm::dvec3 aim =
        glm::normalize(glm::angleAxis(-rad(40.0), up_b) * nose);

    auto run = [&](bool hold_pitch) {
        control::Input in;
        in.target_dir_world = aim;
        in.throttle = 0.7;
        if (hold_pitch) {
            in.override_mask[kPitch] = true;
            in.override_sign[kPitch] = 1.0;
        }
        return control::step(s, in, control::reset(), kAp, kCp, nullptr, kAp.sim_dt);
    };

    const control::Output live = run(false);
    const control::Output held = run(true);

    // Baseline: pursuit live commands a real bank-to-turn roll rate.
    REQUIRE(live.telem.pursuit);
    REQUIRE(std::abs(live.telem.omega_des.z) > 0.5);  // hard roll demand

    // Override held: pursuit suspended, the roll pursuit is gone (only the tiny
    // curvature feedforward on that axis remains), but coordination survives.
    CHECK_FALSE(held.telem.pursuit);
    CHECK(std::abs(held.telem.omega_des.z) < 0.05);  // no bank-to-turn
    // Coordination is still the commanded yaw. It is NOT zeroed by suspension:
    // beta > 0 -> yaw demand < 0. Isolate the coordination(+curvature ff) yaw
    // with a DEADZONE run (aim on the nose, pointing off): the override-held
    // yaw must equal THAT — both carry coordination + the SAME feedforward and
    // no pointing. (Post-S7-yaw the LIVE 40-deg-aim case ALSO yaws toward the
    // aim — yaw pointing survives MANEUVER — so live yaws MORE than held; the
    // old "held == live" premise, that pointing yaw was ~0 at this blend, is
    // gone.)
    // S-yaw-magnet orthogonality: this leg's claim is that GATES don't kill
    // coordination — but the relief legitimately SCALES coordination by err
    // (the deadzone arm is near-center, the held arm is 40 deg off), so both
    // arms run with the relief OFF (frac = 1, its own knob-off identity); the
    // relief's err-dependence has its own S-yaw-magnet legs in test_cascade.
    control::ControllerParams flat = kCp;
    flat.coord_center_frac = 1.0;
    const control::Output held_flat = [&] {
        control::Input in;
        in.target_dir_world = aim;
        in.throttle = 0.7;
        in.override_mask[kPitch] = true;
        in.override_sign[kPitch] = 1.0;
        return control::step(s, in, control::reset(), kAp, flat, nullptr, kAp.sim_dt);
    }();
    control::Input dz_in;
    dz_in.target_dir_world = nose;  // deadzone: pointing off, coordination on
    dz_in.throttle = 0.7;
    const control::Output dz =
        control::step(s, dz_in, control::reset(), kAp, flat, nullptr, kAp.sim_dt);
    CHECK(held_flat.telem.omega_des.y < 0.0);
    CHECK(
        held_flat.telem.omega_des.y ==
        Catch::Approx(dz.telem.omega_des.y).margin(0.02));  // coordination only
    CHECK(std::abs(held.telem.omega_des.y) <
          std::abs(live.telem.omega_des.y));  // live now yaws onto the aim too
}

// ---------------------------------------------------------------------------
// Release recaptures phi_held and resumes pursuit (SPEC §9.5): the instant all
// keys come up, pursuit resumes and phi_held is recaptured at the CURRENT bank
// so the FINE wings-hold and the braking law shape the catch from a truthful
// reference (a stale phi_held would command an uncommanded roll on release).
// ---------------------------------------------------------------------------
TEST_CASE("4c override: release resumes pursuit and recaptures phi_held") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    // Start banked 20 deg; hold a yaw override a while (bank drifts under it),
    // then release and confirm phi_held == the bank AT release, not the stale
    // 0.
    const sim::SimState s0 = harness::flight_state(
        kAp, 140.0, 3000.0, up, heading, rad(20.0), 0.0, 0.0);
    harness::ClosedLoop cl(s0, glm::dvec3{0.0, 0.0, -1.0});
    cl.aim_nose();
    cl.hold_override(kYaw, 1.0);
    for (int i = 0; i < 60; ++i) {  // 0.5 s of held yaw override
        cl.tick(0.7, kAp, kCp);
        REQUIRE(finite_state(cl.state));
        REQUIRE_FALSE(cl.internal.pursuit);  // suspended throughout
    }
    const double bank_at_release = cl.internal.held_bank;  // pre-release value
    cl.release_override();
    const control::Telemetry t = cl.tick(0.7, kAp, kCp);
    // Pursuit resumed and phi_held snapped to the live bank at the release
    // edge.
    CHECK(cl.internal.pursuit);
    CHECK(cl.internal.held_bank ==
          Catch::Approx(t.extracted.phi).margin(1e-12));
    // ...and it actually MOVED off the pre-release held value (the airframe
    // banked while the override was held), proving the recapture is live, not
    // an accident of the start bank.
    CHECK(cl.internal.held_bank != Catch::Approx(bank_at_release));
}

// ---------------------------------------------------------------------------
// AT-7 (S7-ovr2) — keyboard override does NOT move the aim. The mouse owns the
// aim/reticle/camera; the keyboard is a pure supplementary in-envelope nudge.
// The earlier "aim rides the nose" collapsed the aim onto the nose on the
// keypress, which swung the aim-leaning camera back behind the plane (the
// "keypress snaps the view" bug). This pins the fix: while an override is held
// the aim only parallel-transports (never jumps to the nose), the override
// still maneuvers the airframe in-envelope, and on release pursuit resumes
// toward the UNCHANGED held aim. Mutation (re-add aim := nose on override): the
// aim diverges from its pre-override value as the nose pitches off -> aim_drift
// fails.
// ---------------------------------------------------------------------------
TEST_CASE(
    "AT-7 (S7-ovr2): keyboard override does NOT move the aim (no camera "
    "swing)") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    double thr = 0.7;
    const sim::SimState s0 =
        harness::level_trim_state(kAp, 140.0, 3000.0, up, heading, &thr);
    harness::ClosedLoop cl(s0, glm::dvec3{0.0, 0.0, -1.0});
    cl.aim_nose();
    for (int i = 0; i < 120; ++i) cl.tick(thr, kAp, kCp);  // settle 1 s

    auto ang = [](const glm::dvec3& a, const glm::dvec3& b) {
        return std::acos(std::clamp(
            glm::dot(glm::normalize(a), glm::normalize(b)), -1.0, 1.0));
    };
    const glm::dvec3 aim_before = cl.aim;  // the mouse-held aim

    // Full pitch-up override for 1 s. The aim must NOT move (only transport).
    cl.hold_override(kPitch, 1.0);
    control::Telemetry t{};
    double aim_drift_max = 0.0;
    for (int i = 0; i < 120; ++i) {
        t = cl.tick(thr, kAp, kCp);
        REQUIRE(finite_state(cl.state));
        aim_drift_max = std::max(aim_drift_max, ang(cl.aim, aim_before));
    }
    std::printf(
        "[AT-7 S7-ovr2] aim_drift_max=%.4f deg omega_x=%.4f "
        "load_factor=%.3f\n",
        aim_drift_max * 180.0 / kPi, cl.state.angular_vel.x, t.load_factor);
    // The keyboard never hijacked the aim (a re-added aim:=nose would swing it
    // ~the built pointing error; only parallel transport moves it here).
    CHECK(aim_drift_max < rad(1.0));
    // ...yet the override really maneuvered the airframe in-envelope.
    CHECK(std::abs(cl.state.angular_vel.x) > 0.05);
    CHECK(t.load_factor <= kCp.n_max + 0.5);

    // Release: pursuit resumes toward the UNCHANGED held aim (no snap of the
    // aim).
    cl.release_override();
    t = cl.tick(thr, kAp, kCp);
    REQUIRE(finite_state(cl.state));
    CHECK(cl.internal.pursuit);  // pursuit resumed
    CHECK(ang(cl.aim, aim_before) <
          rad(1.0));  // aim still where the mouse left it
}

// ---------------------------------------------------------------------------
// Override RESPECTS AoA/G protection (SPEC §9.5 S7-ovr: keyboard override stays
// IN the envelope). At/above the stall AoA the instructor's mouse-aim path
// clamps pitch DOWN (protection pushback); a full pitch-UP override on the same
// state must ALSO be pushed down — it is driven to its clamped in-envelope rate
// through the same clamp, no longer skipping the outer loop. This INVERTS the
// frozen "bypasses AoA/G" test: a mutation that restored the ±1 leave-the-
// envelope bypass to a held axis would fail here (it would emit +1).
// ---------------------------------------------------------------------------
TEST_CASE(
    "S7-ovr override: a held axis RESPECTS AoA/G protection (in-envelope)") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    const double stall =
        kAp.Cl_max / kAp.Cl_alpha;  // plant stall AoA (~16 deg)
    // At the stall AoA with the filtered AoA already there, the AoA ceiling
    // K_aoa*(aoa_max - aoa_filtered) < 0 -> protection actively commands
    // pitch-DOWN. Aim hard UP so the mouse path WANTS to pitch up into it.
    const sim::SimState s =
        harness::flight_state(kAp, 140.0, 3000.0, up, heading, 0.0, 0.0, stall);
    const glm::dvec3 nose = s.orientation * glm::dvec3{0.0, 0.0, -1.0};
    const glm::dvec3 right = s.orientation * glm::dvec3{1.0, 0.0, 0.0};
    const glm::dvec3 aim = glm::angleAxis(rad(30.0), right) * nose;  // 30 up

    control::Internal base = control::reset();
    base.aoa_filtered = stall;  // protection is primed to bind

    // Mouse path (no override): AoA protection refuses the pitch-up, commanding
    // pitch-DOWN pushback -> Input.pitch < 0 despite the up aim.
    control::Input mouse;
    mouse.target_dir_world = aim;
    mouse.throttle = 0.7;
    const control::Output prot =
        control::step(s, mouse, base, kAp, kCp, nullptr, kAp.sim_dt);
    REQUIRE(prot.telem.omega_des.x < 0.0);  // protection pushed the demand down
    CHECK(prot.inputs.pitch < 0.0f);

    // Override path (full pitch-up, ramp preset to saturation): S7-ovr — the
    // override now OBEYS the same envelope. The AoA pushback clamps the
    // commanded rate to pitch-DOWN, so the override does NOT reach +1; it
    // pushes back exactly like the mouse path (the old "consent to leave the
    // envelope" bypass is gone — a mutation that restored the ±1 bypass fails
    // here).
    control::Internal ovr = base;
    ovr.ovr_ramp = {1.0, 0.0, 0.0};  // ramp already at full
    control::Input keys = mouse;
    keys.override_mask[kPitch] = true;
    keys.override_sign[kPitch] = 1.0;
    const control::Output o = control::step(s, keys, ovr, kAp, kCp, nullptr, kAp.sim_dt);
    CHECK(o.telem.omega_des.x < 0.0);  // in-envelope: AoA pushback, not a pull
    CHECK(o.inputs.pitch < 1.0f);      // NOT full up (bypass gone)
    CHECK(o.inputs.pitch < 0.0f);      // obeys the AoA clamp, like mouse aim
    // MAGNITUDE, not just sign (S7-ovr red-team P2 -> the P0 G-floor fix): the
    // override's commanded pitch rate must EQUAL the mouse path's clamped rate,
    // not a stronger pushback that blows past the -G floor. Before the fix the
    // override commanded ~2.09x the -3G floor at this stall state; a one-side
    // bound (min(w_max, A_c) without the w_min floor) reintroduces that and
    // fails here. Both carry the same feedforward, so equality is exact.
    CHECK(o.telem.omega_des.x ==
          Catch::Approx(prot.telem.omega_des.x).margin(1e-9));
}

// ---------------------------------------------------------------------------
// Override x BALLISTIC (SPEC §9.5 x §9.6): below v_ballistic the outer loop
// hands over to attitude-hold toward the aim — which is itself a form of
// pointing the parked cursor, so a held override must suspend it too, exactly
// as it suspends the cascade. Without the pursuit gate in the ballistic branch,
// the non-held axes would keep auto-pointing while the pilot forces recovery
// (the silent contract violation the 4c red-team surfaced).
// ---------------------------------------------------------------------------
TEST_CASE("4c override: BALLISTIC attitude-hold is suspended under override") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    const double v = 0.5 * kCp.v_ballistic;  // 15 m/s < 30 -> ballistic
    const sim::SimState s =
        harness::flight_state(kAp, v, 3000.0, up, heading, 0.0, 0.0, 0.0);
    const glm::dvec3 nose = s.orientation * glm::dvec3{0.0, 0.0, -1.0};
    const glm::dvec3 up_b = s.orientation * glm::dvec3{0.0, 1.0, 0.0};
    // A 40 deg RIGHT aim: attitude-hold would command a real yaw demand.
    const glm::dvec3 aim =
        glm::normalize(glm::angleAxis(-rad(40.0), up_b) * nose);

    auto run = [&](bool hold_pitch) {
        control::Input in;
        in.target_dir_world = aim;
        in.throttle = 0.0;
        if (hold_pitch) {
            in.override_mask[kPitch] = true;
            in.override_sign[kPitch] = 1.0;
        }
        return control::step(s, in, control::reset(), kAp, kCp, nullptr, kAp.sim_dt);
    };
    const control::Output live = run(false);
    const control::Output held = run(true);

    REQUIRE(live.telem.ballistic);
    REQUIRE(held.telem.ballistic);
    // Live: ballistic attitude-hold commands a real yaw demand toward the aim.
    REQUIRE(live.telem.pursuit);
    REQUIRE(std::abs(live.telem.omega_des.y) > 0.1);
    // Held: pursuit suspended -> the non-held-axis attitude-hold is gone (only
    // the vanishing v->0 feedforward remains).
    CHECK_FALSE(held.telem.pursuit);
    CHECK(std::abs(held.telem.omega_des.y) < 0.02);
}

// ---------------------------------------------------------------------------
// Override x GROUNDED (SPEC §9.5): a grounded tick (spawn/reset/focus-loss)
// while a key is held must reset the override state — ramp to 0, pursuit true —
// and the first airborne tick with the key STILL held must re-fire the engage
// edge cleanly (ramp restarts from 0, pursuit re-suspends), never resume a
// stale mid-ramp. This is the alt-tab-mid-pull / respawn-mid-override path.
// ---------------------------------------------------------------------------
TEST_CASE("4c override: GROUNDED clears the override, re-engages cleanly") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    const sim::SimState s0 =
        harness::level_trim_state(kAp, 140.0, 3000.0, up, heading);
    harness::ClosedLoop cl(s0, glm::dvec3{0.0, 0.0, -1.0});
    cl.aim_nose();
    cl.hold_override(kPitch, 1.0);
    for (int i = 0; i < 20; ++i) cl.tick(0.7, kAp, kCp);  // ramp climbs, held
    REQUIRE(cl.internal.ovr_ramp[kPitch] > 0.0);
    REQUIRE_FALSE(cl.internal.pursuit);

    // A grounded tick with the key still held: state reset in-core.
    cl.tick(0.7, kAp, kCp, /*grounded=*/true);
    CHECK(cl.internal.ovr_ramp[kPitch] == 0.0);
    CHECK(cl.internal.pursuit);
    CHECK_FALSE(cl.internal.any_override);
    CHECK(cl.last_inputs.pitch == 0.0f);  // GROUNDED zeroes Inputs

    // Airborne again, key STILL held: the engage edge re-fires from zero.
    cl.tick(0.7, kAp, kCp);
    CHECK_FALSE(cl.internal.pursuit);  // re-suspended
    CHECK(cl.internal.ovr_ramp[kPitch] ==
          Catch::Approx(kAp.sim_dt / kCp.ovr_ramp_time));  // restarted from 0
}

// ---------------------------------------------------------------------------
// Purity with override state (SPEC §9.7): control::step never mutates its
// `internal` argument even when the override ramp / pursuit / any_override
// fields are populated and a mask is held, and it stays deterministic.
// ---------------------------------------------------------------------------
TEST_CASE("4c override: step is pure with override state populated") {
    const glm::dvec3 up{2.0, 1.0, -3.0}, heading{1.0, -0.5, 0.4};
    const sim::SimState s = harness::flight_state(
        kAp, 135.0, 2500.0, up, heading, rad(18.0), rad(5.0), rad(2.0));
    control::Internal internal = control::reset();
    internal.integ = {0.02, -0.01, 0.03};
    internal.ovr_ramp = {0.4, 0.0, 0.0};
    internal.pursuit = false;
    internal.any_override = true;
    internal.held_bank = rad(12.0);
    const control::Internal before = internal;

    control::Input in;
    in.target_dir_world = glm::normalize(glm::dvec3{0.25, 0.2, -0.9});
    in.throttle = 0.6;
    in.override_mask[kPitch] = true;
    in.override_sign[kPitch] = 1.0;

    const control::Output a =
        control::step(s, in, internal, kAp, kCp, nullptr, kAp.sim_dt);
    CHECK(before.integ == internal.integ);
    CHECK(before.ovr_ramp == internal.ovr_ramp);
    CHECK(before.pursuit == internal.pursuit);
    CHECK(before.any_override == internal.any_override);
    CHECK(before.held_bank == internal.held_bank);

    const control::Output b =
        control::step(s, in, internal, kAp, kCp, nullptr, kAp.sim_dt);
    CHECK(a.inputs.pitch == b.inputs.pitch);
    CHECK(a.inputs.yaw == b.inputs.yaw);
    CHECK(a.inputs.roll == b.inputs.roll);
    CHECK(a.internal.ovr_ramp[kPitch] == b.internal.ovr_ramp[kPitch]);
}
