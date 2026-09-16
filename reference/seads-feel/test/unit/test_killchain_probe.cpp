// PROBE P-A — "THE ACE GETS SHOT AT" (docs/ENEMY_AI_E1_E2_SPEC.md, ACCEPTANCE).
//
// THE MEASURED PROBLEM (tapes 83-88, ~81 min of Chad's own flying): 22 enemy
// rounds TOTAL, 9 of them aimed at the player, 2 hits, zero player deaths ever
// recorded. Five gates starved the guns (G1 closure, G2 phase gap, G3 cone vs
// LOS rate, G4 energy bail, GRACE). RUNG E1 opens them; this probe is the
// acceptance instrument.
//
// PRIMARY-DATA LAW. The probe reproduces the REAL chain, not a convenient
// slice of it:
//   * REAL LOADERS — aircraft.toml / scenario.toml / game.toml, so the dials
//     under test are the dials that ship (never a hand-built DroneParams).
//   * REAL ORDER — combat::assign_foes -> per-drone drone::tick against ITS
//     OWN foe -> combat::enemy_fire_tick -> combat::combat_player_tick, the
//     exact sequence app/instructor_tick.h runs.
//   * REAL WORLD — a live sim::Environment with ground AND the two-faction
//     atmosphere, so terrain avoidance and the flyable-air ceiling both bite.
//   * A STEPPED PLAYER. The player is advanced kinematically every tick with
//     its position, velocity, heading and orientation all mutually consistent
//     — never drone::level_state_at's stamped cruise velocity on a state the
//     fixture never moves. That artifact (docs/killchain_handoff.md, "THE
//     FIXTURE PHANTOM") manufactured a defect that survived two full aiming-law
//     rewrites; a parked target must carry zero velocity or be honestly
//     stepped, and this one is honestly stepped.
//
// The probe runs the SAME ensemble of engagements twice: a PRE arm (every E1
// dial at its off-value and the pre-E1 table restored) and a POST arm (the
// shipped scenario.toml). PRE is the baseline the improvement is measured
// against, and it is also the fixture-no-op guard: if PRE already shot the
// player to bits the mechanism would be certifying nothing.
//
// TEST_CASE names are STRICTLY ASCII (a non-ASCII name silently never runs).

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "combat/conquest.h"
#include "combat/kill.h"
#include "combat/raid.h"
#include "config/load_aircraft.h"
#include "config/load_game.h"
#include "config/load_scenario.h"
#include "drone/drone.h"
#include "sim/aero.h"
#include "sim/environment.h"
#include "sim/fields.h"
#include "sim/world.h"
#include "world/faction_bubbles.h"
#include "world/heightfield.h"

#ifdef NDEBUG
#error "SEADS gate requires an assert-live build (SPEC 6.1)"
#endif

