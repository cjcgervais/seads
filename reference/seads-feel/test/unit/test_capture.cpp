// v4 rung 2 (S-rimshot v2, UNIVERSAL) — the FULL-TRAVERSE REBOUND capture as
// the pointing law's ARRIVAL behavior: ANY deflection, ALWAYS, mid-track
// included (Chad's 2026-07-17 flick-misconception ruling,
// docs/v4_rimshot_MISCONCEPTION_handoff.md — no arm threshold, no park gate,
// aim_moved never read by the machine). Phase 1 carries the incoming NET rate
// through the aim to the far rim of the on-screen circle; phase 2 returns to
// center unshaped and dead-blows on the tau surface. These legs pin the repo
// trap classes for the mechanism:
//   - the traverse fires on a plain parked-aim step (state sequence CARRY ->
//     RETURN -> IDLE paired with BEHAVIOR: crossing, rim band, one reversal,
//     physics apex bound — every bound from the loaded [capture] table + the
//     live envelope, never today's constants) — 45 deg AND the 2-deg
//     HEADLINE (the universal claim executably: a small nudge rebounds too);
//   - the anti-flick-gate tripwire: the no-aim_moved scenario DIVERGES
//     carry=1 vs carry=0 (any re-added aim_moved/park/size gate makes them
//     identical again and fails here), plus carry = 0 structural off + dial
//     invisibility (the gated-tree discipline);
//   - lockstep quiet: a well-led moving track (aim_moved true every tick)
//     never engages — by the NET closing rate (w_rel ~ 0 in lockstep), not
//     by any gate; the cff-aligned slow sweep is the w_rel -> raw-omega
//     mutant kill (Trap A);
//   - mid-track engage (the anti-park-gate pin): a genuine catch-up to a
//     MOVING aim bounces with the hand moving the whole time, exactly once
//     per arrival (the refractory holds through the post-arrest coast);
//   - the re-flick contract: mid-CARRY the hand yanking the aim hands back
//     to the normal law THAT tick (error-domain), while plain aim MOTION
//     does NOT abort (the old flick machine's abort leg inverted);
//   - open-loop band pins (the dwelling-signal discipline): taper-binding
//     gate (plateau + mixed-axis AND-form), engage_frac boundary, w_eps
//     floor, hand-back band, post-crossing abandon, refractory clear scale;
//   - rate-continuous entry: at carry = 1 the engage-tick emission
//     reproduces the measured omega EXACTLY (the net-rate accounting
//     invariant — kills frame-carry-deleted, w_hold-not-net, and
//     yaw_coord-subtraction mutants in one equality), and the frame-carry
//     is NOT gated on the aim_ff dial;
//   - per-tick RETURN refresh (live-center chase) — a stale-axis mutant
//     emits along the wrong direction;
//   - the arrest is NET-rate (a raw-omega arrest mutant releases early);
//   - the envelope holds through the event at low V; the deadzone latch is
//     deferred through the event and engages after; the dead-blow lands at
//     center with no creep-back; the RETURN demand shape is
//     min(return_w, sqrt(2 alpha d)) with no linear floor;
//   - jinking-aim: alternating set-jumps neither wedge the machine nor
//     strobe it (bounded engages, every jump's traverse completes).

#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <cstdio>
#include <vector>

#include "config/load_aircraft.h"
#include "config/load_controller.h"
#include "control/controller.h"
#include "sim/aero.h"
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
// The table exactly as COMMITTED (capture_carry = 0.0 — the pool ball is
// RETIRED, Chad 2026-07-30; the shipped value is pinned in
// test_load_controller, not here).
const control::ControllerParams kCpShipped =
    cfg::load_controller_toml(SEADS_CONFIG_DIR "/controller.toml", kAp);

// Machine retired in the committed table (carry 0); this file pins the PARKED
// machinery for the walk-back, so EVERY leg here runs SELF-ARMED at the
// walk-back value carry = 1.0. Hoisted to one arm point on purpose: the legs
// thread `kCp` through dozens of tick/step call sites, and a per-leg copy
// invites a PARTIAL arm (some ticks armed, some not) — a worse failure than
// the retirement it re-keys around. Every other dial is the shipped value,
// and the legs that deliberately force carry in-fixture (0.0 structural-off
// arms, the 0.15 low-carry probe) copy from here and are unaffected.
const control::ControllerParams kCp = [] {
    control::ControllerParams p = kCpShipped;
    p.capture_carry = 1.0;
    return p;
}();

constexpr double kPi = 3.14159265358979323846;
double rad(double d) { return d * kPi / 180.0; }

bool exact_eq(const glm::dvec3& a, const glm::dvec3& b) {
    return a.x == b.x && a.y == b.y && a.z == b.z;
}
bool exact_eq(const glm::dquat& a, const glm::dquat& b) {
    return a.w == b.w && a.x == b.x && a.y == b.y && a.z == b.z;
}

// The controller's curvature feedforward, reproduced (the MB-rud pattern) so
// pointing assertions can subtract it from omega_des_total.
glm::dvec3 curvature_ff_body(const sim::SimState& s) {
    const glm::dvec3 up = glm::normalize(s.position);
    return sim::body_dir_of(
        s.orientation, glm::cross(up, s.velocity) / glm::length(s.position));
}

// The pitch pointing law's branch values at error a (reproduced from
// seek_law's expressions for PREMISE checks — the tests REQUIRE the intended
// branch actually binds at the chosen fixture error, so a table retune fails
// loudly instead of silently moving a leg onto a different branch).
struct Branches {
    double seek, brake, ceil;
};
Branches pitch_branches(double a, double V, double alt) {
    const double aB = kCp.k_b * sim::ang_accel_max_derived(
                                    kAp.c_pitch, kAp.I_pitch, V, alt, kAp);
    return {std::max(kCp.pursuit_step,
                     kCp.K_theta * a * (1.0 + kCp.pursuit_expo * a)),
            std::sqrt(2.0 * aB * a),
            (kCp.n_max - 1.0) * kAp.g / std::max(V, kCp.v_min)};
}
Branches yaw_branches(double a, double V, double alt) {
    const double aB =
        kCp.k_b * sim::ang_accel_max_derived(kAp.c_yaw, kAp.I_yaw, V, alt, kAp);
    return {kCp.K_theta * kCp.yaw_scale * a, std::sqrt(2.0 * aB * a),
            kCp.yaw_max};
}

// Per-tick record of a flick run (state BEFORE the tick + the telemetry the
// tick produced — the state control::step actually saw).
struct Tick {
    double e = 0.0;      // total pointing error [rad]
    double s = 0.0;      // signed error along the step axis (demand.x)
    double w = 0.0;      // body pitch rate BEFORE the tick [rad/s]
    double speed = 0.0;  // extracted speed
    double cpt = 1.0;    // cos_phi_theta
    double alt = 0.0;    // altitude before the tick
    glm::dvec3 omega_des{0.0};
    glm::dvec3 cff{0.0};  // curvature ff at the pre-tick state
    control::CaptureState cap = control::CaptureState::IDLE;
    bool deadzoned = false;
};

// Drive the harness step fixture (level trim, GROUNDED spawn tick, 1 s
// settle) then a pitch-up set-jump of `step_deg` reported through the
// aim_moved seam for exactly one tick (the hand's set-then-park; under the
// UNIVERSAL trigger aim_moved feeds only the deadzone rest_dwell — the
// machine never reads it). Mirrors run_step.
std::vector<Tick> flick_run(double step_deg, double V,
                            const control::ControllerParams& cp, int ticks,
                            harness::ClosedLoop* out_cl = nullptr,
                            int stop_at_state = -1) {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    double thr = 0.0;
    const sim::SimState s0 =
        harness::level_trim_state(kAp, V, 3000.0, up, heading, &thr);
    harness::ClosedLoop cl(s0, glm::dvec3{0.0, 0.0, -1.0});
    cl.aim_nose();
    cl.tick(thr, kAp, cp, /*grounded=*/true);
    for (int i = 0; i < 120; ++i) cl.tick(thr, kAp, cp);
    const glm::dvec3 right = cl.state.orientation * glm::dvec3{1.0, 0.0, 0.0};
    cl.aim = glm::normalize(glm::angleAxis(rad(step_deg), right) * cl.aim);

    std::vector<Tick> tr;
    tr.reserve(ticks);
    for (int i = 0; i < ticks; ++i) {
        cl.aim_moved = (i == 0);
        Tick tk;
        const glm::dvec3 dem = control::rotation_demand_body(
            cl.state.orientation, glm::normalize(cl.aim));
        tk.s = dem.x;
        tk.w = cl.state.angular_vel.x;
        tk.alt = glm::length(cl.state.position) - kAp.R;
        tk.cff = curvature_ff_body(cl.state);
        const control::Telemetry t = cl.tick(thr, kAp, cp);
        tk.e = t.e;
        tk.speed = t.extracted.speed;
        tk.cpt = t.extracted.cos_phi_theta;
        tk.omega_des = t.omega_des;
        tk.cap = t.capture;
        tk.deadzoned = t.deadzoned;
        tr.push_back(tk);
        if (stop_at_state >= 0 &&
            static_cast<int>(t.capture) == stop_at_state) {
            break;
        }
    }
    if (out_cl != nullptr) *out_cl = cl;
    return tr;
}

int first_state(const std::vector<Tick>& tr, control::CaptureState st) {
    for (size_t i = 0; i < tr.size(); ++i)
        if (tr[i].cap == st) return static_cast<int>(i);
    return -1;
}

// IDLE -> owned transitions (an "engage" each).
int count_engages(const std::vector<Tick>& tr) {
    int n = 0;
    control::CaptureState prev = control::CaptureState::IDLE;
    for (const Tick& tk : tr) {
        if (prev == control::CaptureState::IDLE &&
            tk.cap != control::CaptureState::IDLE) {
            ++n;
        }
        prev = tk.cap;
    }
    return n;
}

