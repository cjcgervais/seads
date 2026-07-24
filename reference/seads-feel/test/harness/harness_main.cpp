// SEADS harness (HARNESS §4): the wind tunnel over the pure plant — CSV
// telemetry, the step injector, and the golden recorder. Grows controller
// columns and scripted AimIntent sequences in Section 4.
//
// Usage:
//   seads_harness fly [ticks] [out.csv]       raw open-loop flight -> telemetry
//   seads_harness ctrl_fly [ticks] [out.csv]  closed-loop AT-12 max-rate turn
//                                             -> controller CSV (energy instr.)
//   seads_harness step [axis] [deg] [V] [ticks] [out.csv]
//                                             closed-loop step response: prints
//                                             settle/overshoot/reversals (§7
//                                             step 1-2 tuning; numbers, not
//                                             vibes)
//   seads_harness loop [V] [pitch_deg] [lateral_deg] [ticks]
//                                             CONTROLLER over-the-top loop (aim
//                                             SET directly): does err drive to
//                                             0 or corkscrew (§7 Item 1)
//   seads_harness mouseloop [up|down] [deflect_deg] [V] [ticks]
//                                             MOUSE-only loop/split-S through
//                                             the shipped app::tick pipeline:
//                                             proves a vertical mouse
//                                             deflection carries the aim over
//                                             the top & the plane follows
//                                             through (§7 mouse loop fix)
//   seads_harness latflick [V] [offset] [p1] [p2] [down_deg]
//                                             LATERAL turn-reversal (S7 Item
//                                             2): right turn then flick the aim
//                                             hard LEFT (optionally down_deg
//                                             below the horizon) -> prints
//                                             roll/pitch/yaw + push_mode. A
//                                             lateral OR down-and- lateral
//                                             flick must ROLL to the aim; only
//                                             a near-straight-down aim
//                                             pure-pitches (no rollover).
//   seads_harness lathold [V] [offset_deg] [ticks]
//                                             RUNG-C hold-the-line: a WORLD-
//                                             FIXED aim offset_deg LEFT of the
//                                             heading at the horizon, HELD at
//                                             full throttle -> per-0.5 s rows +
//                                             a LATHOLD summary (sag/alt-loss/
//                                             windmill). NOT a gate.
//   seads_harness track [axis] [pattern] [rate_dps|freq_hz] [V] [ticks]
//                       [gain_override] [carry_override] [amp_deg]
//                       (pattern = sweep|sine|reversal|nudge; amp_deg is the
//                        sine/nudge amplitude, default 10 sine / 1 nudge)
//                                             MOVING-AIM tracking instrument
//                                             (v4 rung 1, S-aimff): the aim is
//                                             ROTATED per tick (a genuinely
//                                             moving aim, not a set-jump) and
//                                             the machine-greppable TRACK line
//                                             reports lag/err/flips (sweep),
//                                             phase/gain (sine), or the
//                                             reversal transient. axis = pitch
//                                             | yaw; pattern = sweep | sine |
//                                             reversal. The 3rd arg is deg/s
//                                             for sweep/reversal, Hz for sine.
//                                             gain_override (A/B ONLY, never a
//                                             tune path): overrides the loaded
//                                             aim_ff gain for baseline rows.
//   seads_harness alpha                       AT-18a table: measured vs derived
//                                             peak ang accel, per axis, 2
//                                             speeds
//   seads_harness golden                      print test/golden/golden_flight.h
//                                             checkpoint values for
//                                             re-recording
//   seads_harness comfort                     COMFORT INSTRUMENT: scripts the
//                                             disorientation maneuvers (turn +
//                                             freelook tap, immelmann, split-S,
//                                             loop x3, recovery) through the
//                                             shipped app::tick + MiniCamera
//                                             and prints machine-greppable
//                                             `COMFORT <scenario> <metric>
//                                             <value>` lines (M1 convergence /
//                                             M2 up-debt / M3 nan+crash). See
//                                             test/harness/comfort.h.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string>
#include <vector>

#include "app/instructor_tick.h"
#include "config/load_aircraft.h"
#include "config/load_controller.h"
#include "control/controller.h"
#include "sim/aero.h"
#include "sim/state.h"
#include "sim/step.h"
#include "sim/world.h"
#include "test/harness/comfort.h"
#include "test/harness/injector.h"
#include "test/harness/instructor.h"
#include "test/harness/scenarios.h"
#include "test/harness/telemetry.h"

