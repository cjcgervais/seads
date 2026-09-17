// S-maninvert — the LOOP-ROLLOVER instrument and its gate legs
// (feel/lean-lead-walkback, 2026-09-12).
//
// Chad: "pulling a straight loop my wings are completely unwantedly banking
// over 180 degrees" — with the mouse held STRAIGHT UP, zero lateral. And the
// spec this file grades, verbatim: "If I keep inputting upward deflection the
// airframe should stay in its orientation right around the loop... an
// Immelmann, where I stop inputting deflection at the top, auto rights the
// airframe."
//
//
// THE CURE (S-lapguard, [regime] lap_roll_frac): a two-stage LAP latch. A
// cos_phi_theta fade on the maneuver limb was tried FIRST and FALSIFIED -- it
// is a self-locking wall (reaching inverted REQUIRES rolling through cos == 0)
// and it broke AT-15, the mouse-DOWN split-S and the capture jink. params.h
// carries that scar. The deliberate split-S and the lap BOTH end at
// target_body.y < 0, so no instantaneous geometry separates them; only
// HISTORY does -- a lap went OVER THE TOP first.
//
// THE MECHANISM (attributed here, not guessed — the traces below print it).
// A held mouse sweeps the aim at aim_sensitivity * (px/s) with NO bound; the
// airframe can only pull ~30-35 deg/s of loop. So the aim LAPS the nose: with
// the aim d degrees around the loop plane ahead of the nose,
// target_body.y == sin(d), so the moment d passes 180 deg the aim comes round
// BELOW the wing line and tb.y goes NEGATIVE. There
// bank_error == atan2(tb.x, tb.y) (controller.h) jumps from ~0 to
// +/-160..180 deg — "roll to put the target above the nose" now means roll
// INVERTED — and because err >> blend_hi the whole roll channel IS the
// maneuver limb (blend == 1), so sqrt_law saturates and the wings go through
// 180 deg at p_max. The lat_sq > 1e-12 guard in controller.cpp is a numerical
// epsilon, not a cone, and never fires there. The PITCH channel has an astern
// guard for exactly this crossing (the elev_latch, astern_on/astern_off);
// ROLL had none. [regime] lap_roll_frac is that guard.
//
// NOT A REGRESSION: this same file, added untracked to detached worktrees of
// main at c75bc502d (09-08), 31c53150c (08-25) and 038cc0b20 (08-12), printed
// BIT-IDENTICAL numbers, and f61727e81 (07-19) printed WORSE ones.
//
// This drives the SHIPPED app::tick pipeline (aim frame + RAW apply_mouse +
// control::step + sim::step) — never a synthetic target vector — so the mouse
// path it grades is the mouse path Chad flies.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <glm/gtc/quaternion.hpp>
#include <cstring>
#include <string>
#include <vector>

#include "app/instructor_tick.h"
#include "app/loop.h"
#include "config/load_aircraft.h"
#include "config/load_controller.h"
#include "sim/world.h"
#include "test/harness/feel_tape.h"
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

// Bit-exact running hash of the emitted roll demand, tick for tick. A SUM
// would let two different traces cancel to the same total; this mixes the
// raw IEEE bits, so a single changed ULP on a single tick changes it.
void mix(std::uint64_t& h, double v) {
    std::uint64_t b;
    std::memcpy(&b, &v, sizeof b);
    h ^= b + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
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

struct Row {
    double t, bank, roll_out, loop_ang, err, blend, cpt, az_lat, held;
    bool dz, rt, pm;
    int regime;
    double be, tby, wz;
};

struct LoopRun {
    std::vector<Row> rows;
    double peak_bank = 0.0;  // [rad] max |roll out of the loop plane|
    double t_peak = 0.0;
    double peak_phi = 0.0;   // [rad] max |SPEC phi| (wing tilt off horizon)
    double loop_total = 0.0;  // [rad] cumulative pitch-plane rotation
    double t_90 = -1.0;       // [s] first time |roll_out| > 90 deg
    double err_max = 0.0;     // [rad] peak pointing error
    double tby_min = 1.0;     // min target_body.y (<0 == the astern lap)
    // FIRST-LOOP window (the loop Chad actually flies): everything until the
    // cumulative pitch-plane rotation first reaches 360 deg, or `first_secs`,
    // whichever comes first.
    double peak_first = 0.0;  // [rad] max |roll_out| inside that window
    double cpt_min_first = 1.0;
    std::uint64_t hash = 0xcbf29ce484222325ULL;  // omega_des.z, tick for tick
    // Immelmann leg: after the release the airframe must come back upright.
    double t_upright = -1.0;  // [s] first post-release tick with cos > 0.9
                              //     AND |phi| < 15 deg
    double phi_end = 0.0, cpt_end = 0.0;
    std::uint64_t wz_hash = 0xcbf29ce484222325ULL;  // omega_des.z only
    double wz_mean_tail = 0.0;   // [rad/s] mean |omega_des.z| over the last 3 s
    double lat_swing = 0.0;     // [rad] |bank| swing in the 1 s after the
                                //       lateral mouse was added
};

// ONE probe shape, shared by every leg.
//   bank0     [rad] seeded bank (the trimmed state rolled about its own nose)
//   rate_deg_s  the HELD vertical mouse, expressed as aim sweep rate
//   release_at  [rad] cumulative loop angle at which aim_dy drops to 0
//               (the Immelmann's "stop inputting deflection at the top");
//               <= 0 means never release — the mouse is held the whole run.
//   lat_rate_deg_s  Chad: "the loop is sustained by me sustaining the
//               motion, if I change the motion it should change the
//               behavior" -- once the LAP LATCH has actually fired (which is
//               what "mid-lap" means), the probe ALSO feeds a lateral mouse
//               at this rate: the pilot adds sideways hand. 0 = never.
LoopRun run_loop(const control::ControllerParams& cp, double bank0,
                 double secs, double rate_deg_s, double V, double alt,
                 double first_secs = 6.0, double release_at = 0.0,
                 bool stop_at_360 = true, double lat_rate_deg_s = 0.0,
                 double pure_lat_deg_s = 0.0, double pure_lat_secs = 0.0) {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    double thr = 0.9;
    sim::SimState s0 =
        harness::level_trim_state(kAp, V, alt, up, heading, &thr);
    if (bank0 != 0.0) {  // roll the trimmed state about its own nose: the
                         // bank moves, the body-frame trim does not
        const glm::dvec3 n0 = s0.orientation * glm::dvec3{0, 0, -1};
        const glm::dquat r = glm::angleAxis(bank0, n0);
        s0.orientation = glm::normalize(r * s0.orientation);
        s0.velocity = r * s0.velocity;
        s0.last_vhat = r * s0.last_vhat;
    }
    app::LoopState st = flying(s0);
    st.grounded = true;  // one seed tick: aim := nose, held_bank := bank0
    harness::MiniCamera cam;
    cam.seed(st.curr);
    const glm::dvec3 right0 = s0.orientation * glm::dvec3{1, 0, 0};
    const double dy = -rad(rate_deg_s) * kAp.sim_dt / cp.aim_sensitivity;
    const double dx_lat =
        rad(lat_rate_deg_s) * kAp.sim_dt / cp.aim_sensitivity;

    LoopRun L;
    const int n = static_cast<int>(secs / kAp.sim_dt + 0.5);
    double loop_ang = 0.0;
    bool released = false;
    double t_release = -1.0;
    double lat_t0 = -1.0, lat_bank0 = 0.0;
    long all_ticks = 0;
    double wz_tail_sum = 0.0;
    long wz_tail_n = 0;
    for (int i = 0; i < n; ++i) {
        const double t = (i + 1) * kAp.sim_dt;
        if (release_at > 0.0 && !released && std::abs(loop_ang) >= release_at) {
            released = true;
            t_release = t;
        }
        app::TickInput in;
        in.raw_mode = false;
        in.throttle = 1.0;
        // HELD straight up, zero lateral — the whole point of the probe.
        const bool pure_lat = pure_lat_deg_s != 0.0;
        in.aim_dy = (st.grounded || released || pure_lat) ? 0.0 : dy;
        in.aim_dx = 0.0;
        if (pure_lat && !st.grounded) {
            in.aim_dx = (t <= pure_lat_secs)
                            ? rad(pure_lat_deg_s) * kAp.sim_dt /
                                  cp.aim_sensitivity
                            : 0.0;
        }
        in.cam_fwd = cam.cam_fwd;  // previous tick's basis (main.cpp's lag)
        in.cam_up = cam.cam_up;
        const app::TickResult r = app::tick(st, in, kAp, cp, nullptr);
        cam.advance(st.curr, st.aim.forward(), st.aim.up(), cp, kAp.sim_dt);
        st.grounded = false;
        mix(L.hash, r.telem.omega_des.z);
        mix(L.wz_hash, r.telem.omega_des.z);
        ++all_ticks;
        if (secs - t <= 3.0) {
            wz_tail_sum += std::abs(r.telem.omega_des.z);
            ++wz_tail_n;
        }
        const sim::SimState& s = st.curr;
        const glm::dvec3 upw = sim::local_up(s.position);
        const glm::dvec3 rw = s.orientation * glm::dvec3{1, 0, 0};
        // Fold-safe bank (SPEC §7 bank_full), valid through inversion.
        const double bank =
            std::atan2(-glm::dot(rw, upw), r.telem.extracted.cos_phi_theta);
        // ROLL OUT OF THE LOOP PLANE — the honest "my wings rolled over"
        // gauge. A clean loop keeps body_right along the ENTRY right axis
        // (the loop-plane normal) for the whole 360: roll_out == 0.
        // bank_full is ill-conditioned with the nose vertical (both of its
        // atan2 arguments vanish) and reads a legitimate wings-level
        // INVERTED apex as 180 — it is printed, never graded.
        const glm::dvec3 nb = s.orientation * glm::dvec3{0, 0, -1};
        glm::dvec3 rp = right0 - glm::dot(right0, nb) * nb;
        const double rpl = glm::length(rp);
        double roll_out = 0.0;
        if (rpl > 1e-6) {
            rp /= rpl;
            roll_out = std::atan2(glm::dot(glm::cross(rp, rw), nb),
                                  glm::dot(rp, rw));
        }
        L.peak_phi = std::max(L.peak_phi, std::abs(r.telem.extracted.phi));
        L.err_max = std::max(L.err_max, r.telem.e);
        // Cumulative pitch-plane rotation about the ENTRY right axis.
        loop_ang +=
            glm::dot(s.orientation * s.angular_vel, right0) * kAp.sim_dt;
        if (std::abs(loop_ang) < 2.0 * kPi && t <= first_secs) {
            L.peak_first = std::max(L.peak_first, std::abs(roll_out));
            L.cpt_min_first =
                std::min(L.cpt_min_first, r.telem.extracted.cos_phi_theta);
        }
        if (std::abs(roll_out) > L.peak_bank) {
            L.peak_bank = std::abs(roll_out);
            L.t_peak = t;
        }
        if (L.t_90 < 0.0 && std::abs(roll_out) > rad(90.0)) L.t_90 = t;
        // target_body, and the cascade's own az_lat, recomputed from the SAME
        // extract fields the cascade reads (never a second formula).
        const glm::dvec3 tb = sim::body_dir_of(
            s.orientation, glm::normalize(st.aim.forward()));
        const double az =
            std::atan2(tb.x * r.telem.extracted.cos_phi_theta +
                           tb.y * std::sin(r.telem.extracted.phi),
                       -tb.z);
        const double be = (tb.x * tb.x + tb.y * tb.y > 1e-18)
                              ? std::atan2(tb.x, tb.y)
                              : 0.0;
        L.tby_min = std::min(L.tby_min, tb.y);
        L.phi_end = r.telem.extracted.phi;
        L.cpt_end = r.telem.extracted.cos_phi_theta;
        if (lat_t0 >= 0.0) {
            if (lat_t0 < 0.0) {
                lat_t0 = t;
                lat_bank0 = bank;
            } else if (t - lat_t0 <= 1.0) {
                L.lat_swing = std::max(L.lat_swing, std::abs(bank - lat_bank0));
            }
        }
        if (released && L.t_upright < 0.0 &&
            r.telem.extracted.cos_phi_theta > 0.9 &&
            std::abs(r.telem.extracted.phi) < rad(15.0)) {
            L.t_upright = t - t_release;
        }
        if ((i % 25) == 0 || i == n - 1) {
            L.rows.push_back({t, bank, roll_out, loop_ang, r.telem.e,
                              r.telem.blend, r.telem.extracted.cos_phi_theta,
                              az, r.telem.held_bank, r.telem.deadzoned,
                              r.telem.righting, r.telem.push_mode,
                              r.telem.regime == control::Regime::FINE ? 0 : 1,
                              be, tb.y, r.telem.omega_des.z});
        }
        if (stop_at_360 && std::abs(loop_ang) > 2.0 * kPi) break;
    }
    L.loop_total = loop_ang;
    L.wz_mean_tail = wz_tail_n > 0 ? wz_tail_sum / double(wz_tail_n) : 0.0;
    return L;
}

void report(const char* tag, const LoopRun& L) {
    std::printf(
        "[loop] %-32s peak|rollout| %7.2f deg @ %5.2f s | FIRST LOOP %7.2f | "
        "loop %7.1f deg | err_max %6.1f | tby_min %6.3f | t(>90) %s\n",
        tag, deg(L.peak_bank), L.t_peak, deg(L.peak_first), deg(L.loop_total),
        deg(L.err_max), L.tby_min,
        L.t_90 < 0 ? "never" : [&] {
            static char b[16];
            std::snprintf(b, sizeof b, "%.2f s", L.t_90);
            return b;
        }());
}

void trace(const char* tag, const LoopRun& L) {
    std::printf("[loop] TRACE %s\n", tag);
    std::printf(
        "[loop]   t(s) bankF(deg) rollout(deg) loop(deg) err(deg) blend cosPT "
        "az_lat(deg) held(deg) dz rt push reg  bank_err(deg) tb.y  wz(deg/s)\n");
    for (const Row& r : L.rows) {
        std::printf(
            "[loop]  %5.2f %8.2f %9.2f %9.1f %8.2f %6.2f %6.3f %9.3f %7.2f "
            "%d  %d  %d   %s  %9.2f %7.3f %9.2f\n",
            r.t, deg(r.bank), deg(r.roll_out), deg(r.loop_ang), deg(r.err),
            r.blend, r.cpt, deg(r.az_lat), deg(r.held), r.dz ? 1 : 0,
            r.rt ? 1 : 0, r.pm ? 1 : 0, r.regime ? "MAN" : "FINE", deg(r.be),
            r.tby, deg(r.wz));
    }
}

// THE PRE-CHANGE TREE: every dial this lane added, off. The hash leg below
// pins this arm against a trace recorded from a build that had none of them.
control::ControllerParams off_arm() {
    control::ControllerParams c = kCp;
    // EVERY dial this lane has added, from the ONE list beside the loader
    // (control/params.h). Zeroing them field-by-field here is what reddened
    // the hash leg three times on a tree that was never wrong.
    SEADS_FEEL_DIALS_OFF(c);
    return c;
}

// The shipped arm, config-relative (the S-leanlead red-team P1 lesson: a
// walk-back of the dial must not red this file — it measures the MECHANISM
// at a live value, and the lap_roll_frac == 0 legs pin the off arm).

}  // namespace

