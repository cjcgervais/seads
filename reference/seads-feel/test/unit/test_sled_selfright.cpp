// R4a SEATED SELF-RIGHT v2 -- the rocking model. Chad 2026-08-26.
//
//   v1: "IF ON THE SEAT AFTER A ROLLOVER PRESSING THE STAND BUTTON ... WILL
//        RIGHT THE SLEIGHT ONTO ITS SKIS ... NOT ABOVE 5KM/H ... IT CAN FAIL TO
//        RIGHT GIVEN THE SITUATION, PROGRESSIVE HOLD"
//   v2, after he drove v1: "no it didnt work at all, never got the standing
//        function to right me, also should work from a full inversion, just
//        takes a few sustained pushes."
//
// Spec: docs/R4A_THROW_RULING_20260825.md section 5.
//
// ★★ WHY THIS FILE WAS REWRITTEN. v1's suite was GREEN while the drive failed
// completely, and the reason is the sharpest lesson of this rung: every outcome
// leg proved the mechanism at 4000 N m while config/scenario.toml SHIPPED 900.
// The tests certified a kernel nobody ran. THE LAW, paid for again -- a constant
// that describes the shipped table stops describing it the moment the table
// moves, and nothing goes red. `shipped()` below closes that hole structurally:
// the outcome legs read the SAME toml the game reads, so an outcome leg can no
// longer be written against a value nobody ships.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdio>
#include <utility>
#include <cmath>

#include "config/load_aircraft.h"
#include "config/load_scenario.h"
#include "sim/sled.h"
#include "world/snowpack.h"

