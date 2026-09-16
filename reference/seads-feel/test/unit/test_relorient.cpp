// S-relorient (Chad 2026-07-28: the orient verb "should be automatic upon
// release of the spacebar") — EVERY airborne, no-override freelook release
// fires the ORIENT composition: aim := guarded velocity + the reported camera
// hard-cut (TickResult.orient_fired), exactly the flown double-tap, automatic.
//
// Mechanism placement (plan-audit P1/P2): the snap fires in the RULE-3 slot of
// app::tick — BEFORE the S7-hrz capture — so the release tick's up-debt is
// measured about the POST-snap forward. The double-tap block below it cannot
// host a release-edge fire (it would capture the stale pre-snap axis).
//
// This file pins: (1) the mouse-only release snap + orient_fired on a
// NON-TRIVIAL fixture (banked, off-velocity — the fixture-no-op trap);
// (2) the ballistic guard; (3) the knob-OFF legacy arm (release moves nothing
// — the 4d conditional reset, the differential firewall); (4) override
// currently held at release => legacy exactly (no orient); (5) override
// used-then-released before space-up => the rule-3 snap ALSO reports the
// camera cut with the knob on; (6) the S7-hrz ordering (capture about the
// POST-snap forward — mutation: hoist the recovery block above the rule-3
// slot, or move the release fire into the double-tap orient block);
// (7) grounded release never fires.
//
// The CQ2 companion (a mouse delta on the release tick cannot smear the
// snapped aim) is pinned by test_instructor_tick.cpp's ease-back leg, which
// runs the shipped (knob-ON) table differentially.

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
const control::ControllerParams kCpToml =
    cfg::load_controller_toml(SEADS_CONFIG_DIR "/controller.toml", kAp);

constexpr double kPi = 3.14159265358979323846;
double rad(double d) { return d * kPi / 180.0; }
double deg(double r) { return r * 180.0 / kPi; }

// Explicit knob arms — the legs never depend on the shipped toml's value
// (config-relative discipline: a future toml flip must not silently disarm
// or false-fail these pins).
// cp_on/cp_off pin with_keys FALSE explicitly: they are the SEALED-V6 arms,
// and the shipped toml now sets release_orient_with_keys = true — without this
// the legacy legs below (case 4) would silently flip meaning on the toml value.
control::ControllerParams cp_on() {
    control::ControllerParams c = kCpToml;
    c.freelook_release_orient = true;
    c.freelook_release_orient_with_keys = false;
    return c;
}
control::ControllerParams cp_off() {
    control::ControllerParams c = kCpToml;
    c.freelook_release_orient = false;
    c.freelook_release_orient_with_keys = false;
    return c;
}
// The S-relorient ADDENDUM arm: the D9 exception RETIRED (Chad's fly value).
control::ControllerParams cp_keys() {
    control::ControllerParams c = kCpToml;
    c.freelook_release_orient = true;
    c.freelook_release_orient_with_keys = true;
    return c;
}

// A pitch-override key held down (the "hard keys for control surfaces" the
// pilot keeps flying on through the release).
app::TickInput with_key(app::TickInput in) {
    in.override_mask[0] = true;
    in.override_sign[0] = 1.0;
    return in;
}

app::TickInput instr_in(double throttle) {
    app::TickInput in;
    in.throttle = throttle;
    return in;
}

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

// The banked, sideslipping, at-AoA fixture from test_orient: nose, velocity,
// and any parked aim all SEPARATE (candidate targets must separate — a level
// fixture makes the snap a near-no-op and pins nothing).
sim::SimState banked_state() {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    return harness::flight_state(kAp, 180.0, 4000.0, up, heading, rad(40.0),
                                 rad(8.0), rad(6.0));
}

double aim_to_deg(const app::LoopState& st, const glm::dvec3& dir) {
    return deg(std::acos(std::clamp(glm::dot(st.aim.forward(), dir), -1.0,
                                    1.0)));
}

// Hold freelook `hold_ticks`, then run the release tick and return its result.
app::TickResult tap_release(app::LoopState& st,
                            const control::ControllerParams& cp,
                            int hold_ticks = 3) {
    app::TickInput hold = instr_in(0.7);
    hold.freelook_held = true;
    for (int i = 0; i < hold_ticks; ++i) app::tick(st, hold, kAp, cp, nullptr);
    return app::tick(st, instr_in(0.7), kAp, cp, nullptr);
}

}  // namespace