namespace {

constexpr double kPi = 3.14159265358979323846;

const sim::AircraftParams kAp =
    cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");
const cfg::GameParams kGame =
    cfg::load_game_toml(SEADS_CONFIG_DIR "/game.toml", kAp);
const cfg::ScenarioParams kScen =
    cfg::load_scenario_toml(SEADS_CONFIG_DIR "/scenario.toml", kAp);

// The tape baseline this rung is measured against: 9 rounds aimed at the
// player in ~81 minutes of Chad's own flying.
constexpr double kTapeRoundsAtPlayer = 9.0;
constexpr double kTapeMinutes = 81.0;

// Flat terrain (the test_conquest_furball.cpp fixture) so the ground is the
// SAME crash surface sim::step reads, without GIS relief confounding the
// engagement geometry.
world::HeightField uniform_field(double elev_m, double relief = 6000.0) {
    world::HeightField hf;
    hf.w = 8;
    hf.h = 4;
    hf.R = kAp.R;
    hf.relief_scale = relief;
    hf.u_offset = 0.0;
    const double f = std::min(std::max(elev_m / relief, 0.0), 1.0);
    hf.px.assign(static_cast<std::size_t>(hf.w) * hf.h,
                 static_cast<std::uint16_t>(f * 65535.0 + 0.5));
    return hf;
}

sim::GroundParams test_gp() {
    sim::GroundParams gp;
    gp.slope_limit_cos = std::cos(15.0 * kPi / 180.0);
    gp.friction = 0.08;
    gp.max_sink_ms = 5.0;
    gp.normal_probe_m = 60.0;
    gp.contact_height_m = 1.5;
    return gp;
}

// What one arm measured.
struct ArmResult {
    long long rounds_at_player = 0;   // spawned by a drone whose foe IS the player
    long long rounds_total = 0;       // spawned by anyone (the AI-vs-AI volume)
    double max_fire_range_m = 0.0;    // furthest target range at any spawn
    long long player_hits = 0;        // damaging enemy rounds on the player
    long long bfm_transitions = 0;    // mode changes on player-foe drones
    double player_foe_minutes = 0.0;  // drone-minutes spent assigned to the player
    double best_sustained_mps = 0.0;  // fastest speed held >= kSustainS in full air
    int crashes = 0;                  // drone terrain strikes over the run
    double min_range_m = 1e18;        // closest any player-foe drone ever got
    long long wants_fire_ticks = 0;   // player-foe drone-ticks with a solution
    long long in_band_ticks = 0;      // player-foe drone-ticks inside the band
    double best_cone_cos = -1.0;      // best nose-on cos seen while in band
    long long coord_in_band = 0;      // in-band ticks that were coordinated
    // RUNG E7.4 — THE POINTING COLUMNS, read off the E8.1 fire-gate WITNESS
    // (`DroneState::w_cone_cos`, written by the shipped gate's own expression
    // against the full ballistic LEAD point) rather than re-derived against the
    // raw LOS as best_cone_cos above is. ★ WHY THEY EXIST: rounds_at_player is
    // a rare-event count and spec E7.I's noise floor eats anything under 2x.
    // These are DENSE per-tick columns of the very quantity a pointing rung is
    // about, so they resolve where the rare-event columns cannot.
    // RUNG E10 -- THE CRASH CENSUS. Two guesses at the crash mechanism (the
    // pull-up trigger, then an altitude fade on the cap) were both WRONG, and
    // the crash cost is REAL (null floor at cap 78 spans 0.219-0.266 across
    // arms that cannot differ, against 0.063 pre-E10). So record the state the
    // aeroplane was ACTUALLY in the tick before it died, instead of guessing a
    // third time.
    long long crash_engaged = 0;      // ...was fighting the player
    long long crash_avoid_armed = 0;  // ...with the forced pull-up ALREADY on
    double crash_agl_sum = 0.0;       // mean AGL the tick before
    double crash_gamma_sum = 0.0;     // mean flight-path angle (deg, -ve = down)
    double crash_speed_sum = 0.0;
    long long crash_mode[7] = {0, 0, 0, 0, 0, 0, 0};
    long long gate_in_band = 0;       // in-band ticks that evaluated the gate
    long long cone_open_in_band = 0;  // of those, ticks the CONE clause was open
    double sum_cone_cos = 0.0;        // summed witness cone cos over gate_in_band
};

// The pre-E1 table: every RUNG E1 dial at its OFF value, and the E1.2/E1.3
// retunes reverted to what tape 88 flew. This is the BASELINE arm.
drone::DroneParams pre_e1(const drone::DroneParams& shipped) {
    drone::DroneParams dp = shipped;
    dp.bfm.intercept_speed_mps = 0.0;    // E1.1: legacy align-dependent ceiling
    dp.throttle_ff = 0.0;                // E1.1: legacy autothrottle P law
    dp.snapshot_range_m = 0.0;           // E1.2: no snapshot gate
    dp.bank_slew_dps = 0.0;              // E1.4: no bank-rate limit
    dp.bfm.mode_blend_s = 0.0;           // E1.4: step mode changes
    dp.bfm.reenter_closure_mps = 0.0;    // E1.3: no closure-sign exit
    dp.bfm.attack_range_m = 1200.0;      // E1.2 retune reverted
    dp.bfm.min_dwell_s = 2.0;            // E1.2 retune reverted
    dp.fire_range_max = 600.0;           // E1.2 retune reverted
    dp.bfm.extend_energy_m = 3000.0;     // E1.3 retune reverted
    dp.bfm.reenter_energy_m = 200.0;     // E1.3 retune reverted
    dp.bfm.frustration_s = 8.0;          // E1.3 retune reverted
    // Every LATER rung's dial is off in a pre-E1 arm by definition; E7.4's was
    // the one that needed saying out loud, because it ships non-zero (rung E9)
    // and a baseline silently carrying it is a differential against nothing.
    dp.pursue_track_pitch_gain = 0.0;    // E7.4 structural off
    return dp;
}

// A speed must be held this long continuously to count as SUSTAINED (a
// one-tick dive spike is not an achieved intercept speed). "Fastest sustained
// speed" is measured against a ladder of candidate floors rather than a
// windowed minimum: floor F counts as achieved once some player-chasing bandit
// held v >= F for kSustainS continuously in full air, and the statistic is the
// highest achieved floor. Coarse by construction (5 m/s rungs) and never
// flattering — it reports a speed genuinely held, not a peak.
constexpr double kSustainS = 2.0;
constexpr double kFloorLoMps = 150.0;
constexpr double kFloorStepMps = 5.0;
constexpr int kNumFloors = 41;  // 150 .. 350 m/s (255 is a rung of the ladder)

// How many independent engagements make up one arm. A gun solution against a
// 275 m/s ace is a RARE, chaotic event -- a single 10-minute engagement is one
// or two bursts, and any perturbation anywhere (a different bank-slew rate, a
// different blend) re-rolls which merges happen at all. Measured: single-run
// counts swung 0-10 across dial settings with no monotone response, i.e. the
// single run measures luck, not mechanism. The arm is therefore an ENSEMBLE of
// deterministically-varied engagements (different player entry heading, turn
// phase and altitude phase -- no rng anywhere), summed. kEnsemble * 10 minutes
// is sized at ~the 81 minutes of tape 83-88 so the two are at least the same
// ORDER of exposure; the tape is still a different fixture (see the headline
// leg) and the requirement is PRE vs POST inside this one.
constexpr int kEnsemble = 8;

// ---------------------------------------------------------------------------
// One 10-minute engagement. `variant` selects the deterministic script phase.
// `ensemble_n` is the number of engagements the phase rotation is spread
// across. It defaults to kEnsemble so every existing caller is unchanged;
// the noise-floor work passes a larger number to sample the SAME script
// family more finely (E7 build session, docs spec "E7.I").
ArmResult fly_arm(const drone::DroneParams& dp, double minutes, int variant,
                  int ensemble_n = kEnsemble) {
    ArmResult out;

    const world::HeightField hf = uniform_field(0.0);
    std::vector<sim::AtmosphereField::Bubble> bubbles;
    const world::FactionGrowth grow[2]{};
    world::build_faction_bubbles(kGame.atmosphere, grow, bubbles);
    sim::AtmosphereField af;
    af.deck_agl_m = kGame.atmosphere.deck_agl_m;
    af.deck_soft_m = kGame.atmosphere.deck_soft_m;
    af.bubbles = bubbles;
    sim::Environment env;
    env.ground = &hf;
    env.ground_params = test_gp();
    env.atm = &af;
    // env.tunnels stays null: the maverick tunnel machine is structurally
    // skipped, so this arm measures the SURFACE kill chain (rung E1) with no
    // run scheduling pulling pilots out of the fight. The stope offensive is
    // rung E2's probe, not this one.

    // The player raids the SUDBURY zone, so he is the VALLEY faction and the
    // five SUDBURY pilots (indices 0..4) are his enemies.
    const int player_faction = combat::CQ_VALLEY;
    const glm::dvec3 sud = glm::normalize(world::kSudburyCenterDir);
    const glm::dvec3 val = glm::normalize(world::kValleyCenterDir);

    // The fleet: the conquest per-faction disc scatter app/main.cpp uses.
    // The SAME placement doubles as the crash-respawn relocation below —
    // app/instructor_tick.h's FIX-F2 (place_in_faction_air): drone::tick's own
    // crash branch respawns via the stock +X scatter, which for a conquest
    // fleet lands kilometres outside every bubble in thin air, where the drone
    // mushes and crashes again forever (95 dc events per 14 min on the tape).
    // A probe that inherited that pen would be measuring the respawn bug, not
    // the kill chain.
    const double golden = kPi * (3.0 - std::sqrt(5.0));
    constexpr double kConquestSpreadM = 5000.0;
    const auto faction_state = [&](int i) {
        const bool valley = combat::maverick_faction(i) == combat::CQ_VALLEY;
        const glm::dvec3 c = valley ? val : sud;
        const int slot = combat::team_slot(i);
        const double frac = (static_cast<double>(slot) + 0.5) /
                            combat::team_size(combat::maverick_faction(i));
        const double ang = (kConquestSpreadM / kAp.R) * std::sqrt(frac);
        const double brg = golden * static_cast<double>(i);
        glm::dvec3 ref{0.0, 1.0, 0.0};
        if (std::abs(glm::dot(ref, c)) > 0.9) ref = glm::dvec3{1.0, 0.0, 0.0};
        const glm::dvec3 east = glm::normalize(glm::cross(ref, c));
        const glm::dvec3 north = glm::cross(c, east);
        const glm::dvec3 dir = glm::normalize(
            std::cos(ang) * c +
            std::sin(ang) * (std::cos(brg) * east + std::sin(brg) * north));
        const glm::dvec3 heading = std::cos(brg) * east + std::sin(brg) * north;
        return drone::level_state_at(
            dp, dir * (hf.radius_at(dir) + dp.spawn_alt), heading);
    };

    std::vector<drone::DroneState> fleet;
    for (int i = 0; i < combat::kNumMavericks; ++i)
        fleet.push_back(drone::spawn_drone(kAp, dp, i, combat::kNumMavericks));
    for (int i = 0; i < static_cast<int>(fleet.size()); ++i) {
        fleet[i].curr = faction_state(i);
        fleet[i].prev = fleet[i].curr;
    }

    // ---- THE STEPPED PLAYER ------------------------------------------------
    // 275 m/s (inside Chad's measured 259 mean / 362 peak band) flying an ACE's
    // course over the Sudbury dome, not a metronome: a slow mean circuit
    // (~8 km radius, comfortably inside the 17.4 x 13.05 km dome so he stays in
    // full air and in contact) with a hard turn alternating on and off every
    // 20 s, plus a slow altitude weave so the geometry is never planar.
    //
    // THE SCRIPT IS LOAD-BEARING and was measured, not guessed: a permanent
    // 3 km-radius 2.5 g circle -- the first script tried -- is an LOS-rate wall
    // no bandit can point through (best nose-on cos 0.97 vs the 0.9703 gate,
    // zero solutions in 25 drone-minutes), so it would have graded the fire
    // cone, not the kill chain. An ace grants merges and stern chases between
    // his hard turns; those are the seconds E1 exists to convert.
    constexpr double kPlayerSpeed = 275.0;
    constexpr double kTurnMeanRate = 0.034;  // [rad/s] the ~8 km mean circuit
    constexpr double kTurnHardRate = 0.050;  // [rad/s] the alternating hard turn
    constexpr double kTurnBlockS = 20.0;     // [s] hard-turn on/off block
    constexpr double kAltMeanM = 2500.0;     // [m] AGL
    constexpr double kAltSwingM = 300.0;     // [m]
    constexpr double kAltPeriodS = 40.0;

    const double var = static_cast<double>(variant);
    glm::dvec3 p_up = sud;
    glm::dvec3 p_hdg = glm::normalize(glm::cross(p_up, glm::dvec3{0.0, 1.0, 0.2}));
    p_hdg = glm::normalize(
        glm::angleAxis(2.0 * kPi * var / static_cast<double>(ensemble_n), p_up) *
        p_hdg);
    sim::SimState player;
    {
        const glm::dvec3 pos = p_up * (hf.radius_at(p_up) + kAltMeanM);
        player.position = pos;
        player.velocity = kPlayerSpeed * p_hdg;
        player.last_vhat = p_hdg;
        const glm::dvec3 right = glm::normalize(glm::cross(p_hdg, p_up));
        player.orientation =
            glm::normalize(glm::quat_cast(glm::dmat3{right, p_up, -p_hdg}));
    }

    combat::CombatWorld cw;
    cw.setup = kScen.combat;
    cw.damage_params = kScen.damage;
    cw.params.hit_radius_m = dp.hit_radius_m;
    cw.player_hp = combat::summary_hp(cw.damage);

    const int max_engaged =
        combat::difficulty_params(cw.setup.difficulty).max_engaged;
    const double dt = kAp.sim_dt;
    const long long ticks = static_cast<long long>(minutes * 60.0 / dt);

    std::vector<int> prev_mode(fleet.size(), -1);
    std::vector<double> cd_before(fleet.size(), 0.0);
    // hold[i][f] = consecutive ticks drone i has held at least floor f.
    std::vector<std::vector<long long>> hold(
        fleet.size(), std::vector<long long>(kNumFloors, 0));
    int best_floor = -1;
    const long long sustain_ticks =
        static_cast<long long>(std::ceil(kSustainS / dt));

    for (long long t = 0; t < ticks; ++t) {
        // --- step the player -------------------------------------------
        const sim::SimState player_prev = player;
        p_up = glm::normalize(player.position);
        p_hdg = glm::normalize(p_hdg - glm::dot(p_hdg, p_up) * p_up);
        const double time_s = static_cast<double>(t) * dt;
        const long long block = static_cast<long long>(
            std::floor((time_s + 7.0 * var) / kTurnBlockS));
        const double turn_rate =
            kTurnMeanRate +
            (block % 2 == 0 ? kTurnHardRate : -kTurnHardRate);
        p_hdg = glm::normalize(glm::angleAxis(turn_rate * dt, p_up) * p_hdg);
        const double alt =
            kAltMeanM +
            kAltSwingM *
                std::sin(2.0 * kPi * (time_s + 5.0 * var) / kAltPeriodS);
        const glm::dvec3 axis = glm::normalize(glm::cross(p_up, p_hdg));
        glm::dvec3 npos =
            glm::angleAxis(kPlayerSpeed * dt / kAp.R, axis) * player.position;
        const glm::dvec3 nup = glm::normalize(npos);
        npos = nup * (hf.radius_at(nup) + alt);
        player.position = npos;
        player.velocity = (npos - player_prev.position) / dt;  // consistent
        p_hdg = glm::normalize(p_hdg - glm::dot(p_hdg, nup) * nup);
        player.last_vhat = glm::normalize(player.velocity);
        {
            const glm::dvec3 right = glm::normalize(glm::cross(p_hdg, nup));
            player.orientation =
                glm::normalize(glm::quat_cast(glm::dmat3{right, nup, -p_hdg}));
        }

        // --- the REAL app order ----------------------------------------
        combat::assign_foes(fleet, player, player_faction, max_engaged, dp);

        // The bubble-containment leash, armed exactly as
        // app/instructor_tick.h arms it (each pilot to its OWN faction
        // ellipse, live growth-scaled). Without it an unengaged bandit
        // patrols out of its own dome and never comes back, which would
        // quietly measure "how far the fleet wandered" instead of the kill
        // chain. drone::tick suppresses it for a live fight by itself.
        for (drone::DroneState& d : fleet) {
            if (d.inert) continue;
            drone::BubbleLeash lz;
            glm::dvec3 cdir{0.0, 1.0, 0.0};
            glm::dvec3 maj{0.0};
            double a = 0.0, b = 0.0;
            world::faction_ellipse(combat::maverick_faction(d.spawn_index),
                                   grow, cdir, maj, a, b);
            if (a > 0.0 && b > 0.0) {
                lz.enabled = true;
                lz.center_dir = cdir;
                lz.major_axis = maj;
                lz.a_m = a;
                lz.b_m = b;
            }
            d.leash = lz;
        }

        std::vector<sim::SimState> foe_snap;
        foe_snap.reserve(fleet.size());
        for (const drone::DroneState& d : fleet) foe_snap.push_back(d.curr);
        for (std::size_t i = 0; i < fleet.size(); ++i) {
            drone::DroneState& d = fleet[i];
            if (d.inert) continue;
            const sim::SimState* target = nullptr;
            if (d.foe == drone::kFoePlayer)
                target = &player;
            else if (d.foe >= 0 && d.foe < static_cast<int>(foe_snap.size()))
                target = &foe_snap[d.foe];
            const sim::SimState pre_tick = d.curr;
            const bool pre_engaged = d.engaged;
            const bool pre_avoid = d.terrain_avoid_engaged;
            const int pre_mode = static_cast<int>(d.bfm.mode);
            const drone::DroneTickResult r =
                drone::tick(d, kAp, dp, &env, target);
            if (r.respawned) {
                ++out.crashes;
                {   // E10 crash census -- the state one tick BEFORE the wreck
                    const glm::dvec3 up = glm::normalize(pre_tick.position);
                    out.crash_agl_sum +=
                        glm::length(pre_tick.position) - env.ground->radius_at(up);
                    const double sp = glm::length(pre_tick.velocity);
                    out.crash_speed_sum += sp;
                    if (sp > 1e-6)
                        out.crash_gamma_sum +=
                            std::asin(std::clamp(
                                glm::dot(pre_tick.velocity / sp, up), -1.0, 1.0)) *
                            180.0 / kPi;
                    if (pre_engaged) ++out.crash_engaged;
                    if (pre_avoid) ++out.crash_avoid_armed;
                    if (pre_mode >= 0 && pre_mode < 7) ++out.crash_mode[pre_mode];
                }
                d.curr = faction_state(static_cast<int>(i));  // FIX-F2
                d.prev = d.curr;
            }

            if (d.foe == drone::kFoePlayer) {
                out.player_foe_minutes += dt / 60.0;
                out.min_range_m =
                    std::min(out.min_range_m,
                             glm::length(player.position - d.curr.position));
                if (d.wants_fire) ++out.wants_fire_ticks;
                {
                    const double rng =
                        glm::length(player.position - d.curr.position);
                    if (rng >= dp.fire_range_min &&
                        rng <= std::max(dp.fire_range_max,
                                        dp.snapshot_range_m)) {
                        ++out.in_band_ticks;
                        const glm::dvec3 nose =
                            d.curr.orientation * glm::dvec3{0.0, 0.0, -1.0};
                        const glm::dvec3 aim =
                            player.position - d.curr.position;
                        out.best_cone_cos = std::max(
                            out.best_cone_cos,
                            glm::dot(nose, glm::normalize(aim)));
                        const double spd = glm::length(d.curr.velocity);
                        if (spd < 1e-6 ||
                            glm::dot(d.curr.velocity, nose) / spd >=
                                dp.fire_align_cos)
                            ++out.coord_in_band;
                        // E7.4: the witness, on the ticks that actually ran the
                        // gate. Read-only, exactly as the witness contract says.
                        if (d.w_gate_live) {
                            ++out.gate_in_band;
                            out.sum_cone_cos += d.w_cone_cos;
                            if (d.w_cone_cos >= dp.fire_cone_cos)
                                ++out.cone_open_in_band;
                        }
                    }
                }
                const int m = static_cast<int>(d.bfm.mode);
                if (prev_mode[i] >= 0 && m != prev_mode[i])
                    ++out.bfm_transitions;
                prev_mode[i] = m;
                // ACHIEVED, never commanded: the sustained airspeed a
                // player-chasing bandit actually holds where the air is full.
                const double frac = sim::atm_frac_at(d.curr.position, &env, kAp);
                const double v = glm::length(d.curr.velocity);
                const bool full_air = frac >= dp.avoid_air_frac_full;
                for (int f = 0; f < kNumFloors; ++f) {
                    const double floor_v =
                        kFloorLoMps + kFloorStepMps * static_cast<double>(f);
                    if (full_air && v >= floor_v) {
                        ++hold[i][f];
                        if (hold[i][f] >= sustain_ticks && f > best_floor)
                            best_floor = f;
                    } else {
                        hold[i][f] = 0;
                    }
                }
            } else {
                prev_mode[i] = -1;
                for (int f = 0; f < kNumFloors; ++f) hold[i][f] = 0;
            }
        }

        // --- return fire (spawn detection by the cooldown recharge) ------
        for (std::size_t i = 0; i < fleet.size(); ++i)
            cd_before[i] = fleet[i].fire_cooldown;
        combat::enemy_fire_tick(cw, fleet, dt, kAp.g, kAp.R, nullptr);
        for (std::size_t i = 0; i < fleet.size(); ++i) {
            const drone::DroneState& d = fleet[i];
            if (d.fire_cooldown <= cd_before[i]) continue;  // did not shoot
            ++out.rounds_total;
            if (d.foe == drone::kFoePlayer) ++out.rounds_at_player;
            out.max_fire_range_m =
                std::max(out.max_fire_range_m,
                         glm::length(d.gun_tgt_pos - d.curr.position));
        }

        combat::combat_player_tick(cw, player_prev, player);
    }
    out.player_hits = cw.player_hits;
    out.best_sustained_mps =
        best_floor >= 0
            ? kFloorLoMps + kFloorStepMps * static_cast<double>(best_floor)
            : 0.0;
    return out;
}

// Sum an ensemble of engagements into one arm result.
ArmResult fly_ensemble(const drone::DroneParams& dp, double minutes) {
    ArmResult sum;
    for (int v = 0; v < kEnsemble; ++v) {
        const ArmResult r = fly_arm(dp, minutes, v);
        sum.rounds_at_player += r.rounds_at_player;
        sum.rounds_total += r.rounds_total;
        sum.max_fire_range_m = std::max(sum.max_fire_range_m, r.max_fire_range_m);
        sum.player_hits += r.player_hits;
        sum.bfm_transitions += r.bfm_transitions;
        sum.player_foe_minutes += r.player_foe_minutes;
        sum.best_sustained_mps =
            std::max(sum.best_sustained_mps, r.best_sustained_mps);
        sum.crashes += r.crashes;
        sum.min_range_m = std::min(sum.min_range_m, r.min_range_m);
        sum.wants_fire_ticks += r.wants_fire_ticks;
        sum.in_band_ticks += r.in_band_ticks;
        sum.best_cone_cos = std::max(sum.best_cone_cos, r.best_cone_cos);
        sum.coord_in_band += r.coord_in_band;
    }
    return sum;
}

}  // namespace

// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// E1.1 ACCEPTANCE (spec P0-2 fold): pin the ACHIEVED sustained speed, never the
// commanded one. A single bandit is held in a straight-line chase against a
// target far enough ahead to keep it in Intercept for the whole run, in full
// air over flat ground, and flown to steady state. The commanded ceiling is
// 265; what matters is where the autothrottle actually settles.
TEST_CASE("probe P-A prologue: the commanded chase ceiling is actually flown") {
    const world::HeightField hf = uniform_field(0.0);
    std::vector<sim::AtmosphereField::Bubble> bubbles;
    const world::FactionGrowth grow[2]{};
    world::build_faction_bubbles(kGame.atmosphere, grow, bubbles);
    sim::AtmosphereField af;
    af.deck_agl_m = kGame.atmosphere.deck_agl_m;
    af.deck_soft_m = kGame.atmosphere.deck_soft_m;
    af.bubbles = bubbles;
    sim::Environment env;
    env.ground = &hf;
    env.ground_params = test_gp();
    env.atm = &af;

    const glm::dvec3 c = glm::normalize(world::kSudburyCenterDir);

    const auto settled_speed = [&](const drone::DroneParams& dp) {
        const glm::dvec3 east =
            glm::normalize(glm::cross(glm::dvec3{0.0, 1.0, 0.0}, c));
        const glm::dvec3 pos = c * (hf.radius_at(c) + 2500.0);
        drone::DroneState d;
        d.curr = drone::level_state_at(dp, pos, east);
        d.prev = d.curr;
        d.hp = dp.hp;
        d.engaged = true;
        d.foe = drone::kFoePlayer;

        // A target dead ahead, far outside attack_range_m, moving away at the
        // player's own class so the chase never becomes a merge: this holds
        // the machine in Intercept with the chase ceiling armed.
        sim::SimState tgt;
        tgt.orientation = d.curr.orientation;
        const int ticks = static_cast<int>(90.0 / kAp.sim_dt);
        double v = 0.0;
        for (int t = 0; t < ticks; ++t) {
            // GREAT-CIRCLE correct placement at the SAME radius. A naive
            // pos + nose*6000 sits 1.2 km higher on this 15 km planet (the
            // chord-vs-arc trap), which would command a sustained climb and
            // measure a climb speed instead of a chase speed.
            const glm::dvec3 up = glm::normalize(d.curr.position);
            glm::dvec3 nose = d.curr.orientation * glm::dvec3{0.0, 0.0, -1.0};
            nose = nose - glm::dot(nose, up) * up;
            if (glm::length(nose) < 1e-6) break;
            nose = glm::normalize(nose);
            const glm::dvec3 axis = glm::normalize(glm::cross(up, nose));
            tgt.position =
                glm::angleAxis(6000.0 / kAp.R, axis) * d.curr.position;
            tgt.velocity =
                275.0 * glm::normalize(glm::cross(
                            axis, glm::normalize(tgt.position)));
            drone::tick(d, kAp, dp, &env, &tgt);
            v = glm::length(d.curr.velocity);
        }
        return v;
    };

    drone::DroneParams shipped = kScen.drone;
    shipped.maverick.enabled = false;
    drone::DroneParams no_ff = shipped;
    no_ff.throttle_ff = 0.0;
    drone::DroneParams legacy = pre_e1(shipped);
    legacy.maverick.enabled = false;

    const double v_legacy = settled_speed(legacy);
    const double v_no_ff = settled_speed(no_ff);
    const double v_shipped = settled_speed(shipped);
    INFO("settled chase speed: legacy=" << v_legacy << "  ceiling-only(no ff)="
                                        << v_no_ff << "  shipped=" << v_shipped
                                        << "  commanded ceiling="
                                        << shipped.bfm.intercept_speed_mps);
    // The G1 baseline: the pre-E1 ceiling pins the chase at the 175 m/s class.
    REQUIRE(v_legacy < 200.0);
    // The raised ceiling alone leaves the P0-2 droop: the COMMANDED 265 is not
    // the ACHIEVED speed.
    REQUIRE(v_no_ff > v_legacy);
    // The feedforward is what actually flies it, and the achieved speed clears
    // the player's measured 259 m/s mean.
    REQUIRE(v_shipped > v_no_ff);
    REQUIRE(v_shipped >= 255.0);
}

