// AT-9 end-to-end (SPEC §10, HARNESS §4): the plant is frame-rate independent.
// The SAME scripted aim sequence driven through the fixed-dt accumulator loop
// at 30 fps vs 240 fps must produce the IDENTICAL sim trajectory — because the
// sim (and controller) advance ONLY in whole sim_dt ticks; frame time never
// reaches the plant. Only the accumulator MECHANISM was unit-pinned before
// (test_accumulator: tick-count within 1, remainder conserved); this drives the
// whole shipped app frame (app::step_frame -> app::tick -> control/sim) and
// asserts the final DOUBLE SimState is bit-identical (S1: assert on the double
// state, not a rendered float — at |p|=R a float ulp is a tick of gravity).
//
// HONEST SCOPE. The guarantee is: identical TICK-INDEXED input => bit-identical
// trajectory (the between-tick dynamics never see frame time). It is NOT that
// arbitrary wall-clock input timing is rate-invariant: mouse is sampled per
// FRAME and consumed on the next tick, so a flick issued mid-frame quantizes to
// the first tick of the next tick-bearing frame — differing across rates by < 1
// frame (input LATENCY, inherent to any fixed-dt loop, not the frame-rate gain
// variance AT-9 forbids). So the scripted flicks are delivered at common tick
// boundaries (see kFlicks): both schedules consume each flick before the SAME
// tick, which is exactly "the same scripted run at 30 vs 240 fps."
//
// MB-aim NOTE: the rate-keyed sensitivity curve (input/aim_curve.h) is applied
// at the DEVICE accrual in main.cpp, UPSTREAM of pending_* — deltas injected
// here are post-curve by construction, so this pin is curve-independent (it
// certifies the tick loop for ANY delivered deltas). The curve's OWN
// frame-rate invariance is pinned separately in test_aim_curve.cpp (the
// power-of-two scale leg + the quantization-guard leg).
//
// DETERMINISM BY CONSTRUCTION. 30 fps = 4*sim_dt and 240 fps = 0.5*sim_dt are
// EXACT power-of-2 multiples of sim_dt, so the accumulator carries ZERO fp
// remainder (4*dt/dt == 4.0 exactly; 0.5*dt + 0.5*dt == dt exactly) — the slow
// run steps exactly 4 ticks/frame, the fast run exactly 1 tick per 2 frames,
// and both reach the identical tick sequence. Hence exact `==`, not a
// tolerance: a ulp band would hide precisely the frame_dt-leak class this test
// hunts.
//
// S-aimff SCOPE AMENDMENT (v4 rung 1). The aim-rate feedforward's smear is
// PARTITION-DEPENDENT BY DESIGN: the consuming tick spreads the frame's mouse
// rotation over its frame's N ticks (rate = rotation/(N*sim_dt)), so a 4-tick
// frame and a 1-tick frame see DIFFERENT per-tick rate sequences for the same
// tick-indexed aim — equal in the INTEGRAL (sum rate*dt = the frame rotation,
// the frame-rate-independence the mechanism guarantees; a per-tick-identical
// rate is unconstructible from per-frame deltas without the spike train the
// smear exists to remove). The BIT-IDENTITY leg therefore runs with
// aim_ff_gain = 0 (structurally inert — the gated add is the bit-identical
// legacy tree; everything else, aim path included, still compared exact) so
// the sharp frame_dt-leak tripwire keeps its teeth on the whole remaining
// pipeline. The SHIPPED-GAIN leg below pins the FF path by its own honest
// law: aim quaternion still bit-identical, the controller-seen rate INTEGRAL
// equal across rates, and the trajectories close (a gross-divergence
// tripwire). The per-partition smear invariant itself is pinned in
// test_aim_ff (1x4-tick vs 4x1-tick frames).

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <cstdio>
#include <glm/gtc/quaternion.hpp>
#include <vector>

#include "app/instructor_tick.h"
#include "app/loop.h"
#include "config/load_aircraft.h"
#include "config/load_controller.h"
#include "sim/world.h"
#include "test/harness/instructor.h"

#ifdef NDEBUG
#error "SEADS gate requires an assert-live build (SPEC 6.1)"
#endif