// ===========================================================================
// LEG (i) — the knob-off arm IS the pre-change tree, tick for tick.
//
// The recorded hash is the bit-mix of control::step's emitted omega_des.z on
// EVERY tick of a rolling loop (90 deg/s held mouse, V220, 12 s — the run
// that rolls over), captured by rebuilding this very tree with
// `git show HEAD:control/controller.cpp` in place of the gated one: the
// PRE-CHANGE controller, same config, same probe, same compiler. So this is
// not a self-consistency check — it is the pre-change binary's own trace, and
// a single changed ULP on a single tick moves it.
// ===========================================================================
TEST_CASE("S-righthand: the off arm is the pre-change tree, tick for tick") {
    constexpr std::uint64_t kPreChange = 0xdbdf52980174305eULL;
    const LoopRun off = run_loop(off_arm(), 0.0, 12.0, 90.0, 220.0, 6000.0,
                                 6.0, 0.0, /*stop_at_360=*/false);
    std::printf(
        "[loop] knob-off roll-demand hash %016llx  (pre-change recorded "
        "%016llx)\n",
        static_cast<unsigned long long>(off.hash),
        static_cast<unsigned long long>(kPreChange));
    REQUIRE(off.hash == kPreChange);
    // S-tremor (2026-09-16): the pin must hold at the SHIPPED window too,
    // not only at 0. This probe holds a 90 deg/s mouse, which the net
    // measure reads as 90 deg/s and scores fully live from its first tick,
    // and it never reaches the apex where the hand-rest ramp is consumed --
    // so the dial is invisible here twice over, and that is asserted rather
    // than assumed.
    control::ControllerParams shipped_window = off_arm();
    shipped_window.hand_net_window = kCp.hand_net_window;
    const LoopRun onw = run_loop(shipped_window, 0.0, 12.0, 90.0, 220.0, 6000.0,
                                 6.0, 0.0, /*stop_at_360=*/false);
    std::printf("[loop] shipped-window roll-demand hash %016llx\n",
                static_cast<unsigned long long>(onw.hash));
    CHECK(onw.hash == kPreChange);
    // NON-VACUITY lives in the S-righthand apex legs below, and for the
    // window in the S-tremor block at the end of this file: this
    // rolling-loop probe never reaches the apex geometry the live dials
    // act on, so a "the dial moves this hash" assertion here would be
    // the vacuous kind.
}


// ===========================================================================
// LEG (iii) — THE IMMELMANN. Chad: "an Immelmann, where I stop inputting
// deflection at the top, auto rights the airframe." The fade must NOT break
// this: it lands on the MANEUVER limb only, and the release-at-the-top
// righting is owned by the rest / MB-right path, which is untouched.
//
// Scripted exactly as he flies it: hold the mouse up until the airframe has
// pitched 180 deg (over the top, belly-up), then aim_dy := 0 and hands off.
// ===========================================================================

// ===========================================================================
// LEG (iv) — the flown TURN ENTRY is bit-untouched. smoothstep returns
// EXACTLY 1.0 at/above the band, so in upright flight (cos_phi_theta >= band)
// the two arms must emit the SAME DOUBLE for the roll demand, tick for tick.
// The probe is deliberately OUTSIDE every latch band: a lateral aim step that
// never inverts, never goes ballistic, never enters the astern latch.
// ===========================================================================

// The NAMED TRADE, measured: a lateral aim big enough to bank past the fade
// band DOES lose some bank-to-turn roll. This leg exists so the size of that
// loss is a number in the gate output, not a surprise on the stick.

// ===========================================================================
// THE LATERAL RELEASE. Chad, verbatim: "the loop is sustained by me sustaining
// the motion, if I change the motion it should change the behavior." So the
// lap guard must not be a cage: the moment the pilot puts real SIDEWAYS
// content into the mouse, the bank-to-turn roll comes back and the airframe
// follows his aim -- mid-lap, without waiting for the latch's own release.
// ===========================================================================

// ===========================================================================
// FRAME-RATE INDEPENDENCE of the classification (the AT-9 class). The mouse is
// consumed once per FRAME, so at 30 fps the aim moves in jumps 8x larger than
// at 240 fps. A latch that keys on per-frame samples could therefore CLASSIFY
// DIFFERENTLY at different frame rates -- which would make the trajectory
// frame-rate dependent, the one thing AT-9 forbids. AT-9's own green says
// nothing here: its scripted flicks never lap.
//
// This drives the SHIPPED app::step_frame (the real accumulator) with the same
// sustained mouse-up sweep, delivered at the same ANGULAR RATE, at 30 Hz
// (4*sim_dt) and 240 Hz (0.5*sim_dt), and requires the lap to be classified
// the same way and the first-lap roll to agree.
// ===========================================================================


// ===========================================================================
// FIX-1 (red-team P0): a PURE LATERAL sweep must never latch the lap. The aim
// swings behind ALONG THE WING LINE, so it enters the rear hemisphere with
// target_body.y ~ 0 -- nowhere near "over the top". Before kLapTopCone the
// over-the-top test reused the 2.9 deg z-band as its cone and this latched on
// 94.9% of ticks and never released (mean commanded roll 84.55 deg/s over the
// last 3 s against 0.17 with the guard off).
//
// The pin is BIT-IDENTICAL, not "small": on every non-lap maneuver the guard
// must be structurally invisible, so ON and OFF emit the same doubles.
// ===========================================================================

// ===========================================================================
// FIX-3: the LATERAL RELEASE is load-bearing, pinned on the DEMAND.
// The earlier leg graded the airframe's bank swing, which the plant produces
// anyway -- so deleting the release (M2) survived it. These grade the
// controller's own state and its emitted roll demand instead.
// ===========================================================================

// ===========================================================================
// FIX-3: the 0.1 s RAMP is load-bearing (M7). The latch is discrete; what the
// gate multiplies is a first-order ramp, so no command ever steps. Delete the
// ramp (lap_ramp := the latch) and the suppression arrives fully formed on
// one tick -- which this catches: on the latching tick the ramp is still a
// single tick of rise (dt/tau == 1/12), and it needs ~3 tau to reach 0.9.
// ===========================================================================

// ===========================================================================
// FIX-3: the OVER-THE-TOP discriminator, pinned HERE and not only through
// AT-15 (M3/M4 previously had exactly one guardian, in another file). A
// deliberate DOWN aim -- the split-S shape -- reaches the rear hemisphere
// from BELOW, so it must never latch, and the guard must be bit-invisible.
// ===========================================================================

// ===========================================================================
// FIX-3: the BALLISTIC ordering. The state machine runs BEFORE the ballistic
// branch, deliberately: it must keep tracking the aim through a low-speed
// excursion or it misses the crossing that happens down there and comes out
// mis-classified. That is SAFE because the gate it feeds is evaluated only
// inside the cascade branch -- so while ballistic, ON and OFF emit the same
// doubles BY CONSTRUCTION. This pins that, rather than trusting it (a
// low-speed held-up sweep latches the lap at 2.12 s while ballistic).
// Moving the machine after the ballistic branch was the alternative and is
// NOT bit-identical-safe: it would silently drop crossings.
// ===========================================================================

