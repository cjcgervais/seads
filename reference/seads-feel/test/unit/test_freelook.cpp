// Section 4d — freelook (SPEC §9.5, the input state machine's "never surprises
// the player" clause). The pilot lets go of the airframe to check six and the
// instructor keeps flying the LAST COMMANDED turn: the aim is held (only
// parallel-transported, §9.1), the mouse goes to the camera, and nothing the
// pilot didn't just command becomes a surprise on the way back.
//
// The pure controller needs NO freelook flag (SOLUTION §5.5): freelook is
// entirely caller-side, and its three event rules live in the SHARED
// input::Freelook so the harness here and the app run the identical code
// (CLAUDE.md 4b/H1 — route the live path through the tested function). This
// file pins that struct directly (exact latch logic) AND closed-loop through
// the REAL sim::step() (HARNESS §3, never a flat stub) — AT-8.
//
// The 4b controller golden + the 4c override tests are the complementary
// tripwire: with freelook never engaged the aim path is byte-identical, so
// freelook is proven a strict superset (same discipline as the override mask).

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>

#include "config/load_aircraft.h"
#include "config/load_controller.h"
#include "control/controller.h"
#include "input/aim_state.h"
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

constexpr int kPitch = 0, kRoll = 2;

bool finite_state(const sim::SimState& s) {
    auto ok3 = [](const glm::dvec3& v) {
        return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
    };
    return ok3(s.position) && ok3(s.velocity) && ok3(s.angular_vel) &&
           std::isfinite(s.orientation.w) && std::isfinite(s.orientation.x) &&
           std::isfinite(s.orientation.y) && std::isfinite(s.orientation.z);
}

}  // namespace

// ===========================================================================
// input::Freelook — the pure latch logic (SPEC §9.5), tested directly and
// exactly. dt/easeback come from the real committed table so the count can't
// drift from the schema.
// ===========================================================================

// ---------------------------------------------------------------------------
// Rule 1 — held holds the aim: no snap, and mouse->aim SUSPENDED the whole hold
// (the mouse is on the camera). Nothing here touches the aim; the caller only
// transports it.
// ---------------------------------------------------------------------------
TEST_CASE("4d freelook: held holds the aim and suspends mouse->aim") {
    input::Freelook fl;
    const double dt = kAp.sim_dt, E = kCp.freelook_easeback_time;
    for (int i = 0; i < 50; ++i) {
        const input::Freelook::Step s = fl.step(/*freelook=*/true,
                                                /*any_override=*/false, dt, E);
        CHECK_FALSE(s.snap_to_nose);    // aim held, never reset
        CHECK_FALSE(s.mouse_aim_live);  // mouse -> camera
    }
}

// ---------------------------------------------------------------------------
// Rule 2 — the first override activity of a hold snaps aim := nose, exactly
// ONCE (latched): a key pressed DURING freelook fires on the press edge; a key
// already held when freelook begins fires on the first freelook tick. The
// parked cursor you can't see must not survive the instant you seize an axis.
// ---------------------------------------------------------------------------
TEST_CASE(
    "4d freelook: first override during a hold snaps to nose exactly once") {
    const double dt = kAp.sim_dt, E = kCp.freelook_easeback_time;

    SECTION("pressed during freelook") {
        input::Freelook fl;
        // A few ticks of pure freelook: no snap.
        for (int i = 0; i < 5; ++i)
            CHECK_FALSE(fl.step(true, false, dt, E).snap_to_nose);
        // The press edge: snap fires.
        CHECK(fl.step(true, true, dt, E).snap_to_nose);
        // Still held: latched, does NOT snap again (would fight the pilot).
        for (int i = 0; i < 5; ++i)
            CHECK_FALSE(fl.step(true, true, dt, E).snap_to_nose);
    }

    SECTION("already held when freelook begins") {
        input::Freelook fl;
        // Override was already down (MOUSE+override) when Space is pressed:
        // the first freelook tick sees it and snaps.
        CHECK(fl.step(true, true, dt, E).snap_to_nose);
        CHECK_FALSE(fl.step(true, true, dt, E).snap_to_nose);
    }
}