namespace {

const sim::AircraftParams kAp =
    cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");
const control::ControllerParams kCp =
    cfg::load_controller_toml(SEADS_CONFIG_DIR "/controller.toml", kAp);

// A scripted mouse flick: apply (dx,dy) once, delivered so BOTH schedules
// consume it before sim-tick `tick`. `tick` MUST be a multiple of 4 (the slow
// run's ticks-per-frame) so the slow run's frame boundary lands on it; the fast
// run then consumes it on the tick-bearing frame that produces `tick`.
struct Flick {
    long tick;
    double dx;
    double dy;
};
const std::vector<Flick> kFlicks = {
    {160, +300.0, 0.0},   // yaw right
    {320, 0.0, +200.0},   // pitch
    {480, -250.0, +80.0}  // yaw back-left + pitch
};
constexpr long kTotalTicks = 600;  // 5 s at sim_dt = 1/120

// An airborne, flying LoopState — grounded=false, so no spawn/GROUNDED drop
// perturbs the comparison (the spawn tick is pinned separately in
// test_instructor_tick). Identical for both runs.
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

struct RunResult {
    app::LoopState loop;
    long total_ticks = 0;
    size_t flicks_delivered = 0;  // guard: a mis-scheduled flick drops silently
    // S-aimff: the controller-seen aim-rate integral over the whole run
    // (sum of FrameResult::aim_rate_dt_sum) — the smear invariant's probe.
    glm::dvec3 rate_integral{0.0};
    // S-rimshot (red-team P1-1, the AT-15 silent-disarm class): the MAX
    // capture-machine state observed across the run, as an int of the ordered
    // enum (IDLE < CARRY < RETURN). The 15 m divergence bound below is
    // JUSTIFIED by the event firing in both partitions; under an
    // aim_sensitivity/engage_frac/w_eps retune the trigger could go
    // stillborn (never fire), and the widened bound loses its premise — so
    // the legs REQUIRE this reached >= CARRY. Sampled from Internal.capture
    // at each FRAME boundary (== the last tick's Telemetry.capture mirror,
    // controller.cpp); frame sampling cannot miss the event — CARRY+RETURN
    // occupy dozens of ticks vs the slow run's 4-tick frames.
    int max_capture = 0;
};

// Drive `n_frames` frames of fixed `frame_dt` through app::step_frame from
// `s0`. Before each frame, if the cumulative tick count so far equals a
// not-yet- delivered flick's tick, accrue its delta into pending — the SAME
// rule for both rates: in the fast run cumulative==tick is first reached on a
// 0-TICK frame, so the delta CARRIES (Fable AT-9 consult P0-1 #3: exercise
// pending-carry) to the next frame's producing tick; in the slow run it lands
// on the first of the 4. Either way it is consumed before the identical tick.
// `apply_flicks=false` runs the null trajectory for the efficacy companion.
RunResult run_at(double frame_dt, int n_frames, double throttle,
                 const sim::SimState& s0, bool apply_flicks = true,
                 const control::ControllerParams& cp = kCp) {
    RunResult r;
    r.loop = flying(s0);
    app::Accumulator accum(kAp.sim_dt);
    double pending_dx = 0.0, pending_dy = 0.0;
    std::vector<bool> delivered(kFlicks.size(), false);
    long cum = 0;

    app::FrameInput fin;
    fin.throttle = throttle;  // hands off the stick; flick only via mouse->aim

    for (int f = 0; f < n_frames; ++f) {
        if (apply_flicks) {
            for (size_t j = 0; j < kFlicks.size(); ++j) {
                if (!delivered[j] && cum == kFlicks[j].tick) {
                    pending_dx += kFlicks[j].dx;
                    pending_dy += kFlicks[j].dy;
                    delivered[j] = true;
                }
            }
        }
        const app::FrameResult fr = app::step_frame(
            r.loop, accum, frame_dt, fin, pending_dx, pending_dy, kAp, cp);
        cum += fr.ticks;
        r.rate_integral += fr.aim_rate_dt_sum;
        r.max_capture =
            std::max(r.max_capture, static_cast<int>(r.loop.internal.capture));
    }
    r.total_ticks = cum;
    for (bool d : delivered)
        if (d) ++r.flicks_delivered;
    return r;
}

bool exact_eq(const glm::dvec3& a, const glm::dvec3& b) {
    return a.x == b.x && a.y == b.y && a.z == b.z;
}
bool exact_eq(const glm::dquat& a, const glm::dquat& b) {
    return a.w == b.w && a.x == b.x && a.y == b.y && a.z == b.z;
}

}  // namespace