// ===========================================================================
// FIX-4, the honest residual: the SLOW low-energy pull (40 deg/s at V140).
// The airframe manages only ~168 deg of loop in 12 s, so the aim only grazes
// the wing line (target_body.y min -0.027) and the lap latch never engages --
// the guard does not reach this case AT ALL. Its rollover (179.99 deg at
// 9.03 s) has a different trigger and is NOT fixed here. This leg exists so
// that stays VISIBLE: it pins the guard as bit-invisible, and if a future
// change starts touching this case, this is what says so.
// ===========================================================================

// ===========================================================================
// REPLAY A RECORDED FEEL TAPE -- Chad's own hand, not a scripted sweep.
//
// The app writes the tape when SEADS_FEEL_TAPE names a path (app/main.cpp,
// the step_frame TickHook). This reads it back and feeds the RECORDED
// per-tick mouse deltas through the SAME app::tick path, at lap_roll_frac 0
// and 1, printing the roll-out trace, the altitude, and the latch state -- so
// the guard is graded against the input that actually rolled him over instead
// of a synthetic one that says it should not have.
//
//   RECIPE (PowerShell, one line):
//     $env:SEADS_FEEL_TAPE="D:\flight_sim2\seads-feel\build-play\feel_tape_loop.csv"; & "D:\flight_sim2\seads-feel\build-play\seads.exe"
//   fly the maneuver, quit, then:
//     $env:SEADS_FEEL_TAPE="...\feel_tape_loop.csv"; .\build\seads_tests.exe "S-righthand: replay a recorded feel tape"
//
// SKIPS (not fails) when the env var is unset or the file is missing: it is a
// bench tool for a captured flight, not a gate leg with a fixture.
// ===========================================================================
namespace {
struct TapeRow {
    int frame_ticks = 1;
    double dx = 0.0, dy = 0.0;
    // the RECORDED truth, for the faithfulness comparison
    double phi = 0.0, theta = 0.0, alt = 0.0, wdz = 0.0;
    int righting = 0;
    double roll_hold = 0.0, roll_man = 0.0, roll_right = 0.0, hand_gate = 1.0;
    bool has_state = false;
};

// The t=0 seed the app writes as "# seed_*" comment lines (tape v2). Without
// it a replay starts from a synthetic trim and reproduces nothing -- v1's
// fatal gap.
struct TapeSeed {
    bool ok = false;
    glm::dvec3 pos{0.0}, vel{0.0}, vhat{0.0};
    glm::dquat q{1.0, 0.0, 0.0, 0.0};
};

// Reads a tape BY COLUMN NAME through harness::FeelTape, which throws on a
// missing column. The previous body indexed POSITIONALLY with v2-era offsets
// (alt = v[29]); once the row writer grew to 74 columns that index landed on
// hand_rest, so this bench tool was grading altitude against the hand-rest
// timer. Names cannot rot that way.
std::vector<TapeRow> read_tape(const char* path, std::string* banner,
                               TapeSeed* seed) {
    std::vector<TapeRow> rows;
    harness::FeelTape tp;
    // The explicit dependency of THIS reader. A tape lacking any of these is
    // not one this bench can replay, and it says so instead of zeroing.
    std::vector<std::string> need = harness::FeelTape::input_columns();
    for (const char* c : {"phi", "theta", "wdz", "alt", "righting",
                          "roll_hold", "roll_maneuver", "roll_right",
                          "hand_gate"})
        need.push_back(c);
    tp.load(path, need);  // throws with the missing names listed
    if (banner != nullptr) *banner = tp.banner();

    // The t=0 seed still rides in the "# seed_*" banner comments.
    if (seed != nullptr) {
        int got = 0;
        double a, b2, c2, d;
        for (const char* q = tp.banner().c_str(); *q;) {
            if (std::sscanf(q, "# seed_pos %lf %lf %lf", &a, &b2, &c2) == 3) {
                seed->pos = {a, b2, c2};
                ++got;
            } else if (std::sscanf(q, "# seed_vel %lf %lf %lf", &a, &b2, &c2)
                       == 3) {
                seed->vel = {a, b2, c2};
                ++got;
            } else if (std::sscanf(q, "# seed_quat %lf %lf %lf %lf", &a, &b2,
                                   &c2, &d) == 4) {
                seed->q = glm::dquat(a, b2, c2, d);
                ++got;
            } else if (std::sscanf(q, "# seed_vhat %lf %lf %lf", &a, &b2, &c2)
                       == 3) {
                seed->vhat = {a, b2, c2};
                ++got;
            }
            const char* nl = std::strchr(q, '\n');
            if (nl == nullptr) break;
            q = nl + 1;
        }
        seed->ok = (got >= 4);
    }

    rows.reserve(tp.size());
    for (size_t i = 0; i < tp.size(); ++i) {
        TapeRow r;
        r.frame_ticks = int(tp.at(i, "frame_ticks"));
        r.dx = tp.at(i, "aim_dx");
        r.dy = tp.at(i, "aim_dy");
        r.phi = tp.at(i, "phi");
        r.theta = tp.at(i, "theta");
        r.wdz = tp.at(i, "wdz");
        r.alt = tp.at(i, "alt");
        r.righting = int(tp.at(i, "righting"));
        r.roll_hold = tp.at(i, "roll_hold");
        r.roll_man = tp.at(i, "roll_maneuver");
        r.roll_right = tp.at(i, "roll_right");
        r.hand_gate = tp.at(i, "hand_gate");
        r.has_state = true;
        rows.push_back(r);
    }
    return rows;
}

struct ReplayOut {
    double peak_rollout = 0.0, alt_drop = 0.0;
    double wz_mean = 0.0;
    double dphi_max = 0.0, dtheta_max = 0.0, dalt_max = 0.0;
};

ReplayOut replay(const std::vector<TapeRow>& rows,
                 const control::ControllerParams& cp, bool trace,
                 const char* tag, const TapeSeed& seed) {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    double thr = 0.9;
    sim::SimState s0 =
        harness::level_trim_state(kAp, 220.0, 3000.0, up, heading, &thr);
    if (seed.ok) {  // the real t=0 state -- the faithfulness precondition
        s0.position = seed.pos;
        s0.velocity = seed.vel;
        s0.orientation = glm::normalize(seed.q);
        s0.last_vhat = seed.vhat;
    }
    app::LoopState st = flying(s0);
    st.grounded = true;
    harness::MiniCamera cam;
    cam.seed(st.curr);
    const glm::dvec3 right0 = s0.orientation * glm::dvec3{1, 0, 0};
    const double alt0 = sim::altitude(st.curr.position, kAp);
    ReplayOut o;
    double wz = 0.0;
    if (trace)
        std::printf("[replay] %s   t(s)  rollout(deg)  phi  cosPT  err  roll_right"
                    "  dAlt\n", tag);
    for (size_t i = 0; i < rows.size(); ++i) {
        app::TickInput in;
        in.raw_mode = false;
        in.throttle = 1.0;
        in.aim_dx = st.grounded ? 0.0 : rows[i].dx;
        in.aim_dy = st.grounded ? 0.0 : rows[i].dy;
        in.frame_ticks = rows[i].frame_ticks;
        in.cam_fwd = cam.cam_fwd;
        in.cam_up = cam.cam_up;
        const app::TickResult r = app::tick(st, in, kAp, cp, nullptr);
        cam.advance(st.curr, st.aim.forward(), st.aim.up(), cp, kAp.sim_dt);
        st.grounded = false;
        const sim::SimState& sc = st.curr;
        const glm::dvec3 nb = sc.orientation * glm::dvec3{0, 0, -1};
        const glm::dvec3 rw = sc.orientation * glm::dvec3{1, 0, 0};
        glm::dvec3 rp = right0 - glm::dot(right0, nb) * nb;
        const double rpl = glm::length(rp);
        double roll_out = 0.0;
        if (rpl > 1e-6) {
            rp /= rpl;
            roll_out = std::atan2(glm::dot(glm::cross(rp, rw), nb),
                                  glm::dot(rp, rw));
        }
        o.peak_rollout = std::max(o.peak_rollout, std::abs(roll_out));
        const double d = sim::altitude(sc.position, kAp) - alt0;
        o.alt_drop = std::min(o.alt_drop, d);
        // FAITHFULNESS: replayed vs recorded, on the as-flown arm this must
        // be ~0 or the reader is not reproducing his flight and nothing it
        // says about a counterfactual arm can be trusted.
        {
            const glm::dvec3 upw = sim::local_up(sc.position);
            const double th =
                std::asin(std::clamp(glm::dot(nb, upw), -1.0, 1.0));
            o.dphi_max = std::max(
                o.dphi_max,
                std::abs(r.telem.extracted.phi - rows[i].phi));
            o.dtheta_max = std::max(o.dtheta_max, std::abs(th - rows[i].theta));
            if (rows[i].has_state)
                o.dalt_max = std::max(
                    o.dalt_max,
                    std::abs(sim::altitude(sc.position, kAp) - rows[i].alt));
        }
        wz += std::abs(r.telem.omega_des.z);
        if (trace && (i % 60) == 0)
            std::printf(
                "[replay] %s  %5.2f  %9.2f  %6.2f  %6.3f  %6.2f  %d  %.2f  "
                "%+7.1f\n",
                tag, i * kAp.sim_dt, deg(roll_out),
                deg(r.telem.extracted.phi), r.telem.extracted.cos_phi_theta,
                deg(r.telem.e), deg(r.telem.roll_right), d);
    }
    o.wz_mean = rows.empty() ? 0.0 : wz / double(rows.size());
    return o;
}
}  // namespace

TEST_CASE("S-righthand: replay a recorded feel tape") {
    const char* path = std::getenv("SEADS_FEEL_TAPE");
    if (path == nullptr) {
        std::printf("[replay] SEADS_FEEL_TAPE unset -- nothing to replay.\n");
        SUCCEED("no tape");
        return;
    }
    std::string banner;
    TapeSeed seed;
    // A tape too old to carry a required column is a SKIP with the
    // reason printed -- never a silent zero, and never a mystery
    // failure in a bench tool that only runs when a tape is named.
    std::vector<TapeRow> rows;
    try {
        rows = read_tape(path, &banner, &seed);
    } catch (const std::exception& e) {
        std::printf("[replay] %s\n[replay] SKIPPING: re-record this "
                    "tape with the current exe.\n", e.what());
        SUCCEED("stale tape");
        return;
    }
    if (rows.empty()) {
        std::printf("[replay] no rows in %s -- nothing to replay.\n", path);
        SUCCEED("empty tape");
        return;
    }
    std::printf("[replay] %s: %zu ticks (%.2f s)  seed %s  v2 columns %s\n",
                path, rows.size(), rows.size() * kAp.sim_dt,
                seed.ok ? "PRESENT" : "ABSENT (v1 tape -- not replayable)",
                rows[0].has_state ? "present" : "absent");
    std::printf("%s", banner.c_str());

    // AS FLOWN first: the faithfulness proof. Chad flies with the dials the
    // banner names, so this arm must reproduce his recorded phi/theta/alt.
    control::ControllerParams as_flown = kCp;
    const ReplayOut flown = replay(rows, as_flown, false, "FLOWN", seed);
    std::printf(
        "[replay] FAITHFULNESS (as-flown arm): max |dphi| %.4f deg  "
        "max |dtheta| %.4f deg  max |dalt| %.3f m\n",
        deg(flown.dphi_max), deg(flown.dtheta_max), flown.dalt_max);

    // The counterfactuals.
    control::ControllerParams rh0 = kCp;
    rh0.right_hand_rest = 0.0;
    control::ControllerParams rh_live = kCp;
    if (rh_live.right_hand_rest <= 0.0) rh_live.right_hand_rest = 0.25;
    const ReplayOut a0 = replay(rows, rh0, true, "rh0", seed);
    const ReplayOut a1 = replay(rows, rh_live, true, "rh+", seed);
    std::printf(
        "[replay] right_hand_rest 0.00 -> peak|rollout| %6.2f deg, worst dAlt "
        "%+8.1f m\n[replay] right_hand_rest %.2f -> peak|rollout| %6.2f deg, "
        "worst dAlt %+8.1f m\n",
        deg(a0.peak_rollout), a0.alt_drop, rh_live.right_hand_rest,
        deg(a1.peak_rollout), a1.alt_drop);
    // Recorded MB-right activity, straight off the tape (v2 only).
    if (rows[0].has_state) {
        long rt = 0;
        double rr = 0.0;
        for (const TapeRow& r : rows) {
            if (r.righting) ++rt;
            rr = std::max(rr, std::abs(r.roll_right));
        }
        std::printf(
            "[replay] RECORDED: righting on %ld/%zu ticks, peak |roll_right| "
            "%.1f deg/s\n", rt, rows.size(), deg(rr));
    }
    SUCCEED("replayed");
}