// ---------------------------------------------------------------------------
// Rule 3 — the release is CONDITIONAL: aim := nose ONLY if an override was used
// during the hold. Without one, nothing changes (the held turn continues; a
// snap-back to a stored target the pilot didn't just command is
// unrepresentable — CLAUDE.md Learned).
// ---------------------------------------------------------------------------
TEST_CASE("4d freelook: release without an override does not snap") {
    input::Freelook fl;
    const double dt = kAp.sim_dt, E = kCp.freelook_easeback_time;
    for (int i = 0; i < 10; ++i) fl.step(true, false, dt, E);  // pure freelook
    // Release edge: no override was used -> no snap.
    CHECK_FALSE(fl.step(false, false, dt, E).snap_to_nose);
}

TEST_CASE(
    "4d freelook: release after an override snaps to nose (conditional)") {
    input::Freelook fl;
    const double dt = kAp.sim_dt, E = kCp.freelook_easeback_time;
    fl.step(true, false, dt, E);
    fl.step(true, true, dt, E);   // override used (snaps on this edge)
    fl.step(true, false, dt, E);  // key up, still freelook (latch persists)
    // Release edge: override WAS used this hold -> snap := nose.
    CHECK(fl.step(false, false, dt, E).snap_to_nose);
    // The latch was consumed: a subsequent clean hold+release does NOT snap.
    for (int i = 0; i < 40; ++i)
        fl.step(false, false, dt, E);  // clear easeback
    fl.step(true, false, dt, E);
    CHECK_FALSE(fl.step(false, false, dt, E).snap_to_nose);
}

// ---------------------------------------------------------------------------
// AT-8 / CQ2 (SPEC §16) — after a release, mouse->aim is SUSPENDED for the
// ease-back window (<=300 ms) so no easing-frame (smoothed camera) basis ever
// leaks into the aim. The window is ceil(easeback/dt) ticks; re-entering
// freelook mid-ease cancels it (the mouse is back on the camera).
// ---------------------------------------------------------------------------
TEST_CASE(
    "AT-8: mouse->aim suspended across the ease-back window, then resumes") {
    const double dt = kAp.sim_dt, E = kCp.freelook_easeback_time;
    const int window = static_cast<int>(std::ceil(E / dt));

    input::Freelook fl;
    for (int i = 0; i < 10; ++i)
        REQUIRE_FALSE(
            fl.step(true, false, dt, E).mouse_aim_live);  // in freelook

    // Release: the ease-back begins. Scan for the first tick mouse->aim is live
    // again; it must land AT the window boundary (allow +/-1 tick for the fp
    // boundary), everything before it suspended, everything after it live.
    int first_live = -1;
    for (int i = 0; i < window + 20; ++i) {
        const bool live = fl.step(false, false, dt, E).mouse_aim_live;
        if (live && first_live < 0) first_live = i;
        if (first_live >= 0)
            CHECK(live);  // once live, stays live
        else
            CHECK_FALSE(live);  // before that, suspended
    }
    CHECK(first_live >= window - 1);
    CHECK(first_live <= window + 1);

    // Re-entering freelook DURING the ease-back cancels the window: the next
    // release starts a full fresh window, not a shortened remainder.
    input::Freelook fl2;
    fl2.step(true, false, dt, E);
    fl2.step(false, false, dt, E);  // release -> ease-back armed
    for (int i = 0; i < 5; ++i)
        REQUIRE_FALSE(fl2.step(false, false, dt, E).mouse_aim_live);  // easing
    fl2.step(true, false, dt, E);   // back into freelook: cancels ease-back
    fl2.step(false, false, dt, E);  // release again -> fresh window
    int fresh_false = 0;
    for (int i = 0; i < window + 5; ++i) {
        if (!fl2.step(false, false, dt, E).mouse_aim_live)
            ++fresh_false;
        else
            break;
    }
    // A full fresh window (~window ticks) suspended, proving no leftover
    // shortened remainder from the first release.
    CHECK(fresh_false >= window - 2);
}