TEST_CASE("AT-9: 30 fps and 240 fps fly the bit-identical trajectory") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    double thr = 0.7;
    const sim::SimState s0 =
        harness::level_trim_state(kAp, 150.0, 4000.0, up, heading, &thr);

    // S-aimff scope amendment (header comment): the FF smear is partition-
    // dependent by design, so THIS leg — the sharp bit-identity tripwire —
    // runs it structurally OFF (gain 0 = the gated add's bit-identical legacy
    // tree). The shipped-gain frame-rate law is pinned in the leg below.
    control::ControllerParams cp0 = kCp;
    cp0.aim_ff_gain = 0.0;
    // Machine retired in the committed table (carry 0); this leg pins the
    // PARKED machinery for the walk-back — it claims the capture MACHINE is
    // frame-rate bit-identical, so it must SELF-ARM at the walk-back value
    // (the REQUIREs below verify the event actually fired in both runs).
    cp0.capture_carry = 1.0;

    // 30 fps = 4*sim_dt (exact): 150 frames * 4 = 600 ticks.
    // 240 fps = 0.5*sim_dt (exact): 1200 frames, 1 tick / 2 frames = 600 ticks.
    const RunResult slow = run_at(4.0 * kAp.sim_dt, 150, thr, s0, true, cp0);
    const RunResult fast = run_at(0.5 * kAp.sim_dt, 1200, thr, s0, true, cp0);

    const double pos_err =
        glm::length(slow.loop.curr.position - fast.loop.curr.position);
    std::printf(
        "[AT-9] slow_ticks=%ld fast_ticks=%ld  pos_err=%.17e m  "
        "integ_eq=%d aimq_eq=%d\n",
        slow.total_ticks, fast.total_ticks, pos_err,
        exact_eq(slow.loop.internal.integ, fast.loop.internal.integ),
        exact_eq(slow.loop.aim.q, fast.loop.aim.q));

    // Companion 1 (Fable P0-1): exact total tick count in BOTH runs. A pure
    // `==` between the two runs is satisfied by any RATE-SYMMETRIC mutation
    // (e.g. a wrong accumulator dt makes both wrong-but-identical); pinning the
    // count to the wall-time-derived truth kills that class.
    CHECK(slow.total_ticks == kTotalTicks);
    CHECK(fast.total_ticks == kTotalTicks);
    // S-rimshot premise (red-team P1-1): this leg claims to pin the capture
    // MACHINE bit-identically across partitions with carry live — that claim
    // is vacuous unless the event actually fired in both runs (an
    // engage_frac/w_eps retune that leaves the trigger stillborn would
    // silently disarm it — the universal trigger has no size gate, so a
    // scripted step that settles MUST engage at its taper knee).
    REQUIRE(slow.max_capture >= static_cast<int>(control::CaptureState::CARRY));
    REQUIRE(fast.max_capture >= static_cast<int>(control::CaptureState::CARRY));
    // All scripted flicks actually landed (a `tick` not % 4 or >= kTotalTicks
    // would drop from BOTH runs and the `==` below would still pass — P2-2).
    CHECK(slow.flicks_delivered == kFlicks.size());
    CHECK(fast.flicks_delivered == kFlicks.size());

    // The trajectory is bit-identical — compared LoopState-wide (Fable P2-1: a
    // frame_dt leak shows in the controller internals first), exact `==`.
    CHECK(exact_eq(slow.loop.curr.position, fast.loop.curr.position));
    CHECK(exact_eq(slow.loop.curr.velocity, fast.loop.curr.velocity));
    CHECK(exact_eq(slow.loop.curr.orientation, fast.loop.curr.orientation));
    CHECK(exact_eq(slow.loop.curr.angular_vel, fast.loop.curr.angular_vel));
    CHECK(slow.loop.curr.throttle == fast.loop.curr.throttle);
    CHECK(exact_eq(slow.loop.internal.integ, fast.loop.internal.integ));
    CHECK(exact_eq(slow.loop.aim.q, fast.loop.aim.q));

    // Companion 2 (Fable P0-1): EFFICACY. A pure `==` also survives dropping
    // the mouse->aim entirely (both runs then fly the same hands-off path).
    // Assert the scripted flicks actually moved the flight vs a null-input run,
    // and the aim left the nose — so a dropped-mouse / dropped-carry mutation
    // breaks the bit-identity (the fast run delivers on a 0-tick frame) AND
    // this efficacy.
    const RunResult null_run =
        run_at(4.0 * kAp.sim_dt, 150, thr, s0, false, cp0);
    const double flick_effect =
        glm::length(slow.loop.curr.position - null_run.loop.curr.position);
    const glm::dvec3 nose =
        glm::normalize(s0.orientation * glm::dvec3{0, 0, -1});
    std::printf("[AT-9] flick_effect=%.3f m  aim.nose=%.4f\n", flick_effect,
                glm::dot(slow.loop.aim.forward(), nose));
    CHECK(flick_effect > 1.0);  // the scripted aim sequence changed the flight
    CHECK(glm::dot(slow.loop.aim.forward(), nose) < 0.99);  // aim left the nose
}