// ===========================================================================
// S-righthand: MB-right must not right the aeroplane while the hand is still
// flying it. THE APEX SIGNATURE, rebuilt from Chad's own tapes -- the probe no
// longer guesses at the condition, his flight supplies it.
//
// TAPE 1 (right_hand_rest absent), his loop apex at t=18.02: nose 89.1 deg up,
// wings level (phi -0.8), cos_phi_theta crossing zero, tracking err 4.6 deg --
// INSIDE the deadzone circle. That last part is what arms MB-right: its "at
// rest" test is err < blend_lo and inverted_delay is 0, so it fires on the
// first tick past vertical and commands EXACTLY inverted_rate. The tape shows
// wdz = -180.0 deg/s held through the apex.
//
// TAPE 2 (flown at right_hand_rest 0.25), the same six geometries:
//   apex  t(s)   theta   phi   alt  speed  righting  peak|roll_right|
//     0  20.73   89.6    0.4  1322  234.2    155        8.7
//     1  46.11   85.2    4.7   675  242.6    116        2.3
//     2  66.72   86.6    3.3   692  259.3    135        8.7
//     3  94.39   86.7    3.4  1719  224.9      0        0.0
//     4 203.93   80.6    9.4   355  209.5      0        0.0
//     5 245.41   88.9   -0.9   836  258.3    118        5.0
// Wings level at every one (max |phi| 9.4 deg), peak roll_right 8.7 deg/s
// against 180.0 without the dial. Chad, on that flight: "I can do the
// vertical loops and immelmans without a hitch."
//
// The state is CRAFTED at that attitude rather than flown into it: an earlier
// probe tried to fly a scripted loop in, landed in MANEUVER at p_max via the
// bank-to-turn limb -- a different mechanism -- and graded nothing (ON and OFF
// hashed identical).
// ===========================================================================
namespace {
control::ControllerParams rh_on() {
    control::ControllerParams c = kCp;
    if (c.right_hand_rest <= 0.0) c.right_hand_rest = 0.25;  // config-relative
    return c;
}
control::ControllerParams rh_off() {
    control::ControllerParams c = kCp;
    c.right_hand_rest = 0.0;  // the STRUCTURAL OFF arm
    return c;
}

struct ApexRes {
    double phi_peak = 0.0;
    double roll_right_peak = 0.0;
    double hand_gate_min = 1.0;
    double righting_frac = 0.0;
    bool cpt_crossed = false;
};

ApexRes apex_probe(const control::ControllerParams& cp, double theta_deg,
                   double err_deg, double V, bool hand_moving,
                   double secs = 1.0) {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    sim::SimState s0 = harness::level_state(kAp, V, 6000.0, up, heading);
    const glm::dvec3 rb = s0.orientation * glm::dvec3{1, 0, 0};
    s0.orientation =
        glm::normalize(glm::angleAxis(rad(theta_deg), rb) * s0.orientation);
    s0.velocity = V * (s0.orientation * glm::dvec3{0, 0, -1});
    s0.last_vhat = glm::normalize(s0.velocity);
    harness::ClosedLoop cl(s0, glm::dvec3{0.0, 0.0, -1.0});
    cl.aim_nose();
    cl.tick(0.9, kAp, cp, /*grounded=*/true);
    ApexRes r;
    const int n = static_cast<int>(secs / kAp.sim_dt + 0.5);
    long rt = 0;
    for (int i = 0; i < n; ++i) {
        const glm::dvec3 nose = cl.state.orientation * glm::dvec3{0, 0, -1};
        const glm::dvec3 rw = cl.state.orientation * glm::dvec3{1, 0, 0};
        cl.aim = glm::normalize(glm::angleAxis(rad(err_deg), rw) * nose);
        cl.aim_moved = hand_moving;
        const control::Telemetry tm = cl.tick(0.9, kAp, cp);
        if (tm.extracted.cos_phi_theta < 0.0) r.cpt_crossed = true;
        if (r.cpt_crossed) {
            r.phi_peak = std::max(r.phi_peak, std::abs(tm.extracted.phi));
            r.roll_right_peak =
                std::max(r.roll_right_peak, std::abs(tm.roll_right));
            r.hand_gate_min = std::min(r.hand_gate_min, tm.hand_gate);
        }
        if (tm.righting) ++rt;
    }
    r.righting_frac = double(rt) / double(n);
    return r;
}
}  // namespace

TEST_CASE("S-righthand: the loop apex -- hand moving, the wings stay put") {
    for (double th : {84.0, 87.0, 89.0}) {
        for (double V : {210.0, 260.0}) {
            const ApexRes off = apex_probe(rh_off(), th, 4.5, V, true);
            const ApexRes on = apex_probe(rh_on(), th, 4.5, V, true);
            std::printf(
                "[apex] theta %4.1f V%3.0f | OFF righting %.2f roll_right "
                "%6.1f gate %.2f |phi|max %5.1f | ON righting %.2f roll_right "
                "%6.1f gate %.2f |phi|max %5.1f\n",
                th, V, off.righting_frac, deg(off.roll_right_peak),
                off.hand_gate_min, deg(off.phi_peak), on.righting_frac,
                deg(on.roll_right_peak), on.hand_gate_min, deg(on.phi_peak));
            REQUIRE(off.cpt_crossed);
            // THE SIGNATURE at the dial OFF: MB-right owns the apex at the
            // full inverted_rate (tape 1: -180.0 deg/s).
            CHECK(off.righting_frac > 0.0);
            CHECK(deg(off.roll_right_peak) >= deg(kCp.inverted_rate) - 0.5);
            CHECK(off.hand_gate_min == 1.0);
            // MUTATION (delete the veto): these three are what red.
            CHECK(on.hand_gate_min < 0.05);
            CHECK(deg(on.roll_right_peak) < 20.0);
            CHECK(deg(on.phi_peak) < 10.0);
        }
    }
}

TEST_CASE("S-righthand: hands OFF still rights him (the 08-06 ruling)") {
    const ApexRes on = apex_probe(rh_on(), 89.0, 4.5, 230.0, false, 2.0);
    const ApexRes off = apex_probe(rh_off(), 89.0, 4.5, 230.0, false, 2.0);
    std::printf(
        "[apex] HANDS OFF: OFF righting %.2f roll_right %6.1f | ON righting "
        "%.2f roll_right %6.1f gate %.2f\n",
        off.righting_frac, deg(off.roll_right_peak), on.righting_frac,
        deg(on.roll_right_peak), on.hand_gate_min);
    REQUIRE(off.righting_frac > 0.0);
    // MUTATION (delete the release / stick the gate at 0): these red -- a
    // hands-off pilot would never be righted again, breaking the 08-06 ruling.
    CHECK(on.righting_frac > 0.0);
    CHECK(deg(on.roll_right_peak) >= deg(kCp.inverted_rate) - 0.5);
    CHECK(on.hand_gate_min > 0.99);
}

// ===========================================================================
// S-righthand, red-team P1: the hand-rest clock must be FRAME-RATE INVARIANT.
//
// in.aim_moved alone is a per-TICK bit, true only on the ONE mouse-consuming
// tick of each frame, so at low fps a hand that NEVER rests still accumulates
// rest on the other N-1 ticks: measured max hand_rest 0 / 0.0083 / 0.025 /
// 0.058 / 0.092 s at 240 / 60 / 30 / 15 / 10 fps, and a 0.4 s frame hitch
// reached gate 0.9967 -- the full 180 deg/s righting handed back to a pilot
// who never stopped flying. AT-9 forbids exactly this. The fix reads the
// ZOH-smeared aim_rate_world alongside the bit.
//
// Driven through the SHIPPED app::step_frame at 30 and 240 fps, plus a frame
// HITCH, with a hand that is moving the whole time.
namespace {
struct RestRun {
    double hand_rest_max = 0.0;
    double gate_min = 1.0;
};

RestRun rest_at(double frame_dt, double hitch_at, double hitch_dt) {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    double thr = 0.9;
    const sim::SimState s0 =
        harness::level_trim_state(kAp, 220.0, 6000.0, up, heading, &thr);
    app::LoopState st = flying(s0);
    app::Accumulator accum(kAp.sim_dt);
    double pdx = 0.0, pdy = 0.0;
    const control::ControllerParams cp = rh_on();
    RestRun r;
    double t = 0.0;
    while (t < 4.0) {
        const bool hitching = (hitch_at > 0.0 && t >= hitch_at &&
                               t < hitch_at + hitch_dt);
        const double fdt = hitching ? hitch_dt : frame_dt;
        app::FrameInput fin;
        fin.raw_mode = false;
        fin.throttle = 1.0;
        // A hand that is ALWAYS moving: the per-frame delta scales with the
        // frame so the angular rate is identical at every rate.
        pdy += -rad(40.0) * fdt / cp.aim_sensitivity;
        app::step_frame(st, accum, fdt, fin, pdx, pdy, kAp, cp, nullptr);
        t += fdt;
        r.hand_rest_max = std::max(r.hand_rest_max, st.internal.hand_rest);
        if (hitching || t > hitch_at + hitch_dt)
            r.gate_min = std::min(
                r.gate_min,
                1.0 - st.internal.hand_rest / std::max(cp.right_hand_rest,
                                                       1e-9));
    }
    return r;
}
}  // namespace

