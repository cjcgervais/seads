// RUNG E7 — THE KILLERS (docs/ENEMY_AI_E1_E2_SPEC.md "RUNG E7").
//
// CHAD'S RULING (2026-08-20 evening): "I want killers to contend with. That is
// a rule." + BOTH doctrines approved. The named repeal: the 55-deg engaged bank
// cap (the R4/R5 corner-speed ruling) is repealed FOR ENGAGED FIGHTERS ONLY,
// tiered by trait aggression. Walk-back = ace_bank_cap_deg 55 (or 0).
//
// THE WALL this rung exists to break, measured across every probe on this
// ladder: 55 deg of bank at fight speed is ~7 deg/s of turn, against 25-40
// deg/s of LOS rate from a 275 m/s player. No gun solution exists at that ratio
// unless he flies straight.
//
// House discipline applied to every dial here:
//   * every OFF arm is proven bit-identical with `==`, never a tolerance band,
//     and is PAIRED with an ON arm that must DIFFER (a dial that silently did
//     nothing could otherwise pass as "bit-identical");
//   * G HONESTY: the raised bank is measured as ACHIEVED bank / load factor /
//     turn rate out of the real plant (sim::step), never as the commanded
//     number. "Command-and-mush" is a specific failure this rung must exclude,
//     and the soft AoA limiter is exactly the thing that would produce it;
//   * the doctrine split is PROBED, not narrated — the mode histogram and the
//     guns-hot ticks come out of a flown engagement.
//
// TEST_CASE and SECTION names are STRICTLY ASCII (a non-ASCII name silently
// never runs — the recurring trap).

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
#include "control/controller.h"
#include "drone/bfm.h"
#include "drone/drone.h"
#include "drone/maverick.h"
#include "sim/aero.h"
#include "sim/environment.h"
#include "sim/state.h"
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

// ---------------------------------------------------------------------------
// THE OFF TABLE. Every RUNG E7 dial at its off-value, and nothing else touched:
// this is rung E6 exactly, and it is the arm every differential is measured
// against.
drone::DroneParams pre_e7(const drone::DroneParams& shipped) {
    drone::DroneParams dp = shipped;
    dp.ace_bank_cap = 0.0;              // E7.1 structural off (tier == 55)
    dp.mid_bank_cap = 0.0;
    dp.slash_doctrine = false;          // E7.2 doctrine selector off
    dp.bfm.perch_height_m = 0.0;        // E7.2 structural off
    dp.bfm.defensive_range_m = 0.0;     // E7.3 structural off
    // ★ E7.4's dial belongs here too, and its ABSENCE was a live defect: this
    // helper is "rung E6 exactly", and the moment pursue_track_pitch_gain
    // shipped non-zero the OFF arm of every differential in this file was
    // quietly carrying the cure. It surfaced as "E7.1 G honesty" reporting a
    // 4.8-deg swing where THE WALL it exists to pin is 14.2 (rung E9).
    dp.pursue_track_pitch_gain = 0.0;   // E7.4 structural off
    return dp;
}