// S-aimff (v4 rung 1): the SHIPPED-GAIN frame-rate law. With the feedforward
// live (the toml gain), the per-tick rate sequences legitimately differ across
// frame partitions (the smear — header comment) and the CLOSED-LOOP
// trajectories therefore diverge slightly; everything downstream of the state
// (the transported aim, the WORLD direction of each frame's rate vector)
// inherits that divergence, so no bitwise claim survives here. The honest
// cross-rate invariants:
//   1. the controller-seen rate INTEGRAL (sum aim_rate*dt, the
//      FrameResult::aim_rate_dt_sum instrument) is equal across rates up to
//      the trajectory-divergence rotation of the world axes — the frame-rate
//      independence the smear guarantees (the SHARP fixed-horizon partition
//      leg lives in test_aim_ff);
//   2. the trajectories and the aim stay CLOSE (gross-divergence tripwires;
//      the bounds document the accepted smear-vs-spike transient scale, they
//      are NOT precision claims).
// Red-team P2-2, honest scope: the per-tick smear SHAPE (rotation/(N*sim_dt),
// ZOH across the frame) is pinned by test_aim_ff legs 6/7 + the ref_compose
// 2-tick frame — NOT by the integral equality here (a naive per-frame spike
// train shares the integral by construction).
TEST_CASE("AT-9 aim-ff: shipped gain keeps the cross-rate integral and aim") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    double thr = 0.7;
    const sim::SimState s0 =
        harness::level_trim_state(kAp, 150.0, 4000.0, up, heading, &thr);
    REQUIRE(kCp.aim_ff_gain > 0.0);  // premise: the shipped table flies the FF
    // Machine retired in the committed table (carry 0); this leg pins the
    // PARKED machinery for the walk-back — its widened 15 m bound is
    // JUSTIFIED by the capture event firing, so it self-arms at the
    // walk-back value (the REQUIREs below keep that justification honest).
    control::ControllerParams cp_arm = kCp;
    cp_arm.capture_carry = 1.0;

    const RunResult slow = run_at(4.0 * kAp.sim_dt, 150, thr, s0, true, cp_arm);
    const RunResult fast =
        run_at(0.5 * kAp.sim_dt, 1200, thr, s0, true, cp_arm);
    CHECK(slow.total_ticks == kTotalTicks);
    CHECK(fast.total_ticks == kTotalTicks);
    CHECK(slow.flicks_delivered == kFlicks.size());
    CHECK(fast.flicks_delivered == kFlicks.size());
    // S-rimshot premise (red-team P1-1): the widened 15 m bound below is
    // justified by the capture event firing in BOTH partitions — REQUIRE it
    // actually reached the event (>= CARRY), or an engage_frac/w_eps/
    // aim_sensitivity retune silently retires the bound's justification
    // while it keeps passing (the AT-15 silent-disarm class).
    REQUIRE(slow.max_capture >= static_cast<int>(control::CaptureState::CARRY));
    REQUIRE(fast.max_capture >= static_cast<int>(control::CaptureState::CARRY));

    // 1. The smear invariant across REAL rates: equal integrals up to the
    // world-axis rotation of the diverged trajectories (measured 3.4e-6
    // relative; bound 30x above it, still 1000x below any real drop/double-
    // count, which shows at O(1) relative). Non-trivially exercised — a
    // dropped instrument reads 0=0 and proves nothing.
    const double mag = glm::length(slow.rate_integral);
    const double diff = glm::length(slow.rate_integral - fast.rate_integral);
    const double pos_err =
        glm::length(slow.loop.curr.position - fast.loop.curr.position);
    const double aim_dot =
        glm::dot(slow.loop.aim.forward(), fast.loop.aim.forward());
    std::printf(
        "[AT-9 aim-ff] rate_integral=%.6f rad  cross-rate diff=%.3e  "
        "pos_err=%.3f m  aim_dot=%.9f\n",
        mag, diff, pos_err, aim_dot);
    REQUIRE(mag > 0.01);  // the instrument actually measured the flicks
    CHECK(diff <= 1e-4 * mag);

    // 2. Gross-divergence tripwires. Accepted scale history: ~0.7 m pre-
    // rimshot; ~5.2 m at the shipped S-rimshot dial (v4 rung 2) — every
    // settling flick now engages the UNIVERSAL trigger at its taper knee,
    // so the capture event legitimately
    // fires in BOTH partitions (same law both rates — the gain-0 leg above
    // pins the machine itself bit-identical across partitions), and its
    // DISCRETE transitions (engage/rim/center, each a threshold on the
    // smear-diverged state) can land a tick apart between rates, amplifying
    // the accepted FF-smear divergence chaotically. The bound documents that
    // scale with ~3x headroom; the failure mode this tripwire exists for — a
    // frame-rate-DEPENDENT law (dropped mouse, un-smeared spike) — shows at
    // the flick_effect scale (hundreds of m, the efficacy companion above),
    // still ~50x past this bound. NOT a precision claim.
    CHECK(pos_err < 15.0);
    CHECK(aim_dot > 1.0 - 1e-5);  // aim forwards within ~0.26 deg
}

