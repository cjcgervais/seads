// RUNG E1 — CLOSE THE KILL CHAIN (docs/ENEMY_AI_E1_E2_SPEC.md).
//
// Six new dials, each with an OFF value that must reproduce the pre-E1 fleet
// EXACTLY. This TU is the knob-off differential for all six plus the
// mechanism legs; the whole-chain acceptance (rounds actually aimed at the
// player over a real 10-minute engagement) is P-A in test_killchain_probe.cpp.
//
// DIFFERENTIAL DISCIPLINE. "Bit-identical when off" is proven ANALYTICALLY
// here, not by comparing two arms of the new code against each other: each
// off-arm assertion recomputes the LEGACY closed form (the exact expression
// the pre-E1 source evaluated) and requires bit equality (==, not Approx), and
// each is paired with an ON arm that must DIFFER — the fixture-no-op
// discipline, so an off-arm that passed because the mechanism never ran would
// be caught.
//
// TEST_CASE / SECTION names are STRICTLY ASCII (the repo's own trap: a
// non-ASCII name silently never runs under ctest).

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
#include "sim/environment.h"
#include "sim/world.h"
#include "world/heightfield.h"

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

// The BfmDials aggregate drone::tick builds each engaged tick (align2 and the
// air ramps take their struct defaults, exactly as a bare tick would).
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

// The LEGACY Intercept ceiling, transcribed from the pre-E1 bfm.h source. It
// is align-DEPENDENT (dl.pursue_speed_bump arrives already faded), which is
// exactly why the off-value must be structural (<= 0) and not a numeric
// "175-equivalent" default.
double legacy_intercept_speed(const bfm::BfmParams& bp,
                              const bfm::BfmDials& dl, double v_player) {
    return std::clamp(v_player + bp.intercept_speed_bump,
                      dl.speed + bp.intercept_speed_bump,
                      std::max(dl.speed + bp.intercept_speed_bump,
                               dl.speed + dl.pursue_speed_bump));
}

}  // namespace

// ---------------------------------------------------------------------------
// E1.1 — THE CHASE CEILING (intercept_speed_mps)

TEST_CASE("e1.1 knob off: the chase ceiling is the legacy align-dependent "
          "expression, bit for bit") {
    drone::DroneParams dp;
    bfm::BfmParams bp;
    bp.enabled = true;
    bp.attack_range_m = 1200.0;

    drone::DroneState d = drone::spawn_drone(kAp, dp, 0, 1);
    sim::SimState player;
    player.orientation = d.curr.orientation;
    // Well outside attack_range_m so the mode holds Intercept for the tick.
    player.position = d.curr.position + nose_of(d.curr) * 5000.0;

    bool any_raised = false;
    for (double v_p : {0.0, 100.0, 200.0, 300.0}) {
        for (double align : {0.0, 0.5, 0.8, 1.0}) {
            for (double pursue_bump : {0.0, 25.0, 90.0}) {
                const double a2 = align * align;
                bfm::BfmDials dl = dials_of(dp);
                // drone::tick hands bfm an ALREADY-faded pursuit bump plus the
                // raw align^2 — mirror that contract exactly.
                dl.pursue_speed_bump = pursue_bump * a2;
                dl.align2 = a2;
                player.velocity = nose_of(d.curr) * v_p;
                const double v_len = glm::length(player.velocity);

                bfm::BfmParams off = bp;
                off.intercept_speed_mps = 0.0;  // STRUCTURAL off
                bfm::BfmState st_off;
                st_off.mode = bfm::BfmState::Mode::Intercept;
                const bfm::BfmCmd c_off = bfm::bfm_step(
                    st_off, d.curr, player, off, dl, kAp, kAp.sim_dt);
                REQUIRE(st_off.mode == bfm::BfmState::Mode::Intercept);
                REQUIRE(c_off.speed_target ==
                        legacy_intercept_speed(off, dl, v_len));

                bfm::BfmParams on = bp;
                on.intercept_speed_mps = 265.0;  // the shipped value
                bfm::BfmState st_on;
                st_on.mode = bfm::BfmState::Mode::Intercept;
                const bfm::BfmCmd c_on = bfm::bfm_step(st_on, d.curr, player,
                                                       on, dl, kAp, kAp.sim_dt);
                // The raised ceiling fades by the SAME align^2 the corner-speed
                // law uses, and can never LOWER the shipped pursuit bump.
                const double raised = (on.intercept_speed_mps - dl.speed) * a2;
                const double ceiling =
                    dl.speed + std::max(dl.pursue_speed_bump, raised);
                REQUIRE(c_on.speed_target ==
                        std::clamp(v_len + on.intercept_speed_bump,
                                   dl.speed + on.intercept_speed_bump,
                                   std::max(dl.speed + on.intercept_speed_bump,
                                            ceiling)));
                REQUIRE(c_on.speed_target >= c_off.speed_target);
                if (c_on.speed_target > c_off.speed_target) any_raised = true;
            }
        }
    }
    // Non-vacuity: the ON arm actually raised the command somewhere in the
    // sweep (an off-arm identity that held because nothing ever changed would
    // certify nothing).
    REQUIRE(any_raised);
}