world::HeightField flat_field(double elev_m, double relief = 6000.0) {
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

// The SAME world the P-A probe flies: flat ground plus the real two-faction
// atmosphere, so the flyable-air ceiling and terrain avoidance both bite.
struct World {
    world::HeightField hf;
    sim::AtmosphereField af;
    sim::Environment env;
};

void build_world(World& w) {
    w.hf = flat_field(0.0);
    std::vector<sim::AtmosphereField::Bubble> bubbles;
    const world::FactionGrowth grow[2]{};
    world::build_faction_bubbles(kGame.atmosphere, grow, bubbles);
    w.af.deck_agl_m = kGame.atmosphere.deck_agl_m;
    w.af.deck_soft_m = kGame.atmosphere.deck_soft_m;
    w.af.bubbles = bubbles;
    w.env.ground = &w.hf;
    w.env.ground_params = test_gp();
    w.env.atm = &w.af;
}

// The ACHIEVED bank angle, out of the ONE shared extraction the autopilot's own
// roll loop closes on (control::extract) — never a re-derived frame.
double achieved_bank(const sim::SimState& s) {
    return control::extract(s, s.last_vhat, kAp.v_dir_eps).phi;
}

// The ACHIEVED load factor. n = |specific force| / g, where the specific force
// is the total acceleration MINUS gravity: a - (-g*up) = a + g*up. This is what
// a G-meter in the cockpit reads, and it is the honest answer to "did the
// aeroplane actually pull the turn it was told to".
double achieved_n(const sim::SimState& prev, const sim::SimState& curr,
                  double dt) {
    const glm::dvec3 a = (curr.velocity - prev.velocity) / dt;
    const glm::dvec3 up = sim::local_up(curr.position);
    return glm::length(a + kAp.g * up) / kAp.g;
}

// ---------------------------------------------------------------------------
// PROBE 1 — THE SATURATED TURN (the E7.1 G-honesty instrument).
//
// A single engaged bandit is given a target pinned at a FIXED bearing off its
// own nose, recomputed every tick from its own attitude, at a fixed range and
// its own altitude. That saturates pursue()'s bank command (k_az * bearing is
// 3.0 * 0.7 rad at 40 deg, far past any cap) so the COMMAND is exactly the
// tier's cap on every single tick, and whatever the aeroplane then does is
// entirely the plant's answer. BFM is disabled for this leg on purpose: the
// question here is the airframe's, not the state machine's.
//
// The bearing sweep matters because the RULED corner-speed law is still live:
// pursue_speed_bump is faded by align^2, so a 90-deg-off bandit fights at bare
// cruise (85 m/s) and a 30-deg-off one at ~152. Whether the raised cap is flown
// or mushed is therefore a function of bearing, and reporting one number for it
// would be a lie.
// ★ turn_dps IS A NET RATE, and that distinction was MEASURED, not assumed. The
// first draft of this probe averaged the per-tick |heading change|, and it
// reported the 55-deg arm turning 17 deg/s — three times the geometric maximum
// for its own bank and speed. The cause is real and is itself a finding: a
// saturated 55-deg turn PORPOISES (gamma oscillating +/-7 deg, instantaneous n
// swinging 1.75-6.6), and an unsigned per-tick metric counts that vertical
// thrashing as turning. The net signed heading change over the steady window
// counts only the turn that actually happened.
struct TurnResult {
    double cmd_bank_deg = 0.0;   // the FLAT tier cap (engaged_bank_cap)
    // ★ THE CAP THE PILOT WAS ACTUALLY HANDED at this bearing, peak over the
    // steady window. It is NOT cmd_bank_deg whenever the E7.1 tracking deadband
    // is armed: inside ace_bank_track_lo the ramp fades the tier cap all the
    // way back to the ruled 55, ON PURPOSE (the P-A measurement at the dials).
    // The plant can only be asked to fly the command the machine gave it, so
    // this — not the flat tier number — is what "no mush in roll" is measured
    // against. With the deadband off (hi <= lo) the ramp is a constant 1 and
    // this equals cmd_bank_deg exactly, so the FLAT arm is unchanged.
    double cap_handed_deg = 0.0;
    double bank_deg = 0.0;       // ACHIEVED, mean over the steady half
    double cmd_bank_flown_deg = 0.0;  // the post-slew command (E1.4 seam)
    double n = 0.0;              // ACHIEVED load factor, mean over steady half
    double turn_dps = 0.0;       // ACHIEVED NET heading rate over the window
    double gamma_swing_deg = 0.0;  // ACHIEVED peak-to-peak flight-path angle
    double speed = 0.0;          // ACHIEVED airspeed, mean over steady half
    int crashes = 0;             // a respawn would forge every statistic
    double min_alt_m = 1e18;
    double max_alt_m = -1e18;
};

TurnResult saturated_turn(const World& w, const drone::DroneParams& dp_in,
                          int spawn_index, double bearing_rad) {
    drone::DroneParams dp = dp_in;
    dp.maverick.enabled = false;
    dp.bfm.enabled = false;  // the PLANT is on trial, not the machine

    const glm::dvec3 c = glm::normalize(world::kSudburyCenterDir);
    const glm::dvec3 east =
        glm::normalize(glm::cross(glm::dvec3{0.0, 1.0, 0.0}, c));
    drone::DroneState d = drone::spawn_drone(kAp, dp, spawn_index,
                                             combat::kNumMavericks);
    d.curr = drone::level_state_at(
        dp, c * (w.hf.radius_at(c) + 2500.0), east);
    d.prev = d.curr;
    d.hp = dp.hp;
    d.engaged = true;
    d.foe = drone::kFoePlayer;

    const double dt = kAp.sim_dt;
    const int ticks = static_cast<int>(30.0 / dt);
    const int steady_from = ticks / 2;  // the transient is not the answer

    TurnResult r;
    r.cmd_bank_deg = drone::engaged_bank_cap(dp, spawn_index) * 180.0 / kPi;
    double sum_bank = 0.0, sum_n = 0.0, sum_turn = 0.0, sum_v = 0.0;
    int n_samples = 0;
    double gam_lo = 1e18, gam_hi = -1e18;
    glm::dvec3 prev_hdg{0.0};
    for (int t = 0; t < ticks; ++t) {
        // The target, pinned at `bearing_rad` off the nose in the bandit's own
        // local horizontal, 900 m away at its own altitude. Held STATIONARY
        // (zero velocity) so the deflection lead collapses to boresight and the
        // measurement is not confounded by a moving aim point.
        const glm::dvec3 up = sim::local_up(d.curr.position);
        glm::dvec3 nose = d.curr.orientation * glm::dvec3{0.0, 0.0, -1.0};
        nose = nose - glm::dot(nose, up) * up;
        if (glm::length(nose) < 1e-6) break;
        nose = glm::normalize(nose);
        const glm::dvec3 dir =
            glm::normalize(glm::angleAxis(-bearing_rad, up) * nose);
        sim::SimState tgt;
        tgt.position = d.curr.position + dir * 900.0;
        tgt.velocity = glm::dvec3{0.0};
        tgt.orientation = d.curr.orientation;
        tgt.last_vhat = dir;

        const sim::SimState before = d.curr;
        const drone::DroneTickResult rr = drone::tick(d, kAp, dp, &w.env, &tgt);
        if (rr.respawned) ++r.crashes;
        r.min_alt_m = std::min(r.min_alt_m, sim::altitude(d.curr.position, kAp));
        r.max_alt_m = std::max(r.max_alt_m, sim::altitude(d.curr.position, kAp));

        if (t >= steady_from) {
            sum_bank += std::abs(achieved_bank(d.curr)) * 180.0 / kPi;
            sum_n += achieved_n(before, d.curr, dt);
            sum_v += glm::length(d.curr.velocity);
            r.cmd_bank_flown_deg =
                std::max(r.cmd_bank_flown_deg,
                         std::abs(d.bank_slew_cmd) * 180.0 / kPi);
            // The ramped cap this pilot was handed THIS tick, off the same
            // geometry drone::tick derives it from: the 3-D angle between the
            // nose and the line of sight, clamped to [0,1] before the acos
            // exactly as the corner-speed law does it. Mirrored rather than
            // plumbed out so the probe stays a black-box measurement of tick().
            {
                const glm::dvec3 nose3 =
                    d.curr.orientation * glm::dvec3{0.0, 0.0, -1.0};
                const glm::dvec3 rel3 = tgt.position - d.curr.position;
                if (glm::length(rel3) > 1e-6) {
                    const double a = std::clamp(
                        glm::dot(nose3, glm::normalize(rel3)), 0.0, 1.0);
                    r.cap_handed_deg = std::max(
                        r.cap_handed_deg,
                        drone::engaged_bank_cap_at(dp, spawn_index,
                                                   std::acos(a)) *
                            180.0 / kPi);
                }
            }
            const glm::dvec3 up2 = sim::local_up(d.curr.position);
            const double sp = glm::length(d.curr.velocity);
            if (sp > 1e-6) {
                const double gam = std::asin(std::clamp(
                    glm::dot(d.curr.velocity / sp, up2), -1.0, 1.0));
                gam_lo = std::min(gam_lo, gam);
                gam_hi = std::max(gam_hi, gam);
            }
            glm::dvec3 h = d.curr.velocity - glm::dot(d.curr.velocity, up2) * up2;
            if (glm::length(h) > 1e-6) {
                h = glm::normalize(h);
                if (glm::length(prev_hdg) > 1e-6) {
                    // SIGNED about local up, so an oscillation cancels and only
                    // the net turn survives (see the TurnResult comment).
                    sum_turn += std::atan2(
                        glm::dot(glm::cross(prev_hdg, h), up2),
                        std::clamp(glm::dot(prev_hdg, h), -1.0, 1.0));
                }
                prev_hdg = h;
            }
            ++n_samples;
        }
    }
    if (n_samples > 0) {
        r.bank_deg = sum_bank / n_samples;
        r.n = sum_n / n_samples;
        r.turn_dps = std::abs(sum_turn) /
                     (static_cast<double>(n_samples) * dt) * 180.0 / kPi;
        r.gamma_swing_deg =
            gam_hi > gam_lo ? (gam_hi - gam_lo) * 180.0 / kPi : 0.0;
        r.speed = sum_v / n_samples;
    }
    return r;
}

// ---------------------------------------------------------------------------
// PROBE 2 — THE ACE COURSE (the shared engagement fixture for E7.2 / P-H).
//
// One bandit of a chosen spawn index against the SAME stepped 275 m/s ace
// course probe P-A flies (a slow mean circuit with a hard turn alternating on
// and off every 20 s plus a slow altitude weave), through the REAL app order:
// drone::tick -> combat::enemy_fire_tick -> combat::combat_player_tick.
//
// A STEPPED player, never a stamped parked one: the fixture-phantom lesson
// (docs/killchain_handoff.md) cost this ladder two aiming-law rewrites.
//
// ★ THE SPAWN INDEX MUST BE AN ENEMY OF THE PLAYER. combat::maverick_faction
// puts indices 0-4 in SUDBURY and 5-9 in VALLEY, and this fixture flies the
// P-A convention (the player raids Sudbury, so he is VALLEY and 0-4 are his
// enemies). Handing this probe an ALLY produces a perfectly green-looking run
// in which assign_foes never engages anyone and every mode counter reads
// Intercept forever -- measured, and the reason DuelResult carries foe_ticks.
// The enemy tiers are therefore: SHAFT (1, .85) and GULCH (0, .75) = ACE,
// NICKEL (3, .70) = MID, CANARY (2, .55) and SLAGHEAP (4, .60) = BASE.
struct DuelResult {
    long long mode_ticks[7] = {0, 0, 0, 0, 0, 0, 0};
    long long foe_ticks = 0;       // ticks actually assigned to the player
    long long fire_ticks = 0;      // wants_fire
    long long rounds = 0;          // rounds actually spawned
    long long hits = 0;            // damaging rounds on the player
    double best_track_s = 0.0;     // longest CONTINUOUS FIRING solution
                                   //   (cone + coordinated + inside the band)
    double best_cone_s = 0.0;      // longest CONTINUOUS TRACKING solution
                                   //   (cone + coordinated, any range) --
                                   //   "he is holding his nose on me"
    double engaged_min_range_m = 1e18;
    double min_range_m = 1e18;
    double max_bank_deg = 0.0;     // ACHIEVED peak
    double max_n = 0.0;            // ACHIEVED peak
    int crashes = 0;
};

DuelResult ace_course(const World& w, const drone::DroneParams& dp_in,
                      int spawn_index, double minutes, int variant,
                      double start_range_m = 1500.0) {
    drone::DroneParams dp = dp_in;
    dp.maverick.enabled = false;  // the SURFACE kill chain, no run scheduling

    const double dt = kAp.sim_dt;
    const long long ticks = static_cast<long long>(minutes * 60.0 / dt);
    const double var = static_cast<double>(variant);

    constexpr double kPlayerSpeed = 275.0;
    constexpr double kTurnMeanRate = 0.034;
    constexpr double kTurnHardRate = 0.050;
    constexpr double kTurnBlockS = 20.0;
    constexpr double kAltMeanM = 2500.0;
    constexpr double kAltSwingM = 300.0;
    constexpr double kAltPeriodS = 40.0;

    glm::dvec3 p_up = glm::normalize(world::kSudburyCenterDir);
    glm::dvec3 p_hdg =
        glm::normalize(glm::cross(p_up, glm::dvec3{0.0, 1.0, 0.2}));
    p_hdg = glm::normalize(glm::angleAxis(2.0 * kPi * var / 8.0, p_up) * p_hdg);
    sim::SimState player;
    {
        player.position = p_up * (w.hf.radius_at(p_up) + kAltMeanM);
        player.velocity = kPlayerSpeed * p_hdg;
        player.last_vhat = p_hdg;
        const glm::dvec3 right = glm::normalize(glm::cross(p_hdg, p_up));
        player.orientation =
            glm::normalize(glm::quat_cast(glm::dmat3{right, p_up, -p_hdg}));
    }

    // The bandit starts on the ace's six at start_range_m — the geometry the
    // duel is about. Its own faction placement is irrelevant here (there is one
    // drone and no leash order), so it is placed honestly relative to the fight.
    drone::DroneState d = drone::spawn_drone(kAp, dp, spawn_index,
                                             combat::kNumMavericks);
    const glm::dvec3 d_dir = glm::normalize(
        glm::angleAxis(start_range_m / kAp.R,
                       glm::normalize(glm::cross(p_up, p_hdg))) *
        p_up);
    d.curr = drone::level_state_at(
        dp, d_dir * (w.hf.radius_at(d_dir) + kAltMeanM), p_hdg);
    d.prev = d.curr;
    d.hp = dp.hp;

    combat::CombatWorld cw;
    cw.setup = kScen.combat;
    cw.damage_params = kScen.damage;
    cw.params.hit_radius_m = dp.hit_radius_m;
    cw.player_hp = combat::summary_hp(cw.damage);

    std::vector<drone::DroneState> fleet{d};
    DuelResult out;
    long long track_run = 0;
    long long cone_run = 0;

    for (long long t = 0; t < ticks; ++t) {
        const sim::SimState player_prev = player;
        p_up = glm::normalize(player.position);
        p_hdg = glm::normalize(p_hdg - glm::dot(p_hdg, p_up) * p_up);
        const double time_s = static_cast<double>(t) * dt;
        const long long block = static_cast<long long>(
            std::floor((time_s + 7.0 * var) / kTurnBlockS));
        const double turn_rate =
            kTurnMeanRate + (block % 2 == 0 ? kTurnHardRate : -kTurnHardRate);
        p_hdg = glm::normalize(glm::angleAxis(turn_rate * dt, p_up) * p_hdg);
        const double alt =
            kAltMeanM + kAltSwingM * std::sin(2.0 * kPi * (time_s + 5.0 * var) /
                                              kAltPeriodS);
        const glm::dvec3 axis = glm::normalize(glm::cross(p_up, p_hdg));
        glm::dvec3 npos =
            glm::angleAxis(kPlayerSpeed * dt / kAp.R, axis) * player.position;
        const glm::dvec3 nup = glm::normalize(npos);
        npos = nup * (w.hf.radius_at(nup) + alt);
        player.position = npos;
        player.velocity = (npos - player_prev.position) / dt;
        p_hdg = glm::normalize(p_hdg - glm::dot(p_hdg, nup) * nup);
        player.last_vhat = glm::normalize(player.velocity);
        {
            const glm::dvec3 right = glm::normalize(glm::cross(p_hdg, nup));
            player.orientation =
                glm::normalize(glm::quat_cast(glm::dmat3{right, nup, -p_hdg}));
        }

        // The real app order. assign_foes owns engaged/foe (never hand-set).
        const int max_engaged =
            combat::difficulty_params(cw.setup.difficulty).max_engaged;
        combat::assign_foes(fleet, player, combat::CQ_VALLEY, max_engaged, dp);

        drone::DroneState& b = fleet[0];
        const sim::SimState before = b.curr;
        const sim::SimState* tgt =
            b.foe == drone::kFoePlayer ? &player : nullptr;
        const double cd_before = b.fire_cooldown;
        const drone::DroneTickResult r = drone::tick(b, kAp, dp, &w.env, tgt);
        if (r.respawned) {
            ++out.crashes;
            b.curr = drone::level_state_at(
                dp, d_dir * (w.hf.radius_at(d_dir) + kAltMeanM), p_hdg);
            b.prev = b.curr;
            continue;
        }

        // ★ MODE COUNTS ARE FOE-GATED. drone::tick resets d.bfm to a fresh
        // Intercept on every tick the drone is not engaged (FIX-5), so
        // counting unconditionally reports 95 percent Intercept and says
        // nothing about the fight -- measured on the first run of this probe.
        const bool on_player = b.foe == drone::kFoePlayer;
        if (on_player) {
            ++out.foe_ticks;
            const int m = static_cast<int>(b.bfm.mode);
            if (m >= 0 && m < 7) ++out.mode_ticks[m];
        }
        if (b.wants_fire) ++out.fire_ticks;
        out.max_bank_deg = std::max(out.max_bank_deg,
                                    std::abs(achieved_bank(b.curr)) * 180.0 / kPi);
        out.max_n = std::max(out.max_n, achieved_n(before, b.curr, dt));
        const double rng = glm::length(player.position - b.curr.position);
        out.min_range_m = std::min(out.min_range_m, rng);

        // A TRACKING SOLUTION, defined exactly as the fire gate defines one:
        // the full ballistic lead inside the cone, coordinated, in the band.
        // The spec's P-H bar ("a sustained tracking solution >= 2 s") is this.
        {
            const glm::dvec3 nose =
                b.curr.orientation * glm::dvec3{0.0, 0.0, -1.0};
            const glm::dvec3 aim =
                bfm::lead_point(b.curr, player, dp.pursue_lead_speed,
                                dp.pursue_lead_max_s) -
                b.curr.position;
            const double la = glm::length(aim);
            const double spd = glm::length(b.curr.velocity);
            const bool coordinated =
                spd < 1e-6 || glm::dot(b.curr.velocity, nose) / spd >=
                                  dp.fire_align_cos;
            const bool pointed = coordinated && la > 1e-6 &&
                                 glm::dot(nose, aim / la) >= dp.fire_cone_cos;
            const bool solution =
                pointed && rng >= dp.fire_range_min &&
                rng <= std::max(dp.fire_range_max, dp.snapshot_range_m);
            track_run = solution ? track_run + 1 : 0;
            cone_run = pointed ? cone_run + 1 : 0;
            out.best_track_s =
                std::max(out.best_track_s, static_cast<double>(track_run) * dt);
            out.best_cone_s =
                std::max(out.best_cone_s, static_cast<double>(cone_run) * dt);
            if (on_player)
                out.engaged_min_range_m =
                    std::min(out.engaged_min_range_m, rng);
        }

        combat::enemy_fire_tick(cw, fleet, dt, kAp.g, kAp.R, nullptr);
        if (fleet[0].fire_cooldown > cd_before) ++out.rounds;
        combat::combat_player_tick(cw, player_prev, player);
    }
    out.hits = cw.player_hits;
    return out;
}

// ---------------------------------------------------------------------------
// PROBE 3 — UNDER THE GUN (the E7.3 attribution + acceptance instrument).
//
// A SCRIPTED ATTACKER welded to the bandit's six: every tick the "player" is
// placed `range` metres directly behind the bandit along its own velocity, with
// his nose on it and 275 m/s of closure-class speed. That is the geometry Chad
// described, held deterministically, so what the DEFENDER does with it is the
// only variable.
//
// What it measures is the shape of the evasion, not its success:
//   mean_bank_deg   — how banked the defender actually is (a break is banked);
//   gamma_flips     — sign changes of the achieved flight-path-angle RATE, per
//                     minute. THE BOB SIGNATURE: "elevator dipping up and down"
//                     is exactly a high flip count at low bank.
struct EvadeResult {
    double mean_bank_deg = 0.0;
    double gamma_flips_per_min = 0.0;
    double mean_n = 0.0;
    long long defensive_ticks = 0;
    long long snapshot_ticks = 0;
    int crashes = 0;
    long long mode_ticks[7] = {0, 0, 0, 0, 0, 0, 0};
    double gamma_swing_deg = 0.0;
    double min_alt_m = 1e18;
};

EvadeResult under_the_gun(const World& w, const drone::DroneParams& dp_in,
                          int spawn_index, double seconds, double range_m) {
    drone::DroneParams dp = dp_in;
    dp.maverick.enabled = false;

    const glm::dvec3 c = glm::normalize(world::kSudburyCenterDir);
    const glm::dvec3 east =
        glm::normalize(glm::cross(glm::dvec3{0.0, 1.0, 0.0}, c));
    drone::DroneState d = drone::spawn_drone(kAp, dp, spawn_index,
                                             combat::kNumMavericks);
    d.curr = drone::level_state_at(dp, c * (w.hf.radius_at(c) + 2500.0), east);
    d.prev = d.curr;
    d.hp = dp.hp;
    d.engaged = true;  // this leg is about the DEFENSIVE branch, so the fight
    d.foe = drone::kFoePlayer;  //   assignment is held fixed by construction

    const double dt = kAp.sim_dt;
    const int ticks = static_cast<int>(seconds / dt);
    EvadeResult out;
    double sum_bank = 0.0, sum_n = 0.0;
    int n_samples = 0;
    long long flips = 0;
    double prev_gamma = 0.0, prev_rate = 0.0;
    double gam_lo = 1e18, gam_hi = -1e18;
    bool have_prev = false;

    for (int t = 0; t < ticks; ++t) {
        // The attacker: on the six, nose on, at the bandit's own altitude.
        const glm::dvec3 v = d.curr.velocity;
        glm::dvec3 back = glm::length(v) > 1e-6
                              ? -glm::normalize(v)
                              : -(d.curr.orientation *
                                  glm::dvec3{0.0, 0.0, -1.0});
        sim::SimState atk;
        atk.position = d.curr.position + back * range_m;
        const glm::dvec3 to_d = glm::normalize(d.curr.position - atk.position);
        atk.velocity = 275.0 * to_d;
        atk.last_vhat = to_d;
        {
            // The attacker's nose points AT the bandit -- that is the whole
            // fixture, and BfmGeom::threat_cos is measured against it.
            // ★ THE BASIS MUST BE RIGHT-HANDED, in the SAME convention as the
            // stepped player above: right = cross(forward, up), up =
            // cross(right, forward), columns {right, up, -forward}. The first
            // draft used cross(right, -forward) for the up column, which is
            // MINUS up (BAC-CAB), and glm::quat_cast on that mirrored basis
            // produced an attacker whose reported nose wandered -- the break
            // armed and released on noise and the probe read 296 defensive
            // ticks where the machine actually wanted ~1900. Measured, not
            // reasoned: the mode histogram is what exposed it.
            const glm::dvec3 aup = sim::local_up(atk.position);
            const glm::dvec3 right = glm::normalize(glm::cross(to_d, aup));
            const glm::dvec3 upv = glm::cross(right, to_d);
            atk.orientation = glm::normalize(
                glm::quat_cast(glm::dmat3{right, upv, -to_d}));
        }

        // ★ THE ROUNDS MUST ACTUALLY ARRIVE, and this line is the whole reason
        // the probe reported 0 defensive ticks with the dial fully armed. The
        // break's predicate is `under_fire AND the geometry`, and `under_fire`
        // means ROUNDS HAVE LANDED — the P-A measurement forced that (arming on
        // the geometry alone takes the ensemble to zero rounds at the player).
        // This fixture scripts the attacker's GEOMETRY, so it must book his
        // HITS too or the predicate can never arm and the probe measures
        // nothing at all. Booked through the SAME gun window the app's AI-vs-AI
        // damage seam uses (combat::ai_guns_on at the drone fire band), so the
        // fixture models the real seam instead of inventing a cadence: a
        // defender the attacker cannot currently shoot stops being under fire
        // on the defensive_memory_s window, exactly as in the game.
        if (combat::ai_guns_on(atk, d.curr, dp.fire_range_min,
                               dp.fire_range_max))
            d.took_fire = true;

        const sim::SimState before = d.curr;
        const drone::DroneTickResult rr = drone::tick(d, kAp, dp, &w.env, &atk);
        if (rr.respawned) ++out.crashes;
        out.min_alt_m =
            std::min(out.min_alt_m, sim::altitude(d.curr.position, kAp));
        {
            const int mi = static_cast<int>(d.bfm.mode);
            if (mi >= 0 && mi < 7) ++out.mode_ticks[mi];
        }

        if (d.bfm.mode == bfm::BfmState::Mode::Defensive)
            ++out.defensive_ticks;
        if (d.wants_fire) ++out.snapshot_ticks;

        sum_bank += std::abs(achieved_bank(d.curr)) * 180.0 / kPi;
        sum_n += achieved_n(before, d.curr, dt);
        ++n_samples;

        const glm::dvec3 up2 = sim::local_up(d.curr.position);
        const double sp = glm::length(d.curr.velocity);
        const double gamma =
            sp > 1e-6 ? std::asin(std::clamp(
                            glm::dot(d.curr.velocity / sp, up2), -1.0, 1.0))
                      : 0.0;
        if (have_prev) {
            const double rate = (gamma - prev_gamma) / dt;
            // A FLIP is a sign change in the climb RATE with real amplitude.
            // ★ HYSTERETIC, and that is load-bearing: the first draft required
            // BOTH the current and the previous tick to clear the threshold,
            // which is exactly the pair of ticks a smooth zero crossing does
            // NOT clear -- it counted zero flips on a trace with 62 degrees of
            // gamma swing. The counter therefore remembers the last
            // SIGNIFICANT rate and counts a flip when the next significant one
            // has the opposite sign. 0.02 rad/s is ~1.1 deg/s.
            if (std::abs(rate) > 0.02) {
                if (prev_rate != 0.0 && rate * prev_rate < 0.0) ++flips;
                prev_rate = rate;
            }
        }
        gam_lo = std::min(gam_lo, gamma);
        gam_hi = std::max(gam_hi, gamma);
        prev_gamma = gamma;
        have_prev = true;
    }
    if (n_samples > 0) {
        out.mean_bank_deg = sum_bank / n_samples;
        out.mean_n = sum_n / n_samples;
    }
    out.gamma_flips_per_min = static_cast<double>(flips) / (seconds / 60.0);
    out.gamma_swing_deg =
        gam_hi > gam_lo ? (gam_hi - gam_lo) * 180.0 / kPi : 0.0;
    return out;
}

// ---------------------------------------------------------------------------
// PROBE 4 — THE SCOPING TRACE (the E7.4 safety instrument).
//
// ★ WHAT IT EXISTS TO PROVE, and it is a SAFETY claim, not a performance one:
// `pursue_track_pitch_gain` repeals a measured over-gain in the TRACKING task
// ONLY. `pursue_pitch_gain` is ALSO the terrain-avoidance PULL-UP gain and the
// raid / defend / strike / arena-guard errand gain, so a global reduction is a
// CFIT risk (spec E7.E's safety note, and the reason the E7.F sweep arms --
// which lowered it globally -- are not a shippable form).
//
// The trace is deliberately BIT-COMPARABLE: one bandit, one fixed target, N
// ticks, and the final state returned raw. Two arms that must be `==` prove the
// scoping; the SAME fixture flown where the dial IS supposed to bite proves the
// comparison is not vacuous.
struct TrackTrace {
    sim::SimState last;
    double min_agl_m = 1e18;
    long long avoid_ticks = 0;   // ticks the forced pull-up was engaged
    int crashes = 0;
};

TrackTrace track_flight(const World& w, const drone::DroneParams& dp_in,
                        int spawn_index, double start_agl_m, double seconds,
                        bool engaged, bool raiding) {
    drone::DroneParams dp = dp_in;
    dp.maverick.enabled = false;

    const glm::dvec3 c = glm::normalize(world::kSudburyCenterDir);
    const glm::dvec3 east =
        glm::normalize(glm::cross(glm::dvec3{0.0, 1.0, 0.0}, c));
    drone::DroneState d =
        drone::spawn_drone(kAp, dp, spawn_index, combat::kNumMavericks);
    d.curr = drone::level_state_at(
        dp, c * (w.hf.radius_at(c) + start_agl_m), east);
    d.prev = d.curr;
    d.hp = dp.hp;
    d.engaged = engaged;
    d.foe = engaged ? drone::kFoePlayer : drone::kFoeNone;
    if (raiding) {
        d.raid.active = true;
        // A pump 30 km along track and 500 m DOWN: an errand that commands a
        // real descent, which is exactly where a pitch gain shows up.
        const glm::dvec3 tgt_dir =
            glm::normalize(c + east * (30000.0 / kAp.R));
        d.raid.target_pos = tgt_dir * (w.hf.radius_at(tgt_dir) + 200.0);
        d.raid.pump_idx = 0;
    }

    // The target: FIXED in space, 900 m ahead and 30 deg off the initial nose,
    // at the bandit's own start altitude. Fixed (not re-derived per tick like
    // the saturated turn) so the two arms see an identical stimulus until they
    // themselves diverge.
    const glm::dvec3 up0 = sim::local_up(d.curr.position);
    const glm::dvec3 nose0 = d.curr.orientation * glm::dvec3{0.0, 0.0, -1.0};
    const glm::dvec3 dir0 =
        glm::normalize(glm::angleAxis(-30.0 * kPi / 180.0, up0) * nose0);
    sim::SimState tgt;
    tgt.position = d.curr.position + dir0 * 900.0;
    tgt.velocity = glm::dvec3{0.0};
    tgt.orientation = d.curr.orientation;
    tgt.last_vhat = dir0;

    TrackTrace out;
    const double dt = kAp.sim_dt;
    const int ticks = static_cast<int>(seconds / dt);
    for (int t = 0; t < ticks; ++t) {
        const drone::DroneTickResult rr =
            drone::tick(d, kAp, dp, &w.env, engaged ? &tgt : nullptr);
        if (rr.respawned) ++out.crashes;
        const glm::dvec3 u = glm::normalize(d.curr.position);
        out.min_agl_m = std::min(out.min_agl_m,
                                 glm::length(d.curr.position) - w.hf.radius_at(u));
        if (d.terrain_avoid_engaged) ++out.avoid_ticks;
    }
    out.last = d.curr;
    return out;
}

bool same_state(const sim::SimState& a, const sim::SimState& b) {
    return a.position == b.position && a.velocity == b.velocity &&
           a.orientation == b.orientation;
}

}  // namespace