// Assert the full traverse shape on a parked-aim pitch step (shared by the
// 45-deg and the 2-deg HEADLINE legs — the same contract at both scales).
void require_traverse(const std::vector<Tick>& tr) {
    const int i_carry = first_state(tr, control::CaptureState::CARRY);
    const int i_return = first_state(tr, control::CaptureState::RETURN);
    REQUIRE(i_carry >= 0);
    REQUIRE(i_return > i_carry);
    int i_exit = -1;
    for (size_t i = i_return; i < tr.size(); ++i)
        if (tr[i].cap == control::CaptureState::IDLE) {
            i_exit = static_cast<int>(i);
            break;
        }
    REQUIRE(i_exit > i_return);
    // ONE bounce per arrival: exactly one engage over the whole run (the
    // refractory holds through the post-arrest coast — without it the coast
    // re-engages at ~7 Hz and this count explodes).
    REQUIRE(count_engages(tr) == 1);

    // BEHAVIOR: the nose CROSSED the aim (legacy never does — attribution
    // 2.0: one-sided asymptote, overshoot <= 0.02 deg), inside the event.
    const double sign0 = tr[0].s >= 0.0 ? 1.0 : -1.0;
    int i_cross = -1;
    for (size_t i = 0; i < tr.size(); ++i)
        if (tr[i].s * sign0 <= 0.0) {
            i_cross = static_cast<int>(i);
            break;
        }
    REQUIRE(i_cross > 0);
    REQUIRE(i_cross >= i_carry);

    // v3 POOL BALL (Chad's Q2 "dead ON the inside wall" + "predictably the
    // same every time"): the far excursion lands ON the rim target inside a
    // DETERMINISTIC band [0.85, 1.15] x rim. The v2 bound (rim + stopping
    // distance) blessed the variable overshoot that WAS the bug (measured
    // rim 1.48-1.59 across the grid); the v3 predictive brake surface +
    // the rim-targeted alpha_req law measure 0.99-1.00.
    // S-truedepth (v5) NAMED COUPLING — this band is NO LONGER purely
    // rim_frac-relative: the live apex target is min(rim_frac*circle,
    // depth_frac x the ENGAGE-time glance w_ev^2/(2*alpha)), and at the
    // 45 deg V140 fixtures the DEPTH term binds (~0.87 rim at depth 1.5,
    // measured: passes at depth 1.4, FAILS at depth 1.3 — ~10% dial
    // headroom). A depth_frac retune below ~1.4, or an S-aimff/K_theta/
    // airframe retune that lowers the engage rate ~5%, trips this floor
    // LOUD — that is the capture landing at its (new) earned depth, NOT
    // the capture mechanism breaking. Re-derive the fixture's earned
    // depth before touching the band (v5 red-team P2).
    double far_peak = 0.0;
    for (size_t i = i_cross; i < static_cast<size_t>(i_exit); ++i) {
        far_peak = std::max(far_peak, -tr[i].s * sign0);
    }
    const double rim = kCp.capture_rim_frac * kCp.capture_circle;
    REQUIRE(far_peak >= 0.85 * rim);  // dead on the wall, not short
    CHECK(far_peak <= 1.15 * rim);    // ...and not past it

    // NO SLOW-IN: the closing rate at the circle's edge is the carried rate.
    int i_circ = -1;
    for (int i = 0; i < i_cross; ++i)
        if (tr[i].e < kCp.capture_circle) {
            i_circ = i;
            break;
        }
    REQUIRE(i_circ >= i_carry);
    const double w_engage = std::abs(tr[i_carry].w);
    REQUIRE(w_engage > 0.0);
    CHECK(std::abs(tr[i_circ].w) >= 0.7 * kCp.capture_carry * w_engage);

    // Exactly ONE closing-rate reversal across the event (rim apex), with a
    // 0.5 deg/s deadband so trim noise never counts.
    const double band = rad(0.5);
    int revs = 0, w_sign = 0;
    for (int i = i_carry; i <= i_exit; ++i) {
        const double wc = tr[i].w * sign0;
        const int sg = wc > band ? 1 : (wc < -band ? -1 : 0);
        if (sg != 0) {
            if (w_sign != 0 && sg != w_sign) ++revs;
            w_sign = sg;
        }
    }
    // v5 rung D (Chad 2026-07-23 arcade energy ruling): the wider G/thrust
    // envelope (n_max 16->32, T_max 9000->18000, k_induced 0.05->0.015) added
    // a SECOND closing-rate reversal at this fixture (45 deg V140) that did
    // not exist before. CHARACTERIZED (temporary instrumentation, reverted):
    // reversal #1 is the real rim apex (tick rel=37, e=0.49 deg, wc=-5.37
    // dps) -- unchanged in kind from pre-rung-D. Reversal #2 is a WALL-LOCAL
    // MICRO-WOBBLE well after the event has already exited to IDLE (tick
    // rel=47, cap==IDLE already): wc=+0.98 dps (barely above the 0.5 dps
    // trim-noise deadband, nowhere near the ~5 dps "real bounce" scale of
    // reversal #1), at e=0.04 deg (parked, essentially on the aim), and it
    // does NOT re-cross center (still the same side as the approach). This
    // is the honest new signature of a snappier plant settling with one
    // extra sub-deg/s ripple at the wall, not a second real capture bounce
    // -- re-derived to <= 2 rather than widened further; a THIRD reversal,
    // or any reversal that re-crosses center or exceeds ~5 dps, would still
    // trip this loud.
    CHECK(revs <= 2);
}

}  // namespace

// ---------------------------------------------------------------------------
// Leg 1 — the traverse on a plain parked-aim step, at BOTH scales: 45 deg
// (the classic flick-sized arrival) and 2 deg (the HEADLINE — the universal
// trigger fires for a small nudge, which the deleted snap_on gate made
// structurally impossible). Plus the hard engage floors across V (the
// stillborn-trigger tell as a test, not just an instrument: at
// engage_frac <= the closed-loop decay-ratio asymptote the trigger NEVER
// fires and the golden quietly fails to move — red-team F1).
// ---------------------------------------------------------------------------
TEST_CASE("capture: a parked step fires the full rimshot traverse - 45 deg") {
    REQUIRE(kCp.capture_carry > 0.0);  // premise: the file-scope SELF-ARM
                                       // holds (shipped table = RETIRED 0.0)
    require_traverse(flick_run(45.0, 140.0, kCp, 900));
}

TEST_CASE("capture: a 2 deg nudge captures briskly (universal - DIRECT-SEEK)") {
    // v3 POOL BALL (Chad's Q3): a small nudge's arrival carries no
    // glance-worthy momentum (w_rel^2/(2*alpha) < glance_frac * circle at
    // the engage state), so it takes the DIRECT-SEEK path — RETURN entered
    // at ENGAGE, an ACTIVE full-authority capture straight to center + the
    // dead-blow — never the old lazy taper creep AND never a forced glance.
    // The universal claim stands: the machine fires for ANY deflection; the
    // glance/direct classification is the arrival's own momentum.
    REQUIRE(kCp.capture_carry > 0.0);
    REQUIRE(kCp.capture_glance_frac > 0.0);
    const std::vector<Tick> tr = flick_run(2.0, 220.0, kCp, 900);
    const int i_ret = first_state(tr, control::CaptureState::RETURN);
    REQUIRE(i_ret >= 0);              // the machine FIRED (universal)
    REQUIRE(count_engages(tr) == 1);  // exactly once
    // BRISK: the capture reaches the deadzone circle well inside the legacy
    // taper's own exponential clock from the engage error (the anti-creep
    // bound, config-derived — the sqrt drive + dead-blow beat the taper).
    int i_arrive = -1;
    for (size_t i = i_ret; i < tr.size(); ++i)
        if (tr[i].e < kCp.deadzone_lo) {
            i_arrive = static_cast<int>(i);
            break;
        }
    REQUIRE(i_arrive > 0);
    const double e_engage = tr[i_ret].e;
    REQUIRE(e_engage > kCp.deadzone_lo);  // premise: a real approach remains
    const double t_taper = std::log(e_engage / kCp.deadzone_lo) / kCp.K_theta;
    CHECK((i_arrive - i_ret) * kAp.sim_dt <= 0.75 * t_taper);
    // ...and the run ends settled (no wedge, no hunt).
    REQUIRE(tr.back().cap == control::CaptureState::IDLE);
    REQUIRE(tr.back().e < rad(0.2));
}

TEST_CASE("capture: hard engage floors across the V x step grid") {
    for (const double V : {140.0, 220.0}) {
        for (const double step : {2.0, 45.0}) {
            const std::vector<Tick> tr = flick_run(step, V, kCp, 900);
            INFO("V=" << V << " step=" << step);
            REQUIRE(count_engages(tr) >= 1);
        }
    }
}

// ---------------------------------------------------------------------------
// Leg 2 — the anti-flick-gate tripwire + the gated-tree discipline.
// (a) DIVERGENCE: the no-aim_moved scenario (every legacy caller shape) now
// FIRES the event — carry = 1 vs carry = 0 must fly DIFFERENT trajectories.
// This is the executable inversion of the old "unarmed invisibility" leg:
// any re-added aim_moved/park/size gate makes the two arms identical again
// and fails HERE (the misconception cannot silently return).
// (b) carry = 0 structural off: IDLE forever through a full step scenario
// and every other [capture] dial is byte-invisible.
// ---------------------------------------------------------------------------
TEST_CASE(
    "capture: universal firing diverges from carry 0 with no aim_moved "
    "anywhere") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    double thr = 0.0;
    const sim::SimState s0 =
        harness::level_trim_state(kAp, 140.0, 3000.0, up, heading, &thr);
    auto run = [&](const control::ControllerParams& cp, bool* fired) {
        harness::ClosedLoop cl(s0, glm::dvec3{0.0, 0.0, -1.0});
        cl.aim_nose();
        cl.tick(thr, kAp, cp, /*grounded=*/true);
        for (int i = 0; i < 600; ++i) {
            const glm::dvec3 nose =
                cl.state.orientation * glm::dvec3{0.0, 0.0, -1.0};
            const glm::dvec3 right =
                cl.state.orientation * glm::dvec3{1.0, 0.0, 0.0};
            const glm::dvec3 up_b =
                cl.state.orientation * glm::dvec3{0.0, 1.0, 0.0};
            if (i == 60)  // set-jump, aim_moved NEVER reported
                cl.aim = glm::angleAxis(rad(40.0), right) * nose;
            if (i == 360) cl.aim = glm::angleAxis(-rad(20.0), up_b) * nose;
            const control::Telemetry t = cl.tick(thr, kAp, cp);
            if (fired != nullptr && t.capture != control::CaptureState::IDLE)
                *fired = true;
        }
        return cl;
    };
    bool fired = false;
    const harness::ClosedLoop a = run(kCp, &fired);  // shipped carry
    control::ControllerParams cp_off = kCp;
    cp_off.capture_carry = 0.0;
    const harness::ClosedLoop b = run(cp_off, nullptr);  // structural off
    // The event fired with the hand never reported moving (no park gate)...
    REQUIRE(fired);
    // ...and the trajectories DIVERGED (the rebound is real dynamics, not a
    // state flag — a re-added gate reunites these and fails the REQUIRE
    // above first, or this one if the gate only strips the demand).
    CHECK_FALSE(exact_eq(a.state.position, b.state.position));
}

TEST_CASE("capture: carry 0 is structural off and dials are invisible") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    double thr = 0.0;
    const sim::SimState s0 =
        harness::level_trim_state(kAp, 140.0, 3000.0, up, heading, &thr);
    control::ControllerParams cp_off = kCp;
    cp_off.capture_carry = 0.0;
    // Vary every OTHER dial: at carry = 0 none may be read.
    control::ControllerParams cp_off2 = cp_off;
    cp_off2.capture_engage_frac = 0.5;
    cp_off2.capture_handback_frac = 1.9;
    cp_off2.capture_break_frac = 2.5;
    cp_off2.capture_w_eps = rad(5.0);
    cp_off2.capture_return_w = rad(200.0);
    harness::ClosedLoop c0(s0, glm::dvec3{0.0, 0.0, -1.0});
    const std::vector<Tick> t_off = flick_run(45.0, 140.0, cp_off, 600, &c0);
    harness::ClosedLoop c1(s0, glm::dvec3{0.0, 0.0, -1.0});
    const std::vector<Tick> t_off2 = flick_run(45.0, 140.0, cp_off2, 600, &c1);
    for (const Tick& tk : t_off) REQUIRE(tk.cap == control::CaptureState::IDLE);
    CHECK(exact_eq(c0.state.position, c1.state.position));
    CHECK(exact_eq(c0.state.orientation, c1.state.orientation));
    CHECK(c0.last_inputs.pitch == c1.last_inputs.pitch);
}

