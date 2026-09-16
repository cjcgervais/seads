// S-orient (docs/comfort program Q3) — the ORIENT verb: double-tap the freelook
// key to "put me back together" behind the flight path with the horizon
// righted. A DISCRETE, player-commanded composed event:
//   1. aim := guarded velocity (the S7-nest rule-3 guard verbatim),
//   2. S7-hrz up-debt capture (open-loop roll, capture-once),
//   3. a reported camera-forward cut (TickResult.orient_fired) the caller
//      hard-seats.
//
// This file pins: (1) the pure input::OrientTap detector's fire-once semantics
// and off-switch; (2) the orient event END-TO-END through app::tick on a
// NON-TRIVIAL fixture (banked, aim off velocity, carried-up rolled > 30° off
// local-up — NOT level flight, the fixture-no-op trap), proving the S7-hrz
// capture is NOT immediately reset (the composition point) and actually retires
// the debt; (3) the ballistic guard (aim lands on the nose, not vhat);
// (4) the firewall (orient_cmd never set => a 600-tick trajectory is
// BIT-IDENTICAL to a baseline whose detector is present-but-unfired); and
// (5) grounded/raw never fire.
//
// Trap classes designed against (CLAUDE.md "Learned"): the fixture-no-op class
// (the end-to-end leg fires on a banked, misaligned, off-velocity path, and
// REQUIRES the pre-fire debt > eps before checking it falls); config-relative
// bounds (the double-tap cadence is DERIVED from cp.orient_double_tap_s, never
// a hardcoded 0.3); the moved-consumer trap (a leg per new field — orient_fired
// on TickResult AND the aim landing spot AND the debt retirement).

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <cstdio>
#include <glm/gtc/quaternion.hpp>

#include "app/instructor_tick.h"
#include "config/load_aircraft.h"
#include "config/load_controller.h"
#include "input/aim_state.h"
#include "sim/world.h"
#include "test/harness/injector.h"
#include "test/harness/instructor.h"

#ifdef NDEBUG
#error "SEADS gate requires an assert-live build (SPEC 6.1)"
#endif