TEST_CASE("e1.1 the corner-speed law survives: a nose-on chase gets the "
          "raised ceiling, a 90-degree-off one does not") {
    drone::DroneParams dp;
    bfm::BfmParams bp;
    bp.enabled = true;
    bp.attack_range_m = 1200.0;
    bp.intercept_speed_mps = 265.0;

    drone::DroneState d = drone::spawn_drone(kAp, dp, 0, 1);
    sim::SimState player;
    player.orientation = d.curr.orientation;
    player.position = d.curr.position + nose_of(d.curr) * 5000.0;
    player.velocity = nose_of(d.curr) * 300.0;

    bfm::BfmDials nose_on = dials_of(dp);
    nose_on.pursue_speed_bump = 90.0;  // align^2 == 1
    nose_on.align2 = 1.0;
    bfm::BfmState a;
    a.mode = bfm::BfmState::Mode::Intercept;
    const bfm::BfmCmd on_nose =
        bfm::bfm_step(a, d.curr, player, bp, nose_on, kAp, kAp.sim_dt);
    CHECK(on_nose.speed_target == Catch::Approx(265.0));

    bfm::BfmDials broadside = dials_of(dp);
    broadside.pursue_speed_bump = 0.0;  // align^2 == 0 (90 deg off)
    broadside.align2 = 0.0;
    bfm::BfmState b;
    b.mode = bfm::BfmState::Mode::Intercept;
    const bfm::BfmCmd on_broad =
        bfm::bfm_step(b, d.curr, player, bp, broadside, kAp, kAp.sim_dt);
    // "chase fast, turn slow": fully off-nose the ceiling collapses back onto
    // the pre-E1 floor.
    CHECK(on_broad.speed_target ==
          Catch::Approx(dp.speed + bp.intercept_speed_bump));
}

TEST_CASE("e1.1 the raised ceiling is armed only OUTSIDE the merge range") {
    drone::DroneParams dp;
    bfm::BfmParams bp;
    bp.enabled = true;
    bp.attack_range_m = 2200.0;
    bp.intercept_speed_mps = 265.0;
    bp.min_dwell_s = 0.5;

    drone::DroneState d = drone::spawn_drone(kAp, dp, 0, 1);
    sim::SimState player;
    player.orientation = d.curr.orientation;
    player.velocity = nose_of(d.curr) * 300.0;

    bfm::BfmDials dl = dials_of(dp);
    dl.pursue_speed_bump = 90.0;
    dl.align2 = 1.0;

    // Inside the merge: the legacy ceiling governs (Intercept has not yet
    // handed over because min_dwell has not elapsed on tick 0).
    player.position = d.curr.position + nose_of(d.curr) * 1000.0;
    bfm::BfmState in;
    in.mode = bfm::BfmState::Mode::Intercept;
    const bfm::BfmCmd c_in =
        bfm::bfm_step(in, d.curr, player, bp, dl, kAp, kAp.sim_dt);
    REQUIRE(in.mode == bfm::BfmState::Mode::Intercept);
    CHECK(c_in.speed_target ==
          legacy_intercept_speed(bp, dl, glm::length(player.velocity)));

    // Outside it: the chase ceiling.
    player.position = d.curr.position + nose_of(d.curr) * 4000.0;
    bfm::BfmState out;
    out.mode = bfm::BfmState::Mode::Intercept;
    const bfm::BfmCmd c_out =
        bfm::bfm_step(out, d.curr, player, bp, dl, kAp, kAp.sim_dt);
    CHECK(c_out.speed_target == Catch::Approx(265.0));
}

