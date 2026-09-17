// load_controller cross-checks (SPEC Â§9.3b/Â§9.4): the load-bearing validation
// that a controller table which would oscillate by construction, or fail to
// protect the plant's stall, cannot load. These guards are the reason a bad
// tuning fails at load, not in flight. Mutations are applied to the REAL
// committed table so the test can't drift from the schema.

#include <catch2/catch_approx.hpp>
#include <cmath>
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "config/load_aircraft.h"
#include "config/load_controller.h"

#ifdef NDEBUG
#error \
    "SEADS gate requires an assert-live build (SPEC 6.1); configure with CMAKE_BUILD_TYPE=Debug"
#endif

namespace {

std::string slurp(const std::string& path) {
    std::ifstream f(path);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// Write `text` to a unique temp file and return its path.
std::string write_temp(const std::string& text, const char* tag) {
    const std::filesystem::path p =
        std::filesystem::temp_directory_path() /
        (std::string("seads_ctrl_") + tag + ".toml");
    std::ofstream(p) << text;
    return p.string();
}

std::string replace_all(std::string s, const std::string& from,
                        const std::string& to) {
    for (size_t i = s.find(from); i != std::string::npos;
         i = s.find(from, i + to.size())) {
        s.replace(i, from.size(), to);
    }
    return s;
}

}  // namespace

TEST_CASE("load_controller: the committed table loads and is sane") {
    const sim::AircraftParams ap =
        cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");
    const control::ControllerParams cp =
        cfg::load_controller_toml(SEADS_CONFIG_DIR "/controller.toml", ap);

    // Degrees converted to radians at the boundary.
    CHECK(cp.blend_lo > 0.08);  // 5 deg ~ 0.087 rad
    CHECK(cp.blend_lo < 0.10);
    // v5 rung D (Chad 2026-07-23 arcade energy ruling): 16.0 -> 32.0.
    CHECK(cp.n_max ==
          32.0);  // Â§7 performance tune (6.0 -> 10.0 -> 16.0 -> 32.0)
    // The critical-damping rule and the stall bound hold for the real table.
    CHECK(cp.K_w_pitch >= 4.0 * ap.I_pitch * cp.K_theta);
    CHECK(cp.aoa_max <= ap.Cl_max / ap.Cl_alpha);
    // Ease-back: RETIRED at the flown value (pilot ruling 2026-08-06 — the
    // release is instant, so the 300 ms mouse->aim suspension protects nothing
    // mechanical). 0 is the shipped value; the CQ2 cap still binds any re-arm.
    CHECK(cp.freelook_easeback_time == 0.0);
    CHECK(cp.freelook_easeback_time <= 0.300);
    // The double-tap ORIENT verb is retired too (redundant: every airborne
    // release fires the verb + the instant horizon roll).
    // Fly-4 2026-08-06: the double-tap is RESTORED (the pilot's manual echo of
    // the automatic release verb — instant, no dwell, no gates).
    CHECK(cp.orient_double_tap_s == 0.30);
    // Inverted righting carries NO added delay (same ruling): the rest
    // condition is the only wait. The rate knob is still the real mechanism.
    CHECK(cp.inverted_delay == 0.0);
    CHECK(cp.inverted_rate > 0.0);
    // v13 rest-edge camera recovery: a real dwell, on the deadzone's scale.
    CHECK(cp.horizon_recovery_rate > 0.0);
    CHECK(cp.horizon_recovery_rest_dwell >= 0.0);
    // Fly-5/fly-7 2026-08-06 ("engage a little sooner" / "trigger a little
    // sooner when I settle"): the camera's dwell is FASTER than the cascade's
    // deadzone dwell — the two dials are decoupled.
    CHECK(cp.horizon_recovery_rest_dwell == 0.05);
    // v13g ease-in (fly-7): the rest-edge roll ramps in, never launches at
    // full rate on the capture tick.
    CHECK(cp.horizon_recovery_ease_in > 0.0);
    // v13c arm gates (fly-2): inversion-class debt only, aim on the flight
    // path; both load deg -> rad at the boundary.
    constexpr double kPiLocal = 3.14159265358979323846;
    // Fly-4 2026-08-06: any debt above the finish epsilon rights at settled
    // rest — the settled gates (path_band/straight_max/dwell) do the guarding.
    CHECK(cp.horizon_recovery_arm_min == 0.0);
    // Fly-7 2026-08-06 (whole arm chain lowered): path_band 10 -> 15,
    // straight_max 6 -> 9; fly 2026-09-10 straight_max 9 -> 12 (Chad:
    // "trigger a bit sooner, not move faster").
    CHECK(cp.horizon_recovery_path_band ==
          Catch::Approx(15.0 * kPiLocal / 180.0));
    // v13d straightness gate: above the great-circle floor (v_redline/R), far
    // below any real turn.
    CHECK(cp.horizon_recovery_straight_max ==
          Catch::Approx(12.0 * kPiLocal / 180.0));
    // Mouse sensitivities load positive, deg -> rad at the boundary.
    CHECK(cp.aim_sensitivity > 0.0);
    CHECK(cp.aim_sensitivity < 0.01);  // 0.15 deg/px ~ 0.0026 rad/px
    CHECK(cp.freelook_orbit_sensitivity > 0.0);
    // MB-aim curve knobs: rates load in device px/s (NO deg->rad conversion â€”
    // they key a gain, not an angle; a rad() slip would shrink the knee ~57x
    // and curve every micro-adjust). The shipped table has the curve ON.
    CHECK(cp.aim_curve_knee >= 100.0);  // px/s scale, not radians scale
    CHECK(cp.aim_curve_rate_hi > cp.aim_curve_knee);
    CHECK(cp.aim_curve_gain_max >= 1.0);
    CHECK(cp.aim_curve_expo > 0.0);
    CHECK(cp.aim_curve_quant_px >= 0.0);
    // Freelook orbit swing limits load deg -> rad. S-freelook360 (Chad
    // 2026-07-17 "keep going around indefinitely full freedom"): the
    // committed table ships yaw_max = 0 = the UNLIMITED wrapped orbit (the
    // loader accepts [0, pi]; app/main.cpp branches to render::wrap_pi).
    // Pitch 89 -> the eye-below (belly-view) cap (the overhead side is
    // capped tighter in app/main.cpp).
    constexpr double kPi = 3.14159265358979323846;
    CHECK(cp.freelook_orbit_yaw_max == 0.0);
    CHECK(cp.freelook_orbit_pitch_max ==
          Catch::Approx(89.0 * kPi / 180.0).margin(1e-6));
    // S-globelook (v4 rung 3): tau loads in SECONDS (no rad conversion — a
    // rad() slip would shrink the coast 57x and the globe would feel dead);
    // the cap crosses deg/s -> rad/s. The shipped table has the mechanism ON
    // (0.2 s / 90 deg/s).
    CHECK(cp.freelook_inertia_tau == 0.2);
    CHECK(cp.freelook_inertia_cap ==
          Catch::Approx(90.0 * kPi / 180.0).margin(1e-9));
    // Rung-3 red-team P1: the coast-entry stillness dwell, SECONDS like tau
    // (a rad() slip would shrink it 57x and the staircase gate would
    // silently disarm). Shipped 0.075 s (~4-5 frames at 60 fps).
    CHECK(cp.freelook_inertia_dwell == 0.075);
    // MB-lean: gain is DIMENSIONLESS (deg/deg == rad/rad â€” a rad() slip would
    // shrink the lean 57x and the magnet would vanish silently); the cap is
    // an angle (deg -> rad).
    CHECK(cp.lean_gain == 8.0);  // Buttery Rung 1 2026-07-30 (Chad: "more
                                 // bank early on" — the file's pre-agreed
                                 // fallback): 6 -> 8. History 8->10->12->16->
                                 // 10->6->8; 16 flown + REJECTED bank-eager.
    CHECK(cp.lean_max == Catch::Approx(30.0 * kPi / 180.0).margin(1e-9));
    // S-leanlead (feel/yaw-bank-balance 2026-09-10): a dimensionless mix
    // weight. Walked back to 0.0 on 2026-09-12 (Chad: unwanted bank nosing
    // down and through a straight loop), then RESTORED to the flown 0.3 the
    // same day once the regression was attributed to the LEAD reading a
    // pure-pitch aim as lateral -- the fix is the structural gate below, not
    // the scalar. Walk-back order is lean_lead_lateral = false (== the
    // rejected v14), then 0.2 / 0.15 / 0.1, then 0 (the pre-v14 tree).
    CHECK(cp.lean_lead == 0.3);
    // S-leanlead-lateral: the horizon-lateral-share gate on the lead. Shipped
    // ON. lat_lo is sin(lean_max) EXACTLY -- see the wall below; it is the
    // geometry, not a tuned constant, so it is pinned as the identity.
    CHECK(cp.lean_lead_lateral == true);
    CHECK(cp.lean_lead_lat_lo == 0.50);
    CHECK(cp.lean_lead_lat_hi == 0.85);
    CHECK(cp.lean_lead_lat_lo ==
          Catch::Approx(std::sin(cp.lean_max)).margin(1e-12));
    // Aim-motion gate (rudder-flick Fly 6): SECONDS, not rad. Fly-7/8 A/B:
    // both states flown 2026-07-10, Chad chose the GATED build ("smaller
    // steps"); 0.15 is the flown-and-chosen value.
    CHECK(cp.deadzone_rest_dwell == 0.15);
    // S-yaw-magnet: frac dimensionless, band an ANGLE (deg -> rad; a
    // missing rad() would widen the relief 57x past blend_lo). Fly 13:
    // band 0.5 -> 1.5 deg (Chad: "more rudder authority... crabbing? yes!").
    CHECK(cp.coord_center_frac == 0.0);  // Fly 13: 0.25 -> 0.15; Rung Y1
                                         // 2026-07-11: 0.15 -> 0.05; Rung Y2:
                                         // 0.05 -> 0.02; Rung M1 (Chad RULED
                                         // "forget the crabbing, nose in the
                                         // MIDDLE"): 0.02 -> 0.0 — the
                                         // lateral standoff is fully closed
                                         // (rest park lateral 0.0000 at
                                         // every probed V)
    CHECK(cp.coord_center_band ==
          Catch::Approx(1.5 * kPi / 180.0).margin(1e-9));
}

TEST_CASE("load_controller: rejects out-of-range yaw-magnet relief knobs") {
    const sim::AircraftParams ap =
        cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");
    const std::string base = slurp(SEADS_CONFIG_DIR "/controller.toml");
    REQUIRE(base.find("center_frac = 0.0") != std::string::npos);
    REQUIRE(base.find("center_band = 1.5") != std::string::npos);
    // Negative frac would REVERSE the coordination near center (sign-flip).
    CHECK_THROWS(cfg::load_controller_toml(
        write_temp(replace_all(base, "center_frac = 0.0", "center_frac = -0.1"),
                   "coord_frac_neg"),
        ap));
    // frac > 1 would AMPLIFY coordination near center â€” not a relief.
    CHECK_THROWS(cfg::load_controller_toml(
        write_temp(replace_all(base, "center_frac = 0.0", "center_frac = 1.5"),
                   "coord_frac_big"),
        ap));
    // Armed relief with a zero band: smoothstep's divisor (silent NaN class).
    CHECK_THROWS(cfg::load_controller_toml(
        write_temp(replace_all(base, "center_band = 1.5", "center_band = 0.0"),
                   "coord_band_zero"),
        ap));
    // Band past blend_lo/2 leaks the relief toward bank-to-turn territory
    // where AT-16's skid wall lives.
    CHECK_THROWS(cfg::load_controller_toml(
        write_temp(replace_all(base, "center_band = 1.5", "center_band = 4.0"),
                   "coord_band_wide"),
        ap));
}

TEST_CASE("load_controller: aim_ff loads dimensionless and rejects bad gain") {
    const sim::AircraftParams ap =
        cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");
    const control::ControllerParams cp =
        cfg::load_controller_toml(SEADS_CONFIG_DIR "/controller.toml", ap);
    // S-aimff: gain is DIMENSIONLESS (rad/s per rad/s — a rad() slip would
    // shrink the lead 57x and the mechanism would vanish silently); tau is
    // SECONDS. The committed table ships the rung-1 fly values.
    CHECK(cp.aim_ff_gain == 0.3);
    CHECK(cp.aim_ff_tau == 0.01);

    const std::string base = slurp(SEADS_CONFIG_DIR "/controller.toml");
    REQUIRE(base.find("gain = 0.3") != std::string::npos);
    REQUIRE(base.find("tau  = 0.01") != std::string::npos);
    // Negative gain would ANTI-lead the moving aim (the sign-flip class).
    CHECK_THROWS(cfg::load_controller_toml(
        write_temp(replace_all(base, "gain = 0.3", "gain = -0.1"),
                   "aim_ff_gain_neg"),
        ap));
    // Past 2 the lead demand exceeds the PN-honest bound (research wall).
    CHECK_THROWS(cfg::load_controller_toml(
        write_temp(replace_all(base, "gain = 0.3", "gain = 2.5"),
                   "aim_ff_gain_big"),
        ap));
    // Negative tau is nonsense (0 = pass-through is the legal floor).
    CHECK_THROWS(cfg::load_controller_toml(
        write_temp(replace_all(base, "tau  = 0.01", "tau  = -0.01"),
                   "aim_ff_tau_neg"),
        ap));
}

TEST_CASE("load_controller: rejects out-of-range deadzone rest_dwell") {
    const sim::AircraftParams ap =
        cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");
    const std::string base = slurp(SEADS_CONFIG_DIR "/controller.toml");
    REQUIRE(base.find("rest_dwell = 0.15") != std::string::npos);
    // Negative dwell is nonsense.
    const std::string bad_neg =
        replace_all(base, "rest_dwell = 0.15", "rest_dwell = -0.1");
    CHECK_THROWS(
        cfg::load_controller_toml(write_temp(bad_neg, "dz_dwell_neg"), ap));
    // Past ~2 s the deadzone effectively never latches in flight and
    // "Inputs at rest = trim" silently disarms (the retune-disarm class).
    const std::string bad_big =
        replace_all(base, "rest_dwell = 0.15", "rest_dwell = 3.0");
    CHECK_THROWS(
        cfg::load_controller_toml(write_temp(bad_big, "dz_dwell_big"), ap));
    // Below the 0.1 s fps floor the gate breaks by frame rate (red-team
    // P2-1): the accumulator's (N-1) at-rest ticks per frame relatch the
    // deadzone mid-drag and the stairs return with the gate "on".
    const std::string bad_small =
        replace_all(base, "rest_dwell = 0.15", "rest_dwell = 0.05");
    CHECK_THROWS(
        cfg::load_controller_toml(write_temp(bad_small, "dz_dwell_small"), ap));
}

TEST_CASE("load_controller: rejects out-of-range MB-lean knobs") {
    const sim::AircraftParams ap =
        cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");
    const std::string base = slurp(SEADS_CONFIG_DIR "/controller.toml");
    REQUIRE(base.find("lean_gain = 8.0") != std::string::npos);
    REQUIRE(base.find("lean_max  = 30.0") != std::string::npos);
    // Negative gain = a REPELLING magnet (the sign-flip failure class).
    const std::string bad_gain =
        replace_all(base, "lean_gain = 8.0", "lean_gain = -1.0");
    CHECK_THROWS(
        cfg::load_controller_toml(write_temp(bad_gain, "lean_gain_neg"), ap));
    // A 50 deg "lean" is bank-to-turn territory, not fine tracking.
    const std::string bad_max =
        replace_all(base, "lean_max  = 30.0", "lean_max  = 50.0");
    CHECK_THROWS(
        cfg::load_controller_toml(write_temp(bad_max, "lean_max_over"), ap));
    // S-leanlead: a lead PAST the live target is a new oscillator; negative
    // would lag behind held_bank (the sign-flip class).
    REQUIRE(base.find("lean_lead = 0.3") != std::string::npos);
    const std::string bad_lead_hi =
        replace_all(base, "lean_lead = 0.3", "lean_lead = 1.5");
    CHECK_THROWS(cfg::load_controller_toml(
        write_temp(bad_lead_hi, "lean_lead_over"), ap));
    const std::string bad_lead_neg =
        replace_all(base, "lean_lead = 0.3", "lean_lead = -0.5");
    CHECK_THROWS(cfg::load_controller_toml(
        write_temp(bad_lead_neg, "lean_lead_neg"), ap));

    // S-leanlead-lateral (2026-09-12): the share edges. Both are fractions of
    // the total pointing error, so out of [0,1] is meaningless; and the
    // smoothstep needs lo < hi or its divide blows.
    REQUIRE(base.find("lean_lead_lat_lo = 0.50") != std::string::npos);
    REQUIRE(base.find("lean_lead_lat_hi = 0.85") != std::string::npos);
    const std::string bad_lat_lo_neg =
        replace_all(base, "lean_lead_lat_lo = 0.50", "lean_lead_lat_lo = -0.1");
    CHECK_THROWS(cfg::load_controller_toml(
        write_temp(bad_lat_lo_neg, "lean_lat_lo_neg"), ap));
    const std::string bad_lat_hi_over =
        replace_all(base, "lean_lead_lat_hi = 0.85", "lean_lead_lat_hi = 1.4");
    CHECK_THROWS(cfg::load_controller_toml(
        write_temp(bad_lat_hi_over, "lean_lat_hi_over"), ap));
    // Degenerate band (lo >= hi): the smoothstep would divide by zero.
    const std::string bad_lat_band =
        replace_all(base, "lean_lead_lat_hi = 0.85", "lean_lead_lat_hi = 0.50");
    CHECK_THROWS(cfg::load_controller_toml(
        write_temp(bad_lat_band, "lean_lat_band"), ap));
    // THE GEOMETRY TRIPWIRE (the "retune silently disarms" class): the gate
    // exists to read 0 for a pure-PITCH aim at every bank the lean can
    // command, and that aim reads share |sin(phi)| -- so lat_lo must stay at
    // or above sin(lean_max). Lowering lat_lo alone silently re-opens the
    // v14 regression across part of the shallow-bank band; so does RAISING
    // lean_max without re-deriving lat_lo. Both directions must fail loud.
    const std::string bad_lat_lo_under =
        replace_all(base, "lean_lead_lat_lo = 0.50", "lean_lead_lat_lo = 0.30");
    CHECK_THROWS(cfg::load_controller_toml(
        write_temp(bad_lat_lo_under, "lean_lat_lo_under"), ap));
    const std::string bad_lean_max_up =
        replace_all(base, "lean_max  = 30.0", "lean_max  = 40.0");
    CHECK_THROWS(cfg::load_controller_toml(
        write_temp(bad_lean_max_up, "lean_max_vs_lat_lo"), ap));
    // A typo in the flag must fail loud, never silently default to OFF (the
    // optional_bool contract).
    const std::string bad_lat_flag = replace_all(
        base, "lean_lead_lateral = true", "lean_lead_lateral = \"yes\"");
    CHECK_THROWS(cfg::load_controller_toml(
        write_temp(bad_lat_flag, "lean_lat_flag"), ap));
    // ABSENT keys => the structural OFF default (the landed v14 lead), never
    // a throw: an untouched toml must keep loading.
    const std::string no_lat_keys = replace_all(
        replace_all(replace_all(base, "lean_lead_lateral = true", ""),
                    "lean_lead_lat_lo = 0.50", ""),
        "lean_lead_lat_hi = 0.85", "");
    const control::ControllerParams defaulted =
        cfg::load_controller_toml(write_temp(no_lat_keys, "lean_lat_absent"), ap);
    CHECK(defaulted.lean_lead_lateral == false);
    CHECK(defaulted.lean_lead == 0.3);
}

TEST_CASE("load_controller: rejects a righting roll faster than p_max") {
    // The MB dial tripwire (the "retune silently disarms" class): past p_max
    // the inverted_rate clamp goes inert and the "slow righting roll" is
    // silently the p_max slam the mechanism exists to avoid. 300 > 260.
    const sim::AircraftParams ap =
        cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");
    const std::string base = slurp(SEADS_CONFIG_DIR "/controller.toml");
    REQUIRE(base.find("inverted_rate  = 180.0") != std::string::npos);
    const std::string bad =
        replace_all(base, "inverted_rate  = 180.0", "inverted_rate  = 300.0");
    CHECK_THROWS(
        cfg::load_controller_toml(write_temp(bad, "righting_over_pmax"), ap));
}

TEST_CASE("load_controller: rejects quant_px = 0 while the aim curve is on") {
    // The quant guard is the ONLY protection keeping fps-scaled integer-pixel
    // rate noise out of the precision zone (MB-aim diff red-team P2) â€” 0 with
    // the curve ON is the AT-15 "retune silently disarms the rail" class.
    const sim::AircraftParams ap =
        cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");
    const std::string base = slurp(SEADS_CONFIG_DIR "/controller.toml");
    REQUIRE(base.find("aim_curve_quant_px = 3.0") != std::string::npos);
    REQUIRE(base.find("aim_curve_gain_max = 2.5") !=
            std::string::npos);  // the shipped curve is ON
    const std::string bad = replace_all(base, "aim_curve_quant_px = 3.0",
                                        "aim_curve_quant_px = 0.0");
    const std::string path = write_temp(bad, "aim_quant_disarm");
    CHECK_THROWS(cfg::load_controller_toml(path, ap));
    // With the curve OFF the same 0 is legal (the guard has nothing to guard).
    const std::string off = replace_all(bad, "aim_curve_gain_max = 2.5",
                                        "aim_curve_gain_max = 1.0");
    const std::string opath = write_temp(off, "aim_quant_off_ok");
    CHECK_NOTHROW(cfg::load_controller_toml(opath, ap));
}

TEST_CASE("load_controller: rejects a freelook orbit pitch beyond the pole") {
    const sim::AircraftParams ap =
        cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");
    const std::string base = slurp(SEADS_CONFIG_DIR "/controller.toml");
    REQUIRE(base.find("freelook_orbit_pitch_max   = 89") != std::string::npos);
    // 91 deg > 90 -> past the straight-up/down pole (the loader range guard).
    const std::string bad = replace_all(base, "freelook_orbit_pitch_max   = 89",
                                        "freelook_orbit_pitch_max   = 91");
    const std::string path = write_temp(bad, "orbit_pitch_over_pole");
    CHECK_THROWS(cfg::load_controller_toml(path, ap));
}

TEST_CASE("load_controller: rejects an ease-back window past CQ2's 300 ms") {
    const sim::AircraftParams ap =
        cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");
    const std::string base = slurp(SEADS_CONFIG_DIR "/controller.toml");
    REQUIRE(base.find("easeback_time = 0.0") != std::string::npos);
    // 0.40 s > 0.30 s -> a smoothed camera basis suspended far too long,
    // and the held aim would feel frozen after the pilot came back (CQ2). The
    // shipped value is now the retired 0 (pilot ruling 2026-08-06), so the
    // mutation re-arms the window PAST the cap from there — the wall that
    // survives the retirement is the one this leg pins.
    const std::string bad =
        replace_all(base, "easeback_time = 0.0", "easeback_time = 0.40");
    const std::string path = write_temp(bad, "easeback_over_cap");
    CHECK_THROWS(cfg::load_controller_toml(path, ap));
}

TEST_CASE("load_controller: rejects a negative horizon rest dwell") {
    // v13 rest-edge camera recovery (pilot ruling 2026-08-06): the dwell may be
    // 0 (arm on the first still tick) but never negative — a negative is a typo
    // that would arm the capture before any rest was measured.
    const sim::AircraftParams ap =
        cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");
    const std::string base = slurp(SEADS_CONFIG_DIR "/controller.toml");
    // The [horizon_recovery] key specifically (the [deadzone] one ships the
    // same value and has its own wall).
    const std::string key = "rest_dwell = 0.05    # [s] mouse-still";
    REQUIRE(base.find(key) != std::string::npos);
    const std::string bad =
        replace_all(base, key, "rest_dwell = -0.01    # [s] mouse-still");
    const std::string path = write_temp(bad, "horizon_rest_dwell_negative");
    CHECK_THROWS(cfg::load_controller_toml(path, ap));
    // ...and 0 is accepted (the legal "arm immediately" arm).
    const std::string ok_toml =
        replace_all(base, key, "rest_dwell = 0.0    # [s] mouse-still");
    const std::string ok_path = write_temp(ok_toml, "horizon_rest_dwell_zero");
    CHECK_NOTHROW(cfg::load_controller_toml(ok_path, ap));
}

TEST_CASE("load_controller: rejects globe-inertia cap 0 while tau is on") {
    // S-globelook loader wall: with inertia_tau > 0 the cap must be a real
    // ceiling — cap = 0 clamps every seeded coast velocity to zero, an "on"
    // dial that silently disarms (the AT-15 class). A negative tau is a typo.
    const sim::AircraftParams ap =
        cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");
    const std::string base = slurp(SEADS_CONFIG_DIR "/controller.toml");
    REQUIRE(base.find("inertia_cap = 90.0") != std::string::npos);
    const std::string bad_cap =
        replace_all(base, "inertia_cap = 90.0", "inertia_cap = 0.0");
    CHECK_THROWS(
        cfg::load_controller_toml(write_temp(bad_cap, "inertia_cap_zero"), ap));
    REQUIRE(base.find("inertia_tau = 0.2") != std::string::npos);
    const std::string bad_tau =
        replace_all(base, "inertia_tau = 0.2", "inertia_tau = -0.2");
    CHECK_THROWS(
        cfg::load_controller_toml(write_temp(bad_tau, "inertia_tau_neg"), ap));
    // tau = 0 (mechanism OFF) is legal even with cap = 0 — the structural
    // default a hand-built params object carries.
    const std::string off =
        replace_all(replace_all(base, "inertia_tau = 0.2", "inertia_tau = 0.0"),
                    "inertia_cap = 90.0", "inertia_cap = 0.0");
    CHECK_NOTHROW(
        cfg::load_controller_toml(write_temp(off, "inertia_off"), ap));
    // Rung-3 red-team P1 dwell walls: a negative dwell is a typo; 0 is the
    // LEGAL coast-immediately arm (documented as re-opening the staircase).
    REQUIRE(base.find("inertia_dwell = 0.075") != std::string::npos);
    const std::string bad_dwell =
        replace_all(base, "inertia_dwell = 0.075", "inertia_dwell = -0.075");
    CHECK_THROWS(cfg::load_controller_toml(
        write_temp(bad_dwell, "inertia_dwell_neg"), ap));
    const std::string dwell0 =
        replace_all(base, "inertia_dwell = 0.075", "inertia_dwell = 0.0");
    CHECK_NOTHROW(cfg::load_controller_toml(
        write_temp(dwell0, "inertia_dwell_zero"), ap));
}

TEST_CASE("load_controller: rejects a table that violates critical damping") {
    const sim::AircraftParams ap =
        cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");
    const std::string base = slurp(SEADS_CONFIG_DIR "/controller.toml");
    REQUIRE(base.find("150000.0") != std::string::npos);
    // K_w_pitch far below 4*I_pitch*K_theta -> underdamped -> must not load.
    const std::string bad = replace_all(base, "150000.0", "1000.0");
    const std::string path = write_temp(bad, "underdamped");
    CHECK_THROWS(cfg::load_controller_toml(path, ap));
}

TEST_CASE("load_controller: rejects AoA_max above the plant stall alpha") {
    const sim::AircraftParams ap =
        cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");
    const std::string base = slurp(SEADS_CONFIG_DIR "/controller.toml");
    REQUIRE(base.find("= 20.0") !=
            std::string::npos);  // aoa_max/neg (14 -> 18 -> 20); "= 20.0"
                                 // avoids the "120.0" bank/roll substrings
    // 30 deg > stall alpha (Cl_max/Cl_alpha = 1.8/5.0 = 20.6 deg) -> protects
    // nothing. (Replaces both aoa_max and aoa_max_neg; both exceed the stall.)
    const std::string bad = replace_all(base, "= 20.0", "= 30.0");
    const std::string path = write_temp(bad, "aoa_over_stall");
    CHECK_THROWS(cfg::load_controller_toml(path, ap));
}

TEST_CASE("load_controller: rejects out-of-band capture dials (S-rimshot v2)") {
    // The universal capture's band checks (load_controller.cpp): each dial
    // has a stillborn/degenerate zone the loader must refuse — engage_frac
    // at/below the closed-loop decay-ratio floor never fires (the trigger is
    // silently dead); handback_frac <= 1 hands back on the engage tick
    // itself; break_frac <= 1 kills the event at its own honest apex; w_eps
    // <= 0 noise-engages on the coordination residual. A future deletion of
    // any check passes the whole suite otherwise (diff red-team P2-1).
    const sim::AircraftParams ap =
        cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");
    const std::string shipped = slurp(SEADS_CONFIG_DIR "/controller.toml");
    // Machine retired in the committed table (carry 0, Chad 2026-07-30 — "park
    // the cue ball behavior as retired for now"); this leg pins the PARKED
    // machinery's loader bands for the walk-back, and every band check below
    // is illegal-IF-ARMED, so `base` SELF-ARMS at the walk-back value
    // carry = 1.0. The shipped value itself is pinned in the off-arm leg at
    // the bottom of this case.
    REQUIRE(shipped.find("carry    = 0.0") != std::string::npos);
    const std::string base =
        replace_all(shipped, "carry    = 0.0", "carry    = 1.0");
    REQUIRE(base.find("engage_frac = 0.95") != std::string::npos);
    REQUIRE(base.find("handback_frac = 1.1") != std::string::npos);
    REQUIRE(base.find("break_frac = 1.25") != std::string::npos);
    REQUIRE(base.find("w_eps    = 0.4") != std::string::npos);
    CHECK_THROWS(cfg::load_controller_toml(
        write_temp(replace_all(base, "engage_frac = 0.95", "engage_frac = 1.2"),
                   "cap_engage_big"),
        ap));
    CHECK_THROWS(cfg::load_controller_toml(
        write_temp(replace_all(base, "engage_frac = 0.95", "engage_frac = 0.0"),
                   "cap_engage_zero"),
        ap));
    CHECK_THROWS(cfg::load_controller_toml(
        write_temp(
            replace_all(base, "handback_frac = 1.1", "handback_frac = 1.0"),
            "cap_handback_one"),
        ap));
    CHECK_THROWS(cfg::load_controller_toml(
        write_temp(replace_all(base, "break_frac = 1.25", "break_frac = 1.0"),
                   "cap_break_one"),
        ap));
    CHECK_THROWS(cfg::load_controller_toml(
        write_temp(replace_all(base, "w_eps    = 0.4", "w_eps    = 0.0"),
                   "cap_weps_zero"),
        ap));
    // v3 POOL BALL: glance_frac band (0, 1] — 0 makes EVERY arrival
    // direct-seek (the glance structurally dead while carry pretends
    // otherwise); above 1 the classifier exceeds the circle it scales.
    REQUIRE(base.find("glance_frac = 0.25") != std::string::npos);
    CHECK_THROWS(cfg::load_controller_toml(
        write_temp(replace_all(base, "glance_frac = 0.25", "glance_frac = 0.0"),
                   "cap_glance_zero"),
        ap));
    CHECK_THROWS(cfg::load_controller_toml(
        write_temp(replace_all(base, "glance_frac = 0.25", "glance_frac = 1.2"),
                   "cap_glance_big"),
        ap));
    // carry = 0 keeps every band unread (the structural-off arm loads even
    // with a dead dial elsewhere illegal-if-armed — sanity: the off arm).
    // RETIRED (Chad 2026-07-30): the SHIPPED table now IS the off arm —
    // "park the cue ball behavior as retired for now" (the machine and all
    // its dials stay for the walk-back, carry = 1.0). Pin the shipped value
    // so a silent re-arm fails loud, and build the off arm from the table
    // as committed.
    const std::string off =
        replace_all(shipped, "engage_frac = 0.95", "engage_frac = 0.0");
    CHECK_NOTHROW(cfg::load_controller_toml(write_temp(off, "cap_off"), ap));
}

TEST_CASE(
    "load_controller: [freelook] release_orient optional, default false") {
    const sim::AircraftParams ap =
        cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");
    const std::string base = slurp(SEADS_CONFIG_DIR "/controller.toml");
    // The shipped table carries the fly value ON (S-relorient, Chad
    // 2026-07-28; walk-back = one line to false).
    REQUIRE(base.find("release_orient = true") != std::string::npos);
    CHECK(cfg::load_controller_toml(SEADS_CONFIG_DIR "/controller.toml", ap)
              .freelook_release_orient);
    // ABSENT key -> default FALSE (the optional-with-default read, plan-audit
    // P3: an untouched/archived toml must stay legacy bit-identical, never
    // hard-fail). This is the differential-firewall arm's loader half.
    const std::string absent = replace_all(base, "release_orient = true", "");
    CHECK_FALSE(
        cfg::load_controller_toml(write_temp(absent, "relorient_absent"), ap)
            .freelook_release_orient);
    // Explicit false loads false (the walk-back line).
    CHECK_FALSE(cfg::load_controller_toml(
                    write_temp(replace_all(base, "release_orient = true",
                                           "release_orient = false"),
                               "relorient_false"),
                    ap)
                    .freelook_release_orient);
    // Present but NON-BOOLEAN is a hard error (a typo must fail loud, never
    // silently default — the optional() read rejects it).
    CHECK_THROWS(cfg::load_controller_toml(
        write_temp(
            replace_all(base, "release_orient = true", "release_orient = 1"),
            "relorient_nonbool"),
        ap));
}

TEST_CASE("load_controller: line_hold_ff shipped pin + wall (S-straightline)") {
    const sim::AircraftParams ap =
        cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");
    const std::string shipped = slurp(SEADS_CONFIG_DIR "/controller.toml");
    // Shipped-value pin: the axis-correction FF ships LIVE at 1.0 (the exact
    // kinematic complement); 0.0 is the fly kill-switch / golden baseline
    // arm, never the committed value (docs/straightline_thread.md).
    REQUIRE(shipped.find("line_hold_ff = 1.0") != std::string::npos);
    const control::ControllerParams cp =
        cfg::load_controller_toml(SEADS_CONFIG_DIR "/controller.toml", ap);
    CHECK(cp.line_hold_ff == 1.0);
    // Wall: [0, 2] — headroom to 2 for a deliberate over-correction fly,
    // never unbounded; negative flips the FF into a dip AMPLIFIER.
    CHECK_THROWS(cfg::load_controller_toml(
        write_temp(
            replace_all(shipped, "line_hold_ff = 1.0", "line_hold_ff = 3.0"),
            "linehold_big"),
        ap));
    CHECK_THROWS(cfg::load_controller_toml(
        write_temp(
            replace_all(shipped, "line_hold_ff = 1.0", "line_hold_ff = -0.5"),
            "linehold_neg"),
        ap));
}

// S-righthand (red-team P2): the shipped value and its wall had ZERO mentions
// in the loader suite, so a retune to 0 (silently disarming the veto) or past
// the felt ceiling would have landed unremarked.
TEST_CASE("loader: right_hand_rest is shipped live and walled") {
    const sim::AircraftParams ap =
        cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");
    const control::ControllerParams cp =
        cfg::load_controller_toml(SEADS_CONFIG_DIR "/controller.toml", ap);
    // The SHIPPED value: MB-right's hand-rest veto is armed.
    CHECK(cp.right_hand_rest == 0.25);
    // ...and the wall holds both ends (0 is legal -- it is the OFF arm).
    CHECK(cp.right_hand_rest >= 0.0);
    CHECK(cp.right_hand_rest <= 2.0);
}

// S-tremor (kernel v17 candidate): the net-displacement window, walled like
// right_hand_rest above and mutated against the REAL committed table. A
// NEGATIVE window would invert the leak into a divergent accumulator (the
// measure would run away instead of forgetting); past ~1 s the window outlives
// the hand-rest ramp it feeds and would score a sweep that ENDED as still
// live. 0 is legal -- it is the structural OFF arm.
TEST_CASE("loader: hand_net_window is shipped live and walled (S-tremor)") {
    const sim::AircraftParams ap =
        cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");
    const std::string shipped = slurp(SEADS_CONFIG_DIR "/controller.toml");
    REQUIRE(shipped.find("hand_net_window = 0.20") != std::string::npos);
    const control::ControllerParams cp =
        cfg::load_controller_toml(SEADS_CONFIG_DIR "/controller.toml", ap);
    CHECK(cp.hand_net_window == 0.20);
    CHECK(cp.hand_net_window >= 0.0);
    CHECK(cp.hand_net_window <= 1.0);
    // MUTATION: delete the check() in load_controller.cpp and both of these
    // stop throwing.
    CHECK_THROWS(cfg::load_controller_toml(
        write_temp(replace_all(shipped, "hand_net_window = 0.20",
                               "hand_net_window = -0.05"),
                   "netwin_neg"),
        ap));
    CHECK_THROWS(cfg::load_controller_toml(
        write_temp(replace_all(shipped, "hand_net_window = 0.20",
                               "hand_net_window = 1.5"),
                   "netwin_big"),
        ap));
    // The OFF arm must still LOAD (0 is the walk-back, not an error), and it
    // must land as a structural zero, not as the default of a missing key.
    const control::ControllerParams off = cfg::load_controller_toml(
        write_temp(replace_all(shipped, "hand_net_window = 0.20",
                               "hand_net_window = 0.0"),
                   "netwin_off"),
        ap);
    CHECK(off.hand_net_window == 0.0);
    CHECK(off.right_hand_rest == 0.25);  // the v16 dial is untouched by it
}