namespace {

const sim::AircraftParams kAp =
    cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");
const control::ControllerParams kCp =
    cfg::load_controller_toml(SEADS_CONFIG_DIR "/controller.toml", kAp);

constexpr double kPi = 3.14159265358979323846;
double rad(double d) { return d * kPi / 180.0; }
double deg(double r) { return r * 180.0 / kPi; }

bool finite3(const glm::dvec3& v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

app::TickInput instr_in(double throttle) {
    app::TickInput in;
    in.throttle = throttle;
    return in;
}

// An airborne, flying LoopState from a harness state.
app::LoopState flying(const sim::SimState& s) {
    app::LoopState st;
    st.curr = s;
    st.prev = s;
    st.prev_up = sim::local_up(s.position);
    st.aim.reseed(s.orientation, st.prev_up);
    st.internal = control::reset();
    st.grounded = false;
    return st;
}

double debt_deg(const app::LoopState& st) {
    return std::abs(st.aim.up_misalignment(sim::local_up(st.curr.position))) *
           180.0 / kPi;
}

}  // namespace

// ===========================================================================
// 1. The pure OrientTap detector.
// ===========================================================================
TEST_CASE("input::OrientTap: double-tap within window fires ONCE") {
    input::OrientTap t;
    const double W = 0.30, dt = 1.0 / 120.0;
    // First press: no fire (opens the pairing).
    CHECK_FALSE(t.step(true, dt, W));
    // A few no-press ticks well inside the window, then the second press: FIRE.
    for (int i = 0; i < 10; ++i) CHECK_FALSE(t.step(false, dt, W));  // ~0.083 s
    CHECK(t.step(true, dt, W));
    // And it consumed: an immediate third press does NOT double-fire (it opens
    // a fresh first tap).
    CHECK_FALSE(t.step(true, dt, W));
}

TEST_CASE("input::OrientTap: tap-pause-tap OUTSIDE the window never fires") {
    input::OrientTap t;
    const double W = 0.30, dt = 1.0 / 120.0;
    CHECK_FALSE(t.step(true, dt, W));  // first tap
    // Age well past the window (0.5 s > 0.30 s) with no press.
    const int past = static_cast<int>(0.5 / dt);
    for (int i = 0; i < past; ++i) CHECK_FALSE(t.step(false, dt, W));
    // The second press is now stale -> no fire; it merely re-opens the pairing.
    CHECK_FALSE(t.step(true, dt, W));
}

TEST_CASE("input::OrientTap: window = 0 NEVER fires (structural off-switch)") {
    input::OrientTap t;
    const double dt = 1.0 / 120.0;
    // Any pattern of presses with window 0 is inert — the strict-superset
    // proof.
    CHECK_FALSE(t.step(true, dt, 0.0));
    CHECK_FALSE(t.step(false, dt, 0.0));
    CHECK_FALSE(t.step(true, dt, 0.0));  // would fire at window 0.30, not here
    CHECK_FALSE(t.step(true, dt, 0.0));
    // The state never accumulated (short-circuit): `since` stayed the sentinel.
    CHECK(t.since >= 1e8);
}

TEST_CASE("input::OrientTap: three FAST presses fire exactly ONCE") {
    input::OrientTap t;
    const double W = 0.30, dt = 1.0 / 120.0;
    int fires = 0;
    // press, gap, press, gap, press — all inside the window. Only the 2nd press
    // pairs with the 1st and fires; the 3rd opens a fresh pairing (consumed).
    if (t.step(true, dt, W)) ++fires;  // tap 1
    for (int i = 0; i < 5; ++i) t.step(false, dt, W);
    if (t.step(true, dt, W)) ++fires;  // tap 2 -> FIRE
    for (int i = 0; i < 5; ++i) t.step(false, dt, W);
    if (t.step(true, dt, W)) ++fires;  // tap 3 -> fresh first tap, no fire
    CHECK(fires == 1);
}

TEST_CASE("input::OrientTap: reset() forgets a pending tap") {
    input::OrientTap t;
    const double W = 0.30, dt = 1.0 / 120.0;
    CHECK_FALSE(t.step(true, dt, W));  // pending first tap
    t.reset();
    // The next press is a FRESH first tap, not a pairing partner.
    CHECK_FALSE(t.step(true, dt, W));
}

// ===========================================================================
// 2. The orient event END-TO-END through app::tick on a NON-TRIVIAL fixture,
//    driven by the REACHABLE input (P1 red-team fix): the fire tick carries
//    freelook_held=TRUE (the second-tap press), THEN freelook releases a few
//    ticks later with NO override — the up-debt is retired by that RELEASE
//    edge, and under v9 (S-nosesnap, b4c0751) it retires INSTANTLY in that
//    same tick (the orient verb itself only snaps the aim + reports the
//    camera cut). Fixture: banked 40°, velocity off the nose (beta+alpha),
//    carried aim-up rolled an extra 60° so the pre-fire debt is far above the
//    5° target — the fixture-no-op trap killer (pre_debt REQUIREd > 20°
//    first). After the fire: aim.forward() == NOSE (v9); after the release:
//    debt < 5° on that very tick.
// ===========================================================================
TEST_CASE(
    "S-orient: end-to-end retires up-debt on a banked, off-velocity path") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    // Banked, sideslipping, at AoA: nose != velocity so the guard has real
    // work.
    const sim::SimState s = harness::flight_state(
        kAp, 180.0, 4000.0, up, heading, rad(40.0), rad(8.0), rad(6.0));
    app::LoopState st = flying(s);
    // Roll the carried aim-up an extra 60° off local-up (the post-maneuver
    // holonomy the pilot carries). Now the fixture is DECISIVELY non-level.
    st.aim.roll_about_forward(rad(60.0));

    const double pre_debt = debt_deg(st);
    REQUIRE(pre_debt > 20.0);  // the fixture actually carries debt (no-op trap)

    // v9 (S-nosesnap, b4c0751): the verb's target is the NOSE. The bases must
    // still separate or the target assertion pins nothing.
    const glm::dvec3 nose = st.curr.orientation * glm::dvec3{0.0, 0.0, -1.0};
    const double spd = glm::length(st.curr.velocity);
    REQUIRE(spd > kCp.v_ballistic);
    const glm::dvec3 vhat = st.curr.velocity / spd;
    REQUIRE(glm::dot(vhat, nose) > 0.0);
    REQUIRE(deg(std::acos(std::clamp(glm::dot(vhat, nose), -1.0, 1.0))) > 3.0);

    // Fire the orient event on the SECOND-TAP PRESS tick: freelook_held is TRUE
    // this tick (the reachable input — a real double-tap fires on a press). The
    // caller's detector already decided; app::tick consumes in.orient_cmd.
    // (The §5b weld also nests aim := nose on this held tick — verb and weld
    // now agree on the one target.)
    app::TickInput fire = instr_in(0.7);
    fire.orient_cmd = true;
    fire.freelook_held = true;  // the second tap is DOWN on the fire tick
    const app::TickResult r = app::tick(st, fire, kAp, kCp, nullptr, nullptr);
    CHECK(r.orient_fired);