TEST_CASE("e1.1 knob off: the Extend speed command is the legacy value, and "
          "the shipped dial raises it") {
    drone::DroneParams dp;
    bfm::BfmParams bp;
    bp.enabled = true;
    bp.attack_range_m = 2200.0;

    drone::DroneState d = drone::spawn_drone(kAp, dp, 0, 1);
    sim::SimState player;
    player.orientation = d.curr.orientation;
    player.position = d.curr.position + nose_of(d.curr) * 5000.0;
    player.velocity = nose_of(d.curr) * 300.0;
    bfm::BfmDials dl = dials_of(dp);
    dl.pursue_speed_bump = 90.0;
    dl.align2 = 1.0;

    bfm::BfmParams off = bp;
    off.intercept_speed_mps = 0.0;
    bfm::BfmState st_off;
    st_off.mode = bfm::BfmState::Mode::Extend;
    const bfm::BfmCmd c_off =
        bfm::bfm_step(st_off, d.curr, player, off, dl, kAp, kAp.sim_dt);
    REQUIRE(st_off.mode == bfm::BfmState::Mode::Extend);
    // The legacy Extend command is dl.speed + intercept_speed_bump ALONE — NOT
    // the Intercept clamp (a max() applied unconditionally would have silently
    // handed Extend the 175 m/s pursuit bump).
    REQUIRE(c_off.speed_target == dl.speed + off.intercept_speed_bump);

    bfm::BfmParams on = bp;
    on.intercept_speed_mps = 265.0;
    bfm::BfmState st_on;
    st_on.mode = bfm::BfmState::Mode::Extend;
    const bfm::BfmCmd c_on =
        bfm::bfm_step(st_on, d.curr, player, on, dl, kAp, kAp.sim_dt);
    CHECK(c_on.speed_target == Catch::Approx(265.0));
}

// ---------------------------------------------------------------------------
// E1.3 — THE ENERGY BAIL (reenter_closure_mps)

TEST_CASE("e1.3 the closure-sign exit re-enters a fight the absolute energy "
          "test can never release, and knob off keeps the legacy timeout") {
    drone::DroneParams dp;
    bfm::BfmParams bp;
    bp.enabled = true;
    bp.extend_min_s = 8.0;
    bp.extend_max_s = 16.0;
    bp.reenter_energy_m = 200.0;  // the pre-E1 (unreachable) value
    const bfm::BfmDials dl = dials_of(dp);
    const double dt = kAp.sim_dt;

    drone::DroneState d = drone::spawn_drone(kAp, dp, 0, 1);
    const glm::dvec3 n = nose_of(d.curr);
    d.curr.velocity = n * 110.0;  // the bandit's own tape speed

    sim::SimState player;
    player.position = d.curr.position + n * 3000.0;
    player.orientation = d.curr.orientation;
    player.velocity = -n * 259.0;  // flying AT the bandit: closure is huge

    const bfm::BfmGeom g = bfm::bfm_geom(d.curr, player, kAp);
    // The fixture must show the baseline defect: a permanent, structurally
    // unreachable energy deficit with a strongly CLOSING geometry.
    REQUIRE(g.closure > 300.0);
    REQUIRE(g.e_delta < -bp.reenter_energy_m);

    const auto ticks_in_extend = [&](double reenter_closure) {
        bfm::BfmParams p = bp;
        p.reenter_closure_mps = reenter_closure;
        bfm::BfmState st;
        st.mode = bfm::BfmState::Mode::Extend;
        long long n_ticks = 0;
        const long long cap =
            bfm::dwell_ticks(p.extend_max_s, dt) * 4;  // generous
        for (; n_ticks < cap; ++n_ticks) {
            bfm::bfm_step(st, d.curr, player, p, dl, kAp, dt);
            if (st.mode != bfm::BfmState::Mode::Extend) break;
        }
        return n_ticks;
    };

    const long long off_ticks = ticks_in_extend(0.0);   // knob off
    const long long on_ticks = ticks_in_extend(25.0);   // shipped
    INFO("extend length: off=" << off_ticks << " on=" << on_ticks << " ticks");
    // Knob off: the only reachable exit is the extend_max_s timeout, exactly
    // as before (the recovery branch is dead against this deficit).
    REQUIRE(off_ticks == bfm::dwell_ticks(bp.extend_max_s, dt) + 1);
    // Knob on: it re-enters the moment the commit floor is served.
    REQUIRE(on_ticks == bfm::dwell_ticks(bp.extend_min_s, dt));
    // And it can NEVER cut the commit floor short.
    REQUIRE(on_ticks >= bfm::dwell_ticks(bp.extend_min_s, dt));
}