// ===========================================================================
// step_frame crash-mid-frame neutralization (Fable AT-9 consult P0-2). A crash
// that fires on an EARLY tick of a multi-tick frame must fly the REMAINING
// ticks of that frame on a dead stick — the fresh airframe cannot be steered by
// the pre-crash input. If that neutralization were deferred to the caller (only
// the `respawned` flag returned), the residual ticks would fly the dead life's
// stick, and HOW MANY residual ticks there are is frame-rate dependent (3 at
// 30 fps, 0 at 240) — the exact dependence AT-9 forbids, so it must live INSIDE
// the seam.
//
// Drive ONE 30 fps frame (4 ticks) from a steep straight-down dive that crashes
// on tick 1, leaving ticks 2-3 post-respawn. Run it twice: once holding a hard
// ROLL override (roll doesn't deflect a vertical dive, so both crash on the
// SAME tick), once hands-off. Both respawn to the identical fixed spawn_state,
// so the pre-crash tick-0 divergence is WIPED; the final states are equal IFF
// the residual ticks flew neutralized. A mutant that lets the held override
// reach the post-respawn ticks diverges the held run from the hands-off run.
// ===========================================================================
namespace {
// One 30 fps frame (4 ticks) from a steep straight-down dive that crashes on
// tick 1, driven by `fin`. Returns the full LoopState (so the fl latches are
// inspectable). The dive is ~1 m/tick against 0.5 m of slack, and neither a
// roll override nor a pitch stick deflects a vertical dive in one tick — so the
// crash lands on tick 1 regardless of `fin`, leaving ticks 2-3 post-respawn.
app::LoopState post_crash_frame(const app::FrameInput& fin) {
    const glm::dvec3 up{-1.0, 0.0, 0.0},
        heading{0.0, 0.0, -1.0};  // -X antipode
    sim::SimState s = harness::level_state(kAp, 120.0, 1.5, up, heading);
    s.velocity = 120.0 * (-up);  // straight down: ~1 m/tick, crashes on tick 1
    s.last_vhat = -up;
    app::LoopState loop = flying(s);
    app::Accumulator accum(kAp.sim_dt);
    double pdx = 0.0, pdy = 0.0;
    const app::FrameResult fr =
        app::step_frame(loop, accum, 4.0 * kAp.sim_dt, fin, pdx, pdy, kAp, kCp);
    REQUIRE(fr.ticks == 4);  // exact 4-tick frame regardless of the input
    REQUIRE(fr.respawned);   // the dive crashed within the frame
    return loop;
}

app::FrameInput crash_in_instr(bool hold_roll, bool freelook) {
    app::FrameInput fin;
    fin.throttle = 0.3;
    fin.freelook_held = freelook;
    fin.override_mask[2] = hold_roll;  // full roll override, or none
    fin.override_sign[2] = hold_roll ? +1.0 : 0.0;
    // MB-flaps: command the devices down INTO the crash on the held arm only
    // (a one-tick pre-crash slew doesn't deflect a vertical dive within the
    // 0.5 m slack) — the residual post-respawn ticks must fly them
    // NEUTRALIZED (spawn is clean). Diff red-team P2-1: the neutralization
    // was dead code to the suite before this (mutant survived).
    fin.flap_cmd = hold_roll ? 1.0 : 0.0;
    fin.gear_cmd = hold_roll ? 1.0 : 0.0;
    return fin;
}
app::FrameInput crash_in_raw(double pitch_stick) {
    app::FrameInput fin;
    fin.raw_mode = true;
    fin.raw_in.throttle = 0.3f;
    fin.raw_in.pitch = static_cast<float>(pitch_stick);  // full pull, or none
    return fin;
}

// The ABSOLUTE reference (Fable AT-9 impl consult P1-2): the crash fires on
// tick 1 of the 4-tick frame, leaving 2 residual ticks (tick 2 GROUNDED, tick 3
// flying), both flown hands-off at spawn (cruise) power. The stale aim/internal
// are irrelevant — the GROUNDED tick 2 resets them (S5 pairing) — so the result
// is deterministic from spawn_state alone. Pins the COMMON-mode neutralization
// (throttle/freelook/raw) the held-vs-off differential is blind to, AND the
// residual-tick count / crash tick (a config drift moving the crash tick fails
// loudly here instead of silently masking — retires P2-1). `raw` selects the
// raw-mode advance for the raw arm's reference.
sim::SimState expected_post_crash(bool raw) {
    app::LoopState e;
    e.curr = app::spawn_state(kAp);
    e.prev = e.curr;
    e.prev_up = sim::local_up(e.curr.position);
    e.grounded = true;
    app::TickInput ho;  // hands-off: no override, freelook off, no raw stick
    ho.raw_mode = raw;
    ho.throttle = e.curr.throttle;  // reborn at cruise (F2)
    ho.raw_in.throttle = static_cast<float>(e.curr.throttle);
    app::tick(e, ho, kAp, kCp);  // tick 2: GROUNDED
    app::tick(e, ho, kAp, kCp);  // tick 3: flying, hands-off
    return e.curr;
}
}  // namespace

TEST_CASE(
    "step_frame: a mid-frame crash flies the residual ticks neutralized") {
    // Hold a roll override AND freelook into the dive; hands-off comparison.
    const app::LoopState held = post_crash_frame(crash_in_instr(true, true));
    const app::LoopState off = post_crash_frame(crash_in_instr(false, false));
    const double omega_err =
        glm::length(held.curr.angular_vel - off.curr.angular_vel);
    std::printf("[AT-9 neutralize] omega_err(held vs off)=%.3e\n", omega_err);

    // Differential — pins the override-axis neutralization (the held override
    // must not steer the fresh airframe; mutant: held != off). Attitude, not
    // just position: over 1-2 post-respawn ticks position barely moves (S1).
    CHECK(exact_eq(held.curr.position, off.curr.position));
    CHECK(exact_eq(held.curr.orientation, off.curr.orientation));
    CHECK(exact_eq(held.curr.angular_vel, off.curr.angular_vel));

    // Absolute — both equal the hands-off-from-spawn reference. Pins the
    // COMMON-mode throttle neutralization the differential is blind to (M2:
    // drop cur.throttle flies the residual at 0.3 not cruise, survives
    // held==off).
    const sim::SimState exp = expected_post_crash(false);
    CHECK(exact_eq(held.curr.position, exp.position));
    CHECK(exact_eq(held.curr.orientation, exp.orientation));
    CHECK(exact_eq(held.curr.velocity, exp.velocity));
    CHECK(held.curr.throttle == exp.throttle);
    // MB-flaps: the held arm commanded flap/gear down into the crash — the
    // residual ticks flew them neutralized, so the reborn devices match the
    // clean hands-off reference exactly (P2-1: a dropped cur.flap_cmd reset
    // slews them on the fresh airframe and diverges here).
    CHECK(held.curr.flap == exp.flap);
    CHECK(held.curr.gear == exp.gear);
    CHECK(exp.flap == 0.0);  // the reference really is clean (non-vacuous)
    CHECK(exact_eq(off.curr.orientation, exp.orientation));

    // The freelook latches were reset across the respawn — a SimState mirror is
    // blind to this (no pending mouse ⇒ aim-held vs live is identical here), so
    // assert fl directly (M5: drop cur.freelook_held leaves freelook_prev set).
    CHECK(held.fl.freelook_prev == false);
    CHECK(held.fl.override_used == false);
    CHECK(held.fl.easeback == 0.0);
}