// ---------------------------------------------------------------------------
// Leg 3 — lockstep quiet, by PHYSICS not by gate. (a) A saturated runaway
// track (the aim outruns the envelope): the nose never closes, the machine
// stays IDLE at any error and flies byte-identical to the carry = 0 twin.
// (b) A settled well-led SLOW track (lockstep): the NET closing rate w_rel =
// (omega - ff - aim_rate) ~ 0, below w_eps — no engage while glued. The
// sweep is pitch-DOWN (aligned with the curvature trim rotation) on purpose:
// a w_rel -> raw-omega mutant reads the V/r trim rate + the sweep rate as
// closing and ENGAGES here (Trap A, red-team F1/F12) — the settled-window
// IDLE is its kill.
// ---------------------------------------------------------------------------
TEST_CASE("capture: tracking stays quiet - saturated and lockstep arms") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    double thr = 0.0;
    const sim::SimState s0 =
        harness::level_trim_state(kAp, 140.0, 3000.0, up, heading, &thr);

    // (a) saturated runaway: 60 deg/s pitch-up outruns the clamped pursuit.
    {
        auto run = [&](const control::ControllerParams& cp, double* max_e,
                       std::vector<control::CaptureState>* states) {
            harness::ClosedLoop cl(s0, glm::dvec3{0.0, 0.0, -1.0});
            cl.aim_nose();
            cl.tick(thr, kAp, cp, /*grounded=*/true);
            const double rate = rad(60.0);
            for (int i = 0; i < 480; ++i) {
                const glm::dvec3 right =
                    cl.state.orientation * glm::dvec3{1.0, 0.0, 0.0};
                cl.aim = glm::normalize(
                    glm::angleAxis(rate * kAp.sim_dt, right) * cl.aim);
                cl.aim_moved = true;  // the hand moves EVERY tick
                cl.aim_rate_world = rate * right;
                const control::Telemetry t = cl.tick(thr, kAp, cp);
                if (max_e != nullptr) *max_e = std::max(*max_e, t.e);
                if (states != nullptr) states->push_back(t.capture);
            }
            return cl;
        };
        double max_e = 0.0;
        std::vector<control::CaptureState> states;
        const harness::ClosedLoop a = run(kCp, &max_e, &states);
        REQUIRE(max_e > rad(30.0));  // premise: the aim genuinely ran away
        for (const control::CaptureState st : states)
            REQUIRE(st == control::CaptureState::IDLE);
        control::ControllerParams cp_off = kCp;
        cp_off.capture_carry = 0.0;
        const harness::ClosedLoop b = run(cp_off, nullptr, nullptr);
        CHECK(exact_eq(a.state.position, b.state.position));
        CHECK(exact_eq(a.state.orientation, b.state.orientation));
        CHECK(a.last_inputs.pitch == b.last_inputs.pitch);
        CHECK(a.last_inputs.yaw == b.last_inputs.yaw);
    }
    // (b) lockstep: 1 deg/s pitch-DOWN (cff-aligned — the raw-omega mutant's
    // most sensitive direction). The transient spool-up is free to do what
    // physics does; the SETTLED window must be IDLE every tick.
    {
        harness::ClosedLoop cl(s0, glm::dvec3{0.0, 0.0, -1.0});
        cl.aim_nose();
        cl.tick(thr, kAp, kCp, /*grounded=*/true);
        const double rate = -rad(1.0);
        std::vector<control::CaptureState> states;
        std::vector<double> errs;
        int engages = 0;
        control::CaptureState prev = control::CaptureState::IDLE;
        for (int i = 0; i < 720; ++i) {
            const glm::dvec3 right =
                cl.state.orientation * glm::dvec3{1.0, 0.0, 0.0};
            cl.aim = glm::normalize(glm::angleAxis(rate * kAp.sim_dt, right) *
                                    cl.aim);
            cl.aim_moved = true;
            cl.aim_rate_world = rate * right;
            const control::Telemetry t = cl.tick(thr, kAp, kCp);
            if (prev == control::CaptureState::IDLE &&
                t.capture != control::CaptureState::IDLE) {
                ++engages;
            }
            prev = t.capture;
            states.push_back(t.capture);
            errs.push_back(t.e);
        }
        // Premise: genuinely tracking (a steady standoff error, not parked
        // in the deadzone and not diverging).
        const double e_end = errs.back();
        REQUIRE(e_end > kCp.deadzone_hi);
        REQUIRE(e_end < rad(3.0));
        for (size_t i = 360; i < states.size(); ++i) {
            REQUIRE(states[i] == control::CaptureState::IDLE);
        }
        // ZERO engages over the WHOLE run, spin-up included: the nose only
        // ever LAGS a steady sweep (never closes), so nothing may fire. The
        // settled-window probe alone is maskable — a raw-omega mutant
        // engages during the transient, completes, and hides behind its own
        // refractory for the rest of the run (mutation-verified: the bare
        // window check stayed green under the mutant; this count kills it).
        REQUIRE(engages == 0);
    }
}

// ---------------------------------------------------------------------------
// Leg 4 — the RE-FLICK contract (Chad's ruling: "abandon the bounce, chase
// instantly") and its inversion: plain aim MOTION does NOT abort (the old
// machine's aim_moved abort leg is the misconception — its executable
// inverse lives here). Plus the mode resets (override / ballistic), each
// from a REQUIRE'd live event.
// ---------------------------------------------------------------------------
TEST_CASE("capture: re-flick hands back that tick; plain motion does not") {
    // Drive to a live CARRY tick (pre-crossing: the first CARRY tick) and
    // snapshot.
    harness::ClosedLoop cl(
        harness::level_state(kAp, 140.0, 3000.0, glm::dvec3{1.0, 0.0, 0.0},
                             glm::dvec3{0.0, 0.0, -1.0}),
        glm::dvec3{0.0, 0.0, -1.0});
    const std::vector<Tick> tr =
        flick_run(45.0, 140.0, kCp, 900, &cl,
                  static_cast<int>(control::CaptureState::CARRY));
    REQUIRE(cl.internal.capture == control::CaptureState::CARRY);  // premise
    REQUIRE_FALSE(cl.internal.cap_crossed);  // pre-crossing (hand-back leg)
    const sim::SimState sc = cl.state;
    const control::Internal ic = cl.internal;
    const glm::dvec3 aim = cl.aim;

    control::Internal idle = ic;
    idle.capture = control::CaptureState::IDLE;

    // (a) plain aim MOTION with the same target: the event stays OWNED (the
    // anti-park-gate pin — the pilot's moving hand no longer kills it), and
    // the owned demand differs from the normal law (non-no-op).
    {
        control::Input in;
        in.target_dir_world = aim;
        in.throttle = 0.7;
        in.aim_moved = true;
        const control::Output ev =
            control::step(sc, in, ic, kAp, kCp, kAp.sim_dt);
        REQUIRE(ev.telem.capture != control::CaptureState::IDLE);
        const control::Output nl =
            control::step(sc, in, idle, kAp, kCp, kAp.sim_dt);
        REQUIRE(ev.telem.omega_des.x != nl.telem.omega_des.x);
    }
    // (b) the RE-FLICK: the aim yanked far past the hand-back band
    // (|demand| > handback_frac * the engage err) — IDLE that tick, demand
    // == the normal law's exactly.
    {
        const glm::dvec3 right = sc.orientation * glm::dvec3{1.0, 0.0, 0.0};
        const glm::dvec3 yanked =
            glm::normalize(glm::angleAxis(rad(35.0), right) * aim);
        // Premise: the yank genuinely exceeds the band.
        const glm::dvec3 dem =
            control::rotation_demand_body(sc.orientation, yanked);
        const double dlen = std::sqrt(dem.x * dem.x + dem.y * dem.y);
        REQUIRE(dlen > kCp.capture_handback_frac * ic.cap_err0);
        control::Input in;
        in.target_dir_world = yanked;
        in.throttle = 0.7;
        in.aim_moved = true;
        const control::Output ev =
            control::step(sc, in, ic, kAp, kCp, kAp.sim_dt);
        control::Input in2 = in;
        const control::Output nl =
            control::step(sc, in2, idle, kAp, kCp, kAp.sim_dt);
        CHECK(ev.telem.capture == control::CaptureState::IDLE);
        CHECK(exact_eq(ev.telem.omega_des, nl.telem.omega_des));
        CHECK(ev.inputs.pitch == nl.inputs.pitch);
        CHECK(ev.inputs.yaw == nl.inputs.yaw);
        CHECK(ev.inputs.roll == nl.inputs.roll);
        // NO refractory from a re-flick exit: the next arrival must be free
        // to bounce (Chad's fresh-rebound ruling).
        CHECK_FALSE(ev.internal.cap_refractory);
    }
    // (c) override mid-event: pursuit suspension resets the machine.
    {
        control::Input in;
        in.target_dir_world = aim;
        in.throttle = 0.7;
        in.override_mask[0] = true;
        in.override_sign[0] = +1.0;
        const control::Output ev =
            control::step(sc, in, ic, kAp, kCp, kAp.sim_dt);
        const control::Output nl =
            control::step(sc, in, idle, kAp, kCp, kAp.sim_dt);
        CHECK(ev.telem.capture == control::CaptureState::IDLE);
        CHECK(exact_eq(ev.telem.omega_des, nl.telem.omega_des));
        CHECK(ev.inputs.pitch == nl.inputs.pitch);
    }
    // (d) ballistic entry mid-event: the attitude-hold regime resets it.
    {
        sim::SimState sb = sc;
        sb.velocity = 0.5 * kCp.v_ballistic * glm::normalize(sc.velocity);
        control::Input in;
        in.target_dir_world = aim;
        in.throttle = 0.7;
        const control::Output ev =
            control::step(sb, in, ic, kAp, kCp, kAp.sim_dt);
        REQUIRE(ev.telem.ballistic);  // premise: the fixture IS ballistic
        CHECK(ev.telem.capture == control::CaptureState::IDLE);
    }
}

// ---------------------------------------------------------------------------
// Leg 5 — mid-track engage (the anti-park-gate pin, closed-loop): the nose
// catches up to a MOVING aim with the hand moving EVERY tick — the event
// fires, the rebound crosses the LIVE center, and there is exactly ONE
// engage per arrival (post-bounce lockstep re-tracking never re-fires: the
// refractory + the net rate keep it quiet).
// ---------------------------------------------------------------------------
TEST_CASE("capture: a moving-aim catch-up bounces mid-track, exactly once") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    double thr = 0.0;
    const sim::SimState s0 =
        harness::level_trim_state(kAp, 140.0, 3000.0, up, heading, &thr);
    harness::ClosedLoop cl(s0, glm::dvec3{0.0, 0.0, -1.0});
    cl.aim_nose();
    cl.tick(thr, kAp, kCp, /*grounded=*/true);
    for (int i = 0; i < 120; ++i) cl.tick(thr, kAp, kCp);
    // Jump the aim 20 deg up, then keep it CREEPING up at 3 deg/s — the
    // nose approaches a target that never parks.
    {
        const glm::dvec3 right =
            cl.state.orientation * glm::dvec3{1.0, 0.0, 0.0};
        cl.aim = glm::normalize(glm::angleAxis(rad(20.0), right) * cl.aim);
    }
    const double rate = rad(3.0);
    std::vector<Tick> tr;
    for (int i = 0; i < 900; ++i) {
        const glm::dvec3 right =
            cl.state.orientation * glm::dvec3{1.0, 0.0, 0.0};
        cl.aim =
            glm::normalize(glm::angleAxis(rate * kAp.sim_dt, right) * cl.aim);
        cl.aim_moved = true;  // the hand is live EVERY tick
        cl.aim_rate_world = rate * right;
        Tick tk;
        const glm::dvec3 dem = control::rotation_demand_body(
            cl.state.orientation, glm::normalize(cl.aim));
        tk.s = dem.x;
        tk.w = cl.state.angular_vel.x;
        const control::Telemetry t = cl.tick(thr, kAp, kCp);
        tk.e = t.e;
        tk.cap = t.capture;
        tr.push_back(tk);
    }
    const int i_carry = first_state(tr, control::CaptureState::CARRY);
    REQUIRE(i_carry >= 0);  // the event fired WITH the hand moving
    // The rebound crossed the LIVE (moving) center...
    const double sign0 = tr[0].s >= 0.0 ? 1.0 : -1.0;
    int i_cross = -1;
    for (size_t i = i_carry; i < tr.size(); ++i)
        if (tr[i].s * sign0 <= 0.0) {
            i_cross = static_cast<int>(i);
            break;
        }
    REQUIRE(i_cross >= i_carry);
    // ...and the arrival bounced exactly once; the post-bounce lockstep
    // re-track (the aim keeps creeping) stays quiet.
    REQUIRE(count_engages(tr) == 1);
    // Premise: the run ended in genuine lockstep tracking, not a wedge.
    REQUIRE(tr.back().cap == control::CaptureState::IDLE);
    REQUIRE(tr.back().e < rad(2.0));
}