// ===========================================================================
// E7.1 — THE ACES TURN
// ===========================================================================

TEST_CASE("E7.1: the tiers come from the trait table and nobody else moves") {
    const drone::DroneParams dp = kScen.drone;
    const double base = dp.pursue_max_bank * 180.0 / kPi;
    const double ace = dp.ace_bank_cap * 180.0 / kPi;
    const double mid = dp.mid_bank_cap * 180.0 / kPi;
    INFO("shipped caps: base=" << base << " mid=" << mid << " ace=" << ace);
    REQUIRE(base == 55.0);  // the RULED cap is untouched by this rung

    // The whole wing, by callsign, so a trait-table edit shows up here.
    for (int i = 0; i < 10; ++i) {
        const double agg = maverick::traits_for(i).aggression;
        const double cap = drone::engaged_bank_cap(dp, i) * 180.0 / kPi;
        INFO(maverick::traits_for(i).callsign << " aggression=" << agg
                                              << " engaged cap=" << cap);
        if (agg >= dp.ace_aggression_min)
            REQUIRE(cap == ace);
        else if (agg >= dp.mid_aggression_min)
            REQUIRE(cap == mid);
        else
            REQUIRE(cap == base);
    }
    // The named tiers of the ruling, spelled out.
    REQUIRE(drone::engaged_bank_cap(dp, 5) == dp.ace_bank_cap);   // PITVIPER
    REQUIRE(drone::engaged_bank_cap(dp, 1) == dp.ace_bank_cap);   // SHAFT
    REQUIRE(drone::engaged_bank_cap(dp, 8) == dp.ace_bank_cap);   // MURRAY
    REQUIRE(drone::engaged_bank_cap(dp, 3) == dp.mid_bank_cap);   // NICKEL .70
    REQUIRE(drone::engaged_bank_cap(dp, 6) == dp.pursue_max_bank);  // LAMPLIGHT

    // THE SCOPE GUARANTEE: with the dial off EVERY pilot is back on the ruled
    // cap, which is what makes the repeal one number wide.
    const drone::DroneParams off = pre_e7(dp);
    for (int i = 0; i < 10; ++i)
        REQUIRE(drone::engaged_bank_cap(off, i) == off.pursue_max_bank);
}