namespace {

// A GENERIC point: not on a world axis, so a fixed-axis "up" cannot coincide
// with the true local up and pass by luck (the house trap).
const glm::dvec3 kDir = glm::normalize(glm::dvec3(1.0, 1.0, 1.0));
const double kDeg = 3.14159265358979323846 / 180.0;

world::HeightField flat_field() {
    world::HeightField hf;
    hf.w = 64;
    hf.h = 32;
    hf.R = 6'371'000.0;
    hf.relief_scale = 400.0;
    hf.u_offset = 0.0;
    hf.px.assign(static_cast<std::size_t>(hf.w) * hf.h, 0);
    return hf;
}

const world::HeightField& shared_hf() {
    static const world::HeightField hf = flat_field();
    return hf;
}

world::SnowpackField field(double base_m = 0.5) {
    world::SnowpackField f;
    f.hf = &shared_hf();
    f.p.base_m = base_m;
    f.p.curv_gain = 0.0;
    f.p.drain_gain = 0.0;
    f.p.aspect_lee = 0.0;
    f.p.elev_gain_per_km = 0.0;
    f.p.slope_shed = 0.0;
    f.p.depth_max_m = 5.0;
    return f;
}

// ★ THE SHIPPED COMFORT BLOCK, read from the SAME config/scenario.toml the game
// reads. Not a test-local number -- see the banner.
const sim::SledParams& shipped() {
    static const sim::SledParams p = [] {
        sim::SledParams q;
        const sim::AircraftParams ap =
            cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");
        q.comfort =
            cfg::load_scenario_toml(SEADS_CONFIG_DIR "/scenario.toml", ap)
                .sled_comfort;
        return q;
    }();
    return p;
}

sim::SledState tipped_raw(const sim::SledParams& p,
                          const world::SnowpackField& f, double tilt_rad,
                          double speed_ms) {
    sim::SledState s;
    const glm::dvec3 up = kDir;
    glm::dvec3 fwd(0.0, 0.0, 1.0);
    fwd = glm::normalize(fwd - glm::dot(fwd, up) * up);
    const glm::dvec3 right = glm::cross(fwd, up);
    const glm::dvec3 b_up = std::cos(tilt_rad) * up + std::sin(tilt_rad) * right;
    const glm::dvec3 b_right = glm::cross(fwd, b_up);
    glm::dmat3 m;
    m[0] = b_right;
    m[1] = b_up;
    m[2] = -fwd;
    s.orientation = glm::normalize(glm::quat_cast(m));
    s.position = up * (f.drive_radius_at(up) + p.cg_height_m + 0.05);
    s.velocity = fwd * speed_ms;
    return s;
}

// ★ SETTLE BEFORE MEASURING. v1's legs spawned at +0.05 m clearance and pressed
// immediately, so they measured a machine that was still LANDING.
sim::SledState settled(const sim::SledParams& p, double tilt_rad,
                       double speed_ms, double lean_lat,
                       const world::SnowpackField& f) {
    sim::SledState s = tipped_raw(p, f, tilt_rad, speed_ms);
    sim::SledInputs in;
    in.lean_lat = static_cast<float>(lean_lat);
    for (int i = 0; i < 360; ++i) s = sim::step_sled(s, in, p, f, 1.0 / 120.0);
    return s;
}

double tilt_of(const sim::SledState& s) {
    const glm::dmat3 R = glm::mat3_cast(s.orientation);
    return std::acos(
        std::clamp(glm::dot(R[1], glm::normalize(s.position)), -1.0, 1.0));
}

struct RockResult {
    double tilt_final = 0.0;
    double gs_peak = 0.0;
    bool hit_150 = false, hit_90 = false, hit_35 = false;
};

// Drive a scripted press pattern. off_s <= 0 means hold continuously.
RockResult rock(const sim::SledParams& p, double tilt0, double lean_lat,
                double on_s, double off_s, double total_s,
                double base_m = 0.5) {
    const world::SnowpackField f = field(base_m);
    sim::SledState s = settled(p, tilt0, 0.0, lean_lat, f);
    sim::SledInputs in;
    in.lean_lat = static_cast<float>(lean_lat);
    RockResult r;
    const double dt = 1.0 / 120.0;
    const int n = static_cast<int>(total_s / dt);
    const double period = on_s + off_s;
    for (int i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) * dt;
        const bool on = (off_s <= 0.0) ? true : (std::fmod(t, period) < on_s);
        in.stand = on ? 1.0f : 0.0f;
        s = sim::step_sled(s, in, p, f, dt);
        const double tl = tilt_of(s);
        if (tl > 150.0 * kDeg) r.hit_150 = true;
        if (r.hit_150 && tl < 90.0 * kDeg) r.hit_90 = true;
        if (r.hit_90 && tl < 35.0 * kDeg) r.hit_35 = true;
        const glm::dvec3 up = glm::normalize(s.position);
        r.gs_peak = std::max(
            r.gs_peak, glm::length(s.velocity - glm::dot(s.velocity, up) * up));
    }
    r.tilt_final = tilt_of(s);
    return r;
}


// ★★ B2b THE PENDULUM (ladder_v2 §4.4 -- THE LEG THAT DECIDES THE RUNG).
// Chad, 2026-09-18: "self righting with a press and I want to be able to self
// right by rocking bodyweight back an fourth while pressing stand on and off,
// gain pendulum momentum (NOT AUTOMATIC RE RIGHTING)."
//
// Same 180 deg start, same STAND cadence; the ONLY difference between the arms
// is WHEN his body goes across. `lean_lat` is a square wave keyed to the sign of
// the body roll rate `angular_vel.z`.
//
// ⚠ THE SIGN CONVENTION IS MEASURED, NOT ASSUMED, AND IT IS THE OPPOSITE OF THE
// OBVIOUS ONE. Body +Z is BACKWARD (body -Z is forward, SPEC §7), so a positive
// `angular_vel.z` is a roll toward NEGATIVE `lean_lat`. The arm that adds energy
// to the swing -- the one a child uses on a swing set -- is therefore
// `lean_lat = -sign(angular_vel.z)`, and that is what `phase = +1` means here.
// MEASURED at 178 deg / frac 0.5: -sign rights in 1.48 s, +sign in 2.06 s. If
// `phase` were defined the naive way this leg would assert the mistimed arm is
// faster and would be permanently red for a reason that has nothing to do with
// the mechanism.
struct PumpResult {
    double t_right = -1.0;  // seconds to come under right_tilt_lo_rad; -1 never
    double tilt_final = 0.0;
    bool hit_150 = false;
};