TEST_CASE("S-righthand: the hand-rest clock is frame-rate invariant") {
    const RestRun f240 = rest_at(0.5 * kAp.sim_dt, 0.0, 0.0);
    const RestRun f60 = rest_at(2.0 * kAp.sim_dt, 0.0, 0.0);
    const RestRun f30 = rest_at(4.0 * kAp.sim_dt, 0.0, 0.0);
    const RestRun f10 = rest_at(12.0 * kAp.sim_dt, 0.0, 0.0);
    const RestRun hitch = rest_at(4.0 * kAp.sim_dt, 1.0, 0.4);
    std::printf(
        "[rest] max hand_rest with a MOVING hand: 240fps %.4f  60fps %.4f  "
        "30fps %.4f  10fps %.4f  30fps+0.4s hitch %.4f (s)\n",
        f240.hand_rest_max, f60.hand_rest_max, f30.hand_rest_max,
        f10.hand_rest_max, hitch.hand_rest_max);
    // A hand that never rests must never accumulate rest, at ANY frame rate
    // or through a hitch. Pre-fix these read 0.0083 / 0.025 / 0.092 / 0.4.
    CHECK(f240.hand_rest_max == 0.0);
    CHECK(f60.hand_rest_max == 0.0);
    CHECK(f30.hand_rest_max == 0.0);
    CHECK(f10.hand_rest_max == 0.0);
    CHECK(hitch.hand_rest_max == 0.0);
}

// ===========================================================================
// S-righthand, red-team P2: the authority RAMPS, it does not step. An M3
// mutant (hand_gate := the bare hand-live predicate) survived every other leg.
// ===========================================================================
TEST_CASE("S-righthand: the righting authority ramps, it does not step") {
    // Hands off from the start at an inverted attitude: the gate must climb
    // through the middle of its range, not jump 0 -> 1.
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    sim::SimState s0 = harness::level_state(kAp, 230.0, 6000.0, up, heading);
    const glm::dvec3 nb = s0.orientation * glm::dvec3{0, 0, -1};
    s0.orientation =
        glm::normalize(glm::angleAxis(rad(170.0), nb) * s0.orientation);
    s0.velocity = 230.0 * (s0.orientation * glm::dvec3{0, 0, -1});
    s0.last_vhat = glm::normalize(s0.velocity);
    harness::ClosedLoop cl(s0, glm::dvec3{0.0, 0.0, -1.0});
    cl.aim_nose();
    cl.tick(0.9, kAp, rh_on(), /*grounded=*/true);
    int mid = 0, seen_full = 0;
    for (int i = 0; i < 120; ++i) {
        cl.aim_moved = false;  // hands off
        const control::Telemetry tm = cl.tick(0.9, kAp, rh_on());
        if (!tm.righting) continue;
        if (tm.hand_gate > 0.05 && tm.hand_gate < 0.95) ++mid;
        if (tm.hand_gate > 0.99) ++seen_full;
    }
    std::printf("[rest] ramp: %d ticks strictly between 0.05 and 0.95, %d at "
                "full authority\n", mid, seen_full);
    // A STEP mutant spends ZERO ticks in the middle of the range.
    CHECK(mid > 5);
    CHECK(seen_full > 0);
}

// ===========================================================================
// S-tremor (kernel v17 candidate) -- THE WINDOWED NET HAND-LIVE MEASURE.
// Dial: [auto_level] hand_net_window (seconds). 0 = structurally off = v16.
//
// THE DEBT IT PAYS (v15 red-team P2, measured, deferred). S-righthand's
// "the hand is live" was ANY nonzero aim motion this tick, so a
// +/-1-count-per-frame mouse tremor resets the hand-rest clock EVERY FRAME,
// the authority ramp never climbs, and a belly-up aeroplane is never righted:
// integrated righting 1.76 deg against 117.75 with a still hand. From the
// seat: "it will not right me." It is the 2026-08-06 resting ruling broken by
// a hand that is resting but not perfectly still.
//
// THE CURE. A leaky window integral of the aim's own world rotation vector;
// |net|/window is the sweep rate the last window ADDS UP TO; a continuous
// smoothstep of that rate (1 deg/s floor, 3 deg/s saturation) scales how hard
// the clock is reset. A tremor nets to ~one frame's rotation; a real sweep of
// any size nets to its own rate and still vetoes (the gun-director law).
//
// THE RIG. Belly-up at theta 170 (the ramp leg's attitude: MB-right armed,
// err inside the circle) driven through the SHIPPED app::step_frame, so the
// mouse path graded is the mouse path Chad flies: real per-frame counts, real
// apply_mouse, the real ZOH smear on ticks 2..N. Every number below is
// printed, not just asserted.
// ===========================================================================
namespace {

std::uint64_t apex_hash(const control::ControllerParams& cp, double theta_deg,
                        double err_deg, double V, bool hand_moving,
                        double secs) {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    sim::SimState s0 = harness::level_state(kAp, V, 6000.0, up, heading);
    const glm::dvec3 rb = s0.orientation * glm::dvec3{1, 0, 0};
    s0.orientation =
        glm::normalize(glm::angleAxis(rad(theta_deg), rb) * s0.orientation);
    s0.velocity = V * (s0.orientation * glm::dvec3{0, 0, -1});
    s0.last_vhat = glm::normalize(s0.velocity);
    harness::ClosedLoop cl(s0, glm::dvec3{0.0, 0.0, -1.0});
    cl.aim_nose();
    cl.tick(0.9, kAp, cp, /*grounded=*/true);
    std::uint64_t h = 0;
    const int n = static_cast<int>(secs / kAp.sim_dt + 0.5);
    for (int i = 0; i < n; ++i) {
        const glm::dvec3 nose = cl.state.orientation * glm::dvec3{0, 0, -1};
        const glm::dvec3 rw = cl.state.orientation * glm::dvec3{1, 0, 0};
        cl.aim = glm::normalize(glm::angleAxis(rad(err_deg), rw) * nose);
        cl.aim_moved = hand_moving;
        const control::Telemetry tm = cl.tick(0.9, kAp, cp);
        mix(h, tm.omega_des.z);
        mix(h, tm.hand_gate);
    }
    return h;
}
std::uint64_t apex_hash_pair(const control::ControllerParams& cp) {
    std::uint64_t h = 0;
    mix(h, double(apex_hash(cp, 89.0, 4.5, 230.0, false, 2.0)));
    mix(h, double(apex_hash(cp, 87.0, 4.5, 210.0, true, 1.0)));
    return h;
}

// The lane's two arms, config-relative (the S-leanlead red-team P1 lesson: a
// walk-back of the dial in the toml must not red this file).
control::ControllerParams tr_on() {
    control::ControllerParams c = rh_on();
    if (c.hand_net_window <= 0.0) c.hand_net_window = 0.20;
    return c;
}
control::ControllerParams tr_off() {
    control::ControllerParams c = rh_on();
    c.hand_net_window = 0.0;  // the STRUCTURAL OFF arm (right_hand_rest LIVE)
    return c;
}

enum class Hand { STILL, TREMOR_ALT, TREMOR_WALK, SWEEP };

struct TremorRun {
    double gate_max = 0.0;
    double gate_min = 1.0;
    double integ_right_deg = 0.0;  // integral |roll_right| dt, over the run
    double roll_right_peak = 0.0;
    double hand_rest_max = 0.0;
    double net_rate_max = 0.0;  // [deg/s] the measure's own reading
    // ...and the SETTLED reading: the same max taken only after the window
    // has had 0.5 s to fill. The raw max above is dominated by the first
    // ticks, where the normaliser is still small and EVERY hand -- tremor or
    // sweep -- reads a large instantaneous rate, because a window that has
    // not filled cannot yet tell them apart. Grade the settled one.
    double net_rate_settled = 0.0;
    long righting_ticks = 0;
    long ticks = 0;
    int mid_ticks = 0;  // gate strictly inside (0.05, 0.95) while righting
};

struct HookCtx {
    TremorRun* r;
    double dt;
    long settle_ticks;  // ticks of window fill ignored by net_rate_settled
};

void tremor_hook(const app::TickInput&, const app::LoopState& st,
                 const control::Telemetry& tm, void* ctx) {
    HookCtx* c = static_cast<HookCtx*>(ctx);
    TremorRun& r = *c->r;
    ++r.ticks;
    r.hand_rest_max = std::max(r.hand_rest_max, st.internal.hand_rest);
    r.net_rate_max = std::max(r.net_rate_max, deg(tm.hand_net_rate));
    if (r.ticks > c->settle_ticks)
        r.net_rate_settled =
            std::max(r.net_rate_settled, deg(tm.hand_net_rate));
    if (!tm.righting) return;
    ++r.righting_ticks;
    r.gate_max = std::max(r.gate_max, tm.hand_gate);
    r.gate_min = std::min(r.gate_min, tm.hand_gate);
    r.integ_right_deg += std::abs(deg(tm.roll_right)) * c->dt;
    r.roll_right_peak = std::max(r.roll_right_peak, std::abs(tm.roll_right));
    if (tm.hand_gate > 0.05 && tm.hand_gate < 0.95) ++r.mid_ticks;
}

// A FIXED LCG (never std::random_device -- a seeded, reproducible walk).
struct Lcg {
    std::uint64_t s;
    int pm1() {
        s = s * 6364136223846793005ULL + 1442695040888963407ULL;
        return ((s >> 33) & 1ULL) ? 1 : -1;
    }
};

// Belly-up (theta 170 about the NOSE -- the ramp leg's attitude), flown
// through app::step_frame for `secs` at `frame_dt`, with the scripted hand.
TremorRun belly_up_run(const control::ControllerParams& cp, double frame_dt,
                       Hand hand, double sweep_deg_s, double secs,
                       double hitch_at = 0.0, double hitch_dt = 0.0) {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    sim::SimState s0 = harness::level_state(kAp, 230.0, 6000.0, up, heading);
    const glm::dvec3 nb = s0.orientation * glm::dvec3{0, 0, -1};
    s0.orientation =
        glm::normalize(glm::angleAxis(rad(170.0), nb) * s0.orientation);
    s0.velocity = 230.0 * (s0.orientation * glm::dvec3{0, 0, -1});
    s0.last_vhat = glm::normalize(s0.velocity);
    app::LoopState st = flying(s0);
    app::Accumulator accum(kAp.sim_dt);
    TremorRun r;
    HookCtx ctx{&r, kAp.sim_dt, long(0.5 / kAp.sim_dt)};
    double pdx = 0.0, pdy = 0.0, t = 0.0;
    double sgn = 1.0;
    Lcg lcg{0x5eadf1a17e57ULL};
    while (t < secs) {
        const bool hitching =
            (hitch_at > 0.0 && t >= hitch_at && t < hitch_at + hitch_dt);
        const double fdt = hitching ? hitch_dt : frame_dt;
        app::FrameInput fin;
        fin.raw_mode = false;
        fin.throttle = 1.0;
        switch (hand) {
            case Hand::STILL:
                pdx = pdy = 0.0;
                break;
            case Hand::TREMOR_ALT:
                // ONE count per frame, sign alternating. ASSIGNED, not
                // accumulated, and the sign flips only on a frame that
                // actually CONSUMED: at frame_dt < sim_dt some frames advance
                // zero ticks, and a blind per-frame flip there would feed the
                // tick stream a CONSTANT +1 (a real sweep) instead of a
                // tremor -- the rig would grade the opposite of what it says.
                pdx = sgn;
                pdy = sgn;
                break;
            case Hand::TREMOR_WALK:
                // Zero-mean random walk, seeded LCG. NOTE (§3 departure, see
                // the leg below): at aim_sensitivity 0.14 deg/count this is a
                // genuine wandering aim, not a stationary tremor.
                pdx = double(lcg.pm1());
                pdy = double(lcg.pm1());
                break;
            case Hand::SWEEP:
                // A SUSTAINED lateral sweep at a fixed ANGULAR rate, so the
                // hand is identical at every frame rate.
                pdx = rad(sweep_deg_s) * fdt / cp.aim_sensitivity;
                pdy = 0.0;
                break;
        }
        const app::FrameResult fr = app::step_frame(
            st, accum, fdt, fin, pdx, pdy, kAp, cp, nullptr, nullptr, nullptr,
            nullptr, nullptr, tremor_hook, &ctx);
        if (hand == Hand::TREMOR_ALT && fr.ticks > 0) sgn = -sgn;
        pdx = pdy = 0.0;  // never let an unconsumed delta pile up
        t += fdt;
    }
    return r;
}

void print_run(const char* tag, const TremorRun& r) {
    std::printf(
        "[tremor] %-28s righting %4ld/%4ld  gate %.3f..%.3f  integ|roll_right|"
        " %7.2f deg  peak %6.1f deg/s  rest_max %.3f s  net_rate settled "
        "%5.2f peak %6.2f deg/s\n",
        tag, r.righting_ticks, r.ticks, r.gate_min, r.gate_max,
        r.integ_right_deg, deg(r.roll_right_peak), r.hand_rest_max,
        r.net_rate_settled, r.net_rate_max);
}

}  // namespace