TEST_CASE("E7.1: the raised cap is engaged-only, never the errand paths") {
    // A pilot flying a RAID errand steers on raid.bank_cap, and a PATROL pilot
    // on program_bank — neither may acquire the ace's authority. Proven by
    // flying an ACE (PITVIPER, index 5) on a raid with the dial on and off:
    // the trajectories must be bit-identical, because the repeal exists at one
    // seam that a raider never reaches.
    World w;
    build_world(w);
    const auto fly = [&](const drone::DroneParams& dp_in) {
        drone::DroneParams dp = dp_in;
        dp.maverick.enabled = false;
        const glm::dvec3 c = glm::normalize(world::kSudburyCenterDir);
        const glm::dvec3 east =
            glm::normalize(glm::cross(glm::dvec3{0.0, 1.0, 0.0}, c));
        const double ang = 3000.0 / kAp.R;
        const glm::dvec3 pdir =
            glm::normalize(std::cos(ang) * c + std::sin(ang) * east);
        drone::DroneState d =
            drone::spawn_drone(kAp, dp, 5, combat::kNumMavericks);
        d.curr = drone::level_state_at(dp, c * (w.hf.radius_at(c) + 2500.0),
                                       east);
        d.prev = d.curr;
        d.hp = dp.hp;
        d.raid.active = true;
        d.raid.target_pos = pdir * (w.hf.radius_at(pdir) + 10.0);
        for (int t = 0; t < 2400; ++t)
            drone::tick(d, kAp, dp, &w.env, nullptr);  // no foe: pure errand
        return d.curr;
    };
    const sim::SimState on = fly(kScen.drone);
    const sim::SimState off = fly(pre_e7(kScen.drone));
    REQUIRE(on.position == off.position);
    REQUIRE(on.velocity == off.velocity);
    REQUIRE(on.orientation == off.orientation);
}

TEST_CASE("E7.1 G honesty: the plant actually flies the raised bank") {
    // THE MEASUREMENT THIS RUNG LIVES OR DIES ON. A commanded 72 deg that the
    // soft AoA limiter mushes into 50 is a number, not a killer. The SAME pilot
    // (PITVIPER, index 5) is flown through the real plant with and without the
    // repeal at four bearings, and everything reported is ACHIEVED.
    World w;
    build_world(w);
    const drone::DroneParams on = kScen.drone;
    const drone::DroneParams off = pre_e7(kScen.drone);
    // The RULED cap, which is what the deadband fades back TO and what the
    // base tier flies everywhere. Read off the loader, never written down.
    const double base_cap_deg = off.pursue_max_bank * 180.0 / kPi;

    const double bearings[4] = {30.0, 50.0, 70.0, 90.0};
    for (double bd : bearings) {
        const double b = bd * kPi / 180.0;
        const TurnResult ace = saturated_turn(w, on, 5, b);
        const TurnResult base = saturated_turn(w, off, 5, b);
        INFO("bearing " << bd << " deg | ACE cmd=" << ace.cmd_bank_deg
                        << " slewed_cmd=" << ace.cmd_bank_flown_deg
                        << " ACHIEVED bank=" << ace.bank_deg
                        << " net turn=" << ace.turn_dps << " deg/s n=" << ace.n
                        << " V=" << ace.speed
                        << " gamma swing=" << ace.gamma_swing_deg
                        << " crashes=" << ace.crashes
                        << "  || BASE cmd=" << base.cmd_bank_deg
                        << " slewed_cmd=" << base.cmd_bank_flown_deg
                        << " ACHIEVED bank=" << base.bank_deg
                        << " net turn=" << base.turn_dps << " deg/s n="
                        << base.n << " V=" << base.speed
                        << " gamma swing=" << base.gamma_swing_deg
                        << " crashes=" << base.crashes);
        REQUIRE(ace.cmd_bank_deg == on.ace_bank_cap * 180.0 / kPi);
        REQUIRE(base.cmd_bank_deg == 55.0);
        // Neither arm is measuring a wreck.
        REQUIRE(ace.crashes == 0);
        REQUIRE(base.crashes == 0);
        // ★ WHAT IS ON TRIAL HERE IS THE PLANT, so every clause below is
        // measured against cap_handed_deg — the cap the MACHINE gave this pilot
        // at this bearing — not against the flat tier number. Inside
        // ace_bank_track_lo the E7.1 deadband hands out the ruled 55 on purpose
        // and the aeroplane flying 55 there is the design working, not mush.
        // (With the deadband off, cap_handed_deg IS the flat tier cap and these
        // are the same assertions they were.)
        REQUIRE(ace.cap_handed_deg >= base.cap_handed_deg - 1e-9);
        // (a) THE E1.4 SLEW AT THE HIGHER RATE. bank_slew_dps must still carry
        // the command all the way to the cap handed out — a slew that could not
        // cross it would make the repeal a number the aeroplane never sees.
        // (Loader-checked too; measured here at the seam.)
        REQUIRE(ace.cmd_bank_flown_deg >= 0.999 * ace.cap_handed_deg);
        // (b) NO MUSH IN ROLL: the achieved bank tracks the raised command.
        REQUIRE(ace.bank_deg >= 0.95 * ace.cap_handed_deg);
        REQUIRE(base.bank_deg >= 0.95 * base.cmd_bank_deg);
        // ★★ (b2) AND THE DELIVERED TURN MUST BE CLEAN AT EVERY BEARING (rung
        // E10). This clause existed only at the single 100-deg payoff bearing
        // below, and that is exactly how a cap of 80 nearly shipped: at 80 the
        // aeroplane still HOLDS the bank (101-103% of command, so clause (b)
        // passes everywhere) while the flight path swings 13.9 deg abeam --
        // half the fire cone. At 83 it collapses outright (achieved 46-54% of
        // command, 103-120 deg of swing). A bank the plant holds but cannot fly
        // SMOOTHLY is not a turn a gun solution survives, and only a
        // per-bearing clause catches it. Bound is half the fire cone, the same
        // ruler rung E9 picked its pitch gain with, config-relative.
        {
            const double cone_deg =
                std::acos(kScen.drone.fire_cone_cos) * 180.0 / kPi;
            INFO("bearing " << bd << " ACE swing " << ace.gamma_swing_deg
                            << " deg against half the fire cone "
                            << 0.5 * cone_deg);
            REQUIRE(ace.gamma_swing_deg < 0.5 * cone_deg);
        }
        // (c) THE PAYOFF: WHERE THE REPEAL IS FULLY DELIVERED, it must buy real
        // net TURN RATE — the wall this rung exists to break.
        //
        // ★ THE CLAUSE IS GATED ON FULL DELIVERY, and that is a measurement,
        // not a hedge to make a number pass. PARTIAL delivery is not a small
        // payoff, it is NO payoff: at 30 deg the ramp leaks 1.3 deg past the
        // ruled cap (the off-angle wobbles just outside track_lo in the turn)
        // and the ace measurably turns SLOWER for it — 2.21 deg/s against the
        // base arm's 2.39. The reason is visible in the same trace: this
        // fixture's saturated turn is PORPOISING at n ~ 5.5 against 1/cos(55) =
        // 1.74, so its net turn rate is dominated by pitch thrash, and 1.3 deg
        // of extra bank moves the thrash more than it moves the turn. Claiming
        // a payoff from an authority the pilot was never actually handed is how
        // a rung ships a dial that does nothing. The partial cells are PRINTED
        // instead, which is where that finding came from.
        if (ace.cap_handed_deg >= 0.99 * ace.cmd_bank_deg) {
            REQUIRE(ace.turn_dps > base.turn_dps);
        } else {
            INFO("bearing " << bd << " deg: the tracking deadband handed out "
                 << ace.cap_handed_deg << " of the tier's " << ace.cmd_bank_deg
                 << " (base " << base.cap_handed_deg << ") — PARTIAL, so no "
                 << "payoff is claimed. turn " << ace.turn_dps << " vs "
                 << base.turn_dps << " deg/s");
        }
    }

    // ★ THE DEADBAND IS A MEASURED PROPERTY, not a comment. Pinned so a retune
    // cannot silently turn the ramp into a flat cap (or into nothing) without
    // this failing: on the nose the ace flies the RULED 55; abeam he flies his
    // full tier authority. Skipped when the deadband is dialled off, which is
    // the FLAT arm and a legitimate shipping choice.
    if (on.ace_bank_track_hi > on.ace_bank_track_lo) {
        const TurnResult nose = saturated_turn(w, on, 5, 10.0 * kPi / 180.0);
        const TurnResult abeam = saturated_turn(w, on, 5, 100.0 * kPi / 180.0);
        INFO("deadband: cap handed at 10 deg off = " << nose.cap_handed_deg
             << " (ruled 55 expected), at 100 deg off = " << abeam.cap_handed_deg
             << " (tier " << on.ace_bank_cap * 180.0 / kPi << " expected)");
        REQUIRE(nose.cap_handed_deg < base_cap_deg + 1.0);
        REQUIRE(abeam.cap_handed_deg > base_cap_deg + 1.0);
    }

    // ★ THE HONEST G STORY, recorded rather than asserted away, and taken at a
    // bearing where the repeal is ACTUALLY DELIVERED (100 deg off — abeam, past
    // ace_bank_track_hi, so the ramp hands out the full tier cap; at 30 deg the
    // deadband hands out the ruled 55 and there is no raised bank to tell a
    // story about). The load factor there is NOT 1/cos(cap), because that
    // identity only holds for a LEVEL turn and this one is not level. The turn
    // rate the plant actually delivers is the one that follows from the G it
    // actually pulls, g*sqrt(n^2-1)/V, and THAT is what is checked: the
    // measurement is internally consistent, so neither number is invented.
    const double kPayoffBearing = 100.0 * kPi / 180.0;
    const TurnResult ace_hi = saturated_turn(w, on, 5, kPayoffBearing);
    const double predicted_dps =
        kAp.g * std::sqrt(std::max(0.0, ace_hi.n * ace_hi.n - 1.0)) /
        std::max(1.0, ace_hi.speed) * 180.0 / kPi;
    INFO("ACE at 100 deg bearing: tier cap " << ace_hi.cmd_bank_deg
         << " handed " << ace_hi.cap_handed_deg
         << " achieved bank " << ace_hi.bank_deg << " n=" << ace_hi.n
         << " V=" << ace_hi.speed << " net turn=" << ace_hi.turn_dps
         << " deg/s (g*sqrt(n^2-1)/V predicts " << predicted_dps << ")");
    REQUIRE(ace_hi.turn_dps > 0.75 * predicted_dps);
    REQUIRE(ace_hi.turn_dps < 1.35 * predicted_dps);

    // ★★ THE WALL AND THE REPEAL, IN ONE PAIR OF NUMBERS — the finding this
    // probe exists to record, and it is NOT the one the E7 draft recorded.
    //
    // The draft claimed "the raised bank reduces the porpoise", measured at 30
    // deg bearing. That claim does not survive the tracking deadband: at 30 deg
    // BOTH arms are handed the ruled 55, so its ACE(72)/BASE(55) comparison was
    // the same aeroplane flown twice (14.07 vs 14.21 deg of swing — a null).
    // What IS true, and is pinned here, is a statement about the PLANT:
    //
    //   ON THE NOSE, at the ruled 55, the saturated turn PORPOISES — ~14 deg of
    //   flight-path swing and n ~ 5.5 against 1/cos(55) = 1.74, most of the G
    //   going into pitch thrash instead of turn — and it nets ~2.4 deg/s. THAT
    //   is the wall this ladder has been hitting, and it is also Chad's
    //   "elevator dipping up and down" appearing in a second, independent
    //   fixture that has nothing to do with evasion.
    //
    //   OFF THE NOSE, at the full tier cap, the SAME aeroplane turns cleanly:
    //   ~1.5 deg of swing and ~28 deg/s — inside the 25-40 deg/s LOS band the
    //   rung's own attribution says a gun solution needs.
    //
    // So the repeal is REAL where it is delivered, and the deadband is exactly
    // what denies it in the tracking regime. Both halves are asserted so a
    // retune cannot lose either.
    const TurnResult nose_base = saturated_turn(w, off, 5, 30.0 * kPi / 180.0);
    INFO("THE WALL (55 deg, on the nose): gamma swing "
         << nose_base.gamma_swing_deg << " deg, n=" << nose_base.n
         << ", net turn " << nose_base.turn_dps << " deg/s   ||   THE REPEAL ("
         << ace_hi.cap_handed_deg << " deg, abeam): gamma swing "
         << ace_hi.gamma_swing_deg << " deg, n=" << ace_hi.n << ", net turn "
         << ace_hi.turn_dps << " deg/s");
    REQUIRE(nose_base.gamma_swing_deg > 10.0);   // it porpoises
    REQUIRE(nose_base.n > 3.0);                  // on G that is not turning it
    REQUIRE(nose_base.turn_dps < 5.0);           // and it barely turns
    REQUIRE(ace_hi.gamma_swing_deg < 5.0);       // the delivered turn is clean
    REQUIRE(ace_hi.turn_dps > 20.0);             // and it is a REAL turn rate
}