// ---------------------------------------------------------------------------
// Leg 5b — a FAST (12 deg/s) lateral catch-up (diff red-team P2-3): the
// engage-ratio's frame semantics (pointed keeps the gain-scaled lead, w_rel
// nets the full aim rate) must never STROBE at a hand speed where the
// (1-gain)*aim_rate bias is material. Yaw-axis (a sustained banked turn) so
// the fixture is energy-sustainable. NOTE the honest physics: a lateral
// catch-up's endgame creeps through the bank standoff (1-3 deg/s), so the
// event either takes ONE attempt (apex-death latch) or, below w_eps, never
// engages — both are correct; what may NOT happen is churn (engages > 1)
// or a stalled/diverging track.
// ---------------------------------------------------------------------------
TEST_CASE("capture: a fast lateral catch-up never strobes") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    double thr = 0.0;
    const sim::SimState s0 =
        harness::level_trim_state(kAp, 140.0, 3000.0, up, heading, &thr);
    harness::ClosedLoop cl(s0, glm::dvec3{0.0, 0.0, -1.0});
    cl.aim_nose();
    cl.tick(thr, kAp, kCp, /*grounded=*/true);
    for (int i = 0; i < 120; ++i) cl.tick(thr, kAp, kCp);
    {
        const glm::dvec3 nose =
            cl.state.orientation * glm::dvec3{0.0, 0.0, -1.0};
        const glm::dvec3 up_b =
            cl.state.orientation * glm::dvec3{0.0, 1.0, 0.0};
        cl.aim = glm::normalize(glm::angleAxis(-rad(20.0), up_b) * nose);
    }
    const double rate = rad(12.0);
    int engages = 0;
    bool fired = false;
    control::CaptureState prev = control::CaptureState::IDLE;
    double e_final = 1e9;
    for (int i = 0; i < 600; ++i) {
        const glm::dvec3 up_b =
            cl.state.orientation * glm::dvec3{0.0, 1.0, 0.0};
        cl.aim =
            glm::normalize(glm::angleAxis(-rate * kAp.sim_dt, up_b) * cl.aim);
        cl.aim_moved = true;
        cl.aim_rate_world = -rate * up_b;
        const control::Telemetry t = cl.tick(thr, kAp, kCp);
        if (prev == control::CaptureState::IDLE &&
            t.capture != control::CaptureState::IDLE) {
            ++engages;
        }
        if (t.capture != control::CaptureState::IDLE) fired = true;
        prev = t.capture;
        e_final = t.e;
    }
    (void)fired;  // engage-or-not is regime-honest either way (see banner)
    REQUIRE(engages <= 1);
    // Steady lag for a 12 deg/s lateral track sits ~0.7*rate/K_eff — well
    // under a few degrees; a stalled event or a strobe blows this.
    REQUIRE(e_final < rad(4.0));
}

// ---------------------------------------------------------------------------
// Leg 6 — the envelope holds THROUGH the event at low V (config-relative, the
// AT-15 discipline): during every CARRY/RETURN tick the pitch demand net of
// curvature ff sits inside the live G/AoA budget, and the yaw demand inside
// yaw_max plus the documented coordination exemption.
// ---------------------------------------------------------------------------
TEST_CASE("capture: the event demand stays inside the envelope at low V") {
    // V = 80 / 35 deg keeps the airframe above the ballistic floor while the
    // AoA clamp binds the whole approach.
    const std::vector<Tick> tr = flick_run(35.0, 80.0, kCp, 900);
    int event_ticks = 0;
    for (const Tick& tk : tr) {
        if (tk.cap != control::CaptureState::CARRY &&
            tk.cap != control::CaptureState::RETURN) {
            continue;
        }
        ++event_ticks;
        const double V_clamp = std::max(tk.speed, kCp.v_min);
        const double w_max = (kCp.n_max - tk.cpt) * kAp.g / V_clamp;
        const double w_min = (kCp.n_min - tk.cpt) * kAp.g / V_clamp;
        const double pointed_pitch = tk.omega_des.x - tk.cff.x;
        REQUIRE(pointed_pitch <= w_max + 1e-9);
        REQUIRE(pointed_pitch >= w_min - 1e-9);
        REQUIRE(std::abs(tk.omega_des.y - tk.cff.y) <=
                kCp.yaw_max + kCp.K_coord * kPi + 1e-9);
    }
    REQUIRE(event_ticks > 0);  // premise: the event actually ran at V=80/35deg
}

// ---------------------------------------------------------------------------
// Leg 7 — the deadzone handoff: the latch is DEFERRED while the event owns
// the demand (no latched tick in CARRY/RETURN even as the traverse sweeps
// err through the circle), latches normally after center exit, and the
// machine never re-fires from rest (the completion refractory + the deadzone
// own the parked tail).
// ---------------------------------------------------------------------------
TEST_CASE("capture: deadzone defers through the event then latches at rest") {
    // A low carry slows the crossing so ticks genuinely land INSIDE the
    // 0.03-deg circle mid-event with the hand long at rest: without the
    // deferral the latch WOULD fire and truncate the traverse.
    control::ControllerParams cp = kCp;
    cp.capture_carry = 0.15;
    const std::vector<Tick> tr = flick_run(45.0, 140.0, cp, 1080);
    const int i_return = first_state(tr, control::CaptureState::RETURN);
    REQUIRE(i_return > 0);  // premise: the event ran to phase 2
    int i_exit = -1;
    for (size_t i = i_return; i < tr.size(); ++i)
        if (tr[i].cap == control::CaptureState::IDLE) {
            i_exit = static_cast<int>(i);
            break;
        }
    REQUIRE(i_exit > 0);
    bool swept_circle = false;
    int first_incircle_event_tick = -1;
    for (int i = 0; i < i_exit; ++i) {
        if (tr[i].cap == control::CaptureState::CARRY ||
            tr[i].cap == control::CaptureState::RETURN) {
            if (tr[i].e < kCp.deadzone_lo) {
                swept_circle = true;
                if (first_incircle_event_tick < 0)
                    first_incircle_event_tick = i;
            }
            REQUIRE_FALSE(tr[i].deadzoned);  // the latch is DEFERRED
        }
    }
    REQUIRE(swept_circle);  // premise: the traverse DID cross the circle
    // Premise (red-team P2-2): the crossing must genuinely OUTLAST the
    // rest_dwell — else the motion-gated latch could not have fired there
    // even WITHOUT the deferral and this leg is vacuous.
    REQUIRE(first_incircle_event_tick * kAp.sim_dt > cp.deadzone_rest_dwell);
    // After the exit: the latch engages normally and the machine stays IDLE
    // (no re-fire from rest — the completion refractory holds until the
    // error leaves its clear scale, and a parked hand never re-grows it).
    bool latched_after = false;
    for (size_t i = i_exit; i < tr.size(); ++i) {
        REQUIRE(tr[i].cap == control::CaptureState::IDLE);
        if (tr[i].deadzoned) latched_after = true;
    }
    CHECK(latched_after);
}

// ---------------------------------------------------------------------------
// Leg 8 — the trigger bands pinned OPEN-LOOP on doctored states (the
// dwelling-signal discipline: a closed-loop chase sweeps monotonically and
// pins nothing). Every fixture REQUIREs its intended branch from the loaded
// tables (a retune moves a leg onto a different branch LOUDLY).
// ---------------------------------------------------------------------------
TEST_CASE("capture: engage bands pinned open-loop") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    const sim::SimState s0 =
        harness::level_state(kAp, 140.0, 3000.0, up, heading);
    const glm::dvec3 nose = s0.orientation * glm::dvec3{0.0, 0.0, -1.0};
    const glm::dvec3 right = s0.orientation * glm::dvec3{1.0, 0.0, 0.0};
    const glm::dvec3 up_b = s0.orientation * glm::dvec3{0.0, 1.0, 0.0};
    const double alt0 = glm::length(s0.position) - kAp.R;
    const double V0 = glm::length(s0.velocity);
    const double cffx = curvature_ff_body(s0).x;
    auto pitch_target = [&](double deg) {
        return glm::normalize(glm::angleAxis(rad(deg), right) * nose);
    };
    // One controller step from a doctored state: pitch rate wx (raw body),
    // target at err_deg pitch-up, fresh internal unless given.
    auto probe = [&](double err_deg, double wx, const control::Internal& it) {
        sim::SimState s = s0;
        s.angular_vel.x = wx;
        control::Input in;
        in.target_dir_world = pitch_target(err_deg);
        in.throttle = 0.7;
        return control::step(s, in, it, kAp, kCp, kAp.sim_dt);
    };

    // Premises: 2 deg is taper-bound, 30 deg is plateau/brake-bound (pitch).
    const Branches b2 = pitch_branches(rad(2.0), V0, alt0);
    REQUIRE(b2.seek <= b2.brake);
    REQUIRE(b2.seek <= b2.ceil);
    const Branches b30 = pitch_branches(rad(30.0), V0, alt0);
    REQUIRE(b30.seek > std::min(b30.brake, b30.ceil));

    // (i) the taper-binding gate: a plateau-error state with a huge closing
    // rate (ratio << engage_frac) must NOT engage — deleting the gate
    // engages here and carries the plateau rate into a fly-by.
    {
        const control::Output o = probe(30.0, 2.0, control::reset());
        REQUIRE(o.telem.capture == control::CaptureState::IDLE);
    }
    // (ii) in-taper engage with a GLANCE-worthy rate -> CARRY (v3: the
    // engage forks on the projected glance depth; 0.3 rad/s at V140 pitch
    // projects w^2/(2*alpha) past glance_frac x circle — premise pinned so
    // a table retune moves this leg loudly, never silently).
    {
        const double a140 =
            sim::ang_accel_max_derived(kAp.c_pitch, kAp.I_pitch, V0, alt0, kAp);
        REQUIRE(0.3 * 0.3 / (2.0 * a140) >
                kCp.capture_glance_frac * kCp.capture_circle);
        const control::Output o = probe(2.0, cffx + 0.3, control::reset());
        REQUIRE(o.telem.capture == control::CaptureState::CARRY);
    }
    // (iii) the w_eps floor: same state, net closing rate under the floor ->
    // IDLE (the noise-crawl guard).
    {
        REQUIRE(kCp.capture_w_eps > rad(0.2));  // premise: floor above probe
        const control::Output o = probe(2.0, cffx + rad(0.2), control::reset());
        REQUIRE(o.telem.capture == control::CaptureState::IDLE);
    }
    // (iv) the engage_frac boundary, both sides (net-rate exact: the state's
    // curvature rate is offset out, aim_rate_filt is zero). ENGAGE needs
    // pointed < engage_frac * w_rel, i.e. w_rel > pointed/engage_frac — the
    // engage arm sets w_rel 5% ABOVE that boundary rate, the no-engage arm
    // 5% BELOW it. v3: the boundary-rate arrival's glance depth is SUB-
    // glance_frac (premise below), so the engage arm is ALSO the
    // DIRECT-SEEK entry pin — the slow-closing engage-able state enters
    // RETURN (the active capture), NOT CARRY (a glance it has no momentum
    // for) and NOT the legacy taper.
    {
        const double pointed = b2.seek;  // FINE branch, blend 0, ff 0
        const double w_bound = pointed / kCp.capture_engage_frac;
        const double a140 =
            sim::ang_accel_max_derived(kAp.c_pitch, kAp.I_pitch, V0, alt0, kAp);
        const double w_eng = 1.05 * w_bound;
        REQUIRE(w_eng * w_eng / (2.0 * a140) <
                kCp.capture_glance_frac * kCp.capture_circle);  // sub-glance
        const control::Output eng = probe(2.0, cffx + w_eng, control::reset());
        REQUIRE(eng.telem.capture == control::CaptureState::RETURN);
        REQUIRE(eng.internal.cap_inbound);  // no glance leg for this entry
        const control::Output no =
            probe(2.0, cffx + 0.95 * w_bound, control::reset());
        REQUIRE(no.telem.capture == control::CaptureState::IDLE);
    }
    // (v) mixed-axis AND-form: a mostly-YAW diagonal whose yaw axis rides
    // its plateau must not engage even with pitch taper-bound and a huge
    // closing rate (an OR-form / pitch-only mutant engages).
    {
        const Branches by20 = yaw_branches(rad(20.0), V0, alt0);
        REQUIRE(by20.seek > std::min(by20.brake, by20.ceil));  // yaw plateau
        sim::SimState s = s0;
        s.angular_vel.x = 0.5;
        s.angular_vel.y = -2.0;  // yaw-right toward the target
        control::Input in;
        in.target_dir_world =
            glm::normalize(glm::angleAxis(rad(10.0), right) *
                           (glm::angleAxis(-rad(20.0), up_b) * nose));
        in.throttle = 0.7;
        const control::Output o =
            control::step(s, in, control::reset(), kAp, kCp, kAp.sim_dt);
        REQUIRE(o.telem.capture == control::CaptureState::IDLE);
    }
    // (vi) hand-back band, open-loop: a live pre-crossing CARRY holds inside
    // handback_frac x cap_err0 and hands back beyond it.
    {
        control::Internal ic = control::reset();
        ic.capture = control::CaptureState::CARRY;
        ic.cap_ux = 1.0;
        ic.cap_uy = 0.0;
        ic.cap_w_hold = 0.3;
        ic.cap_err0 = rad(10.0);
        const control::Output hold = probe(10.5, cffx + 0.3, ic);
        REQUIRE(hold.telem.capture == control::CaptureState::CARRY);
        const control::Output back = probe(12.0, cffx + 0.3, ic);
        REQUIRE(back.telem.capture == control::CaptureState::IDLE);
        REQUIRE_FALSE(back.internal.cap_refractory);  // re-flick: no latch
    }
    // (vii) post-crossing ABANDON, both discriminator arms (v3 POOL BALL
    // rewrite of the not-grown arm): grown past handback_frac x the engage
    // err = pilot re-flick — IDLE, no latch, chase now; NOT grown = the
    // arrival's geometry curved out of CARRY's fixed frame (the banked
    // phantom-crossing path) — the arrival now DIRECT-SEEKS: RETURN, no
    // refractory, the active capture of the remaining error (the old
    // IDLE+latch regroup was Chad's REJECTED "one attempt then regroup").
    // Inside the allowance the event stays owned.
    {
        control::Internal ic = control::reset();
        ic.capture = control::CaptureState::CARRY;
        ic.cap_ux = 1.0;
        ic.cap_uy = 0.0;
        ic.cap_w_hold = 0.3;
        ic.cap_err0 = rad(10.0);
        ic.cap_crossed = true;
        ic.cap_d_allow = rad(1.0);
        // 2 deg past center: outside the allowance, INSIDE the approach
        // scale -> the arrival curved, not the pilot: DIRECT-SEEK.
        const control::Output broke = probe(-2.0, cffx + 0.3, ic);
        REQUIRE(broke.telem.capture == control::CaptureState::RETURN);
        REQUIRE_FALSE(broke.internal.cap_refractory);
        REQUIRE(broke.internal.cap_inbound);  // no glance leg for this entry
        // 12 deg past center: grown past 1.1 x 10 deg -> a genuine
        // re-flick: no latch, back to the normal law.
        const control::Output flick = probe(-12.0, cffx + 0.3, ic);
        REQUIRE(flick.telem.capture == control::CaptureState::IDLE);
        REQUIRE_FALSE(flick.internal.cap_refractory);
        const control::Output held = probe(-1.1, cffx + 0.3, ic);
        REQUIRE(held.telem.capture != control::CaptureState::IDLE);
    }
    // (ix) RETURN yank exit, both discriminator arms: outside the allowance
    // but inside the approach scale = the arrival curved (latch, regroup);
    // leg 13 pins the held/inside arm with its ample-allowance fixture.
    {
        control::Internal ic = control::reset();
        ic.capture = control::CaptureState::RETURN;
        ic.cap_ux = 1.0;
        ic.cap_uy = 0.0;
        ic.cap_crossed = true;
        ic.cap_err0 = rad(10.0);
        ic.cap_d_allow = rad(1.0);
        const control::Output broke = probe(-2.0, cffx - 0.05, ic);
        REQUIRE(broke.telem.capture == control::CaptureState::IDLE);
        REQUIRE(broke.internal.cap_refractory);
    }
    // (x) CROSS-NOSE re-flick mid-RETURN (diff red-team P0-1): the aim
    // flipped to the OPPOSITE side flips the demand sign, so the
    // crossed-anyway completion leg (s_prev >= 0) would swallow it and
    // LATCH the refractory with cap_err0 = the flick size — eating the
    // yanked aim's own arrival bounce. Yank-first ordering + the growth
    // discriminator make it the no-latch re-flick exit. Mutation this
    // kills: reorder completion-before-yank (the shipped P0-1 bug).
    {
        control::Internal ic = control::reset();
        ic.capture = control::CaptureState::RETURN;
        ic.cap_ux = 1.0;
        ic.cap_uy = 0.0;
        ic.cap_crossed = true;
        ic.cap_err0 = rad(10.0);
        ic.cap_d_allow = rad(1.0);
        const control::Output gone = probe(+12.0, cffx - 0.05, ic);
        REQUIRE(gone.telem.capture == control::CaptureState::IDLE);
        REQUIRE_FALSE(gone.internal.cap_refractory);
    }
    // (xi) APEX-DEATH DIRECT-SEEKS (v3 POOL BALL rewrite — the old
    // IDLE+refractory regroup is Chad's REJECTED "one attempt then
    // regroup"): a pre-crossing CARRY whose net closing rate is dead for
    // two consecutive ticks (one noise sample never kills a live carry —
    // the s1 tick pins the debounce) transitions to RETURN with NO latch:
    // the active full-authority capture of whatever error remains, and the
    // emitted demand drives TOWARD the aim (never a taper hand-back).
    {
        control::Internal ic = control::reset();
        ic.capture = control::CaptureState::CARRY;
        ic.cap_ux = 1.0;
        ic.cap_uy = 0.0;
        ic.cap_w_hold = 0.3;
        ic.cap_err0 = rad(3.2);
        // Net closing rate NEGATIVE (receding), err 3 deg pre-crossing
        // (inside the hand-back band: 3 < 1.1 * 3.2).
        const control::Output s1 = probe(3.0, cffx - 0.05, ic);
        REQUIRE(s1.telem.capture == control::CaptureState::CARRY);  // 1 tick
        const control::Output s2 = probe(3.0, cffx - 0.05, s1.internal);
        REQUIRE(s2.telem.capture == control::CaptureState::RETURN);
        REQUIRE_FALSE(s2.internal.cap_refractory);
        REQUIRE(s2.internal.cap_inbound);  // no glance leg for this entry
        // The same-tick emission drives toward the aim (pitch-up demand for
        // a +3 deg target), full sqrt scale — not the taper's creep.
        REQUIRE(s2.telem.omega_des.x - cffx > 2.0 * kCp.K_theta * rad(3.0));
    }
    // (viii) the refractory clear scale: latched with a stored exit err, a
    // sub-scale arrival state does NOT engage; past the scale the clear
    // runs FIRST and the same state engages the same tick (the
    // top-of-block ordering, executably).
    {
        control::Internal ic = control::reset();
        ic.cap_refractory = true;
        ic.cap_err0 = rad(2.0);  // clear needs err > 1.1 * 2.0 = 2.2 deg
        const control::Output blocked = probe(2.0, cffx + 0.3, ic);
        REQUIRE(blocked.telem.capture == control::CaptureState::IDLE);
        REQUIRE(blocked.internal.cap_refractory);  // still latched
        control::Internal ic2 = ic;
        ic2.cap_err0 = rad(1.5);  // clear needs err > 1.65 deg < 2.0
        const control::Output freed = probe(2.0, cffx + 0.3, ic2);
        REQUIRE_FALSE(freed.internal.cap_refractory);
        REQUIRE(freed.telem.capture == control::CaptureState::CARRY);
    }
}