// ---------------------------------------------------------------------------
// E1.4 — THE C1 MODE-CHANGE BLEND (mode_blend_s)

TEST_CASE("e1.4 knob off: a mode change issues the new mode's own command, "
          "and the blend slides it out of the previous one") {
    drone::DroneParams dp;
    bfm::BfmParams bp;
    bp.enabled = true;
    bp.attack_range_m = 2200.0;
    bp.min_dwell_s = 0.5;
    bp.intercept_speed_mps = 0.0;
    bfm::BfmDials dl = dials_of(dp);
    dl.pursue_speed_bump = 90.0;
    dl.align2 = 1.0;
    const double dt = kAp.sim_dt;

    drone::DroneState d = drone::spawn_drone(kAp, dp, 0, 1);
    const glm::dvec3 n = nose_of(d.curr);
    d.curr.velocity = n * 110.0;
    sim::SimState player;
    player.orientation = d.curr.orientation;
    player.position = d.curr.position + n * 1500.0;  // inside attack_range
    player.velocity = n * 120.0;  // matched-ish: no overshoot predictor

    // Drive Intercept -> Offensive (the handover fires once min_dwell elapses)
    // on both arms and capture the command on the transition tick plus the one
    // before it.
    const auto run = [&](double blend_s, double& pre_speed, double& at_speed) {
        bfm::BfmParams p = bp;
        p.mode_blend_s = blend_s;
        bfm::BfmState st;
        st.mode = bfm::BfmState::Mode::Intercept;
        double last = 0.0;
        for (int i = 0; i < 4000; ++i) {
            const bfm::BfmCmd c =
                bfm::bfm_step(st, d.curr, player, p, dl, kAp, dt);
            if (st.mode == bfm::BfmState::Mode::Offensive) {
                pre_speed = last;
                at_speed = c.speed_target;
                return true;
            }
            last = c.speed_target;
        }
        return false;
    };

    double off_pre = 0.0, off_at = 0.0, on_pre = 0.0, on_at = 0.0;
    REQUIRE(run(0.0, off_pre, off_at));
    REQUIRE(run(0.7, on_pre, on_at));

    // Knob off: the transition tick issues Offensive's OWN command, a STEP off
    // the Intercept value (the pre-E1 behaviour, bit for bit).
    REQUIRE(off_at == dl.speed + dl.pursue_speed_bump);
    REQUIRE(off_at != off_pre);
    // Knob on: the transition tick issues the PREVIOUS command exactly (w == 0
    // at the start of a smoothstep), i.e. no step at all.
    REQUIRE(on_at == on_pre);
    REQUIRE(on_at != off_at);
}