// ===========================================================================
// E7.2 — THE SLASHERS
// ===========================================================================

TEST_CASE("E7.2: the doctrine off arm is bit identical") {
    // TWO structural off-values, each proven on its own against a FULLY ARMED
    // slasher (CANARY, index 2 — an ENEMY of the player and base tier, so the
    // doctrine selector picks him up).
    World w;
    build_world(w);
    const auto fly = [&](bool selector, double perch_h) {
        drone::DroneParams dp = kScen.drone;
        dp.maverick.enabled = false;
        dp.slash_doctrine = selector;
        dp.bfm.perch_height_m = perch_h;
        dp.bfm.defensive_range_m = 0.0;  // one variable at a time
        return ace_course(w, dp, 2, 1.5, 0);
    };
    const DuelResult sel_off = fly(false, kScen.drone.bfm.perch_height_m);
    const DuelResult h_off = fly(true, 0.0);
    const DuelResult both_off = fly(false, 0.0);
    const DuelResult on = fly(true, kScen.drone.bfm.perch_height_m);
    INFO("Perch/Slash ticks: selector-off=" << sel_off.mode_ticks[4] << "/"
         << sel_off.mode_ticks[5] << "  height-off=" << h_off.mode_ticks[4]
         << "/" << h_off.mode_ticks[5] << "  ON=" << on.mode_ticks[4] << "/"
         << on.mode_ticks[5]);
    // Either off-value alone removes the doctrine entirely...
    REQUIRE(sel_off.mode_ticks[4] == 0);
    REQUIRE(sel_off.mode_ticks[5] == 0);
    REQUIRE(h_off.mode_ticks[4] == 0);
    REQUIRE(h_off.mode_ticks[5] == 0);
    // ...and reproduces the pre-E7 machine tick for tick.
    for (int m = 0; m < 7; ++m) {
        REQUIRE(sel_off.mode_ticks[m] == both_off.mode_ticks[m]);
        REQUIRE(h_off.mode_ticks[m] == both_off.mode_ticks[m]);
    }
    REQUIRE(sel_off.rounds == both_off.rounds);
    REQUIRE(h_off.rounds == both_off.rounds);
    // NON-VACUITY: the dial must actually do something when it is on.
    REQUIRE(on.mode_ticks[4] + on.mode_ticks[5] > 0);
}

TEST_CASE("E7.2: a slasher perches slashes and never sustained turns") {
    World w;
    build_world(w);
    drone::DroneParams dp = kScen.drone;
    dp.bfm.defensive_range_m = 0.0;  // isolate the doctrine from the break
    // ★ THE DOCTRINE IS ARMED HERE, NOT READ FROM THE SHIPPED TABLE. E7.2 is
    // HELD OFF in config (spec E7.N: the slashers have never been measured
    // firing a round through the real fire gate, and `probe P-D` — a signed E3
    // rung — reads 0 rounds with it on). This leg tests the doctrine's SHAPE,
    // which is correct and worth keeping alive while the gun defect is chased,
    // so it arms its own dial. That is the right form regardless: a behaviour
    // test should never depend on a dial's shipped value to be reached.
    dp.slash_doctrine = true;
    const DuelResult r = ace_course(w, dp, 2, 5.0, 1);  // CANARY, base tier
    const long long fight = r.mode_ticks[1] + r.mode_ticks[2] +
                            r.mode_ticks[4] + r.mode_ticks[5];
    INFO("CANARY modes I/O/Y/E/P/S/D = "
         << r.mode_ticks[0] << "/" << r.mode_ticks[1] << "/" << r.mode_ticks[2]
         << "/" << r.mode_ticks[3] << "/" << r.mode_ticks[4] << "/"
         << r.mode_ticks[5] << "/" << r.mode_ticks[6]
         << "  rounds=" << r.rounds << " best_track=" << r.best_track_s
         << " s min_range=" << r.min_range_m);
    REQUIRE(r.foe_ticks > 0);  // non-vacuity: he was actually engaged
    REQUIRE(fight > 0);
    // The doctrine REPLACES the sustained turning fight for this pilot: he
    // never enters Offensive or Yoyo while he can climb. (The documented
    // fallback is the thin-air one, and this fixture flies in full air.)
    REQUIRE(r.mode_ticks[1] == 0);
    REQUIRE(r.mode_ticks[2] == 0);
    // Both halves of the doctrine actually run — a perch that never slashes is
    // a guns-cold pilot, and a slash that never perches is bare pursuit.
    REQUIRE(r.mode_ticks[4] > 0);
    REQUIRE(r.mode_ticks[5] > 0);
    // THE ANTI-DITHER GUARANTEE, measured: the guns-hot half of the doctrine
    // gets a real share of the fight, never a token tick.
    const double slash_frac =
        static_cast<double>(r.mode_ticks[5]) / static_cast<double>(fight);
    INFO("guns-hot (Slash) share of the fight: " << slash_frac);
    REQUIRE(slash_frac > 0.15);
}

TEST_CASE("E7.2: the slash is the doctrines only guns hot mode") {
    // Pure bfm, no plant: the guns_hot veto per mode. Perch and the break are
    // cold; only the pass is hot.
    bfm::BfmParams bp = kScen.drone.bfm;
    bp.enabled = true;
    bfm::BfmDials dl;
    dl.doctrine_slash = true;
    sim::SimState s;
    s.position = glm::dvec3{kAp.R + 2500.0, 0.0, 0.0};
    s.velocity = glm::dvec3{0.0, 150.0, 0.0};
    s.last_vhat = glm::dvec3{0.0, 1.0, 0.0};
    sim::SimState p = s;
    p.position += glm::dvec3{0.0, 600.0, 0.0};
    p.velocity = glm::dvec3{0.0, 275.0, 0.0};
    p.last_vhat = glm::dvec3{0.0, 1.0, 0.0};
    const auto hot = [&](bfm::BfmState::Mode m) {
        bfm::BfmState st;
        st.mode = m;
        st.mode_ticks = 1;
        return bfm::bfm_step(st, s, p, bp, dl, kAp, kAp.sim_dt).guns_hot;
    };
    REQUIRE_FALSE(hot(bfm::BfmState::Mode::Perch));
    REQUIRE(hot(bfm::BfmState::Mode::Slash));
    REQUIRE_FALSE(hot(bfm::BfmState::Mode::Defensive));
}

// ===========================================================================
// E7.3 — REAL EVASION
// ===========================================================================

TEST_CASE("E7.3 attribution: the elevator bob is a tracking artefact") {
    // ATTRIBUTION BEFORE THE FIX. Chad: "They only tried to dodge my shots with
    // elevator dipping up and down." The measurement below shows WHY, with the
    // rung's own dial OFF: a bandit with an attacker welded to its six is still
    // flying a TRACKING mode (there has never been a defensive branch at all),
    // so its bank sits low while its flight-path angle reverses over and over
    // chasing the attacker's elevation. That is the bob, and it is an artefact
    // of tracking, not an evasion behaviour anyone wrote.
    World w;
    build_world(w);
    const EvadeResult before = under_the_gun(w, pre_e7(kScen.drone), 5, 25.0,
                                             500.0);
    INFO("E6 (no defensive branch): mean bank=" << before.mean_bank_deg
         << " deg  gamma flips/min=" << before.gamma_flips_per_min
         << "  mean n=" << before.mean_n
         << "  Defensive ticks=" << before.defensive_ticks
         << "  gamma swing=" << before.gamma_swing_deg
         << "  crashes=" << before.crashes << "  min_alt=" << before.min_alt_m
         << "  modes I/O/Y/E/P/S/D=" << before.mode_ticks[0] << "/"
         << before.mode_ticks[1] << "/" << before.mode_ticks[2] << "/"
         << before.mode_ticks[3] << "/" << before.mode_ticks[4] << "/"
         << before.mode_ticks[5] << "/" << before.mode_ticks[6]);
    REQUIRE(before.defensive_ticks == 0);  // the branch does not exist off
    // The bob is REAL and MEASURED, not quoted: the vertical channel reverses
    // repeatedly while the aeroplane is barely banked.
    REQUIRE(before.gamma_flips_per_min > 0.0);
}

TEST_CASE("E7.3: the break turn replaces the bob with flying") {
    World w;
    build_world(w);
    // MURRAY (index 8, aggression 0.80) — an ace, so the break is flown at the
    // raised authority, which is the whole point of "at their bank authority".
    const EvadeResult off = under_the_gun(w, pre_e7(kScen.drone), 8, 25.0, 500.0);
    const EvadeResult on = under_the_gun(w, kScen.drone, 8, 25.0, 500.0);
    INFO("under the gun, MURRAY | OFF mean bank=" << off.mean_bank_deg
         << " deg flips/min=" << off.gamma_flips_per_min << " n=" << off.mean_n
         << " || ON mean bank=" << on.mean_bank_deg
         << " deg flips/min=" << on.gamma_flips_per_min << " n=" << on.mean_n
         << " defensive ticks=" << on.defensive_ticks
         << " | OFF swing=" << off.gamma_swing_deg << " crashes=" << off.crashes
         << " modes=" << off.mode_ticks[0] << "/" << off.mode_ticks[1] << "/"
         << off.mode_ticks[2] << "/" << off.mode_ticks[3] << "/"
         << off.mode_ticks[4] << "/" << off.mode_ticks[5] << "/"
         << off.mode_ticks[6]
         << " | ON swing=" << on.gamma_swing_deg << " crashes=" << on.crashes
         << " modes=" << on.mode_ticks[0] << "/" << on.mode_ticks[1] << "/"
         << on.mode_ticks[2] << "/" << on.mode_ticks[3] << "/"
         << on.mode_ticks[4] << "/" << on.mode_ticks[5] << "/"
         << on.mode_ticks[6]);
    // The branch arms.
    REQUIRE(on.defensive_ticks > 0);
    // IT READS AS FLYING, and each clause is a measured shape, not a hope:
    //  * the aeroplane is genuinely BANKED (a break is a banked turn), and at
    //    an ace's raised authority it is banked hard;
    REQUIRE(on.mean_bank_deg > 2.0 * off.mean_bank_deg);
    //  * the VERTICAL channel quiets down on both instruments -- the total
    //    flight-path excursion shrinks and the up-down reversals thin out.
    //    THAT is "stop elevator-jink dipping".
    REQUIRE(on.gamma_swing_deg < off.gamma_swing_deg);
    REQUIRE(on.gamma_flips_per_min < off.gamma_flips_per_min);
    // ★ MEASURED AND RECORDED RATHER THAN ASSERTED AWAY: the mean load factor
    // goes DOWN (3.3 -> 2.9), and that is the fix working, not failing. The
    // pre-E7 arm's G is thrash -- a Yoyo's +/-30 deg climb command alternating
    // with an Extend's dive, which is where its 62 deg of gamma swing comes
    // from. A steady banked break pulls less instantaneous G and turns more.
    INFO("mean n OFF=" << off.mean_n << " ON=" << on.mean_n
         << " (down is correct: the pre-E7 G is vertical thrash)");
    // ★ AND THE DEFENDER STOPS RUNNING: the OFF arm spends a third of the
    // engagement in Extend (guns cold, flying away); the break replaces it.
    INFO("Extend ticks under fire: OFF=" << off.mode_ticks[3]
         << " ON=" << on.mode_ticks[3]);
    REQUIRE(on.mode_ticks[3] < off.mode_ticks[3]);
}