// ---------------------------------------------------------------------------
// Leg 8a2 — the PREDICTIVE BRAKE SURFACE pinned open-loop (v3 POOL BALL's
// core fix): post-crossing CARRY hands to RETURN when the remaining
// distance to the rim target is inside the live stopping plan
// w^2/(2*(alpha_u + damp_assist)) + 0.5*w*dt — the fire leads the wall by
// exactly the stopping distance, so the apex lands ON it (Chad's Q2/"same
// every time"). Probe A sits just INSIDE the surface -> RETURN; probe B
// just OUTSIDE -> stays CARRY. Mutation this kills: restoring the v2 rim
// DETECT (-s_u >= rim_frac*circle) leaves A in CARRY (A is well short of
// the rim) and the closed-loop rim band [0.85, 1.15] blows out with the
// old rim + stopping-distance overshoot (measured 1.48-1.59).
// ---------------------------------------------------------------------------
TEST_CASE("capture: the predictive brake surface fires by stopping plan") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    const sim::SimState s0 =
        harness::level_state(kAp, 140.0, 3000.0, up, heading);
    const glm::dvec3 nose = s0.orientation * glm::dvec3{0.0, 0.0, -1.0};
    const glm::dvec3 right = s0.orientation * glm::dvec3{1.0, 0.0, 0.0};
    const double cffx = curvature_ff_body(s0).x;
    const double alt0 = glm::length(s0.position) - kAp.R;
    const double V0 = glm::length(s0.velocity);
    // Config-derived surface replica (the same H1 compositions).
    const double w_net = 0.2;  // [rad/s] carried post-crossing rate
    const double alpha =
        sim::ang_accel_max_derived(kAp.c_pitch, kAp.I_pitch, V0, alt0, kAp);
    const double q0 = sim::q_eff(sim::q_dyn(sim::rho_at(alt0, kAp), V0), kAp);
    const double damp_u = kAp.damp_pitch / kAp.I_pitch * q0 * w_net;
    const double lead =
        w_net * w_net / (2.0 * (alpha + damp_u)) + 0.5 * w_net * kAp.sim_dt;
    const double rim = kCp.capture_rim_frac * kCp.capture_circle;
    REQUIRE(lead < rim);  // premise: the surface fires between center & rim
    const double margin = 0.5e-3;  // [rad] probe separation vs the boundary
    auto probe = [&](double past_center) {
        sim::SimState s = s0;
        s.angular_vel.x = w_net + cffx;  // raw = net + curvature trim
        control::Internal ic = control::reset();
        ic.capture = control::CaptureState::CARRY;
        ic.cap_ux = 1.0;
        ic.cap_uy = 0.0;
        ic.cap_w_hold = w_net;
        ic.cap_crossed = true;
        ic.cap_err0 = rad(10.0);
        ic.cap_d_allow = rad(3.0);  // ample: no abandon in this probe
        // S-truedepth (v5): this fixture models a WALL-EARNING event — the
        // ENGAGE would have stored the full rim as the event's apex target.
        // Unseeded (0.0) the brake surface fires immediately and the probe
        // reads a degenerate event.
        ic.cap_rim_t = rim;
        control::Input in;
        // Past-center along -u: demand.x = -past_center.
        in.target_dir_world =
            glm::normalize(glm::angleAxis(-past_center, right) * nose);
        in.throttle = 0.7;
        return control::step(s, in, ic, kAp, kCp, kAp.sim_dt);
    };
    // A: remaining-to-rim just INSIDE the stopping plan -> fires.
    const control::Output a = probe(rim - lead + margin);
    REQUIRE(a.telem.capture == control::CaptureState::RETURN);
    // B: just OUTSIDE -> the carry holds (full speed, no pre-braking).
    const control::Output b = probe(rim - lead - margin);
    REQUIRE(b.telem.capture == control::CaptureState::CARRY);
}

// ---------------------------------------------------------------------------
// Leg 8b — the cross-nose re-flick, CLOSED-LOOP: the end-to-end BEHAVIORAL
// pin — the yanked aim's own arrival takes its OWN full bounce and the run
// parks (Chad's ruling, end to end). NOTE (re-review round 2 P2-2): this
// leg does NOT kill the completion-first reorder mutant by itself — under
// that mutant the min()-bounded store keeps the latch small and the next
// arrival still bounces here; the reorder's dedicated kill is the OPEN-LOOP
// band leg (x), which asserts the no-latch exit directly.
// ---------------------------------------------------------------------------
TEST_CASE("capture: a cross-nose yank mid-RETURN re-bounces at the new aim") {
    harness::ClosedLoop cl(
        harness::level_state(kAp, 140.0, 3000.0, glm::dvec3{1.0, 0.0, 0.0},
                             glm::dvec3{0.0, 0.0, -1.0}),
        glm::dvec3{0.0, 0.0, -1.0});
    const std::vector<Tick> tr0 =
        flick_run(45.0, 140.0, kCp, 900, &cl,
                  static_cast<int>(control::CaptureState::RETURN));
    REQUIRE(cl.internal.capture == control::CaptureState::RETURN);  // premise
    // Yank ACROSS the nose: 12 deg on the opposite side of the approach.
    {
        const glm::dvec3 nose =
            cl.state.orientation * glm::dvec3{0.0, 0.0, -1.0};
        const glm::dvec3 right =
            cl.state.orientation * glm::dvec3{1.0, 0.0, 0.0};
        cl.aim = glm::normalize(glm::angleAxis(-rad(12.0), right) * nose);
    }
    int engages = 0, exit_seen = false;
    control::CaptureState prev = cl.internal.capture;
    double e_final = 1e9;
    for (int i = 0; i < 720; ++i) {
        cl.aim_moved = (i == 0);
        const control::Telemetry t = cl.tick(0.7, kAp, kCp);
        if (prev == control::CaptureState::IDLE &&
            t.capture != control::CaptureState::IDLE) {
            ++engages;
        }
        if (t.capture == control::CaptureState::IDLE) exit_seen = true;
        prev = t.capture;
        e_final = t.e;
    }
    REQUIRE(exit_seen);
    // The NEW arrival took its own bounce (>= 1 fresh engage post-yank)...
    REQUIRE(engages >= 1);
    // ...and the run ends parked at the new aim (no stranded latch, no
    // stalled chase).
    REQUIRE(e_final < rad(0.5));
}