// The crash predicate + neutralization are MODE-COMMON: raw mode reaches them
// too (the F1 debug mode Sections 1-3 flew). A held raw stick must NOT steer
// the fresh airframe through the residual ticks (M4: dropping the raw_in reset
// ships green while a raw mid-frame crash flies up to 3 residual ticks on the
// dead life's full stick). Same differential + absolute shape, raw advance.
TEST_CASE("step_frame: a mid-frame crash neutralizes the RAW stick too") {
    const app::LoopState held =
        post_crash_frame(crash_in_raw(1.0));  // full pull
    const app::LoopState off = post_crash_frame(crash_in_raw(0.0));
    const sim::SimState exp = expected_post_crash(true);
    std::printf("[AT-9 neutralize-raw] omega_err(held vs off)=%.3e\n",
                glm::length(held.curr.angular_vel - off.curr.angular_vel));
    CHECK(
        exact_eq(held.curr.orientation, off.curr.orientation));  // differential
    CHECK(exact_eq(held.curr.angular_vel, off.curr.angular_vel));
    CHECK(exact_eq(held.curr.orientation, exp.orientation));  // absolute
    CHECK(exact_eq(held.curr.angular_vel, exp.angular_vel));
    CHECK(exact_eq(held.curr.velocity, exp.velocity));
    CHECK(held.curr.throttle == exp.throttle);
}