    // The aim landed on the NOSE (within a tick of transport), NOT velocity.
    const double aim_nose_deg =
        deg(std::acos(std::clamp(glm::dot(st.aim.forward(), nose), -1.0, 1.0)));
    std::printf("[S-orient e2e] pre_debt=%.1f  aim_vs_nose=%.3f deg\n",
                pre_debt, aim_nose_deg);
    CHECK(aim_nose_deg < 1.0);  // aim := NOSE (v9)
    CHECK(deg(std::acos(
              std::clamp(glm::dot(st.aim.forward(), vhat), -1.0, 1.0))) > 2.0);

    // The debt is still on the frame right after the fire (the orient did NOT
    // itself retire it — the righting rides the release edge below).
    REQUIRE(debt_deg(st) > 20.0);

    // Hold the second tap a couple ticks, then RELEASE freelook with NO
    // override. v9 instant law (b4c0751): the release edge retires the WHOLE
    // debt in that same tick — no open-loop drain, no latch.
    app::TickInput hold_fl = instr_in(0.7);
    hold_fl.freelook_held = true;
    for (int tk = 0; tk < 2; ++tk)
        app::tick(st, hold_fl, kAp, kCp, nullptr, nullptr);

    const app::TickInput release = instr_in(0.7);  // freelook up, no override
    app::tick(st, release, kAp, kCp, nullptr);
    CHECK(st.recov.remaining == 0.0);  // no latch — righted NOW
    CHECK(debt_deg(st) < 5.0);         // up-debt retired ON the release tick

    // And stays retired (transport preserves it on an open leg).
    for (int tk = 0; tk < 120; ++tk) app::tick(st, release, kAp, kCp, nullptr);
    CHECK(debt_deg(st) < 5.0);
    CHECK(st.recov.remaining == 0.0);
}

// ===========================================================================
// 2b. P0-1: the orient fire SURVIVES a 0-tick frame (render fps > sim). The
//    caller (main.cpp) latches the fire in `pending_orient`, OFFERS it to
//    step_frame every frame, and clears it only after a frame that ran >= 1
//    tick. step_frame gates the offer onto the first tick, so a 0-tick frame
//    (fr.ticks==0) delivers nothing and the latch must carry to the next
//    tick-bearing frame, where the orient fires exactly once. This mirrors the
//    AT-9 pending-mouse carry (test_at9.cpp) for the discrete orient event.
// ===========================================================================
TEST_CASE("S-orient: the fire survives a 0-tick frame (caller pending latch)") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    const sim::SimState s = harness::flight_state(
        kAp, 180.0, 4000.0, up, heading, rad(40.0), rad(8.0), rad(6.0));
    app::LoopState st = flying(s);
    st.aim.roll_about_forward(
        rad(60.0));  // carry real debt (no-op-trap killer)
    const double pre_debt = debt_deg(st);
    REQUIRE(pre_debt > 20.0);

    app::Accumulator accum(kAp.sim_dt);
    double pdx = 0.0, pdy = 0.0;
    bool pending_orient = true;  // the caller detected a double-tap this frame

    // Frame 1: a 0-tick frame (half a sim_dt) carrying the fire. step_frame
    // runs 0 ticks, delivers nothing; the caller carries the latch (only clears
    // it when fr.ticks > 0).
    app::FrameInput fin;
    fin.throttle = 0.7;
    fin.orient_cmd = pending_orient;
    app::FrameResult fr = app::step_frame(st, accum, 0.5 * kAp.sim_dt, fin, pdx,
                                          pdy, kAp, kCp, nullptr, nullptr);
    CHECK(fr.ticks == 0);
    CHECK_FALSE(fr.orient_fired);  // nothing delivered on a 0-tick frame
    if (fr.ticks > 0) pending_orient = false;  // the caller's clear rule
    CHECK(pending_orient);  // the fire is still pending (carried), not lost

    // Frame 2: another half sim_dt completes one whole tick. The re-offered
    // fire now lands.
    fin.orient_cmd = pending_orient;
    fr = app::step_frame(st, accum, 0.5 * kAp.sim_dt, fin, pdx, pdy, kAp, kCp,
                         nullptr, nullptr);
    CHECK(fr.ticks == 1);
    CHECK(fr.orient_fired);  // the carried fire delivered on the tick-bearing
                             // frame — NOT lost to the 0-tick frame
    if (fr.ticks > 0) pending_orient = false;
    CHECK_FALSE(pending_orient);  // retired once a tick saw it

    // And it actually did the orient work (not a phantom flag): the aim snapped
    // off the pre-fire carried forward onto the NOSE (v9 S-nosesnap, b4c0751).
    // (The up-debt retirement itself rides the second tap's RELEASE edge —
    // pinned in the end-to-end leg above — not this delivery test.)
    const glm::dvec3 nose = st.curr.orientation * glm::dvec3{0.0, 0.0, -1.0};
    const glm::dvec3 vhat = glm::normalize(st.curr.velocity);
    const double aim_nose_deg =
        deg(std::acos(std::clamp(glm::dot(st.aim.forward(), nose), -1.0, 1.0)));
    REQUIRE(deg(std::acos(std::clamp(glm::dot(vhat, nose), -1.0, 1.0))) >
            3.0);               // nose != velocity (the snap is observable)
    CHECK(aim_nose_deg < 1.0);  // aim landed on the NOSE
    CHECK(deg(std::acos(std::clamp(glm::dot(st.aim.forward(), vhat), -1.0,
                                   1.0))) > 2.0);  // NOT the old velocity
}