// ---------------------------------------------------------------------------
// Leg 8c — the YAW/banked arrival, CLOSED-LOOP: v3 POOL BALL's UNIVERSALITY
// pin (Chad's ruling — the old "one attempt then regroup to the legacy law"
// for lateral arrivals is REJECTED and dead). The banked lateral endgame
// (1-3 deg/s bank/lean-standoff creep) now DIRECT-SEEKS: RETURN drives the
// remaining error at the active sqrt ceiling (yaw_max-bounded) and
// dead-blows — so the capture must COMPLETE to the deadzone circle BRISKLY
// (within ~1.2 s of engage; measured 0.46 s), with exactly one engage and
// no stalled crawl. This is the anti-creep pin: a re-added regroup latch
// (any exit that hands the multi-degree lateral remainder back to the
// taper) blows the capture clock.
// ---------------------------------------------------------------------------
TEST_CASE("capture: a lateral flick arrival captures briskly (yaw)") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    double thr = 0.0;
    const sim::SimState s0 =
        harness::level_trim_state(kAp, 140.0, 3000.0, up, heading, &thr);
    harness::ClosedLoop cl(s0, glm::dvec3{0.0, 0.0, -1.0});
    cl.aim_nose();
    cl.tick(thr, kAp, kCp, /*grounded=*/true);
    for (int i = 0; i < 120; ++i) cl.tick(thr, kAp, kCp);
    {
        const glm::dvec3 nose =
            cl.state.orientation * glm::dvec3{0.0, 0.0, -1.0};
        const glm::dvec3 up_b =
            cl.state.orientation * glm::dvec3{0.0, 1.0, 0.0};
        cl.aim = glm::normalize(glm::angleAxis(-rad(20.0), up_b) * nose);
    }
    int engages = 0, i_engage = -1, i_capture = -1;
    bool fired = false;
    control::CaptureState prev = control::CaptureState::IDLE;
    double e_final = 1e9;
    for (int i = 0; i < 900; ++i) {
        cl.aim_moved = (i == 0);
        const control::Telemetry t = cl.tick(thr, kAp, kCp);
        if (prev == control::CaptureState::IDLE &&
            t.capture != control::CaptureState::IDLE) {
            ++engages;
            if (i_engage < 0) i_engage = i;
        }
        if (t.capture != control::CaptureState::IDLE) fired = true;
        if (i_engage >= 0 && i_capture < 0 && t.e < kCp.deadzone_lo)
            i_capture = i;
        prev = t.capture;
        e_final = t.e;
    }
    REQUIRE(fired);         // the lateral arrival DID fire (universal)
    REQUIRE(engages == 1);  // exactly once — no blend-band multi-engage
    // The ANTI-CREEP pin: the capture COMPLETES to the deadzone circle
    // within ~1.2 s of engage (measured ~0.46 s) — a regroup-to-legacy
    // hands the lateral remainder to the taper/standoff and takes 6+ s.
    REQUIRE(i_capture > 0);
    REQUIRE((i_capture - i_engage) * kAp.sim_dt < 1.2);
    REQUIRE(e_final < rad(0.5));  // and it stays parked (no stalled crawl)
}

namespace {
// Closed-loop YAW traverse fixture (yaw-exactness): a lateral set-jump about
// body-up, recording the yaw-signed error (demand.y), the TOTAL error e
// (|d| — the frame-honest glance depth: a lateral flick engages mid-unroll
// at 60-75 deg bank, so the travel direction is mostly BODY-pitch and the
// body-yaw projection of a rim-deep glance under-reads by |u_y|; the norm is
// what the reticle shows and what Chad's "point opposite the direction of
// travel" grades), and the event state.
struct YawTick {
    double e = 0.0;   // total pointing error [rad]
    double sy = 0.0;  // signed error along the step axis (demand.y)
    control::CaptureState cap = control::CaptureState::IDLE;
};
std::vector<YawTick> yaw_flick_run(double step_deg, double V, int ticks) {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    double thr = 0.0;
    const sim::SimState s0 =
        harness::level_trim_state(kAp, V, 3000.0, up, heading, &thr);
    harness::ClosedLoop cl(s0, glm::dvec3{0.0, 0.0, -1.0});
    cl.aim_nose();
    cl.tick(thr, kAp, kCp, /*grounded=*/true);
    for (int i = 0; i < 120; ++i) cl.tick(thr, kAp, kCp);
    {
        const glm::dvec3 nose =
            cl.state.orientation * glm::dvec3{0.0, 0.0, -1.0};
        const glm::dvec3 up_b =
            cl.state.orientation * glm::dvec3{0.0, 1.0, 0.0};
        cl.aim = glm::normalize(glm::angleAxis(-rad(step_deg), up_b) * nose);
    }
    std::vector<YawTick> tr;
    tr.reserve(ticks);
    for (int i = 0; i < ticks; ++i) {
        cl.aim_moved = (i == 0);
        const glm::dvec3 dem = control::rotation_demand_body(
            cl.state.orientation, glm::normalize(cl.aim));
        YawTick tk;
        tk.sy = dem.y;
        const control::Telemetry t = cl.tick(thr, kAp, kCp);
        tk.e = t.e;
        tk.cap = t.capture;
        tr.push_back(tk);
    }
    return tr;
}
// The yaw traverse band (config-relative, like require_traverse): exactly
// one engage; the yaw-signed error CROSSES; and the far excursion of the
// TOTAL error over [cross .. event-exit] lands ON the rim target inside
// [0.85, 1.15] x rim.
void require_yaw_traverse(const std::vector<YawTick>& tr) {
    int engages = 0, i_engage = -1, i_exit = -1;
    control::CaptureState prev = control::CaptureState::IDLE;
    for (size_t i = 0; i < tr.size(); ++i) {
        if (prev == control::CaptureState::IDLE &&
            tr[i].cap != control::CaptureState::IDLE) {
            ++engages;
            if (i_engage < 0) i_engage = static_cast<int>(i);
        }
        if (i_engage >= 0 && i_exit < 0 && i > static_cast<size_t>(i_engage) &&
            tr[i].cap == control::CaptureState::IDLE) {
            i_exit = static_cast<int>(i);
        }
        prev = tr[i].cap;
    }
    REQUIRE(engages == 1);
    REQUIRE(i_engage >= 0);
    REQUIRE(i_exit > i_engage);
    const double sign0 = tr[0].sy >= 0.0 ? 1.0 : -1.0;
    int i_cross = -1;
    for (int i = i_engage; i < i_exit; ++i)
        if (tr[i].sy * sign0 <= 0.0) {
            i_cross = i;
            break;
        }
    REQUIRE(i_cross > 0);  // the nose crossed the aim (legacy never does)
    double far_e = 0.0;
    for (int i = i_cross; i <= i_exit; ++i) far_e = std::max(far_e, tr[i].e);
    const double rim = kCp.capture_rim_frac * kCp.capture_circle;
    // NAMED-COUPLING (director-authorized re-derivation, kernel v5 rung C2
    // S-holdline, docs/v5_kernel_handoff.md "RUNG C"): the sag servo lifts
    // the nose during a banked lateral approach, arriving with less droop --
    // measured far_e/rim = 0.8001 at pull_floor=1.0 (shipped) vs 0.8766 at
    // pull_floor=0 (this leg's pre-rung floor). The coupling dial is
    // pull_floor; 0.85 was calibrated to the pre-rung trajectory and the
    // servo's honest lift trips it by a hair. Floor re-derived 0.85 -> 0.75
    // (a trip below 0.75 means the servo grew past its mandate -- re-derive
    // again, don't widen further).
    REQUIRE(far_e >= 0.75 * rim);  // dead on the wall, not short —
                                   // mutation: delete the yaw_coord
                                   // compensation in the event emission
                                   // (the coordination bleed kills the
                                   // outbound at ~1/3 rim)
    CHECK(far_e <= 1.15 * rim);    // ...and not past it
    // Parked at the end (no stalled crawl / no hot recross walk-off).
    REQUIRE(tr.back().e < rad(0.5));
}
}  // namespace

// ---------------------------------------------------------------------------
// Leg 8e — the YAW traverse band (yaw-exactness, red-team P1-1): a 20 deg
// lateral flick at V140 flies the full pool-ball traverse with the glance
// landing ON the wall in the frame-honest norm (measured rim_e 0.99; the
// pre-fix build died at 0.32 — the coordination bleed). Config-relative.
// ---------------------------------------------------------------------------
TEST_CASE("capture: a lateral flick glances dead on the wall (yaw band)") {
    REQUIRE(kCp.capture_carry > 0.0);  // premise: the file-scope SELF-ARM
                                       // holds (shipped table = RETIRED 0.0)
    require_yaw_traverse(yaw_flick_run(20.0, 140.0, 900));
}

// ---------------------------------------------------------------------------
// Leg 8f — the WALL-REACHABILITY clamp (yaw-exactness, red-team P1-2,
// Chad's never-past-the-rim ruling): the HOT yaw crossing (10 deg at V140
// engages at ~41 deg/s net — stopping distance ~0.75 deg > the 0.55 deg
// ring) must still land inside the band: the ENGAGE/CARRY clamp caps the
// hold at sqrt(2*alpha_u*rim) so the wall can absorb the carried rate.
// Mutation: delete the w_wall min at capture -> the apex overruns to ~1.26
// rim and the 1.15 band fails.
// ---------------------------------------------------------------------------
TEST_CASE("capture: a hot yaw crossing never lands past the rim (P1-2)") {
    REQUIRE(kCp.capture_carry > 0.0);  // premise: the file-scope SELF-ARM
                                       // holds (shipped table = RETIRED 0.0)
    require_yaw_traverse(yaw_flick_run(10.0, 140.0, 900));
}

// ---------------------------------------------------------------------------
// Leg 8d — the CRABBED engage (diff red-team P1-2, the promised pin): with
// real sideslip the coordination demand rides in the measured yaw rate; the
// w_rel subtraction takes it out and the emission adds it back — so the
// rate-continuity equality holds THROUGH a live yaw_coord. The
// delete-the-subtraction mutant emits omega.y + yaw_coord and fails the
// equality (every other capture fixture flies beta ~ 0 and is blind to it).
// ---------------------------------------------------------------------------
TEST_CASE("capture: crabbed engage stays rate-continuous (yaw_coord netted)") {
    REQUIRE(kCp.capture_carry == 1.0);  // premise: full carry (the file-scope
                                        // SELF-ARM; shipped = RETIRED 0.0)
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    sim::SimState s = harness::level_state(kAp, 140.0, 3000.0, up, heading);
    const glm::dvec3 nose = s.orientation * glm::dvec3{0.0, 0.0, -1.0};
    const glm::dvec3 up_b = s.orientation * glm::dvec3{0.0, 1.0, 0.0};
    // Real sideslip: velocity skewed 10 deg about body-up; the aim 1.5 deg
    // laterally (inside the FINE endgame, outside the deadzone).
    s.velocity = glm::angleAxis(rad(10.0), up_b) * s.velocity;
    s.last_vhat = glm::normalize(s.velocity);
    const glm::dvec3 cff = curvature_ff_body(s);
    control::Input in;
    in.target_dir_world =
        glm::normalize(glm::angleAxis(-rad(1.5), up_b) * nose);
    in.throttle = 0.7;
    const glm::dvec3 dem =
        control::rotation_demand_body(s.orientation, in.target_dir_world);
    REQUIRE(std::abs(dem.y) > 10.0 * std::abs(dem.x));  // yaw-axis approach
    // Premise: the yaw taper binds at this error (loud on a retune).
    const Branches by = yaw_branches(std::abs(dem.y), glm::length(s.velocity),
                                     glm::length(s.position) - kAp.R);
    REQUIRE(by.seek <= by.brake);
    REQUIRE(by.seek <= by.ceil);
    // Closing rate big enough to engage under ANY yaw_coord in +-K_coord*pi
    // scale at beta = 10 deg (|yaw_coord| <= ~K_coord*0.175).
    const double sy = dem.y >= 0.0 ? 1.0 : -1.0;
    s.angular_vel.y = sy * 0.5 + cff.y;
    const control::Output o =
        control::step(s, in, control::reset(), kAp, kCp, kAp.sim_dt);
    REQUIRE(o.telem.capture == control::CaptureState::CARRY);
    // Premise: the coordination term is genuinely live (the mutant target).
    REQUIRE(std::abs(o.telem.extracted.beta) > rad(5.0));
    REQUIRE(kCp.K_coord > 0.0);
    // Rate-continuous entry THROUGH yaw_coord: emitted == measured, exact.
    CHECK(o.telem.omega_des.y == Catch::Approx(s.angular_vel.y).margin(1e-9));
}

