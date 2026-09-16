// FIX-F1 (docs/ai_phase2_fix_spec.md, Opus probe attribution
// D:\seads_sandboxes\ai-probes\f1\ATTRIBUTION.md): THE ENERGY BAIL KILLS
// EVERY FIGHT AGAINST THE PLAYER.
//
// PROBE FACT: bfm.h's Offensive->Extend specific-energy bail
// (e_delta < -bfm_extend_energy_m, shipped at 600 m) tripped on 100% of
// foe==player ticks -- Chad flies ~259 m/s vs the bandit's ~110, a
// permanent ~2800 m deficit -- so every bandit left the ONLY guns-hot mode
// exactly min_dwell after entering it and never came back (the recovery
// threshold was unreachable). Second-order: Intercept flew dl.speed +
// intercept_speed_bump alone while only Offensive/Yoyo got the 2026-07-26
// pursuit closing-speed bump, so a stern chase (the mode that actually does
// the closing) never closed on a fast player.
//
// THE FIX (as shipped — narrower than Opus's original recommendation):
// scenario.toml's bfm_extend_energy_m 600 -> 3000, and drone/bfm.h's
// Intercept speed law keyed to the TARGET'S speed (fly intercept_speed_bump
// faster than the target, floored at the pre-fix value, capped at the
// pursuit ceiling — see the bfm.h comment). bfm_attack_range_m STAYS 1200:
// the probe re-run showed the extend fix alone reopens the fire chain
// (wants_fire 0 -> 90 at tape speed), and 750 broke the game-loop
// certificate's slow-target kill chain for no measured gain.
//
// This TU is standalone from test_bfm.cpp (off-limits per the spec) and
// follows its OPEN-LOOP discipline (S6 lesson): transition predicates are
// driven on fixed synthetic geometry, never inferred from a closed-loop
// chase. TEST_CASE names are STRICTLY ASCII (the repo's own non-ASCII-name
// trap: such a name silently never runs under ctest).

#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "config/load_aircraft.h"
#include "drone/bfm.h"
#include "drone/drone.h"
#include "sim/aero.h"
#include "sim/world.h"

#ifdef NDEBUG
#error "SEADS gate requires an assert-live build (SPEC 6.1)"
#endif

namespace {

const sim::AircraftParams kAp =
    cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");

constexpr double kPi = 3.14159265358979323846;

glm::dvec3 nose_of(const sim::SimState& s) {
    return s.orientation * glm::dvec3{0.0, 0.0, -1.0};
}

// The BfmDials aggregate drone::tick builds each engaged tick (the
// test_bfm.cpp precedent, duplicated here since that TU is off-limits).
bfm::BfmDials dials_of(const drone::DroneParams& dp) {
    return bfm::BfmDials{dp.pursue_k_az,
                         dp.pursue_k_el,
                         dp.pursue_max_bank,
                         dp.pursue_max_gamma,
                         dp.pursue_lead_speed,
                         dp.pursue_lead_max_s,
                         dp.speed,
                         dp.pursue_speed_bump};
}

}  // namespace