TEST_CASE("probe P-A: the ace gets shot at") {
    const drone::DroneParams shipped = kScen.drone;
    const drone::DroneParams baseline = pre_e1(shipped);

    // The fixture must show the baseline defect, and the shipped table must
    // close it, over the SAME ensemble of engagements.
    const ArmResult pre = fly_ensemble(baseline, 10.0);
    const ArmResult post = fly_ensemble(shipped, 10.0);
    const double arm_minutes = 10.0 * static_cast<double>(kEnsemble);

    INFO("PRE  rounds_at_player=" << pre.rounds_at_player
                                  << " rounds_total=" << pre.rounds_total
                                  << " hits=" << pre.player_hits
                                  << " max_fire_range=" << pre.max_fire_range_m
                                  << " bfm_transitions=" << pre.bfm_transitions
                                  << " player_foe_min=" << pre.player_foe_minutes
                                  << " sustained=" << pre.best_sustained_mps
                                  << " crashes=" << pre.crashes
                                  << " min_range=" << pre.min_range_m
                                  << " wants_fire_ticks=" << pre.wants_fire_ticks
                                  << " in_band=" << pre.in_band_ticks
                                  << " best_cone_cos=" << pre.best_cone_cos
                                  << " coord_in_band=" << pre.coord_in_band);
    INFO("POST rounds_at_player=" << post.rounds_at_player
                                  << " rounds_total=" << post.rounds_total
                                  << " hits=" << post.player_hits
                                  << " max_fire_range=" << post.max_fire_range_m
                                  << " bfm_transitions=" << post.bfm_transitions
                                  << " player_foe_min=" << post.player_foe_minutes
                                  << " sustained=" << post.best_sustained_mps
                                  << " crashes=" << post.crashes
                                  << " min_range=" << post.min_range_m
                                  << " wants_fire_ticks=" << post.wants_fire_ticks
                                  << " in_band=" << post.in_band_ticks
                                  << " best_cone_cos=" << post.best_cone_cos
                                  << " coord_in_band=" << post.coord_in_band);

    // (0) Non-vacuity: the engagement actually happened, and happened in BOTH
    // arms for a comparable share of the run. A difference in rounds is then a
    // difference in the KILL CHAIN, not in who showed up.
    REQUIRE(pre.player_foe_minutes > 0.25 * arm_minutes);
    REQUIRE(post.player_foe_minutes > 0.25 * arm_minutes);
    REQUIRE(post.player_foe_minutes <= 1.5 * pre.player_foe_minutes);

    // (1) THE HEADLINE — rounds actually aimed at the player.
    //
    // The comparison that carries weight is PRE vs POST IN THIS FIXTURE: both
    // arms fly the identical ensemble against the identical ace with the
    // identical composition, so the only variable is the rung. The spec's "10x
    // the tape baseline" bar is quoted below as an INFO because the tape is a
    // DIFFERENT fixture — six of Chad's flights with tunnel runs, raids and
    // long stretches where no enemy was assigned to him at all, versus this
    // probe's near-continuous assignment — and mixing the two would compare a
    // rate to a rate that never measured the same thing. The requirement is
    // therefore 10x the in-fixture baseline, with a floor of 10 rounds so a
    // zero-baseline arm still has to produce real volume.
    const double tape_rate = kTapeRoundsAtPlayer / kTapeMinutes;
    const double pre_rate =
        static_cast<double>(pre.rounds_at_player) / arm_minutes;
    const double post_rate =
        static_cast<double>(post.rounds_at_player) / arm_minutes;
    INFO("rounds/min at the player: tape=" << tape_rate << "  PRE=" << pre_rate
                                           << "  POST=" << post_rate
                                           << "  (over " << arm_minutes
                                           << " simulated minutes per arm)");
    // ★★ THE 10x MULTIPLIER IS WITHDRAWN, and this is an adjudicated re-pin,
    // not a silent one (docs spec "E7.I"). The old bar was
    // max(10, 10 * pre.rounds_at_player), i.e. a factor-of-ten claim resting on
    // a PRE count of 2 rare events. The noise floor measured this session says
    // that is not a claim this fixture can carry: arms whose engaged bank cap
    // differs by half a degree -- the same aeroplane by any physical argument --
    // score 4 to 39 rounds at kEnsemble 8. A 2 -> 20 bar sits inside the band
    // that identical aeroplanes already span, so the old bar was passing and
    // failing on which ensemble got lucky, not on the kill chain.
    //
    // What replaces it: an ABSOLUTE floor that only a genuine collapse can
    // breach, plus the two links of the chain that ARE resolvable here --
    // in_band time (stable to ~+/-10% across the noise floor, asserted at the
    // end of this leg) and a non-zero hit count. The rung's actual quantitative
    // ruling was taken at n=64 and lives in the ledger, where the instrument
    // resolves a real 2x effect.
    const long long bar = 5;
    REQUIRE(post.rounds_at_player >= bar);

    // (2) Hit sequences land on the ace.
    //
    // ★★ THE ABSOLUTE FLOOR ON HITS IS WITHDRAWN TOO (rung E9, a DELIBERATE
    // re-pin, recorded in the spec). `post.player_hits >= 1` was the last
    // rare-event absolute left standing at kEnsemble 8 after E7.L withdrew its
    // neighbours, and E7.I's own floor study says it cannot be carried here:
    // hits ranged 0 to 18 across arms that cannot fight differently, and the
    // n=8 ensemble is 8 phase rotations of ONE script whose rates do not match
    // the converged ones (E7.I: 4.88 rounds/eng at n=8 against 1.94 at n=64).
    //
    // IT WAS WITHDRAWN ON EVIDENCE, NOT ON CONVENIENCE. E7.4 shipping at 1.5
    // turned this clause red (0 hits over 8 engagements) while the SAME table
    // at n=64 reads hits/eng 0.531 against the off arm's 0.469 -- hits are 13%
    // UP, not down, on 4x the data (spec E9.3). A clause that goes red on a
    // change that improves the very quantity it measures is measuring the
    // ensemble, not the kill chain.
    //
    // WHAT STILL CATCHES A COLLAPSE HERE: the absolute rounds floor above, the
    // in_band clauses at the end of this leg, and the n=64 sweep in the ledger.
    INFO("hits PRE=" << pre.player_hits << " POST=" << post.player_hits
         << " (RECORDED, NOT ASSERTED -- the noise floor spans 0-18 for arms "
            "that cannot fight differently; the ruling is the n=64 table)");

    // (3) REQUIREMENT, not a hope: no round is ever spawned beyond the
    // snapshot band. This is what forbids the 24 km drone-vs-drone lobbing the
    // wider gate could otherwise re-create.
    REQUIRE(shipped.snapshot_range_m > 0.0);
    REQUIRE(post.max_fire_range_m <= shipped.snapshot_range_m);
    REQUIRE(post.max_fire_range_m <= shipped.fire_range_max);

    // (4) min_dwell 0.5 must not have turned the machine into a chatterbox:
    // BFM transitions per player-assigned drone-minute stay bounded, and the
    // faster machine does not change mode dramatically more often than the
    // 2.0 s-dwell baseline did.
    const double pre_tpm =
        pre.bfm_transitions / std::max(1e-9, pre.player_foe_minutes);
    const double post_tpm =
        post.bfm_transitions / std::max(1e-9, post.player_foe_minutes);
    INFO("BFM transitions per player-assigned drone-minute: PRE=" << pre_tpm
                                                                  << " POST="
                                                                  << post_tpm);
    REQUIRE(post_tpm <= 6.0);
    REQUIRE(post_tpm <= 4.0 * std::max(0.05, pre_tpm));

    // (5) ACHIEVED (never commanded) intercept speed.
    //
    // The spec's >= 255 m/s floor is a STRAIGHT-LINE chase property and is
    // pinned where that condition actually holds — the prologue above, which
    // measures 257 m/s settled against 175 m/s on the pre-E1 ceiling. Inside a
    // turning fight the ruled corner-speed law deliberately fades the chase
    // ceiling with align^2 and Offensive caps at the pursuit bump, so the
    // fastest speed any player-chasing bandit HOLDS for 2 s here is 250 m/s,
    // not 257. Pinning 255 on this leg would be pinning the prologue's number
    // on the wrong measurement; what this leg pins is that the in-fight machine
    // reaches the player's own class at all, and that it is this rung that put
    // it there (the baseline sits exactly on its 175 m/s ceiling).
    INFO("sustained speed in full air: PRE=" << pre.best_sustained_mps
                                             << " POST="
                                             << post.best_sustained_mps);
    REQUIRE(post.best_sustained_mps >= 245.0);
    REQUIRE(post.best_sustained_mps > pre.best_sustained_mps);

    // (6) The fight is not being paid for in wrecks, and the guns are actually
    // getting the time they were starved of.
    REQUIRE(post.crashes <= pre.crashes + kEnsemble);
    REQUIRE(post.in_band_ticks > pre.in_band_ticks);
}