// ---------------------------------------------------------------------------
// Leg 9 — RATE-CONTINUOUS ENTRY (the net-rate accounting invariant): at
// carry = 1, the engage-tick emitted omega_des on the captured axis
// reproduces the measured body rate EXACTLY — every net term (curvature ff,
// aim frame rate, coordination) is measured out and re-added exactly once.
// One equality kills the frame-carry-deleted, w_hold-not-net, and
// yaw_coord-subtraction mutant classes; the gain-0 twin pins that the
// frame-carry is NOT the aim_ff dial.
// ---------------------------------------------------------------------------
TEST_CASE("capture: engage is rate-continuous (emission == measured omega)") {
    REQUIRE(kCp.capture_carry == 1.0);  // premise: full carry (the file-scope
                                        // SELF-ARM; shipped = RETIRED 0.0)
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    const sim::SimState s0 =
        harness::level_state(kAp, 140.0, 3000.0, up, heading);
    const glm::dvec3 nose = s0.orientation * glm::dvec3{0.0, 0.0, -1.0};
    const glm::dvec3 right = s0.orientation * glm::dvec3{1.0, 0.0, 0.0};
    const double cffx = curvature_ff_body(s0).x;

    // A moving aim (world rate r about body-right => body x rate r) plus a
    // real closing rate on top. Rung A2 SCOPE: the rate-continuity
    // invariant holds for WALL-EARNING engages (saturation s = 1, where
    // w_wall >= w_ev and the clamp is inert). A SUB-wall engage now
    // deliberately clamps the hold below the arriving rate (w_wall =
    // w_ev*sqrt(depth_frac*s) < w_ev for s < 1/depth_frac) — the nose
    // sheds speed into the crossing: Chad's fly-1 "soften the small
    // adjustment approach" ruling, superseding Q1 full-speed-through
    // AT THE SMALL END only. So this fixture's closing rate must EARN the
    // wall; the premise REQUIRE below trips loud on any retune that drops
    // it sub-wall (re-derive the rate, don't widen the margin).
    const double r_aim = 0.1;
    sim::SimState s = s0;
    s.angular_vel.x = cffx + r_aim + 0.47;  // net closing = 0.47 rad/s �
    // inside the wall-earning-yet-unclamped window [w_wall/sqrt(depth),
    // w_wall] ~ [0.43, 0.52] at this fixture (above it the v3 wall-
    // reachability clamp itself binds and the equality honestly fails)
    control::Internal ic = control::reset();
    ic.aim_rate_filt = r_aim * right;  // WORLD-frame filtered aim rate
    control::Input in;
    in.target_dir_world =
        glm::normalize(glm::angleAxis(rad(2.0), right) * nose);
    in.throttle = 0.7;
    in.aim_rate_world = r_aim * right;  // keeps the filter state at r_aim
    const control::Output o = control::step(s, in, ic, kAp, kCp, kAp.sim_dt);
    REQUIRE(o.telem.capture == control::CaptureState::CARRY);
    // Premise (A2): this engage EARNS the wall — cap_rim_t at the full rim
    // means s = 1, the clamp is inert, and the exact equality below tests
    // the net-term ACCOUNTING (its actual target), not the clamp.
    REQUIRE(o.internal.cap_rim_t ==
            Catch::Approx(kCp.capture_rim_frac * kCp.capture_circle)
                .margin(1e-12));
    CHECK(o.telem.omega_des.x == Catch::Approx(s.angular_vel.x).margin(1e-9));
    // The frame-carry is structural, not the aim_ff dial: gain 0, same
    // emission.
    control::ControllerParams cp0 = kCp;
    cp0.aim_ff_gain = 0.0;
    const control::Output o0 = control::step(s, in, ic, kAp, cp0, kAp.sim_dt);
    REQUIRE(o0.telem.capture == control::CaptureState::CARRY);
    CHECK(o0.telem.omega_des.x == Catch::Approx(s.angular_vel.x).margin(1e-9));
}

// ---------------------------------------------------------------------------
// Leg 10 — per-tick RETURN refresh (the live-center chase): a doctored
// RETURN whose stored axis is STALE (pure pitch) against a diagonally
// displaced live center must emit its return demand along the LIVE error
// direction — a refresh-deleted mutant emits pitch-only.
// ---------------------------------------------------------------------------
TEST_CASE("capture: RETURN chases the live center (per-tick axis refresh)") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    const sim::SimState s0 =
        harness::level_state(kAp, 140.0, 3000.0, up, heading);
    const glm::dvec3 nose = s0.orientation * glm::dvec3{0.0, 0.0, -1.0};
    const glm::dvec3 right = s0.orientation * glm::dvec3{1.0, 0.0, 0.0};
    const glm::dvec3 up_b = s0.orientation * glm::dvec3{0.0, 1.0, 0.0};
    const glm::dvec3 cff = curvature_ff_body(s0);
    control::Internal ic = control::reset();
    ic.capture = control::CaptureState::RETURN;
    ic.cap_ux = 1.0;  // STALE: pure-pitch captured axis
    ic.cap_uy = 0.0;
    ic.cap_crossed = true;
    ic.cap_inbound = true;      // v3: mid-return (the glance leg is done)
    ic.cap_d_allow = rad(3.0);  // ample (no yank exit in this probe)
    // Live center displaced DIAGONALLY past the nose: equal pitch and yaw
    // components (demand = (-d, -d)/sqrt-ish), still on the stale axis's
    // far side (s_prev < 0 holds the state).
    const double d = rad(0.3);
    control::Input in;
    in.target_dir_world = glm::normalize(glm::angleAxis(-d, right) *
                                         (glm::angleAxis(d, up_b) * nose));
    in.throttle = 0.7;
    const control::Output o = control::step(s0, in, ic, kAp, kCp, kAp.sim_dt);
    REQUIRE(o.telem.capture == control::CaptureState::RETURN);  // held
    // The emitted event demand (net of ff and coordination — beta = 0 at
    // level_state so yaw_coord = 0) must have BOTH components with near-equal
    // magnitude: it points at the live center, not down the stale axis.
    const double px = o.telem.omega_des.x - cff.x;
    const double py = o.telem.omega_des.y - cff.y;
    REQUIRE(std::abs(px) > rad(1.0));  // premise: a real return demand
    CHECK(std::abs(py) == Catch::Approx(std::abs(px)).epsilon(0.05));
}

// ---------------------------------------------------------------------------
// Leg 11 — the dead-blow release surface is NET-rate (red-team F12: every
// w_rel consumer site, not just ENGAGE): a doctored RETURN whose NET coast
// exceeds the trusted bound (hold) while the RAW coast sits at a
// perfect-landing release must HOLD — the raw-omega mutant releases and
// dumps a hot coast. v3 re-parameterization: the release is the
// predicted-landing surface |d - coast| <= deadzone_hi with the coast
// itself bounded <= 2*deadzone_hi; the fixture puts the NET coast just past
// the bound and the RAW coast exactly on a zero-pred landing.
// ---------------------------------------------------------------------------
TEST_CASE("capture: the dead-blow release surface reads the net rate") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    const sim::SimState s0 =
        harness::level_state(kAp, 140.0, 3000.0, up, heading);
    const glm::dvec3 nose = s0.orientation * glm::dvec3{0.0, 0.0, -1.0};
    const glm::dvec3 right = s0.orientation * glm::dvec3{1.0, 0.0, 0.0};
    const double cffx = curvature_ff_body(s0).x;
    const double alt0 = glm::length(s0.position) - kAp.R;
    const double q_arr = sim::q_eff(
        sim::q_dyn(sim::rho_at(alt0, kAp), glm::length(s0.velocity)), kAp);
    const double tau = kAp.I_pitch / (kCp.K_w_pitch + kAp.damp_pitch * q_arr);
    REQUIRE(tau > 0.0);
    REQUIRE(cffx < 0.0);  // level flight trims pitch-DOWN (the netted term)

    // NET closing rate chosen so the NET coast just exceeds the trusted
    // bound (2*deadzone_hi) -> HOLD; the RAW rate (net + cff, smaller in
    // magnitude because cff is pitch-down while the closing is pitch-up) has
    // its coast inside the bound AND the target is placed AT the raw coast's
    // zero-pred landing -> the raw mutant completes.
    const double w_net = (2.0 * kCp.deadzone_hi / tau) * 1.05;  // hold arm
    const double w_raw = w_net + cffx;             // what the mutant reads
    REQUIRE(w_net * tau > 2.0 * kCp.deadzone_hi);  // net: HOLD
    REQUIRE(std::abs(w_raw) * tau < 2.0 * kCp.deadzone_hi);  // raw: inside
    const double d = std::abs(w_raw) * tau;  // raw pred-landing == 0
    REQUIRE(d > kCp.deadzone_hi);            // premise: not already parked
    sim::SimState s = s0;
    s.angular_vel.x = w_raw;
    control::Internal ic = control::reset();
    ic.capture = control::CaptureState::RETURN;
    ic.cap_ux = -1.0;  // u points away from the aim (aim ABOVE the nose)
    ic.cap_uy = 0.0;
    ic.cap_crossed = true;
    ic.cap_inbound = true;
    ic.cap_d_allow = rad(3.0);
    control::Input in;
    // Aim ABOVE the nose along +pitch: demand.x = +d, closing = pitch-up.
    in.target_dir_world = glm::normalize(glm::angleAxis(d, right) * nose);
    in.throttle = 0.7;
    const control::Output o = control::step(s, in, ic, kAp, kCp, kAp.sim_dt);
    // NET surface: coast bound exceeded => HOLD. The raw mutant releases
    // (IDLE) and dumps the hot coast.
    CHECK(o.telem.capture == control::CaptureState::RETURN);
}