TEST_CASE("E7.3: defensive off is bit identical and the cooldown is real") {
    World w;
    build_world(w);
    const auto fly = [&](double range_dial) {
        drone::DroneParams dp = kScen.drone;
        dp.bfm.defensive_range_m = range_dial;
        return under_the_gun(w, dp, 8, 12.0, 500.0);
    };
    const EvadeResult off = fly(0.0);
    const EvadeResult on = fly(kScen.drone.bfm.defensive_range_m);
    REQUIRE(off.defensive_ticks == 0);
    REQUIRE(on.defensive_ticks > 0);
    // THE DUTY-CYCLE GUARANTEE: an attacker permanently on the six must not
    // latch the defender guns-cold forever. defensive_max_s + the cooldown
    // bound the break's share of the engagement well below 1.
    const double frac = static_cast<double>(on.defensive_ticks) /
                        (12.0 / kAp.sim_dt);
    INFO("Defensive share of a 12 s continuous six-o-clock threat: " << frac);
    REQUIRE(frac < 0.85);
    REQUIRE(frac > 0.05);
}

TEST_CASE("E7.3: a break ends in a reversal or an unloaded extension") {
    // Pure bfm. A break whose threat has released rolls back into the FIGHT
    // mode (the reversal); the same break with the energy gone takes Extend
    // (the unloaded extension). Both endings are the ruling's own words.
    bfm::BfmParams bp = kScen.drone.bfm;
    bp.enabled = true;
    const auto run = [&](bool energy_dead, bool slasher) {
        bfm::BfmDials dl;
        dl.doctrine_slash = slasher;
        sim::SimState s;
        s.position = glm::dvec3{kAp.R + 2500.0, 0.0, 0.0};
        s.velocity = glm::dvec3{0.0, 150.0, 0.0};
        s.last_vhat = glm::dvec3{0.0, 1.0, 0.0};
        sim::SimState p = s;
        // The attacker is AHEAD and pointing AWAY: threat_cos is deeply
        // negative, so the break's release predicate holds on entry.
        p.position += glm::dvec3{0.0, 600.0, 0.0};
        p.velocity = glm::dvec3{0.0, 275.0, 0.0};
        p.last_vhat = glm::dvec3{0.0, 1.0, 0.0};
        if (energy_dead) {
            // Put the player far higher: e_delta collapses past
            // -extend_energy_m and the break's exit must take the extension.
            p.position = glm::dvec3{kAp.R + 2500.0 + 4.0 * bp.extend_energy_m,
                                    600.0, 0.0};
        }
        bfm::BfmState st;
        st.mode = bfm::BfmState::Mode::Defensive;
        st.mode_ticks = 0;
        for (int t = 0; t < 4000; ++t) {
            bfm::bfm_step(st, s, p, bp, dl, kAp, kAp.sim_dt);
            if (st.mode != bfm::BfmState::Mode::Defensive) break;
        }
        return st.mode;
    };
    REQUIRE(run(false, false) == bfm::BfmState::Mode::Offensive);  // reversal
    REQUIRE(run(false, true) == bfm::BfmState::Mode::Perch);  // slasher reversal
    REQUIRE(run(true, false) == bfm::BfmState::Mode::Extend);  // the extension
}

// ===========================================================================
// PROBE P-H — "THE ACE DUEL" (spec ACCEPTANCE)
// ===========================================================================

TEST_CASE("probe P-H: the ace duel") {
    // A stepped ace-course player against ONE PITVIPER-class pilot at the
    // shipped dials, 10 minutes, versus the same duel
    // on the pre-E7 table. The spec's bar: a sustained tracking solution of
    // >= 2 s at least once per 10 min, and hits above the E6 baseline.
    World w;
    build_world(w);
    const DuelResult off = ace_course(w, pre_e7(kScen.drone), 1, 10.0, 2);
    const DuelResult on = ace_course(w, kScen.drone, 1, 10.0, 2);

    INFO("P-H OFF (E6 table) rounds=" << off.rounds << " hits=" << off.hits
         << " fire_ticks=" << off.fire_ticks << " best_track="
         << off.best_track_s << " s min_range=" << off.min_range_m
         << " max_bank=" << off.max_bank_deg << " deg max_n=" << off.max_n
         << " crashes=" << off.crashes << "  modes I/O/Y/E/P/S/D="
         << off.mode_ticks[0] << "/" << off.mode_ticks[1] << "/"
         << off.mode_ticks[2] << "/" << off.mode_ticks[3] << "/"
         << off.mode_ticks[4] << "/" << off.mode_ticks[5] << "/"
         << off.mode_ticks[6] << " foe_ticks=" << off.foe_ticks
         << " best_cone=" << off.best_cone_s << " s");
    INFO("P-H ON  (E7 table) rounds=" << on.rounds << " hits=" << on.hits
         << " fire_ticks=" << on.fire_ticks << " best_track="
         << on.best_track_s << " s min_range=" << on.min_range_m
         << " max_bank=" << on.max_bank_deg << " deg max_n=" << on.max_n
         << " crashes=" << on.crashes << "  modes I/O/Y/E/P/S/D="
         << on.mode_ticks[0] << "/" << on.mode_ticks[1] << "/"
         << on.mode_ticks[2] << "/" << on.mode_ticks[3] << "/"
         << on.mode_ticks[4] << "/" << on.mode_ticks[5] << "/"
         << on.mode_ticks[6] << " foe_ticks=" << on.foe_ticks
         << " best_cone=" << on.best_cone_s << " s");

    // (0) NON-VACUITY: the duel happened, and the ace was actually engaged for
    // a comparable share of both arms (a difference in rounds is then a
    // difference in the KILL CHAIN, not in who showed up).
    REQUIRE(on.min_range_m < kScen.drone.engage_range);
    REQUIRE(on.foe_ticks > 0);
    REQUIRE(off.foe_ticks > 0);
    // (1) ★ THE SPEC'S 2-SECOND BAR IS **NOT MET**, AND IT IS RECORDED AS AN
    // OPEN RESIDUAL FOR CHAD RATHER THAN RE-PINNED AWAY OR ASSERTED INTO A
    // FAILURE THIS RUNG CANNOT FIX. Measured best sustained solution: ~0.5 s at
    // the flat cap, against the spec's >= 2.0 s.
    //
    // ATTRIBUTION (from the same printout, so it is read not guessed): the ace
    // holds the player as its foe for only ~24% of the run, and spends ~78% of
    // THAT in Intercept. It is not losing the tracking fight -- it is failing to
    // CLOSE on a 275 m/s target in the first place. That is an E1.1 residual
    // (chase ceiling / closure), living upstream of everything E7 touches: no
    // bank cap, doctrine or break can hold a gun solution on a man you never
    // arrive behind. Fixing it is its own rung and must not be bolted onto this
    // one.
    INFO("★ OPEN RESIDUAL FOR CHAD: best sustained solution "
         << on.best_track_s << " s against the spec bar of 2.0 s. The ace is "
         << "foe for " << on.foe_ticks << " ticks and cannot close -- an E1.1 "
         << "closure residual, NOT an E7 failure. See the ledger.");
    // ★★ (2)(3) THE ON-vs-OFF COMPARATIVE CLAIMS ARE WITHDRAWN — SAME REASON AS
    // P-A, ONLY WORSE. This leg is ONE 10-minute duel: n = 1, the noisiest
    // instrument in the suite. The noise floor (docs spec "E7.I") showed that
    // even an n=8 ensemble cannot separate arms that differ by half a degree of
    // bank cap; a single engagement separates nothing at all. The evidence is
    // in this fixture's own history: at the FLAT cap it read ON 4 rounds / 2
    // hits against OFF 2 / 0 and these clauses passed; at the shipped deadband
    // it reads ON 0 / 0 against OFF 2 / 0 and they fail. Those two tables are
    // the same rung. A clause that flips on which arm got lucky is not a gate,
    // it is a coin toss with an assertion around it.
    //
    // The rung's quantitative ruling was taken at n=64 on P-A and lives in the
    // ledger. This leg keeps its value as ATTRIBUTION — the mode histogram and
    // the foe-time are what diagnosed the closure residual above — plus the
    // structural clauses below, which n=1 CAN carry because they are facts
    // about the machine rather than rates over a chaotic fight.
    INFO("P-H comparative (RECORDED, not asserted -- n=1): rounds ON="
         << on.rounds << " OFF=" << off.rounds << ", hits ON=" << on.hits
         << " OFF=" << off.hits << ", best_track ON=" << on.best_track_s
         << " OFF=" << off.best_track_s << " s");
    // (4) The ace really is flying the repealed envelope, in a REAL fight.
    REQUIRE(on.max_bank_deg > 55.0);
    // (5) It is not being paid for in wrecks.
    REQUIRE(on.crashes <= off.crashes + 1);
}

// ===========================================================================
// RUNG E8 — THE ENERGY FIGHT (Chad, 2026-08-21, after conquest_tape_100:
// "Need to give enemies more energy fighting abilities as that is how I am
// able to beat them. I almost died to them last match though").
// ===========================================================================

TEST_CASE("E8.2: the fight speed off arm is bit identical") {
    World w;
    build_world(w);
    const auto fly = [&](double fs) {
        drone::DroneParams dp = kScen.drone;
        dp.maverick.enabled = false;
        dp.bfm.fight_speed_mps = fs;
        return ace_course(w, dp, 2, 1.5, 0);
    };
    const DuelResult off = fly(0.0);
    const DuelResult on = fly(kScen.drone.bfm.fight_speed_mps);
    // STRUCTURAL off: max(dl.speed, 0) IS dl.speed, so every engaged mode
    // command is the pre-E8 expression bit-for-bit -- not a numeric
    // equivalent. A value BELOW the patrol cruise would also be a no-op, and
    // the loader rejects that precisely so this identity can never be
    // satisfied by an accidentally-dead dial instead of by the off-value.
    //
    // The arm that PROVES the max() semantics on the live path is a value
    // strictly BELOW the patrol cruise: it must reproduce the off-value
    // exactly. The loader refuses to ship such a value; constructing it
    // directly here is the FIX-3 precedent (the clamp is the invariant, the
    // loader check is the friendlier failure).
    const DuelResult below = fly(kScen.drone.speed * 0.5);
    for (int m = 0; m < 7; ++m) REQUIRE(off.mode_ticks[m] == below.mode_ticks[m]);
    REQUIRE(off.rounds == below.rounds);
    REQUIRE(off.foe_ticks == below.foe_ticks);
    REQUIRE(off.min_range_m == below.min_range_m);

    INFO("off: rounds=" << off.rounds << " best_cone_s=" << off.best_cone_s
                        << " min_range=" << off.min_range_m
                        << "   ON: rounds=" << on.rounds << " best_cone_s="
                        << on.best_cone_s << " min_range=" << on.min_range_m);
    // NON-VACUITY: the dial must actually change the fight. Without this the
    // identity above is satisfied by a dial that is wired to nothing -- the
    // E7.2 trap (every test green, the aeroplane never fires) one level up.
    REQUIRE(kScen.drone.bfm.fight_speed_mps > kScen.drone.speed);
    bool moved = off.rounds != on.rounds || off.min_range_m != on.min_range_m;
    for (int m = 0; m < 7; ++m) moved = moved || off.mode_ticks[m] != on.mode_ticks[m];
    REQUIRE(moved);
}

TEST_CASE("E8.2: the fight speed never touches the errand paths") {
    // The 2026-07-14 ruling that set the patrol cruise to 85 ("a couple flew
    // away and would take long to catch") is about a bandit NOT in a fight,
    // and it still stands: patrol / maverick / raid steering never come
    // through bfm_step at all, so no value of this dial can reach them. Pinned
    // structurally -- the dial lives on BfmParams, and DroneParams::speed (the
    // errand cruise) is untouched by construction.
    //
    // CONFIG-RELATIVE, never a welded 85: a patrol-cruise retune must not
    // false-fail this leg (the AT-15 trap), and a fight speed that had
    // silently fallen to or below the cruise must not pass it either.
    REQUIRE(kScen.drone.speed > 0.0);
    REQUIRE(kScen.drone.bfm.fight_speed_mps > kScen.drone.speed);
}