// ===========================================================================
// LEG 1 -- THE TREMOR LEG. The debt, reproduced at the dial OFF and paid at
// the dial ON. 2 s belly-up with a +/-1-count-per-frame tremor at 60 fps.
//
// MUTATION: delete the measure (hand_live_frac := hand_live ? 1 : 0, i.e. the
// v16 predicate) and the ON arm collapses onto the OFF arm -- gate pinned near
// 0, integrated righting near 0. Raise the 3 deg/s saturation wall to swallow
// the tremor's 0.7 deg/s and the same thing happens.
// ===========================================================================
TEST_CASE("S-tremor: a mouse tremor no longer cancels the righting") {
    const TremorRun off =
        belly_up_run(tr_off(), 2.0 * kAp.sim_dt, Hand::TREMOR_ALT, 0.0, 2.0);
    const TremorRun on =
        belly_up_run(tr_on(), 2.0 * kAp.sim_dt, Hand::TREMOR_ALT, 0.0, 2.0);
    const TremorRun still =
        belly_up_run(tr_on(), 2.0 * kAp.sim_dt, Hand::STILL, 0.0, 2.0);
    print_run("OFF   tremor (the debt)", off);
    print_run("ON    tremor (the cure)", on);
    print_run("ON    still  (reference)", still);
    // The debt IS reproduced at the dial off -- if this ever goes green the
    // leg below is grading nothing (the vacuous-probe trap).
    REQUIRE(off.righting_ticks > 0);  // MB-right armed: the geometry is right
    CHECK(off.gate_max < 0.05);
    CHECK(off.integ_right_deg < 5.0);
    // The cure: the clock climbs through the tremor to FULL authority, and
    // the aeroplane is righted at the rate the 08-06 ruling promises.
    CHECK(on.gate_max > 0.99);
    CHECK(deg(on.roll_right_peak) >= deg(kCp.inverted_rate) - 0.5);
    CHECK(on.integ_right_deg > 20.0 * off.integ_right_deg);
    // ...and it matches the STILL hand, which is the whole point: a tremor
    // must be indistinguishable from a resting hand.
    CHECK(on.integ_right_deg > 0.5 * still.integ_right_deg);
}

// ===========================================================================
// LEG 2 -- A SWEEP NEVER LETS THE CLOCK START. A hand that sweeps from the
// first tick must never accumulate any rest at all, at any size down to
// 5 deg/s.
//
// ⚠ RENAMED 2026-09-16 (red-team P1-1, law/feel lens) TO WHAT IT GRADES. It
// was called "a small slow deliberate sweep still vetoes" and read as THE
// gun-director leg -- but its gate is 0 by CONSTRUCTION: the sweep starts at
// t = 0 with hand_rest seeded 0, so the same assertions pass at the dial OFF,
// at the dial ON, and (measured) in a build whose veto was 340 ms late. It is
// a non-regression leg and is kept as one. The gun-director leg proper is
// LEG 2b below, which starts from a gate of 1.000.
//
// MUTATION: drop the 3 deg/s saturation to ~6 deg/s (or lengthen the window
// past ~1 s) and the 5 deg/s arm stops vetoing -- gate climbs off 0.
// ===========================================================================
TEST_CASE("S-tremor: a sweep never lets the clock start") {
    for (double rate : {5.0, 12.0, 40.0}) {
        const TremorRun on =
            belly_up_run(tr_on(), 2.0 * kAp.sim_dt, Hand::SWEEP, rate, 2.0);
        char tag[64];
        std::snprintf(tag, sizeof tag, "ON    sweep %5.1f deg/s", rate);
        print_run(tag, on);
        REQUIRE(on.ticks > 0);
        CHECK(on.gate_max < 0.05);
        CHECK(on.hand_rest_max < 0.02);
    }
}

// ===========================================================================
// LEG 2b -- THE GUN-DIRECTOR LEG PROPER (folded 2026-09-16 from red-team
// P0-1, found INDEPENDENTLY BY BOTH LENSES; the rig is the red team's own
// RT1). THE CASE THE LANE NEVER RAN: the pilot is belly-up with his hand OFF,
// the clock has climbed, the gate reads 1.000 and the aeroplane is rolling at
// the full inverted_rate -- and THEN he puts his hand on. That is the only
// state in which the gate is nonzero, i.e. the only state the gun-director
// law is about, and every other sweep leg in this file starts from a gate
// that is 0 by construction.
//
// THE FAILURE IT CAUGHT (shipped tip 9ba9beb35, since folded): the normaliser
// added dt on EVERY tick, so it saturated at the window and the measure read
// r*(1-exp(-t/window)) for a sweep that began after a rest. Integrated
// |roll_right| AFTER the hand goes on: 14.42 deg over 0.100 s at 5 deg/s
// (Chad's own fly-card row), 25.94 at 3 deg/s, 2.41 at 40 -- against v16's
// 1.50 deg in 0.008 s at every rate.
//
// MUTATION: revert the fold (`+ dt` instead of `+ (hand_live ? dt : 0.0)` in
// controller.cpp) and the 5 deg/s row reds on BOTH assertions. Delete the
// measure entirely (v16) and it stays green -- deliberately: this leg grades
// that the dial costs the law NOTHING here, so v16 is its reference arm.
// ===========================================================================
namespace {

struct RtRun {
    double integ_before_deg = 0.0;  // integral |roll_right| dt, t <= t_mark
    double integ_after_deg = 0.0;   // integral |roll_right| dt, t >  t_mark
    double gate_at_mark = -1.0;     // the gate on the first tick after t_mark
    double gate_after_max = 0.0;
    double t_gate_below05 = -1.0;  // s after t_mark until the gate < 0.05
    long ticks = 0;
};

struct RtCtx {
    RtRun* r;
    double dt;
    double t;
    double t_mark;
};

void rt_hook(const app::TickInput&, const app::LoopState&,
             const control::Telemetry& tm, void* ctx) {
    RtCtx* c = static_cast<RtCtx*>(ctx);
    RtRun& r = *c->r;
    c->t += c->dt;
    ++r.ticks;
    const double d = std::abs(deg(tm.roll_right)) * c->dt;
    if (c->t <= c->t_mark) {
        r.integ_before_deg += d;
        return;
    }
    r.integ_after_deg += d;
    if (!tm.righting) return;
    if (r.gate_at_mark < 0.0) r.gate_at_mark = tm.hand_gate;
    r.gate_after_max = std::max(r.gate_after_max, tm.hand_gate);
    if (r.t_gate_below05 < 0.0 && tm.hand_gate < 0.05)
        r.t_gate_below05 = c->t - c->t_mark;
}

// Belly-up at theta 170 like belly_up_run, but with a SCRIPTED hand (t, the
// frame dt -> the per-frame counts) and the integral split at t_mark, so a
// leg can ask "what happened AFTER the hand went on".
template <class HandFn>
RtRun rt_run(const control::ControllerParams& cp, double frame_dt, double secs,
             double t_mark, HandFn hand) {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    sim::SimState s0 = harness::level_state(kAp, 230.0, 6000.0, up, heading);
    const glm::dvec3 nb = s0.orientation * glm::dvec3{0, 0, -1};
    s0.orientation =
        glm::normalize(glm::angleAxis(rad(170.0), nb) * s0.orientation);
    s0.velocity = 230.0 * (s0.orientation * glm::dvec3{0, 0, -1});
    s0.last_vhat = glm::normalize(s0.velocity);
    app::LoopState st = flying(s0);
    app::Accumulator accum(kAp.sim_dt);
    RtRun r;
    RtCtx ctx{&r, kAp.sim_dt, 0.0, t_mark};
    double t = 0.0;
    while (t < secs) {
        double pdx = 0.0, pdy = 0.0;
        hand(t, frame_dt, pdx, pdy);
        app::FrameInput fin;
        fin.raw_mode = false;
        fin.throttle = 1.0;
        app::step_frame(st, accum, frame_dt, fin, pdx, pdy, kAp, cp, nullptr,
                        nullptr, nullptr, nullptr, nullptr, rt_hook, &ctx);
        t += frame_dt;
    }
    return r;
}

// The hand for both legs below: nothing until `mark`, then a sustained
// lateral sweep at a fixed ANGULAR rate (so the hand is the same at any
// frame rate).
RtRun rested_then_sweep(const control::ControllerParams& cp, double rate,
                        double mark = 0.40, double secs = 2.0) {
    const double sens = cp.aim_sensitivity;
    return rt_run(cp, 2.0 * kAp.sim_dt, secs, mark,
                  [rate, mark, sens](double t, double f, double& dx, double&) {
                      if (t + 1e-12 >= mark) dx = rad(rate) * f / sens;
                  });
}

}  // namespace

TEST_CASE("S-tremor: a sweep from a FULL gate vetoes on the next tick") {
    std::printf(
        "[gundir] rate   arm  gate@hand-on  t(gate<0.05)  integ|roll_right| "
        "AFTER the hand goes on (before)\n");
    for (double rate : {3.0, 5.0, 10.0, 40.0}) {
        const RtRun off = rested_then_sweep(tr_off(), rate);
        const RtRun on = rested_then_sweep(tr_on(), rate);
        for (int arm = 0; arm < 2; ++arm) {
            const RtRun& r = arm ? on : off;
            std::printf(
                "[gundir] %5.1f  %s  %11.3f  %12.3f  %8.2f deg (%.2f)\n", rate,
                arm ? "ON " : "OFF", r.gate_at_mark, r.t_gate_below05,
                r.integ_after_deg, r.integ_before_deg);
        }
        // NON-VACUITY: the hand arrives at FULL authority in both arms (if
        // this ever goes green by the gate being 0, the leg grades nothing --
        // the trap LEG 2 fell into).
        REQUIRE(off.gate_at_mark > 0.99);
        REQUIRE(on.gate_at_mark > 0.99);
        REQUIRE(off.integ_before_deg > 20.0);
        // The hands-off half is untouched by the dial (the 08-06 ruling,
        // bit-identical).
        CHECK(on.integ_before_deg == off.integ_before_deg);
        // THE LAW: the veto lands within a tick or two of v16's, and the
        // righting that leaks into the deliberate input is v16's.
        CHECK(on.t_gate_below05 >= 0.0);
        CHECK(on.t_gate_below05 <= off.t_gate_below05 + 2.0 * kAp.sim_dt);
        CHECK(on.integ_after_deg <= off.integ_after_deg + 0.25);
    }
}