// ---------------------------------------------------------------------------
// Leg 8: the energy bail no longer auto-loses a fight against a much faster
// player. Geometry pinned at range 500 m, bandit at 110 m/s, player at
// 259 m/s (Chad's own tape numbers) -- with g = 9.81 (aircraft.toml) this is
// EXACTLY the probe's e_delta ~ -2800: (110^2 - 259^2) / (2*9.81) = -2802.0.
// Nose-on, matched heading (player flying the same way, faster) keeps
// closure negative (no overshoot predictor, so the machine cannot detour
// through Yoyo -- the leg isolates the energy-bail/frustration exit only).
// ---------------------------------------------------------------------------
TEST_CASE("offensive persists against a much faster player") {
    drone::DroneParams dp;
    bfm::BfmParams bp;
    bp.enabled = true;
    bp.extend_energy_m = 3000.0;  // FIX-F1: config/scenario.toml's new value
    bp.frustration_s = 8.0;       // the shipped table (game-AI-R5)
    const bfm::BfmDials dl = dials_of(dp);
    const double dt = kAp.sim_dt;

    drone::DroneState d = drone::spawn_drone(kAp, dp, 0, 1);
    d.curr.velocity = nose_of(d.curr) * 110.0;  // the bandit's ~110 m/s

    sim::SimState fast_player;
    fast_player.position = d.curr.position + nose_of(d.curr) * 500.0;
    fast_player.orientation = d.curr.orientation;
    fast_player.velocity = nose_of(d.curr) * 259.0;  // Chad's ~259 m/s

    const bfm::BfmGeom g = bfm::bfm_geom(d.curr, fast_player, kAp);
    REQUIRE(g.range == Catch::Approx(500.0).margin(0.5));
    REQUIRE(g.closure < 0.0);  // diverging: the overshoot predictor cannot arm
    // The tape's own deficit: well past the OLD 600 m bail (would have
    // tripped this and every subsequent tick) but comfortably inside the
    // NEW 3000 m band (does not bail at all -- the fix).
    CHECK(g.e_delta < -600.0);
    REQUIRE(g.e_delta == Catch::Approx(-2802.0).margin(10.0));  // altitude
                                        // differs slightly between the two
                                        // positions (nose_of has a small
                                        // vertical component at spawn)
    REQUIRE(g.e_delta > -bp.extend_energy_m);

    bfm::BfmState st;
    st.mode = bfm::BfmState::Mode::Offensive;
    const long long min_dwell_ticks =
        static_cast<long long>(std::ceil(bp.min_dwell_s / dt));
    const long long frustration_ticks =
        static_cast<long long>(std::ceil(bp.frustration_s / dt));
    REQUIRE(3 * min_dwell_ticks < frustration_ticks);  // non-vacuous ordering

    bool guns_hot_seen = false;
    bool entered_extend = false;
    long long steps = 0;
    const long long cap = frustration_ticks * 3;  // generous ceiling
    for (; steps < cap; ++steps) {
        const bfm::BfmCmd cmd =
            bfm::bfm_step(st, d.curr, fast_player, bp, dl, kAp, dt);
        if (st.mode == bfm::BfmState::Mode::Offensive && cmd.guns_hot)
            guns_hot_seen = true;
        // MUTATION: extend_energy_m reverted to the old 600 fails HERE --
        // the bail trips within min_dwell_ticks and this REQUIRE breaks well
        // before 3*min_dwell_ticks.
        if (steps < 3 * min_dwell_ticks)
            REQUIRE(st.mode == bfm::BfmState::Mode::Offensive);
        if (st.mode == bfm::BfmState::Mode::Extend) {
            entered_extend = true;
            break;
        }
    }
    REQUIRE(guns_hot_seen);  // the guns-hot mode actually ran, not skipped
    REQUIRE(entered_extend);  // it does eventually leave -- via frustration
    // Within ~1.5x frustration_s of actual fight time (the test_bfm.cpp
    // frustration leg's own slack bound).
    REQUIRE(static_cast<double>(steps) <=
            1.5 * static_cast<double>(frustration_ticks));
}