// ---------------------------------------------------------------------------
// RUNG E7 — THE DECISION TABLE.
//
// P-A is this ladder's acceptance instrument and E7's own acceptance says the
// bar RISES against the E6 numbers (39 rounds on the ace / 18 hits). A rung
// with three independent behaviours in it can only be judged one behaviour at a
// time, so this leg runs the SAME ensemble across the lattice and prints the
// table. It ASSERTS only what it must (the shipped table beats the E6 arm on
// the goal metric); the attribution is the printout.
TEST_CASE("probe P-A: the RUNG E7 decision table") {
    const drone::DroneParams shipped = kScen.drone;

    // The E6 arm: every E7 dial at its off-value, nothing else touched.
    drone::DroneParams e6 = shipped;
    e6.ace_bank_cap = 0.0;
    e6.mid_bank_cap = 0.0;
    e6.slash_doctrine = false;
    e6.bfm.perch_height_m = 0.0;
    e6.bfm.defensive_range_m = 0.0;
    e6.pursue_track_pitch_gain = 0.0;  // E7.4 -- the dial rung E9 ships; without
                                       // this line the "all E7 off" arm carried
                                       // the cure and every row below it was a
                                       // differential against the wrong table.

    drone::DroneParams e71 = e6;  // E7.1 alone: the tiered bank repeal
    e71.ace_bank_cap = shipped.ace_bank_cap;
    e71.mid_bank_cap = shipped.mid_bank_cap;

    drone::DroneParams e71_72 = e71;  // + the slasher doctrine
    // ★ ARMED EXPLICITLY, never read off the shipped table -- and that was a
    // live defect found by rung E9. This row used to copy
    // `shipped.slash_doctrine`, which E8 ruled to FALSE on measurement (the
    // perch-and-dive is not flyable in this envelope, spec E8.1). From that
    // moment this arm was bit-identical to the E7.1 arm and the table printed
    // FOUR IDENTICAL ROWS under four different labels -- a table that reads as
    // a decomposition and is one arm shown four times. The row is the DOCTRINE
    // arm by definition, so it arms the doctrine by definition; the non-vacuity
    // clause at the bottom keeps it that way.
    e71_72.slash_doctrine = true;
    e71_72.bfm.perch_height_m = shipped.bfm.perch_height_m;

    drone::DroneParams e71_73 = e71;  // + the defensive break
    e71_73.bfm.defensive_range_m = shipped.bfm.defensive_range_m;

    drone::DroneParams e74 = e6;  // E7.4 alone: the tracking pitch gain
    e74.pursue_track_pitch_gain = shipped.pursue_track_pitch_gain;

    struct Arm {
        const char* name;
        drone::DroneParams dp;
    };
    constexpr int kArms = 6;
    const Arm arms[kArms] = {{"E6 (all E7 off)", e6},
                             {"E7.1 only (tiered bank)", e71},
                             {"E7.1 + E7.2 (slashers, ARMED)", e71_72},
                             {"E7.1 + E7.3 (break)", e71_73},
                             {"E7.4 only (tracking pitch gain)", e74},
                             {"SHIPPED (E7.1+3+4, doctrine off)", shipped}};
    ArmResult r[kArms];
    for (int i = 0; i < kArms; ++i) r[i] = fly_ensemble(arms[i].dp, 10.0);
    // ONE accumulated string, not five INFOs: a scoped INFO dies at the end of
    // its loop iteration and an UNSCOPED_INFO is consumed by the next
    // assertion, so both print nothing on the assertion that actually matters.
    std::string table = "\nRUNG E7 DECISION TABLE (P-A ensemble, 8 x 10 min):";
    for (int i = 0; i < 5; ++i) {
        table += "\n  ";
        table += arms[i].name;
        table += " : rounds_at_player=" + std::to_string(r[i].rounds_at_player) +
                 " hits=" + std::to_string(r[i].player_hits) +
                 " rounds_total=" + std::to_string(r[i].rounds_total) +
                 " wants_fire_ticks=" + std::to_string(r[i].wants_fire_ticks) +
                 " in_band=" + std::to_string(r[i].in_band_ticks) +
                 " bfm_transitions=" + std::to_string(r[i].bfm_transitions) +
                 " player_foe_min=" + std::to_string(r[i].player_foe_minutes) +
                 " sustained=" + std::to_string(r[i].best_sustained_mps) +
                 " crashes=" + std::to_string(r[i].crashes);
    }
    INFO(table);
    // ★★ WHAT THIS LEG MAY AND MAY NOT ASSERT (docs spec "E7.I"). The draft
    // asserted `shipped.rounds_at_player > e6.rounds_at_player` at this
    // ensemble. THAT CLAIM IS NOT ANSWERABLE HERE, and the proof is measured:
    // arms whose engaged bank cap differs by HALF A DEGREE -- which no pilot
    // can fly differently -- score 4 to 39 rounds and 0 to 18 hits at
    // kEnsemble 8. The spread across arms that CANNOT differ covers the entire
    // range this table reports, because rounds_at_player is a rare-event count
    // over a chaotic furball averaged across 8 phase rotations of ONE script.
    //
    // The same 55-vs-55.5 pair converges as the ensemble grows (rounds per
    // engagement): n=8 4.88 vs 2.50, n=16 2.00 vs 2.00, n=32 2.16 vs 2.72,
    // n=64 1.94 vs 2.28. So the metric is REPAIRABLE BY ENSEMBLE SIZE, and the
    // rung's actual ruling was taken at n=64, where the instrument resolves a
    // genuine 2x effect. Those converged numbers are the ledger's, not this
    // test's -- an n=64 sweep is ~11 minutes PER ARM and has no business in a
    // gate.
    //
    // ★ AND THE E6 BASELINE THIS LADDER HAS QUOTED IS AN OUTLIER: 4.88 rounds
    // per engagement at n=8 against a converged 1.94. The famous "39 rounds /
    // 18 hits" is the single luckiest cell in the sweep, and every E7 arm was
    // being compared against it.
    //
    // So this leg keeps the table as the ATTRIBUTION PRINTOUT it always was,
    // and asserts only the two things n=8 can carry: that the off-arm is
    // genuinely live, and that the shipped table has not COLLAPSED the kill
    // chain (a real collapse -- zero hits, or in_band falling away -- is far
    // outside the noise band and would still be caught here).
    REQUIRE(r[0].rounds_at_player > 0);
    // ★ `r[shipped].player_hits > 0` WITHDRAWN (rung E9, deliberate re-pin --
    // same finding as `probe P-A: the ace gets shot at`, clause (2), and the
    // same evidence: at n=64 the shipped table's hits/eng is 0.531 against the
    // E7.4-off 0.469, i.e. UP, while this n=8 cell reads zero). The shipped arm
    // still has to put real volume downrange, which IS resolvable here:
    REQUIRE(r[kArms - 1].rounds_at_player > 0);
    REQUIRE(r[kArms - 1].wants_fire_ticks > 0);
    // in_band is the ONE column that was stable across the noise floor
    // (9830-12132 across the same-aeroplane arms, ~+/-10%), so a half-or-worse
    // collapse in time-on-target is resolvable and is a real failure.
    REQUIRE(r[kArms - 1].in_band_ticks > r[0].in_band_ticks / 2);
    // ★ A PINNED STRUCTURAL FINDING, not a performance claim: the E7.1+E7.3 arm
    // is BIT-IDENTICAL to the E7.1 arm on every column, because this fixture's
    // scripted player never fires -- combat::combat_tick never runs, no drone is
    // ever damaged, and E7.3's `under_fire` predicate (which requires ROUNDS TO
    // HAVE LANDED) can never arm. E7.3 IS INERT IN P-A BY CONSTRUCTION, and its
    // acceptance lives on the `under_the_gun` probe in test_enemy_ai_e7.cpp
    // instead. If this ever stops holding, someone has given the probe's player
    // guns and every E7.3 number in the ledger needs re-taking.
    // ★ NON-VACUITY FOR THE DOCTRINE ROW (rung E9). Its twin above was silently
    // true for a whole rung: once E8 ruled slash_doctrine false, this row copied
    // the off value and became the E7.1 arm wearing a slasher's label. A row
    // that cannot differ from its own baseline is not an arm.
    REQUIRE((r[2].rounds_at_player != r[1].rounds_at_player ||
             r[2].wants_fire_ticks != r[1].wants_fire_ticks ||
             r[2].in_band_ticks != r[1].in_band_ticks));
    REQUIRE(r[3].rounds_at_player == r[1].rounds_at_player);
    REQUIRE(r[3].player_hits == r[1].player_hits);
    REQUIRE(r[3].wants_fire_ticks == r[1].wants_fire_ticks);
    REQUIRE(r[3].in_band_ticks == r[1].in_band_ticks);
}