// ===========================================================================
// step_frame forwards EVERY resolved field to app::tick and REPORTS the
// consumed mouse (Fable AT-9 impl consult P1-1). The extraction rerouted
// throttle, freelook, override, raw_mode/raw_in, the per-frame mouse, and
// (later) the RMB-zoom aim_gain_scale THROUGH a new seam (FrameInput -> the
// tick fan-out
// -> FrameResult); a dropped field
// is a live-app feature silently dead while the gate stays green — Fable ran M1
// (consumed:=0, orbit dies), M3 (freelook:=false, freelook dies) and a throttle
// drop, all 126/126. This is the round-2/round-3 "moved consumer left
// uncovered" trap one layer up (round 3 pinned app::tick -> control::step; this
// pins main.cpp -> step_frame). Mirror step_frame against the pre-extraction
// composition — direct app::tick with hand-built TickInputs, the old inline
// main.cpp loop body — over a mixed schedule (instructor + raw, freelook,
// override, varied throttle, a 0-tick carry frame). Bit-equal LoopState pins
// the dynamics-bearing fields; consumed_dx/dy == the offered pending pins the
// orbit report (cosmetic, so invisible to a state mirror — the M1 hole).
// ===========================================================================
namespace {
struct Frame {
    double frame_dt;
    bool raw_mode;
    double throttle;
    bool freelook;
    double osign[3];   // per-axis override sign; 0 = not held on that axis
    double raw_pitch;  // raw-mode stick (pitch), exercised in the raw frame
    double mdx, mdy;
    double gain;  // RMB-zoom aim_gain_scale (1.0 = unzoomed; <1 on a zoomed
                  // mouse frame — drop its forward and the aim quat diverges)
    double flap;  // MB-flaps command (5th/6th defaulted forward fields — the
    double gear;  //   round-4 trap; rows without them zero-init = clean)
};

// The pre-extraction composition: the frame loop via direct app::tick, every
// TickInput field set explicitly — so a step_frame that drops a field diverges.
app::LoopState ref_compose(const std::vector<Frame>& sched,
                           const sim::SimState& s0, std::vector<double>& cdx,
                           std::vector<double>& cdy) {
    app::LoopState loop = flying(s0);
    app::Accumulator accum(kAp.sim_dt);
    double pdx = 0.0, pdy = 0.0;
    for (const Frame& fm : sched) {
        pdx += fm.mdx;
        pdy += fm.mdy;
        const int ticks = accum.advance(fm.frame_dt);
        bool consumed = false;
        double fdx = 0.0, fdy = 0.0;
        // mutable per-frame input, neutralized on a mid-frame respawn
        bool rm = fm.raw_mode, fl = fm.freelook;
        double thr = fm.throttle,
               os[3] = {fm.osign[0], fm.osign[1], fm.osign[2]};
        sim::Inputs rin;
        rin.throttle = static_cast<float>(fm.throttle);
        rin.pitch = static_cast<float>(fm.raw_pitch);
        // S-aimff: the hand-built smear — the consuming tick declares the
        // frame's tick count, its computed rate forwards to the rest (the two
        // NEW forwarded fields; a step_frame that drops either diverges).
        glm::dvec3 fwd_rate{0.0};
        for (int t = 0; t < ticks; ++t) {
            app::TickInput in;
            in.raw_mode = rm;
            in.raw_in = rin;
            in.throttle = thr;
            in.flap_cmd = fm.flap;  // MB-flaps forward (mirrored below)
            in.gear_cmd = fm.gear;
            in.freelook_held = fl;
            in.aim_gain_scale = fm.gain;  // RMB-zoom mouse-gain forward
            for (int i = 0; i < 3; ++i) {
                in.override_mask[i] = os[i] != 0.0;
                in.override_sign[i] = os[i];
            }
            in.aim_rate_ff = fwd_rate;  // 0 until the consuming tick computes
            const bool consuming = !consumed;
            if (consuming) {
                in.aim_dx = pdx;
                in.aim_dy = pdy;
                in.frame_ticks = ticks;  // S-aimff smear denominator
                fdx = pdx;
                fdy = pdy;
                pdx = pdy = 0.0;
                consumed = true;
            }
            const app::TickResult r = app::tick(loop, in, kAp, kCp);
            if (consuming) fwd_rate = r.aim_rate_ff;
            if (r.respawned) {
                rin = sim::Inputs{};
                rin.throttle = static_cast<float>(loop.curr.throttle);
                thr = loop.curr.throttle;
                fl = false;
                os[0] = os[1] = os[2] = 0.0;
                pdx = pdy = 0.0;
                fwd_rate = glm::dvec3{0.0};
            }
        }
        cdx.push_back(fdx);
        cdy.push_back(fdy);
    }
    return loop;
}

app::LoopState sf_compose(const std::vector<Frame>& sched,
                          const sim::SimState& s0, std::vector<double>& cdx,
                          std::vector<double>& cdy) {
    app::LoopState loop = flying(s0);
    app::Accumulator accum(kAp.sim_dt);
    double pdx = 0.0, pdy = 0.0;
    for (const Frame& fm : sched) {
        app::FrameInput fin;
        fin.raw_mode = fm.raw_mode;
        fin.raw_in.throttle = static_cast<float>(fm.throttle);
        fin.raw_in.pitch = static_cast<float>(fm.raw_pitch);
        fin.throttle = fm.throttle;
        fin.flap_cmd = fm.flap;  // MB-flaps forward (a drop diverges the
        fin.gear_cmd = fm.gear;  //   trajectory once the slewed flap bites)
        fin.freelook_held = fm.freelook;
        fin.aim_gain_scale = fm.gain;  // RMB-zoom mouse-gain forward
        for (int i = 0; i < 3; ++i) {
            fin.override_mask[i] = fm.osign[i] != 0.0;
            fin.override_sign[i] = fm.osign[i];
        }
        pdx += fm.mdx;
        pdy += fm.mdy;
        const app::FrameResult fr =
            app::step_frame(loop, accum, fm.frame_dt, fin, pdx, pdy, kAp, kCp);
        // This schedule never crashes (high alt): the respawned flag must be
        // FALSE every frame. Nothing else pins the false case, yet main.cpp
        // keys its persistent-device reset on it — a stuck-true flag re-zeroes
        // the raw virtual stick and freelook state every frame (Fable AT-9
        // verify: a `respawned = true` default survives the whole gate
        // otherwise).
        CHECK(!fr.respawned);
        cdx.push_back(fr.consumed_dx);
        cdy.push_back(fr.consumed_dy);
    }
    return loop;
}

bool loopstate_eq(const app::LoopState& a, const app::LoopState& b) {
    return exact_eq(a.curr.position, b.curr.position) &&
           exact_eq(a.curr.velocity, b.curr.velocity) &&
           exact_eq(a.curr.orientation, b.curr.orientation) &&
           exact_eq(a.curr.angular_vel, b.curr.angular_vel) &&
           a.curr.throttle == b.curr.throttle && a.curr.flap == b.curr.flap &&
           a.curr.gear == b.curr.gear &&
           exact_eq(a.internal.integ, b.internal.integ) &&
           exact_eq(a.aim.q, b.aim.q) && exact_eq(a.prev_up, b.prev_up) &&
           a.grounded == b.grounded &&
           a.fl.override_used == b.fl.override_used &&
           a.fl.freelook_prev == b.fl.freelook_prev &&
           a.fl.easeback == b.fl.easeback;
}
}  // namespace