// ---------------------------------------------------------------------------
// AT-8 / CQ2 — the ease-back window LENGTH is pinned EXACTLY, not with a +/-1
// tick slop that would hide an off-by-one (decrement-before-compare, or >= vs
// >). The realistic-values test above must tolerate +/-1 for the fp boundary;
// this one removes the fp entirely by driving the pure machine with dt=1 and
// easeback=5 — exact integers in double (5-1-1-1-1-1 == 0 with zero rounding) —
// so the first live tick is deterministic. (S4a red-team lesson: pin the
// predicate with an exactly-representable magnitude, not a derived time.)
// ---------------------------------------------------------------------------
TEST_CASE("AT-8: the ease-back window length is exact (no off-by-one)") {
    input::Freelook fl;
    const double dt = 1.0, E = 5.0;  // exact in double
    REQUIRE_FALSE(fl.step(true, false, dt, E).mouse_aim_live);  // in freelook
    // Release: ticks 0..4 suspended (easeback 5->4->3->2->1->0), live at
    // tick 5.
    int first_live = -1;
    for (int i = 0; i < 12; ++i) {
        const bool live = fl.step(false, false, dt, E).mouse_aim_live;
        if (live && first_live < 0) first_live = i;
        if (first_live >= 0)
            CHECK(live);  // once live, stays live
        else
            CHECK_FALSE(live);  // before that, exactly suspended
    }
    CHECK(first_live == 5);  // exact: an off-by-one shifts this to 4 or 6
}

// ---------------------------------------------------------------------------
// Focus loss / crash-reset (SPEC §9.5 robustness): reset() drops every latch so
// an alt-tab mid-freelook can't resume a stale hold and a respawn never
// inherits the previous life's freelook state.
// ---------------------------------------------------------------------------
TEST_CASE("4d freelook: reset drops all latches") {
    input::Freelook fl;
    const double dt = kAp.sim_dt, E = kCp.freelook_easeback_time;
    fl.step(true, true, dt, E);    // override used, freelook held
    fl.step(false, false, dt, E);  // release -> easeback armed + latch state
    fl.reset();
    CHECK_FALSE(fl.override_used);
    CHECK_FALSE(fl.freelook_prev);
    CHECK(fl.easeback == 0.0);
    // Post-reset behaves like a fresh machine: a clean release does not snap.
    fl.step(true, false, dt, E);
    CHECK_FALSE(fl.step(false, false, dt, E).snap_to_nose);
}

// ---------------------------------------------------------------------------
// AT-13 / 4d: a GROUNDED (spawn/reset) tick must reset the CALLER-side freelook
// latches and the aim, not just the in-core controller. The harness ClosedLoop
// is the caller-of-record; the app will do the IDENTICAL pairing when it wires
// the instructor, so modelling it here is what keeps the two callers from
// drifting (input::Freelook is a shared owner precisely to prevent that,
// CLAUDE.md 4d). Without the pairing a reset mid-ease-back inherits the prior
// life's stale off-nose aim and its still-running ease-back window.
// ---------------------------------------------------------------------------
TEST_CASE("4d: a grounded reset tick resets the caller freelook and aim") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    double thr = 0.7;
    const sim::SimState s0 =
        harness::level_trim_state(kAp, 140.0, 3000.0, up, heading, &thr);
    harness::ClosedLoop cl(s0, glm::dvec3{0.0, 0.0, -1.0});
    cl.aim_nose();
    for (int i = 0; i < 120; ++i) cl.tick(thr, kAp, kCp);  // settle

    // Command a 40 deg-right aim, check six briefly (aim held off-nose), then
    // release freelook: the ease-back window arms and the mouse suspends.
    const glm::dvec3 nose = cl.state.orientation * glm::dvec3{0.0, 0.0, -1.0};
    const glm::dvec3 up_b = cl.state.orientation * glm::dvec3{0.0, 1.0, 0.0};
    cl.aim = glm::normalize(glm::angleAxis(-rad(40.0), up_b) * nose);
    cl.hold_freelook();
    for (int i = 0; i < 5; ++i) cl.tick(thr, kAp, kCp);
    cl.release_freelook();
    cl.tick(thr, kAp,
            kCp);  // release edge: ease-back armed, aim still off-nose

    // Preconditions for a meaningful test: mid-ease, aim well off the nose.
    const glm::dvec3 nose_pre =
        cl.state.orientation * glm::dvec3{0.0, 0.0, -1.0};
    REQUIRE(cl.fl.easeback > 0.0);
    REQUIRE_FALSE(cl.mouse_aim_live);
    REQUIRE(glm::dot(cl.aim, nose_pre) < std::cos(rad(20.0)));

    // A GROUNDED tick (as at spawn/reset): the caller drops the freelook
    // latches and re-aims to the nose, mirroring the in-core control::reset().
    cl.tick(thr, kAp, kCp, /*grounded=*/true);
    const glm::dvec3 nose_now =
        cl.state.orientation * glm::dvec3{0.0, 0.0, -1.0};
    CHECK(cl.fl.easeback == 0.0);                            // window dropped
    CHECK_FALSE(cl.fl.override_used);                        // latch cleared
    CHECK(cl.mouse_aim_live);                                // mouse live again
    CHECK(glm::dot(cl.aim, nose_now) > std::cos(rad(3.0)));  // aim := nose
    // In-core GROUNDED reset the controller too (assert the returned Internal,
    // not telem.pursuit — the grounded path leaves that at its default).
    CHECK(cl.internal.pursuit);
    CHECK(cl.internal.regime == control::Regime::FINE);
}