TEST_CASE("E8.1: the perch standoff is diveable inside the pursuit envelope") {
    // ★ THE CHECK WHOSE ABSENCE LET AN UN-DIVEABLE PERCH SHIP GREEN. The
    // pre-E8 perch sat 700 m over the 220 m TRACKING lag point: the dive onto
    // him needed atan(700/220) = 72.6 deg against a pursue_max_gamma of 30, so
    // the slasher could not point at the target it perched over. Probe P-S
    // measured a whole pass at a best cone cos of -0.106 (96 deg off) and ZERO
    // rounds.
    const auto& b = kScen.drone.bfm;
    const double lag = b.perch_lag_m > 0.0 ? b.perch_lag_m : b.lag_dist_m;
    REQUIRE(b.perch_height_m > 0.0);
    REQUIRE(lag > 0.0);
    const double need = std::atan2(b.perch_height_m, lag);
    INFO("perch dive needs " << need * 57.2958 << " deg, envelope is "
                             << kScen.drone.pursue_max_gamma * 57.2958
                             << " deg");
    REQUIRE(need <= 0.80 * kScen.drone.pursue_max_gamma);
    // And the perch must still be REACHABLE, or `perched` never latches and
    // the slasher climbs forever: the loader bounds one end, this bounds the
    // other.
    const double perch_range =
        std::sqrt(lag * lag + b.perch_height_m * b.perch_height_m);
    INFO("perch range " << perch_range << " vs attack_range "
                        << b.attack_range_m);
    REQUIRE(perch_range < b.attack_range_m);
}

// ===========================================================================
// RUNG E7.4 — THE TRACKING PITCH GAIN (the pitch limit cycle).
//
// THE HIDDEN PLANT SWEEP. `pursue_track_pitch_gain` was BUILT in the E7 session
// and shipped at 0.0 with NO test of its own; spec E7.E's sweep that motivated
// it lowered `pursue_pitch_gain` GLOBALLY, which is not the shippable form. So
// two things are unmeasured and are measured here:
//   (1) does the SCOPED dial reproduce the global sweep's plant behaviour at the
//       engaged seam (it must, or E7.E's numbers do not transfer to the dial
//       that actually ships), and
//   (2) where exactly is the cliff? E7.E only knows it is somewhere between 1.8
//       and 1.4 -- a coarse grid, and the value this rung ships comes off it.
//
// ★ THIS IS A DETERMINISTIC PLANT MEASUREMENT, which is the whole point. Spec
// E7.I's noise law kills any furball claim below 2x; the saturated turn is one
// aeroplane, one command, no chaos, and it resolves a 14-deg swing collapsing to
// 0.2 with no ensemble at all. The value is picked HERE and the furball is asked
// only whether it costs anything resolvable.
//
// HIDDEN ([.]) because it is a printout, not an acceptance.
// ===========================================================================
TEST_CASE("E7.4 plant sweep: the tracking pitch gain cliff", "[.e74plant]") {
    World w;
    build_world(w);
    const drone::DroneParams shipped = kScen.drone;
    const double global_gain = shipped.pursue_pitch_gain;

    std::string table =
        "\nE7.4 PLANT SWEEP -- the tracking pitch gain, SCOPED dial, saturated "
        "turn\n  pursue_pitch_gain (the global gain, = the OFF value) = " +
        std::to_string(global_gain) + "\n  n needed to hold the ruled 55 deg = " +
        std::to_string(1.0 / std::cos(55.0 * kPi / 180.0)) + "\n";

    const double bearings[4] = {10.0, 30.0, 50.0, 90.0};
    const double gains[9] = {0.0, 2.0, 1.8, 1.7, 1.6, 1.5, 1.4, 1.2, 1.0};
    for (int pilot : {5, 2}) {  // 5 = PITVIPER (ace tier), 2 = CANARY (base)
        for (double bd : bearings) {
            table += "\n  pilot index " + std::to_string(pilot) + ", bearing " +
                     std::to_string(bd) + " deg";
            for (double g : gains) {
                drone::DroneParams dp = shipped;
                dp.pursue_track_pitch_gain = g;
                const TurnResult r = saturated_turn(w, dp, pilot,
                                                    bd * kPi / 180.0);
                table += "\n    track_gain=" + std::to_string(g) +
                         (g <= 0.0 ? " (OFF, = the global gain)" : "") +
                         " swing=" + std::to_string(r.gamma_swing_deg) +
                         " n=" + std::to_string(r.n) +
                         " turn=" + std::to_string(r.turn_dps) +
                         " V=" + std::to_string(r.speed) +
                         " bank=" + std::to_string(r.bank_deg) +
                         " alt=[" + std::to_string(r.min_alt_m) + "," +
                         std::to_string(r.max_alt_m) + "]" +
                         " crashes=" + std::to_string(r.crashes);
            }
        }
    }

    // ★ THE EQUIVALENCE CHECK. E7.E's numbers were taken with the gain lowered
    // GLOBALLY. If the scoped dial does not reproduce them at this seam, every
    // number in E7.E is about a different aeroplane than the one that ships.
    table += "\n\n  SCOPED vs GLOBAL at 30 deg (pilot 5) -- must agree:";
    for (double g : {1.8, 1.4, 1.0}) {
        drone::DroneParams sc = shipped;
        sc.pursue_track_pitch_gain = g;
        drone::DroneParams gl = shipped;
        gl.pursue_pitch_gain = g;  // the E7.F form: lowered everywhere
        const TurnResult a = saturated_turn(w, sc, 5, 30.0 * kPi / 180.0);
        const TurnResult b = saturated_turn(w, gl, 5, 30.0 * kPi / 180.0);
        table += "\n    gain=" + std::to_string(g) +
                 " scoped swing=" + std::to_string(a.gamma_swing_deg) +
                 " turn=" + std::to_string(a.turn_dps) +
                 " || global swing=" + std::to_string(b.gamma_swing_deg) +
                 " turn=" + std::to_string(b.turn_dps) +
                 (a.gamma_swing_deg == b.gamma_swing_deg &&
                          a.turn_dps == b.turn_dps
                      ? "  IDENTICAL"
                      : "  ** DIFFER **");
    }

    // The BOB signature from the second, independent fixture (E7.3's), because
    // "elevator dipping up and down" is a flip COUNT and the saturated turn
    // reports an amplitude. Two different instruments on one mechanism.
    table += "\n\n  UNDER THE GUN (the bob signature, 400 m on the six):";
    for (double g : {0.0, 1.8, 1.4, 1.0}) {
        drone::DroneParams dp = shipped;
        dp.pursue_track_pitch_gain = g;
        const EvadeResult e = under_the_gun(w, dp, 5, 40.0, 400.0);
        table += "\n    track_gain=" + std::to_string(g) +
                 " flips/min=" + std::to_string(e.gamma_flips_per_min) +
                 " swing=" + std::to_string(e.gamma_swing_deg) +
                 " mean_n=" + std::to_string(e.mean_n) +
                 " bank=" + std::to_string(e.mean_bank_deg) +
                 " crashes=" + std::to_string(e.crashes);
    }
    WARN(table);
    SUCCEED("plant sweep printout");
}

// ===========================================================================
// RUNG E10 -- THE SIGNED TRACKING AUTHORITY. DO NOT REGRESS.
//
// \u2605\u2605\u2605 CHAD'S RULING, 2026-08-23, after conquest_tape_7: "I think this qualifies
// as a game loop. I had fun, many suspenseful moments... Right now im satisfied
// with the enemies AS LONG AS THEY DONT REGRESS. I like them attacking and
// defending."
//
// THIS LEG IS THAT SENTENCE. It does not pin the DIALS -- a future rung is free
// to reach this behaviour any way it likes -- it pins the BEHAVIOUR he signed:
// that inside the tracking cone, where the gun solution lives, an engaged ace
// can actually turn. Before E10 the machine handed him the ruled 55 deg there
// and he flew at 2.8 deg/s on n = 1.2, which is why 5 rounds were aimed at Chad
// in 49 minutes. At the signed table it is 10.9 deg/s on n = 3.1, and the
// flight he signed put 39 rounds on him in 22.
//
// The bars are set at 70% of the signed measurement: enough headroom that an
// honest retune elsewhere does not false-fail, tight enough that anyone who
// gives the tracking authority back has to come here and say so out loud.
// ===========================================================================
TEST_CASE("E10: the SIGNED tracking authority does not regress") {
    World w;
    build_world(w);
    const double bearing = 30.0 * kPi / 180.0;  // the tracking cone
    const TurnResult ace = saturated_turn(w, kScen.drone, 5, bearing);
    INFO("SIGNED (tape 7, 2026-08-23): turn 10.9 deg/s at n 3.1 in the tracking "
         "cone. MEASURED NOW: turn " << ace.turn_dps << " deg/s, n=" << ace.n
         << ", achieved bank " << ace.bank_deg << " deg, cap handed "
         << ace.cap_handed_deg << ", swing " << ace.gamma_swing_deg);
    // (1) THE TURN HE SIGNED. This is the whole rung in one number.
    REQUIRE(ace.turn_dps > 0.70 * 10.9);
    // (2) AND IT IS PULLED, not mushed -- the load factor is what distinguishes
    // a real turn from a commanded one (pre-E10 was n = 1.2, essentially 1 g).
    REQUIRE(ace.n > 0.70 * 3.1);
    // (3) THE AUTHORITY REACHES THE TRACKING CONE AT ALL. The E7.1 deadband
    // faded the tier cap back to the ruled 55 inside 30 deg off, which is
    // exactly where a gun solution has to be held; that is what E10 repealed.
    // Config-relative: a future retune may raise the ruled cap, and this clause
    // must still mean "he gets MORE than the floor everyone already has".
    REQUIRE(ace.cap_handed_deg > kScen.drone.pursue_max_bank * 180.0 / kPi + 1.0);
    // (4) ...and the turn is still CLEAN. A 14-deg flight-path swing is half
    // the fire cone; buying turn rate by re-introducing the porpoise would pass
    // (1) and (2) while handing back the gun solution they buy.
    const double cone_deg = std::acos(kScen.drone.fire_cone_cos) * 180.0 / kPi;
    REQUIRE(ace.gamma_swing_deg < 0.5 * cone_deg);
    REQUIRE(ace.crashes == 0);
}

TEST_CASE("E7.4: the tracking pitch gain off arm is bit identical") {
    // TWO structural off-forms, each proven on its own, and PAIRED with an ON
    // arm that must differ -- a dial that silently did nothing would otherwise
    // pass the identity clauses perfectly (the house rule, and the E7.2 trap:
    // proven to INTEND to do something, never proven to do it).
    World w;
    build_world(w);
    drone::DroneParams off = kScen.drone;
    off.pursue_track_pitch_gain = 0.0;  // the <= 0 sentinel
    drone::DroneParams eq = kScen.drone;
    // The second off-form: dialled exactly TO the global gain. The seam is
    // `track > 0 ? track : pursue_pitch_gain`, so this must be the same double
    // by construction and the two arms must agree to the last bit.
    eq.pursue_track_pitch_gain = kScen.drone.pursue_pitch_gain;
    drone::DroneParams on = kScen.drone;
    on.pursue_track_pitch_gain = 1.0;

    const TrackTrace a = track_flight(w, off, 5, 2500.0, 20.0, true, false);
    const TrackTrace b = track_flight(w, eq, 5, 2500.0, 20.0, true, false);
    const TrackTrace c = track_flight(w, on, 5, 2500.0, 20.0, true, false);
    INFO("off vs eq: " << glm::length(a.last.position - b.last.position)
                       << " m apart | off vs ON(1.0): "
                       << glm::length(a.last.position - c.last.position)
                       << " m apart");
    REQUIRE(same_state(a.last, b.last));
    // NON-VACUITY: the fixture really does run the tracking seam, so the
    // identity above is a scoping fact and not an inert fixture.
    REQUIRE_FALSE(same_state(a.last, c.last));
    REQUIRE(a.crashes == 0);
    REQUIRE(c.crashes == 0);
}