// ===========================================================================
// 1. The core ask, v9 re-scope (S-nosesnap, docs/DECISIONS.md @ b4c0751): a
//    MOUSE-ONLY release lands the aim on the NOSE and reports the camera cut.
//    The guarded-velocity oracle was term A of the v9 defect (a measured
//    18.6 deg jump at the mouse handover); under the §5b WELD the aim already
//    rides the nose all freelook, so the release is a no-op on the aim.
// ===========================================================================
TEST_CASE("S-relorient: mouse-only release lands aim on the NOSE + cam cut") {
    const control::ControllerParams cp = cp_on();
    app::LoopState st = flying(banked_state());

    // Drag the aim well off both candidates BEFORE the hold. Under v9 the
    // WELD erases this at the first held tick — the drag now proves the weld
    // has real work, and the release must land on the NOSE side of the
    // nose-vs-velocity gap (bases separated below, so this pins the target).
    st.aim.apply_mouse(300.0, -200.0, rad(0.15));

    const app::TickResult r = tap_release(st, cp);
    CHECK(r.orient_fired);  // the camera cut is reported on the release tick

    const glm::dvec3 nose = st.curr.orientation * glm::dvec3{0.0, 0.0, -1.0};
    const double spd = glm::length(st.curr.velocity);
    REQUIRE(spd > cp.v_ballistic);
    const glm::dvec3 vhat = st.curr.velocity / spd;
    REQUIRE(glm::dot(vhat, nose) > 0.0);
    REQUIRE(deg(std::acos(std::clamp(glm::dot(vhat, nose), -1.0, 1.0))) > 3.0);

    const double to_nose = aim_to_deg(st, nose);
    const double to_vhat = aim_to_deg(st, vhat);
    std::printf("[S-relorient release] aim_vs_nose=%.3f aim_vs_vhat=%.3f deg\n",
                to_nose, to_vhat);
    CHECK(to_nose < 1.0);  // aim := NOSE (behind the aircraft)
    CHECK(to_vhat > 2.0);  // and NOT the old velocity target

    // ONE-SHOT (diff red-team P1-1): the fire is the fs.released EDGE, not
    // the easeback window — a re-fire would re-snap the aim to the drifting
    // velocity and re-zero the camera orbit/inertia every tick for ~0.3 s,
    // killing the ease-in. Mutation this kills (verified): gate on
    // `fs.released || st.fl.easeback > 0.0` — re-fires the very next tick.
    for (int t = 0; t < 5; ++t) {
        const app::TickResult r2 = app::tick(st, instr_in(0.7), kAp, cp, nullptr);
        CHECK_FALSE(r2.orient_fired);
    }
}

// ===========================================================================
// 2. The ballistic guard: sub-ballistic (or tail-slide) falls back to the
//    NOSE — the S7-nest F5 guard verbatim, shared with the double-tap.
// ===========================================================================
TEST_CASE("S-relorient: below ballistic speed the release lands on the NOSE") {
    const control::ControllerParams cp = cp_on();
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    sim::SimState s = harness::level_state(kAp, 150.0, 4000.0, up, heading);
    const glm::dvec3 nose = s.orientation * glm::dvec3{0.0, 0.0, -1.0};
    const glm::dvec3 side = s.orientation * glm::dvec3{1.0, 0.0, 0.0};
    s.velocity = 0.5 * cp.v_ballistic * glm::normalize(0.5 * nose + side);
    s.last_vhat = glm::normalize(s.velocity);
    app::LoopState st = flying(s);
    st.aim.apply_mouse(300.0, 0.0, rad(0.15));  // aim off both candidates

    const app::TickResult r = tap_release(st, cp, 1);
    CHECK(r.orient_fired);

    const glm::dvec3 nose_now =
        st.curr.orientation * glm::dvec3{0.0, 0.0, -1.0};
    const glm::dvec3 vhat_now = glm::normalize(st.curr.velocity);
    CHECK(aim_to_deg(st, nose_now) < 2.0);   // fell back to the nose
    CHECK(aim_to_deg(st, vhat_now) > 20.0);  // NOT the (lying) velocity
}