// ===========================================================================
// AT-8 — closed-loop through the REAL sim::step() (HARNESS §3).
// ===========================================================================

// ---------------------------------------------------------------------------
// AT-8: freelook keeps flying the LAST COMMANDED turn. With no override and no
// mouse input, freelook is a pure no-op on the FLIGHT — it only changes where
// the mouse would go (camera vs aim). So a freelooking run and a
// non-freelooking run from the same state, given the same commanded aim, must
// fly BIT-IDENTICALLY; only the mouse->aim gate differs. This is both the
// "keeps flying the turn" proof and the strict-superset proof for the flight.
// ---------------------------------------------------------------------------
TEST_CASE("AT-8: freelook (no override) is a pure no-op on the flight") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    double thr = 0.7;
    const sim::SimState s0 =
        harness::level_trim_state(kAp, 140.0, 3000.0, up, heading, &thr);

    harness::ClosedLoop ref(s0, glm::dvec3{0.0, 0.0, -1.0});  // never freelooks
    harness::ClosedLoop look(s0,
                             glm::dvec3{0.0, 0.0, -1.0});  // enters freelook
    ref.aim_nose();
    look.aim_nose();

    // Settle, then command an identical 20 deg up + 15 deg right maneuver on
    // BOTH — a gentle pull+bank that never approaches the 90 deg lateral-roll
    // regime (cos_phi_theta stays > 0 throughout).
    for (int i = 0; i < 120; ++i) {
        ref.tick(thr, kAp, kCp);
        look.tick(thr, kAp, kCp);
    }
    auto command = [](harness::ClosedLoop& cl) {
        const glm::dvec3 nose =
            cl.state.orientation * glm::dvec3{0.0, 0.0, -1.0};
        const glm::dvec3 right =
            cl.state.orientation * glm::dvec3{1.0, 0.0, 0.0};
        const glm::dvec3 up_b =
            cl.state.orientation * glm::dvec3{0.0, 1.0, 0.0};
        glm::dvec3 a = glm::angleAxis(rad(20.0), right) * nose;
        a = glm::angleAxis(-rad(15.0), up_b) * a;
        cl.aim = glm::normalize(a);
    };
    command(ref);
    command(look);

    // `look` is mid-maneuver when the pilot checks six.
    look.hold_freelook();
    const control::Telemetry t_entry = look.tick(thr, kAp, kCp);
    ref.tick(thr, kAp, kCp);
    const glm::dvec3 aim_entry = look.aim;
    const double e_entry = t_entry.e;
    REQUIRE(e_entry > rad(15.0));  // a real commanded maneuver to keep flying

    for (int i = 0; i < 90; ++i) {  // 0.75 s of checking six
        const control::Telemetry tl = look.tick(thr, kAp, kCp);
        const control::Telemetry tr = ref.tick(thr, kAp, kCp);
        REQUIRE(finite_state(look.state));

        // The flight is bit-identical to the non-freelooking reference: same
        // Inputs, same state. Freelook changed nothing about the turn.
        CHECK(look.last_inputs.pitch == ref.last_inputs.pitch);
        CHECK(look.last_inputs.roll == ref.last_inputs.roll);
        CHECK(look.last_inputs.yaw == ref.last_inputs.yaw);
        CHECK(look.state.position == ref.state.position);
        CHECK(look.state.orientation == ref.state.orientation);

        // ...but the mouse is on the camera, not the aim.
        CHECK_FALSE(look.mouse_aim_live);
        CHECK(tl.pursuit == true);  // pursuit never suspended (no override)
        CHECK(tr.pursuit == true);
    }
    // The aim was HELD in world space (only parallel-transported: < ~1 deg over
    // this window), and the instructor made real progress flying to it.
    CHECK(glm::dot(look.aim, aim_entry) > std::cos(rad(2.0)));
    CHECK(look.tick(thr, kAp, kCp).e <
          e_entry);  // still pursuing the held turn
}