TEST_CASE("step_frame forwards every resolved field to app::tick") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    double thr = 0.7;
    const sim::SimState s0 =
        harness::level_trim_state(kAp, 150.0, 4000.0, up, heading, &thr);
    const double D = kAp.sim_dt;
    // Mixed frames (no crash — high alt): 1-tick w/ mouse; a 0-tick carry pair;
    // a 4-tick freelook-held frame w/ mouse (aim suspended, orbit gets it);
    // pitch/yaw/roll override frames (ALL three axes — M7: only axis 2 was
    // exercised before); a raw-mode frame WITH a pitch stick (raw_in
    // forwarding); a 2-tick instructor frame.
    // Last field = RMB-zoom gain. Frame 0 is ZOOMED (0.5) — a mouse-bearing
    // instructor frame BEFORE any freelook, so its mouse is actually applied to
    // the aim (mouse_aim_live, not grounded). A dropped step_frame->tick
    // forward uses the default 1.0 instead => the aim quat diverges from the
    // hand-built ref there (Fable red-team P1-1; mutation-verified: `= 1.0` ->
    // loopstate_eq 1->0). The post-freelook mouse frames CAN'T carry the pin —
    // the CQ2 easeback (0.30 s ~ 36 ticks at sim_dt=1/120) suspends mouse->aim
    // for the whole rest of this 13-tick schedule — so frame 0 is the
    // load-bearing one.
    const std::vector<Frame> sched = {
        {D, false, 0.8, false, {0, 0, 0}, 0, 40.0, 20.0, 0.5},  // ZOOMED
                                                                // (bites)
        {0.5 * D, false, 0.8, false, {0, 0, 0}, 0, 15.0, -10.0, 1.0},  // 0-tick
                                                                       // carry
        {0.5 * D, false, 0.8, false, {0, 0, 0}, 0, 0.0, 0.0, 1.0},  // consumes
                                                                    // it
        // S-aimff: a MULTI-TICK live-mouse instructor frame — the smear's
        // load-bearing pin. frame_ticks = 2 halves the consuming tick's rate
        // and the computed rate forwards to tick 2; a step_frame that drops
        // either new field diverges from the hand-built ref HERE (the earlier
        // mouse frames are all 1-tick, where frame_ticks defaults right and
        // there is no tick 2 to forward to; the later ones are freelook/
        // easeback-suppressed).
        {2.0 * D, false, 0.8, false, {0, 0, 0}, 0, 25.0, -12.0, 1.0},
        {4.0 * D, false, 0.6, true, {0, 0, 0}, 0, 60.0, 30.0, 1.0},  // freelook
        {D, false, 0.6, false, {+1, 0, 0}, 0, 0.0, 0.0, 1.0},  // pitch override
        {D, false, 0.6, false, {0, -1, 0}, 0, 0.0, 0.0, 1.0},  // yaw override
        {D, false, 0.6, false, {0, 0, +1}, 0, 0.0, 0.0, 1.0},  // roll override
        {D, true, 0.9, false, {0, 0, 0}, 0.5, 0.0, 0.0, 1.0},  // raw + stick
        {2.0 * D, false, 0.7, false, {0, 0, 0}, 0, -30.0, 5.0, 1.0},
        {D, false, 0.7, false, {0, 0, 0}, 0, 0.0, 0.0, 1.0},
        // MB-flaps forwarding (the 5th/6th defaulted fields): commanded down
        // for 3 ticks — the slewed state.flap/gear bite the forces, so a
        // dropped FrameInput->TickInput forward diverges the trajectory AND
        // the flap/gear fields of loopstate_eq.
        {2.0 * D, false, 0.7, false, {0, 0, 0}, 0, 0.0, 0.0, 1.0, 1.0, 1.0},
        {D, false, 0.7, false, {0, 0, 0}, 0, 0.0, 0.0, 1.0, 0.5, 1.0},
    };

    std::vector<double> ref_cdx, ref_cdy, sf_cdx, sf_cdy;
    const app::LoopState ref = ref_compose(sched, s0, ref_cdx, ref_cdy);
    const app::LoopState sf = sf_compose(sched, s0, sf_cdx, sf_cdy);

    std::printf("[AT-9 forward] loopstate_eq=%d\n", loopstate_eq(sf, ref));
    // Dynamics-bearing forwarding (throttle/freelook/override/raw_mode/raw_in/
    // mouse): a dropped field diverges the flight from the hand-built
    // reference.
    CHECK(loopstate_eq(sf, ref));

    // The consumed-mouse REPORT (cosmetic orbit feed): invisible to the state
    // mirror, so assert it explicitly per frame. M1 (report:=0) dies here.
    REQUIRE(sf_cdx.size() == ref_cdx.size());
    bool consumed_match = true;
    for (size_t i = 0; i < sf_cdx.size(); ++i)
        if (sf_cdx[i] != ref_cdx[i] || sf_cdy[i] != ref_cdy[i])
            consumed_match = false;
    CHECK(consumed_match);
    // And it is non-trivially exercised: the frame-0 offer (40,20) was
    // reported.
    CHECK(sf_cdx[0] == 40.0);
    CHECK(sf_cdy[0] == 20.0);
    CHECK(sf_cdx[1] == 0.0);  // the 0-tick frame reported nothing (carried)
    CHECK(sf_cdx[2] ==
          15.0);  // the carry landed on the next tick-bearing frame
}