// ===========================================================================
// 3. Knob OFF disables the FIRE only: a mouse-only release reports no cut and
//    snaps nothing on the release tick. v9 re-scope (b4c0751): the §5b WELD is
//    NOT behind release_orient — Chad's ruling is unconditional — so the aim
//    was already nested to the nose during the hold; what this leg pins is
//    that the release TICK itself moves nothing (transport only) and fires
//    nothing when the knob is off.
// ===========================================================================
TEST_CASE("S-relorient: knob OFF leaves the mouse-only release untouched") {
    const control::ControllerParams cp = cp_off();
    app::LoopState st = flying(banked_state());
    st.aim.apply_mouse(300.0, -200.0, rad(0.15));

    // The welded aim direction just before the release tick.
    app::TickInput hold = instr_in(0.7);
    hold.freelook_held = true;
    for (int i = 0; i < 3; ++i) app::tick(st, hold, kAp, cp, nullptr);
    const glm::dvec3 held_fwd = st.aim.forward();
    // The weld put it on the nose during the hold (unconditional, knob-off
    // included — a knob-gated weld would be a second camera law).
    CHECK(aim_to_deg(st, st.curr.orientation * glm::dvec3{0.0, 0.0, -1.0}) <
          1.5);

    const app::TickResult r = app::tick(st, instr_in(0.7), kAp, cp, nullptr);
    CHECK_FALSE(r.orient_fired);
    // One tick of parallel transport moves the carried aim by ~V*dt/R
    // (~1e-4 rad) — the release itself SNAPPED nothing.
    const double moved =
        deg(std::acos(std::clamp(glm::dot(st.aim.forward(), held_fwd), -1.0,
                                 1.0)));
    std::printf("[S-relorient knob-off] release moved aim %.5f deg\n", moved);
    CHECK(moved < 0.1);  // transport only — no snap fired
}

// ===========================================================================
// 4. KNOB-OFF ARM of the S-relorient ADDENDUM (was the sealed-v6 behavior):
//    with release_orient_with_keys FALSE, an override key still held on the
//    release tick keeps LEGACY exactly — rule-3 (override_used) still snaps,
//    but no camera cut is reported (the D9 precedent). This leg is the
//    strict-superset proof that the addendum's walk-back restores v6.
// ===========================================================================
TEST_CASE("S-relorient: override held at release = legacy, no camera cut") {
    const control::ControllerParams cp = cp_on();
    app::LoopState st = flying(banked_state());

    app::TickInput hold_both = instr_in(0.7);
    hold_both.freelook_held = true;
    hold_both.override_mask[0] = true;
    hold_both.override_sign[0] = 1.0;
    for (int i = 0; i < 5; ++i) app::tick(st, hold_both, kAp, cp, nullptr);

    app::TickInput key_only = instr_in(0.7);  // Space up, key still down
    key_only.override_mask[0] = true;
    key_only.override_sign[0] = 1.0;
    const app::TickResult r = app::tick(st, key_only, kAp, cp, nullptr);
    CHECK_FALSE(r.orient_fired);  // no cut while the pilot holds a key
}

// ===========================================================================
// 5. Override used DURING the hold, keys up BEFORE the release: rule-3 fires
//    its own guarded-velocity snap (pre-existing), and with the knob ON the
//    release ALSO reports the camera cut — the one thing that case lacked.
// ===========================================================================
TEST_CASE("S-relorient: override-used release keeps the snap AND gains cut") {
    const control::ControllerParams cp = cp_on();
    app::LoopState st = flying(banked_state());

    app::TickInput hold = instr_in(0.7);
    hold.freelook_held = true;
    app::TickInput hold_key = hold;
    hold_key.override_mask[0] = true;
    hold_key.override_sign[0] = 1.0;
    for (int i = 0; i < 3; ++i) app::tick(st, hold, kAp, cp, nullptr);
    for (int i = 0; i < 30; ++i) app::tick(st, hold_key, kAp, cp, nullptr);
    for (int i = 0; i < 3; ++i) app::tick(st, hold, kAp, cp, nullptr);  // keys up

    const app::TickResult r = app::tick(st, instr_in(0.7), kAp, cp, nullptr);
    CHECK(r.orient_fired);  // the knob adds the cut to rule-3's own snap

    // v9 (b4c0751): the snap target is the NOSE at every speed — no guard
    // branch to mirror here anymore.
    const glm::dvec3 nose = st.curr.orientation * glm::dvec3{0.0, 0.0, -1.0};
    CHECK(aim_to_deg(st, nose) < 1.0);
}

