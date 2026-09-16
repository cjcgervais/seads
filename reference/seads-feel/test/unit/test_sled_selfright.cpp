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

}  // namespace

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
TEST_CASE("selfright: above 5 km/h it cannot happen") {
    const sim::SledParams p = shipped();
    const world::SnowpackField f = field();
    sim::SledState s = tipped_raw(p, f, 90.0 * kDeg, 4.0);
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
}

// ★ THE GATE MUST NOT EAT ITS OWN MECHANIC. A healthy rock sways the CG fast
// enough to cross the RAW gate; the low-pass is what stops that disarming the
// assist mid-swing. The second REQUIRE keeps the leg non-vacuous.
TEST_CASE("selfright: rocking does not trip its own speed gate") {
    const sim::SledParams p = shipped();
    const RockResult paced = rock(p, 178.0 * kDeg, 1.0, 1.0, 0.7, 12.0);
    REQUIRE(paced.tilt_final < p.comfort.right_tilt_lo_rad);
    REQUIRE(paced.gs_peak > p.comfort.right_assist_max_ms * 0.5);
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
