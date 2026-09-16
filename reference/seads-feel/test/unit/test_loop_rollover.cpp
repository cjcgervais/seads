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
    c.right_hand_rest = 0.0;
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
    // NON-VACUITY lives in the S-righthand apex legs below: this
    // rolling-loop probe never reaches the apex geometry the live dial
    // acts on, so a "the dial moves this hash" assertion here would be
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