// ===========================================================================
// RUNG E8.2 — THE ENGAGED FIGHT-SPEED SWEEP.
//
// HIDDEN ([.] tag): an ensemble this size is minutes per arm and has no
// business in the gate. Run it by name when the dial is retuned.
//
// WHY IT EXISTS. Chad's 49-minute tape (conquest_tape_100, 2026-08-21) reads:
// enemies holding him as foe were inside 900 m for 200 s, at a median 124 m/s
// against his 202, nose a median 149 deg off. His ask -- "give enemies more
// energy fighting abilities as that is how I am able to beat them" -- is the
// same finding in his words. bfm_fight_speed_mps is the dial; THIS is what
// picks its value.
//
// ★ THE LAW THIS OBEYS (spec E7.I): rounds_at_player is a rare-event count
// over a chaotic furball and at n=8 arms that CANNOT differ scored 4 to 39.
// So: a large ensemble, and every number reported as a RATE PER ENGAGEMENT,
// never a raw sum. n=64 resolves a genuine 2x effect and CANNOT resolve 20% --
// no acceptance here may be written as "better by X%".
// ===========================================================================
TEST_CASE("probe P-A sweep: the engaged fight speed", "[.e8sweep]") {
    const drone::DroneParams shipped = kScen.drone;
    // n=64: the ensemble at which spec E7.I's convergence study says the
    // instrument resolves a genuine 2x effect. The first pass of this sweep
    // was run at n=32 and is recorded in the ledger as SUGGESTIVE ONLY --
    // rounds/eng 1.88 (off) -> 2.41 (120) -> 2.66 (150) -> 1.81 (180) -> 1.66
    // (210), an interior optimum with the right SHAPE for a corner-speed
    // trade, but a 1.4x on the rounds column is inside the noise the law
    // forbids ruling on. Only the hits column reached 2x, and hits is the
    // rarest count of all. Hence this run.
    constexpr int kN = 64;  // engagements per arm

    std::string table =
        "\nE8.2 FIGHT-SPEED SWEEP (P-A, " + std::to_string(kN) +
        " x 10 min per arm; RATES PER ENGAGEMENT)\n"
        "  patrol cruise = " + std::to_string(shipped.speed) + " m/s\n";
    for (double fs : {0.0, 120.0, 150.0, 180.0, 210.0}) {
        drone::DroneParams dp = shipped;
        dp.bfm.fight_speed_mps = fs;
        ArmResult sum;
        for (int v = 0; v < kN; ++v) {
            const ArmResult r = fly_arm(dp, 10.0, v, kN);
            sum.rounds_at_player += r.rounds_at_player;
            sum.player_hits += r.player_hits;
            sum.in_band_ticks += r.in_band_ticks;
            sum.wants_fire_ticks += r.wants_fire_ticks;
            sum.player_foe_minutes += r.player_foe_minutes;
            sum.crashes += r.crashes;
            sum.bfm_transitions += r.bfm_transitions;
            sum.best_cone_cos = std::max(sum.best_cone_cos, r.best_cone_cos);
        }
        const double n = static_cast<double>(kN);
        table += "\n  fight_speed=" + std::to_string(fs) +
                 (fs <= 0.0 ? " (OFF, = patrol cruise)" : "") +
                 "\n    rounds/eng=" +
                 std::to_string(sum.rounds_at_player / n) +
                 "  hits/eng=" + std::to_string(sum.player_hits / n) +
                 "  in_band/eng=" + std::to_string(sum.in_band_ticks / n) +
                 "  wants_fire/eng=" + std::to_string(sum.wants_fire_ticks / n) +
                 "\n    foe_min/eng=" +
                 std::to_string(sum.player_foe_minutes / n) +
                 "  bfm_trans/eng=" + std::to_string(sum.bfm_transitions / n) +
                 "  crashes/eng=" + std::to_string(sum.crashes / n) +
                 "  best_cone_cos=" + std::to_string(sum.best_cone_cos);
    }
    WARN(table);
    SUCCEED("sweep printout");
}