TEST_CASE("e1.4 the blend completes: the command reaches the new mode's own "
          "value and stops moving") {
    drone::DroneParams dp;
    bfm::BfmParams bp;
    bp.enabled = true;
    bp.attack_range_m = 2200.0;
    bp.min_dwell_s = 0.5;
    bp.mode_blend_s = 0.7;
    bfm::BfmDials dl = dials_of(dp);
    dl.pursue_speed_bump = 90.0;
    dl.align2 = 1.0;
    const double dt = kAp.sim_dt;

    drone::DroneState d = drone::spawn_drone(kAp, dp, 0, 1);
    const glm::dvec3 n = nose_of(d.curr);
    d.curr.velocity = n * 110.0;
    sim::SimState player;
    player.orientation = d.curr.orientation;
    player.position = d.curr.position + n * 1500.0;
    player.velocity = n * 120.0;

    bfm::BfmState st;
    st.mode = bfm::BfmState::Mode::Intercept;
    bool entered = false;
    long long since = 0;
    double final_speed = 0.0;
    for (int i = 0; i < 4000; ++i) {
        const bfm::BfmCmd c = bfm::bfm_step(st, d.curr, player, bp, dl, kAp, dt);
        if (entered) ++since;
        if (!entered && st.mode == bfm::BfmState::Mode::Offensive)
            entered = true;
        final_speed = c.speed_target;
        if (entered && since > bfm::dwell_ticks(bp.mode_blend_s, dt) + 2) break;
    }
    REQUIRE(entered);
    // Past the blend window the command is Offensive's own, unblended.
    REQUIRE(final_speed == dl.speed + dl.pursue_speed_bump);
}

// ---------------------------------------------------------------------------
// E1.2 — THE SNAPSHOT GATE (snapshot_range_m)

TEST_CASE("e1.2 knob off: pursue reports no snapshot; the shipped band opens "
          "the shot and structurally forbids anything beyond it") {
    drone::DroneParams dp;
    dp.fire_range_max = 600.0;  // the pre-E1 band ceiling

    drone::DroneState d = drone::spawn_drone(kAp, dp, 0, 1);
    const glm::dvec3 n = nose_of(d.curr);
    d.curr.velocity = n * 120.0;  // coordinated: flying down its own nose

    sim::SimState player;
    player.orientation = d.curr.orientation;
    player.velocity = d.curr.velocity;  // zero relative motion: lead == target

    // 800 m: beyond the pre-E1 fire band, inside the snapshot band.
    player.position = d.curr.position + n * 800.0;
    {
        const drone::PursueCmd off = drone::pursue(d.curr, player, dp);
        REQUIRE_FALSE(off.fire);      // baseline defect: no shot at 800 m
        REQUIRE_FALSE(off.snapshot);  // and the gate is structurally absent

        drone::DroneParams on = dp;
        on.snapshot_range_m = 900.0;
        const drone::PursueCmd c = drone::pursue(d.curr, player, on);
        REQUIRE_FALSE(c.fire);  // the BFM-gated band is unchanged
        REQUIRE(c.snapshot);    // the snapshot takes it
    }

    // 1000 m: beyond the snapshot band. This is the REQUIREMENT — the band is
    // a structural bound on where a round can ever be spawned, so no round is
    // lobbed at long range at a drone foe either.
    player.position = d.curr.position + n * 1000.0;
    {
        drone::DroneParams on = dp;
        on.snapshot_range_m = 900.0;
        const drone::PursueCmd c = drone::pursue(d.curr, player, on);
        REQUIRE_FALSE(c.snapshot);
        REQUIRE_FALSE(c.fire);
    }

    // Inside fire_range_min: the spawn-on-top guard applies to the snapshot
    // exactly as it does to the gated shot.
    player.position = d.curr.position + n * 40.0;
    {
        drone::DroneParams on = dp;
        on.snapshot_range_m = 900.0;
        const drone::PursueCmd c = drone::pursue(d.curr, player, on);
        REQUIRE_FALSE(c.snapshot);
    }
}