// ===========================================================================
// 6. ORDERING (plan-audit P1), v9 re-scope (b4c0751) — HONEST DEGRADATION
//    NOTE: the righting still runs AFTER the snap (the ordering is kept in
//    code), but the hoist mutant is NO LONGER KILLABLE at magnitude — under
//    the WELD the aim already rides the nose during the hold, so the snap
//    moves the forward by at most one tick of body rotation (~sub-degree) and
//    a pre-snap-axis righting lands inside the oracle. Do NOT claim
//    "mutation-verified" for the hoist here anymore. What this leg pins now:
//    the release rights the WHOLE carried up-debt in the SAME tick (the
//    instant law), with real two-axis work in the fixture.
// ===========================================================================
TEST_CASE("S-relorient: the release tick rights the whole up-debt (instant)") {
    control::ControllerParams cp = cp_on();
    cp.horizon_recovery_rate = rad(150.0);  // > 0 = enabled (v9: pure enable)
    cp.horizon_recovery_settle = 5.0;
    app::LoopState st = flying(banked_state());
    st.aim.apply_mouse(400.0, -300.0, rad(0.15));  // erased by the weld
    st.aim.roll_about_forward(rad(60.0));          // real carried up-debt

    const app::TickResult r = tap_release(st, cp);
    CHECK(r.orient_fired);
    CHECK(st.recov.remaining == 0.0);  // no latch — the roll happened NOW
    const double residual =
        std::abs(deg(st.aim.up_misalignment(sim::local_up(st.curr.position))));
    std::printf("[S-relorient instant] residual misalign=%.4f deg\n",
                residual);
    CHECK(residual < 1.0);  // righted ON the release tick, no roll-in
}

// ===========================================================================
// 7. A grounded release never fires. HONEST BANNER (diff red-team P2-1): the
//    REAL guard is the GROUNDED pairing's fl.reset() eating the release edge
//    (fs.released && grounded is unreachable); the gate's !st.grounded term
//    is redundant defense and this leg is UNKILLABLE by dropping it — it
//    pins the composed behavior, not that term.
// ===========================================================================
TEST_CASE("S-relorient: a grounded release never fires") {
    const control::ControllerParams cp = cp_on();
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    app::LoopState st =
        flying(harness::level_state(kAp, 150.0, 3000.0, up, heading));

    app::TickInput hold = instr_in(0.7);
    hold.freelook_held = true;
    app::tick(st, hold, kAp, cp, nullptr);
    st.grounded = true;  // the release lands on a spawn/reset tick
    const app::TickResult r = app::tick(st, instr_in(0.7), kAp, cp, nullptr);
    CHECK_FALSE(r.orient_fired);
}

// ===========================================================================
// S-relorient ADDENDUM (Chad 2026-07-28, flying the sealed v6): "anytime my
// finger isn't pressing freelook, I am in chase camera directly behind and
// using mouse aim — even if still turning and pressing hard keys for control
// surfaces." The D9 exception is RETIRED under release_orient_with_keys at all
// three sites. Cases 8-11 pin the knob-ON arm; case 4 above is its knob-OFF
// twin (sealed v6 exactly), so each leg has both arms.
//
// The four-phase repro is Chad's own report: hold Space -> press an override
// mid-hold -> release Space WITH THE KEY STILL DOWN -> keep flying on the key.
// Under v6 that spent the one-tick fs.released edge for good: the reticle
// snapped (rule 3) while the camera never cut and the horizon never righted.
// ===========================================================================
namespace {

// Phases 1-3 of the repro. Leaves the state ON the release tick's result, with
// the override key still held. Returns that tick's result.
app::TickResult release_with_key_held(app::LoopState& st,
                                      const control::ControllerParams& cp) {
    app::TickInput hold = instr_in(0.7);
    hold.freelook_held = true;
    for (int i = 0; i < 3; ++i)  // freelook
        app::tick(st, hold, kAp, cp, nullptr);
    for (int i = 0; i < 5; ++i)  // + an override key mid-hold
        app::tick(st, with_key(hold), kAp, cp, nullptr);
    // Space up, key STILL down — the release tick Chad reported.
    return app::tick(st, with_key(instr_in(0.7)), kAp, cp, nullptr);
}

}  // namespace