PumpResult pump(const sim::SledParams& p, double tilt0, double on_s,
                double off_s, double total_s, int phase) {
    const world::SnowpackField f = field();
    sim::SledState s = settled(p, tilt0, 0.0, 0.0, f);
    sim::SledInputs in;
    PumpResult r;
    const double dt = 1.0 / 120.0;
    const int n = static_cast<int>(total_s / dt);
    const double period = on_s + off_s;
    for (int i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) * dt;
        in.stand = (std::fmod(t, period) < on_s) ? 1.0f : 0.0f;
        if (phase == 0) {
            // ★★ FOLDED RED-TEAM P1-5: THE HUMAN-CADENCE ARM. `phase` +/-1 is a
            // 120 Hz sign-following BANG-BANG controller -- perfect-information
            // feedback, not a hand on a mouse. It proves "a feedback controller
            // beats an anti-feedback controller", which for a pendulum is
            // nearly a tautology. This arm is a FREE-RUNNING square wave
            // phase-locked to the PRESS (the same 1.0 s / 0.7 s rhythm his
            // thumb is on) and blind to `angular_vel.z` -- the closest thing a
            // fixture can get to a man rocking in time with his own pressing.
            in.lean_lat = (std::fmod(t, period) < on_s) ? 1.0f : -1.0f;
        } else {
            const double w = s.angular_vel.z;
            const double sgn = (w > 0.0) ? -1.0 : (w < 0.0 ? 1.0 : 0.0);
            in.lean_lat = static_cast<float>(phase * sgn);
        }
        s = sim::step_sled(s, in, p, f, dt);
        const double tl = tilt_of(s);
        if (tl > 150.0 * kDeg) r.hit_150 = true;
        if (r.t_right < 0.0 && tl < p.comfort.right_tilt_lo_rad) r.t_right = t;
    }
    r.tilt_final = tilt_of(s);
    return r;
}

}  // namespace

