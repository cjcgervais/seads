#pragma once

// COMFORT INSTRUMENT (docs/comfort program, auto/comfort-orient): scripts the
// exact disorientation maneuvers Chad reported (sustained turn + freelook
// tap-release, immelmann, split-S, full loop, x3 chained loop) through the
// SHIPPED app::tick pipeline (aim frame + control::step + sim::step) with the
// shipped harness::MiniCamera chase basis, and prints machine-greppable metric
// lines:  COMFORT <scenario> <metric> <value>  (one number per line).
//
// The numbers measure GEOMETRY (where the camera points, how the camera-up
// drifts off the local horizon) — a true-answer signal, the sanctioned
// "explain a score" use. They do NOT measure comfort, and this file adds NO
// mechanism and touches NO kernel: it is a READ-ONLY instrument over the
// existing app::tick + MiniCamera, mirroring run_mouseloop's driver exactly.
//
// Metric definitions (dt = p.sim_dt = 1/120, degrees + ticks):
//   up_debt_deg  = |st.aim.up_misalignment(local_up)| * 180/pi  — the
//                  camera-up (== aim.up()) vs local-horizon roll debt.
//   oblique_deg  = angle(cam.cam_fwd, vhat)  — "the camera is NOT behind your
//                  flight path" number (vhat = normalize(velocity)).
//   converged    = oblique_deg < 10 AND up_debt_deg < 10.
//   nan_events   = ticks where any of position/velocity/orientation/cam_fwd/
//                  aim.forward()/aim.up() has a non-finite component.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>

#include "app/instructor_tick.h"
#include "config/load_controller.h"
#include "control/controller.h"
#include "sim/state.h"
#include "sim/world.h"
#include "test/harness/instructor.h"