// ===========================================================================
// 3. The ballistic guard: below v_ballistic the aim lands on the NOSE, not the
//    velocity (the tail-slide lie, S7-nest F5).
// ===========================================================================
TEST_CASE("S-orient: below ballistic speed the aim lands on the NOSE") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    sim::SimState s = harness::level_state(kAp, 150.0, 4000.0, up, heading);
    // Force a sub-ballistic speed with velocity pointing well OFF the nose, so
    // "nose" and "velocity" are unambiguously different targets.
    const glm::dvec3 nose = s.orientation * glm::dvec3{0.0, 0.0, -1.0};
    const glm::dvec3 side = s.orientation * glm::dvec3{1.0, 0.0, 0.0};
    const double vslow = 0.5 * kCp.v_ballistic;
    s.velocity = vslow * glm::normalize(0.5 * nose + side);  // 45°+ off nose
    s.last_vhat = glm::normalize(s.velocity);
    app::LoopState st = flying(s);
    st.aim.roll_about_forward(rad(50.0));  // carry some debt too

    app::TickInput fire = instr_in(0.7);
    fire.orient_cmd = true;
    const app::TickResult r = app::tick(st, fire, kAp, kCp, nullptr, nullptr);
    CHECK(r.orient_fired);

    const glm::dvec3 nose_now =
        st.curr.orientation * glm::dvec3{0.0, 0.0, -1.0};
    const glm::dvec3 vhat_now = glm::normalize(st.curr.velocity);
    const double to_nose = deg(
        std::acos(std::clamp(glm::dot(st.aim.forward(), nose_now), -1.0, 1.0)));
    const double to_vhat = deg(
        std::acos(std::clamp(glm::dot(st.aim.forward(), vhat_now), -1.0, 1.0)));
    std::printf("[S-orient ballistic] aim->nose=%.2f  aim->vhat=%.2f deg\n",
                to_nose, to_vhat);
    CHECK(to_nose < 2.0);   // guard fell back to the nose
    CHECK(to_vhat > 20.0);  // NOT the (lying) velocity
}