TEST_CASE("selfright_a_timed_rock_beats_a_mistimed_one") {
    // THE CLAIM, in his words: at B2b's candidate the TIMING has to matter.
    //
    // ★★ AND A REPORT AGAINST THE SPEC'S OWN POSITIVE CONTROL. §4.4 predicts
    // that at the shipped 1.0 the leg "should show NO discrimination", because
    // the automatic brace commands the whole shift. MEASURED, IT DOES NOT:
    // at 1.0 the timed rock still wins, 1.48 s against 1.77 s. The brace does
    // not drown his body -- it is ADDED to it. sim/sled.cpp:417-421:
    // `lat_target_sr = clamp(target_lat_m + right_shift_cmd * frac *
    // lean_lat_stand_m, +/- lean_lat_stand_m)`. His lean is never REPLACED; at
    // 1.0 the brace can only saturate the clamp on one side, which is why the
    // advantage shrinks instead of vanishing. So the positive control as
    // written is FALSE, and it is reported here rather than asserted.
    //
    // What IS true, measured, and is the rung's actual claim: the discrimination
    // GROWS as the brace is turned down. 0.29 s of advantage at 1.0, 0.58 s at
    // 0.5, and at 0.25 and 0.0 the mistimed arm never rights from inversion at
    // all. That ordering is what this leg asserts.
    //
    // KILLED BY: `right_stand_shift_frac` not reaching the rider's lateral
    // target at all (an app override that writes a field nobody reads, or the
    // frac dropped from the clamp) -- both gaps then come out equal and the
    // final REQUIRE reds. Also killed by a fixture that holds a CONSTANT lean:
    // that measures which SIDE he picked, not when he went there, and both arms
    // would separate for the wrong reason.
    const double kStart = 178.0 * kDeg;
    auto gap = [&](double frac) {
        sim::SledParams p = shipped();
        p.comfort.right_stand_shift_frac = frac;
        const PumpResult timed = pump(p, kStart, 1.0, 0.7, 16.0, +1);
        const PumpResult mistimed = pump(p, kStart, 1.0, 0.7, 16.0, -1);
        REQUIRE(timed.hit_150);     // both arms really started inverted
        REQUIRE(mistimed.hit_150);
        REQUIRE(timed.t_right >= 0.0);  // the timed rock always comes up
        std::printf("[B2 pendulum] frac=%.2f  timed=%6.2f s  mistimed=%6.2f s "
                    "(end %5.1f deg)\n",
                    frac, timed.t_right, mistimed.t_right,
                    mistimed.tilt_final / kDeg);
        // A mistimed rock that NEVER rights is an infinite advantage; score it
        // as the whole run so the ordering below stays well defined.
        const double m = mistimed.t_right < 0.0 ? 16.0 : mistimed.t_right;
        return m - timed.t_right;
    };
    const double at_shipped = gap(1.0);   // the brace carries most of it
    const double at_candidate = gap(0.5); // B2b's drive value
    const double at_quarter = gap(0.25);
    REQUIRE(at_candidate > at_shipped);   // turning the brace down ...
    REQUIRE(at_quarter > at_candidate);   // ... makes the timing matter MORE
    REQUIRE(at_candidate > 0.0);          // and at the candidate, timing wins

    // ★★ FOLDED RED-TEAM P1-5 (law+feel): B2b IS A SUBTRACTION SOLD AS A GAIN,
    // AND THIS LEG'S OWN ROWS SAY SO. MEASURED, timed / mistimed, in seconds:
    //     frac 1.00   1.48 / 1.77
    //     frac 0.50   1.48 / 2.06
    //     frac 0.25   1.51 / never (ends 107.7 deg)
    //     frac 0.00   1.57 / never (ends 107.4 deg)
    // THE TIMED ARM NEVER IMPROVES -- it gets slightly SLOWER, 1.48 -> 1.57 s.
    // Every bit of the widening gap comes from the MISTIMED arm degrading. So
    // what this dial delivers is not "gain pendulum momentum"; it is "a bad
    // rhythm stops being free". That may still be the feel he wants -- a
    // mechanic you can fail is a mechanic -- but it is a DIFFERENT mechanic
    // from the one his sentence asks for, the four rows are on his drive sheet
    // above run 5, and handoff §5 carries the owed ruling: DID YOU WANT THE
    // ROCK ITSELF TO PAY? If yes, `right_stand_shift_frac` is the wrong dial
    // entirely and the rung needs a term converting his lean RATE into
    // righting torque -- another dial, another night.
    //
    // ★ AND THE THIRD ARM, because the two above are bang-bang controllers.
    // `phase == 0` is a FREE-RUNNING human cadence, phase-locked to the press
    // and blind to the roll rate. The claim it tests is the one that actually
    // matters to a hand: does a rock that is merely IN RHYTHM WITH THE PRESSING
    // beat one that is fighting the machine? Reported at both ends of the dial;
    // asserted only where the measurement supports it.
    auto human = [&](double frac) {
        sim::SledParams p = shipped();
        p.comfort.right_stand_shift_frac = frac;
        const PumpResult free_run = pump(p, kStart, 1.0, 0.7, 16.0, 0);
        const PumpResult mistimed = pump(p, kStart, 1.0, 0.7, 16.0, -1);
        std::printf("[B2 human] frac=%.2f  free-running=%6.2f s (end %5.1f "
                    "deg)  mistimed=%6.2f s\n",
                    frac, free_run.t_right, free_run.tilt_final / kDeg,
                    mistimed.t_right);
        return free_run;
    };
    const PumpResult human_shipped = human(1.0);
    const PumpResult human_candidate = human(0.5);
    // A free-running rock must still right her at the drive value, or the rung
    // is not established FOR A HAND and that is a finding worth more than this
    // leg's pass. KILLED BY: a dial that only works for a 120 Hz controller.
    REQUIRE(human_candidate.hit_150);
    REQUIRE(human_candidate.t_right >= 0.0);
    REQUIRE(human_shipped.t_right >= 0.0);
}