// ===========================================================================
// RUNG E7.4 — THE TRACKING PITCH GAIN, ON THE FURBALL.
//
// HIDDEN ([.]): n=64 x 10 min x 6 arms is over an hour and has no business in
// the gate. Run it by name when the dial is retuned.
//
// ★ WHAT THIS SWEEP IS FOR, AND WHAT IT IS NOT FOR. The VALUE is picked by the
// DETERMINISTIC PLANT measurement (test_enemy_ai_e7.cpp, "E7.4 plant sweep"):
// one aeroplane, one saturated command, no chaos, and a 14-deg limit cycle
// collapsing to 0.2 is a 60x effect that no ensemble is needed to see. This
// leg asks the furball only the question the furball can answer at its own
// resolution: DOES IT COST ANYTHING RESOLVABLE (spec E7.I: n=64 resolves a
// genuine 2x and cannot resolve 20%).
//
// ★ AND IT CARRIES ITS OWN NOISE FLOOR. `track_gain = 2.1999` against the
// global 2.2 is a 0.005% change in a pitch gain: no aeroplane can fly that
// differently, so the spread between THAT arm and the OFF arm is this table's
// floor, measured in the same run rather than inherited from the E7.1 sweep.
// It is measured for EVERY column, including in_band -- spec E8.4 leaned on
// in_band and foe_min as "the stable columns" and their floor had never
// actually been taken.
// ===========================================================================
// ===========================================================================
// RUNG E10 -- THE BANK AUTHORITY x THE TRACKING DEADBAND.
//
// Chad, 2026-08-23 after tape 5: "make them good." The plant says the wall is
// the ENGAGED BANK CAP and, above it, the DEADBAND that hands the cap back
// exactly where a gun solution lives (spec E10.1, 30 deg bearing, 144 m/s):
//     cap 83 deadband OFF : 27.0 deg/s at n 7.2, achieved bank 84.3
//     cap 83 deadband ON  :  2.8 deg/s at n 1.2, achieved bank 55.6
// a 9.5x that P-A never resolved because P-A cannot resolve anything (E9.7).
//
// ★ SO THIS LEG DOES NOT ASK P-A FOR THE PERFORMANCE CLAIM. It asks it the two
// questions it CAN answer:
//   (1) CRASHES -- the only reason E7.K kept the deadband ("the flat cap flies
//       aces into the ground", 12 vs 7). That is a terrain statistic, not a
//       statistic about the player's evasion, so this fixture is fit for it.
//   (2) THE DEADBAND'S STATED MECHANISM. Its rationale at the dial is that a
//       high-cap tracker "yanks its nose straight through the solution". That
//       is a POINTING claim, and the E8.1 witness measures pointing directly:
//       cone_open_in_band is a DENSE per-tick column, not a rare-event count.
//       If the mechanism is real, cone_open must FALL with the flat cap.
// Hidden ([.]): n=64 x 4 arms is ~45 minutes.
// ===========================================================================
TEST_CASE("probe P-A sweep: bank authority and the deadband", "[.e10sweep]") {
    const drone::DroneParams shipped = kScen.drone;
    constexpr int kN = 16;  // a MECHANISM, not a rate
    struct Arm { const char* name; drone::DroneParams dp; };
    // ★ THE SHIPPED TABLE IS NOW cap 78/76 FLAT. 83 was measured and REJECTED
    // (the plant mushes it, spec §E10.4) and 80 sits on an instability
    // boundary. These arms decompose what actually ships against what it
    // replaced, and add the crash buy-back arms if the cost is real.
    // ★ THE CRASH CENSUS. Two guesses at the mechanism (the pull-up trigger,
    // then an altitude fade on the cap) were BOTH WRONG -- the fade changed
    // nothing at all (0.266 with it, 0.266 without). And the cost is real: the
    // null floor at cap 78 spans 0.219-0.266 across arms whose bank cap differs
    // by hundredths of a degree, against 0.063 pre-E10. So this records the
    // state one tick BEFORE every wreck instead of proposing a third mechanism.
    // ★ THE DIVE-RECOVERY ARMS. The census says every wreck is an UNENGAGED
    // drone at ~250 m/s and -58 deg with the pull-up ALREADY ARMED -- so the
    // pull-up fires and cannot save it. From -58 deg at 250 m/s the sink is
    // 212 m/s and rotating the flight path 84 deg to the +26 deg climb command
    // at the G available takes ~6 s, i.e. ~1400 m of altitude. avoid_lookahead_s
    // is 3.0 s = ~640 m of warning. This sweeps the warning, nothing else.
    // ★★★ THE FORCED PULL-UP IS TOO POLITE (rung E11, Chad's tape 6: 21 AI
    // terrain crashes in 14 minutes). The census on HIS tape: PATROL 10,
    // TRANSIT 7, RUN 4 (all four in the tunnel net); the PATROL/TRANSIT ones
    // die at a median -42 to -46 deg with four at -67 deg. From -67 deg at
    // 185 m/s the sink is 170 m/s and rotating 93 deg to the +26 deg climb
    // command at the G the pull-up actually uses takes ~5 s = ~900 m; it is
    // given ~760 m of warning. But this airframe has n_max = 32: at max G it
    // rotates 93 deg in about a SECOND, needing ~170 m. The pull-up is not out
    // of runway, it is under-commanding. This sweeps the climb command it uses;
    // aoa_protect is already ON in that branch, so the AoA limiter -- not the
    // dial -- is what bounds the pull.
    // ★ THE AIR-SEEK ALTITUDE FADE. Everything downstream of the dive was
    // swept and none of it moved the crash count (lookahead 3->9 s: 3/3/4/3;
    // climb command 26->70 deg: 3/4/6/7 -- STEEPER WAS WORSE). This sweeps the
    // dive itself.
    drone::DroneParams a = shipped;  // 0 = off = the shipped behaviour
    drone::DroneParams b = shipped; b.avoid_air_dive_agl_m = 1000.0;
    drone::DroneParams c = shipped; c.avoid_air_dive_agl_m = 1500.0;
    drone::DroneParams d = shipped; d.avoid_air_dive_agl_m = 2500.0;
    const Arm arms[4] = {{"air-seek fade OFF (shipped)", a},
                         {"air-seek faded below 1000 m AGL", b},
                         {"air-seek faded below 1500 m AGL", c},
                         {"air-seek faded below 2500 m AGL", d}};
    std::string table =
        "\nE10 BANK-AUTHORITY SWEEP (P-A, " + std::to_string(kN) +
        " x 10 min per arm; RATES PER ENGAGEMENT)\n"
        "  ASK ONLY: crashes (a terrain statistic) and cone_open (a DENSE\n"
        "  pointing column). Rounds/hits are inside the null floor here.\n";
    for (const Arm& arm : arms) {
        ArmResult sum;
        for (int v = 0; v < kN; ++v) {
            const ArmResult r = fly_arm(arm.dp, 10.0, v, kN);
            sum.rounds_at_player += r.rounds_at_player;
            sum.player_hits += r.player_hits;
            sum.in_band_ticks += r.in_band_ticks;
            sum.wants_fire_ticks += r.wants_fire_ticks;
            sum.crashes += r.crashes;
            sum.gate_in_band += r.gate_in_band;
            sum.cone_open_in_band += r.cone_open_in_band;
            sum.sum_cone_cos += r.sum_cone_cos;
            sum.crash_engaged += r.crash_engaged;
            sum.crash_avoid_armed += r.crash_avoid_armed;
            sum.crash_agl_sum += r.crash_agl_sum;
            sum.crash_gamma_sum += r.crash_gamma_sum;
            sum.crash_speed_sum += r.crash_speed_sum;
            for (int m = 0; m < 7; ++m) sum.crash_mode[m] += r.crash_mode[m];
        }
        const double n = static_cast<double>(kN);
        const double mean_cone_deg =
            sum.gate_in_band > 0
                ? std::acos(std::clamp(sum.sum_cone_cos /
                                static_cast<double>(sum.gate_in_band), -1.0, 1.0)) *
                      180.0 / kPi
                : -1.0;
        const double nc =
            static_cast<double>(std::max<long long>(sum.crashes, 1));
        std::string modes;
        {
            const char* mn[7] = {"Intercept", "Offensive", "Defensive",
                                 "Extend", "Perch", "Slash", "Yoyo"};
            for (int m = 0; m < 7; ++m)
                if (sum.crash_mode[m] > 0)
                    modes += std::string(" ") + mn[m] + "=" +
                             std::to_string(sum.crash_mode[m]);
        }
        table += std::string("\n  ") + arm.name +
                 "\n    CRASH CENSUS: n=" + std::to_string(sum.crashes) +
                 "  engaged=" + std::to_string(sum.crash_engaged) +
                 "  pull-up ALREADY armed=" +
                 std::to_string(sum.crash_avoid_armed) +
                 "\n      mean AGL one tick before=" +
                 std::to_string(sum.crash_agl_sum / nc) +
                 "  mean gamma=" + std::to_string(sum.crash_gamma_sum / nc) +
                 " deg  mean V=" + std::to_string(sum.crash_speed_sum / nc) +
                 "\n      bfm mode at the wreck:" + modes +
                 "\n    CRASHES/eng=" + std::to_string(sum.crashes / n) +
                 "  *cone_open/eng=" +
                 std::to_string(static_cast<double>(sum.cone_open_in_band) / n) +
                 "  mean_cone_deg=" + std::to_string(mean_cone_deg) +
                 "\n    rounds/eng=" + std::to_string(sum.rounds_at_player / n) +
                 "  hits/eng=" + std::to_string(sum.player_hits / n) +
                 "  wants_fire/eng=" + std::to_string(sum.wants_fire_ticks / n) +
                 "  in_band/eng=" + std::to_string(sum.in_band_ticks / n);
    }
    WARN(table);
    SUCCEED("E10 sweep printout");
}