// ===========================================================================
// 4. Firewall — orient_cmd never set => a 600-tick trajectory is BIT-IDENTICAL
//    to a baseline whose OrientTap detector is present-but-unfired. Two
//    LoopStates side by side: `base` never touches orient; `cand` runs the
//    detector every tick (single taps, never a double) and forwards its
//    (always-false) fire to in.orient_cmd. If the detector's presence perturbs
//    the tick even once, the states diverge.
// ===========================================================================
TEST_CASE("S-orient: unfired detector is bit-identical over 600 ticks") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    double thr = 0.7;
    const sim::SimState s0 =
        harness::level_trim_state(kAp, 150.0, 4000.0, up, heading, &thr);

    app::LoopState base = flying(s0);
    app::LoopState cand = flying(s0);
    input::OrientTap tap;
    bool prev_fl = false;

    for (int i = 0; i < 600; ++i) {
        app::tick(base, instr_in(thr), kAp, kCp, nullptr, nullptr);

        // The candidate runs the live detector: a lone freelook press every
        // ~50 ticks (a SINGLE tap, never paired within the window), so the
        // detector churns state but NEVER fires. Its (false) result flows to
        // orient_cmd.
        const bool fl = (i % 50 == 0);
        app::TickInput in = instr_in(thr);
        // The shipped window is the RETIRED 0 (pilot ruling 2026-08-06 - the
        // double-tap is redundant now that every release fires the verb), and
        // 0 short-circuits the detector before it touches any state. Pin a
        // live window here so the detector really CHURNS across this firewall
        // (reading the shipped dial would make the leg vacuous).
        in.orient_cmd = tap.step(fl && !prev_fl, kAp.sim_dt, 0.30);
        prev_fl = fl;
        in.freelook_held = false;      // keep the aim path identical to base
        REQUIRE_FALSE(in.orient_cmd);  // it must never have fired
        app::tick(cand, in, kAp, kCp, nullptr, nullptr);
    }
    const double pos_err = glm::length(base.curr.position - cand.curr.position);
    const glm::dquat& qb = base.curr.orientation;
    const glm::dquat& qc = cand.curr.orientation;
    std::printf(
        "[S-orient firewall] pos_err=%.17e  dq=(%.17e,%.17e,%.17e,%.17e)\n",
        pos_err, qb.w - qc.w, qb.x - qc.x, qb.y - qc.y, qb.z - qc.z);
    // pos_err == 0 is the bit-identical trajectory (the strict-superset proof):
    // an unfired detector perturbs the tick by NOTHING. P2a red-team fix: pin
    // the orientation COMPONENT-WISE exact, not a loose dot bound. base and
    // cand run the IDENTICAL shared app::tick with orient_cmd always false, so
    // there is no separately-compiled call site here (unlike the
    // mirror-equivalence test's TWO composers) — the trajectory is truly
    // bit-identical and exact
    // `==` is the honest firewall (a loose |dot|>1-1e-12 would tolerate a real
    // per-tick perturbation the detector's presence could introduce).
    CHECK(pos_err == 0.0);
    CHECK(qb.w == qc.w);
    CHECK(qb.x == qc.x);
    CHECK(qb.y == qc.y);
    CHECK(qb.z == qc.z);
    CHECK(finite3(cand.curr.position));
}

// ===========================================================================
// 5. Grounded / raw never fire the orient event.
// ===========================================================================
TEST_CASE("S-orient: a grounded tick never fires") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    const sim::SimState s =
        harness::level_state(kAp, 150.0, 4000.0, up, heading);
    app::LoopState st = flying(s);
    st.grounded = true;  // a spawn/reset tick
    st.aim.roll_about_forward(rad(40.0));

    app::TickInput fire = instr_in(0.7);
    fire.orient_cmd = true;
    const app::TickResult r = app::tick(st, fire, kAp, kCp, nullptr, nullptr);
    CHECK_FALSE(r.orient_fired);  // suppressed on the grounded tick
    CHECK(st.recov.remaining == 0.0);
}

TEST_CASE("S-orient: raw mode never fires (the event is instructor-only)") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    const sim::SimState s =
        harness::level_state(kAp, 150.0, 4000.0, up, heading);
    app::LoopState st = flying(s);

    app::TickInput fire = instr_in(0.7);
    fire.raw_mode = true;
    fire.orient_cmd = true;
    const app::TickResult r = app::tick(st, fire, kAp, kCp, nullptr, nullptr);
    CHECK_FALSE(
        r.orient_fired);  // the orient block lives in the instructor arm
}

// ===========================================================================
// 6. Override suppression: an override held on the orient tick suppresses the
//    fire (the §5b nested aim := nose would fight the velocity snap — the D9
//    precedent). Documented in instructor_tick.h.
//
//    S-relorient ADDENDUM (2026-07-28): this is now the SEALED-V6 (knob-OFF)
//    arm and pins the knob explicitly — the shipped toml sets
//    release_orient_with_keys = true, which RETIRES this suppression so the
//    double-tap is truly redundant with the release. The knob-ON arm is
//    test_relorient.cpp case 11 ("the double-tap fires with a key held").
// ===========================================================================
TEST_CASE("S-orient: an override held on the orient tick suppresses the fire") {
    control::ControllerParams cp_v6 = kCp;
    cp_v6.freelook_release_orient_with_keys = false;
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    const sim::SimState s =
        harness::level_state(kAp, 150.0, 4000.0, up, heading);
    app::LoopState st = flying(s);
    st.aim.roll_about_forward(rad(40.0));

    app::TickInput fire = instr_in(0.7);
    fire.orient_cmd = true;
    fire.override_mask[2] = true;  // roll-axis keyboard jink held
    fire.override_sign[2] = 1.0;
    const app::TickResult r = app::tick(st, fire, kAp, cp_v6, nullptr, nullptr);
    CHECK_FALSE(r.orient_fired);       // suppressed while any override is held
    CHECK(st.recov.remaining == 0.0);  // no capture either
}