// ★★ THE LEG THAT CATCHES v1's KILLER DEFECT. v1's error signal was
// -up_body.x = sin(tilt), IDENTICALLY ZERO at 180 deg: at the one attitude Chad
// most needed it the assist applied exactly nothing. Under v1 this reads at
// most 900*sin(178 deg) ~ 31 N m and fails.
TEST_CASE("selfright: FULL authority at full inversion") {
    const sim::SledParams p = shipped();
    REQUIRE(p.comfort.right_assist_nm > 0.0);  // non-vacuity: it ships ON
    const world::SnowpackField f = field();
    // ★ THE TEST IS THE SHAPE OF THE SIGNAL, not its size at one instant.
    // v1's error was -up_body.x = sin(tilt), so its authority COLLAPSED toward
    // inversion: sin(173 deg)/sin(90 deg) = 0.12, i.e. near the top it kept
    // barely a tenth of what it had on the side. That is why the machine would
    // not move. v3 saturates the direction, so the two must be comparable.
    // Returns {applied torque, the tilt it was applied at} so the v1 baseline
    // is computed at the SAME attitudes -- no hand-picked constant anywhere.
    auto probe = [&](double deg) {
        sim::SledState s = settled(p, deg * kDeg, 0.0, 1.0, f);
        const double tl = tilt_of(s);
        sim::SledInputs in;
        in.stand = 1.0f;
        in.lean_lat = 1.0f;
        s = sim::step_sled(s, in, p, f, 1.0 / 120.0);
        return std::pair<double, double>{std::fabs(s.right_assist_nm_now), tl};
    };
    const auto side = probe(90.0);
    const auto inv = probe(178.0);
    REQUIRE(side.first > 0.0);  // non-vacuity
    REQUIRE(inv.first > 0.0);   // v1 at the top was ~0 and the machine sat there
    REQUIRE(inv.second > 150.0 * kDeg);  // it really is near inversion
    // v1's authority ratio between these two attitudes, from its OWN formula
    // (nm * sin(tilt)) at the tilts actually measured. v3 must beat that shape
    // by a wide margin -- that IS the defect, expressed as a differential.
    const double v1_ratio = std::sin(inv.second) / std::sin(side.second);
    const double v3_ratio = inv.first / side.first;
    REQUIRE(v3_ratio > 2.0 * v1_ratio);
}

// ★★ "A FEW SUSTAINED PUSHES", both halves.
// A) an infinite HOLD must NOT do it -- the pusher tires, so a hold injects
//    finite energy and then nothing. This kills one-press-does-everything.
// B) a PACED pattern must, and must TRAVERSE the arc rather than teleport --
//    the milestone ordering kills the fixture-no-op class.
// ★ IT RIGHTS FROM FULL INVERSION AT ANY LEAN. This is Chad's v3 report --
// "a multiple press from full inversion should actually right the sled, not
// fail" -- and the leg that would have caught why it failed for him:
//
// ⚠⚠ EVERY v3 OUTCOME LEG HELD lean AT FULL, and that was the one variable
// that made it work. Measured at the time: from 178 deg the machine righted at
// lean 1.0 and NEVER at 0.0 / 0.25 / 0.5. The fixture-no-op class in a new
// dimension -- the fixture set the thing under test. He does not necessarily
// lean while pressing, so his drive found it and the suite could not.
TEST_CASE("selfright: rights from full inversion at ANY lean") {
    const sim::SledParams p = shipped();
    for (double ln : {0.0, 0.25, 0.5, 1.0}) {
        const RockResult r = rock(p, 178.0 * kDeg, ln, 1.0, 0.7, 12.0);
        REQUIRE(r.tilt_final < p.comfort.right_tilt_lo_rad);
        REQUIRE(r.hit_150);  // it really started inverted ...
        REQUIRE(r.hit_35);   // ... and really traversed the arc
    }
}