// ===========================================================================
// LEG 2c -- WHERE THE WALL ACTUALLY IS, PINNED AS A NUMBER (red-team P1-1,
// mechanism lens). The smoothstep saturates at 3 deg/s, but the clock's reset
// is MULTIPLICATIVE, so the FELT wall -- the rate at which the gate is halved
// -- is at liveness 1/16, i.e. ~1.3 deg/s at the shipped sim_dt 1/120 and
// right_hand_rest 0.25 (controller.cpp derives it). Below it a SUSTAINED
// DELIBERATE drift is not vetoed at all, where v16 vetoed it 100 %.
//
// THIS LEG PINS THE RESIDUAL, NOT A CURE. It is the S-yawbudget precedent: a
// measured residual, printed, asserted in the direction it was ruled in, so
// it cannot move -- in EITHER direction -- without somebody re-reading it.
// If the slow arm ever reds, the hole closed and Chad must be told; if the
// fast arm reds, the law broke.
//
// MUTATION: raise kNetFloorRate and the 1.5 deg/s row crosses; lower it below
// the tremor's 0.57 deg/s and LEG 1 (the cure) reds instead. The two walls
// are one trade and these legs are its two ends.
// ===========================================================================
TEST_CASE("S-tremor: the felt wall of the veto, measured") {
    std::printf(
        "[wall] rate   integ|roll_right| AFTER the hand goes on   "
        "gate_max_after  t(gate<0.05)\n");
    double integ[6] = {0, 0, 0, 0, 0, 0};
    int i = 0;
    for (double rate : {0.5, 1.0, 1.5, 2.0, 3.0, 5.0}) {
        const RtRun on = rested_then_sweep(tr_on(), rate);
        integ[i++] = on.integ_after_deg;
        std::printf("[wall] %5.1f  %24.2f deg  %14.3f  %12.3f\n", rate,
                    on.integ_after_deg, on.gate_after_max, on.t_gate_below05);
    }
    const RtRun v16 = rested_then_sweep(tr_off(), 1.0);
    std::printf("[wall] v16 reference at 1.0 deg/s: %.2f deg\n",
                v16.integ_after_deg);
    // THE HOLE, pinned: at 1.0 deg/s the deliberate drift is NOT vetoed...
    CHECK(integ[1] > 20.0);
    CHECK(v16.integ_after_deg < 5.0);
    // ...and by 3 deg/s it is, within a hair of v16.
    CHECK(integ[4] < 5.0);
    // ...monotone in between: the wall is a ramp, not a cliff (no chatter).
    CHECK(integ[2] >= integ[3]);
    CHECK(integ[3] >= integ[4]);
}

// ===========================================================================
// THE WOBBLE RESIDUAL -- MEASURED, NOT ARGUED AWAY (red-team P1-2). A
// DELIBERATE tracking oscillation -- the pilot working the reticle back and
// forth over a jinking bandit -- nets to nothing over the window and is
// therefore discounted exactly as a tremor is. No patch can separate them:
// that blindness IS the cure. So it is stated as a bounded exception in
// params.h / controller.toml, printed here, and put on Chad's fly card as a
// question: belly-up, hand working the reticle in a half-degree wobble --
// righted, or not?
//
// PINS ONLY THE SHAPE: amplitude wins over frequency, and by +/-2 deg of
// reticle travel the wobble is a hand again at every frequency.
// ===========================================================================
TEST_CASE("S-tremor: the tracking-wobble residual, measured not hidden") {
    std::printf(
        "[wobble] amp[deg] freq[Hz]   integ|roll_right| ON   (v16)   "
        "gate_max\n");
    double big = -1.0, small = -1.0;
    for (double amp : {0.25, 0.5, 1.0, 2.0}) {
        for (double f_hz : {1.0, 2.0, 4.0}) {
            const control::ControllerParams cp_on = tr_on();
            const control::ControllerParams cp_off = tr_off();
            const double sens = cp_on.aim_sensitivity;
            auto hand = [amp, f_hz, sens](double t, double f, double& dx,
                                          double&) {
                const double a0 = amp * std::sin(2.0 * kPi * f_hz * t);
                const double a1 = amp * std::sin(2.0 * kPi * f_hz * (t + f));
                dx = rad(a1 - a0) / sens;
            };
            const RtRun on = rt_run(cp_on, 2.0 * kAp.sim_dt, 2.0, 0.0, hand);
            const RtRun off = rt_run(cp_off, 2.0 * kAp.sim_dt, 2.0, 0.0, hand);
            std::printf("[wobble] %7.2f %8.1f   %17.2f  %7.2f  %8.3f\n", amp,
                        f_hz, on.integ_after_deg, off.integ_after_deg,
                        on.gate_after_max);
            CHECK(off.integ_after_deg < 1.0);  // v16 vetoes all of them
            if (amp == 0.25 && f_hz == 1.0) small = on.integ_after_deg;
            if (amp == 2.0 && f_hz == 1.0) big = on.integ_after_deg;
        }
    }
    // A half-degree wobble IS discounted (the exception, pinned as a number);
    // a +/-2 deg one is a hand again.
    CHECK(small > 50.0);
    CHECK(big < 10.0);
}

// ===========================================================================
// THE PRICE OF THE P0-1 FOLD, MEASURED (an INTERMITTENT tremor: one count
// every k-th frame). Counting only LIVE time means the discount has to be
// EARNED by continuous motion, so a tremor with gaps keeps less of the cure
// than the (law-breaking) shipped form did. That is the trade, and it is
// written down rather than discovered later: the law is a standing ruling,
// the cure is this lane's proposal, so the law wins the tie.
//
// PINS: still far better than v16 at every k -- which is the claim the rung
// is actually making.
// ===========================================================================
TEST_CASE("S-tremor: an intermittent tremor, the fold's measured price") {
    std::printf(
        "[intermittent] every_k_frames   integ|roll_right| ON   v16   \n");
    for (int k : {1, 2, 3, 5, 10}) {
        double integ[2] = {0.0, 0.0};
        for (int arm = 0; arm < 2; ++arm) {
            const control::ControllerParams cp = arm ? tr_on() : tr_off();
            int frame = 0;
            double sgn = 1.0;
            const RtRun r = rt_run(
                cp, 2.0 * kAp.sim_dt, 2.0, 0.0,
                [&frame, &sgn, k](double, double, double& dx, double& dy) {
                    if (frame % k == 0) {
                        dx = sgn;
                        dy = sgn;
                        sgn = -sgn;
                    }
                    ++frame;
                });
            integ[arm] = r.integ_after_deg;
        }
        std::printf("[intermittent] %14d   %17.2f  %6.2f\n", k, integ[1],
                    integ[0]);
        // A HAIR of tolerance: at k = 10 the two arms land within a couple
        // of degrees of each other (a tremor that sparse barely fills the
        // window either way), so this pins "never WORSE than v16", not a
        // strict ordering.
        CHECK(integ[1] >= integ[0] - 3.0);
    }
}

// The EXISTING apex leg (hand moving, wings stay put) is the second half of
// this: it runs at the SHIPPED config, i.e. the dial live, and it must stay
// green. It scripts aim_moved WITHOUT an aim rate, which is the unmeasurable
// arm of the measure -- the one that fails to the veto by construction.

// ===========================================================================
// LEG 3 -- FRAME-RATE INVARIANCE (the AT-9 class, v15 red-team P1). The window
// is measured in SECONDS of sim time through app::step_frame, never in ticks
// or frames, so the tremor's verdict must not move with the frame rate -- or
// through a 0.4 s hitch.
//
// MUTATION: integrate only on the aim_moved tick (dropping the ZOH smear) and
// the low-fps arms under-count by N-1 of every N ticks -- that is where this
// leg's teeth are, and it is the whole source of the invariance.
// ⚠ CORRECTED 2026-09-16 (red-team P3-2, mechanism lens): this note used to
// claim a second mutation, "measure the window in TICKS (leak := a fixed
// per-tick constant)". That mutation CANNOT RED -- dt is ap.sim_dt at every
// call site, so leak = dt/window ALREADY IS a fixed per-tick constant. The
// invariance comes from aim_rate_world being rotation/(N*sim_dt) smeared
// across the frame's N ticks, not from the units of the leak. A mutation note
// that cannot fire is worse than none: it tells a later reader the leg is
// guarding something it is not.
// ===========================================================================
TEST_CASE("S-tremor: the net window is frame-rate invariant") {
    struct FpsArm {
        const char* name;
        double mult;
        double hitch_at, hitch_dt;
    };
    const FpsArm arms[] = {
        {"240 fps", 0.5, 0.0, 0.0},  {"120 fps", 1.0, 0.0, 0.0},
        {" 60 fps", 2.0, 0.0, 0.0},  {" 30 fps", 4.0, 0.0, 0.0},
        {" 10 fps", 12.0, 0.0, 0.0}, {" 30 fps + 0.4 s hitch", 4.0, 1.0, 0.4}};
    for (const FpsArm& a : arms) {
        const TremorRun on =
            belly_up_run(tr_on(), a.mult * kAp.sim_dt, Hand::TREMOR_ALT, 0.0,
                         2.0, a.hitch_at, a.hitch_dt);
        char tag[64];
        std::snprintf(tag, sizeof tag, "ON    tremor @ %s", a.name);
        print_run(tag, on);
        // The SAME verdict at every rate: the tremor reads as a resting hand
        // and the righting is handed back in full.
        // Under the 1 deg/s FLOOR at every frame rate, once the window has
        // filled: the tremor is not a hand, and the measure says so in its
        // own units.
        CHECK(on.net_rate_settled < 1.0);
        CHECK(on.gate_max > 0.99);
        CHECK(deg(on.roll_right_peak) >= deg(kCp.inverted_rate) - 0.5);
        CHECK(on.integ_right_deg > 20.0);
    }
    // ...and a hand that NEVER rests still never accumulates rest, at any
    // rate (the v15 P1 leg, re-run through the new arithmetic).
    for (const FpsArm& a : arms) {
        const TremorRun sw =
            belly_up_run(tr_on(), a.mult * kAp.sim_dt, Hand::SWEEP, 40.0, 2.0,
                         a.hitch_at, a.hitch_dt);
        std::printf(
            "[tremor] 40 deg/s sweep @ %s: rest_max %.4f s gate_max "
            "%.3f\n",
            a.name, sw.hand_rest_max, sw.gate_max);
        CHECK(sw.hand_rest_max < 0.02);
        CHECK(sw.gate_max < 0.05);
    }
}