namespace {

int run_fly(const sim::AircraftParams& p, int ticks,
            const std::string& out_path) {
    if (ticks <= 0) {
        std::fprintf(stderr, "bad tick count\n");
        return 2;
    }
    harness::CsvTelemetry csv(out_path);
    if (!csv.ok()) {
        std::fprintf(stderr, "cannot open %s\n", out_path.c_str());
        return 2;
    }

    sim::SimState s = harness::golden_start(p);
    csv.write(0.0, s, p);
    for (int i = 1; i <= ticks; ++i) {
        s = sim::step(s, harness::golden_input(i - 1), p, p.sim_dt);
        csv.write(i * p.sim_dt, s, p);
        if (sim::altitude(s.position, p) <= 0.0) {
            std::fprintf(stderr, "CRASH at tick %d (altitude <= 0)\n", i);
            return 1;
        }
    }
    std::printf(
        "wrote %d ticks to %s (final altitude %.1f m, speed %.1f m/s)\n", ticks,
        out_path.c_str(), sim::altitude(s.position, p),
        glm::length(s.velocity));
    return 0;
}

int run_alpha(const sim::AircraftParams& p) {
    // AT-18a, human-readable: the ctest version lives in test_at18a.cpp.
    const struct {
        harness::Axis axis;
        const char* name;
        double c, I;
    } axes[] = {
        {harness::Axis::pitch, "pitch", p.c_pitch, p.I_pitch},
        {harness::Axis::yaw, "yaw", p.c_yaw, p.I_yaw},
        {harness::Axis::roll, "roll", p.c_roll, p.I_roll},
    };
    const double crossover = std::sqrt(2.0 * p.q_att_floor / p.rho);
    std::printf("floor crossover: %.2f m/s (q_att_floor %.1f Pa)\n", crossover,
                p.q_att_floor);
    std::printf("%-6s %8s %14s %14s %10s\n", "axis", "V m/s", "measured",
                "derived", "rel err");
    for (const auto& ax : axes) {
        for (double V : {15.0, 150.0}) {
            const double measured =
                harness::measure_ang_accel_max(p, V, ax.axis, 2000.0);
            const double derived =
                sim::ang_accel_max_derived(ax.c, ax.I, V, 2000.0, p);
            std::printf("%-6s %8.1f %14.9f %14.9f %10.2e\n", ax.name, V,
                        measured, derived,
                        std::abs(measured - derived) / derived);
        }
    }
    return 0;
}

int run_golden(const sim::AircraftParams& p) {
    std::printf("// paste into test/golden/golden_flight.h kFlight[]\n");
    sim::SimState s = harness::golden_start(p);
    for (int tick = 1; tick <= harness::kGoldenTicks; ++tick) {
        s = sim::step(s, harness::golden_input(tick - 1), p, p.sim_dt);
        if (tick % harness::kGoldenCheckpointEvery != 0) continue;
        std::printf("    {%d,\n", tick);
        std::printf("     %.17g, %.17g, %.17g,\n", s.position.x, s.position.y,
                    s.position.z);
        std::printf("     %.17g, %.17g, %.17g,\n", s.velocity.x, s.velocity.y,
                    s.velocity.z);
        std::printf("     %.17g, %.17g, %.17g, %.17g,\n", s.orientation.w,
                    s.orientation.x, s.orientation.y, s.orientation.z);
        std::printf("     %.17g, %.17g, %.17g},\n", s.angular_vel.x,
                    s.angular_vel.y, s.angular_vel.z);
    }
    return 0;
}

// The controller golden recorder (HARNESS §5): closed-loop instructor flight,
// checkpoint values for test/golden/controller_golden.h. Re-record ONLY when a
// cascade/config change is confirmed intentional (a moved golden HALTS).
int run_ctrl_golden(const sim::AircraftParams& p) {
    const control::ControllerParams cp =
        cfg::load_controller_toml(SEADS_CONFIG_DIR "/controller.toml", p);
    double thr = 0.0;
    const sim::SimState s0 = harness::ctrl_golden_start(p, &thr);
    harness::ClosedLoop cl(s0, glm::dvec3{0.0, 0.0, -1.0});
    cl.aim_nose();

    std::printf("// paste into test/golden/controller_golden.h kFlight[]\n");
    for (int tick = 1; tick <= harness::kCtrlGoldenTicks; ++tick) {
        const control::Telemetry t =
            harness::ctrl_golden_step(cl, tick, thr, p, cp);
        if (tick % harness::kCtrlGoldenCheckpointEvery != 0) continue;
        const sim::SimState& s = cl.state;
        const int regime = t.regime == control::Regime::FINE ? 0 : 1;
        std::printf("    {%d,\n", tick);
        std::printf("     %.17g, %.17g, %.17g,\n", s.position.x, s.position.y,
                    s.position.z);
        std::printf("     %.17g, %.17g, %.17g,\n", s.velocity.x, s.velocity.y,
                    s.velocity.z);
        std::printf("     %.17g, %.17g, %.17g, %.17g,\n", s.orientation.w,
                    s.orientation.x, s.orientation.y, s.orientation.z);
        std::printf("     %.17g, %.17g, %.17g,\n", s.angular_vel.x,
                    s.angular_vel.y, s.angular_vel.z);
        std::printf("     %.17g, %.17g, %.17g, %.17g, %d},\n",
                    cl.internal.integ.x, cl.internal.integ.y,
                    cl.internal.integ.z, cl.internal.held_bank, regime);
    }
    std::printf("// final: alt %.2f m, speed %.3f m/s\n",
                sim::altitude(cl.state.position, p),
                glm::length(cl.state.velocity));
    return 0;
}

// ctrl_fly (HARNESS §4 / SPEC §16): the controller CSV energy instrument.
// A closed-loop sustained max-rate turn (the AT-12 maneuver, shared driver)
// written per-tick with the full controller columns — true load factor beside
// the flat cosΦθ G-proxy (CQ1). Read alongside the flight log in Section-7
// tuning of the drag knob; NOT a gate (the numeric retention gate is AT-12 in
// test_acceptance.cpp, which flies this same maneuver).
int run_ctrl_fly(const sim::AircraftParams& p, int ticks,
                 const std::string& out_path) {
    if (ticks <= 0) {
        std::fprintf(stderr, "bad tick count\n");
        return 2;
    }
    const control::ControllerParams cp =
        cfg::load_controller_toml(SEADS_CONFIG_DIR "/controller.toml", p);
    harness::CtrlCsvTelemetry csv(out_path);
    if (!csv.ok()) {
        std::fprintf(stderr, "cannot open %s\n", out_path.c_str());
        return 2;
    }

    harness::ClosedLoop cl(harness::at12_start(p), glm::dvec3{0.0, 0.0, -1.0});
    cl.aim_nose();
    // GROUNDED spawn tick: captures held_bank at the level bank so no spurious
    // leveling roll fires (SPEC §9.5), matching the acceptance legs.
    control::Telemetry t = cl.tick(harness::kAt12Throttle, p, cp,
                                   /*grounded=*/true);
    csv.write(0.0, cl.state, cl.last_inputs, cl.internal.integ, t, p);

    const double V0 = glm::length(cl.state.velocity);
    for (int i = 1; i <= ticks; ++i) {
        harness::at12_reaim(cl);
        t = cl.tick(harness::kAt12Throttle, p, cp);
        csv.write(i * p.sim_dt, cl.state, cl.last_inputs, cl.internal.integ, t,
                  p);
        if (sim::altitude(cl.state.position, p) <= 0.0) {
            std::fprintf(stderr, "CRASH at tick %d (altitude <= 0)\n", i);
            return 1;
        }
    }
    const double Vf = glm::length(cl.state.velocity);
    std::printf(
        "wrote %d ctrl ticks to %s | speed %.1f -> %.1f m/s (retention %.1f%%) "
        "| final alt %.0f m\n",
        ticks, out_path.c_str(), V0, Vf, 100.0 * Vf / V0,
        sim::altitude(cl.state.position, p));
    return 0;
}

// ---- step: closed-loop step-response instrument (HARNESS §4/§8) -----------
// The Section-7 tuning tool for steps 1-2 (inner loop K_w, then outer K_theta):
// drive a pointing/bank step at cruise through the SAME ClosedLoop the tests
// replay (never a flat stub, HARNESS §3), then print the three numbers the
// mantra turns on — settle time, overshoot, reversals — computed on the TOTAL
// pointing error t.e (the SAME signal AT-2 grades, honest on every axis), plus
// the rate-tracking for the INNER loop. Read alongside the flight log
// (HARNESS §8); NEVER a gate (AT-2 in test_cascade.cpp owns the pass/fail).
//
// Axis selects only the maneuver + which body rate the INNER block reads (be
// honest about coupling — there is no isolated single-axis maneuver on an
// aircraft):
//   pitch : pitch-up pointing step (the AT-2 scenario). The clean primary axis.
//           A LARGE step saturates omega_des to the clamp -> the early rate
//           response IS the inner-loop step (K_w_pitch); a SMALL step is
//           outer-loop dominated (K_theta). Vary the deg to isolate.
//   roll  : lateral aim step -> bank-to-turn (MANEUVER). Reads the roll rate
//           (K_w_roll, K_phi); the OUTER t.e is a clean capture, but a big
//           lateral step is a sustained TURN -> read reversals, not settle
//           time.
//   yaw   : small lateral pointing step in FINE -> the coordinated yaw pointing
//           (K_w_yaw, yaw_scale). Coupled by nature; rudder alone can't null
//           the pointing error, so expect a residual steady t.e (honest, not a
//           bug).
// INNER rate overshoot within LOADABLE tables is driven by K_wi (the integral),
// NOT K_w: raising K_w only adds damping and the ZOH ceiling (K_w*dt/I<=0.5) is
// reached before any P-overshoot appears (measured: K_w at ceiling -> 0.0%;
// K_wi_pitch 2e6 -> 13.6%). The flight log's step-1 guidance names K_wi so.
// ⚠ SUPERSEDED on a damp_ff'd axis (S-dampff, SPEC §0): with the damping
// feedforward the integrator idles (~0) and the rate overshoot is the
// ff-forced response (measured ~10-17% at damp_ff_pitch=1, falling with V),
// not a K_wi artifact — read K_wi as model-error trim there, not a dial.
constexpr double kPi_step = 3.14159265358979323846;

int run_step(const sim::AircraftParams& p, const std::string& axis_name,
             double step_deg, double V, int ticks, const std::string& csv_path,
             double altitude = 3000.0, double carry_override = -1.0) {
    int axis = axis_name == "pitch"  ? 0
               : axis_name == "yaw"  ? 1
               : axis_name == "roll" ? 2
                                     : -1;
    if (axis < 0) {
        std::fprintf(stderr, "bad axis '%s' (pitch | yaw | roll)\n",
                     axis_name.c_str());
        return 2;
    }
    if (ticks <= 0 || V <= 0.0) {
        std::fprintf(stderr, "bad ticks/V\n");
        return 2;
    }
    control::ControllerParams cp =
        cfg::load_controller_toml(SEADS_CONFIG_DIR "/controller.toml", p);
    // carry_override is the A/B baseline arg ONLY (0 = the structural
    // legacy baseline; never a tune path — the toml is the single source of
    // the shipped value; the track gain_override precedent). Negative (the
    // default) = fly the loaded table.
    if (carry_override >= 0.0) cp.capture_carry = carry_override;

    const double K_w[3] = {cp.K_w_pitch, cp.K_w_yaw, cp.K_w_roll};
    const double I[3] = {p.I_pitch, p.I_yaw, p.I_roll};
    const double K_out[3] = {cp.K_theta, cp.K_theta, cp.K_phi};
    const char* out_name[3] = {"K_theta", "K_theta", "K_phi"};
    const double step_rad = step_deg * kPi_step / 180.0;
    const double dt = p.sim_dt;
    constexpr double settle_tol = 0.5 * kPi_step / 180.0;  // AT-2: 0.5 deg

    // Level trim, then 1 s to settle the integrator (matches the AT-2 fixture).
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    double thr = 0.0;
    const sim::SimState s0 =
        harness::level_trim_state(p, V, altitude, up, heading, &thr);
    harness::ClosedLoop cl(s0, glm::dvec3{0.0, 0.0, -1.0});
    cl.aim_nose();
    cl.tick(thr, p, cp, /*grounded=*/true);
    for (int i = 0; i < 120; ++i) cl.tick(thr, p, cp);

    // Apply the step: rotate the aim about body_right (pitch) or body_up (yaw/
    // roll lateral). Sign is arbitrary — the metrics are sign-consistent.
    const glm::dvec3 body_ax =
        axis == 0 ? cl.state.orientation * glm::dvec3{1.0, 0.0, 0.0}
                  : cl.state.orientation * glm::dvec3{0.0, 1.0, 0.0};
    cl.aim = glm::normalize(glm::angleAxis(step_rad, body_ax) * cl.aim);

    harness::CsvTelemetry* csv = nullptr;
    harness::CsvTelemetry csv_store(csv_path);
    if (!csv_path.empty()) {
        if (!csv_store.ok()) {
            std::fprintf(stderr, "cannot open %s\n", csv_path.c_str());
            return 2;
        }
        csv = &csv_store;
    }

    // OUTER metrics run on the TOTAL pointing error t.e (the aim-cone angle) —
    // the SAME signal AT-2 grades (test_cascade.cpp), for EVERY axis. A signed
    // single-axis error is degenerate off the pole for roll (bank_error is pure
    // direction noise once the nose captures, sweeping +/-180 deg) — it would
    // fabricate a "did not settle" on a clean bank-to-turn capture. t.e is
    // frame-honest on all three axes; overshoot is the error's rebound above
    // its best approach so far (0 for a monotone settle, the past-target
    // excursion when the nose crosses the aim and returns).
    const int win = std::max(1, static_cast<int>(std::lround(0.5 / dt)));
    double peak_err = 0.0, steady_sum = 0.0;
    double peak_rate = 0.0, peak_des = 0.0;
    int steady_n = 0, i_min = 0;
    double e_min = 1e18;
    int dz_tail = 0, dz_total = 0;
    // Park DIRECTION decomposition (tail means): where does the aim sit in the
    // body frame at the park (tb.x = aim right of nose, tb.y = aim above), and
    // what rides along (residual sideslip beta, bank phi)? Attribution for a
    // "parks off to a corner" report — total e alone cannot see the direction.
    double park_lat = 0.0, park_vert = 0.0, park_beta = 0.0, park_phi = 0.0;
    // Instrument hygiene (B's requirement): the full-throttle closed-loop rig
    // CLIMBS and DECELERATES over the window, so the entry-V label is NOT the
    // V the park is measured at. Report the tail-mean ACTUAL speed and altitude
    // so the geometric ff-deficit law (standoff ~ V_actual*h/(R*(R+h)*K_theta))
    // is read against the right V and h, not the entry label.
    double tail_V = 0.0, tail_alt = 0.0;
    std::vector<double> e_trace(ticks), w_trace(ticks), wd_trace(ticks);
    // S-rimshot (v4 rung 2) instrument traces: s_trace = the SIGNED error
    // component along the stepped pointing axis (demand.x for pitch, .y for
    // yaw — the sign the crossing/rim readout turns on; total e is unsigned
    // and cannot see the far side). branch_trace = WHICH seek-branch owns the
    // pointed demand this tick (0 = linear/parabolic taper, 1 = braking
    // sqrt, 2 = the rate ceiling), recomputed from the loaded table + the
    // live state — the attribution readout the rung-2 spec orders FIRST.
    // cap_trace = the event machine's telemetry state.
    std::vector<double> s_trace(ticks);
    std::vector<int> branch_trace(ticks, -1);
    std::vector<int> cap_trace(ticks, 0);
    for (int i = 0; i < ticks; ++i) {
        // The set-jump above IS the hand's flick: report it through the
        // aim_moved seam for exactly one tick (the app's apply_mouse site
        // would have done the same on a real flick). S-rimshot v2: the
        // UNIVERSAL machine never reads aim_moved — this seam now feeds
        // only the deadzone rest_dwell (the parked hand re-accrues the
        // latch dwell over the approach exactly like the app would).
        cl.aim_moved = (i == 0);
        // Signed error along the step axis, pre-tick (the aim is transported
        // inside tick; one tick of transport ~8e-5 rad — instrument-grade).
        {
            const glm::dvec3 dem = control::rotation_demand_body(
                cl.state.orientation, glm::normalize(cl.aim));
            s_trace[i] = (axis == 1) ? dem.y : dem.x;
        }
        const control::Telemetry t = cl.tick(thr, p, cp);
        cap_trace[i] = static_cast<int>(t.capture);
        if (csv) csv->write((i + 1) * dt, cl.state, p);
        const double e = t.e;  // total pointing error [rad]
        e_trace[i] = e;
        w_trace[i] = cl.state.angular_vel[axis];
        wd_trace[i] = t.omega_des[axis];
        if (t.deadzoned) {  // the park readout (S-yaw-magnet crossing/latch)
            ++dz_total;
            if (i >= ticks - win) ++dz_tail;
        }

        // Seek-branch classification for the ATTRIB readout (pitch/yaw
        // pointing axes; roll's bank law is out of the rimshot's scope). The
        // three candidate demands are recomputed EXACTLY as seek_law/sqrt_law
        // compose them from the loaded table + this tick's state — argmin =
        // the binding branch. (The AoA clamp almost never binds at these
        // fixtures; the G ceiling IS branch 2 for pitch.) COUPLED RE-DERIVED
        // COPY of controller.cpp's seek_law/sqrt_law composition
        // (instrument-scope H1, red-team P2-3): a branch/shape change there
        // silently misattributes here — the cross-link comment at seek_law
        // points back. PLANAR-ONLY caveat on the YAW output: the controller's
        // yaw pointing is scaled by ((1-blend) + blend*yaw_gate) (the bank
        // blend / alignment gate), which this classifier omits — valid for
        // the harness's planar wings-level yaw steps (blend ~ 0), a
        // misattribution risk on any banked/off-plane fixture.
        if (axis != 2) {
            const double V_now = glm::length(cl.state.velocity);
            const double alt_now = glm::length(cl.state.position) - p.R;
            double seek, brake, ceil_w;
            // The AoA pushback is a 4th candidate on pitch (the protection
            // clamp K_aoa*(aoa_max - aoa_filt) — it BINDS through the
            // mid-approach of a hard pull as V bleeds; classifying it as
            // "ceil" would misattribute the plateau).
            double aoa_cap = 1e18;
            if (axis == 0) {
                seek = std::max(cp.pursuit_step,
                                cp.K_theta * e * (1.0 + cp.pursuit_expo * e));
                brake = std::sqrt(2.0 * cp.k_b *
                                  sim::ang_accel_max_derived(
                                      p.c_pitch, p.I_pitch, V_now, alt_now, p) *
                                  e);
                ceil_w = (cp.n_max - t.extracted.cos_phi_theta) * p.g /
                         std::max(V_now, cp.v_min);
                aoa_cap = cp.K_aoa * (cp.aoa_max - t.aoa_filtered);
            } else {
                seek = cp.K_theta * cp.yaw_scale * e;
                brake = std::sqrt(2.0 * cp.k_b *
                                  sim::ang_accel_max_derived(
                                      p.c_yaw, p.I_yaw, V_now, alt_now, p) *
                                  e);
                ceil_w = cp.yaw_max;
            }
            const double m = std::min({seek, brake, ceil_w, aoa_cap});
            branch_trace[i] = (m == seek)     ? 0
                              : (m == brake)  ? 1
                              : (m == ceil_w) ? 2
                                              : 3;
        }

        peak_err = std::max(peak_err, e);
        if (e < e_min) {
            e_min = e;
            i_min = i;  // closest approach (the transient's end reference)
        }
        peak_rate = std::max(peak_rate, std::abs(w_trace[i]));
        peak_des = std::max(peak_des, std::abs(wd_trace[i]));
        if (i >= ticks - win) {  // last 0.5 s -> the steady/late-drift readout
            steady_sum += e;
            ++steady_n;
            const glm::dvec3 tb =
                sim::body_dir_of(cl.state.orientation, glm::normalize(cl.aim));
            park_lat += std::asin(std::clamp(tb.x, -1.0, 1.0));
            park_vert += std::asin(std::clamp(tb.y, -1.0, 1.0));
            park_beta += t.extracted.beta;
            park_phi += t.extracted.phi;
            tail_V += glm::length(cl.state.velocity);
            tail_alt += glm::length(cl.state.position) - p.R;
        }
    }
    // Overshoot, reversals and settle are TRANSIENT properties — bound them to
    // the closest approach plus one ring-down window (`transient_end`), NOT the
    // whole run. Otherwise slow post-capture world-aim drift (the AT-4/5 steady
    // channel) re-inflates `e` above its best approach and gets misread as
    // "overshoot" / "limit cycle" — and the number would then grow with the
    // (CLI-chosen) tick count. `e_min` is the capture; drift after it belongs
    // to the steady-error line, not here. (Fable re-attack N1/N2.)
    const int transient_end = std::min(ticks, i_min + win + 1);
    double overshoot = 0.0, run_min = 1e18;
    int reversals = 0, settle_tick = -1;
    double prev_e = 0.0, prev_de = 0.0;
    for (int i = 0; i < transient_end; ++i) {
        const double e = e_trace[i];
        overshoot =
            std::max(overshoot, e - run_min);  // rebound past best approach
        run_min = std::min(run_min, e);
        if (e > settle_tol)
            settle_tick = i;  // last out-of-band tick (transient)
        const double de = e - prev_e;
        if (i >= win && de * prev_de < 0.0)
            ++reversals;  // post-0.5 s slope flip
        prev_de = de;
        prev_e = e;
    }
    // Inner-loop rate overshoot: how far the achieved rate exceeds the demand
    // ONLY while the demand is near its saturated plateau (|wd| >= 0.5*peak),
    // and ONLY within the transient (like the OUTER metrics) — else the late
    // drift-correction rates read as inner-loop overshoot and the number grows
    // with the window. Restricting to the held phase also excludes the honest
    // coast-down where the demand relaxes to 0 but momentum carries |w| past it
    // (capture, not inner-loop overshoot) — the artifact a naive |w|>|wd| test
    // reports.
    double rate_over = 0.0;
    for (int i = 0; i < transient_end; ++i) {
        if (std::abs(wd_trace[i]) < 0.5 * peak_des) continue;
        if (w_trace[i] * wd_trace[i] > 0.0 &&
            std::abs(w_trace[i]) > std::abs(wd_trace[i]))
            rate_over = std::max(rate_over,
                                 std::abs(w_trace[i]) - std::abs(wd_trace[i]));
    }

    const double deg = 180.0 / kPi_step;
    // settle_tick = last out-of-band tick within the transient (-1 = never left
    // the band -> settled at 0.0 s). Not settled only if still out of band at
    // the transient's end (a maneuver that never captures, e.g. a sustained
    // roll turn whose residual t.e stays above the band).
    const bool settled = settle_tick < transient_end - 1;
    std::printf(
        "step: %s %+.1f deg at V=%.1f m/s | %d ticks (%.2f s), dt=%gs\n",
        axis_name.c_str(), step_deg, V, ticks, ticks * dt, dt);
    std::printf(
        "  gains: K_w_%s=%.0f  %s=%.2f  I=%.0f | ZOH K_w*dt/I=%.3f (ceil 0.50) "
        "|"
        " crit K_w/(4*I*%s)=%.2f (>=1)\n",
        axis_name.c_str(), K_w[axis], out_name[axis], K_out[axis], I[axis],
        K_w[axis] * dt / I[axis], out_name[axis],
        K_w[axis] / (4.0 * I[axis] * K_out[axis]));
    std::printf("  OUTER (total pointing error t.e, AT-2 signal):\n");
    std::printf("    peak error   : %6.2f deg  (step = %.1f)\n", peak_err * deg,
                step_deg);
    if (settled)
        std::printf(
            "    settle time  : %6.3f s   (|e| < 0.50 deg thereafter)\n",
            (settle_tick + 1) * dt);
    else
        std::printf(
            "    settle time  :   >%.2f s  (did NOT settle in window)\n",
            ticks * dt);
    std::printf(
        "    overshoot    : %6.2f deg  (error rebound past best approach)\n",
        std::max(0.0, overshoot) * deg);
    std::printf("    reversals    : %6d      (post-0.5s slope flips)\n",
                reversals);
    std::printf("    steady error : %6.3f deg  (mean |e|, last 0.5 s)\n",
                steady_n ? steady_sum / steady_n * deg : 0.0);
    // PARK: did the capture actually CROSS into the deadzone circle and latch
    // (the S-yaw-magnet "found center" claim), or does it hang outside (the
    // pointing-vs-coordination standoff)? e_min < lo == crossed by design.
    std::printf(
        "  PARK (deadzone circle lo/hi = %.2f/%.2f deg):\n"
        "    closest approach: %6.4f deg at %.2f s  (%s lo)\n"
        "    deadzone latched : %6d ticks in last 0.5 s (%d total)\n"
        "    park direction   : aim %+.4f deg lateral (+ = right of nose), "
        "%+.4f deg vertical (+ = above)\n"
        "    park rides       : beta %+.3f deg (residual crab), phi %+.3f deg "
        "(bank)\n"
        "    tail actual      : V %.1f m/s (entry %.1f), alt %.0f m (entry "
        "%.0f) | latched %d of last %d ticks\n",
        cp.deadzone_lo * deg, cp.deadzone_hi * deg, e_min * deg,
        (i_min + 1) * dt, e_min < cp.deadzone_lo ? "CROSSED" : "hangs OUTSIDE",
        dz_tail, dz_total, steady_n ? park_lat / steady_n * deg : 0.0,
        steady_n ? park_vert / steady_n * deg : 0.0,
        steady_n ? park_beta / steady_n * deg : 0.0,
        steady_n ? park_phi / steady_n * deg : 0.0,
        steady_n ? tail_V / steady_n : 0.0, V,
        steady_n ? tail_alt / steady_n : 0.0, altitude, dz_tail, steady_n);
    std::printf("  INNER (body rate, axis %s):\n", axis_name.c_str());
    std::printf("    peak |omega| : %6.2f deg/s\n", peak_rate * deg);
    std::printf(
        "    peak |des|   : %6.2f deg/s  (demand; saturates -> clamp)\n",
        peak_des * deg);
    std::printf("    rate overshoot: %5.2f deg/s (%.1f%% of demand)\n",
                rate_over * deg,
                peak_des > 0.0 ? 100.0 * rate_over / peak_des : 0.0);

    // ---- S-rimshot (v4 rung 2): ATTRIB + CAPTURE readouts ----------------
    // ATTRIB: which seek branch shapes the approach (the rung-2 spec's STEP 1
    // — mechanism design follows these numbers, never the other way). Prints
    // the branch handoff radii and the rate profile over the last 2 deg of
    // the approach. CAPTURE: the circle-scaled event readout — no-slow-in
    // ratio, far-rim excursion in rim units, reversal count, return time,
    // terminal drift. Machine-greppable lines. Pointing axes only.
    if (axis != 2 && std::abs(s_trace[0]) > 1e-6) {
        const double sign0 = s_trace[0] >= 0.0 ? 1.0 : -1.0;
        const double circle = cp.capture_circle;
        // Crossing: first tick the signed error goes past the aim.
        int cross = -1;
        for (int i = 0; i < ticks; ++i)
            if (s_trace[i] * sign0 <= 0.0) {
                cross = i;
                break;
            }
        const int approach_end = cross >= 0 ? cross : ticks;
        // Branch handoffs over the approach.
        const char* bname[4] = {"taper", "brake", "ceil", "aoa"};
        std::printf("  ATTRIB (approach branch handoffs, %s axis):\n",
                    axis_name.c_str());
        for (int i = 1; i < approach_end; ++i) {
            if (branch_trace[i] != branch_trace[i - 1] &&
                branch_trace[i] >= 0 && branch_trace[i - 1] >= 0) {
                std::printf(
                    "    ATTRIB handoff %s->%s at e=%.3f deg (t=%.2f s, "
                    "|w|=%.1f deg/s)\n",
                    bname[branch_trace[i - 1]], bname[branch_trace[i]],
                    e_trace[i] * deg, (i + 1) * dt, std::abs(w_trace[i]) * deg);
            }
        }
        // Rate profile over the last 2 deg of approach.
        const double thresholds[] = {2.0, 1.5, 1.0, 0.75, 0.5,
                                     0.4, 0.3, 0.2, 0.1,  0.05};
        std::printf(
            "    ATTRIB last-2deg profile (e, |w|, |w_des|, branch):\n");
        for (const double thr_deg : thresholds) {
            for (int i = 0; i < approach_end; ++i) {
                if (e_trace[i] <= thr_deg / deg) {
                    std::printf(
                        "      e<=%.2f: e=%.3f deg |w|=%.2f deg/s "
                        "|wd|=%.2f deg/s %s\n",
                        thr_deg, e_trace[i] * deg, std::abs(w_trace[i]) * deg,
                        std::abs(wd_trace[i]) * deg,
                        branch_trace[i] >= 0 ? bname[branch_trace[i]] : "?");
                    break;
                }
            }
        }
        // CAPTURE metrics. capture point = e first inside the aim circle
        // (pre-crossing); peak approach rate over the whole approach.
        int cap_tick = -1;
        for (int i = 0; i < approach_end; ++i)
            if (e_trace[i] < circle) {
                cap_tick = i;
                break;
            }
        double w_peak = 0.0;
        for (int i = 0; i < approach_end; ++i)
            w_peak = std::max(w_peak, std::abs(w_trace[i]));
        const double w_cap = cap_tick >= 0 ? std::abs(w_trace[cap_tick]) : 0.0;
        // Far-side excursion (rim units) + apex + recross.
        double far_peak = 0.0;
        int apex = -1, recross = -1;
        if (cross >= 0) {
            for (int i = cross; i < ticks; ++i) {
                const double f = -s_trace[i] * sign0;
                if (f > far_peak) {
                    far_peak = f;
                    apex = i;
                }
                if (f < 0.0) break;  // recrossed before growing further
            }
            for (int i = std::max(apex, cross); i < ticks; ++i)
                if (s_trace[i] * sign0 >= 0.0) {
                    recross = i;
                    break;
                }
        }
        // rim_e: the frame-HONEST glance depth — the far excursion of the
        // TOTAL pointing error |d| (what the reticle shows: basis-free,
        // norm-preserving under bank) over the crossing..recross window.
        // The s_trace projection above under-reads any BANKED arrival (a
        // world-lateral flick engages mid-unroll at 60-75 deg bank, so the
        // travel direction is mostly BODY-pitch and the body-yaw projection
        // of a rim-deep glance reads only rim*|u_y| — the same
        // bank-gauge-contamination class as feeding a roll gauge phi).
        // Chad's spec grades the glance "at the point opposite the
        // direction of travel" — rim_e is that read; rim stays printed for
        // the pure-axis rows where the two coincide.
        double rim_e = 0.0;
        if (cross >= 0) {
            const int re_end = recross >= 0 ? recross + 1 : ticks;
            for (int i = cross; i < re_end; ++i)
                rim_e = std::max(rim_e, e_trace[i]);
        }
        // Reversals: sign flips of the CLOSING rate (w along the step axis,
        // toward-the-aim positive) between the step and the recross, outside
        // a 0.5 deg/s deadband (trim noise never counts).
        const int rev_end = recross >= 0 ? recross + 1 : ticks;
        const double band = 0.5 / deg;
        int cap_revs = 0, w_sign = 0;
        for (int i = 0; i < rev_end; ++i) {
            const double wc = w_trace[i] * sign0;
            const int s = wc > band ? 1 : (wc < -band ? -1 : 0);
            if (s != 0) {
                if (w_sign != 0 && s != w_sign) ++cap_revs;
                w_sign = s;
            }
        }
        // Per-tick arrest attribution trace (env SEADS_CAP_TRACE=1): the
        // apex-to-arrival window that attributed the 2.2 drift (demand vs
        // achieved rate through the RETURN + the handback coast).
        if (std::getenv("SEADS_CAP_TRACE") && apex >= 0) {
            for (int i = std::max(0, apex - 4);
                 i < std::min(ticks, (recross >= 0 ? recross : apex) + 40); ++i)
                std::printf(
                    "    TRACE i=%d s=%+.4f e=%.4f w=%+.2f wd=%+.2f cap=%d "
                    "dz=%d\n",
                    i, s_trace[i] * deg, e_trace[i] * deg, w_trace[i] * deg,
                    wd_trace[i] * deg, cap_trace[i], 0);
        }
        // Arrest diagnostics (rimshot-arrest): the body rate AT the recross
        // tick (the dead-stop claim is |w_exit| ~ 0) and the SECOND overshoot
        // — how far the signed error blows back past center on the near side
        // after the recross (the "blows past then creeps back" read the
        // arrest exists to kill).
        const double w_exit = recross >= 0 ? std::abs(w_trace[recross]) : 0.0;
        double over2 = 0.0;
        if (recross >= 0) {
            for (int i = recross; i < ticks; ++i)
                over2 = std::max(over2, s_trace[i] * sign0);
        }
        // Arrival: first tick AFTER the apex the total error is inside the
        // deadzone circle (the felt "landed at center" beat — with the
        // rimshot arrest the recross happens at sub-deg/s deep inside the
        // circle, so return_ms alone overstates the beat).
        int arrive = -1;
        if (apex >= 0) {
            for (int i = apex; i < ticks; ++i)
                if (e_trace[i] < cp.deadzone_lo) {
                    arrive = i;
                    break;
                }
        }
        // Terminal drift: mean |e| over the 0.25 s after the recross (or the
        // trace tail if the event never recrossed).
        const int drift_start = recross >= 0 ? recross : ticks - win / 2;
        const int drift_end =
            std::min(ticks, drift_start + static_cast<int>(0.25 / dt));
        double drift = 0.0;
        int dn = 0;
        for (int i = drift_start; i < drift_end; ++i) {
            drift += e_trace[i];
            ++dn;
        }
        // Event-machine occupancy (0 everywhere at carry = 0 / legacy) +
        // the engage count (S-rimshot v2: exactly 1 per arrival — a higher
        // count on a plain step is the refractory failing, the coast
        // re-engage hunt made visible).
        const int kIdleInt = static_cast<int>(control::CaptureState::IDLE);
        const int kCarryInt = static_cast<int>(control::CaptureState::CARRY);
        const int kReturnInt = static_cast<int>(control::CaptureState::RETURN);
        int carry_ticks = 0, return_ticks = 0, engage_tick = -1, engages = 0;
        int prev_cap = kIdleInt;
        for (int i = 0; i < ticks; ++i) {
            if (cap_trace[i] == kCarryInt) {
                ++carry_ticks;
                if (engage_tick < 0) engage_tick = i;
            } else if (cap_trace[i] == kReturnInt) {
                ++return_ticks;
            }
            if (prev_cap == kIdleInt && cap_trace[i] != kIdleInt) ++engages;
            prev_cap = cap_trace[i];
        }
        std::printf(
            "  CAPTURE V=%.0f step=%.1f carry=%.2f approach_ratio=%.3f "
            "rim=%.2f rim_e=%.2f reversals=%d return_ms=%.0f arrive_ms=%.0f "
            "drift_deg=%.4f w_exit_dps=%.1f over2_deg=%.4f "
            "w_cap_dps=%.1f w_peak_dps=%.1f cross=%s carry_ticks=%d "
            "return_ticks=%d engage_e_deg=%.2f engages=%d\n",
            V, step_deg, cp.capture_carry, w_peak > 0.0 ? w_cap / w_peak : 0.0,
            far_peak / circle, rim_e / circle, cap_revs,
            (apex >= 0 && recross >= 0) ? (recross - apex) * dt * 1000.0 : -1.0,
            (apex >= 0 && arrive >= 0) ? (arrive - apex) * dt * 1000.0 : -1.0,
            dn ? drift / dn * deg : 0.0, w_exit * deg, over2 * deg, w_cap * deg,
            w_peak * deg, cross >= 0 ? "YES" : "NO", carry_ticks, return_ticks,
            engage_tick >= 0 ? e_trace[engage_tick] * deg : -1.0, engages);
    }
    if (csv) std::printf("  wrote per-tick trace to %s\n", csv_path.c_str());
    return 0;
}

// loop: closed-loop CONTROLLER loop instrument (§7 Item 1). SETS a fixed world
// aim "up-and-over" (with a chosen lateral tilt) DIRECTLY and dumps the
// cascade's per-tick decisions — it tests the INSTRUCTOR's over-the-top
// pull-through given the aim, not the mouse->aim mechanics. Watch phi (does the
// near-vertical pull corkscrew — the deferred roll-at-rest issue) and whether
// err drives to ~0. NOT a gate (AT-15 owns the split-S pass/fail); a §7 tuning
// aid. For the MOUSE-only loop/split-S (the aim mechanics), use `mouseloop`.
int run_loop(const sim::AircraftParams& p, double V, double pitch_deg,
             double lateral_deg, int ticks) {
    const control::ControllerParams cp =
        cfg::load_controller_toml(SEADS_CONFIG_DIR "/controller.toml", p);
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    double thr = 0.0;
    const sim::SimState s0 =
        harness::level_trim_state(p, V, 3500.0, up, heading, &thr);
    harness::ClosedLoop cl(s0, glm::dvec3{0.0, 0.0, -1.0});
    cl.aim_nose();
    cl.tick(thr, p, cp, /*grounded=*/true);
    for (int i = 0; i < 120; ++i) cl.tick(thr, p, cp);

    // Set the aim: pitch up by pitch_deg about an axis = body_right tilted
    // lateral_deg about the nose (injects an out-of-plane component).
    const glm::dvec3 nose = cl.state.orientation * glm::dvec3{0.0, 0.0, -1.0};
    const glm::dvec3 right = cl.state.orientation * glm::dvec3{1.0, 0.0, 0.0};
    const glm::dvec3 axis = glm::normalize(
        glm::angleAxis(lateral_deg * kPi_step / 180.0, nose) * right);
    cl.aim = glm::normalize(glm::angleAxis(pitch_deg * kPi_step / 180.0, axis) *
                            nose);

    std::printf(
        "loop: V=%.0f pitch=%.0f lateral=%.0f | tick  err   tb.x   tb.y   "
        "tb.z    phi   cPT  push  w_pitch  w_roll  in_pitch in_roll\n",
        V, pitch_deg, lateral_deg);
    double last_err = 0.0, phi_absmax = 0.0;
    for (int i = 1; i <= ticks; ++i) {
        const control::Telemetry t = cl.tick(thr, p, cp);
        last_err = t.e * 180.0 / kPi_step;
        phi_absmax = std::max(phi_absmax, std::abs(t.extracted.phi));
        const glm::dvec3 tb =
            sim::body_dir_of(cl.state.orientation, glm::normalize(cl.aim));
        if (i % 30 == 0)
            std::printf(
                "      %5d %5.1f %6.2f %6.2f %6.2f %6.1f %5.2f %4d %8.1f %7.1f "
                "%8.3f %7.3f\n",
                i, last_err, tb.x, tb.y, tb.z,
                t.extracted.phi * 180.0 / kPi_step, t.extracted.cos_phi_theta,
                t.push_mode ? 1 : 0, t.omega_des.x * 180.0 / kPi_step,
                t.omega_des.z * 180.0 / kPi_step, (double)cl.last_inputs.pitch,
                (double)cl.last_inputs.roll);
        if (sim::altitude(cl.state.position, p) <= 0.0) {
            std::printf("  CRASH at tick %d\n", i);
            return 0;
        }
    }
    std::printf(
        "  final err %.1f deg (loop %s), peak bank %.0f deg, alt %.0f m\n",
        last_err, last_err < 10.0 ? "COMPLETED" : "did NOT capture",
        phi_absmax * 180.0 / kPi_step, sim::altitude(cl.state.position, p));
    return 0;
}

// mouseloop: MOUSE-DRIVEN loop/split-S through the SHIPPED app pipeline
// (app::tick — aim frame + screen-relative apply_mouse over the real camera
// basis + control::step + sim), the exact code the live game runs. Holds a
// steady vertical mouse (up = loop,
// down = split-S) and reports whether the AIM sweeps over the top AND the plane
// follows through inverted and back — the mouse-only test Chad flies by hand,
// as NUMBERS. This is what proves the fix (the harness `loop` mode SETS the aim
// directly and never exercises the mouse->aim mechanics). NOT a gate.
int run_mouseloop(const sim::AircraftParams& p, const std::string& dir,
                  double deflect_deg, double V, int ticks) {
    const control::ControllerParams cp =
        cfg::load_controller_toml(SEADS_CONFIG_DIR "/controller.toml", p);
    const int sgn = (dir == "down") ? +1 : -1;  // mouse-down dy>0 (pitch down)

    app::LoopState st;
    const glm::dvec3 up0{1.0, 0.0, 0.0}, heading0{0.0, 0.0, -1.0};
    st.curr = harness::level_trim_state(p, V, 3500.0, up0, heading0, nullptr);
    st.prev = st.curr;
    st.prev_up = sim::local_up(st.curr.position);
    st.aim.reseed(st.curr.orientation, st.prev_up);
    st.grounded = true;

    // RAW mouse (S7-raw): app::tick ignores the camera basis, but advance the
    // real MiniCamera anyway so in.cam_fwd/cam_up carry the shipped (vestigial-
    // to-the-mouse) values — the loop/split-S proof is the shipped app::tick
    // path exactly as it flies.
    harness::MiniCamera cam;
    cam.seed(st.curr);

    // DEFLECT-then-HOLD (the realistic "big vertical deflection"): sweep the
    // mouse at ~180 deg/s until the aim has been rotated deflect_deg from the
    // nose, then HOLD (zero mouse) and let the plane fly the maneuver to the
    // parked aim. A forever-sweep just laps the lagging plane — not a loop.
    const double rate_deg_s = 180.0;
    const double dy_tick =
        sgn * (rate_deg_s * kPi_step / 180.0) * p.sim_dt / cp.aim_sensitivity;
    const int push_ticks =
        static_cast<int>(deflect_deg / (rate_deg_s * p.sim_dt));

    const glm::dvec3 nose0 = st.curr.orientation * glm::dvec3{0.0, 0.0, -1.0};
    double thr = 1.0;
    double max_aim_elev = -90.0, min_cpt = 1e9, max_cpt = -1e9, min_err = 1e9;
    bool aim_went_behind = false, plane_inverted = false;
    std::printf(
        "mouseloop %s deflect=%.0f deg V=%.0f (push %d ticks) | tick  aimElev "
        "aimBehind  noseElev  cPT   phi    err\n",
        dir.c_str(), deflect_deg, V, push_ticks);
    for (int i = 1; i <= ticks; ++i) {
        app::TickInput in;
        in.throttle = thr;
        // Sweep the mouse for push_ticks, then hold (zero) — deflect and hold.
        in.aim_dx = 0.0;
        in.aim_dy = (i <= push_ticks) ? dy_tick : 0.0;
        in.cam_fwd = cam.cam_fwd;  // previous tick's basis (one-tick lag)
        in.cam_up = cam.cam_up;
        const app::TickResult r = app::tick(st, in, p, cp);
        cam.advance(st.curr, st.aim.forward(), st.aim.up(), cp, p.sim_dt);

        const glm::dvec3 lu = sim::local_up(st.curr.position);
        const glm::dvec3 nose =
            st.curr.orientation * glm::dvec3{0.0, 0.0, -1.0};
        const glm::dvec3 aim = st.aim.forward();
        const double aim_elev =
            std::asin(std::clamp(glm::dot(aim, lu), -1.0, 1.0)) * 180.0 /
            kPi_step;
        const double nose_elev =
            std::asin(std::clamp(glm::dot(nose, lu), -1.0, 1.0)) * 180.0 /
            kPi_step;
        const double aim_behind = glm::dot(aim, nose0);  // <0 = aim is behind
        max_aim_elev = std::max(max_aim_elev, aim_elev);
        min_cpt = std::min(min_cpt, r.telem.extracted.cos_phi_theta);
        max_cpt = std::max(max_cpt, r.telem.extracted.cos_phi_theta);
        min_err = std::min(min_err, r.telem.e * 180.0 / kPi_step);
        if (aim_behind < -0.3) aim_went_behind = true;
        if (r.telem.extracted.cos_phi_theta < -0.5) plane_inverted = true;
        if (i % 40 == 0)
            std::printf("      %5d  %6.1f   %6.2f    %6.1f %6.2f %6.1f %6.1f\n",
                        i, aim_elev, aim_behind, nose_elev,
                        r.telem.extracted.cos_phi_theta,
                        r.telem.extracted.phi * 180.0 / kPi_step,
                        r.telem.e * 180.0 / kPi_step);
        if (sim::altitude(st.curr.position, p) <= 0.0) {
            std::printf("  CRASH at tick %d\n", i);
            break;
        }
    }
    const glm::dvec3 nose_f = st.curr.orientation * glm::dvec3{0.0, 0.0, -1.0};
    const double captured = glm::dot(nose_f, st.aim.forward());
    std::printf(
        "  SUMMARY: max aim elevation=%.0f deg | aim went behind=%s | plane "
        "inverted=%s (cPT range %.2f..%.2f) | min err=%.0f captured=%.2f\n",
        max_aim_elev, aim_went_behind ? "YES" : "NO",
        plane_inverted ? "YES" : "NO", min_cpt, max_cpt, min_err, captured);
    std::printf("  => %s\n",
                (aim_went_behind && plane_inverted && captured > 0.8)
                    ? "LOOP/SPLIT-S FOLLOWS THROUGH & CAPTURES"
                : aim_went_behind
                    ? "aim went over but PLANE did not fully follow/capture"
                    : "AIM STUCK — cannot go past vertical");
    return 0;
}

// ---- track: the MOVING-AIM tracking instrument (v4 rung 1, S-aimff) -------
// The step instrument's sibling for a CONTINUOUSLY MOVING aim: `step` jumps
// the aim once and grades the capture; `track` rotates the aim per tick (the
// scripted hand) and grades the PURSUIT — tracking lag, oscillation while
// tracking (rim-bounce, trap #10), sine phase/gain, and the instant-reversal
// transient. Drives the SAME ClosedLoop the goldens replay (never a flat
// stub, HARNESS §3), with aim_moved = true and aim_rate_world = the scripted
// rate while sweeping — exactly the signals the app seam feeds the aim-rate
// feedforward. NOT a gate; the v4 ledger reads its TRACK lines.
//
// The rotation axis is a WORLD vector chosen at sweep start (pitch = the
// trimmed state's body_right, yaw = local_up) and parallel-transported each
// tick alongside the aim (the frame-carried discipline — a world-frozen axis
// would tilt off the sweep plane at V*t/R). Sine phase/gain come from a
// quadrature fit of the NOSE's angle about that axis over whole cycles.
int run_track(const sim::AircraftParams& p, const std::string& axis_name,
              const std::string& pattern, double rate_arg, double V, int ticks,
              double gain_override, double carry_override,
              double amp_deg = 10.0) {
    if (axis_name != "pitch" && axis_name != "yaw") {
        std::fprintf(stderr, "bad axis '%s' (pitch | yaw)\n",
                     axis_name.c_str());
        return 2;
    }
    if (pattern != "sweep" && pattern != "sine" && pattern != "reversal" &&
        pattern != "nudge") {
        std::fprintf(stderr,
                     "bad pattern '%s' (sweep | sine | reversal | nudge)\n",
                     pattern.c_str());
        return 2;
    }
    if (ticks <= 0 || V <= 0.0 || rate_arg <= 0.0) {
        std::fprintf(stderr, "bad ticks/V/rate\n");
        return 2;
    }
    control::ControllerParams cp =
        cfg::load_controller_toml(SEADS_CONFIG_DIR "/controller.toml", p);
    // gain_override / carry_override are A/B baseline args ONLY (0 = the
    // structural baseline; never a tune path — the toml is the single
    // source of the shipped values). Negative (the default) = fly the
    // loaded table. carry 0-vs-1 is the S-rimshot-v2 WOBBLE instrument arm
    // (research trap #10: does the universal rebound weave a smooth track?
    // — the cap_* columns below answer with numbers, the stick judges).
    if (gain_override >= 0.0) cp.aim_ff_gain = gain_override;
    if (carry_override >= 0.0) cp.capture_carry = carry_override;
    const double gain_now = cp.aim_ff_gain;
    const double dt = p.sim_dt;
    const double deg = 180.0 / kPi_step;

    // Level trim, GROUNDED spawn tick, then 1 s to settle (the step fixture).
    const glm::dvec3 up0{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    double thr = 0.0;
    const sim::SimState s0 =
        harness::level_trim_state(p, V, 3000.0, up0, heading, &thr);
    harness::ClosedLoop cl(s0, glm::dvec3{0.0, 0.0, -1.0});
    cl.aim_nose();
    cl.tick(thr, p, cp, /*grounded=*/true);
    for (int i = 0; i < 120; ++i) cl.tick(thr, p, cp);

    glm::dvec3 axis = glm::normalize(
        axis_name == "pitch" ? cl.state.orientation * glm::dvec3{1.0, 0.0, 0.0}
                             : sim::local_up(cl.state.position));
    glm::dvec3 base =
        cl.state.orientation * glm::dvec3{0.0, 0.0, -1.0};  // sine reference

    // S-rimshot v2 capture columns (the wobble instrument): event
    // occupancy, engage count, and the max excursion PAST the moving aim in
    // the direction of travel (rim units) — the mid-track rebound made
    // visible. NOTE cap_far conflates rebound excursion with plain
    // reversal-overshoot kinematics (a reversal reads 3-4 rim units in BOTH
    // interpretations) — it is only meaningful AGAINST the carry_override=0
    // twin, never absolutely. Accumulated inside `drive` so every pattern
    // reports them.
    const int kCapIdle = static_cast<int>(control::CaptureState::IDLE);
    int cap_ticks = 0, cap_engages = 0, cap_prev = kCapIdle;
    double cap_far = 0.0;
    // One tick of the scripted hand: transport the axis/base with the SAME up
    // pair cl.tick is about to use, rotate the aim incrementally about the
    // transported axis (dang this tick), and report the scripted rate through
    // the aim_moved/aim_rate_world seam.
    auto drive = [&](double dang, double rate_now) {
        const glm::dvec3 up_now = sim::local_up(cl.state.position);
        axis = control::transport_aim(axis, cl.prev_up, up_now);
        base = control::transport_aim(base, cl.prev_up, up_now);
        if (dang != 0.0)
            cl.aim = glm::normalize(glm::angleAxis(dang, axis) * cl.aim);
        cl.aim_moved = dang != 0.0;
        cl.aim_rate_world = rate_now * axis;
        const control::Telemetry t = cl.tick(thr, p, cp);
        const int ci = static_cast<int>(t.capture);
        if (ci != kCapIdle) ++cap_ticks;
        if (cap_prev == kCapIdle && ci != kCapIdle) ++cap_engages;
        cap_prev = ci;
        if (dang != 0.0) {
            const glm::dvec3 nose =
                cl.state.orientation * glm::dvec3{0.0, 0.0, -1.0};
            const glm::dvec3 np = nose - glm::dot(nose, axis) * axis;
            const glm::dvec3 ap = cl.aim - glm::dot(cl.aim, axis) * axis;
            const double lead = std::atan2(glm::dot(axis, glm::cross(ap, np)),
                                           glm::dot(ap, np));
            const double dir = dang > 0.0 ? 1.0 : -1.0;
            cap_far = std::max(cap_far, lead * dir);
        }
        return t;
    };
    // Signed angle of v about `axis` relative to b (projections; atan2 is
    // scale-invariant so neither needs normalizing).
    auto ang_about = [&](const glm::dvec3& v, const glm::dvec3& b) {
        const glm::dvec3 vp = v - glm::dot(v, axis) * axis;
        const glm::dvec3 bp = b - glm::dot(b, axis) * axis;
        return std::atan2(glm::dot(axis, glm::cross(bp, vp)), glm::dot(bp, vp));
    };
    // World angular velocity of the airframe about the sweep axis [rad/s].
    auto omega_about = [&]() {
        return glm::dot(cl.state.orientation * cl.state.angular_vel, axis);
    };

    if (pattern == "sweep") {
        // Constant-rate rotation; metrics over the steady phase (skip the
        // first 0.5 s of the sweep). lag_ms = mean|e|/rate — the time the
        // nose trails the moving aim. flips = slope-sign changes of e (the
        // oscillation detector; MUST be ~0 for smooth tracking).
        const double rate = rate_arg / deg;
        const int skip = static_cast<int>(std::lround(0.5 / dt));
        double sum_e = 0.0, peak_e = 0.0, prev_e = 0.0, prev_de = 0.0;
        int n = 0, flips = 0;
        for (int i = 0; i < ticks; ++i) {
            const control::Telemetry t = drive(rate * dt, rate);
            if (sim::altitude(cl.state.position, p) <= 0.0) {
                std::fprintf(stderr, "CRASH at sweep tick %d\n", i);
                return 1;
            }
            const double e = t.e;
            if (i >= skip) {
                sum_e += e;
                peak_e = std::max(peak_e, e);
                ++n;
                const double de = e - prev_e;
                // 1e-7 rad deadband: don't count fp noise as a slope flip.
                if (std::abs(de) > 1e-7) {
                    if (prev_de != 0.0 && de * prev_de < 0.0) ++flips;
                    prev_de = de;
                }
            }
            prev_e = e;
        }
        const double mean_e = n ? sum_e / n : 0.0;
        std::printf(
            "TRACK %s sweep rate=%.1f V=%.0f gain=%.3f lag_ms=%.1f "
            "mean_err_deg=%.3f peak_err_deg=%.3f flips=%d carry=%.2f "
            "cap_engages=%d cap_ticks=%d cap_far_rim=%.2f\n",
            axis_name.c_str(), rate_arg, V, gain_now, mean_e / rate * 1000.0,
            mean_e * deg, peak_e * deg, flips, cp.capture_carry, cap_engages,
            cap_ticks, cap_far / cp.capture_circle);
        return 0;
    }

    if (pattern == "sine") {
        // Sinusoidal aim, amplitude amp_deg (default 10), freq rate_arg [Hz].
        // Phase lag and
        // gain (nose amplitude / aim amplitude) by quadrature fit of the nose
        // angle about the axis over the last WHOLE cycles (1-cycle transient
        // skipped): y ~ G*A*sin(w t - phi) => Sum y*sin = GAn/2 cos(phi),
        // Sum y*cos = -GAn/2 sin(phi).
        const double f = rate_arg;
        const double A = amp_deg / deg;
        const double w = 2.0 * kPi_step * f;
        const double T = 1.0 / f;
        const int t_skip = static_cast<int>(std::lround(T / dt));
        const int cycles =
            static_cast<int>(std::floor((ticks - t_skip) * dt / T));
        if (cycles < 1) {
            std::fprintf(stderr, "ticks too short for one measured cycle\n");
            return 2;
        }
        const int meas = static_cast<int>(std::lround(cycles * T / dt));
        const int m_start = ticks - meas;
        double Is = 0.0, Ic = 0.0, sum_e = 0.0, peak_e = 0.0, prev_ang = 0.0;
        int n = 0;
        for (int i = 0; i < ticks; ++i) {
            const double tnow = (i + 1) * dt;
            const double ang = A * std::sin(w * tnow);
            const control::Telemetry t =
                drive(ang - prev_ang, A * w * std::cos(w * tnow));
            prev_ang = ang;
            if (sim::altitude(cl.state.position, p) <= 0.0) {
                std::fprintf(stderr, "CRASH at sine tick %d\n", i);
                return 1;
            }
            if (i >= m_start) {
                const glm::dvec3 nose =
                    cl.state.orientation * glm::dvec3{0.0, 0.0, -1.0};
                const double y = ang_about(nose, base);
                Is += y * std::sin(w * tnow);
                Ic += y * std::cos(w * tnow);
                sum_e += t.e;
                peak_e = std::max(peak_e, t.e);
                ++n;
            }
        }
        const double amp = 2.0 * std::sqrt(Is * Is + Ic * Ic) / n;
        const double phase = std::atan2(-Ic, Is);
        std::printf(
            "TRACK %s sine freq=%.2f V=%.0f gain=%.3f phase_deg=%.1f "
            "gain_ratio=%.3f mean_err_deg=%.3f peak_err_deg=%.3f carry=%.2f "
            "cap_engages=%d cap_ticks=%d cap_far_rim=%.2f amp=%.1f\n",
            axis_name.c_str(), f, V, gain_now, phase * deg, amp / A,
            (n ? sum_e / n : 0.0) * deg, peak_e * deg, cp.capture_carry,
            cap_engages, cap_ticks, cap_far / cp.capture_circle, amp_deg);
        return 0;
    }

    if (pattern == "nudge") {
        // The REAL HAND: a repeated small hand adjustment. Each cycle EASES the
        // aim amp_deg (default 1.0) about the axis over T_move via a smoothstep
        // position profile s(x)=3x^2-2x^3, then RESTS; the next cycle nudges the
        // SAME direction. Reports the rebound each engage earns — an event
        // table (engage tick/time, state entered, apex excursion PAST the aim in
        // deg + rim units, and w_rel_approx_dps) plus the summary counters.
        const double A = amp_deg / deg;
        const double dir = amp_deg >= 0.0 ? 1.0 : -1.0;
        const double T_move = 0.7, T_rest = 1.5;
        const int move_ticks = static_cast<int>(std::lround(T_move / dt));
        const int rest_ticks = static_cast<int>(std::lround(T_rest / dt));
        const int cycle_ticks = move_ticks + rest_ticks;
        const int cycles = cycle_ticks > 0 ? ticks / cycle_ticks : 0;
        if (cycles < 1) {
            std::fprintf(stderr, "ticks too short for one nudge cycle (need %d)\n",
                         cycle_ticks);
            return 2;
        }
        const int settle_ticks = static_cast<int>(std::lround(0.5 / dt));
        auto smooth = [](double x) { return x * x * (3.0 - 2.0 * x); };

        // Signed lead of the nose PAST the aim about `axis` [rad] (>0 = the nose
        // is ahead of the moving aim in the + rotation sense — the overshoot).
        auto lead_now = [&]() {
            const glm::dvec3 nose =
                cl.state.orientation * glm::dvec3{0.0, 0.0, -1.0};
            const glm::dvec3 np = nose - glm::dot(nose, axis) * axis;
            const glm::dvec3 ap = cl.aim - glm::dot(cl.aim, axis) * axis;
            return std::atan2(glm::dot(axis, glm::cross(ap, np)),
                              glm::dot(ap, np));
        };

        // Per-event accumulators (one event = a capture engage leaving IDLE).
        struct NEvent {
            int tick;
            double time;
            control::CaptureState state;
            double apex;      // max lead*dir over the event [rad]
            double w_rel_dps; // w_rel_approx at engage [deg/s]
        };
        std::vector<NEvent> events;
        int nprev = kCapIdle;
        bool in_event = false;
        double cur_apex = 0.0;
        // Rest-window metrics: settle band + flip detector (rest ticks only).
        double settle_sum = 0.0;
        int settle_n = 0, flips = 0;
        double rest_prev_e = 0.0, rest_prev_de = 0.0;
        int gi = 0;  // global tick counter across all cycles

        for (int c = 0; c < cycles; ++c) {
            for (int k = 1; k <= cycle_ticks; ++k, ++gi) {
                double dang = 0.0, rate_now = 0.0;
                const bool moving = k <= move_ticks;
                if (moving) {
                    const double t1 = std::min(k * dt, T_move);
                    const double t0 = std::min((k - 1) * dt, T_move);
                    dang = A * (smooth(t1 / T_move) - smooth(t0 / T_move));
                    const double x = t1 / T_move;
                    rate_now = A * 6.0 * x * (1.0 - x) / T_move;  // analytic ds/dt
                }
                const control::Telemetry t = drive(dang, rate_now);
                if (sim::altitude(cl.state.position, p) <= 0.0) {
                    std::fprintf(stderr, "CRASH at nudge tick %d\n", gi);
                    return 1;
                }
                const int ci = static_cast<int>(t.capture);
                // Event bookkeeping (per capture engage).
                if (nprev == kCapIdle && ci != kCapIdle) {
                    in_event = true;
                    cur_apex = lead_now() * dir;
                    // w_rel_approx: the nose's angular rate about the axis minus
                    // the scripted aim rate. INSTRUMENT APPROXIMATION only — the
                    // real net closing rate the controller ENGAGED on nets
                    // curvature/coordination/mouse-frame too, which we cannot see
                    // from outside; this is the visible-kinematics stand-in.
                    events.push_back({gi, (gi + 1) * dt, t.capture, cur_apex,
                                      (omega_about() - rate_now) * deg});
                }
                if (in_event) {
                    cur_apex = std::max(cur_apex, lead_now() * dir);
                    events.back().apex = cur_apex;
                    if (ci == kCapIdle) in_event = false;
                }
                // Rest-window settle band + flip count (REST ticks only).
                if (!moving) {
                    const double e = t.e;
                    if (k == move_ticks + 1) {
                        rest_prev_e = e;
                        rest_prev_de = 0.0;  // fresh window: no cross-window slope
                    } else {
                        const double de = e - rest_prev_e;
                        if (std::abs(de) > 1e-7) {
                            if (rest_prev_de != 0.0 && de * rest_prev_de < 0.0)
                                ++flips;
                            rest_prev_de = de;
                        }
                    }
                    rest_prev_e = e;
                    if (k > cycle_ticks - settle_ticks) {
                        settle_sum += std::abs(e);
                        ++settle_n;
                    }
                }
                nprev = ci;
            }
        }

        // Per-EVENT table.
        std::printf("  nudge events (axis=%s amp=%.1f):\n", axis_name.c_str(),
                    amp_deg);
        std::printf(
            "    engage_tick  time_s   state   apex_deg  apex_rim  "
            "w_rel_approx_dps\n");
        double max_apex = 0.0, sum_apex = 0.0;
        for (const NEvent& e : events) {
            const char* sn = e.state == control::CaptureState::CARRY  ? "CARRY"
                             : e.state == control::CaptureState::RETURN ? "RETURN"
                                                                        : "IDLE";
            std::printf("    %11d  %6.3f  %6s  %8.3f  %8.3f  %8.2f\n", e.tick,
                        e.time, sn, e.apex * deg, e.apex / cp.capture_circle,
                        e.w_rel_dps);
            max_apex = std::max(max_apex, e.apex);
            sum_apex += e.apex;
        }
        const int nev = static_cast<int>(events.size());
        const double mean_apex = nev ? sum_apex / nev : 0.0;
        const double rest_settle = settle_n ? settle_sum / settle_n : 0.0;
        std::printf(
            "TRACK %s nudge amp=%.1f V=%.0f gain=%.3f carry=%.2f cycles=%d "
            "cap_engages=%d cap_ticks=%d max_apex_deg=%.3f mean_apex_deg=%.3f "
            "rest_settle_deg=%.3f flips=%d\n",
            axis_name.c_str(), amp_deg, V, gain_now, cp.capture_carry, cycles,
            cap_engages, cap_ticks, max_apex * deg, mean_apex * deg,
            rest_settle * deg, flips);
        return 0;
    }

    // reversal: sweep at rate for 1.2 s, then an INSTANT sign flip (the
    // <100 ms 180-deg reversal trap probe). Reports the transient peak |e|,
    // time from the post-flip PEAK back inside the pre-flip envelope, and the
    // anti-phase kick — the nose rate moving FURTHER in the OLD direction
    // within 50 ms of the flip (should be ~0: pure FF has no r>0 kick).
    const double rate = rate_arg / deg;
    const int flip_tick = static_cast<int>(std::lround(1.2 / dt));
    if (ticks <= flip_tick + 120) {
        std::fprintf(stderr, "ticks too short (need > %d)\n", flip_tick + 120);
        return 2;
    }
    const int pre_start = flip_tick - static_cast<int>(std::lround(0.5 / dt));
    const int kick_win = static_cast<int>(std::lround(0.05 / dt));
    double pre_sum = 0.0, pre_peak = 0.0, w0 = 0.0, kick = 0.0;
    int pre_n = 0;
    std::vector<double> post_e;
    for (int i = 0; i < ticks; ++i) {
        if (i == flip_tick) w0 = omega_about();  // nose rate at the flip
        const double sgn = (i < flip_tick) ? 1.0 : -1.0;
        const control::Telemetry t = drive(sgn * rate * dt, sgn * rate);
        if (sim::altitude(cl.state.position, p) <= 0.0) {
            std::fprintf(stderr, "CRASH at reversal tick %d\n", i);
            return 1;
        }
        if (i >= pre_start && i < flip_tick) {
            pre_sum += t.e;
            pre_peak = std::max(pre_peak, t.e);
            ++pre_n;
        }
        if (i >= flip_tick) {
            post_e.push_back(t.e);
            if (i < flip_tick + kick_win)
                kick = std::max(kick, omega_about() - w0);  // + = OLD direction
        }
    }
    double post_peak = 0.0;
    int i_peak = 0;
    for (size_t i = 0; i < post_e.size(); ++i)
        if (post_e[i] > post_peak) {
            post_peak = post_e[i];
            i_peak = static_cast<int>(i);
        }
    int reconv = -1;  // ticks from the flip to back-inside-the-envelope
    for (size_t i = i_peak; i < post_e.size(); ++i)
        if (post_e[i] <= pre_peak) {
            reconv = static_cast<int>(i);
            break;
        }
    std::printf(
        "TRACK %s reversal rate=%.1f V=%.0f gain=%.3f peak_err_deg=%.2f "
        "reconverge_ms=%.0f kick_dps=%.2f pre_env_deg=%.3f carry=%.2f "
        "cap_engages=%d cap_ticks=%d cap_far_rim=%.2f\n",
        axis_name.c_str(), rate_arg, V, gain_now, post_peak * deg,
        reconv < 0 ? -1.0 : reconv * dt * 1000.0, kick * deg,
        (pre_n ? pre_sum / pre_n : 0.0) * deg, cp.capture_carry, cap_engages,
        cap_ticks, cap_far / cp.capture_circle);
    return 0;
}

// latflick: the LATERAL turn-reversal instrument (§7 Item 2, the "picks the
// WRONG way" crux). Establish a hard RIGHT turn (aim held a fixed offset RIGHT
// of the horizontal heading), then FLICK the aim the same offset LEFT and hold
// — exactly Chad's "banked right, mouse hard left" case. Prints roll/pitch/yaw
// Input, bank, the bank_error the roll law sees, and push_mode/regime each tick
// through the reversal, so the roll-vs-pitch choice is visible as NUMBERS. NOT
// a gate.
int run_latflick(const sim::AircraftParams& p, double V, double offset_deg,
                 int phase1, int phase2, double down_deg) {
    const control::ControllerParams cp =
        cfg::load_controller_toml(SEADS_CONFIG_DIR "/controller.toml", p);
    sim::SimState s0 =
        harness::level_trim_state(p, V, 3500.0, glm::dvec3{1.0, 0.0, 0.0},
                                  glm::dvec3{0.0, 0.0, -1.0}, nullptr);
    harness::ClosedLoop cl(s0, glm::dvec3{0.0, 0.0, -1.0});
    cl.aim_nose();
    const double off = offset_deg * kPi_step / 180.0;
    const double down = down_deg * kPi_step / 180.0;
    std::printf(
        "latflick V=%.0f offset=%.0f down=%.0f | RIGHT %d then LEFT(+down) %d\n"
        "  tick  side   bank   roll   pitch    yaw    err  bankErr  tb.x  tb.y "
        " "
        "tb.z  push regime\n",
        V, offset_deg, down_deg, phase1, phase2);
    for (int i = 1; i <= phase1 + phase2; ++i) {
        const glm::dvec3 up = sim::local_up(cl.state.position);
        const glm::dvec3 nose =
            cl.state.orientation * glm::dvec3{0.0, 0.0, -1.0};
        glm::dvec3 h = nose - glm::dot(nose, up) * up;
        if (glm::length(h) < 1e-6)
            h = cl.state.orientation * glm::dvec3{1.0, 0.0, 0.0};
        h = glm::normalize(h);
        const double sgn = (i <= phase1) ? +1.0 : -1.0;  // RIGHT then LEFT
        glm::dvec3 aim = glm::angleAxis(-sgn * off, up) * h;
        // On the LEFT flick, tilt the aim DOWN by `down` deg (test "down and to
        // the side": must ROLL over, not push straight at the belly).
        if (i > phase1 && down != 0.0) {
            const glm::dvec3 tilt = glm::normalize(glm::cross(aim, up));
            aim =
                glm::angleAxis(down, tilt) * aim;  // toward -up (below horizon)
        }
        cl.aim = glm::normalize(aim);
        const control::Telemetry t = cl.tick(1.0, p, cp);
        const glm::dvec3 tb =
            sim::body_dir_of(cl.state.orientation, glm::normalize(cl.aim));
        const double be =
            (tb.x * tb.x + tb.y * tb.y > 1e-12) ? control::bank_error(tb) : 0.0;
        const double bank =
            control::unfold_bank(t.extracted.phi, t.extracted.cos_phi_theta);
        const bool near_flick = (i > phase1 - 6 && i < phase1 + 80);
        if (i % 20 == 0 || near_flick)
            std::printf(
                "  %5d  %5s  %5.0f  %6.2f  %6.2f  %6.2f  %5.0f  %6.0f  %5.2f  "
                "%5.2f  %5.2f   %d   %s\n",
                i, (i <= phase1 ? "RIGHT" : "LEFT"), bank * 180.0 / kPi_step,
                cl.last_inputs.roll, cl.last_inputs.pitch, cl.last_inputs.yaw,
                t.e * 180.0 / kPi_step, be * 180.0 / kPi_step, tb.x, tb.y, tb.z,
                t.push_mode ? 1 : 0,
                t.regime == control::Regime::FINE ? "FINE" : "MANV");
    }
    return 0;
}

// lathold: the RUNG-C "hold the line" instrument (docs/v5_kernel_handoff.md
// RUNG C). Spawn level-trimmed at 3000 m, settle like latflick, then capture a
// WORLD-FIXED aim offset_deg to the LEFT of the initial heading AT THE HORIZON
// (elevation 0) and HOLD it — parallel-transported per tick exactly the way
// cl.tick transports a held aim (aim_moved fires ONLY on the capture tick, no
// scripted aim rate) — while flying FULL throttle (the sustained-turn energy
// case). Each ~0.5 s prints t/V/alt/alpha/phi/nose_elev/err/in_pitch/in_roll;
// the LATHOLD summary line reports the capture time, the deepest the nose sags
// below the horizon, altitude lost, the p95 ride AoA, the longest saturated-
// roll (windmill) run, and whether it crashed. THE rung-C regression surface.
// NOT a gate.
int run_lathold(const sim::AircraftParams& p, double V, double offset_deg,
                int ticks) {
    const control::ControllerParams cp =
        cfg::load_controller_toml(SEADS_CONFIG_DIR "/controller.toml", p);
    const double dt = p.sim_dt;
    const double deg = 180.0 / kPi_step;
    const glm::dvec3 up0{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    double thr = 0.0;
    const sim::SimState s0 =
        harness::level_trim_state(p, V, 3000.0, up0, heading, &thr);
    harness::ClosedLoop cl(s0, glm::dvec3{0.0, 0.0, -1.0});
    cl.aim_nose();
    // Settle exactly like the step/latflick fixture: one GROUNDED tick then
    // ~120 ticks at trim throttle with the aim on the nose.
    cl.tick(thr, p, cp, /*grounded=*/true);
    for (int i = 0; i < 120; ++i) cl.tick(thr, p, cp);

    // Capture the WORLD-FIXED held aim ONCE: the current horizontal nose
    // projection rotated offset_deg LEFT about local_up (latflick's LEFT sign
    // = +off), then dropped to exactly the horizon (elevation 0 vs local_up).
    // From here the aim is only parallel-transported by cl.tick — a held world
    // direction. aim_moved fires ONLY on the capture tick; aim_rate_world = 0
    // (a held aim carries no scripted mouse rate).
    const double off = offset_deg * kPi_step / 180.0;
    const glm::dvec3 up_cap = sim::local_up(cl.state.position);
    const glm::dvec3 nose_cap =
        cl.state.orientation * glm::dvec3{0.0, 0.0, -1.0};
    glm::dvec3 h = nose_cap - glm::dot(nose_cap, up_cap) * up_cap;
    if (glm::length(h) < 1e-6)
        h = cl.state.orientation * glm::dvec3{1.0, 0.0, 0.0};
    h = glm::normalize(h);
    glm::dvec3 hr = glm::angleAxis(off, up_cap) * h;  // rotate LEFT about up
    hr = hr - glm::dot(hr, up_cap) * up_cap;           // project to the horizon
    cl.aim = glm::normalize(hr);

    std::printf(
        "lathold V=%.0f offset=%.0f ticks=%d | world-fixed horizon aim, full "
        "throttle\n"
        "      t      V     alt   alpha     phi  nose_elev     err  in_pitch  "
        "in_roll\n",
        V, offset_deg, ticks);

    const double start_alt = sim::altitude(cl.state.position, p);
    const int print_every = static_cast<int>(std::lround(0.5 / dt));
    double min_nose_elev = 1e9, min_alt = start_alt;
    int t_capture_tick = -1, windmill_ticks = 0, windmill_cur = 0, crashed = 0;
    std::vector<double> alpha_pos;
    alpha_pos.reserve(static_cast<size_t>(ticks));

    for (int i = 0; i < ticks; ++i) {
        cl.aim_moved = (i == 0);              // fires only on the capture tick
        cl.aim_rate_world = glm::dvec3{0.0};  // a HELD aim has no scripted rate
        const control::Telemetry t = cl.tick(1.0, p, cp);  // FULL throttle
        const double alt = sim::altitude(cl.state.position, p);
        min_alt = std::min(min_alt, alt);
        if (alt <= 0.0) {
            std::printf("  CRASH at tick %d\n", i);
            crashed = 1;
            break;
        }
        const glm::dvec3 up = sim::local_up(cl.state.position);
        const glm::dvec3 nose =
            cl.state.orientation * glm::dvec3{0.0, 0.0, -1.0};
        const double nose_elev =
            std::asin(std::clamp(glm::dot(nose, up), -1.0, 1.0)) * deg;
        const double alpha = t.extracted.alpha * deg;
        const double phi =
            control::unfold_bank(t.extracted.phi, t.extracted.cos_phi_theta) *
            deg;
        const double err = t.e * deg;
        const double sp = glm::length(cl.state.velocity);
        const double in_roll = cl.last_inputs.roll;

        if (t_capture_tick < 0 && err < 5.0) t_capture_tick = i;
        min_nose_elev = std::min(min_nose_elev, nose_elev);
        if (alpha > 0.0) alpha_pos.push_back(alpha);
        if (std::abs(in_roll) > 0.95) {
            ++windmill_cur;
            windmill_ticks = std::max(windmill_ticks, windmill_cur);
        } else {
            windmill_cur = 0;
        }

        if (i % print_every == 0)
            std::printf(
                "  %5.2f  %5.0f  %6.0f  %6.2f  %6.1f  %9.2f  %6.1f  %8.3f  "
                "%8.3f\n",
                (i + 1) * dt, sp, alt, alpha, phi, nose_elev, err,
                cl.last_inputs.pitch, in_roll);
    }

    // alpha p95 over the POSITIVE lobe (sorted, index floor(0.95*(n-1))).
    double alpha_p95 = 0.0;
    if (!alpha_pos.empty()) {
        std::sort(alpha_pos.begin(), alpha_pos.end());
        const size_t idx =
            static_cast<size_t>(std::floor(0.95 * (alpha_pos.size() - 1)));
        alpha_p95 = alpha_pos[idx];
    }
    const double t_capture_s =
        t_capture_tick < 0 ? -1.0 : (t_capture_tick + 1) * dt;
    if (min_nose_elev > 1e8) min_nose_elev = 0.0;  // crash-on-tick-0 guard
    std::printf(
        "LATHOLD V=%.0f offset=%.0f t_capture_s=%.2f min_nose_elev_deg=%.2f "
        "alt_loss_m=%.0f alpha_p95_deg=%.2f windmill_ticks=%d crashed=%d\n",
        V, offset_deg, t_capture_s, min_nose_elev, start_alt - min_alt,
        alpha_p95, windmill_ticks, crashed);
    return crashed ? 1 : 0;
}

}  // namespace

int main(int argc, char** argv) {
    sim::AircraftParams p;
    try {
        p = cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 2;
    }

    const std::string mode = argc > 1 ? argv[1] : "fly";
    if (mode == "fly") {
        const int ticks = argc > 2 ? std::atoi(argv[2]) : 1200;
        const std::string out = argc > 3 ? argv[3] : "telemetry_flight.csv";
        return run_fly(p, ticks, out);
    }
    if (mode == "step") {
        const std::string axis = argc > 2 ? argv[2] : "pitch";
        const double deg = argc > 3 ? std::atof(argv[3]) : 30.0;
        const double V = argc > 4 ? std::atof(argv[4]) : 140.0;
        const int ticks = argc > 5 ? std::atoi(argv[5]) : 720;
        const std::string out = argc > 6 ? argv[6] : "";
        const double alt = argc > 7 ? std::atof(argv[7]) : 3000.0;
        // arg 8: capture carry A/B override (S-rimshot; -1 = loaded table,
        // 0 = the structural legacy baseline row — never a tune path).
        const double carry_ovr = argc > 8 ? std::atof(argv[8]) : -1.0;
        return run_step(p, axis, deg, V, ticks, out, alt, carry_ovr);
    }
    if (mode == "loop") {
        const double V = argc > 2 ? std::atof(argv[2]) : 220.0;
        const double pitch = argc > 3 ? std::atof(argv[3]) : 160.0;
        const double lateral = argc > 4 ? std::atof(argv[4]) : 8.0;
        const int ticks = argc > 5 ? std::atoi(argv[5]) : 1800;
        return run_loop(p, V, pitch, lateral, ticks);
    }
    if (mode == "mouseloop") {
        const std::string dir = argc > 2 ? argv[2] : "up";
        const double deflect = argc > 3 ? std::atof(argv[3]) : 165.0;
        const double V = argc > 4 ? std::atof(argv[4]) : 220.0;
        const int ticks = argc > 5 ? std::atoi(argv[5]) : 1200;
        return run_mouseloop(p, dir, deflect, V, ticks);
    }
    if (mode == "latflick") {
        const double V = argc > 2 ? std::atof(argv[2]) : 200.0;
        const double offset = argc > 3 ? std::atof(argv[3]) : 70.0;
        const int phase1 = argc > 4 ? std::atoi(argv[4]) : 240;
        const int phase2 = argc > 5 ? std::atoi(argv[5]) : 300;
        const double down = argc > 6 ? std::atof(argv[6]) : 0.0;
        return run_latflick(p, V, offset, phase1, phase2, down);
    }
    if (mode == "lathold") {
        const double V = argc > 2 ? std::atof(argv[2]) : 140.0;
        const double offset = argc > 3 ? std::atof(argv[3]) : 90.0;
        const int ticks = argc > 4 ? std::atoi(argv[4]) : 3600;
        return run_lathold(p, V, offset, ticks);
    }
    if (mode == "track") {
        const std::string axis = argc > 2 ? argv[2] : "pitch";
        const std::string pattern = argc > 3 ? argv[3] : "sweep";
        const double rate =
            argc > 4 ? std::atof(argv[4]) : (pattern == "sine" ? 0.7 : 20.0);
        const double V = argc > 5 ? std::atof(argv[5]) : 140.0;
        const int ticks = argc > 6 ? std::atoi(argv[6]) : 600;
        const double gain_ovr = argc > 7 ? std::atof(argv[7]) : -1.0;
        const double carry_ovr = argc > 8 ? std::atof(argv[8]) : -1.0;
        // argv[9] = sine/nudge amplitude [deg]. Default 10 for sine (bit-stable
        // legacy), 1 for nudge (the real hand's small adjustment).
        const double amp = argc > 9 ? std::atof(argv[9])
                                    : (pattern == "nudge" ? 1.0 : 10.0);
        return run_track(p, axis, pattern, rate, V, ticks, gain_ovr, carry_ovr,
                         amp);
    }
    if (mode == "comfort") return harness::run_comfort(p);
    if (mode == "alpha") return run_alpha(p);
    if (mode == "golden") return run_golden(p);
    if (mode == "ctrl_golden") return run_ctrl_golden(p);
    if (mode == "ctrl_fly") {
        const int ticks = argc > 2 ? std::atoi(argv[2]) : harness::kAt12Ticks;
        const std::string out = argc > 3 ? argv[3] : "ctrl_telemetry.csv";
        return run_ctrl_fly(p, ticks, out);
    }

    std::fprintf(
        stderr,
        "unknown mode '%s' (fly | ctrl_fly | step | track | loop | mouseloop | "
        "latflick | lathold | comfort | alpha | golden | ctrl_golden)\n",
        mode.c_str());
    return 2;
}