// ★★ THE LEG THAT WOULD HAVE CAUGHT THE FAILED DRIVE.
TEST_CASE("selfright: the legs run at the SHIPPED dial, not a test-local one") {
    const sim::SledParams p = shipped();
    const sim::SledParams dflt;  // struct defaults: the mechanic is OFF
    REQUIRE(dflt.comfort.right_assist_nm == 0.0);
    REQUIRE(p.comfort.right_assist_nm > 0.0);
    // And the shipped dial matters: a machine on its SIDE comes up. Under v1's
    // shipped 900 (static stall ~46 deg) this fails.
    const RockResult side = rock(p, 90.0 * kDeg, 1.0, 1.0, 0.7, 10.0);
    REQUIRE(side.tilt_final < p.comfort.right_tilt_lo_rad);
}

// ★ ANTI-RITUAL: mashing must be WORSE than pacing, or the mechanic degenerates
// into button-mashing. An off-phase push torques against the swing.
// ★ ANTI-RITUAL, honestly scoped. With no lean of his own, a COMMITTED press
// rights the machine and mashing does not -- timing still beats button-spam.
// ⚠ At FULL lean mashing also succeeds now: the automatic stand-shift plus his
// own lean is enough authority that cadence stops mattering. That is a real
// property of the shipped dials, recorded rather than hidden, and it is the
// thing to watch if the mechanic ever starts to feel free.
TEST_CASE("selfright: a committed press beats mashing") {
    const sim::SledParams p = shipped();
    const RockResult committed = rock(p, 178.0 * kDeg, 0.0, 1.0, 0.7, 12.0);
    const RockResult mash =
        rock(p, 178.0 * kDeg, 0.0, 1.0 / 12.0, 1.0 / 12.0, 12.0);
    REQUIRE(committed.tilt_final < p.comfort.right_tilt_lo_rad);
    REQUIRE(mash.tilt_final > 90.0 * kDeg);
}

// ★ HIS LEAN PICKS THE SIDE at the unstable equilibrium. A flipped seed sign
// would make the assist fight the very tip his lean produces.
TEST_CASE("selfright: leaning picks the side, and the assist agrees") {
    const sim::SledParams p = shipped();
    REQUIRE(rock(p, 178.0 * kDeg, +1.0, 1.0, 0.7, 12.0).tilt_final <
            90.0 * kDeg);
    REQUIRE(rock(p, 178.0 * kDeg, -1.0, 1.0, 0.7, 12.0).tilt_final <
            90.0 * kDeg);
}

// ★ EMERGENT FAILURE. Deep snow eats the per-swing energy, so the SAME pattern
// that rights it on a normal pack plateaus. Same gates open in both arms -- the
// only difference is the snow, so this is LOSING, not refusing.
TEST_CASE("selfright: it can still fail, and the failure is emergent") {
    // ⚠ MEASURED, and it corrected my assumption: DEEP SNOW DOES NOT STOP IT --
    // it rights slightly BETTER in 2.5 m of base than in 0.5 m (0.030 vs 0.052
    // rad final). So "deep snow is the failure case" was a story, not a result.
    //
    // The honest emergent failure is AUTHORITY. Same gates open in both arms,
    // same fixture, same presses -- only the strength differs, so when the arm
    // below fails it is gravity winning, never a branch declining to fire.
    const sim::SledParams p = shipped();
    sim::SledParams weak = p;
    weak.comfort.right_assist_nm = 1800.0;  // measured: never rights, any cadence
    const RockResult strong = rock(p, 178.0 * kDeg, 1.0, 1.0, 0.7, 12.0);
    const RockResult feeble = rock(weak, 178.0 * kDeg, 1.0, 1.0, 0.7, 12.0);
    REQUIRE(strong.tilt_final < p.comfort.right_tilt_lo_rad);
    REQUIRE(feeble.tilt_final > 90.0 * kDeg);  // still on its back
}