// ---------------------------------------------------------------------------
// AT-8: freelook COMPOSES with a keyboard override (SPEC §9.5: "freelook +
// keyboard roll must compose"), and the first override of the hold snaps
// aim := nose. Held mid-maneuver with the aim well off the nose, pressing roll
// must: drive the roll axis through the override (mask reaches the core,
// pursuit suspended) AND collapse the pointing error to ~0 because aim snapped
// to the current nose that instant.
// ---------------------------------------------------------------------------
TEST_CASE(
    "AT-8: freelook composes with a keyboard roll override, snaps to nose") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    double thr = 0.7;
    const sim::SimState s0 =
        harness::level_trim_state(kAp, 140.0, 3000.0, up, heading, &thr);
    harness::ClosedLoop cl(s0, glm::dvec3{0.0, 0.0, -1.0});
    cl.aim_nose();
    for (int i = 0; i < 120; ++i) cl.tick(thr, kAp, kCp);  // settle

    // Command a 50 deg RIGHT aim and check six the SAME tick, so the held aim
    // stays well off the nose for the next few ticks.
    const glm::dvec3 nose = cl.state.orientation * glm::dvec3{0.0, 0.0, -1.0};
    const glm::dvec3 up_b = cl.state.orientation * glm::dvec3{0.0, 1.0, 0.0};
    cl.aim = glm::normalize(glm::angleAxis(-rad(50.0), up_b) * nose);
    cl.hold_freelook();

    control::Telemetry t{};
    for (int i = 0; i < 3; ++i)
        t = cl.tick(thr, kAp, kCp);  // aim held, off-nose
    const double e_pre = t.e;
    REQUIRE(e_pre > rad(30.0));        // the parked cursor is far off the nose
    REQUIRE_FALSE(cl.mouse_aim_live);  // freelook: mouse on the camera

    // Seize roll while freelooking: compose + snap.
    cl.hold_override(kRoll, 1.0);
    t = cl.tick(thr, kAp, kCp);
    REQUIRE(finite_state(cl.state));

    // Snap fired: aim := nose that instant -> the pointing error collapsed.
    CHECK(t.e < rad(2.0));
    CHECK(e_pre - t.e > rad(25.0));  // separated from the no-snap case
    // Compose: the roll override reached the core (roll Input is the engage
    // ramp, signed +1), and pursuit is suspended by the held key.
    CHECK(cl.last_inputs.roll > 0.0f);
    CHECK_FALSE(t.pursuit);
    // Still freelook: the mouse stays on the camera even with the key down.
    CHECK_FALSE(cl.mouse_aim_live);

    // A couple more ticks: the ramp keeps climbing, no re-snap, finite.
    const float roll_prev = cl.last_inputs.roll;
    t = cl.tick(thr, kAp, kCp);
    CHECK(cl.last_inputs.roll > roll_prev);  // override engage still ramping
    CHECK(t.e < rad(3.0));  // aim stays ~ nose (no second snap)
}