// 8. The core addendum: a release WITH a key held fires the FULL verb.
TEST_CASE("S-relorient addendum: keys-held release fires the orient verb") {
    const control::ControllerParams cp = cp_keys();
    app::LoopState st = flying(banked_state());
    st.aim.apply_mouse(300.0, -200.0, rad(0.15));  // real work for the snap

    const app::TickResult r = release_with_key_held(st, cp);
    CHECK(r.orient_fired);  // v6 withheld this — the camera never cut

    // v9 (b4c0751): the verb lands on the NOSE, and in the keys-held case it
    // is a no-op on the aim BY CONSTRUCTION — the §5b weld nested the aim on
    // the nose every held tick, so the release only re-aligns to one tick of
    // nose motion. The bases-separate REQUIREs keep the leg non-vacuous.
    const glm::dvec3 nose = st.curr.orientation * glm::dvec3{0.0, 0.0, -1.0};
    const double spd = glm::length(st.curr.velocity);
    REQUIRE(spd > cp.v_ballistic);
    const glm::dvec3 vhat = st.curr.velocity / spd;
    REQUIRE(glm::dot(vhat, nose) > 0.0);
    REQUIRE(deg(std::acos(std::clamp(glm::dot(vhat, nose), -1.0, 1.0))) > 3.0);
    const double to_nose = aim_to_deg(st, nose);
    std::printf("[addendum keys-held] aim_vs_nose=%.3f deg\n", to_nose);
    CHECK(to_nose < 1.0);  // aim on the NOSE, same verb as a clean release
    CHECK(aim_to_deg(st, vhat) > 2.0);  // NOT the old velocity target
}

// 9. The up-debt RETIRES with the key still down — v9 re-scope (b4c0751):
//    INSTANTLY, on the release tick itself (no roll to run to completion; the
//    instant law). Mutation this kills: restoring `any_ovr` to the righting
//    condition — the release rights nothing and the world stays rolled, which
//    is the loudest "this isn't chase view" cue.
TEST_CASE("S-relorient addendum: the horizon rights with the key still down") {
    control::ControllerParams cp = cp_keys();
    cp.horizon_recovery_rate = rad(150.0);  // > 0 = enabled (v9: pure enable)
    cp.horizon_recovery_settle = 5.0;
    app::LoopState st = flying(banked_state());
    st.aim.apply_mouse(400.0, -300.0, rad(0.15));
    st.aim.roll_about_forward(rad(60.0));  // real carried up-debt

    const app::TickResult r = release_with_key_held(st, cp);
    CHECK(r.orient_fired);
    CHECK(st.recov.remaining == 0.0);  // no latch armed — righted NOW
    const double at_release =
        std::abs(deg(st.aim.up_misalignment(sim::local_up(st.curr.position))));
    std::printf("[addendum keys-held] misalign at release=%.4f deg\n",
                at_release);
    CHECK(at_release < 1.0);  // the whole debt retired ON the release tick

    // ...and it STAYS righted while the pilot keeps flying on that key.
    // RESPAWN GUARD (red-team P2-1): `grounded` is true for exactly ONE tick
    // after a crash, so sampling it after the loop proves nothing — and a
    // respawn would satisfy the assertion below for the wrong reason
    // (aim.reseed() zeroes the misalignment). Watch the per-tick flag.
    const app::TickInput key = with_key(instr_in(0.7));
    for (int t = 0; t < 2 * 120; ++t) {
        REQUIRE_FALSE(app::tick(st, key, kAp, cp, nullptr).respawned);
    }
    CHECK(st.recov.remaining == 0.0);
}