// "MACHINE NEEDS TO BE TIPPING OVER" -- upright gets nothing, so this can never
// act as free roll stiffness in ordinary riding.
TEST_CASE("selfright: an upright machine is untouched") {
    const sim::SledParams on = shipped();
    sim::SledParams off = on;
    off.comfort.right_assist_nm = 0.0;
    const double t0 = 0.05;
    REQUIRE(t0 < on.comfort.right_tilt_lo_rad);
    const world::SnowpackField f = field();
    sim::SledState a = settled(on, t0, 0.0, 0.0, f);
    sim::SledState b = settled(off, t0, 0.0, 0.0, f);
    sim::SledInputs in;
    in.stand = 1.0f;
    for (int i = 0; i < 120; ++i) {
        a = sim::step_sled(a, in, on, f, 1.0 / 120.0);
        b = sim::step_sled(b, in, off, f, 1.0 / 120.0);
    }
    REQUIRE(std::fabs(tilt_of(a) - tilt_of(b)) < 1e-12);
}

// "NOT ABOVE 5KM/H" -- measured on the READOUT, because an outcome test cannot
// tell a closed gate from a machine that slowed down and rightly rose.
//
// ⚠⚠ RENAMED AND RE-BASED AT SLED KERNEL v2 (Chad 2026-09-18). It was
// `selfright: above 5 km/h it cannot happen` and it drove a FIXED 4.0 m/s
// against a gate that was 1.3889 m/s -- a hard-coded speed that only worked
// while the gate stood still. HE MOVED THE GATE HIMSELF: "4 is approved I can
// land upright more often" (drive run 4), so `[sled_comfort]
// right_assist_max_ms` is 4.0 m/s now and a 4.0 m/s fixture sits exactly ON the
// threshold and never disarms. THE LAW IS UNCHANGED AND IS WHAT THIS LEG STILL
// ASSERTS -- there is a speed above which the assist cannot happen, and above
// it the assist is SILENT, not merely weak. Only the number is read from the
// shipped table instead of being retyped, which is the same fix `shipped()`
// itself was written for (see the banner at the top of this file).
TEST_CASE("selfright: above the shipped speed gate it cannot happen") {
    const sim::SledParams p = shipped();
    const world::SnowpackField f = field();
    // Comfortably above the gate, whatever the gate is. TWICE the gate, and
    // the factor is MEASURED on this fixture, not picked: over 240 ticks from a
    // 90 deg tip the assist never disarms at 1.5x (6.0 m/s) because the 0.5 s
    // low-pass is still climbing while she drags to a stop, and disarms at
    // 2x / 3x / 4x / 6x (8, 12, 16, 24 m/s). The old fixed 4.0 m/s was 2.88x
    // the pre-v2 gate, so 2x is if anything the tighter test.
    const double v_over = p.comfort.right_assist_max_ms * 2.0;
    sim::SledState s = tipped_raw(p, f, 90.0 * kDeg, v_over);
    sim::SledInputs in;
    in.stand = 1.0f;
    // The low-pass needs a moment to see the real speed (tau = 0.5 s); that is
    // the whole point of it, so the leg waits rather than reading a transient.
    bool ever_disarmed = false;
    for (int i = 0; i < 240; ++i) {
        s = sim::step_sled(s, in, p, f, 1.0 / 120.0);
        if (!s.right_assist_armed) {
            ever_disarmed = true;
            REQUIRE(s.right_assist_nm_now == 0.0);  // disarmed means SILENT
        }
    }
    REQUIRE(ever_disarmed);  // non-vacuity: the gate really did close
    // ★ v2 PIN: the shipped gate is Chad's driven 4.0 m/s, by name, so this leg
    // reds if the TOML key is quietly walked back to the pre-v2 1.3889.
    REQUIRE(p.comfort.right_assist_max_ms == 4.0);
}