TEST_CASE("e1.2 the snapshot bypasses the BFM guns-hot veto but never the "
          "safety mutes") {
    drone::DroneParams dp;
    dp.maverick.enabled = false;
    dp.bfm.enabled = true;
    dp.bfm.attack_range_m = 2200.0;
    dp.fire_range_max = 900.0;
    dp.snapshot_range_m = 900.0;

    drone::DroneState d = drone::spawn_drone(kAp, dp, 0, 1);
    const glm::dvec3 n = nose_of(d.curr);
    d.curr.velocity = n * 120.0;
    d.engaged = true;
    // Force the guns-COLD mode: Extend never allows a shot on the gated path.
    d.bfm.mode = bfm::BfmState::Mode::Extend;
    d.bfm.mode_ticks = 1;

    sim::SimState player;
    player.orientation = d.curr.orientation;
    player.velocity = d.curr.velocity;
    player.position = d.curr.position + n * 500.0;

    drone::DroneState armed = d;
    drone::tick(armed, kAp, dp, /*env=*/nullptr, &player);
    REQUIRE(armed.bfm.mode == bfm::BfmState::Mode::Extend);  // still guns-cold
    REQUIRE(armed.wants_fire);  // the snapshot took the shot anyway

    drone::DroneParams off = dp;
    off.snapshot_range_m = 0.0;
    drone::DroneState muted = d;
    drone::tick(muted, kAp, off, /*env=*/nullptr, &player);
    REQUIRE(muted.bfm.mode == bfm::BfmState::Mode::Extend);
    REQUIRE_FALSE(muted.wants_fire);  // knob off: the pre-E1 silence

    // MUTE ORDERING (normative): every later block still clears wants_fire
    // AFTER the pursuit branch set it. The terrain-avoid pull-up is the
    // sharpest case — a snapshot must never outrank the safety mute.
    {
        world::HeightField hf;
        hf.w = 8;
        hf.h = 4;
        hf.R = kAp.R;
        hf.relief_scale = 4000.0;
        hf.u_offset = 0.0;
        hf.px.assign(static_cast<std::size_t>(hf.w) * hf.h, 0);
        sim::Environment env;
        env.ground = &hf;
        env.ground_params = sim::GroundParams{};

        const glm::dvec3 up = glm::normalize(d.curr.position);
        drone::DroneState low = d;
        // 100 m AGL: well inside avoid_agl_enter_m, so the guard latches on
        // this very tick and owns the command.
        low.curr.position = up * (hf.radius_at(up) + 100.0);
        low.prev = low.curr;
        sim::SimState near_player = player;
        near_player.position = low.curr.position + nose_of(low.curr) * 500.0;
        near_player.velocity = low.curr.velocity;
        drone::tick(low, kAp, dp, &env, &near_player);
        REQUIRE(low.terrain_avoid_engaged);  // non-vacuous: the mute ran
        REQUIRE_FALSE(low.wants_fire);
    }
}

// ---------------------------------------------------------------------------
// E1.1 — THE AUTOTHROTTLE DEFICIT FEEDFORWARD (throttle_ff)

TEST_CASE("e1.1 knob off: the autothrottle is the legacy P law, bit for bit; "
          "the feedforward closes the droop and never overshoots") {
    drone::DroneParams dp;
    dp.maverick.enabled = false;
    dp.turn_time = 0.0;  // pure straight patrol: target_gamma == 0
    dp.speed = 200.0;    // a setpoint well above the drone's current speed

    const glm::dvec3 up = glm::normalize(glm::dvec3{0.55, 0.15, 0.82});
    const glm::dvec3 hdg =
        glm::normalize(glm::cross(up, glm::dvec3{0.2, 0.9, 0.4}));
    drone::DroneState d;
    d.curr = drone::level_state_at(dp, up * (kAp.R + 3000.0), hdg);
    // A 20 m/s deficit: the legacy P law is at 0.8 throttle here, NOT already
    // saturated, so the feedforward has somewhere to go (a bigger deficit
    // would have both arms pinned at 1.0 and certify nothing).
    d.curr.velocity = hdg * 180.0;
    d.prev = d.curr;
    d.hp = dp.hp;

    const double V0 = glm::length(d.curr.velocity);

    drone::DroneState off = d;
    drone::DroneParams off_dp = dp;  // throttle_ff defaults to 0.0
    drone::tick(off, kAp, off_dp, /*env=*/nullptr, /*player=*/nullptr);
    const float legacy = static_cast<float>(
        std::clamp(drone::kCruiseTrim + drone::kSpeedP * (dp.speed - V0), 0.0,
                   dp.throttle));
    REQUIRE(off.last_inputs.throttle == legacy);

    drone::DroneState on = d;
    drone::DroneParams on_dp = dp;
    on_dp.throttle_ff = 0.08;
    drone::tick(on, kAp, on_dp, /*env=*/nullptr, /*player=*/nullptr);
    REQUIRE(on.last_inputs.throttle > off.last_inputs.throttle);

    // The feedforward is DEFICIT-only: at (and above) the setpoint it is
    // identically zero, so it can never drive the drone past its own command.
    drone::DroneState at_speed = d;
    at_speed.curr.velocity = hdg * dp.speed;
    at_speed.prev = at_speed.curr;
    drone::DroneState at_speed_off = at_speed;
    drone::tick(at_speed, kAp, on_dp, nullptr, nullptr);
    drone::tick(at_speed_off, kAp, off_dp, nullptr, nullptr);
    REQUIRE(at_speed.last_inputs.throttle == at_speed_off.last_inputs.throttle);

    drone::DroneState fast = d;
    fast.curr.velocity = hdg * (dp.speed + 60.0);
    fast.prev = fast.curr;
    drone::DroneState fast_off = fast;
    drone::tick(fast, kAp, on_dp, nullptr, nullptr);
    drone::tick(fast_off, kAp, off_dp, nullptr, nullptr);
    REQUIRE(fast.last_inputs.throttle == fast_off.last_inputs.throttle);
}