// 9b. The knob-OFF twin of case 9: under sealed v6 the debt is NOT retired.
//     v9 hardening (mutation-exposed): with the instant law, recov.remaining
//     is 0 in BOTH arms (the struct is inert), so the old remaining==0 check
//     is vacuous — the honest pin is the MISALIGNMENT itself surviving the
//     keys-held release when the knob is off. (M5, the dropped-knob-clause
//     mutant, passed the old shape and fails this one.)
TEST_CASE("S-relorient addendum: knob OFF leaves the horizon debt unretired") {
    control::ControllerParams cp = cp_on();  // with_keys FALSE
    cp.horizon_recovery_rate = rad(150.0);
    cp.horizon_recovery_settle = 5.0;
    app::LoopState st = flying(banked_state());
    st.aim.apply_mouse(400.0, -300.0, rad(0.15));
    st.aim.roll_about_forward(rad(60.0));
    const double m0 =
        std::abs(deg(st.aim.up_misalignment(sim::local_up(st.curr.position))));
    REQUIRE(m0 > 20.0);  // real debt on the frame (fixture-no-op trap)

    const app::TickResult r = release_with_key_held(st, cp);
    CHECK_FALSE(r.orient_fired);
    CHECK(st.recov.remaining == 0.0);
    // D9 (sealed v6) still eats the righting: the world stays rolled.
    // Honest value ~19.7 deg (the WELD's hold-snaps geometrically shrink the
    // 60 deg seed — forward moves tens of degrees and the up follows the
    // geodesic); the M5 mutant (knob clause dropped => righting fires) lands
    // ~0. The bound sits between, far from both.
    CHECK(std::abs(deg(st.aim.up_misalignment(
              sim::local_up(st.curr.position)))) > 10.0);
}

// 9c. v9 re-scope (b4c0751): "an override pressed mid-roll no longer cancels"
//     is UNREPRESENTABLE under the instant law — there is no mid-roll to
//     cancel. The surviving semantics this leg pins: the clean release rights
//     the whole debt on its own tick, and a key jabbed on the very NEXT tick
//     neither disturbs the righted frame nor triggers anything (a keypress is
//     not a release edge; keys never touch the carried frame).
TEST_CASE("S-relorient addendum: a post-release key leaves the frame righted") {
    control::ControllerParams cp = cp_keys();
    cp.horizon_recovery_rate = rad(150.0);
    cp.horizon_recovery_settle = 5.0;
    app::LoopState st = flying(banked_state());
    st.aim.roll_about_forward(rad(60.0));

    const app::TickResult r = tap_release(st, cp);  // clean release: rights NOW
    REQUIRE(r.orient_fired);
    CHECK(st.recov.remaining == 0.0);
    REQUIRE(std::abs(deg(st.aim.up_misalignment(
                sim::local_up(st.curr.position)))) < 1.0);

    // Jab the key immediately and keep flying on it.
    const app::TickInput key = with_key(instr_in(0.7));
    for (int t = 0; t < 2 * 120; ++t) {  // respawn guard: see case 9 (P2-1)
        REQUIRE_FALSE(app::tick(st, key, kAp, cp, nullptr).respawned);
        CHECK(st.recov.remaining == 0.0);
    }
    // The keyboard flew the PLANE; the carried frame stayed righted (the
    // misalignment is local_up-referenced and transport-preserved).
    CHECK(std::abs(deg(st.aim.up_misalignment(
              sim::local_up(st.curr.position)))) < 5.0);
}

// 10. ONE-SHOT: the addendum fires on the release EDGE only. Letting go of the
//     keys later must NOT produce a second fire — this guards against anyone
//     "fixing" the spent-edge problem by stacking a deferred latch on top.
TEST_CASE("S-relorient addendum: no re-fire when the keys are released later") {
    const control::ControllerParams cp = cp_keys();
    app::LoopState st = flying(banked_state());
    st.aim.apply_mouse(300.0, -200.0, rad(0.15));

    REQUIRE(release_with_key_held(st, cp).orient_fired);

    const app::TickInput key = with_key(instr_in(0.7));
    for (int t = 0; t < 5; ++t) {
        CHECK_FALSE(app::tick(st, key, kAp, cp, nullptr).orient_fired);  // key still down
    }
    for (int t = 0; t < 5; ++t) {
        CHECK_FALSE(app::tick(st, instr_in(0.7), kAp, cp, nullptr).orient_fired);  // key up
    }
}