// ---------------------------------------------------------------------------
// AT-8: release WITHOUT an override leaves the held turn untouched — the aim is
// NOT reset to the nose (contrast the conditional-reset direct test). The plane
// is still pursuing an off-nose aim at release, so the error stays large; a
// spurious snap-back would collapse it to ~0. Also: the ease-back suspends the
// mouse on the way back.
// ---------------------------------------------------------------------------
TEST_CASE("AT-8: freelook release without override keeps the held aim") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    double thr = 0.7;
    const sim::SimState s0 =
        harness::level_trim_state(kAp, 140.0, 3000.0, up, heading, &thr);
    harness::ClosedLoop cl(s0, glm::dvec3{0.0, 0.0, -1.0});
    cl.aim_nose();
    for (int i = 0; i < 120; ++i) cl.tick(thr, kAp, kCp);  // settle

    // 30 deg UP aim (a pull — no hard lateral roll, cos_phi_theta stays > 0),
    // freelook the same tick and hold briefly so the nose is still climbing
    // toward it at release (error still large).
    const glm::dvec3 nose = cl.state.orientation * glm::dvec3{0.0, 0.0, -1.0};
    const glm::dvec3 right = cl.state.orientation * glm::dvec3{1.0, 0.0, 0.0};
    cl.aim = glm::normalize(glm::angleAxis(rad(30.0), right) * nose);
    cl.hold_freelook();

    control::Telemetry t{};
    for (int i = 0; i < 12; ++i) {
        t = cl.tick(thr, kAp, kCp);
        REQUIRE(finite_state(cl.state));
        REQUIRE(t.extracted.cos_phi_theta > 0.0);  // no inverted transient
    }
    const double e_before = t.e;
    REQUIRE(e_before > rad(15.0));  // still well off the held aim

    // Release with NO override used: aim is NOT snapped to nose.
    cl.release_freelook();
    t = cl.tick(thr, kAp, kCp);
    REQUIRE(finite_state(cl.state));
    // The error is still large (had it snapped to nose, it would be ~0).
    CHECK(t.e > rad(12.0));
    // The aim really is still the ~30 deg-off held target, not the nose.
    const glm::dvec3 nose_now =
        cl.state.orientation * glm::dvec3{0.0, 0.0, -1.0};
    CHECK(glm::dot(cl.aim, nose_now) < std::cos(rad(12.0)));
    // Ease-back suspends the mouse on the way home (CQ2).
    CHECK_FALSE(cl.mouse_aim_live);
}

// ---------------------------------------------------------------------------
// AT-8: the no-inverted-transient clause (SPEC §16 CQ2 / §9.5). Fly the full
// freelook-with-override arc — check six, seize pitch (snap := nose), pull the
// nose off, let the key up, come home — and require the instructor never
// commands an inverted/surprising attitude anywhere: cos_phi_theta > 0 every
// tick, state finite, and on the freelook release the aim conditionally resets
// to the nose (override was used) so the return is a gentle catch, not a jolt.
// ---------------------------------------------------------------------------
TEST_CASE("AT-8: full freelook+override arc shows no inverted transient") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    double thr = 0.7;
    const sim::SimState s0 =
        harness::level_trim_state(kAp, 140.0, 3000.0, up, heading, &thr);
    harness::ClosedLoop cl(s0, glm::dvec3{0.0, 0.0, -1.0});
    cl.aim_nose();
    for (int i = 0; i < 120; ++i) cl.tick(thr, kAp, kCp);  // settle

    auto require_sane = [&](const control::Telemetry& t) {
        REQUIRE(finite_state(cl.state));
        REQUIRE(std::isfinite(t.e));
        // Never rolls past 90 deg (the fold): no inverted transient.
        REQUIRE(t.extracted.cos_phi_theta > 0.0);
    };

    // Check six.
    cl.hold_freelook();
    for (int i = 0; i < 30; ++i) require_sane(cl.tick(thr, kAp, kCp));

    // Seize pitch (snaps aim := nose) and pull the nose up off the aim.
    cl.hold_override(kPitch, 1.0);
    control::Telemetry t{};
    for (int i = 0; i < 40; ++i) {
        t = cl.tick(thr, kAp, kCp);
        require_sane(t);
        REQUIRE_FALSE(t.pursuit);  // suspended while held
    }
    const double e_built = t.e;
    // The override really flew the nose off the held (freelook) aim. It now
    // does so at the IN-ENVELOPE rate (S7-ovr, bounded) rather than a raw ±1,
    // so it builds less error than the frozen test's 8 deg — but still a real
    // pull.
    REQUIRE(e_built > rad(3.0));

    // Let the key up but stay in freelook: pursuit resumes to the
    // (nose-snapped) aim; the instructor catches under the braking law.
    cl.release_override();
    for (int i = 0; i < 40; ++i) require_sane(cl.tick(thr, kAp, kCp));

    // Come home: release freelook. An override WAS used this hold, so aim :=
    // nose (conditional reset) — the pointing error collapses, a gentle catch.
    cl.release_freelook();
    t = cl.tick(thr, kAp, kCp);
    require_sane(t);
    CHECK(t.e < rad(2.0));  // reset to nose -> no surprise turn on the way back

    // And it flies on cleanly through the ease-back and after.
    for (int i = 0; i < 120; ++i) require_sane(cl.tick(thr, kAp, kCp));
}