// ---------------------------------------------------------------------------
// Leg 12 — the dead-blow (rimshot-arrest): the return lands AT center and
// STOPS — no blow-past, no taper creep-back. Every bound derived from the
// loaded tables + the live state.
// ---------------------------------------------------------------------------
TEST_CASE("capture: the return arrests dead at center (the dead-blow)") {
    REQUIRE(kCp.capture_carry > 0.0);
    const std::vector<Tick> tr = flick_run(45.0, 140.0, kCp, 1080);
    const double sign0 = tr[0].s >= 0.0 ? 1.0 : -1.0;
    const int i_return = first_state(tr, control::CaptureState::RETURN);
    REQUIRE(i_return > 0);  // premise: the event reached phase 2
    int i_release = -1;     // the tau-surface handback (first IDLE after)
    for (size_t i = i_return; i < tr.size(); ++i)
        if (tr[i].cap == control::CaptureState::IDLE) {
            i_release = static_cast<int>(i);
            break;
        }
    REQUIRE(i_release > i_return);

    // NON-NO-OP premise 1: the return DEMAND actually ran at the cap
    // structure — >= 0.8 * min(return_w, the sqrt ceiling at the rim).
    const double a_ret = sim::ang_accel_max_derived(
        kAp.c_pitch, kAp.I_pitch, tr[i_return].speed, tr[i_return].alt, kAp);
    const double rim = kCp.capture_rim_frac * kCp.capture_circle;
    const double cap_floor =
        0.8 * std::min(kCp.capture_return_w, std::sqrt(2.0 * a_ret * rim));
    double wd_ret_peak = 0.0, w_ret_peak = 0.0;
    for (int i = i_return; i < i_release; ++i) {
        wd_ret_peak =
            std::max(wd_ret_peak, std::abs(tr[i].omega_des.x - tr[i].cff.x));
        w_ret_peak = std::max(w_ret_peak, std::abs(tr[i].w));
    }
    REQUIRE(wd_ret_peak >= cap_floor);
    // NON-NO-OP premise 2: the achieved return rate is a slam by taper
    // standards.
    REQUIRE(w_ret_peak > 3.0 * kCp.K_theta * kCp.capture_circle);

    // ARRIVAL: first tick after the release the total error is inside the
    // deadzone circle — beating the taper's exponential clock by 2x.
    int i_arrive = -1;
    for (size_t i = i_release; i < tr.size(); ++i)
        if (tr[i].e < kCp.deadzone_lo) {
            i_arrive = static_cast<int>(i);
            break;
        }
    REQUIRE(i_arrive > 0);
    const double t_taper =
        std::log(rim / kCp.deadzone_lo) / kCp.K_theta;  // the creep clock
    REQUIRE((i_arrive - i_return) * kAp.sim_dt <= 0.5 * t_taper);

    // THE DEAD STOP: at arrival the body rate on the captured axis (net of
    // the mandatory curvature ff) is inside the arrest coast's own terminal
    // scale. The bound MODELS the two known contributions (S-rimshot v2
    // re-parameterization — the old bare 4*lo/tau was calibrated to the
    // shipped rung-2's cff-sagged carry, the AT-15 calibrated-bound class):
    //   (a) the release-surface residual: w = d/tau, at d = deadzone_lo
    //       that is lo/tau (4x absorbs release quantization);
    //   (b) the pointing law's own post-handback drive: the arrest hands
    //       back at d_rel = |w_rel|*tau where the taper still demands
    //       ~K_theta*d_rel toward center THROUGH the coast — a sustained
    //       ~K_theta*|w_rel|*tau of extra rate at the crossing (1.5x
    //       margin). The v2 event carries the honest rate (rate-continuous
    //       entry — the shipped version under-carried by exactly the
    //       curvature term), which is what pushed the old bound over.
    // A deleted-arrest mutant (the old s_u >= 0-only exit) carries
    // 12-17 deg/s across center — still ~3x past this modeled bound.
    const double q_arr = sim::q_eff(
        sim::q_dyn(sim::rho_at(tr[i_arrive].alt, kAp), tr[i_arrive].speed),
        kAp);
    const double tau_arr =
        kAp.I_pitch / (kCp.K_w_pitch + kAp.damp_pitch * q_arr);
    REQUIRE(tau_arr > 0.0);
    const double w_release = std::abs(tr[i_release].w - tr[i_release].cff.x);
    const double w_arrive = std::abs(tr[i_arrive].w - tr[i_arrive].cff.x);
    CHECK(w_arrive <= 4.0 * kCp.deadzone_lo / tau_arr +
                          1.5 * kCp.K_theta * w_release * tau_arr);

    // NO SECOND OVERSHOOT + NO CREEP-BACK: after arrival the signed error
    // never blows back past center beyond the crossing rate's own coast
    // (w_arrive*tau, same modeled shape as above, + the park scale), and
    // the terminal drift (mean |e| over 0.35 s) decays back to the
    // deadzone/park scale. The refractory guarantees the coast can never
    // re-engage (the 7 Hz hunt is unrepresentable while it holds).
    const double over_bound = 2.0 * kCp.deadzone_hi + 1.5 * w_arrive * tau_arr;
    const int n_drift =
        std::min(static_cast<int>(tr.size()),
                 i_arrive + static_cast<int>(0.35 / kAp.sim_dt));
    for (int i = i_arrive; i < n_drift; ++i) {
        CHECK(tr[i].s * sign0 <= over_bound);
        REQUIRE(tr[i].cap == control::CaptureState::IDLE);
    }
    REQUIRE(n_drift > i_arrive);
    // The drift DECAYS: the mean over the last 0.1 s of the window is back
    // at park scale (a sustained weave would hold the over_bound scale).
    double tail_sum = 0.0;
    int tail_n = 0;
    for (int i =
             std::max(i_arrive, n_drift - static_cast<int>(0.1 / kAp.sim_dt));
         i < n_drift; ++i) {
        tail_sum += tr[i].e;
        ++tail_n;
    }
    REQUIRE(tail_n > 0);
    CHECK(tail_sum / tail_n <= 3.0 * kCp.deadzone_hi);
}

// ---------------------------------------------------------------------------
// Leg 13 — the RETURN demand SHAPE pinned open-loop: -min(return_w,
// sqrt(2*alpha*d_remaining)) along the (refreshed) axis. Two distances in
// the sqrt-bound region pin the sqrt(d) shape (ratio 2 at 4x distance — a
// linear branch would read 4: the no-linear-floor clause, executably); a
// lowered return_w dial pins the min() cap.
// ---------------------------------------------------------------------------
TEST_CASE(
    "capture: return demand is min(return_w, sqrt(2 alpha d)) - "
    "pinned open-loop") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    const sim::SimState s0 =
        harness::level_state(kAp, 140.0, 3000.0, up, heading);
    const glm::dvec3 nose = s0.orientation * glm::dvec3{0.0, 0.0, -1.0};
    const glm::dvec3 right = s0.orientation * glm::dvec3{1.0, 0.0, 0.0};
    control::Internal ic = control::reset();
    ic.capture = control::CaptureState::RETURN;
    ic.cap_ux = 1.0;  // pure-pitch captured axis
    ic.cap_uy = 0.0;
    ic.cap_crossed = true;
    ic.cap_inbound = true;  // v3: mid-return (the glance leg is done)
    REQUIRE(glm::length(s0.angular_vel) == 0.0);
    const double alt0 = glm::length(s0.position) - kAp.R;
    const double a_pitch = sim::ang_accel_max_derived(
        kAp.c_pitch, kAp.I_pitch, glm::length(s0.velocity), alt0, kAp);
    const double cff_x = curvature_ff_body(s0).x;

    // Probe: target d PAST the aim center along -u (demand.x = -d).
    auto probe = [&](double d_rad, const control::ControllerParams& cp) {
        control::Input in;
        in.target_dir_world =
            glm::normalize(glm::angleAxis(-d_rad, right) * nose);
        in.throttle = 0.7;
        const control::Output o =
            control::step(s0, in, ic, kAp, cp, kAp.sim_dt);
        REQUIRE(o.telem.capture == control::CaptureState::RETURN);  // held
        return o.telem.omega_des.x - cff_x;  // the raw event demand
    };
    // Two sqrt-bound distances (both under return_w AND under the live G
    // floor so no clamp shapes the probe) — derived from the loaded tables.
    const double d2 =
        0.9 *
        std::min(kCp.capture_return_w * kCp.capture_return_w / (2.0 * a_pitch),
                 std::pow((std::abs(kCp.n_min) + 1.0) * kAp.g / 140.0, 2.0) /
                     (2.0 * a_pitch));
    const double d1 = d2 / 4.0;
    REQUIRE(d1 > kCp.deadzone_lo);  // premise: the probe is outside the exit
    ic.cap_d_allow = 2.0 * d2;      // ample: the yank exit never fires here
    const double w1 = probe(d1, kCp);
    const double w2 = probe(d2, kCp);
    REQUIRE(w1 < 0.0);  // toward center (negative pitch for a +u capture)
    REQUIRE(w2 < 0.0);
    CHECK(w1 == Catch::Approx(-std::sqrt(2.0 * a_pitch * d1)).margin(1e-9));
    CHECK(w2 == Catch::Approx(-std::sqrt(2.0 * a_pitch * d2)).margin(1e-9));
    // sqrt shape: 4x the distance = 2x the rate (linear would read 4x).
    CHECK(w2 / w1 == Catch::Approx(2.0).epsilon(1e-6));
    // The min() cap, config-varied: a return_w BELOW the sqrt at d2 must
    // bind exactly.
    control::ControllerParams cp_low = kCp;
    cp_low.capture_return_w = 0.5 * std::sqrt(2.0 * a_pitch * d2);
    const double w_capped = probe(d2, cp_low);
    CHECK(w_capped == Catch::Approx(-cp_low.capture_return_w).margin(1e-9));
}

// ---------------------------------------------------------------------------
// Leg 14 — jinking aim (the wedge/double-fire machine catch): alternating
// set-jumps every 0.75 s. Each jump's traverse must COMPLETE inside its
// window (the F4 hang class), and the engage count stays bounded by the
// arrival count (no strobing).
// ---------------------------------------------------------------------------
TEST_CASE("capture: jinking aim neither wedges nor strobes the machine") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    double thr = 0.0;
    const sim::SimState s0 =
        harness::level_trim_state(kAp, 180.0, 3000.0, up, heading, &thr);
    harness::ClosedLoop cl(s0, glm::dvec3{0.0, 0.0, -1.0});
    cl.aim_nose();
    cl.tick(thr, kAp, kCp, /*grounded=*/true);
    for (int i = 0; i < 120; ++i) cl.tick(thr, kAp, kCp);
    const int kJumps = 8, kWin = 120;  // 1 s per jump: 15 deg approach
                                       // (~0.4 s) + bounce + settle margin
    std::vector<Tick> tr;
    double sgn = 1.0;
    for (int j = 0; j < kJumps; ++j) {
        const glm::dvec3 nose =
            cl.state.orientation * glm::dvec3{0.0, 0.0, -1.0};
        const glm::dvec3 right =
            cl.state.orientation * glm::dvec3{1.0, 0.0, 0.0};
        cl.aim = glm::normalize(glm::angleAxis(sgn * rad(15.0), right) * nose);
        sgn = -sgn;
        for (int i = 0; i < kWin; ++i) {
            cl.aim_moved = (i == 0);
            Tick tk;
            const control::Telemetry t = cl.tick(thr, kAp, kCp);
            tk.e = t.e;
            tk.cap = t.capture;
            tr.push_back(tk);
        }
        // The traverse completed inside the window: error back near the
        // aim, machine not wedged mid-event.
        INFO("jump " << j);
        REQUIRE(tr.back().e < rad(3.0));
    }
    // Bounded engages: one per arrival, plus at most one crossed-anyway
    // micro-bounce across the whole run (diff red-team P2-4 — the old
    // 2-per-jump license quietly permitted the blend-band multi-engage the
    // P0-2 gate exists to kill).
    CHECK(count_engages(tr) <= kJumps + 1);
    REQUIRE(count_engages(tr) >= kJumps / 2);  // premise: events really ran
}

// ---------------------------------------------------------------------------
// Leg 15 — parked-aim frame-carry no-op: with a parked aim (zero filtered
// aim rate) the aim_ff dial is invisible to an owned event tick (the
// gain-gated ADD contributes exactly +0.0 and the event emission's
// frame-carry term is 0) — the S-wvane ±0.0 discipline for the composition.
// ---------------------------------------------------------------------------
TEST_CASE("capture: parked-aim owned tick identical with aim_ff on or off") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    const sim::SimState s0 =
        harness::level_state(kAp, 140.0, 3000.0, up, heading);
    const glm::dvec3 nose = s0.orientation * glm::dvec3{0.0, 0.0, -1.0};
    const glm::dvec3 right = s0.orientation * glm::dvec3{1.0, 0.0, 0.0};
    control::Internal ic = control::reset();
    ic.capture = control::CaptureState::CARRY;
    ic.cap_ux = 1.0;
    ic.cap_uy = 0.0;
    ic.cap_w_hold = 0.4;
    ic.cap_err0 = rad(10.0);
    ic.aim_rate_filt = glm::dvec3{0.0};  // parked: no frame rate
    control::Input in;
    in.target_dir_world = glm::angleAxis(rad(1.0), right) * nose;
    in.throttle = 0.7;

    REQUIRE(kCp.aim_ff_gain > 0.0);  // premise: the shipped table flies FF
    control::ControllerParams cp_off = kCp;
    cp_off.aim_ff_gain = 0.0;
    const control::Output a = control::step(s0, in, ic, kAp, kCp, kAp.sim_dt);
    const control::Output b =
        control::step(s0, in, ic, kAp, cp_off, kAp.sim_dt);
    REQUIRE(a.telem.capture == control::CaptureState::CARRY);  // still owned
    CHECK(exact_eq(a.telem.omega_des, b.telem.omega_des));
    CHECK(a.inputs.pitch == b.inputs.pitch);
    CHECK(a.inputs.yaw == b.inputs.yaw);
    CHECK(a.inputs.roll == b.inputs.roll);
}