// 11. The double-tap is TRULY redundant: it fires with a key held too (Chad
//     reached for it precisely because the release failed). Both arms pinned.
//
//     v9 NOTE (b4c0751): the old "HONEST SCOPE" caveat here — the verb's aim
//     half lived one tick because the §5b nest re-snapped to the NOSE while
//     the verb had snapped to VELOCITY — is DISSOLVED, not just re-scoped:
//     the verb's target and the nest's target are now the same nose, so "one
//     verb, two triggers" is exact for the flag, the target, AND the dwell.
//     Pinned on the fire tick and the following held tick below.
TEST_CASE("S-relorient addendum: the double-tap fires with a key held") {
    app::LoopState st = flying(banked_state());
    st.aim.apply_mouse(300.0, -200.0, rad(0.15));

    app::TickInput tap = with_key(instr_in(0.7));
    tap.freelook_held = true;   // the SECOND press tick of the double-tap
    tap.orient_cmd = true;

    app::LoopState st_v6 = st;  // same start, both arms
    CHECK_FALSE(app::tick(st_v6, tap, kAp, cp_on(), nullptr).orient_fired);  // sealed v6
    CHECK(app::tick(st, tap, kAp, cp_keys(), nullptr).orient_fired);         // addendum

    // The aim landed on the NOSE on the fire tick (the flag alone would pass
    // under a mutant that fires but snaps nowhere), and NOT on the velocity
    // (bases separated — the old target must be measurably rejected).
    const glm::dvec3 nose = st.curr.orientation * glm::dvec3{0.0, 0.0, -1.0};
    const double spd = glm::length(st.curr.velocity);
    REQUIRE(spd > kCpToml.v_ballistic);
    const glm::dvec3 vhat = st.curr.velocity / spd;
    REQUIRE(deg(std::acos(std::clamp(glm::dot(vhat, nose), -1.0, 1.0))) > 3.0);
    CHECK(aim_to_deg(st, nose) < 1.0);
    CHECK(aim_to_deg(st, vhat) > 2.0);

    // ...and it STAYS on the nose through the next held tick (the weld and
    // the verb agree now — the one-tick-dwell caveat is gone).
    app::TickInput held = with_key(instr_in(0.7));
    held.freelook_held = true;  // still in freelook, no second command
    app::tick(st, held, kAp, cp_keys(), nullptr);
    CHECK(aim_to_deg(st, st.curr.orientation * glm::dvec3{0.0, 0.0, -1.0}) <
          1.0);
}