TEST_CASE("probe P-A sweep: the tracking pitch gain", "[.e74sweep]") {
    const drone::DroneParams shipped = kScen.drone;
    constexpr int kN = 64;

    std::string table =
        "\nE7.4 TRACKING-PITCH-GAIN SWEEP (P-A, " + std::to_string(kN) +
        " x 10 min per arm; RATES PER ENGAGEMENT)\n"
        "  the global pursue_pitch_gain (= the OFF value) = " +
        std::to_string(shipped.pursue_pitch_gain) +
        "\n  the shipped table underneath: fight_speed=" +
        std::to_string(shipped.bfm.fight_speed_mps) + " slash_doctrine=" +
        (shipped.slash_doctrine ? "true" : "false") + "\n";
    for (double g : {0.0, 2.1999, 1.7, 1.5, 1.4, 1.0}) {
        drone::DroneParams dp = shipped;
        dp.pursue_track_pitch_gain = g;
        ArmResult sum;
        for (int v = 0; v < kN; ++v) {
            const ArmResult r = fly_arm(dp, 10.0, v, kN);
            sum.rounds_at_player += r.rounds_at_player;
            sum.player_hits += r.player_hits;
            sum.in_band_ticks += r.in_band_ticks;
            sum.wants_fire_ticks += r.wants_fire_ticks;
            sum.player_foe_minutes += r.player_foe_minutes;
            sum.crashes += r.crashes;
            sum.bfm_transitions += r.bfm_transitions;
            sum.gate_in_band += r.gate_in_band;
            sum.cone_open_in_band += r.cone_open_in_band;
            sum.sum_cone_cos += r.sum_cone_cos;
            sum.best_cone_cos = std::max(sum.best_cone_cos, r.best_cone_cos);
        }
        const double n = static_cast<double>(kN);
        const double mean_cone_deg =
            sum.gate_in_band > 0
                ? std::acos(std::clamp(sum.sum_cone_cos /
                                           static_cast<double>(sum.gate_in_band),
                                       -1.0, 1.0)) *
                      180.0 / kPi
                : -1.0;
        table += "\n  track_gain=" + std::to_string(g) +
                 (g <= 0.0 ? " (OFF, = the global gain)" : "") +
                 (g > 2.19 && g < 2.2 ? " (THE NOISE FLOOR ARM)" : "") +
                 "\n    rounds/eng=" + std::to_string(sum.rounds_at_player / n) +
                 "  hits/eng=" + std::to_string(sum.player_hits / n) +
                 "  wants_fire/eng=" + std::to_string(sum.wants_fire_ticks / n) +
                 "\n    in_band/eng=" + std::to_string(sum.in_band_ticks / n) +
                 "  foe_min/eng=" + std::to_string(sum.player_foe_minutes / n) +
                 "  crashes/eng=" + std::to_string(sum.crashes / n) +
                 "\n    POINTING: cone_open/eng=" +
                 std::to_string(static_cast<double>(sum.cone_open_in_band) / n) +
                 "  cone_open_frac=" +
                 std::to_string(sum.gate_in_band > 0
                                    ? static_cast<double>(sum.cone_open_in_band) /
                                          static_cast<double>(sum.gate_in_band)
                                    : 0.0) +
                 "  mean_cone_deg=" + std::to_string(mean_cone_deg) +
                 "  gate_in_band/eng=" +
                 std::to_string(static_cast<double>(sum.gate_in_band) / n);
    }
    WARN(table);
    SUCCEED("sweep printout");
}