// ---------------------------------------------------------------------------
// Leg 9: Intercept's closing bump is keyed to the TARGET'S OWN SPEED —
// catch_up = min(pursue_speed_bump, max(0, |v_target| - dl.speed)), then
// speed_target = dl.speed + max(intercept_speed_bump, catch_up). A fast
// target is chased hot; a slow target gets the pre-fix command BIT-IDENTICAL
// (the unconditional max() variant made 175 m/s intercepts overshoot slow
// certificate targets into a 0-damage kill chain — bisected on the game-loop
// certificate, 2026-08-07).
// ---------------------------------------------------------------------------
TEST_CASE("intercept inherits the closing bump") {
    drone::DroneParams dp;
    bfm::BfmParams bp;
    bp.enabled = true;
    bp.intercept_speed_bump = 25.0;

    drone::DroneState d = drone::spawn_drone(kAp, dp, 0, 1);
    sim::SimState player;
    // Well beyond attack_range_m so the mode stays Intercept this tick.
    player.position = d.curr.position + nose_of(d.curr) * 5000.0;
    player.orientation = d.curr.orientation;
    player.velocity = glm::dvec3{0.0};

    // Case A: a FAST target (faster than own cruise by more than the pursuit
    // bump) -> the catch-up saturates at the (already align^2-faded, per
    // BfmDials' contract) pursue_speed_bump: the mode that does the closing
    // finally gets the closing speed.
    {
        bfm::BfmState st;
        st.mode = bfm::BfmState::Mode::Intercept;
        bfm::BfmDials dl = dials_of(dp);
        dl.pursue_speed_bump = 60.0;  // > intercept_speed_bump (25)
        player.velocity = nose_of(d.curr) * (dl.speed + 150.0);  // deficit 150
        const bfm::BfmCmd cmd =
            bfm::bfm_step(st, d.curr, player, bp, dl, kAp, kAp.sim_dt);
        REQUIRE(st.mode == bfm::BfmState::Mode::Intercept);  // non-vacuous
        CHECK(cmd.speed_target == Catch::Approx(dl.speed + 60.0));
    }

    // Case A3 (red-team P1-1's kill case): a target slightly faster than own
    // cruise (deficit UNDER the pursuit bump) must be chased with a real
    // CLOSING margin — |v_target| + intercept_speed_bump — not merely
    // speed-matched (the first-draft min(bump, deficit) law equalized speed
    // and never closed; this leg fails on that law).
    {
        bfm::BfmState st;
        st.mode = bfm::BfmState::Mode::Intercept;
        bfm::BfmDials dl = dials_of(dp);
        dl.pursue_speed_bump = 60.0;
        player.velocity = nose_of(d.curr) * (dl.speed + 10.0);  // deficit 10
        const bfm::BfmCmd cmd =
            bfm::bfm_step(st, d.curr, player, bp, dl, kAp, kAp.sim_dt);
        REQUIRE(st.mode == bfm::BfmState::Mode::Intercept);
        CHECK(cmd.speed_target ==
              Catch::Approx(dl.speed + 10.0 + bp.intercept_speed_bump));
    }

    // Case A2: a SLOW target under a hot pursuit bump -> catch_up is 0 and
    // the command stays the pre-fix intercept value (the certificate-pinned
    // slow case: no overshoot regression).
    {
        bfm::BfmState st;
        st.mode = bfm::BfmState::Mode::Intercept;
        bfm::BfmDials dl = dials_of(dp);
        dl.pursue_speed_bump = 60.0;
        player.velocity = glm::dvec3{0.0};  // parked target
        const bfm::BfmCmd cmd =
            bfm::bfm_step(st, d.curr, player, bp, dl, kAp, kAp.sim_dt);
        REQUIRE(st.mode == bfm::BfmState::Mode::Intercept);
        CHECK(cmd.speed_target ==
              Catch::Approx(dl.speed + bp.intercept_speed_bump));
    }

    // Case B: patrol-grade (pursue bump faded/zeroed, e.g. a wide-off-nose
    // pursuit or an unengaged bandit) -> bit-identical to the pre-fix value,
    // since max(intercept_speed_bump, 0) == intercept_speed_bump.
    {
        bfm::BfmState st;
        st.mode = bfm::BfmState::Mode::Intercept;
        bfm::BfmDials dl = dials_of(dp);
        dl.pursue_speed_bump = 0.0;
        const bfm::BfmCmd cmd =
            bfm::bfm_step(st, d.curr, player, bp, dl, kAp, kAp.sim_dt);
        REQUIRE(st.mode == bfm::BfmState::Mode::Intercept);
        CHECK(cmd.speed_target == Catch::Approx(dl.speed + bp.intercept_speed_bump));
    }
}