namespace harness {

namespace comfort_detail {

constexpr double kPi = 3.14159265358979323846;
constexpr double kDeg = 180.0 / kPi;

inline bool finite3(const glm::dvec3& v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}
inline bool finite4(const glm::dquat& q) {
    return std::isfinite(q.w) && std::isfinite(q.x) && std::isfinite(q.y) &&
           std::isfinite(q.z);
}

// A tick's NaN check over the whole state + camera + aim frame (M3 legality).
inline bool tick_has_nan(const app::LoopState& st, const MiniCamera& cam) {
    return !(finite3(st.curr.position) && finite3(st.curr.velocity) &&
             finite4(st.curr.orientation) && finite3(cam.cam_fwd) &&
             finite3(cam.cam_up) && finite3(st.aim.forward()) &&
             finite3(st.aim.up()));
}

inline double up_debt_deg(const app::LoopState& st) {
    const glm::dvec3 lu = sim::local_up(st.curr.position);
    return std::abs(st.aim.up_misalignment(lu)) * kDeg;
}

// angle(cam_fwd, velocity-hat) — the "front oblique" number. Falls back to the
// camera-forward itself when the plane is nearly stopped (vhat undefined), so
// the metric is 0 rather than NaN in that (never-hit here) degenerate case.
inline double oblique_deg(const app::LoopState& st, const MiniCamera& cam) {
    const double sp = glm::length(st.curr.velocity);
    if (sp < 1e-6) return 0.0;
    const glm::dvec3 vhat = st.curr.velocity / sp;
    const glm::dvec3 cf = glm::normalize(cam.cam_fwd);
    const double d = std::clamp(glm::dot(cf, vhat), -1.0, 1.0);
    return std::acos(d) * kDeg;
}

// The at12_reaim geometry, applied to st.aim (a fresh max-rate turn command
// each tick): a horizontal direction kTurnOffsetDeg to the RIGHT of the current
// horizontal heading, recomputed from local_up every tick (SPEC §6.1 — never a
// cached axis), snapped into the aim frame KEEPING the carried up.
constexpr double kTurnOffsetDeg = 70.0;
inline void turn_reaim(app::LoopState& st) {
    const glm::dvec3 up = sim::local_up(st.curr.position);
    const glm::dvec3 nose = st.curr.orientation * glm::dvec3{0.0, 0.0, -1.0};
    glm::dvec3 h = nose - glm::dot(nose, up) * up;  // heading in the horizon
    if (glm::length(h) < 1e-6)                      // nose ~ straight up/down
        h = st.curr.orientation * glm::dvec3{1.0, 0.0, 0.0};
    h = glm::normalize(h);
    const double offset = kTurnOffsetDeg * kPi / 180.0;
    // -offset about up == RIGHT of the heading (matches at12_reaim's sign).
    const glm::dvec3 target = glm::normalize(glm::angleAxis(-offset, up) * h);
    st.aim.snap_forward_to_dir(target, st.curr.orientation);
}

// Shared init mirroring run_mouseloop: level trim, aim reseed, grounded=true,
// MiniCamera seed. Throttle stays 1.0 in every scenario.
inline void init(app::LoopState& st, MiniCamera& cam,
                 const sim::AircraftParams& p, double V, double alt) {
    const glm::dvec3 up0{1.0, 0.0, 0.0}, heading0{0.0, 0.0, -1.0};
    st.curr = level_trim_state(p, V, alt, up0, heading0, nullptr);
    st.prev = st.curr;
    st.prev_up = sim::local_up(st.curr.position);
    st.aim.reseed(st.curr.orientation, st.prev_up);
    st.grounded = true;
    cam.seed(st.curr);
}

// One shipped tick with the one-tick camera lag, then advance the camera.
// Returns the tick's TickResult (telem + respawned). Feeds cam.cam_fwd/cam_up
// (PREVIOUS values) in, exactly as run_mouseloop / main.cpp do. Throttle is
// forced to 1.0 (every comfort scenario runs full throttle).
//
// ⚠ HISTORICAL HAZARD, now structurally impossible (S-keyprec / v8). Under v7
// this wrapper called `cam.advance(...)` with NO keys_flying argument, silently
// taking the `false` default — the S-keychase flag had to be threaded BY HAND
// through every call site, and this convenience wrapper dropped it. That is the
// very instrument fork render::chase_anchor's purity was built to prevent.
// MEASURED HONESTLY when it was fixed: NO comfort number moved, because no
// drive()-based scenario ever reached keys-without-freelook (turnsnap_ovr holds
// its override only while freelook is also held, which the selector excluded;
// no other scenario sets an override at all). So the fork was a real latent
// defect with zero measured consequence — it would have bitten the FIRST
// scenario to model keys-in-mouse-aim, which is exactly what mouseaim_keys is.
// v8 removes the class entirely: MiniCamera::advance now takes no key state.
inline app::TickResult drive(app::LoopState& st, MiniCamera& cam,
                             app::TickInput& in, const sim::AircraftParams& p,
                             const control::ControllerParams& cp) {
    in.throttle = 1.0;
    in.cam_fwd = cam.cam_fwd;  // previous tick's basis (one-tick lag)
    in.cam_up = cam.cam_up;
    const app::TickResult r = app::tick(st, in, p, cp, nullptr);
    // S-orient (Q3): the ORIENT verb's hard camera cut fires BEFORE the frame's
    // camera advance (P2b red-team fix), mirroring main.cpp's shipped order:
    // step_frame -> `if (fr.orient_fired) cam_fwd = loop.aim.forward()` (the
    // cut) -> `cam_fwd = ease_chase_forward(...)` (the advance). So the ease
    // this tick starts FROM the cut forward, not from the pre-cut lag.
    if (r.orient_fired) cam.orient_cut(st.aim.forward());
    cam.advance(st.curr, st.aim.forward(), st.aim.up(), cp, p.sim_dt);
    return r;
}

// mouseloop's deflect-then-hold mouse schedule (§7 mouse-loop). Returns the
// per-tick aim_dy and the number of push ticks for a `deflect_deg` sweep at
// `rate_deg_s`, in the same direction convention as run_mouseloop
// (dir=="down" => +dy => pitch down; "up" => -dy => pitch up).
struct MouseSchedule {
    double dy_tick = 0.0;
    int push_ticks = 0;
};
inline MouseSchedule mouse_schedule(const std::string& dir, double deflect_deg,
                                    double rate_deg_s,
                                    const sim::AircraftParams& p,
                                    const control::ControllerParams& cp) {
    const int sgn = (dir == "down") ? +1 : -1;
    MouseSchedule s;
    s.dy_tick =
        sgn * (rate_deg_s * kPi / 180.0) * p.sim_dt / cp.aim_sensitivity;
    s.push_ticks = static_cast<int>(deflect_deg / (rate_deg_s * p.sim_dt));
    return s;
}

// ---- SHARED double-tap cadence (P1-4) --------------------------------------
// ONE source for the orient double-tap schedule, used by BOTH the `orient` and
// `composed` scenarios (previously `orient` read cp.orient_double_tap_s DIRECTLY
// — ships 0.0 => the verb never fired, silent-disarming every metric — while
// `composed` re-derived the cadence with a local 0.30 fallback, a fork).
//
// window_used = the config dial if a pilot has set it (> 0), else 0.30 s for the
// MECHANISM-MEASUREMENT (we do NOT change the shipped default; the app's orient
// verb stays OFF). tap_gap / dbl_ticks are the scripted press schedule; fl_at(i)
// is the two-hold freelook_held pattern (press, release, press->FIRE, release).
struct TapCadence {
    double window_used = 0.30;
    int tap_gap = 1;
    int dbl_ticks = 8;
    // The scripted freelook_held at scenario-tick i: two holds a tap_gap apart.
    bool fl_at(int i) const {
        return (i >= 0 && i < tap_gap) ||
               (i >= 2 * tap_gap && i < 3 * tap_gap);
    }
};
inline TapCadence tap_cadence(const control::ControllerParams& cp,
                              const sim::AircraftParams& p) {
    TapCadence c;
    c.window_used = cp.orient_double_tap_s > 0.0 ? cp.orient_double_tap_s : 0.30;
    c.tap_gap = std::max(1, static_cast<int>(0.4 * c.window_used / p.sim_dt));
    c.dbl_ticks = 4 * c.tap_gap + 4;
    return c;
}

}  // namespace comfort_detail

// ---- Scenario 1: sustained hard turn (the "front oblique" baseline) --------
inline void comfort_turnsteady(const sim::AircraftParams& p,
                               const control::ControllerParams& cp) {
    using namespace comfort_detail;
    app::LoopState st;
    MiniCamera cam;
    init(st, cam, p, 150.0, 3500.0);

    const int total = static_cast<int>(8.0 / p.sim_dt);  // 8 s
    const int window_start =
        total - static_cast<int>(4.0 / p.sim_dt);  // last 4 s
    double sum_obl = 0.0, sum_debt = 0.0;
    int n_win = 0, converged_ever = 0, nan_events = 0, crashed = 0;

    for (int i = 1; i <= total; ++i) {
        turn_reaim(st);  // model the pilot holding a max-rate turn
        app::TickInput in;
        const app::TickResult r = drive(st, cam, in, p, cp);
        if (tick_has_nan(st, cam)) ++nan_events;
        if (r.respawned) crashed = 1;
        if (i > window_start) {
            const double obl = oblique_deg(st, cam);
            const double debt = up_debt_deg(st);
            sum_obl += obl;
            sum_debt += debt;
            ++n_win;
            if (obl < 10.0 && debt < 10.0) converged_ever = 1;
        }
    }
    std::printf("COMFORT turnsteady standing_oblique_deg %.3f\n",
                n_win ? sum_obl / n_win : 0.0);
    std::printf("COMFORT turnsteady standing_updebt_deg %.3f\n",
                n_win ? sum_debt / n_win : 0.0);
    std::printf("COMFORT turnsteady converged %d\n", converged_ever);
    std::printf("COMFORT turnsteady nan_events %d\n", nan_events);
    std::printf("COMFORT turnsteady crashed %d\n", crashed);
}

// ---- Scenario 1b: sustained turn flown on the KEYS -------------------------
// ⚠⚠ READ THIS BEFORE CALLING THE NUMBER A DEFECT. Under S-keyprec (Chad
// 2026-07-29, SEALED KERNEL v8) this scenario's standing oblique — the camera
// sitting ~16.3 deg off the flight path while the pilot flies on keys with a
// parked aim — is the EXPECTED, CHAD-RULED behavior, not a problem to solve.
// The camera is bound to the AIM by ruling; fly on keys without touching the
// mouse and the plane turns away from where you are pointing, so of course you
// see it obliquely. That IS the deflection view, and the cure is to move the
// mouse. This leg exists to PIN that number, not to drive it down.
//
// History, so it is not re-litigated: v7's S-keychase drove this figure to
// 0.000 by re-anchoring the camera on the flight path whenever a key was held,
// and was flown-approved AGAINST THIS SCENARIO. It was then retired, because
// this scenario parks the aim and Chad does not — he flies mouse-aim AND keys
// together, where the same mechanism measured a 0 -> 67.8 deg camera departure
// from his aim (see comfort_mouseaim_keys, the scenario that was missing). A
// leg that only ever models one hand is how three camera mechanisms in a row
// got validated against a case he does not fly.
// MECHANICS: the turnsteady scenario above re-aims every tick (a MOUSE pilot),
// so the camera chases a target that tracks the turn. THIS one parks the aim
// and flies the turn on a held roll+elevator override — the pure keyboard case,
// where the aim does NOT track and the standing oblique is at its largest. It
// prints the same metric as turnsteady so the two are directly comparable.
// Single-arm since v8: the A/B (key_anchor_rate as shipped vs forced 0) went
// with the retired knob — there is only one camera law now.
inline void comfort_turnsteady_keys(const sim::AircraftParams& p,
                                    const control::ControllerParams& cp) {
    using namespace comfort_detail;
    app::LoopState st;
    MiniCamera cam;
    init(st, cam, p, 150.0, 3500.0);

    const int total = static_cast<int>(8.0 / p.sim_dt);
    const int window_start = total - static_cast<int>(4.0 / p.sim_dt);
    double sum_obl = 0.0;
    int n_win = 0, converged_ever = 0, crashed = 0;

    for (int i = 1; i <= total; ++i) {
        app::TickInput in;
        in.throttle = 1.0;
        in.override_mask[2] = true;  // held roll key: the pilot flies on keys
        in.override_sign[2] = 1.0;
        in.override_mask[0] = true;  // and pulls: a real sustained turn
        in.override_sign[0] = 1.0;
        in.cam_fwd = cam.cam_fwd;
        in.cam_up = cam.cam_up;
        const app::TickResult r = app::tick(st, in, p, cp, nullptr);
        if (r.orient_fired) cam.orient_cut(st.aim.forward());
        cam.advance(st.curr, st.aim.forward(), st.aim.up(), cp, p.sim_dt);
        if (r.respawned) crashed = 1;
        if (i > window_start) {
            const double obl = oblique_deg(st, cam);
            sum_obl += obl;
            ++n_win;
            if (obl < 10.0) converged_ever = 1;
        }
    }
    // EXPECTED ~16.3 deg — the Chad-ruled deflection view, not a defect.
    std::printf("COMFORT turnsteady_keys standing_oblique_deg %.3f\n",
                n_win ? sum_obl / n_win : 0.0);
    std::printf("COMFORT turnsteady_keys converged %d\n", converged_ever);
    std::printf("COMFORT turnsteady_keys crashed %d\n", crashed);
}

// ---- Scenario 1c: MOUSE-AIM flying with hard key modulation (S-keyprec) ----
// THE MISSING CASE, and the reason three camera mechanisms in a row were
// validated against a scenario Chad does not fly. `turnsteady` is a pure mouse
// pilot (aim re-commanded every tick, no keys); `turnsteady_keys` is a pure
// keyboard pilot (aim parked, keys held). Chad's DOMINANT style is BOTH AT
// ONCE — "I usually fly with a combination of mouse aim with hard key inputs to
// maximize control for the fight" (2026-07-29) — and no scenario modeled it.
//
// The pilot holds a mouse-aim turn throughout (turn_reaim every tick, exactly
// as turnsteady) and at t = 4 s adds a held aileron+elevator override to cut
// inside — "if I input some aileron to cut into their path sooner." Under
// Chad's v8 ruling the keys change the TRAJECTORY and must change NOTHING about
// the camera: it stays bound to the aim with the flown lag, continuously.
//
// Metric = aim_lag_deg = angle(cam_fwd, aim.forward()) — the camera's lag
// BEHIND THE AIM, which is the quantity the ruling constrains (oblique_deg
// measures against velocity, which is the S-keychase question, not this one).
//   lag_before = mean over the 1 s BEFORE the keypress
//   lag_during = mean over the last 2 s of the key hold
//   max_step   = largest single-tick change in aim_lag_deg in the 1 s AFTER the
//                keypress edge — the "snap"
//
// ⚠ HOW TO READ max_step. The S-keychase anchor swap is EASED, not instant: the
// per-tick step is bounded by min(gap, key_anchor_rate*dt) = 6.0/120 = 0.05 rad
// ~ 2.9 deg/tick BY CONSTRUCTION. So max_step saturates near 2.9 no matter how
// violent the snap feels in the hands, and reading the peak alone under-reports
// it badly. The signal is (a) lag_during vs lag_before — the camera ABANDONING
// the aim — and (b) that 2.9 deg/tick rate SUSTAINED until the gap closes.
inline void comfort_mouseaim_keys(const sim::AircraftParams& p,
                                  const control::ControllerParams& cp) {
    using namespace comfort_detail;
    app::LoopState st;
    MiniCamera cam;
    init(st, cam, p, 150.0, 3500.0);

    const int total = static_cast<int>(8.0 / p.sim_dt);       // 8 s
    const int key_at = static_cast<int>(4.0 / p.sim_dt);      // keys down at 4 s
    const int pre_start = key_at - static_cast<int>(1.0 / p.sim_dt);  // 1 s pre
    const int dur_start = total - static_cast<int>(2.0 / p.sim_dt);   // last 2 s
    const int step_end = key_at + static_cast<int>(1.0 / p.sim_dt);   // 1 s post

    double sum_pre = 0.0, sum_dur = 0.0, max_step = 0.0, prev_lag = 0.0;
    int n_pre = 0, n_dur = 0, crashed = 0, nan_events = 0;

    for (int i = 1; i <= total; ++i) {
        turn_reaim(st);  // the MOUSE hand, commanding the turn every tick
        app::TickInput in;
        if (i >= key_at) {  // and the hard keys, cutting inside
            in.override_mask[2] = true;  // aileron
            in.override_sign[2] = 1.0;
            in.override_mask[0] = true;  // elevator
            in.override_sign[0] = 1.0;
        }
        const app::TickResult r = drive(st, cam, in, p, cp);
        if (tick_has_nan(st, cam)) ++nan_events;
        if (r.respawned) crashed = 1;

        const double lag =
            std::acos(std::clamp(
                glm::dot(glm::normalize(cam.cam_fwd), st.aim.forward()), -1.0,
                1.0)) *
            kDeg;
        if (i > pre_start && i < key_at) {
            sum_pre += lag;
            ++n_pre;
        }
        if (i > dur_start) {
            sum_dur += lag;
            ++n_dur;
        }
        // The snap: per-tick change in the lag, measured across the key edge.
        if (i >= key_at && i <= step_end)
            max_step = std::max(max_step, std::abs(lag - prev_lag));
        prev_lag = lag;
    }
    std::printf("COMFORT mouseaim_keys lag_before_deg %.3f\n",
                n_pre ? sum_pre / n_pre : 0.0);
    std::printf("COMFORT mouseaim_keys lag_during_deg %.3f\n",
                n_dur ? sum_dur / n_dur : 0.0);
    std::printf("COMFORT mouseaim_keys max_step_deg %.3f\n", max_step);
    std::printf("COMFORT mouseaim_keys nan_events %d\n", nan_events);
    std::printf("COMFORT mouseaim_keys crashed %d\n", crashed);
}

// ---- Scenario 2: turn then a freelook TAP with NO override ------------------
// He keeps commanding the turn throughout (per-tick aim snap models his held
// mouse turn even while freelook is held — the camera law is what's under
// test). LAW NOTE (S-relorient 2026-07-28): under the shipped table
// (release_orient = true) a no-override release now SNAPS aim := guarded
// velocity + camera-cuts; the pre-relorient "release moves nothing (S7-nest
// 4d)" arm is the knob-off table only. The per-tick turn_reaim overwrites
// the snapped aim next tick, so the metric still reads camera reconvergence
// under a held turn — with a one-tick snap+cut transient at the release.
inline void comfort_turnsnap_tap(const sim::AircraftParams& p,
                                 const control::ControllerParams& cp) {
    using namespace comfort_detail;
    app::LoopState st;
    MiniCamera cam;
    init(st, cam, p, 150.0, 3500.0);

    const int turn_ticks = static_cast<int>(6.0 / p.sim_dt);  // 6 s
    const int hold_ticks = 30;
    const int post_ticks = static_cast<int>(6.0 / p.sim_dt);  // 6 s
    int nan_events = 0, crashed = 0;
    int converged_ticks = -1;
    double peak_oblique = 0.0, final_updebt = 0.0;

    // Phase A: establish the turn.
    for (int i = 0; i < turn_ticks; ++i) {
        turn_reaim(st);
        app::TickInput in;
        drive(st, cam, in, p, cp);
        if (tick_has_nan(st, cam)) ++nan_events;
    }
    // Phase B: freelook held 30 ticks, no override, KEEP turning.
    for (int i = 0; i < hold_ticks; ++i) {
        turn_reaim(st);
        app::TickInput in;
        in.freelook_held = true;
        drive(st, cam, in, p, cp);
        if (tick_has_nan(st, cam)) ++nan_events;
    }
    // Phase C: release, KEEP turning; measure from the release tick on.
    for (int i = 0; i < post_ticks; ++i) {
        turn_reaim(st);
        app::TickInput in;  // freelook released
        const app::TickResult r = drive(st, cam, in, p, cp);
        if (tick_has_nan(st, cam)) ++nan_events;
        if (r.respawned) crashed = 1;
        const double obl = oblique_deg(st, cam);
        const double debt = up_debt_deg(st);
        peak_oblique = std::max(peak_oblique, obl);
        final_updebt = debt;
        if (converged_ticks < 0 && obl < 10.0 && debt < 10.0)
            converged_ticks = i;
    }
    std::printf("COMFORT turnsnap_tap converged_ticks %d\n", converged_ticks);
    std::printf("COMFORT turnsnap_tap peak_oblique_deg %.3f\n", peak_oblique);
    std::printf("COMFORT turnsnap_tap final_updebt_deg %.3f\n", final_updebt);
    std::printf("COMFORT turnsnap_tap nan_events %d\n", nan_events);
    std::printf("COMFORT turnsnap_tap crashed %d\n", crashed);
}

// ---- Scenario 2b: the ORIENT verb (S-orient, Q3) ---------------------------
// Same sustained turn as turnsnap_tap, but at 6 s the pilot DOUBLE-TAPS the
// freelook key (two press-release pairs within orient_double_tap_s). The
// caller-side input::OrientTap detector fires on the second press-edge; the
// tick snaps aim := guarded velocity, captures the S7-hrz up-debt roll, and the
// MiniCamera hard-cuts cam_fwd behind the flight path. Expected vs the tap
// baseline (converged_ticks -1): converged_ticks collapses to a small number
// (the cut is instant), up-debt settles at the S7-hrz rate.
inline void comfort_orient(const sim::AircraftParams& p,
                           const control::ControllerParams& cp) {
    using namespace comfort_detail;
    app::LoopState st;
    MiniCamera cam;
    init(st, cam, p, 150.0, 3500.0);

    const int turn_ticks = static_cast<int>(6.0 / p.sim_dt);  // 6 s
    const int post_ticks = static_cast<int>(6.0 / p.sim_dt);  // 6 s
    // The double-tap cadence from the SHARED helper (P1-4): window_used is the
    // config dial if set, else 0.30 for the mechanism measurement — so `orient`
    // FIRES again even though cp.orient_double_tap_s ships 0.0 (reading the dial
    // directly silent-disarmed every metric). Same helper `composed` uses.
    const TapCadence cad = tap_cadence(cp, p);
    const double window_used = cad.window_used;
    std::printf("COMFORT orient window_used %.2f\n", window_used);
    int nan_events = 0, crashed = 0;
    int converged_ticks = -1, updebt_settle_ticks = -1;
    double peak_oblique = 0.0, updebt_at_fire = 0.0;
    double oblique_before_fire = 0.0, oblique_at_fire = 0.0;

    // The caller's OrientTap detector + press-edge tracker, run at sim_dt here
    // (mirrors main.cpp: the detector is caller-side, feeding in.orient_cmd).
    input::OrientTap tap;
    bool prev_fl = false;
    bool fired_once = false;

    // Phase A: establish the turn, no freelook.
    for (int i = 0; i < turn_ticks; ++i) {
        turn_reaim(st);
        app::TickInput in;
        const bool fl = false;
        in.orient_cmd = tap.step(fl && !prev_fl, p.sim_dt, window_used);
        prev_fl = fl;
        in.freelook_held = fl;
        drive(st, cam, in, p, cp);
        if (tick_has_nan(st, cam)) ++nan_events;
    }
    // Phase B: DOUBLE-TAP over the next few ticks (press, release, press,
    // release) — a scripted freelook_held schedule; the detector builds the
    // press-edges and fires on the second. Keep commanding the turn.
    //   tick 0: press (edge 1)   tick tap_gap: release
    //   tick 2*tap_gap: press (edge 2 -> FIRE)  then release
    const int dbl_ticks = cad.dbl_ticks;
    for (int i = 0; i < dbl_ticks; ++i) {
        turn_reaim(st);
        app::TickInput in;
        const bool fl = cad.fl_at(i);  // two holds, shared cadence
        in.orient_cmd = tap.step(fl && !prev_fl, p.sim_dt, window_used);
        prev_fl = fl;
        in.freelook_held = fl;
        const double obl_pre =
            oblique_deg(st, cam);  // BEFORE this tick's drive
        const app::TickResult r = drive(st, cam, in, p, cp);
        if (r.orient_fired && !fired_once) {
            fired_once = true;
            updebt_at_fire = up_debt_deg(st);
            oblique_before_fire = obl_pre;           // the lagged oblique
            oblique_at_fire = oblique_deg(st, cam);  // AFTER the hard cut
        }
        if (tick_has_nan(st, cam)) ++nan_events;
        if (r.respawned) crashed = 1;
    }
    // Phase C: released, KEEP turning; measure convergence from here (the
    // S7-hrz roll plays out open-loop, the cut already re-seated the camera).
    for (int i = 0; i < post_ticks; ++i) {
        turn_reaim(st);
        app::TickInput in;  // freelook released, no orient
        in.orient_cmd = tap.step(false, p.sim_dt, window_used);
        prev_fl = false;
        const app::TickResult r = drive(st, cam, in, p, cp);
        if (tick_has_nan(st, cam)) ++nan_events;
        if (r.respawned) crashed = 1;
        const double obl = oblique_deg(st, cam);
        const double debt = up_debt_deg(st);
        peak_oblique = std::max(peak_oblique, obl);
        if (converged_ticks < 0 && obl < 10.0 && debt < 10.0)
            converged_ticks = i;
        if (updebt_settle_ticks < 0 && debt < 5.0) updebt_settle_ticks = i;
    }
    std::printf("COMFORT orient fired %d\n", fired_once ? 1 : 0);
    std::printf("COMFORT orient oblique_before_fire_deg %.3f\n",
                oblique_before_fire);
    std::printf("COMFORT orient oblique_at_fire_deg %.3f\n", oblique_at_fire);
    std::printf("COMFORT orient converged_ticks %d\n", converged_ticks);
    std::printf("COMFORT orient peak_oblique_deg %.3f\n", peak_oblique);
    std::printf("COMFORT orient updebt_at_fire_deg %.3f\n", updebt_at_fire);
    std::printf("COMFORT orient updebt_settle_ticks %d\n", updebt_settle_ticks);
    std::printf("COMFORT orient nan_events %d\n", nan_events);
    std::printf("COMFORT orient crashed %d\n", crashed);
}

// ---- Scenario 3: turn then freelook + override, release BOTH & let settle --
// The S7-nest release snaps the aim to the guarded velocity and the pilot lets
// it settle (STOP commanding the aim from the release tick on).
inline void comfort_turnsnap_ovr(const sim::AircraftParams& p,
                                 const control::ControllerParams& cp) {
    using namespace comfort_detail;
    app::LoopState st;
    MiniCamera cam;
    init(st, cam, p, 150.0, 3500.0);

    const int turn_ticks = static_cast<int>(6.0 / p.sim_dt);
    const int hold_ticks = 30;
    const int post_ticks = static_cast<int>(6.0 / p.sim_dt);
    int nan_events = 0, crashed = 0;
    int converged_ticks = -1, updebt_settle_ticks = -1;
    double peak_oblique = 0.0, updebt_at_release = 0.0;

    // Phase A: establish the turn.
    for (int i = 0; i < turn_ticks; ++i) {
        turn_reaim(st);
        app::TickInput in;
        drive(st, cam, in, p, cp);
        if (tick_has_nan(st, cam)) ++nan_events;
    }
    // Phase B: freelook + override[2] (+1) held 30 ticks, KEEP turning.
    for (int i = 0; i < hold_ticks; ++i) {
        turn_reaim(st);
        app::TickInput in;
        in.freelook_held = true;
        in.override_mask[2] = true;
        in.override_sign[2] = 1.0;
        drive(st, cam, in, p, cp);
        if (tick_has_nan(st, cam)) ++nan_events;
    }
    // Phase C: release BOTH and STOP commanding the aim; let it settle.
    for (int i = 0; i < post_ticks; ++i) {
        app::TickInput in;  // no freelook, no override, no aim command
        const app::TickResult r = drive(st, cam, in, p, cp);
        if (tick_has_nan(st, cam)) ++nan_events;
        if (r.respawned) crashed = 1;
        const double obl = oblique_deg(st, cam);
        const double debt = up_debt_deg(st);
        if (i == 0) updebt_at_release = debt;
        peak_oblique = std::max(peak_oblique, obl);
        if (converged_ticks < 0 && obl < 10.0 && debt < 10.0)
            converged_ticks = i;
        if (updebt_settle_ticks < 0 && debt < 5.0) updebt_settle_ticks = i;
    }
    std::printf("COMFORT turnsnap_ovr converged_ticks %d\n", converged_ticks);
    std::printf("COMFORT turnsnap_ovr peak_oblique_deg %.3f\n", peak_oblique);
    std::printf("COMFORT turnsnap_ovr updebt_at_release_deg %.3f\n",
                updebt_at_release);
    std::printf("COMFORT turnsnap_ovr updebt_settle_ticks %d\n",
                updebt_settle_ticks);
    std::printf("COMFORT turnsnap_ovr nan_events %d\n", nan_events);
    std::printf("COMFORT turnsnap_ovr crashed %d\n", crashed);
}

// ---- Scenario 3b (v9): freelook + keys held, RELEASE, keep flying on keys --
// THE sequence Chad actually flies, never instrumented before v9 (the fourth
// camera round's process lesson — docs/DECISIONS.md, mandalark-kernel ledger
// @ b4c0751): hard turn -> freelook WITH override keys held -> release
// freelook -> keep turning on the keys. Metrics are NOSE-referenced: the
// historical oblique_at_fire_deg 0.375 was VELOCITY-referenced and read
// perfect while the camera sat ~AoA off the AIRFRAME at the exact instant the
// mouse takes over (term A), and never printed the up term at all (term B).
// vel_at_fire_deg is kept beside the nose number to prove the old metric was
// blind, not wrong. nose_after_1s/3s are the sustained reads, so an
// instant-only fix cannot masquerade as complete.
inline void comfort_freelook_release_keys(const sim::AircraftParams& p,
                                          const control::ControllerParams& cp) {
    using namespace comfort_detail;
    app::LoopState st;
    MiniCamera cam;
    init(st, cam, p, 150.0, 3500.0);

    const int turn_ticks = static_cast<int>(6.0 / p.sim_dt);
    const int hold_ticks = 30;
    const int post_ticks = static_cast<int>(4.0 / p.sim_dt);
    const int t_1s = static_cast<int>(1.0 / p.sim_dt);
    const int t_3s = static_cast<int>(3.0 / p.sim_dt);
    int nan_events = 0, crashed = 0, fired = 0;
    double nose_at_fire = 0.0, vel_at_fire = 0.0, updebt_after_release = 0.0;
    double nose_after_1s = 0.0, nose_after_3s = 0.0;

    const auto cam_to = [&](const glm::dvec3& ref) {
        return std::acos(std::clamp(glm::dot(glm::normalize(cam.cam_fwd),
                                             glm::normalize(ref)),
                                    -1.0, 1.0)) *
               kDeg;
    };
    const auto set_keys = [](app::TickInput& in) {
        in.override_mask[2] = true;  // held aileron
        in.override_sign[2] = 1.0;
        in.override_mask[0] = true;  // and elevator: a real sustained turn
        in.override_sign[0] = 1.0;
    };

    // Phase A: establish the turn (mouse pilot).
    for (int i = 0; i < turn_ticks; ++i) {
        turn_reaim(st);
        app::TickInput in;
        drive(st, cam, in, p, cp);
        if (tick_has_nan(st, cam)) ++nan_events;
    }
    // Phase B: freelook held WITH the keys held. No turn_reaim — in freelook
    // the mouse is on the camera, not the aim.
    for (int i = 0; i < hold_ticks; ++i) {
        app::TickInput in;
        in.freelook_held = true;
        set_keys(in);
        drive(st, cam, in, p, cp);
        if (tick_has_nan(st, cam)) ++nan_events;
    }
    // Phase C: release freelook, keys STILL held, keep flying on the keys.
    for (int i = 0; i < post_ticks; ++i) {
        app::TickInput in;
        set_keys(in);
        const app::TickResult r = drive(st, cam, in, p, cp);
        if (tick_has_nan(st, cam)) ++nan_events;
        if (r.respawned) crashed = 1;
        const glm::dvec3 nose =
            st.curr.orientation * glm::dvec3{0.0, 0.0, -1.0};
        if (i == 0) {
            fired = r.orient_fired ? 1 : 0;
            nose_at_fire = cam_to(nose);
            vel_at_fire = cam_to(st.curr.velocity);
            updebt_after_release = up_debt_deg(st);
        }
        if (i == t_1s) nose_after_1s = cam_to(nose);
        if (i == t_3s) nose_after_3s = cam_to(nose);
    }
    std::printf("COMFORT freelook_release_keys fired %d\n", fired);
    std::printf("COMFORT freelook_release_keys nose_at_fire_deg %.3f\n",
                nose_at_fire);
    std::printf("COMFORT freelook_release_keys vel_at_fire_deg %.3f\n",
                vel_at_fire);
    std::printf("COMFORT freelook_release_keys nose_after_1s_deg %.3f\n",
                nose_after_1s);
    std::printf("COMFORT freelook_release_keys nose_after_3s_deg %.3f\n",
                nose_after_3s);
    std::printf("COMFORT freelook_release_keys updebt_after_release_deg %.3f\n",
                updebt_after_release);
    std::printf("COMFORT freelook_release_keys nan_events %d\n", nan_events);
    std::printf("COMFORT freelook_release_keys crashed %d\n", crashed);
}

// ---- Scenario 3c (v9): mid-turn freelook with NO keys — the weld pin -------
// Guard-required by the weld ruling (docs/DECISIONS.md @ b4c0751, entry 1):
// freelook ENTRY welds aim := nose, keys or not, so mid-turn Space with hands
// off the keys must HOLD the nose heading (the old law kept carving the parked
// aim's turn — the retired MB-lean side scope) and the release must be a no-op
// on the aim. On v8 this leg DOCUMENTS the carve; after the weld it pins the
// ruling.
//   nose_drift_hold_deg  = nose heading change over the 2 s hold (carve size)
//   aim_nose_hold_deg    = angle(aim, nose) at the end of the hold (weld => ~0)
//   aim_jump_release_deg = the aim's own move across the release tick (~0.005
//                          deg of transport is the honest floor, not zero)
//   aim_nose_release_deg = angle(aim, nose) after the release tick (no-op => ~0)
inline void comfort_freelook_nokeys(const sim::AircraftParams& p,
                                    const control::ControllerParams& cp) {
    using namespace comfort_detail;
    app::LoopState st;
    MiniCamera cam;
    init(st, cam, p, 150.0, 3500.0);

    const int turn_ticks = static_cast<int>(6.0 / p.sim_dt);
    const int hold_ticks = static_cast<int>(2.0 / p.sim_dt);
    int nan_events = 0, crashed = 0;
    const auto angle_deg = [](const glm::dvec3& a, const glm::dvec3& b) {
        return std::acos(std::clamp(glm::dot(glm::normalize(a),
                                             glm::normalize(b)),
                                    -1.0, 1.0)) *
               kDeg;
    };

    // Phase A: establish the turn (mouse pilot).
    for (int i = 0; i < turn_ticks; ++i) {
        turn_reaim(st);
        app::TickInput in;
        drive(st, cam, in, p, cp);
        if (tick_has_nan(st, cam)) ++nan_events;
    }
    const glm::dvec3 nose_entry =
        st.curr.orientation * glm::dvec3{0.0, 0.0, -1.0};
    // Phase B: hold Space, hands off the keys AND the aim (the mouse is on the
    // camera in freelook).
    for (int i = 0; i < hold_ticks; ++i) {
        app::TickInput in;
        in.freelook_held = true;
        const app::TickResult r = drive(st, cam, in, p, cp);
        if (tick_has_nan(st, cam)) ++nan_events;
        if (r.respawned) crashed = 1;
    }
    const glm::dvec3 nose_hold =
        st.curr.orientation * glm::dvec3{0.0, 0.0, -1.0};
    const double nose_drift_hold = angle_deg(nose_entry, nose_hold);
    const double aim_nose_hold = angle_deg(st.aim.forward(), nose_hold);
    const glm::dvec3 aim_pre = st.aim.forward();
    // Phase C: release, hands still off everything — ONE tick, the aim law.
    app::TickInput rel;
    const app::TickResult r = drive(st, cam, rel, p, cp);
    if (tick_has_nan(st, cam)) ++nan_events;
    if (r.respawned) crashed = 1;
    const glm::dvec3 nose_rel =
        st.curr.orientation * glm::dvec3{0.0, 0.0, -1.0};
    const double aim_jump_release = angle_deg(aim_pre, st.aim.forward());
    const double aim_nose_release = angle_deg(st.aim.forward(), nose_rel);

    std::printf("COMFORT freelook_nokeys nose_drift_hold_deg %.3f\n",
                nose_drift_hold);
    std::printf("COMFORT freelook_nokeys aim_nose_hold_deg %.3f\n",
                aim_nose_hold);
    std::printf("COMFORT freelook_nokeys aim_jump_release_deg %.3f\n",
                aim_jump_release);
    std::printf("COMFORT freelook_nokeys aim_nose_release_deg %.3f\n",
                aim_nose_release);
    std::printf("COMFORT freelook_nokeys fired %d\n", r.orient_fired ? 1 : 0);
    std::printf("COMFORT freelook_nokeys nan_events %d\n", nan_events);
    std::printf("COMFORT freelook_nokeys crashed %d\n", crashed);
}

// ---- Scenarios 4/5: immelmann / split-S (mouse deflect-then-hold) ----------
// Copies run_mouseloop's deflect-then-hold: sweep the mouse at 180 deg/s to
// `deflect` deg, then hold, run to `total_s` total. dir=="up" => immelmann,
// "down" => split-S.
inline void comfort_vertical(const char* scenario, const std::string& dir,
                             double deflect_deg, double V, double alt,
                             double total_s, const sim::AircraftParams& p,
                             const control::ControllerParams& cp) {
    using namespace comfort_detail;
    app::LoopState st;
    MiniCamera cam;
    init(st, cam, p, V, alt);

    const MouseSchedule sch = mouse_schedule(dir, deflect_deg, 180.0, p, cp);
    const int total = static_cast<int>(total_s / p.sim_dt);
    int nan_events = 0, crashed = 0;
    double max_oblique = 0.0, final_updebt = 0.0;
    control::Telemetry last{};

    for (int i = 1; i <= total; ++i) {
        app::TickInput in;
        in.aim_dy = (i <= sch.push_ticks) ? sch.dy_tick : 0.0;
        const app::TickResult r = drive(st, cam, in, p, cp);
        last = r.telem;
        if (tick_has_nan(st, cam)) ++nan_events;
        if (r.respawned) crashed = 1;
        max_oblique = std::max(max_oblique, oblique_deg(st, cam));
        final_updebt = up_debt_deg(st);
    }
    const glm::dvec3 nose = st.curr.orientation * glm::dvec3{0.0, 0.0, -1.0};
    const int captured = glm::dot(nose, st.aim.forward()) > 0.8 ? 1 : 0;
    (void)last;
    std::printf("COMFORT %s final_updebt_deg %.3f\n", scenario, final_updebt);
    std::printf("COMFORT %s max_oblique_deg %.3f\n", scenario, max_oblique);
    std::printf("COMFORT %s captured %d\n", scenario, captured);
    std::printf("COMFORT %s nan_events %d\n", scenario, nan_events);
    std::printf("COMFORT %s crashed %d\n", scenario, crashed);
}

// ---- Scenario 6: three chained vertical loops -----------------------------
// For k in 1..3: up-deflect 170 deg at 180 deg/s, hold until telem.e < 10 deg
// OR 6 s timeout; up-deflect 170 again, hold until captured again. Each k is
// ~360 deg of pitch. Prints up-debt after each loop, plus nan/crash. Returns
// the END LoopState/MiniCamera by out-params so the recovery scenario can
// continue from it.
inline void comfort_loopx3(const sim::AircraftParams& p,
                           const control::ControllerParams& cp,
                           app::LoopState* out_st, MiniCamera* out_cam,
                           int* out_nan) {
    using namespace comfort_detail;
    app::LoopState st;
    MiniCamera cam;
    init(st, cam, p, 220.0, 4000.0);

    const MouseSchedule sch = mouse_schedule("up", 170.0, 180.0, p, cp);
    const int cap_timeout = static_cast<int>(6.0 / p.sim_dt);  // 6 s
    const double cap_e = 10.0 * kPi / 180.0;  // telem.e < 10 deg
    int nan_events = 0, crashed = 0;

    // One half-loop: sweep push_ticks, then hold until e < cap_e or timeout.
    auto half_loop = [&](void) {
        for (int i = 0; i < cap_timeout; ++i) {
            app::TickInput in;
            in.aim_dy = (i < sch.push_ticks) ? sch.dy_tick : 0.0;
            const app::TickResult r = drive(st, cam, in, p, cp);
            if (tick_has_nan(st, cam)) ++nan_events;
            if (r.respawned) crashed = 1;
            // Capture check only AFTER the push has finished sweeping.
            if (i >= sch.push_ticks && r.telem.e < cap_e) break;
        }
    };

    for (int k = 1; k <= 3; ++k) {
        half_loop();  // ~180 deg of pitch
        half_loop();  // ~another 180 deg -> ~360 deg total this k
        std::printf("COMFORT loopx3 updebt_after_loop%d_deg %.3f\n", k,
                    up_debt_deg(st));
    }
    std::printf("COMFORT loopx3 nan_events %d\n", nan_events);
    std::printf("COMFORT loopx3 crashed %d\n", crashed);

    if (out_st) *out_st = st;
    if (out_cam) *out_cam = cam;
    if (out_nan) *out_nan = nan_events;
}

// ---- Scenario 7: recovery — continue from loopx3's end state ---------------
// A freelook tap (30 ticks held, no override, release) then zero input. Metrics
// from the tap. roll_played_deg is INFERRED from the up-debt at release minus
// the residual (do NOT modify input/ to instrument it).
inline void comfort_recovery(const sim::AircraftParams& p,
                             const control::ControllerParams& cp,
                             app::LoopState st, MiniCamera cam) {
    using namespace comfort_detail;
    const int hold_ticks = 30;
    const int post_ticks = static_cast<int>(6.0 / p.sim_dt);  // 6 s settle
    int nan_events = 0, crashed = 0;
    int updebt_settle_ticks = -1;
    double updebt_at_tap = up_debt_deg(st);

    // Freelook tap: 30 ticks held, no override, zero mouse.
    for (int i = 0; i < hold_ticks; ++i) {
        app::TickInput in;
        in.freelook_held = true;
        const app::TickResult r = drive(st, cam, in, p, cp);
        if (tick_has_nan(st, cam)) ++nan_events;
        if (r.respawned) crashed = 1;
    }
    // Release + zero input; measure the open-loop roll recovery playing out.
    double updebt_at_release = up_debt_deg(st);
    double residual_updebt = updebt_at_release;
    for (int i = 0; i < post_ticks; ++i) {
        app::TickInput in;  // released, zero mouse
        const app::TickResult r = drive(st, cam, in, p, cp);
        if (tick_has_nan(st, cam)) ++nan_events;
        if (r.respawned) crashed = 1;
        residual_updebt = up_debt_deg(st);
        if (updebt_settle_ticks < 0 && residual_updebt < 5.0)
            updebt_settle_ticks = i;
    }
    // roll_played_deg: the amount of up-debt the recovery retired (release
    // debt minus the residual it settled to). Inferred, not instrumented.
    const double roll_played = updebt_at_release - residual_updebt;

    std::printf("COMFORT recovery updebt_at_tap_deg %.3f\n", updebt_at_tap);
    std::printf("COMFORT recovery updebt_settle_ticks %d\n",
                updebt_settle_ticks);
    std::printf("COMFORT recovery roll_played_deg %.3f\n", roll_played);
    std::printf("COMFORT recovery nan_events %d\n", nan_events);
    std::printf("COMFORT recovery crashed %d\n", crashed);
}

// ---- Scenario 8: the COMPOSITION (Q7 — the "ultimate solution" number) -----
// immelmann exit + one orient verb = camera behind flight path AND world
// righted, in ONE composed maneuver. Fly the immelmann (up-deflect 170, hold to
// capture), wait +1 s, then execute a full double-tap through the OrientTap
// cadence — the same detector the `orient` scenario models. The orient verb
// (a) snaps aim := guarded velocity, (b) captures the S7-hrz up-debt roll, and
// (c) hard-cuts the lagged camera behind the flight path.
//
// DETECTOR-CADENCE MODELING NOTE: cp.orient_double_tap_s ships 0.0 (the verb is
// OFF by default). This scenario MEASURES THE MECHANISM, so we read that dial
// and — only if it is 0 — use 0.30 s as the LOCAL detector window for THIS
// scenario's cadence modeling (printed as `window_used`). We do NOT change the
// shipped default; the app's orient verb stays OFF. If Chad has set a nonzero
// window, we honor it.
inline void comfort_composed(const sim::AircraftParams& p,
                             const control::ControllerParams& cp) {
    using namespace comfort_detail;
    app::LoopState st;
    MiniCamera cam;
    init(st, cam, p, 220.0, 3500.0);

    // Detector window + cadence from the SHARED helper (P1-4): identical to the
    // `orient` scenario. window_used = the config dial if set, else 0.30 for the
    // mechanism measurement (we do NOT change the shipped default).
    const TapCadence cad = tap_cadence(cp, p);
    const double window_used = cad.window_used;
    std::printf("COMFORT composed window_used %.2f\n", window_used);

    const MouseSchedule sch = mouse_schedule("up", 170.0, 180.0, p, cp);
    const double cap_e = 10.0 * kPi / 180.0;                    // telem.e < 10°
    const int cap_timeout = static_cast<int>(8.0 / p.sim_dt);   // 8 s cap
    const int post_ticks = static_cast<int>(6.0 / p.sim_dt);    // 6 s settle
    int nan_events = 0, crashed = 0;

    // Phase A: the immelmann — sweep the up-deflect, then hold to capture.
    for (int i = 0; i < cap_timeout; ++i) {
        app::TickInput in;
        in.aim_dy = (i < sch.push_ticks) ? sch.dy_tick : 0.0;
        const app::TickResult r = drive(st, cam, in, p, cp);
        if (tick_has_nan(st, cam)) ++nan_events;
        if (r.respawned) crashed = 1;
        if (i >= sch.push_ticks && r.telem.e < cap_e) break;  // captured
    }

    // Phase B: +1 s of level hold after capture (aim settled, camera lagging).
    const int settle_ticks = static_cast<int>(1.0 / p.sim_dt);  // +1 s
    for (int i = 0; i < settle_ticks; ++i) {
        app::TickInput in;  // zero mouse (holding the exit attitude)
        const app::TickResult r = drive(st, cam, in, p, cp);
        if (tick_has_nan(st, cam)) ++nan_events;
        if (r.respawned) crashed = 1;
    }

    // The pre-fire state = the immelmann-exit debt the pilot carries in.
    const double updebt_before_fire = up_debt_deg(st);   // expect ~180

    // Phase C: the DOUBLE-TAP through the OrientTap cadence, using the SHARED
    // helper's window_used (NOT cp.orient_double_tap_s directly — that ships 0
    // and would never fire). Same scripted press schedule as the `orient`
    // scenario.
    input::OrientTap tap;
    bool prev_fl = false, fired_once = false;
    double updebt_at_fire = 0.0, oblique_at_fire = 0.0;
    // P2: measure oblique_before_fire on the tick BEFORE the fire (same
    // semantics as `orient`'s obl_pre — the LAGGED oblique the pilot carried in),
    // not a fixed post-Phase-B snapshot.
    double oblique_before_fire = 0.0;

    const int dbl_ticks = cad.dbl_ticks;
    for (int i = 0; i < dbl_ticks; ++i) {
        app::TickInput in;  // zero mouse: the orient verb, not a mouse move
        const bool fl = cad.fl_at(i);  // two holds, shared cadence
        in.orient_cmd = tap.step(fl && !prev_fl, p.sim_dt, window_used);
        prev_fl = fl;
        in.freelook_held = fl;
        const double obl_pre = oblique_deg(st, cam);  // BEFORE this tick's drive
        const app::TickResult r = drive(st, cam, in, p, cp);
        if (r.orient_fired && !fired_once) {
            fired_once = true;
            updebt_at_fire = up_debt_deg(st);
            oblique_before_fire = obl_pre;           // the lagged oblique
            oblique_at_fire = oblique_deg(st, cam);  // AFTER the hard cut
        }
        if (tick_has_nan(st, cam)) ++nan_events;
        if (r.respawned) crashed = 1;
    }

    // Phase D: released, zero input; measure the S7-hrz roll draining the debt
    // and the camera convergence behind the (now steady) flight path.
    int converged_ticks = -1, updebt_settle_ticks = -1;
    for (int i = 0; i < post_ticks; ++i) {
        app::TickInput in;  // freelook released, no orient, zero mouse
        in.orient_cmd = tap.step(false, p.sim_dt, window_used);
        prev_fl = false;
        const app::TickResult r = drive(st, cam, in, p, cp);
        if (tick_has_nan(st, cam)) ++nan_events;
        if (r.respawned) crashed = 1;
        const double obl = oblique_deg(st, cam);
        const double debt = up_debt_deg(st);
        if (converged_ticks < 0 && obl < 10.0 && debt < 10.0)
            converged_ticks = i;
        if (updebt_settle_ticks < 0 && debt < 5.0) updebt_settle_ticks = i;
    }

    std::printf("COMFORT composed fired %d\n", fired_once ? 1 : 0);
    std::printf("COMFORT composed updebt_before_fire_deg %.3f\n",
                updebt_before_fire);
    std::printf("COMFORT composed oblique_before_fire_deg %.3f\n",
                oblique_before_fire);
    std::printf("COMFORT composed oblique_at_fire_deg %.3f\n", oblique_at_fire);
    std::printf("COMFORT composed updebt_at_fire_deg %.3f\n", updebt_at_fire);
    std::printf("COMFORT composed updebt_settle_ticks %d\n",
                updebt_settle_ticks);
    std::printf("COMFORT composed converged_ticks %d\n", converged_ticks);
    std::printf("COMFORT composed nan_events %d\n", nan_events);
    std::printf("COMFORT composed crashed %d\n", crashed);
}

// The `comfort` mode entry point (dispatched from harness_main.cpp).
inline int run_comfort(const sim::AircraftParams& p) {
    const control::ControllerParams cp =
        cfg::load_controller_toml(SEADS_CONFIG_DIR "/controller.toml", p);

    comfort_turnsteady(p, cp);
    comfort_turnsteady_keys(p, cp);  // keyboard-only: the ~16.3 deg deflection view
    comfort_mouseaim_keys(p, cp);  // S-keyprec: mouse-aim AND keys (Chad's style)
    comfort_turnsnap_tap(p, cp);
    comfort_orient(p, cp);
    comfort_turnsnap_ovr(p, cp);
    comfort_freelook_release_keys(p, cp);  // v9: THE flown sequence (nose-ref)
    comfort_freelook_nokeys(p, cp);        // v9: the weld pin (guard-required)
    comfort_vertical("immelmann", "up", 170.0, 220.0, 3500.0, 12.0, p, cp);
    comfort_vertical("splits", "down", 170.0, 150.0, 5000.0, 12.0, p, cp);

    app::LoopState end_st;
    MiniCamera end_cam;
    int loop_nan = 0;
    comfort_loopx3(p, cp, &end_st, &end_cam, &loop_nan);
    comfort_recovery(p, cp, end_st, end_cam);

    comfort_composed(p, cp);  // Q7: the composition (immelmann exit + orient)

    std::printf(
        "\n--- comfort summary ---\n"
        "standing turn establishes a large front-oblique camera; vertical\n"
        "maneuvers accumulate camera-up debt. See COMFORT lines above for the\n"
        "per-scenario M1 (convergence) / M2 (up-debt) / M3 (nan+crash) "
        "numbers.\n");
    return 0;
}

}  // namespace harness