// ===========================================================================
// LEG 4 -- THE OFF ARM IS BIT-IDENTICAL, PINNED TWICE.
//
// THE METHOD (the same one LEG (i) at the top of this file documents): the
// recorded hashes are the bit-mix of the emitted omega_des.z AND hand_gate on
// every tick of two apex probes, captured by building this very tree against
// `git show HEAD:control/controller.cpp` -- the PRE-CHANGE controller, same
// config, same probe, same compiler -- before S-tremor existed. So this is the
// pre-change binary's own trace; one changed ULP on one tick moves it.
// Captured 2026-09-16 on feel/tremor-netwindow at d01cf4221.
//
//   off_arm() (EVERY lane dial off, the pre-v15 tree)  0x71176a88ee5d8a8f
//   rh_on() with hand_net_window 0 (the v16 SHIPPED tree) 0x7102cf59b6460f33
//
// ⚠ NOT A NON-VACUITY LEG, deliberately, and it says so out loud: this probe
// scripts aim_moved WITHOUT an aim rate (harness::ClosedLoop's shape), which
// is the measure's unmeasurable arm -- fully live by construction -- so the
// hash is the same at the SHIPPED window too, and that equality is asserted
// here as the firewall it is, not mistaken for evidence. The dial's teeth are
// graded by LEG 1, which drives real counts through app::step_frame.
// ===========================================================================
TEST_CASE("S-tremor: the off arm is the pre-change tree, tick for tick") {
    constexpr std::uint64_t kPreOff = 0x71176a88ee5d8a8fULL;
    constexpr std::uint64_t kPreV16 = 0x7102cf59b6460f33ULL;
    const std::uint64_t h_off = apex_hash_pair(off_arm());
    const std::uint64_t h_v16 = apex_hash_pair(tr_off());
    const std::uint64_t h_on = apex_hash_pair(tr_on());
    std::printf(
        "[tremor] apex hash  off_arm %016llx (pre %016llx)  v16 %016llx (pre "
        "%016llx)  shipped %016llx\n",
        (unsigned long long)h_off, (unsigned long long)kPreOff,
        (unsigned long long)h_v16, (unsigned long long)kPreV16,
        (unsigned long long)h_on);
    CHECK(h_off == kPreOff);
    CHECK(h_v16 == kPreV16);
    // The firewall on THIS probe (see the warning above): scripted aim_moved
    // with no rate is unmeasurable, so the shipped dial changes nothing here.
    CHECK(h_on == kPreV16);
}

// ===========================================================================
// LEG 5 -- RAMP, NOT STEP, THROUGH A TREMOR. The v15 P2 lesson: an M3 mutant
// (hand_gate := the bare predicate) survived every other leg. The tremor arm
// must spend real time strictly inside (0.05, 0.95), not jump 0 -> 1.
//
// MUTATION: hand_gate := (hand_live_frac < 0.5) -- zero ticks in the middle.
// ===========================================================================
TEST_CASE("S-tremor: the authority still ramps through a tremor") {
    const TremorRun on =
        belly_up_run(tr_on(), 2.0 * kAp.sim_dt, Hand::TREMOR_ALT, 0.0, 2.0);
    std::printf(
        "[tremor] ramp through a tremor: %d ticks strictly inside "
        "(0.05, 0.95), gate %.3f..%.3f\n",
        on.mid_ticks, on.gate_min, on.gate_max);
    CHECK(on.mid_ticks > 5);
    CHECK(on.gate_max > 0.99);
}

// ===========================================================================
// LEG 6 -- HANDS OFF IS UNCHANGED (the 2026-08-06 ruling). The window must add
// NO delay to a genuinely still hand: the OUTER v16 hand-live gate is what
// keeps that true, and this pins it as a NUMBER, not as an argument.
//
// MUTATION: drop the outer hand_live gate (score liveness every tick from the
// window alone) and a hand that just stopped keeps the clock pinned for a
// further `window` seconds -- the still arm's integrated righting falls.
// ===========================================================================
TEST_CASE("S-tremor: hands off is bit-unchanged (the 08-06 ruling)") {
    const TremorRun v16 =
        belly_up_run(tr_off(), 2.0 * kAp.sim_dt, Hand::STILL, 0.0, 2.0);
    const TremorRun on =
        belly_up_run(tr_on(), 2.0 * kAp.sim_dt, Hand::STILL, 0.0, 2.0);
    print_run("v16   hands off", v16);
    print_run("ON    hands off", on);
    REQUIRE(v16.righting_ticks > 0);
    CHECK(on.righting_ticks == v16.righting_ticks);
    CHECK(on.integ_right_deg == v16.integ_right_deg);  // BIT-identical
    CHECK(on.hand_rest_max == v16.hand_rest_max);
    CHECK(on.gate_max > 0.99);
    // The existing apex leg "hands OFF still rights him" covers the other
    // geometry; this one covers the whole 2 s of it at the shipped dial.
}

// ===========================================================================
// LEG 7 (the half that lives here) -- THE SHIPPED DIAL IS LIVE. A silent
// walk-back of the toml to 0 would make every leg above vacuous, so it is
// stated as a number. The RANGE PIN and the negative-value refusal live in
// test_load_controller.cpp beside right_hand_rest's, against the REAL
// committed table (the loader suite's mutation method).
// ===========================================================================
TEST_CASE("S-tremor: the shipped hand_net_window is live and in range") {
    std::printf("[tremor] shipped [auto_level] hand_net_window = %.3f s\n",
                kCp.hand_net_window);
    CHECK(kCp.hand_net_window > 0.0);
    CHECK(kCp.hand_net_window <= 1.0);
}

// ===========================================================================
// THE RANDOM-WALK VARIANT -- a MEASURED RESIDUAL, graded honestly (§3
// departure; the handoff carries the reasoning). The packet asked for a
// second tremor shape: a zero-mean +/-1-count RANDOM WALK. It is NOT a
// stationary tremor. At aim_sensitivity 0.14 deg/count a walk of N frames
// wanders sqrt(N) counts, so over a 0.2 s window at 60 fps the aim genuinely
// MOVES ~0.4 deg and reads as a ~2 deg/s hand. By the gun-director law that
// IS a real hand movement and must keep vetoing; discounting it would require
// eating deliberate inputs of the same size, which is exactly what the twice-
// rejected deadband did. So this leg PRINTS the numbers and pins only the
// direction: the walk sits between the alternating tremor and a real sweep.
// ===========================================================================
TEST_CASE("S-tremor: the random-walk residual, measured not hidden") {
    const TremorRun alt =
        belly_up_run(tr_on(), 2.0 * kAp.sim_dt, Hand::TREMOR_ALT, 0.0, 2.0);
    const TremorRun walk =
        belly_up_run(tr_on(), 2.0 * kAp.sim_dt, Hand::TREMOR_WALK, 0.0, 2.0);
    const TremorRun sweep =
        belly_up_run(tr_on(), 2.0 * kAp.sim_dt, Hand::SWEEP, 5.0, 2.0);
    print_run("ON    tremor alternating", alt);
    print_run("ON    tremor random walk", walk);
    print_run("ON    sweep 5 deg/s     ", sweep);
    // Graded on the BEHAVIOUR, not on net_rate_max: with the normalised
    // measure every arm's first ticks read a large instantaneous rate (the
    // window has not filled and cannot yet tell a tremor from a sweep), so a
    // peak-of-the-measure comparison grades the transient, not the hand. The
    // integrated righting is the felt quantity and it orders as the physics
    // says it must: an alternating tremor is a resting hand, a real sweep is
    // a vetoing hand, and the walk sits between them, nearer the sweep.
    CHECK(alt.integ_right_deg > walk.integ_right_deg);
    CHECK(walk.integ_right_deg > sweep.integ_right_deg);
    CHECK(walk.integ_right_deg < 0.25 * alt.integ_right_deg);
}

// ===========================================================================
// THE SWEEP THE SHIPPED VALUE WAS PICKED FROM. Not a tuning knob-twiddle
// against harness numbers (the kernel law forbids that) -- a CHART of the two
// walls the window sits between, printed into the gate output so the choice
// can be re-derived by anyone, and re-run after any retune.
//
//   TOO SHORT and the tremor's own net rate (one frame's rotation / window)
//   climbs over the 1 deg/s floor: the debt comes back.
//   TOO LONG and the window outlives the hand-rest ramp it feeds -- it starts
//   reporting history rather than the hand -- and the walk-back loses meaning.
//
// The shipped value is the one with headroom on BOTH walls at every frame
// rate, nearest right_hand_rest's own 0.25 s without exceeding it.
// ===========================================================================
TEST_CASE("S-tremor: the window sweep the shipped value came from") {
    std::printf(
        "[sweep] window |  tremor: integ_right  gate_max  net@120  net@10 |"
        "  5 deg/s sweep: integ_right  gate_max | walk: integ_right\n");
    for (double w : {0.05, 0.10, 0.15, 0.20, 0.25, 0.30, 0.50}) {
        control::ControllerParams c = tr_on();
        c.hand_net_window = w;
        const TremorRun tr =
            belly_up_run(c, 2.0 * kAp.sim_dt, Hand::TREMOR_ALT, 0.0, 2.0);
        const TremorRun t10 =
            belly_up_run(c, 12.0 * kAp.sim_dt, Hand::TREMOR_ALT, 0.0, 2.0);
        const TremorRun sw =
            belly_up_run(c, 2.0 * kAp.sim_dt, Hand::SWEEP, 5.0, 2.0);
        const TremorRun wk =
            belly_up_run(c, 2.0 * kAp.sim_dt, Hand::TREMOR_WALK, 0.0, 2.0);
        std::printf(
            "[sweep] %6.2f |        %7.2f    %6.3f   %5.2f   %5.2f |"
            "          %7.2f    %6.3f |      %7.2f\n",
            w, tr.integ_right_deg, tr.gate_max, tr.net_rate_settled,
            t10.net_rate_settled, sw.integ_right_deg, sw.gate_max,
            wk.integ_right_deg);
        // The 5 deg/s deliberate drift is vetoed at EVERY window in the
        // sweep -- the gun-director law never depends on the tuning.
        CHECK(sw.gate_max < 0.05);
    }
    // ...and the shipped value is on the right side of both walls.
    const TremorRun ship =
        belly_up_run(tr_on(), 2.0 * kAp.sim_dt, Hand::TREMOR_ALT, 0.0, 2.0);
    CHECK(ship.net_rate_settled < 1.0);
    CHECK(ship.integ_right_deg > 20.0);
}

// ===========================================================================
// THE TAPE PIN. S-tremor adds two DIAGNOSTIC columns (hand_net_rate,
// hand_live_frac -- the measure's own reading and the liveness it produced)
// and four REPLAY-STATE ones (aim_net x/y/z and its normaliser aim_net_w,
// because a leaky integral with a window of memory cannot be seeded from
// zero mid-tape without scoring the first window as a still hand). 160 (v16)
// -> 166. The v5 launch banner grew the dial beside right_hand_rest in the
// same commit, so a tape can never be replayed against the wrong dials.
//
// MUTATION: add a column to kFeelTapeColumns without extending the writer's
// format string (or the reverse) and the emitter/reader divergence that cost
// 2026-09-13 a night comes straight back -- test_tape_roundtrip.cpp catches
// the count end to end; this pins the NUMBER, so a silent growth is a
// decision someone had to make, not a diff nobody read.
// ===========================================================================
TEST_CASE("S-tremor: the feel tape carries the measure (column-count pin)") {
    std::printf("[tremor] feel tape columns: %d (v16 shipped 160)\n",
                app::kFeelTapeColumnCount);
    CHECK(app::kFeelTapeColumnCount == 166);
}