// ---------------------------------------------------------------------------
// AT-8: an override held THROUGH the freelook release (the key comes up on a
// LATER tick than Space) composes cleanly. The spec sequences each event but
// not their overlap; the two mechanisms are orthogonal and must not fight:
//   - freelook release fires aim := nose (an override was used this hold) while
//     the key still drives its axis in-core (pursuit stays suspended);
//   - the LATER key release is the ordinary 4c catch — pursuit resumes and
//     phi_held is recaptured at the live bank.
// No inverted transient anywhere. Complements the roll-compose case (which
// releases the key BEFORE freelook) by pinning the opposite ordering — the
// "a contract must hold in EVERY branch that does the gated thing" lesson (4c).
// ---------------------------------------------------------------------------
TEST_CASE(
    "AT-8: an override held through the freelook release composes cleanly") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    double thr = 0.7;
    const sim::SimState s0 =
        harness::level_trim_state(kAp, 140.0, 3000.0, up, heading, &thr);
    harness::ClosedLoop cl(s0, glm::dvec3{0.0, 0.0, -1.0});
    cl.aim_nose();
    for (int i = 0; i < 120; ++i) cl.tick(thr, kAp, kCp);  // settle

    auto require_sane = [&](const control::Telemetry& t) {
        REQUIRE(finite_state(cl.state));
        REQUIRE(std::isfinite(t.e));
        REQUIRE(t.extracted.cos_phi_theta > 0.0);  // no inverted transient
    };

    // Check six (pure freelook, instructor flying).
    cl.hold_freelook();
    for (int i = 0; i < 20; ++i) require_sane(cl.tick(thr, kAp, kCp));

    // Seize pitch while freelooking (snaps aim := nose), fly the nose up off
    // it.
    cl.hold_override(kPitch, 1.0);
    control::Telemetry t{};
    for (int i = 0; i < 40; ++i) {
        t = cl.tick(thr, kAp, kCp);
        require_sane(t);
        REQUIRE_FALSE(t.pursuit);  // held key suspends pursuit
    }
    REQUIRE(t.e > rad(3.0));  // the override really flew the nose off the aim
                              // (in-envelope rate now, S7-ovr — bounded, was 6)

    // Release FREELOOK with the pitch key STILL held.
    cl.release_freelook();
    t = cl.tick(thr, kAp, kCp);
    require_sane(t);
    CHECK(t.e < rad(2.0));   // freelook release snapped aim := nose
    CHECK_FALSE(t.pursuit);  // key still held -> pursuit still suspended
    CHECK(t.omega_des.x >
          0.0);  // the key still drives its axis (in-envelope
                 //   pitch-up rate; S7-ovr — no longer a raw
                 //   ±1, so assert the COMMAND, not |Input|>0.9)
    CHECK_FALSE(cl.mouse_aim_live);  // ease-back suspends the mouse

    // A few more ticks, key STILL held but now OUT of freelook: the override
    // does NOT touch the aim (S7-ovr2 — the mouse owns it), so the nose flies
    // off the just-snapped aim again and the pointing error GROWS. (The
    // keyboard is a supplementary nudge; it never hijacks the aim/reticle —
    // that is what keeps a keypress from swinging the camera.)
    for (int i = 0; i < 20; ++i) {
        t = cl.tick(thr, kAp, kCp);
        require_sane(t);
        REQUIRE_FALSE(t.pursuit);
    }
    REQUIRE(t.e > rad(2.0));  // e grew back: the aim is parked, not re-snapping

    // NOW release the pitch key: the ordinary 4c catch — pursuit resumes and
    // phi_held is recaptured at the live bank, not a stale reference.
    cl.release_override();
    t = cl.tick(thr, kAp, kCp);
    require_sane(t);
    CHECK(t.pursuit);  // pursuit resumed on the key-release edge
    CHECK(cl.internal.held_bank ==
          Catch::Approx(t.extracted.phi).margin(1e-12));

    // Flies on cleanly through the ease-back and the catch.
    for (int i = 0; i < 150; ++i) require_sane(cl.tick(thr, kAp, kCp));
}