// ★ THE GATE MUST NOT EAT ITS OWN MECHANIC. A healthy rock sways the CG fast
// enough to cross the RAW gate; the low-pass is what stops that disarming the
// assist mid-swing. The second REQUIRE keeps the leg non-vacuous.
TEST_CASE("selfright: rocking does not trip its own speed gate") {
    const sim::SledParams p = shipped();
    const RockResult paced = rock(p, 178.0 * kDeg, 1.0, 1.0, 0.7, 12.0);
    REQUIRE(paced.tilt_final < p.comfort.right_tilt_lo_rad);
    // ⚠ THE NON-VACUITY CLAUSE IS MEASURED AGAINST THE **PRE-v2** RAW GATE, AS
    // AN EXPLICIT CONSTANT, AND HERE IS WHY (SLED KERNEL v2, 2026-09-18).
    // The claim is "a healthy rock sways the CG fast enough to cross the RAW
    // gate, and the 0.5 s low-pass is what stops that disarming the assist
    // mid-swing". MEASURED, the rock peaks at 1.9224 m/s. Against the pre-v2
    // gate of 1.3889 m/s that is 1.38x the whole gate -- the hazard was real and
    // the low-pass is why it never bit. Chad's v2 gate is 4.0 m/s, so the rock
    // now peaks at 0.48x the gate and the hazard has receded; writing the clause
    // as `> shipped_gate * 0.5` would red for the wrong reason (1.9224 > 2.0 is
    // false) and would be asserting that the rock is FAST, which was never the
    // claim. The constant below is the gate the hazard was measured against.
    constexpr double kPreV2RawGateMs = 5.0 / 3.6;  // 1.3888888888888888
    REQUIRE(paced.gs_peak > kPreV2RawGateMs);
    // ... and the headroom his v2 gate bought, stated rather than implied.
    REQUIRE(paced.gs_peak < p.comfort.right_assist_max_ms);
}

// A TUCK is not a stand; each arm compared against the SAME input with the
// assist off, never against a different pose (in.stand also moves the rider).
TEST_CASE("selfright: seated and tucked do nothing") {
    const sim::SledParams on = shipped();
    sim::SledParams off = on;
    off.comfort.right_assist_nm = 0.0;
    const world::SnowpackField f = field();
    for (float st : {0.0f, -1.0f}) {
        sim::SledState a = settled(on, 90.0 * kDeg, 0.0, 0.0, f);
        sim::SledState b = settled(off, 90.0 * kDeg, 0.0, 0.0, f);
        sim::SledInputs in;
        in.stand = st;
        for (int i = 0; i < 120; ++i) {
            a = sim::step_sled(a, in, on, f, 1.0 / 120.0);
            b = sim::step_sled(b, in, off, f, 1.0 / 120.0);
        }
        REQUIRE(std::fabs(tilt_of(a) - tilt_of(b)) < 1e-12);
    }
}

// A hidden PROBE (tag [.probe] = not run by the gate). Measures what the
// mechanic actually does across the dial rather than guessing a number --
// the NO GUESSING law applied to my own tuning.
// A hidden PROBE ([.probe] = not run by the gate). This is how the shipped
// dials were MEASURED rather than guessed -- and how the knife-edge cells were
// found. Re-run it before moving right_assist_nm: the response is NOT monotonic
// in the dial (2400 and 3000 right the machine from 150-178 deg; 2600 and 2800
// do not), so a "small" change can land in a dead zone.
TEST_CASE("selfright probe sweep", "[.probe]") {
    sim::SledParams p = shipped();
    std::printf("\n   nm   lean   1p    2p    3p    4p   HOLD  MASH\n");
    for (double nm : {1400.0, 1600.0, 1800.0, 2000.0, 2200.0, 2400.0}) {
        p.comfort.right_assist_nm = nm;
        for (double ln : {0.0, 1.0}) {
            std::printf("  %5.0f  %4.1f", nm, ln);
            for (int k = 1; k <= 4; ++k)
                std::printf(" %5.1f", rock(p, 178.0 * kDeg, ln, 1.0, 0.7,
                                           k * 1.7 + 0.4).tilt_final / kDeg);
            std::printf(" %5.1f %5.1f\n",
                rock(p, 178.0*kDeg, ln, 1.0, 0.0, 20.0).tilt_final / kDeg,
                rock(p, 178.0*kDeg, ln, 1.0/12, 1.0/12, 20.0).tilt_final / kDeg);
        }
    }
}