// ---------------------------------------------------------------------------
// E1.4 — THE BANK-RATE SLEW (bank_slew_dps)

TEST_CASE("e1.4 knob off: the bank slew is structurally absent; armed, it "
          "bounds the commanded bank rate and resets on respawn") {
    drone::DroneParams dp;
    dp.maverick.enabled = false;
    dp.bfm.enabled = false;  // isolate bare pursuit's bang-bang roll command

    const glm::dvec3 up = glm::normalize(glm::dvec3{0.55, 0.15, 0.82});
    const glm::dvec3 hdg =
        glm::normalize(glm::cross(up, glm::dvec3{0.2, 0.9, 0.4}));
    const glm::dvec3 right = glm::normalize(glm::cross(hdg, up));

    drone::DroneState base;
    base.curr = drone::level_state_at(dp, up * (kAp.R + 3000.0), hdg);
    base.prev = base.curr;
    base.hp = dp.hp;
    base.engaged = true;

    // A player hard off the beam: pursue's k_az 3.0 saturates the bank cap
    // instantly, and the side flips every 3 s, so the commanded bank is a
    // square wave between +/- pursue_max_bank -- the bang-bang the slew exists
    // to smooth.
    sim::SimState player;
    player.orientation = base.curr.orientation;
    player.velocity = glm::dvec3{0.0};

    drone::DroneState off = base;
    drone::DroneState on = base;
    drone::DroneParams on_dp = dp;
    on_dp.bank_slew_dps = 90.0;
    const double step = on_dp.bank_slew_dps * kPi / 180.0 * kAp.sim_dt;

    const int ticks = static_cast<int>(12.0 / kAp.sim_dt);
    double prev_cmd = 0.0;
    bool first = true;
    double worst_rate = 0.0;
    for (int i = 0; i < ticks; ++i) {
        const double phase = std::floor(static_cast<double>(i) * kAp.sim_dt / 3.0);
        const double side = (static_cast<long long>(phase) % 2 == 0) ? 1.0 : -1.0;
        player.position = base.curr.position + right * (side * 1500.0);
        drone::tick(off, kAp, dp, nullptr, &player);
        drone::tick(on, kAp, on_dp, nullptr, &player);
        if (!first) {
            worst_rate =
                std::max(worst_rate, std::abs(on.bank_slew_cmd - prev_cmd));
        }
        prev_cmd = on.bank_slew_cmd;
        first = false;
    }
    INFO("worst commanded-bank step = " << worst_rate << " rad, limit " << step);
    // Knob off: the slew state is never touched (structurally skipped).
    REQUIRE(off.bank_slew_cmd == 0.0);
    REQUIRE_FALSE(off.bank_slew_init);
    // Armed: the commanded bank never moves faster than the limit...
    REQUIRE(on.bank_slew_init);
    REQUIRE(worst_rate <= step + 1e-12);
    // ...and it actually bound something (non-vacuity: the two arms diverge).
    REQUIRE(glm::length(on.curr.position - off.curr.position) > 1.0);

    // The latch is reset on respawn, like every other one (a reborn bandit
    // must not slew out of a bank it held kilometres away).
    drone::respawn_in_place(on, kAp, on_dp);
    REQUIRE(on.bank_slew_cmd == 0.0);
    REQUIRE_FALSE(on.bank_slew_init);
}