// ===========================================================================
// S-keyprec (Chad 2026-07-29, SEALED KERNEL v8) — THE HARD KEY DOES NOT MOVE
// THE CAMERA.
//
// Chad, flying v7: "When I am flying in mouse aim the snap back to chase is
// occurring with every hard key press... if I input some aileron to cut into
// their path sooner, I get a disorienting snap to the chase cam which throws
// off my aim." His rule: "Only the precedence of the freelook push shall do
// that." One camera automation (the freelook-release snap, pinned above);
// after it the camera is bound to the AIM under mouse authority alone, and the
// override keys reach the TRAJECTORY and nothing else.
//
// This is the leg the retired S-keychase had no equivalent of, and its absence
// is why the mechanism shipped: every camera scenario modeled ONE hand (a pure
// mouse pilot, or a pure keyboard pilot with a parked aim). Chad flies BOTH AT
// ONCE. Here the pilot holds a mouse-aim turn AND presses a hard key.
//
// WHAT IT ASSERTS, and why there is no calibrated constant: the camera must sit
// nearer the AIM than the FLIGHT PATH — that IS the ruling ("bound to the
// aim"), stated as a relation between two measured angles rather than a
// threshold some future retune of [camera] lag_base/lag_gain would false-fail
// or silently disarm. Under S-keychase the relation INVERTS (the rest target
// became velocity: measured lag-to-aim 67.8 deg while lag-to-velocity went to
// ~0), so a re-introduction fails this leg by construction, whether it arrives
// as a parameter, a config dial, or an inlined predicate.
//
// Non-vacuity: the aim and the flight path must be MATERIALLY separated in the
// measurement window, or "nearer the aim" is satisfied trivially by level
// flight where they coincide (the fixture-no-op class). REQUIREd below.
TEST_CASE("S-keyprec: a hard key in mouse-aim leaves the camera on the aim") {
    app::LoopState st = flying(banked_state());
    harness::MiniCamera cam;
    cam.seed(st.curr);

    // The MOUSE hand: re-command a hard turn every tick (70 deg right of the
    // current horizontal heading, recomputed from local_up — never a cached
    // axis, SPEC 6.1). This is comfort_detail::turn_reaim's geometry.
    const auto reaim = [&](void) {
        const glm::dvec3 up = sim::local_up(st.curr.position);
        const glm::dvec3 nose =
            st.curr.orientation * glm::dvec3{0.0, 0.0, -1.0};
        glm::dvec3 h = nose - glm::dot(nose, up) * up;
        if (glm::length(h) < 1e-6)
            h = st.curr.orientation * glm::dvec3{1.0, 0.0, 0.0};
        h = glm::normalize(h);
        const glm::dvec3 target =
            glm::normalize(glm::angleAxis(-rad(70.0), up) * h);
        st.aim.snap_forward_to_dir(target, st.curr.orientation);
    };
    const auto ang = [](const glm::dvec3& a, const glm::dvec3& b) {
        return deg(std::acos(std::clamp(
            glm::dot(glm::normalize(a), glm::normalize(b)), -1.0, 1.0)));
    };

    const int total = static_cast<int>(8.0 / kAp.sim_dt);
    const int key_at = static_cast<int>(4.0 / kAp.sim_dt);
    double sum_to_aim = 0.0, sum_to_vel = 0.0, sum_sep = 0.0;
    int n = 0;

    for (int i = 1; i <= total; ++i) {
        reaim();
        // The hard key joins the mouse at 4 s and stays down — "some aileron
        // to cut into their path sooner", never released.
        app::TickInput in = instr_in(1.0);
        if (i >= key_at) {
            in.override_mask[2] = true;  // aileron
            in.override_sign[2] = 1.0;
            in.override_mask[0] = true;  // elevator
            in.override_sign[0] = 1.0;
        }
        in.cam_fwd = cam.cam_fwd;
        in.cam_up = cam.cam_up;
        const app::TickResult r = app::tick(st, in, kAp, kCpToml, nullptr);
        REQUIRE_FALSE(r.respawned);  // the measurement window must stay alive
        if (r.orient_fired) cam.orient_cut(st.aim.forward());
        cam.advance(st.curr, st.aim.forward(), st.aim.up(), kCpToml,
                    kAp.sim_dt);

        if (i > total - static_cast<int>(2.0 / kAp.sim_dt)) {  // last 2 s
            const double sp = glm::length(st.curr.velocity);
            REQUIRE(sp > 1.0);
            const glm::dvec3 vhat = st.curr.velocity / sp;
            sum_to_aim += ang(cam.cam_fwd, st.aim.forward());
            sum_to_vel += ang(cam.cam_fwd, vhat);
            sum_sep += ang(st.aim.forward(), vhat);
            ++n;
        }
    }
    REQUIRE(n > 0);
    const double to_aim = sum_to_aim / n;
    const double to_vel = sum_to_vel / n;
    const double sep = sum_sep / n;
    std::printf(
        "[S-keyprec] key held in mouse-aim: camera-to-AIM %.2f deg, "
        "camera-to-PATH %.2f deg (aim/path separation %.2f deg)\n",
        to_aim, to_vel, sep);

    // Non-vacuity: the two candidate anchors must actually separate, or
    // "nearer the aim" is free. (Also the S3 "make the bases separate" rule.)
    REQUIRE(sep > 20.0);
    // THE RULING: the camera is anchored to the aim, not to the flight path.
    // S-keychase inverts this pair.
    CHECK(to_aim < to_vel);
    // ...and by a clear margin, not a coin-flip: it sits in the aim's half of
    // the separation. Derived from the measured separation, not a constant.
    CHECK(to_aim < 0.5 * sep);
}