TEST_CASE("E7.4: the tracking gain never touches the errand or pull-up paths") {
    // ★★ THE SAFETY LEG, and it is the reason this dial exists in the scoped
    // form at all. `pursue_pitch_gain` is the terrain-avoidance PULL-UP gain
    // (drone.h, the avoid branch) and the raid / defend / strike / arena-guard
    // errand gain. The E7.F sweep that MOTIVATED this rung lowered it globally,
    // which would have slowed every pull-up: a CFIT risk. So the claim "the
    // repeal is confined to the tracking task" is a SAFETY claim and it is
    // measured here, three ways, bit-for-bit.
    World w;
    build_world(w);
    const auto arm = [&](double g) {
        drone::DroneParams dp = kScen.drone;
        dp.pursue_track_pitch_gain = g;
        return dp;
    };

    // (a) THE UNENGAGED BANDIT. No foe, no player pointer: the tracking seam is
    // structurally unreachable, so the trajectory may not move by one bit.
    const TrackTrace pa = track_flight(w, arm(0.0), 5, 2500.0, 30.0, false, false);
    const TrackTrace pb = track_flight(w, arm(1.0), 5, 2500.0, 30.0, false, false);
    REQUIRE(same_state(pa.last, pb.last));

    // (b) THE RAID ERRAND, which commands a real descent onto a pump 30 km out
    // and 2300 m below -- exactly the kind of gamma command a pitch gain moves.
    const TrackTrace ra = track_flight(w, arm(0.0), 5, 2500.0, 30.0, false, true);
    const TrackTrace rb = track_flight(w, arm(1.0), 5, 2500.0, 30.0, false, true);
    INFO("raid errand: min_agl " << ra.min_agl_m << " vs " << rb.min_agl_m);
    REQUIRE(same_state(ra.last, rb.last));
    // NON-VACUITY for the errand arms: they must actually have FLOWN the
    // errand, or "identical" is a statement about two aeroplanes doing nothing.
    REQUIRE(ra.min_agl_m < 2500.0 - 1.0);

    // (c) ★ THE FORCED PULL-UP, flown by an ENGAGED bandit -- the case that
    // matters most, because an engaged tracker IS the pilot whose level_p this
    // rung moves, and the avoid branch must still overwrite it. Started at 150
    // m AGL, below avoid_agl_enter_m (250), so the pull-up is engaged from the
    // first tick and stays latched to the 400 m release.
    const TrackTrace ga = track_flight(w, arm(0.0), 5, 150.0, 3.0, true, false);
    const TrackTrace gb = track_flight(w, arm(1.0), 5, 150.0, 3.0, true, false);
    INFO("pull-up: avoid_ticks " << ga.avoid_ticks << "/" << gb.avoid_ticks
                                 << " min_agl " << ga.min_agl_m << " vs "
                                 << gb.min_agl_m);
    // NON-VACUITY FIRST: the forced pull-up really was engaged for the whole
    // window, so the identity below is about the avoid branch and not about a
    // fixture that never went near the ground.
    REQUIRE(ga.avoid_ticks > 0);
    REQUIRE(ga.avoid_ticks == gb.avoid_ticks);
    REQUIRE(same_state(ga.last, gb.last));
    REQUIRE(ga.crashes == 0);
    // ...and the pull-up actually CLIMBED AWAY -- the safety behaviour itself,
    // not just its bit-identity. Final AGL against the 150 m it started at.
    const glm::dvec3 gu = glm::normalize(ga.last.position);
    const double end_agl = glm::length(ga.last.position) - w.hf.radius_at(gu);
    INFO("pull-up end AGL " << end_agl << " m (started 150)");
    REQUIRE(end_agl > 150.0);
}

TEST_CASE("E7.4: the pitch limit cycle, and what the shipped gain does to it") {
    // ★ THE PLANT MEASUREMENT THIS RUNG IS RULED ON, pinned so a retune cannot
    // lose it. The saturated turn is DETERMINISTIC -- one aeroplane, one
    // command, no furball -- which is why the value comes off it and not off
    // P-A, where spec E7.I's noise floor eats anything under 2x.
    //
    // THE WALL (measured, spec E7.E): at the global 2.2, a 55-deg saturated
    // turn on the nose swings its flight path ~14 deg peak-to-peak and pulls
    // n ~ 5.5 to hold a turn that needs 1/cos(55) = 1.74. Roughly 4 G into
    // pitch thrash instead of turn, and a nose thrashing 14 deg vertically is
    // exactly what a gun solution cannot survive.
    World w;
    build_world(w);
    const double bearing = 30.0 * kPi / 180.0;  // the tracking regime
    // ★★★ THE BANK REGIME IS CONSTRUCTED, NOT INHERITED (rung E10, and this is
    // the E9 lesson landing on E9's own leg). This test read its WALL arm off
    // `kScen.drone`, so when E10 took the deadband off and the cap to 78 the
    // "wall" stopped existing: the same fixture reads swing 0.19 deg, n 3.3,
    // turn 11.9 deg/s. THE PITCH LIMIT CYCLE IS A PROPERTY OF THE RULED 55-DEG
    // BANK, not of the aeroplane -- at 78 deg of bank there is no porpoise to
    // cure. So the regime is now pinned explicitly (ace_bank_cap = 0 hands
    // every pilot the ruled cap) and this leg measures what it always claimed
    // to: what the tracking pitch gain does to the cycle, at the bank where the
    // cycle lives. An arm defined by READING THE SHIPPED TABLE stops being that
    // arm the moment the table moves.
    drone::DroneParams ruled = kScen.drone;
    ruled.ace_bank_cap = 0.0;  // the ruled 55 for everyone
    ruled.mid_bank_cap = 0.0;
    drone::DroneParams off = ruled;
    off.pursue_track_pitch_gain = 0.0;
    const TurnResult wall = saturated_turn(w, off, 5, bearing);
    const double n_needed = 1.0 / std::cos(55.0 * kPi / 180.0);
    INFO("THE WALL (track gain OFF = " << kScen.drone.pursue_pitch_gain
         << "): swing " << wall.gamma_swing_deg << " deg, n=" << wall.n
         << " (needs " << n_needed << "), net turn " << wall.turn_dps
         << " deg/s, V=" << wall.speed);
    REQUIRE(wall.gamma_swing_deg > 10.0);       // it porpoises
    REQUIRE(wall.n > 3.0 * n_needed);           // on G that is not turning it
    REQUIRE(wall.crashes == 0);

    if (kScen.drone.pursue_track_pitch_gain <= 0.0) {
        // The dial ships OFF: there is nothing to assert about a cure, and
        // saying so is the honest form. The wall above is still pinned.
        SUCCEED("pursue_track_pitch_gain is off; the wall is recorded only");
        return;
    }

    // ★ BOTH TIERS, because the cost of this dial is not tier-neutral (spec
    // E9.1: off the nose the base tier pays four times what the ace pays) and a
    // one-pilot measurement would hide that. Index 5 = PITVIPER (ace tier),
    // index 2 = CANARY (base tier) -- the same pair the rest of this file uses.
    const TurnResult wall_base = saturated_turn(w, off, 2, bearing);
    INFO("THE WALL, base tier: swing " << wall_base.gamma_swing_deg
         << " deg, n=" << wall_base.n << ", net turn " << wall_base.turn_dps);
    REQUIRE(wall_base.gamma_swing_deg > 10.0);
    REQUIRE(wall_base.n > 3.0 * n_needed);

    const TurnResult cured = saturated_turn(w, ruled, 5, bearing);
    const TurnResult cured_base = saturated_turn(w, ruled, 2, bearing);
    // ★ AND THE SHIPPED TABLE HAS LEFT THE REGIME ENTIRELY, recorded because it
    // is the more important fact: at the E10 cap the tracking turn does not
    // porpoise at all, so the cure below is now insurance on a regime the
    // fleet only re-enters if the bank repeal is walked back.
    {
        const TurnResult shipped_now = saturated_turn(w, kScen.drone, 5, bearing);
        INFO("THE SHIPPED TABLE at this bearing: swing "
             << shipped_now.gamma_swing_deg << " deg, n=" << shipped_now.n
             << ", net turn " << shipped_now.turn_dps << " deg/s");
        REQUIRE(shipped_now.crashes == 0);
    }
    INFO("THE CURE (track gain " << kScen.drone.pursue_track_pitch_gain
         << "): swing " << cured.gamma_swing_deg << " deg, n=" << cured.n
         << ", net turn " << cured.turn_dps << " deg/s, V=" << cured.speed
         << ", crashes=" << cured.crashes);
    // ★ THE BAR IS THE GUN'S OWN CONE, not a number picked to pass. The fire
    // gate needs the lead point inside fire_cone_deg of the nose; a limit cycle
    // that swings the flight path by more than HALF that cone has eaten a
    // quarter of the budget in each direction before any tracking error, lead
    // error or LOS rate is spent. Config-relative, so a cone retune moves the
    // bar with it.
    const double cone_deg = std::acos(kScen.drone.fire_cone_cos) * 180.0 / kPi;
    INFO("fire cone half-angle = " << cone_deg << " deg; bar = " << 0.5 * cone_deg);
    REQUIRE(cured.gamma_swing_deg < 0.5 * cone_deg);
    // The over-pull is repealed: the plant now pulls about what the turn needs
    // rather than three times it.
    REQUIRE(cured.n < 1.5 * n_needed);
    // ★ AND IT IS NOT PAID FOR IN TURN RATE ON THE NOSE. This is the surprise
    // in the measurement and it is why the dial is worth shipping: the porpoise
    // was EATING turn rate, so killing it turns FASTER, not slower.
    REQUIRE(cured.turn_dps > wall.turn_dps);
    REQUIRE(cured.speed > wall.speed);
    REQUIRE(cured.crashes == 0);
    // The same four clauses on the BASE tier, which is most of the fleet.
    INFO("THE CURE, base tier: swing " << cured_base.gamma_swing_deg
         << " deg, n=" << cured_base.n << ", net turn " << cured_base.turn_dps
         << ", V=" << cured_base.speed);
    REQUIRE(cured_base.gamma_swing_deg < 0.5 * cone_deg);
    REQUIRE(cured_base.n < 1.5 * n_needed);
    REQUIRE(cured_base.turn_dps > wall_base.turn_dps);
    REQUIRE(cured_base.speed > wall_base.speed);
    REQUIRE(cured_base.crashes == 0);
    // The loader's own bound, restated where a reader will see it: this dial
    // may only ever REPEAL an over-gain, never raise pitch authority by a
    // second unruled route.
    REQUIRE(kScen.drone.pursue_track_pitch_gain <= kScen.drone.pursue_pitch_gain);
}

TEST_CASE("E8.1: the perch standoff off arm is bit identical") {
    // ★★ THE NAME NOW MATCHES THE ASSERTIONS (rung E9, red team P1-11). As E8
    // shipped it this leg asserted `REQUIRE(moved)` -- the OPPOSITE of
    // bit-identity -- and contained no identity clause at all, while its E8.2
    // sibling had a real one. A test whose NAME outruns its reach is this
    // house's own recurring trap; the identity it claims is now measured.
    World w;
    build_world(w);
    const auto fly = [&](double lag) {
        drone::DroneParams dp = kScen.drone;
        dp.maverick.enabled = false;
        dp.slash_doctrine = true;  // the doctrine must be REACHABLE or this
        dp.bfm.perch_lag_m = lag;  //   differential is vacuous
        return ace_course(w, dp, 2, 1.5, 0);
    };
    const DuelResult off = fly(0.0);   // falls back to lag_dist_m = pre-E8
    const DuelResult on = fly(kScen.drone.bfm.perch_lag_m);
    // ★ THE IDENTITY THE NAME PROMISES: the `0` sentinel falls back to
    // `lag_dist_m`, so dialling that value in EXPLICITLY must be the same
    // aeroplane to the last bit. That is what "off = pre-E8 bit-for-bit" means,
    // and until now nothing checked it.
    const DuelResult explicit_pre_e8 = fly(kScen.drone.bfm.lag_dist_m);
    INFO("sentinel vs explicit lag_dist_m: min_range " << off.min_range_m
         << " vs " << explicit_pre_e8.min_range_m);
    REQUIRE(off.min_range_m == explicit_pre_e8.min_range_m);
    for (int m = 0; m < 7; ++m)
        REQUIRE(off.mode_ticks[m] == explicit_pre_e8.mode_ticks[m]);
    INFO("perch/slash ticks off=" << off.mode_ticks[4] << "/"
                                  << off.mode_ticks[5] << " on="
                                  << on.mode_ticks[4] << "/" << on.mode_ticks[5]);
    // NON-VACUITY first: the doctrine really was flown in both arms, so the
    // comparison below is about the standoff and not about an inert mode.
    REQUIRE(off.mode_ticks[4] + off.mode_ticks[5] > 0);
    REQUIRE(on.mode_ticks[4] + on.mode_ticks[5] > 0);
    bool moved = off.min_range_m != on.min_range_m;
    for (int m = 0; m < 7; ++m) moved = moved || off.mode_ticks[m] != on.mode_ticks[m];
    REQUIRE(moved);
}
